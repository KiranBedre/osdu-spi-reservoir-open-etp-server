// ============================================================================
// Copyright 2019-2026 Emerson Paradigm Holding LLC. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

// Integration (regression) coverage for the custom DataArray compute
// extensions (STATS / REDUCE / WHERE / WHEREN) driven through the REAL GetDataArrayMetadata dispatch path end to
// end (marker detection -> concurrency gate -> dataspace lookup -> CAN_READ ->
// reader open -> (epc_guid,path) array resolution -> streaming fold -> reply),
// asserting the happy paths (summary, range count, histogram + percentiles,
// integer null sentinel) and the error channels (dataspace-not-found,
// path-not-found, malformed marker rejected, concurrency limit). The pure fold
// maths live in the ArrayStats unit suite; this file guards the plumbing those
// unit tests cannot reach.

#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/eml/TestingMocks.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/eml/ServerTesting.hpp>

#include <oes/common/etp12/EmlUri.h>

#include <oes/eml/openkv/ArrayProto.h>
#include <oes/eml/openkv/ComputeGate.h>
#include <oes/eml/etp12/Extensions.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace {

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;
namespace common12 = oes::common::etp12;
using common12::EmlUri;

//============================================================================
// A dataspace holding an EpcExternalPartReference container plus one or more
// numeric double arrays under it, ready to be resolved by the compute core.

struct ComputeResolveFixture : public OpenKVServerFixture {
    // Build an EmlAryMock of the given dims whose elements are 0,1,2,... as
    // doubles (so count == product(dims), min == 0, max == count-1).
    static EmlAryMock rampArray(const std::string& path, std::vector<int64_t> dims) {
        int64_t n = 1;
        for (int64_t d : dims) {
            n *= d;
        }
        return EmlAryMock{
            path, std::move(dims), AnyArrayType::eArrayOfDouble, {},
            [n](AnyArray_item_v& any_ary) {
                ArrayOfDouble dbl;
                dbl.m_values.resize(static_cast<size_t>(n));
                for (int64_t i = 0; i < n; ++i) {
                    dbl.m_values[static_cast<size_t>(i)] = static_cast<double>(i);
                }
                any_ary = std::move(dbl);
            }
        };
    }

    // Create the dataspace + HDF container and store the given arrays under it.
    // Returns the full EpcExternalPartReference URI the arrays live under.
    std::string setupContainer(
        const data12::Object::Dataspace& space, const std::vector<EmlAryMock>& arrays
    ) {
        const EmlObjMock& mock_hdf = epc_ext_mock();
        putMockObj(space, mock_hdf);
        const std::string epc_uri = EmlUri::make_obj(
            EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
        ).to_string();
        for (const EmlAryMock& ary : arrays) {
            putMockAry(space, epc_uri, ary);
        }
        return epc_uri;
    }

    // Build an EmlAryMock of int32 values (discrete property) from a literal list.
    static EmlAryMock intArray(const std::string& path, std::vector<int32_t> vals) {
        const int64_t n = static_cast<int64_t>(vals.size());
        return EmlAryMock{
            path, {n}, AnyArrayType::eArrayOfInt, {},
            [vals](AnyArray_item_v& any_ary) {
                ArrayOfInt ia;
                ia.m_values = vals;
                any_ary = std::move(ia);
            }
        };
    }

    // Re-point every entry of a built extension request at the guid-bearing
    // container URI, so resolution keys directly on (epc_guid, path) exactly as
    // the production REST path does once it resolves an array to its proxy.
    template <typename Req>
    static void retargetUri(Req& req, const std::string& uri) {
        for (auto& kv : req.m_dataArrays) {
            kv.second.m_uri = uri;
        }
    }
};

//============================================================================

BOOST_FIXTURE_TEST_SUITE(ComputeResolveTests, ComputeResolveFixture)

