// ============================================================================
// Copyright 2026 Emerson Paradigm Holding LLC. All rights reserved.
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

// WARNING: Custom extension. NOT part of official ETP 1.2
//
// Client <-> server WebSocket round trips for the array-compute extensions
// (STATS, REDUCE, WHERE, WHEREN). Unlike ComputeResolveTests (which call the
// OpenKV handler directly over a PG fixture) these go through the real ETP
// client library: session negotiation (the supportedArrayExtensions advert +
// the client-side guard), Avro encoding of the GetDataArrayMetadata extension
// request, the server dispatcher, and the decoded reply / ProtocolException
// on the way back.

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/eml/OpenKVClientServerFixture.h>

#include <oes/client/Client.hpp>
#include <oes/common/etp12/EmlUri.h>
#include <oes/eml/etp12/Extensions.h>

#include <oes/core/utils/Logger.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/eml/TestingMocks.h>

#include <cmath>
#include <numeric>

using namespace oes::eml::etp12;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;
using oes::common::etp12::EmlUri;

namespace {

constexpr std::chrono::milliseconds kTimeout(10'000);

//============================================================================
// A dataspace with one EpcExternalPartReference container, a 3-D ramp array
// [nk=2, nj=3, ni=4] (values 0..23) stored at the path a Grid2dRepresentation
// declares under the container (OpenKV links arrays by declared path, not
// shape, and the commit refuses orphan arrays), and a RESQML 2.0.1 trajectory
// whose arrays are stored under the same container but addressed through the
// OWNER object URI (the AryContainerResolver path).

struct ComputeWireFixture : public OpenKVClientServerFixture {
    const std::string space_path_ = "EPCs/compute-wire";
    const std::vector<int64_t> grid_dims_{ 2, 3, 4 };
    std::string grid_path_;
    std::string proxy_uri_;
    std::string owner_uri_;
    std::string owner_path_;
    int64_t owner_count_ = 0;

    static int64_t product(const std::vector<int64_t>& dims) {
        return std::accumulate(dims.begin(), dims.end(), int64_t(1), std::multiplies<int64_t>());
    }

    static ArrayOfDouble ramp(int64_t count) {
        ArrayOfDouble ary;
        ary.m_values.resize(size_t(count));
        std::iota(ary.m_values.begin(), ary.m_values.end(), 0.0);
        return ary;
    }

    void createSpace(oes::client::Client& clt) {
        auto rpc = oes::client::rpcFor(clt.space_proto(), &IDataspace::PutDataspaces);
        Object::Dataspace space;
        space.m_path = space_path_;
        space.m_uri = EmlUri::make_space(space_path_).to_string();
        rpc.req_.m_dataspaces.emplace("1", space);
        std::ostringstream oss;
        auto ok = rpc.call(oss, kTimeout);
        if (!call_succeeded(ok, oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
        }
        current_space_ = space_path_;
    }

    // Everything is written through the public ETP protocols (Store +
    // DataArray) inside one transaction, exactly as a client would.
    void seed(oes::client::Client& clt) {
        const EmlObjMock& proxy = epc_ext_mock();
        const EmlObjMock& grid = grid2d_mock();
        EmlObjMock owner = traj_mock();
        // The mock trajectory points at an interpretation we do not store;
        // drop the reference so the transaction's referential check passes.
        EmlObjMock::erase_tag("resqml:RepresentedInterpretation", owner.xml_);
        const EmlUri space_uri = EmlUri::make_space(space_path_);

        BOOST_REQUIRE(!grid.arrays_.empty());
        grid_path_ = grid.arrays_.front().path_;

        proxy_uri_ = EmlUri::make_obj(space_path_, proxy.type_, proxy.guid_, EmlUri::eEML20).to_string();
        owner_uri_ = space_uri.child_type(owner.type_, EmlUri::eRESQML20).child_obj(owner.guid_).to_string();
        BOOST_REQUIRE(!owner.arrays_.empty());
        owner_path_ = owner.arrays_.front().path_;
        owner_count_ = owner.arrays_.front().size();

        oes::client::ScopedTransaction tx(clt, kTimeout);
        BOOST_REQUIRE(tx.start(space_uri.to_string(), "compute-wire", false));

        auto obj_rpc = oes::client::rpcFor(clt.store_proto(), &IStore::PutDataObjects);
        int key = 0;
        for (const EmlObjMock* mock : std::initializer_list<const EmlObjMock*>{
            &crs_mock(), &datum_mock(), &proxy, &grid, &owner
        }) {
            Object::DataObject obj;
            obj.m_resource.m_uri = space_uri.child_type(mock->type_, EmlUri::eRESQML20).child_obj(mock->guid_).to_string();
            obj.m_resource.m_name = mock->name_;
            obj.m_format = "xml";
            obj.m_data.assign(mock->xml_.begin(), mock->xml_.end());
            obj_rpc.req_.m_dataObjects.emplace(std::to_string(++key), std::move(obj));
        }
        std::ostringstream obj_oss;
        auto obj_ok = obj_rpc.call(obj_oss, kTimeout);
        if (!call_succeeded(obj_ok, obj_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(obj_ok, obj_oss));
        }

        auto ary_rpc = oes::client::rpcFor(clt.array_proto(), &IDataArray::PutDataArrays);
        {
            DataArrayTypes::PutDataArraysType ary;
            ary.m_uid.m_uri = proxy_uri_;
            ary.m_uid.m_pathInResource = grid_path_;
            ary.m_array.m_dimensions = grid_dims_;
            ary.m_array.m_data.m_item = ramp(product(grid_dims_));
            ary_rpc.req_.m_dataArrays.emplace(std::to_string(++key), std::move(ary));
        }
        for (const EmlAryMock& mock : owner.arrays_) {
            DataArrayTypes::PutDataArraysType ary;
            ary.m_uid.m_uri = proxy_uri_;
            ary.m_uid.m_pathInResource = mock.path_;
            ary.m_array.m_dimensions = mock.dims_;
            ary.m_array.m_data.m_item = ramp(mock.size());
            ary_rpc.req_.m_dataArrays.emplace(std::to_string(++key), std::move(ary));
        }
        std::ostringstream ary_oss;
        auto ary_ok = ary_rpc.call(ary_oss, kTimeout);
        if (!call_succeeded(ary_ok, ary_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ary_ok, ary_oss));
        }

        BOOST_REQUIRE(tx.commit());
    }

    Client_uptr startAndSeed() {
        initServer();
        startServer();
        auto clt = connectClient();
        createSpace(*clt);
        seed(*clt);
        return clt;
    }

    // Re-point every entry of a built extension request at the given URI
    // (the builders default to the bare dataspace URI).
    template <typename Req>
    static void retargetUri(Req& req, const std::string& uri) {
        for (auto& kv : req.m_dataArrays) {
            kv.second.m_uri = uri;
        }
    }

    static int64_t longOf(const std::map<std::string, DataValue>& cd, const char* k) {
        BOOST_REQUIRE_MESSAGE(cd.count(k), "missing customData key " << k);
        return std::get<int64_t>(cd.at(k).m_item);
    }
    static double doubleOf(const std::map<std::string, DataValue>& cd, const char* k) {
        BOOST_REQUIRE_MESSAGE(cd.count(k), "missing customData key " << k);
        return std::get<double>(cd.at(k).m_item);
    }
    static bool boolOf(const std::map<std::string, DataValue>& cd, const char* k) {
        BOOST_REQUIRE_MESSAGE(cd.count(k), "missing customData key " << k);
        return std::get<bool>(cd.at(k).m_item);
    }

    // Issue a compute request over the wire; on success returns the single
    // reply entry's customData, on a ProtocolException fails the test.
    template <typename Rpc>
    static const std::map<std::string, DataValue>& callOk(Rpc& rpc, const std::string& cid) {
        std::ostringstream oss;
        auto ok = rpc.call(oss, kTimeout);
        if (!call_succeeded(ok, oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
        }
        BOOST_REQUIRE_EQUAL(rpc.res_.m_arrayMetadata.size(), 1u);
        BOOST_REQUIRE(rpc.res_.m_arrayMetadata.count(cid));
        return rpc.res_.m_arrayMetadata.at(cid).m_customData;
    }

    // Issue a compute request expected to FAIL; returns the ETP error code
    // reported for \p cid (or the message-level error when the server used one).
    template <typename Rpc>
    static int32_t callErr(Rpc& rpc, const std::string& cid) {
        std::ostringstream oss;
        auto ok = rpc.call(oss, kTimeout);
        BOOST_REQUIRE_MESSAGE(!call_succeeded(ok, oss), "expected a ProtocolException for " << cid);
        BOOST_REQUIRE_MESSAGE(ok.second == std::future_status::ready, "request timed out");
        if (rpc.rc_.ex.m_errors.count(cid)) {
            return rpc.rc_.ex.m_errors.at(cid).m_code;
        }
        BOOST_REQUIRE_MESSAGE(rpc.rc_.ex.m_error, "no error for key " << cid);
        return rpc.rc_.ex.m_error->m_code;
    }
};

//============================================================================

BOOST_FIXTURE_TEST_SUITE(Etp12ComputeTests, ComputeWireFixture)

// The server advertises the extensions at negotiation and the client guard
// opens the compute routes on that advert alone.
BOOST_AUTO_TEST_CASE(negotiation_advertises_compute_and_guard_opens) {
    initServer();
    startServer();
    auto clt = connectClient();

    BOOST_CHECK(clt->server_supports_array_compute());
    const std::string& exts = clt->server_array_compute_extensions();
    BOOST_CHECK_MESSAGE(exts.find("stats") != std::string::npos, "advert: " << exts);
    BOOST_CHECK_MESSAGE(exts.find("histogram") != std::string::npos, "advert: " << exts);
    BOOST_CHECK_MESSAGE(exts.find("reduce") != std::string::npos, "advert: " << exts);
    BOOST_CHECK_MESSAGE(exts.find("where") != std::string::npos, "advert: " << exts);
    BOOST_CHECK_MESSAGE(exts.find("wheren") != std::string::npos, "advert: " << exts);

    ErrorInfo info;
    info.m_code = 0;
    BOOST_CHECK(clt->guard_array_compute(&info));
    BOOST_CHECK_EQUAL(info.m_code, 0); // untouched when supported
}

BOOST_AUTO_TEST_CASE(stats_round_trip_on_container_uri) {
    auto clt = startAndSeed();

    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req(space_path_, grid_path_);
    retargetUri(rpc.req_, proxy_uri_);

    const auto& cd = callOk(rpc, "STATS");
    BOOST_CHECK_EQUAL(longOf(cd, DataArrayExt::kComputeSchemaKey), DataArrayExt::kComputeSchemaVersion);
    BOOST_CHECK(boolOf(cd, "stats.valid"));
    BOOST_CHECK_EQUAL(longOf(cd, "stats.count"), 24);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.nullCount"), 0);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.min"), 0.0, 1e-9);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.max"), 23.0, 1e-9);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.mean"), 11.5, 1e-9);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.sum"), 276.0, 1e-9);
    BOOST_CHECK(!cd.count("stats.histogram"));

    // The reply still carries the array's real metadata (dims + type).
    const auto& md = rpc.res_.m_arrayMetadata.at("STATS");
    BOOST_CHECK(md.m_dimensions == grid_dims_);
    BOOST_CHECK(md.m_transportArrayType == AnyArrayType::eArrayOfDouble);
}

