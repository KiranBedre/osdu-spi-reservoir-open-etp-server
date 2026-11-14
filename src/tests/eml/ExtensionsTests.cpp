// ============================================================================
// Copyright 2023-2026 Emerson Paradigm Holding LLC. All rights reserved.
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

// Coverage for the custom GetDataArrayMetadata compute EXTENSION markers
// (STATS / REDUCE / WHERE / WHEREN): every
// to_array_stats_req() builder output must be recognised and parsed back by
// is_array_stats_req(); a plain (non-extension) metadata request must not be
// detected; and a marker that IS a STATS request but is malformed must be
// reported as such (detected + err set) so the server rejects it instead of
// silently serving plain metadata.

#include <oes/eml/etp12/Extensions.h>
#include <oes/common/etp12/ArrayComputeCap.h>

#include <string>
#include <vector>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

using namespace oes::eml::etp12::DataArrayExt;
namespace common12 = oes::common::etp12;
using Meta = oes::common::etp12::Protocol::DataArray::GetDataArrayMetadata;

const std::string kDs = "demo";

Meta meta_with_cid(const std::string& cid) {
    Meta req;
    common12::AryIdent ident;
    ident.m_uri = "eml:///dataspace('demo')";
    ident.m_pathInResource = "grid/poro";
    req.m_dataArrays.emplace(cid, std::move(ident));
    return req;
}

BOOST_AUTO_TEST_SUITE(ExtensionsTests)

//=============================== STATS ======================================

BOOST_AUTO_TEST_CASE(stats_plain_round_trip) {
    Meta req = to_array_stats_req(kDs, "grid/poro");
    StatsQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_stats_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.cid, "STATS");
    BOOST_CHECK_EQUAL(q.path, "grid/poro");
    BOOST_CHECK(!q.uri.empty());
    BOOST_CHECK(!q.has_range);
    BOOST_CHECK(!q.has_histogram);
}

BOOST_AUTO_TEST_CASE(stats_range_round_trip) {
    Meta req = to_array_stats_req(kDs, "grid/poro", true, 0.1, 0.3);
    StatsQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_stats_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK(q.has_range);
    BOOST_CHECK_CLOSE(q.range_lo, 0.1, 1e-9);
    BOOST_CHECK_CLOSE(q.range_hi, 0.3, 1e-9);
    BOOST_CHECK(!q.has_histogram);
}

BOOST_AUTO_TEST_CASE(stats_range_lo_equal_hi_is_valid_without_histogram) {
    Meta req = to_array_stats_req(kDs, "grid/poro", true, 0.5, 0.5);
    StatsQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_stats_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK(q.has_range);
    BOOST_CHECK(!q.has_histogram);
}

BOOST_AUTO_TEST_CASE(stats_histogram_round_trip) {
    Meta req = to_array_stats_req(kDs, "grid/poro", true, 0.0, 1.0, 20);
    StatsQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_stats_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK(q.has_range);
    BOOST_CHECK(q.has_histogram);
    BOOST_CHECK_EQUAL(q.hist_bins, 20u);
}

BOOST_AUTO_TEST_CASE(stats_histogram_max_bins_round_trip) {
    Meta req = to_array_stats_req(kDs, "grid/poro", true, 0.0, 1.0, kMaxStatsHistBins);
    StatsQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_stats_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.hist_bins, kMaxStatsHistBins);
}

// The builder emits bounds with %.17g so a double survives the text marker
// bit-exactly (no 1e-9 tolerance needed).
BOOST_AUTO_TEST_CASE(stats_bounds_survive_round_trip_exactly) {
    const double lo = 0.1 + 0.2; // 0.30000000000000004
    const double hi = 1e300;
    Meta req = to_array_stats_req(kDs, "grid/poro", true, lo, hi);
    StatsQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_stats_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.range_lo, lo);
    BOOST_CHECK_EQUAL(q.range_hi, hi);
}

//===================== plain request is not an extension ====================

BOOST_AUTO_TEST_CASE(plain_metadata_is_not_an_extension) {
    StatsQuery q;
    std::string err;
    BOOST_CHECK(!is_array_stats_req(meta_with_cid("client-correlation-1"), q, err));
    BOOST_CHECK(err.empty());
}