namespace {
    const std::map<std::string, DataValue>& statsOf(
        const DataArray::GetDataArrayMetadataResponse& res, const std::string& cid) {
        BOOST_REQUIRE_EQUAL(res.m_arrayMetadata.size(), 1u);
        BOOST_REQUIRE(res.m_arrayMetadata.count(cid));
        return res.m_arrayMetadata.at(cid).m_customData;
    }
    int64_t longOf(const std::map<std::string, DataValue>& cd, const char* k) {
        BOOST_REQUIRE_MESSAGE(cd.count(k), k);
        return std::get<int64_t>(cd.at(k).m_item);
    }
    double doubleOf(const std::map<std::string, DataValue>& cd, const char* k) {
        BOOST_REQUIRE_MESSAGE(cd.count(k), k);
        return std::get<double>(cd.at(k).m_item);
    }
    // Per-cid error code if the reply carried one, else the fatal error code, else 0.
    int errorCodeOf(const EtpResponseStatus& rc, const std::string& cid) {
        const auto it = rc.ex.m_errors.find(cid);
        if (it != rc.ex.m_errors.end()) {
            return int(it->second.m_code);
        }
        return rc.ex.m_error ? int(rc.ex.m_error->m_code) : 0;
    }
} // namespace

//=============================== HAPPY PATHS =================================

// STATS resolves the array through the compute core and folds it: the reply
// carries a valid summary with the expected element/null counts and min/max.
BOOST_AUTO_TEST_CASE(stats_resolves_and_folds) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-stats/values", {2, 3, 4}); // 24 cells, 0..23
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, "STATS");
    BOOST_CHECK_EQUAL(longOf(cd, DataArrayExt::kComputeSchemaKey), DataArrayExt::kComputeSchemaVersion);
    BOOST_REQUIRE(cd.count("stats.valid"));
    BOOST_CHECK(std::get<bool>(cd.at("stats.valid").m_item));
    BOOST_CHECK_EQUAL(longOf(cd, "stats.count"), 24);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.nullCount"), 0);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.min"), 0.0, 1e-9);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.max"), 23.0, 1e-9);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.mean"), 11.5, 1e-9);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.sum"), 276.0, 1e-9);
    BOOST_CHECK(!cd.count("stats.inRangeCount"));
    BOOST_CHECK(!cd.count("stats.histogram"));
    // The ordinary metadata is still reported alongside the summary.
    const auto& md = rpc.res_.m_arrayMetadata.at("STATS");
    BOOST_REQUIRE_EQUAL(md.m_dimensions.size(), 3u);
    BOOST_CHECK_EQUAL(md.m_dimensions[0], 2);
    BOOST_CHECK_EQUAL(md.m_dimensions[2], 4);
}

// STATS|lo|hi adds the in-range count over the same single pass.
BOOST_AUTO_TEST_CASE(stats_range_count) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-range/values", {24});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_, true, 5.0, 10.0);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, rpc.req_.m_dataArrays.begin()->first);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.inRangeCount"), 6); // 5,6,7,8,9,10 inclusive
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.rangeLo"), 5.0, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.rangeHi"), 10.0, 1e-12);
    BOOST_CHECK(!cd.count("stats.histogram"));
}

// STATS|lo|hi|bins folds a histogram + p10/p50/p90 in the same pass.
BOOST_AUTO_TEST_CASE(stats_histogram_and_percentiles) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-hist/values", {100}); // 0..99
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_, true, 0.0, 100.0, 10);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, rpc.req_.m_dataArrays.begin()->first);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.histBins"), 10);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.histUnderflow"), 0);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.histOverflow"), 0);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.histLo"), 0.0, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.histHi"), 100.0, 1e-12);
    BOOST_REQUIRE(cd.count("stats.histogram"));
    const auto& bins = std::get<ArrayOfLong>(cd.at("stats.histogram").m_item).m_values;
    BOOST_REQUIRE_EQUAL(bins.size(), 10u);
    for (int64_t c : bins) {
        BOOST_CHECK_EQUAL(c, 10); // uniform ramp: 10 values per 10-wide bin
    }
    // Percentiles are interpolated to within one bin width (10).
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.p50"), 50.0, 20.0);
    BOOST_CHECK(doubleOf(cd, "stats.p10") < doubleOf(cd, "stats.p50"));
    BOOST_CHECK(doubleOf(cd, "stats.p50") < doubleOf(cd, "stats.p90"));
}