// RESQML 2.0.1: the request names the OWNING data object, not the HDF proxy.
BOOST_AUTO_TEST_CASE(stats_round_trip_on_owner_uri) {
    auto clt = startAndSeed();

    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req(space_path_, owner_path_);
    retargetUri(rpc.req_, owner_uri_);

    const auto& cd = callOk(rpc, "STATS");
    BOOST_CHECK(boolOf(cd, "stats.valid"));
    BOOST_CHECK_EQUAL(longOf(cd, "stats.count"), owner_count_);
    BOOST_CHECK_CLOSE(doubleOf(cd, "stats.max"), double(owner_count_ - 1), 1e-9);
}

BOOST_AUTO_TEST_CASE(stats_histogram_round_trip) {
    auto clt = startAndSeed();

    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_stats_req(space_path_, grid_path_, true, 0.0, 24.0, 4);
    retargetUri(rpc.req_, proxy_uri_);
    const std::string cid = rpc.req_.m_dataArrays.begin()->first;
    BOOST_REQUIRE_EQUAL(cid.rfind("STATS|", 0), 0u);

    const auto& cd = callOk(rpc, cid);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.inRangeCount"), 24);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.histBins"), 4);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.histUnderflow"), 0);
    BOOST_CHECK_EQUAL(longOf(cd, "stats.histOverflow"), 0);
    BOOST_REQUIRE(cd.count("stats.histogram"));
    const auto& hist = std::get<ArrayOfLong>(cd.at("stats.histogram").m_item).m_values;
    BOOST_REQUIRE_EQUAL(hist.size(), 4u);
    for (int64_t c : hist) {
        BOOST_CHECK_EQUAL(c, 6); // 24 values, 6 per bin of width 6
    }
    const double p50 = doubleOf(cd, "stats.p50");
    BOOST_CHECK_MESSAGE(p50 >= 6.0 && p50 <= 18.0, "p50 within one bin of 11.5: " << p50);
}