// A correlation id that merely STARTS with the letters is an ordinary metadata
// request, not a compute marker (review finding: "STATS_poro" used to match).
BOOST_AUTO_TEST_CASE(cid_with_stats_prefix_is_not_an_extension) {
    StatsQuery q;
    std::string err;
    BOOST_CHECK(!is_array_stats_req(meta_with_cid("STATS_poro"), q, err));
    BOOST_CHECK(!is_array_stats_req(meta_with_cid("STATSX"), q, err));
    BOOST_CHECK(!is_array_stats_req(meta_with_cid("stats"), q, err));
    BOOST_CHECK(err.empty());
}

BOOST_AUTO_TEST_CASE(multi_array_request_is_not_an_extension) {
    Meta req = meta_with_cid("STATS");
    common12::AryIdent ident;
    ident.m_uri = "eml:///dataspace('demo')";
    ident.m_pathInResource = "grid/perm";
    req.m_dataArrays.emplace("STATS|0|1", std::move(ident));
    StatsQuery q;
    std::string err;
    BOOST_CHECK(!is_array_stats_req(req, q, err));
}

//================== malformed markers are detected AND rejected =============

void check_malformed(const std::string& cid) {
    StatsQuery q;
    std::string err;
    BOOST_TEST_CONTEXT("cid=" << cid) {
        BOOST_CHECK(is_array_stats_req(meta_with_cid(cid), q, err));
        BOOST_CHECK(!err.empty());
        BOOST_CHECK_NE(err.find(cid), std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(malformed_token_count_is_rejected) {
    check_malformed("STATS|");
    check_malformed("STATS|1");
    check_malformed("STATS|0|1|10|extra");
}

BOOST_AUTO_TEST_CASE(non_finite_or_non_numeric_bounds_are_rejected) {
    check_malformed("STATS|nan|1");
    check_malformed("STATS|0|inf");
    check_malformed("STATS|-inf|0");
    check_malformed("STATS|1e400|2");
    check_malformed("STATS|abc|1");
    check_malformed("STATS|1x|2");
    check_malformed("STATS||1");
}

BOOST_AUTO_TEST_CASE(inverted_range_is_rejected) {
    check_malformed("STATS|2|1");
}

BOOST_AUTO_TEST_CASE(bad_bin_counts_are_rejected) {
    check_malformed("STATS|0|1|0");
    check_malformed("STATS|0|1|-1");
    check_malformed("STATS|0|1|70000");
    check_malformed("STATS|0|1|5000000");
    check_malformed("STATS|0|1|ten");
    check_malformed("STATS|0|1|1.5");
}

BOOST_AUTO_TEST_CASE(histogram_over_empty_domain_is_rejected) {
    check_malformed("STATS|0|0|10");
}

//=============================== REDUCE =====================================

BOOST_AUTO_TEST_CASE(reduce_plain_round_trip) {
    Meta req = to_array_reduce_req(kDs, "grid/poro", "mean");
    ReduceQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_reduce_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.cid, "REDUCE|mean");
    BOOST_CHECK_EQUAL(q.op, "mean");
    BOOST_CHECK_EQUAL(q.path, "grid/poro");
    BOOST_CHECK(!q.has_k_range);
    BOOST_CHECK(!q.has_range);
    BOOST_CHECK(!q.has_dz);
}

BOOST_AUTO_TEST_CASE(reduce_k_zone_round_trip) {
    Meta req = to_array_reduce_req(kDs, "grid/poro", "max", true, 3, 9);
    ReduceQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_reduce_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK(q.has_k_range);
    BOOST_CHECK_EQUAL(q.k0, 3u);
    BOOST_CHECK_EQUAL(q.k1, 9u);
    BOOST_CHECK(!q.has_range);
}

BOOST_AUTO_TEST_CASE(reduce_range_without_k_zone_uses_placeholder) {
    Meta req = to_array_reduce_req(kDs, "grid/poro", "count", false, 0, 0, true, 0.1, 0.3);
    BOOST_CHECK_EQUAL(req.m_dataArrays.begin()->first.rfind("REDUCE|count|-|-|", 0), 0u);
    ReduceQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_reduce_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK(!q.has_k_range);
    BOOST_CHECK(q.has_range);
    BOOST_CHECK_CLOSE(q.range_lo, 0.1, 1e-9);
    BOOST_CHECK_CLOSE(q.range_hi, 0.3, 1e-9);
}

BOOST_AUTO_TEST_CASE(reduce_full_net_round_trip_exactly) {
    const double lo = 0.1 + 0.2, hi = 1e300, dz = 2.5;
    Meta req = to_array_reduce_req(kDs, "grid/poro", "net", true, 0, 4, true, lo, hi, true, dz);
    ReduceQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_reduce_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.op, "net");
    BOOST_CHECK(q.has_k_range);
    BOOST_CHECK_EQUAL(q.k0, 0u);
    BOOST_CHECK_EQUAL(q.k1, 4u);
    BOOST_CHECK(q.has_range);
    BOOST_CHECK_EQUAL(q.range_lo, lo);
    BOOST_CHECK_EQUAL(q.range_hi, hi);
    BOOST_CHECK(q.has_dz);
    BOOST_CHECK_EQUAL(q.cell_dz, dz);
}

BOOST_AUTO_TEST_CASE(reduce_prefix_without_bar_is_not_an_extension) {
    ReduceQuery q;
    std::string err;
    BOOST_CHECK(!is_array_reduce_req(meta_with_cid("REDUCE"), q, err));
    BOOST_CHECK(!is_array_reduce_req(meta_with_cid("REDUCE_sum"), q, err));
    BOOST_CHECK(!is_array_reduce_req(meta_with_cid("reduce|sum"), q, err));
    BOOST_CHECK(err.empty());
}

void check_malformed_reduce(const std::string& cid) {
    ReduceQuery q;
    std::string err;
    BOOST_TEST_CONTEXT("cid=" << cid) {
        BOOST_CHECK(is_array_reduce_req(meta_with_cid(cid), q, err));
        BOOST_CHECK(!err.empty());
        BOOST_CHECK_NE(err.find(cid), std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(reduce_malformed_markers_are_rejected) {
    check_malformed_reduce("REDUCE|");             // empty op
    check_malformed_reduce("REDUCE|sum|0");        // dangling k0
    check_malformed_reduce("REDUCE|sum|0|1|2");    // dangling lo
    check_malformed_reduce("REDUCE|sum|0|1|0|1|1|x"); // too many
    check_malformed_reduce("REDUCE|sum|-1|2");     // negative k
    check_malformed_reduce("REDUCE|sum|2|2");      // k1 <= k0
    check_malformed_reduce("REDUCE|sum|3|2");
    check_malformed_reduce("REDUCE|sum|a|b");
    check_malformed_reduce("REDUCE|sum|1.5|2");
    check_malformed_reduce("REDUCE|sum|-|2");      // half placeholder
    check_malformed_reduce("REDUCE|count|-|-|nan|1");
    check_malformed_reduce("REDUCE|count|-|-|0|inf");
    check_malformed_reduce("REDUCE|count|-|-|2|1");  // lo > hi
    check_malformed_reduce("REDUCE|net|-|-|0|1|0");  // dz == 0
    check_malformed_reduce("REDUCE|net|-|-|0|1|-1"); // dz < 0
    check_malformed_reduce("REDUCE|net|-|-|0|1|nan");
}

// Unknown operators are a semantic error the HANDLER reports (EINVALID_ARGUMENT)
// - the parser only checks shape, so the marker is detected without an error.
BOOST_AUTO_TEST_CASE(reduce_unknown_op_parses_as_well_formed) {
    ReduceQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_reduce_req(meta_with_cid("REDUCE|median"), q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.op, "median");
}

//=============================== WHERE ======================================

BOOST_AUTO_TEST_CASE(where_round_trip_exactly) {
    const double lo = 0.1 + 0.2, hi = 0.35;
    Meta req = to_array_where_req(kDs, "grid/poro", lo, hi);
    WhereQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_where_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.path, "grid/poro");
    BOOST_CHECK_EQUAL(q.range_lo, lo);
    BOOST_CHECK_EQUAL(q.range_hi, hi);
}

BOOST_AUTO_TEST_CASE(where_lo_equal_hi_is_valid) {
    WhereQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_where_req(meta_with_cid("WHERE|1|1"), q, err));
    BOOST_CHECK(err.empty());
}

BOOST_AUTO_TEST_CASE(where_prefix_is_not_an_extension) {
    WhereQuery q;
    std::string err;
    BOOST_CHECK(!is_array_where_req(meta_with_cid("WHERE"), q, err));
    BOOST_CHECK(!is_array_where_req(meta_with_cid("WHERE_poro"), q, err));
    BOOST_CHECK(!is_array_where_req(meta_with_cid("WHEREN|0|0|1"), q, err)); // WHEREN is its own op
    BOOST_CHECK(err.empty());
}

void check_malformed_where(const std::string& cid) {
    WhereQuery q;
    std::string err;
    BOOST_TEST_CONTEXT("cid=" << cid) {
        BOOST_CHECK(is_array_where_req(meta_with_cid(cid), q, err));
        BOOST_CHECK(!err.empty());
        BOOST_CHECK_NE(err.find(cid), std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(where_malformed_markers_are_rejected) {
    check_malformed_where("WHERE|");
    check_malformed_where("WHERE|1");
    check_malformed_where("WHERE|0|1|2");
    check_malformed_where("WHERE|2|1");   // hi < lo (review finding)
    check_malformed_where("WHERE|nan|1");
    check_malformed_where("WHERE|0|inf");
    check_malformed_where("WHERE|abc|1");
    check_malformed_where("WHERE||1");
}

//=============================== WHEREN =====================================

BOOST_AUTO_TEST_CASE(multiwhere_round_trip_preserves_order_and_bounds) {
    std::vector<MultiWhereEntry> in(3);
    in[0].path = "grid/poro"; in[0].range_lo = 0.1; in[0].range_hi = 0.4;
    in[1].path = "grid/sw";   in[1].range_lo = 0.0; in[1].range_hi = 0.5;
    in[2].path = "grid/ntg";  in[2].range_lo = 0.6; in[2].range_hi = 1.0;
    Meta req = to_array_multiwhere_req(kDs, in);
    BOOST_REQUIRE_EQUAL(req.m_dataArrays.size(), 3u);

    MultiWhereQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_multiwhere_req(req, q, err));
    BOOST_CHECK(err.empty());
    BOOST_REQUIRE_EQUAL(q.entries.size(), 3u);
    BOOST_CHECK_EQUAL(q.cid, q.entries[0].cid);
    BOOST_CHECK_EQUAL(q.cid.rfind("WHEREN|000|", 0), 0u);
    for (size_t i = 0; i < 3; ++i) {
        BOOST_CHECK_EQUAL(q.entries[i].path, in[i].path);
        BOOST_CHECK_EQUAL(q.entries[i].range_lo, in[i].range_lo);
        BOOST_CHECK_EQUAL(q.entries[i].range_hi, in[i].range_hi);
        BOOST_CHECK(!q.entries[i].uri.empty());
    }
}

BOOST_AUTO_TEST_CASE(multiwhere_single_entry_is_valid) {
    MultiWhereQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_multiwhere_req(meta_with_cid("WHEREN|0|0|1"), q, err));
    BOOST_CHECK(err.empty());
    BOOST_REQUIRE_EQUAL(q.entries.size(), 1u);
}

BOOST_AUTO_TEST_CASE(multiwhere_prefix_is_not_an_extension) {
    MultiWhereQuery q;
    std::string err;
    BOOST_CHECK(!is_array_multiwhere_req(meta_with_cid("WHEREN"), q, err));
    BOOST_CHECK(!is_array_multiwhere_req(meta_with_cid("WHERENX|0|0|1"), q, err));
    BOOST_CHECK(!is_array_multiwhere_req(meta_with_cid("WHERE|0|1"), q, err));
    BOOST_CHECK(err.empty());
}

void check_malformed_multiwhere(const Meta& req, const std::string& needle) {
    MultiWhereQuery q;
    std::string err;
    BOOST_TEST_CONTEXT("needle=" << needle) {
        BOOST_CHECK(is_array_multiwhere_req(req, q, err));
        BOOST_CHECK(!err.empty());
        BOOST_CHECK_NE(err.find(needle), std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(multiwhere_malformed_markers_are_rejected) {
    check_malformed_multiwhere(meta_with_cid("WHEREN|"), "WHEREN|");
    check_malformed_multiwhere(meta_with_cid("WHEREN|0|1"), "WHEREN|0|1");
    check_malformed_multiwhere(meta_with_cid("WHEREN|0|0|1|2"), "WHEREN|0|0|1|2");
    check_malformed_multiwhere(meta_with_cid("WHEREN|a|0|1"), "WHEREN|a|0|1");
    check_malformed_multiwhere(meta_with_cid("WHEREN|-1|0|1"), "WHEREN|-1|0|1");
    check_malformed_multiwhere(meta_with_cid("WHEREN|0|2|1"), "WHEREN|0|2|1"); // hi < lo
    check_malformed_multiwhere(meta_with_cid("WHEREN|0|nan|1"), "WHEREN|0|nan|1");
}

// A batch that mixes a WHEREN key with a plain key is malformed, NOT a plain
// metadata request (no silent fallthrough into the normal handler).
BOOST_AUTO_TEST_CASE(multiwhere_mixed_batch_is_rejected) {
    Meta req = meta_with_cid("WHEREN|0|0|1");
    common12::AryIdent ident;
    ident.m_uri = "eml:///dataspace('demo')";
    ident.m_pathInResource = "grid/perm";
    req.m_dataArrays.emplace("client-correlation-1", std::move(ident));
    check_malformed_multiwhere(req, "client-correlation-1");
}

BOOST_AUTO_TEST_CASE(multiwhere_entry_cap_is_enforced) {
    std::vector<MultiWhereEntry> in(kMaxMultiWhereEntries + 1);
    for (size_t i = 0; i < in.size(); ++i) {
        in[i].path = "grid/p" + std::to_string(i);
        in[i].range_hi = 1.0;
    }
    check_malformed_multiwhere(to_array_multiwhere_req(kDs, in), "at most");

    in.pop_back();
    MultiWhereQuery q;
    std::string err;
    BOOST_REQUIRE(is_array_multiwhere_req(to_array_multiwhere_req(kDs, in), q, err));
    BOOST_CHECK(err.empty());
    BOOST_CHECK_EQUAL(q.entries.size(), kMaxMultiWhereEntries);
}

//======================= ops do not shadow one another ======================

BOOST_AUTO_TEST_CASE(ops_are_mutually_exclusive) {
    StatsQuery s; ReduceQuery r; WhereQuery w; MultiWhereQuery m;
    std::string err;
    const Meta reduce = meta_with_cid("REDUCE|sum");
    const Meta where = meta_with_cid("WHERE|0|1");
    const Meta wheren = meta_with_cid("WHEREN|0|0|1");
    const Meta stats = meta_with_cid("STATS");
    BOOST_CHECK(!is_array_stats_req(reduce, s, err) && !is_array_where_req(reduce, w, err) &&
                !is_array_multiwhere_req(reduce, m, err));
    BOOST_CHECK(!is_array_stats_req(where, s, err) && !is_array_reduce_req(where, r, err) &&
                !is_array_multiwhere_req(where, m, err));
    BOOST_CHECK(!is_array_stats_req(wheren, s, err) && !is_array_reduce_req(wheren, r, err) &&
                !is_array_where_req(wheren, w, err));
    BOOST_CHECK(!is_array_reduce_req(stats, r, err) && !is_array_where_req(stats, w, err) &&
                !is_array_multiwhere_req(stats, m, err));
    BOOST_CHECK(err.empty());
}

BOOST_AUTO_TEST_SUITE_END()

//======================= compute capability guard ===========================
// The client guards every compute route on the server's advertised
// supportedArrayExtensions endpoint capability. An un-updated M27 store server
// omits the key, so the guard helper must report "unsupported" for it.

BOOST_AUTO_TEST_SUITE(ArrayComputeCapTests)

BOOST_AUTO_TEST_CASE(advertised_capability_is_detected_with_value) {
    std::map<std::string, common12::Datatypes::DataValue> caps;
    caps[oes::common::etp12::SUPPORTED_ARRAY_EXTENSIONS_CAP].m_item =
        std::string("stats,histogram");

    std::string exts;
    BOOST_CHECK(oes::common::etp12::server_advertises_array_extensions(caps, &exts));
    BOOST_CHECK_EQUAL(exts, "stats,histogram");
}

BOOST_AUTO_TEST_CASE(missing_capability_is_not_supported) {
    std::map<std::string, common12::Datatypes::DataValue> caps;
    caps["MaxWebSocketMessagePayloadSize"].m_item = int64_t(1000000);

    std::string exts = "unchanged";
    BOOST_CHECK(!oes::common::etp12::server_advertises_array_extensions(caps, &exts));
    BOOST_CHECK_EQUAL(exts, "unchanged"); // out is left untouched when absent
}

BOOST_AUTO_TEST_CASE(empty_capabilities_are_not_supported) {
    std::map<std::string, common12::Datatypes::DataValue> caps;
    BOOST_CHECK(!oes::common::etp12::server_advertises_array_extensions(caps));
}

BOOST_AUTO_TEST_CASE(present_but_non_string_value_is_still_supported) {
    // A malformed/odd advert (key present, value not a string) still counts as
    // "supported" -- the key's presence is the signal -- and leaves *out as-is.
    std::map<std::string, common12::Datatypes::DataValue> caps;
    caps[oes::common::etp12::SUPPORTED_ARRAY_EXTENSIONS_CAP].m_item = int64_t(1);

    std::string exts = "unchanged";
    BOOST_CHECK(oes::common::etp12::server_advertises_array_extensions(caps, &exts));
    BOOST_CHECK_EQUAL(exts, "unchanged");
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