// Integer (discrete) arrays: INT32_MIN is the store's null sentinel and must be
// counted as null, not folded into min/mean (review finding: integer null_value
// was never applied).
BOOST_AUTO_TEST_CASE(stats_integer_null_sentinel_is_excluded) {
    auto space = putSpace();
    const int32_t kNull = std::numeric_limits<int32_t>::min();
    const EmlAryMock ary = intArray("RESQML/compute-int/facies", {1, 2, kNull, 3, kNull, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, "STATS");
    BOOST_CHECK(std::get<bool>(cd.at("stats.valid").m_item));
    BOOST_CHECK_EQUAL(longOf(cd, "stats.count"), 4);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.nullCount"), 2);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.min"), 1.0, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.max"), 4.0, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.mean"), 2.5, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.sum"), 10.0, 1e-12);
}

// A correlation id that merely starts with "STATS" is an ORDINARY metadata
// request and must be answered with plain metadata (no stats keys).
BOOST_AUTO_TEST_CASE(stats_prefix_cid_is_plain_metadata) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-prefix/values", {2, 3});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_);
    auto node = rpc.req_.m_dataArrays.extract("STATS");
    node.key() = "STATS_poro";
    rpc.req_.m_dataArrays.insert(std::move(node));
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    BOOST_REQUIRE_EQUAL(rpc.res_.m_arrayMetadata.size(), 1u);
    BOOST_REQUIRE(rpc.res_.m_arrayMetadata.count("STATS_poro"));
    const auto& md = rpc.res_.m_arrayMetadata.at("STATS_poro");
    BOOST_CHECK(!md.m_customData.count("stats.valid"));
    BOOST_REQUIRE_EQUAL(md.m_dimensions.size(), 2u);
}

//=============================== ERROR PATHS ================================
// These assert the resolver's / dispatcher's error channels are reached (and
// surfaced through the handler) rather than crashing or silently succeeding.

// STATS against a dataspace that does not exist: resolveComputeArray fails at
// findSpaceInfo and the handler reports the error.
BOOST_AUTO_TEST_CASE(stats_missing_dataspace_errors) {
    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("no/such/dataspace", "grid/poro");
    rpc.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, "STATS"), int(ENOT_FOUND));
}

// STATS against a real container but a path with no matching array: resolution
// reaches resolve_compute_ary and returns an invalid array -> handler error.
BOOST_AUTO_TEST_CASE(stats_missing_path_errors) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-stats2/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("ignored", "RESQML/compute-stats2/does-not-exist");
    retargetUri(rpc.req_, epc_uri);
    rpc.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, "STATS"), int(ENOT_FOUND));
}

// A malformed STATS marker (review finding: used to fall through to plain
// metadata) is rejected with EINVALID_ARGUMENT and no metadata is returned.
BOOST_AUTO_TEST_CASE(stats_malformed_marker_is_rejected) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-bad/values", {2, 3});
    const std::string epc_uri = setupContainer(space, {ary});

    for (const char* cid : {"STATS|2|1", "STATS|nan|1", "STATS|0|1|0", "STATS|0|1|70000", "STATS|0|0|10", "STATS|1"}) {
        BOOST_TEST_CONTEXT("cid=" << cid) {
            auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
            rpc.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_);
            auto node = rpc.req_.m_dataArrays.extract("STATS");
            node.key() = cid;
            rpc.req_.m_dataArrays.insert(std::move(node));
            retargetUri(rpc.req_, epc_uri);
            rpc.call();
            BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, cid), int(EINVALID_ARGUMENT));
            BOOST_CHECK(rpc.res_.m_arrayMetadata.empty());
        }
    }
}