// Negative paths cross the wire as a ProtocolException keyed on the request
// entry, never as a silent fall-back to plain metadata.
BOOST_AUTO_TEST_CASE(stats_errors_round_trip) {
    auto clt = startAndSeed();

    { // malformed marker (lo > hi) -> EINVALID_ARGUMENT
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_stats_req(space_path_, grid_path_, true, 10.0, 5.0);
        retargetUri(rpc.req_, proxy_uri_);
        const std::string cid = rpc.req_.m_dataArrays.begin()->first;
        BOOST_CHECK_EQUAL(callErr(rpc, cid), int32_t(EINVALID_ARGUMENT));
    }
    { // histogram bins above the hard cap -> EINVALID_ARGUMENT
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_stats_req(
            space_path_, grid_path_, true, 0.0, 24.0, DataArrayExt::kMaxStatsHistBins + 1
        );
        retargetUri(rpc.req_, proxy_uri_);
        const std::string cid = rpc.req_.m_dataArrays.begin()->first;
        BOOST_CHECK_EQUAL(callErr(rpc, cid), int32_t(EINVALID_ARGUMENT));
    }
    { // unknown array -> ENOT_FOUND
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_stats_req(space_path_, "RESQML/compute-wire/missing");
        retargetUri(rpc.req_, proxy_uri_);
        BOOST_CHECK_EQUAL(callErr(rpc, "STATS"), int32_t(ENOT_FOUND));
    }
    { // a plain correlation id that merely starts with STATS is NOT a compute
      // request: it is answered as ordinary metadata (no compute keys).
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        DataArrayTypes::DataArrayIdentifier ident;
        ident.m_uri = proxy_uri_;
        ident.m_pathInResource = grid_path_;
        rpc.req_.m_dataArrays.emplace("STATS_poro", ident);
        const auto& cd = callOk(rpc, "STATS_poro");
        BOOST_CHECK(!cd.count(DataArrayExt::kComputeSchemaKey));
        BOOST_CHECK(!cd.count("stats.count"));
    }
}