// When the process-wide compute slots are exhausted the request is rejected
// with ELIMIT_EXCEEDED instead of queueing on the worker thread.
BOOST_AUTO_TEST_CASE(stats_rejected_when_concurrency_limit_reached) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-gate/values", {2, 3});
    const std::string epc_uri = setupContainer(space, {ary});

    // Default cap is kDefaultComputeMaxConcurrency (4): occupy every slot.
    BOOST_REQUIRE_EQUAL(compute_max_concurrency(), kDefaultComputeMaxConcurrency);
    std::vector<std::unique_ptr<ComputeGate>> held;
    for (int i = 0; i < kDefaultComputeMaxConcurrency; ++i) {
        held.emplace_back(std::make_unique<ComputeGate>(kDefaultComputeMaxConcurrency));
        BOOST_REQUIRE(held.back()->acquired_);
    }

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, "STATS"), int(ELIMIT_EXCEEDED));
    BOOST_CHECK(rpc.res_.m_arrayMetadata.empty());
    // A rejected token holds no slot: the counter is unchanged.
    BOOST_CHECK_EQUAL(g_compute_inflight.load(), kDefaultComputeMaxConcurrency);

    // Releasing one slot lets the next request through.
    held.pop_back();
    auto rpc2 = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc2.req_ = DataArrayExt::to_array_stats_req("ignored", ary.path_);
    retargetUri(rpc2.req_, epc_uri);
    rpc2.call();
    BOOST_CHECK_MESSAGE(!hasError(rpc2.rc_), (hasError(rpc2.rc_) ? errorOf(rpc2.rc_) : std::string{}));
    BOOST_CHECK_EQUAL(longOf(statsOf(rpc2.res_, "STATS"), "stats.count"), 6);
    // The dispatcher's own token was released after the synchronous fold.
    BOOST_CHECK_EQUAL(g_compute_inflight.load(), kDefaultComputeMaxConcurrency - 1);
}

//=============================== REDUCE =====================================
// Grid [nk=2, nj=3, ni=4] ramp 0..23: value(i,j,k) = 12k + 4j + i.

namespace {
    const std::vector<double>& mapOf(const std::map<std::string, DataValue>& cd) {
        BOOST_REQUIRE(cd.count("reduce.map"));
        return std::get<ArrayOfDouble>(cd.at("reduce.map").m_item).m_values;
    }
} // namespace

BOOST_AUTO_TEST_CASE(reduce_sum_over_k_reports_2d_map) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-reduce/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_reduce_req("ignored", ary.path_, "sum");
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, "REDUCE|sum");
    BOOST_CHECK_EQUAL(longOf(cd, DataArrayExt::kComputeSchemaKey), DataArrayExt::kComputeSchemaVersion);
    BOOST_CHECK_EQUAL(std::get<std::string>(cd.at("reduce.op").m_item), "sum");
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.ni"), 4);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.nj"), 3);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.nk"), 2);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.columns"), 12);
    const auto& map = mapOf(cd);
    BOOST_REQUIRE_EQUAL(map.size(), 12u);
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 4; ++i)
            BOOST_CHECK_CLOSE(map[j * 4 + i], 12.0 + 2 * (4.0 * j + i), 1e-12); // (4j+i) + (12+4j+i)
    // The reply advertises the reduced 2-D [nj, ni] shape.
    const auto& md = rpc.res_.m_arrayMetadata.at("REDUCE|sum");
    BOOST_REQUIRE_EQUAL(md.m_dimensions.size(), 2u);
    BOOST_CHECK_EQUAL(md.m_dimensions[0], 3);
    BOOST_CHECK_EQUAL(md.m_dimensions[1], 4);
}

BOOST_AUTO_TEST_CASE(reduce_net_with_k_zone_and_range) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-net/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    // k-zone [1,2) -> layer 1 only (values 12..23); range [14, 20]; dz 2.5
    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_reduce_req("ignored", ary.path_, "net", true, 1, 2, true, 14.0, 20.0, true, 2.5);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, rpc.req_.m_dataArrays.begin()->first);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.k0"), 1);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.k1"), 2);
    BOOST_CHECK_CLOSE(doubleOf(cd, "reduce.rangeLo"), 14.0, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "reduce.dz"), 2.5, 1e-12);
    const auto& map = mapOf(cd);
    BOOST_REQUIRE_EQUAL(map.size(), 12u);
    // layer-1 value at column c is 12 + c; in range for c in [2, 8]
    for (int c = 0; c < 12; ++c) {
        BOOST_CHECK_CLOSE(map[c], (c >= 2 && c <= 8) ? 2.5 : 0.0, 1e-12);
    }
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.columns"), 7);
}

BOOST_AUTO_TEST_CASE(reduce_k_zone_beyond_nk_is_clamped_or_rejected) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-kclamp/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    // k1 past the last layer is clamped to nk.
    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_reduce_req("ignored", ary.path_, "max", true, 0, 99);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();
    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    BOOST_CHECK_EQUAL(longOf(statsOf(rpc.res_, rpc.req_.m_dataArrays.begin()->first), "reduce.k1"), 2);

    // k0 at/after the last layer selects nothing: rejected as an argument error.
    auto rpc2 = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc2.req_ = DataArrayExt::to_array_reduce_req("ignored", ary.path_, "max", true, 2, 3);
    retargetUri(rpc2.req_, epc_uri);
    rpc2.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc2.rc_, rpc2.req_.m_dataArrays.begin()->first), int(EINVALID_ARGUMENT));
}

// Operator/argument mismatches are semantic errors answered by the handler
// with EINVALID_ARGUMENT (the marker itself is well-formed).
BOOST_AUTO_TEST_CASE(reduce_operator_argument_mismatch_is_rejected) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-opargs/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    for (const char* cid : {"REDUCE|median", "REDUCE|count", "REDUCE|net", "REDUCE|sum|-|-|0|1",
                            "REDUCE|count|-|-|0|1|2"}) {
        BOOST_TEST_CONTEXT("cid=" << cid) {
            auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
            rpc.req_ = DataArrayExt::to_array_reduce_req("ignored", ary.path_, "sum");
            auto node = rpc.req_.m_dataArrays.extract("REDUCE|sum");
            node.key() = cid;
            rpc.req_.m_dataArrays.insert(std::move(node));
            retargetUri(rpc.req_, epc_uri);
            rpc.call();
            BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, cid), int(EINVALID_ARGUMENT));
            BOOST_CHECK(rpc.res_.m_arrayMetadata.empty());
        }
    }
}

BOOST_AUTO_TEST_CASE(reduce_malformed_marker_is_rejected) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-reduce-bad/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    for (const char* cid : {"REDUCE|", "REDUCE|sum|2|1", "REDUCE|count|-|-|2|1", "REDUCE|net|-|-|0|1|0",
                            "REDUCE|sum|0"}) {
        BOOST_TEST_CONTEXT("cid=" << cid) {
            auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
            rpc.req_ = DataArrayExt::to_array_reduce_req("ignored", ary.path_, "sum");
            auto node = rpc.req_.m_dataArrays.extract("REDUCE|sum");
            node.key() = cid;
            rpc.req_.m_dataArrays.insert(std::move(node));
            retargetUri(rpc.req_, epc_uri);
            rpc.call();
            BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, cid), int(EINVALID_ARGUMENT));
            BOOST_CHECK(rpc.res_.m_arrayMetadata.empty());
        }
    }
}