//=============================== REDUCE =====================================

// Grid [nk=2, nj=3, ni=4], value 12k + 4j + i: sum over K is a [3 x 4] map
// carried as a native ArrayOfDouble in customData.
BOOST_AUTO_TEST_CASE(reduce_sum_round_trip) {
    auto clt = startAndSeed();

    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_reduce_req(space_path_, grid_path_, "sum");
    retargetUri(rpc.req_, proxy_uri_);

    const auto& cd = callOk(rpc, "REDUCE|sum");
    BOOST_CHECK_EQUAL(longOf(cd, DataArrayExt::kComputeSchemaKey), DataArrayExt::kComputeSchemaVersion);
    BOOST_CHECK_EQUAL(std::get<std::string>(cd.at("reduce.op").m_item), "sum");
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.ni"), 4);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.nj"), 3);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.nk"), 2);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.columns"), 12);
    BOOST_REQUIRE(cd.count("reduce.map"));
    const auto& map = std::get<ArrayOfDouble>(cd.at("reduce.map").m_item).m_values;
    BOOST_REQUIRE_EQUAL(map.size(), 12u);
    for (int j = 0; j < 3; ++j) {
        for (int i = 0; i < 4; ++i) {
            BOOST_CHECK_CLOSE(map[size_t(j * 4 + i)], 12.0 + 2 * (4.0 * j + i), 1e-12);
        }
    }
    const auto& md = rpc.res_.m_arrayMetadata.at("REDUCE|sum");
    BOOST_REQUIRE_EQUAL(md.m_dimensions.size(), 2u); // reduced [nj, ni] shape
    BOOST_CHECK_EQUAL(md.m_dimensions[0], 3);
    BOOST_CHECK_EQUAL(md.m_dimensions[1], 4);
}