BOOST_AUTO_TEST_CASE(reduce_integer_null_sentinel_is_excluded) {
    auto space = putSpace();
    const int32_t kNull = std::numeric_limits<int32_t>::min();
    // 1-D int array of 6 -> ni=6, nj=1, nk=1: one layer, mean per column.
    const EmlAryMock ary = intArray("RESQML/compute-reduce-int/facies", {1, kNull, 3, kNull, 5, 6});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_reduce_req("ignored", ary.path_, "mean");
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, "REDUCE|mean");
    const auto& map = mapOf(cd);
    BOOST_REQUIRE_EQUAL(map.size(), 6u);
    BOOST_CHECK_EQUAL(map[0], 1.0);
    BOOST_CHECK(std::isnan(map[1])); // null-only column
    BOOST_CHECK_EQUAL(map[5], 6.0);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.columns"), 4);
}

BOOST_AUTO_TEST_CASE(reduce_missing_path_errors) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-reduce-miss/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_reduce_req("ignored", "RESQML/compute-reduce-miss/nope", "sum");
    retargetUri(rpc.req_, epc_uri);
    rpc.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, "REDUCE|sum"), int(ENOT_FOUND));
}

//=============================== WHERE ======================================

BOOST_AUTO_TEST_CASE(where_reports_count_bbox_centroid) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-where/values", {2, 3, 4}); // 12k + 4j + i
    const std::string epc_uri = setupContainer(space, {ary});

    // [16, 19] -> k=1, j=1, i=0..3
    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_where_req("ignored", ary.path_, 16.0, 19.0);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const std::string cid = rpc.req_.m_dataArrays.begin()->first;
    const auto& cd = statsOf(rpc.res_, cid);
    BOOST_CHECK_EQUAL(longOf(cd, DataArrayExt::kComputeSchemaKey), DataArrayExt::kComputeSchemaVersion);
    BOOST_CHECK(std::get<bool>(cd.at("where.valid").m_item));
    BOOST_CHECK_EQUAL(longOf(cd, "where.count"), 4);
    BOOST_CHECK_EQUAL(longOf(cd, "where.iMin"), 0);
    BOOST_CHECK_EQUAL(longOf(cd, "where.iMax"), 3);
    BOOST_CHECK_EQUAL(longOf(cd, "where.jMin"), 1);
    BOOST_CHECK_EQUAL(longOf(cd, "where.jMax"), 1);
    BOOST_CHECK_EQUAL(longOf(cd, "where.kMin"), 1);
    BOOST_CHECK_EQUAL(longOf(cd, "where.kMax"), 1);
    BOOST_CHECK_CLOSE(doubleOf(cd, "where.ci"), 1.5, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "where.cj"), 1.0, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "where.ck"), 1.0, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd, "where.rangeLo"), 16.0, 1e-12);
    BOOST_CHECK(!cd.count("where.bodyCount")); // single-array WHERE has no body pass
    // Source dims are reported unchanged.
    BOOST_REQUIRE_EQUAL(rpc.res_.m_arrayMetadata.at(cid).m_dimensions.size(), 3u);
}

BOOST_AUTO_TEST_CASE(where_no_hit_is_valid_false) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-where-none/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_where_req("ignored", ary.path_, 100.0, 200.0);
    retargetUri(rpc.req_, epc_uri);
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    const auto& cd = statsOf(rpc.res_, rpc.req_.m_dataArrays.begin()->first);
    BOOST_CHECK(!std::get<bool>(cd.at("where.valid").m_item));
    BOOST_CHECK_EQUAL(longOf(cd, "where.count"), 0);
    BOOST_CHECK(!cd.count("where.iMin"));
}

BOOST_AUTO_TEST_CASE(where_malformed_marker_is_rejected) {
    auto space = putSpace();
    const EmlAryMock ary = rampArray("RESQML/compute-where-bad/values", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {ary});

    for (const char* cid : {"WHERE|2|1", "WHERE|nan|1", "WHERE|1", "WHERE|0|1|2"}) {
        BOOST_TEST_CONTEXT("cid=" << cid) {
            auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
            rpc.req_ = DataArrayExt::to_array_where_req("ignored", ary.path_, 0.0, 1.0);
            auto node = rpc.req_.m_dataArrays.extract(rpc.req_.m_dataArrays.begin());
            node.key() = cid;
            rpc.req_.m_dataArrays.insert(std::move(node));
            retargetUri(rpc.req_, epc_uri);
            rpc.call();
            BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, cid), int(EINVALID_ARGUMENT));
            BOOST_CHECK(rpc.res_.m_arrayMetadata.empty());
        }
    }
}

//=============================== WHEREN =====================================

namespace {
    DataArray::GetDataArrayMetadata multiwhereReq(
        const std::string& epc_uri, const std::vector<std::pair<std::string, std::pair<double, double>>>& preds
    ) {
        std::vector<DataArrayExt::MultiWhereEntry> entries;
        for (const auto& p : preds) {
            DataArrayExt::MultiWhereEntry e;
            e.uri = epc_uri;
            e.path = p.first;
            e.range_lo = p.second.first;
            e.range_hi = p.second.second;
            entries.push_back(std::move(e));
        }
        return DataArrayExt::to_array_multiwhere_req("ignored", entries);
    }
} // namespace

// Two predicates over one grid: every request key is answered - the first
// carries the AND result + connected body, the second a ref back to it.
BOOST_AUTO_TEST_CASE(multiwhere_answers_every_key) {
    auto space = putSpace();
    const EmlAryMock a = rampArray("RESQML/compute-wheren/a", {2, 3, 4}); // 12k + 4j + i
    const EmlAryMock b = rampArray("RESQML/compute-wheren/b", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {a, b});

    // a in [12, 23] -> k=1 (12 cells); b in [0, 15] -> flat 0..15 => AND = flat 12..15 (k=1, j=0)
    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = multiwhereReq(epc_uri, {{a.path_, {12.0, 23.0}}, {b.path_, {0.0, 15.0}}});
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    BOOST_REQUIRE_EQUAL(rpc.res_.m_arrayMetadata.size(), 2u);
    auto it = rpc.req_.m_dataArrays.begin();
    const std::string cid0 = it->first;
    const std::string cid1 = (++it)->first;
    BOOST_REQUIRE(rpc.res_.m_arrayMetadata.count(cid0));
    BOOST_REQUIRE(rpc.res_.m_arrayMetadata.count(cid1));

    const auto& cd0 = rpc.res_.m_arrayMetadata.at(cid0).m_customData;
    BOOST_CHECK(std::get<bool>(cd0.at("where.valid").m_item));
    BOOST_CHECK_EQUAL(longOf(cd0, "where.arrays"), 2);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.arraysStreamed"), 2);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.count"), 4);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.iMin"), 0);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.iMax"), 3);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.jMin"), 0);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.jMax"), 0);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.kMin"), 1);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.kMax"), 1);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.components"), 1);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.bodyCount"), 4);
    BOOST_CHECK_CLOSE(doubleOf(cd0, "where.bci"), 1.5, 1e-12);
    BOOST_CHECK_CLOSE(doubleOf(cd0, "where.rangeLo"), 12.0, 1e-12);

    const auto& cd1 = rpc.res_.m_arrayMetadata.at(cid1).m_customData;
    BOOST_REQUIRE(cd1.count("where.ref"));
    BOOST_CHECK_EQUAL(std::get<std::string>(cd1.at("where.ref").m_item), cid0);
    BOOST_CHECK_CLOSE(doubleOf(cd1, "where.rangeHi"), 15.0, 1e-12);
    BOOST_CHECK(!cd1.count("where.count"));
    BOOST_REQUIRE_EQUAL(rpc.res_.m_arrayMetadata.at(cid1).m_dimensions.size(), 3u);
}