BOOST_AUTO_TEST_CASE(reduce_net_with_k_zone_and_range_round_trip) {
    auto clt = startAndSeed();

    // k-zone [1,2) -> layer 1 only (values 12..23); range [14, 20]; dz 2.5
    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_reduce_req(
        space_path_, grid_path_, "net", true, 1, 2, true, 14.0, 20.0, true, 2.5
    );
    retargetUri(rpc.req_, proxy_uri_);
    const std::string cid = rpc.req_.m_dataArrays.begin()->first;

    const auto& cd = callOk(rpc, cid);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.k0"), 1);
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.k1"), 2);
    BOOST_CHECK_CLOSE(doubleOf(cd, "reduce.dz"), 2.5, 1e-12);
    const auto& map = std::get<ArrayOfDouble>(cd.at("reduce.map").m_item).m_values;
    BOOST_REQUIRE_EQUAL(map.size(), 12u);
    for (int c = 0; c < 12; ++c) { // layer-1 value at column c is 12 + c
        BOOST_CHECK_CLOSE(map[size_t(c)], (c >= 2 && c <= 8) ? 2.5 : 0.0, 1e-12);
    }
    BOOST_CHECK_EQUAL(longOf(cd, "reduce.columns"), 7);
}

BOOST_AUTO_TEST_CASE(reduce_errors_round_trip) {
    auto clt = startAndSeed();

    { // operator/argument mismatch -> EINVALID_ARGUMENT keyed on the entry
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_reduce_req(space_path_, grid_path_, "count"); // needs lo|hi
        retargetUri(rpc.req_, proxy_uri_);
        BOOST_CHECK_EQUAL(callErr(rpc, "REDUCE|count"), int32_t(EINVALID_ARGUMENT));
    }
    { // K-zone outside the grid -> EINVALID_ARGUMENT
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_reduce_req(space_path_, grid_path_, "max", true, 2, 3);
        retargetUri(rpc.req_, proxy_uri_);
        BOOST_CHECK_EQUAL(callErr(rpc, rpc.req_.m_dataArrays.begin()->first), int32_t(EINVALID_ARGUMENT));
    }
    { // unknown array -> ENOT_FOUND
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_reduce_req(space_path_, "RESQML/compute-wire/missing", "sum");
        retargetUri(rpc.req_, proxy_uri_);
        BOOST_CHECK_EQUAL(callErr(rpc, "REDUCE|sum"), int32_t(ENOT_FOUND));
    }
}

//=============================== WHERE ======================================

BOOST_AUTO_TEST_CASE(where_round_trip) {
    auto clt = startAndSeed();

    // [16, 19] -> k=1, j=1, i=0..3
    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_where_req(space_path_, grid_path_, 16.0, 19.0);
    retargetUri(rpc.req_, proxy_uri_);
    const std::string cid = rpc.req_.m_dataArrays.begin()->first;

    const auto& cd = callOk(rpc, cid);
    BOOST_CHECK_EQUAL(longOf(cd, DataArrayExt::kComputeSchemaKey), DataArrayExt::kComputeSchemaVersion);
    BOOST_CHECK(boolOf(cd, "where.valid"));
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
    BOOST_CHECK(!cd.count("where.bodyCount")); // single-array WHERE has no body pass
    BOOST_CHECK(rpc.res_.m_arrayMetadata.at(cid).m_dimensions == grid_dims_);

    // No hit: valid=false, count=0, no bbox.
    auto none = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    none.req_ = DataArrayExt::to_array_where_req(space_path_, grid_path_, 100.0, 200.0);
    retargetUri(none.req_, proxy_uri_);
    const auto& cd_none = callOk(none, none.req_.m_dataArrays.begin()->first);
    BOOST_CHECK(!boolOf(cd_none, "where.valid"));
    BOOST_CHECK_EQUAL(longOf(cd_none, "where.count"), 0);
    BOOST_CHECK(!cd_none.count("where.iMin"));
}

BOOST_AUTO_TEST_CASE(where_malformed_marker_round_trip) {
    auto clt = startAndSeed();

    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_where_req(space_path_, grid_path_, 2.0, 1.0); // lo > hi
    retargetUri(rpc.req_, proxy_uri_);
    BOOST_CHECK_EQUAL(callErr(rpc, rpc.req_.m_dataArrays.begin()->first), int32_t(EINVALID_ARGUMENT));
}

//=============================== WHEREN =====================================