// Once the AND-mask is empty the remaining arrays are not read.
BOOST_AUTO_TEST_CASE(multiwhere_short_circuits_on_empty_mask) {
    auto space = putSpace();
    const EmlAryMock a = rampArray("RESQML/compute-wheren-sc/a", {2, 3, 4});
    const EmlAryMock b = rampArray("RESQML/compute-wheren-sc/b", {2, 3, 4});
    const EmlAryMock c = rampArray("RESQML/compute-wheren-sc/c", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {a, b, c});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = multiwhereReq(epc_uri, {{a.path_, {0.0, 5.0}}, {b.path_, {20.0, 23.0}}, {c.path_, {0.0, 23.0}}});
    rpc.call();

    BOOST_REQUIRE_MESSAGE(!hasError(rpc.rc_), (hasError(rpc.rc_) ? errorOf(rpc.rc_) : std::string{}));
    BOOST_REQUIRE_EQUAL(rpc.res_.m_arrayMetadata.size(), 3u);
    const auto& cd0 = rpc.res_.m_arrayMetadata.at(rpc.req_.m_dataArrays.begin()->first).m_customData;
    BOOST_CHECK(!std::get<bool>(cd0.at("where.valid").m_item));
    BOOST_CHECK_EQUAL(longOf(cd0, "where.count"), 0);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.arrays"), 3);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.arraysStreamed"), 2);
}

BOOST_AUTO_TEST_CASE(multiwhere_shape_mismatch_is_rejected_before_streaming) {
    auto space = putSpace();
    const EmlAryMock a = rampArray("RESQML/compute-wheren-shape/a", {2, 3, 4});
    const EmlAryMock b = rampArray("RESQML/compute-wheren-shape/b", {3, 2, 4});
    const std::string epc_uri = setupContainer(space, {a, b});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = multiwhereReq(epc_uri, {{a.path_, {0.0, 23.0}}, {b.path_, {0.0, 23.0}}});
    rpc.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, rpc.req_.m_dataArrays.begin()->first), int(EINVALID_STATE));
    BOOST_CHECK(rpc.res_.m_arrayMetadata.empty());
}

BOOST_AUTO_TEST_CASE(multiwhere_missing_array_errors) {
    auto space = putSpace();
    const EmlAryMock a = rampArray("RESQML/compute-wheren-miss/a", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {a});

    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = multiwhereReq(epc_uri, {{a.path_, {0.0, 23.0}}, {"RESQML/compute-wheren-miss/nope", {0.0, 1.0}}});
    rpc.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, rpc.req_.m_dataArrays.begin()->first), int(ENOT_FOUND));
}

BOOST_AUTO_TEST_CASE(multiwhere_malformed_batch_is_rejected) {
    auto space = putSpace();
    const EmlAryMock a = rampArray("RESQML/compute-wheren-bad/a", {2, 3, 4});
    const std::string epc_uri = setupContainer(space, {a});

    // hi < lo in the second entry
    auto rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc.req_ = multiwhereReq(epc_uri, {{a.path_, {0.0, 23.0}}, {a.path_, {5.0, 1.0}}});
    rpc.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc.rc_, rpc.req_.m_dataArrays.begin()->first), int(EINVALID_ARGUMENT));
    BOOST_CHECK(rpc.res_.m_arrayMetadata.empty());

    // a WHEREN key mixed with a plain key
    auto rpc2 = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);
    rpc2.req_ = multiwhereReq(epc_uri, {{a.path_, {0.0, 23.0}}});
    common12::AryIdent ident;
    ident.m_uri = epc_uri;
    ident.m_pathInResource = a.path_;
    rpc2.req_.m_dataArrays.emplace("plain", std::move(ident));
    rpc2.call();
    BOOST_CHECK_EQUAL(errorCodeOf(rpc2.rc_, rpc2.req_.m_dataArrays.begin()->first), int(EINVALID_ARGUMENT));
    BOOST_CHECK(rpc2.res_.m_arrayMetadata.empty());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