// Two predicates over the same grid: every request key is answered - the
// first carries the AND result + connected body, the second a ref back to it.
BOOST_AUTO_TEST_CASE(multiwhere_round_trip_answers_every_key) {
    auto clt = startAndSeed();

    // [12, 23] -> k=1 (12 cells); [0, 15] -> flat 0..15 => AND = flat 12..15 (k=1, j=0)
    std::vector<DataArrayExt::MultiWhereEntry> entries(2);
    entries[0].uri = proxy_uri_; entries[0].path = grid_path_; entries[0].range_lo = 12.0; entries[0].range_hi = 23.0;
    entries[1].uri = proxy_uri_; entries[1].path = grid_path_; entries[1].range_lo = 0.0;  entries[1].range_hi = 15.0;

    auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
    rpc.req_ = DataArrayExt::to_array_multiwhere_req(space_path_, entries);
    BOOST_REQUIRE_EQUAL(rpc.req_.m_dataArrays.size(), 2u);
    auto it = rpc.req_.m_dataArrays.begin();
    const std::string cid0 = it->first;
    const std::string cid1 = (++it)->first;

    std::ostringstream oss;
    auto ok = rpc.call(oss, kTimeout);
    if (!call_succeeded(ok, oss)) {
        BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
    }
    BOOST_REQUIRE_EQUAL(rpc.res_.m_arrayMetadata.size(), 2u);
    BOOST_REQUIRE(rpc.res_.m_arrayMetadata.count(cid0));
    BOOST_REQUIRE(rpc.res_.m_arrayMetadata.count(cid1));

    const auto& cd0 = rpc.res_.m_arrayMetadata.at(cid0).m_customData;
    BOOST_CHECK(boolOf(cd0, "where.valid"));
    BOOST_CHECK_EQUAL(longOf(cd0, "where.arrays"), 2);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.arraysStreamed"), 2);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.count"), 4);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.jMin"), 0);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.jMax"), 0);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.kMin"), 1);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.components"), 1);
    BOOST_CHECK_EQUAL(longOf(cd0, "where.bodyCount"), 4);
    BOOST_CHECK_CLOSE(doubleOf(cd0, "where.bci"), 1.5, 1e-12);

    const auto& cd1 = rpc.res_.m_arrayMetadata.at(cid1).m_customData;
    BOOST_REQUIRE(cd1.count("where.ref"));
    BOOST_CHECK_EQUAL(std::get<std::string>(cd1.at("where.ref").m_item), cid0);
    BOOST_CHECK_CLOSE(doubleOf(cd1, "where.rangeHi"), 15.0, 1e-12);
    BOOST_CHECK(!cd1.count("where.count"));
}

BOOST_AUTO_TEST_CASE(multiwhere_errors_round_trip) {
    auto clt = startAndSeed();

    { // more than kMaxMultiWhereEntries predicates -> EINVALID_ARGUMENT (parse-level)
        std::vector<DataArrayExt::MultiWhereEntry> entries(DataArrayExt::kMaxMultiWhereEntries + 1);
        for (auto& e : entries) {
            e.uri = proxy_uri_; e.path = grid_path_; e.range_lo = 0.0; e.range_hi = 1.0;
        }
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_multiwhere_req(space_path_, entries);
        BOOST_CHECK_EQUAL(callErr(rpc, rpc.req_.m_dataArrays.begin()->first), int32_t(EINVALID_ARGUMENT));
    }
    { // one predicate names a missing array -> ENOT_FOUND. The batch is ONE
      // query with one answer, so the error is reported once, under the FIRST
      // key (never under the offending entry's key).
        std::vector<DataArrayExt::MultiWhereEntry> entries(2);
        entries[0].uri = proxy_uri_; entries[0].path = grid_path_; entries[0].range_lo = 0.0; entries[0].range_hi = 1.0;
        entries[1].uri = proxy_uri_; entries[1].path = "RESQML/compute-wire/missing"; entries[1].range_lo = 0.0; entries[1].range_hi = 1.0;
        auto rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::GetDataArrayMetadata);
        rpc.req_ = DataArrayExt::to_array_multiwhere_req(space_path_, entries);
        auto it = rpc.req_.m_dataArrays.begin();
        const std::string cid0 = it->first;
        const std::string cid1 = (++it)->first;
        BOOST_CHECK_EQUAL(callErr(rpc, cid0), int32_t(ENOT_FOUND));
        BOOST_CHECK(!rpc.rc_.ex.m_errors.count(cid1));
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
