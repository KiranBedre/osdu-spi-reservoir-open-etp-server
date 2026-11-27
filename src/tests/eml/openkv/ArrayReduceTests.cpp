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

// Kernel-level coverage for the REDUCE / WHERE / WHEREN accumulators. All grids
// are deliberately NON-square (ni != nj != nk) so an i/j/k axis mix-up cannot
// pass by symmetry (review finding on the original square 4x4x4 fixtures).

#include <oes/eml/openkv/ArrayReduce.h>
#include <oes/common/HdfAry.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

using namespace oes::eml::openkv;
using oes::common::hdf::eAryOfDouble;
using oes::common::hdf::eAryOfFloat;
using oes::common::hdf::eAryOfInt;
using oes::common::hdf::eAryOfLong;

// Grid [nk=4, nj=3, ni=5] in C order: flat = (k*nj + j)*ni + i.
constexpr uint64_t NI = 5, NJ = 3, NK = 4;
constexpr uint64_t NCOLS = NI * NJ;
constexpr uint64_t NCELLS = NCOLS * NK;

inline uint64_t flat(uint64_t i, uint64_t j, uint64_t k) { return (k * NJ + j) * NI + i; }

// value(i,j,k) = 100*k + 10*j + i : every cell distinct, axis roles recoverable.
std::vector<double> rampGrid() {
    std::vector<double> v(NCELLS);
    for (uint64_t k = 0; k < NK; ++k)
        for (uint64_t j = 0; j < NJ; ++j)
            for (uint64_t i = 0; i < NI; ++i)
                v[flat(i, j, k)] = 100.0 * k + 10.0 * j + i;
    return v;
}

// Feed `v` in windows of `win` elements (exercises the incremental ijk cursor).
template <typename Acc>
void feedWindows(Acc& acc, const std::vector<double>& v, uint64_t win) {
    for (uint64_t off = 0; off < v.size(); off += win) {
        const uint64_t n = std::min<uint64_t>(win, v.size() - off);
        acc.accumulate(eAryOfDouble, v.data() + off, n, off);
    }
}

ColumnReduceOptions baseOpts(ReduceOp op) {
    ColumnReduceOptions o;
    o.ni = NI;
    o.nj = NJ;
    o.op = op;
    return o;
}

BOOST_AUTO_TEST_SUITE(ArrayReduceTests)

//============================ ColumnReduce ==================================

BOOST_AUTO_TEST_CASE(reduce_empty_grid_is_invalid) {
    ColumnReduceOptions o = baseOpts(ReduceOp::Sum);
    o.ni = 0;
    ColumnReduceAccumulator acc(o);
    ColumnReduceResult r = acc.finalize();
    BOOST_CHECK(!r.valid);
    BOOST_CHECK(r.map.empty());
}

BOOST_AUTO_TEST_CASE(reduce_sum_over_k_non_square_windows) {
    const std::vector<double> v = rampGrid();
    for (uint64_t win : {NCELLS, uint64_t(7), uint64_t(1)}) {
        ColumnReduceAccumulator acc(baseOpts(ReduceOp::Sum));
        feedWindows(acc, v, win);
        ColumnReduceResult r = acc.finalize();
        BOOST_TEST_CONTEXT("win=" << win) {
            BOOST_REQUIRE(r.valid);
            BOOST_CHECK_EQUAL(r.ni, NI);
            BOOST_CHECK_EQUAL(r.nj, NJ);
            BOOST_REQUIRE_EQUAL(r.map.size(), NCOLS);
            BOOST_CHECK_EQUAL(r.columns_with_data, NCOLS);
            for (uint64_t j = 0; j < NJ; ++j)
                for (uint64_t i = 0; i < NI; ++i) {
                    // sum_k (100k + 10j + i) = 100*(0+1+2+3) + NK*(10j + i)
                    const double expect = 600.0 + NK * (10.0 * j + i);
                    BOOST_CHECK_CLOSE(r.map[j * NI + i], expect, 1e-12);
                }
        }
    }
}

BOOST_AUTO_TEST_CASE(reduce_mean_min_max_axes_are_not_confused) {
    const std::vector<double> v = rampGrid();
    ColumnReduceAccumulator mean(baseOpts(ReduceOp::Mean));
    ColumnReduceAccumulator mn(baseOpts(ReduceOp::Min));
    ColumnReduceAccumulator mx(baseOpts(ReduceOp::Max));
    feedWindows(mean, v, 4);
    feedWindows(mn, v, 4);
    feedWindows(mx, v, 4);
    const auto rm = mean.finalize(), rmin = mn.finalize(), rmax = mx.finalize();
    // Column (i=4, j=2): values 24, 124, 224, 324.
    const uint64_t c = 2 * NI + 4;
    BOOST_CHECK_CLOSE(rm.map[c], 174.0, 1e-12);
    BOOST_CHECK_CLOSE(rmin.map[c], 24.0, 1e-12);
    BOOST_CHECK_CLOSE(rmax.map[c], 324.0, 1e-12);
    // Column (i=0, j=1) must differ from (i=1, j=0): 10 vs 1 at k=0.
    BOOST_CHECK_CLOSE(rmin.map[1 * NI + 0], 10.0, 1e-12);
    BOOST_CHECK_CLOSE(rmin.map[0 * NI + 1], 1.0, 1e-12);
}

BOOST_AUTO_TEST_CASE(reduce_k_zone_restricts_fold) {
    const std::vector<double> v = rampGrid();
    ColumnReduceOptions o = baseOpts(ReduceOp::Sum);
    o.has_k_range = true;
    o.k0 = 1;
    o.k1 = 3; // layers 1 and 2 only
    ColumnReduceAccumulator acc(o);
    feedWindows(acc, v, 6);
    ColumnReduceResult r = acc.finalize();
    // sum over k in {1,2}: 100 + 200 + 2*(10j + i)
    BOOST_CHECK_CLOSE(r.map[0], 300.0, 1e-12);
    BOOST_CHECK_CLOSE(r.map[2 * NI + 4], 300.0 + 2 * 24.0, 1e-12);
}

BOOST_AUTO_TEST_CASE(reduce_count_and_net_thickness) {
    const std::vector<double> v = rampGrid();
    ColumnReduceOptions o = baseOpts(ReduceOp::CountInRange);
    o.range_lo = 100.0;
    o.range_hi = 299.0; // k=1 and k=2 for every column (values 100..224)
    ColumnReduceAccumulator cnt(o);
    feedWindows(cnt, v, NCELLS);
    ColumnReduceResult rc = cnt.finalize();
    for (uint64_t c = 0; c < NCOLS; ++c) BOOST_CHECK_EQUAL(rc.map[c], 2.0);

    o.op = ReduceOp::NetThickness;
    o.cell_dz = 2.5;
    ColumnReduceAccumulator net(o);
    feedWindows(net, v, 5);
    ColumnReduceResult rn = net.finalize();
    for (uint64_t c = 0; c < NCOLS; ++c) BOOST_CHECK_CLOSE(rn.map[c], 5.0, 1e-12);
}

BOOST_AUTO_TEST_CASE(reduce_nan_and_null_sentinel_are_excluded) {
    std::vector<double> v = rampGrid();
    v[flat(0, 0, 0)] = std::numeric_limits<double>::quiet_NaN();
    v[flat(0, 0, 1)] = -999.25;
    ColumnReduceOptions o = baseOpts(ReduceOp::Mean);
    o.has_null_value = true;
    o.null_value = -999.25;
    ColumnReduceAccumulator acc(o);
    feedWindows(acc, v, 3);
    ColumnReduceResult r = acc.finalize();
    // column 0 keeps k=2,3 only: (200 + 300) / 2
    BOOST_CHECK_CLOSE(r.map[0], 250.0, 1e-12);
    BOOST_CHECK_EQUAL(r.columns_with_data, NCOLS);
}

BOOST_AUTO_TEST_CASE(reduce_empty_column_is_nan_for_mean_min_max_zero_for_sum) {
    std::vector<double> v = rampGrid();
    for (uint64_t k = 0; k < NK; ++k) v[flat(2, 1, k)] = std::numeric_limits<double>::quiet_NaN();
    const uint64_t c = 1 * NI + 2;
    for (ReduceOp op : {ReduceOp::Mean, ReduceOp::Min, ReduceOp::Max}) {
        ColumnReduceAccumulator acc(baseOpts(op));
        feedWindows(acc, v, NCELLS);
        ColumnReduceResult r = acc.finalize();
        BOOST_CHECK(std::isnan(r.map[c]));
        BOOST_CHECK_EQUAL(r.columns_with_data, NCOLS - 1);
    }
    ColumnReduceAccumulator sum(baseOpts(ReduceOp::Sum));
    feedWindows(sum, v, NCELLS);
    BOOST_CHECK_EQUAL(sum.finalize().map[c], 0.0);
}

BOOST_AUTO_TEST_CASE(reduce_integer_types_fold_with_sentinel) {
    std::vector<int32_t> v(NCELLS, 2);
    v[flat(1, 1, 0)] = std::numeric_limits<int32_t>::min();
    ColumnReduceOptions o = baseOpts(ReduceOp::Sum);
    o.has_null_value = true;
    o.null_value = static_cast<double>(std::numeric_limits<int32_t>::min());
    ColumnReduceAccumulator acc(o);
    acc.accumulate(eAryOfInt, v.data(), NCELLS, 0);
    ColumnReduceResult r = acc.finalize();
    BOOST_CHECK_EQUAL(r.map[1 * NI + 1], 2.0 * (NK - 1));
    BOOST_CHECK_EQUAL(r.map[0], 2.0 * NK);

    std::vector<int64_t> vl(NCELLS, 3);
    ColumnReduceAccumulator accl(baseOpts(ReduceOp::Max));
    accl.accumulate(eAryOfLong, vl.data(), NCELLS, 0);
    BOOST_CHECK_EQUAL(accl.finalize().map[NCOLS - 1], 3.0);

    std::vector<float> vf(NCELLS, 1.5f);
    ColumnReduceAccumulator accf(baseOpts(ReduceOp::Sum));
    accf.accumulate(eAryOfFloat, vf.data(), NCELLS, 0);
    BOOST_CHECK_CLOSE(accf.finalize().map[0], 6.0, 1e-9);
}

BOOST_AUTO_TEST_CASE(reduce_merge_equals_single_pass) {
    const std::vector<double> v = rampGrid();
    for (ReduceOp op : {ReduceOp::Sum, ReduceOp::Mean, ReduceOp::Min, ReduceOp::Max,
                        ReduceOp::CountInRange, ReduceOp::NetThickness}) {
        ColumnReduceOptions o = baseOpts(op);
        o.range_lo = 50.0;
        o.range_hi = 250.0;
        o.cell_dz = 0.5;
        ColumnReduceAccumulator whole(o), a(o), b(o);
        whole.accumulate(eAryOfDouble, v.data(), NCELLS, 0);
        const uint64_t split = 23; // not window- or column-aligned
        a.accumulate(eAryOfDouble, v.data(), split, 0);
        b.accumulate(eAryOfDouble, v.data() + split, NCELLS - split, split);
        a.merge(b);
        const auto rw = whole.finalize(), rm = a.finalize();
        BOOST_TEST_CONTEXT("op=" << static_cast<int>(op)) {
            BOOST_CHECK_EQUAL(rw.columns_with_data, rm.columns_with_data);
            for (uint64_t c = 0; c < NCOLS; ++c) BOOST_CHECK_EQUAL(rw.map[c], rm.map[c]);
        }
    }
}

BOOST_AUTO_TEST_CASE(reduce_non_numeric_type_is_ignored) {
    const std::vector<double> v = rampGrid();
    ColumnReduceAccumulator acc(baseOpts(ReduceOp::Sum));
    acc.accumulate(oes::common::hdf::eAryOfString, v.data(), NCELLS, 0);
    BOOST_CHECK_EQUAL(acc.finalize().columns_with_data, 0u);
}

BOOST_AUTO_TEST_CASE(parse_reduce_op_tokens) {
    ReduceOp op;
    BOOST_CHECK(parseReduceOp("sum", op) && op == ReduceOp::Sum);
    BOOST_CHECK(parseReduceOp("mean", op) && op == ReduceOp::Mean);
    BOOST_CHECK(parseReduceOp("min", op) && op == ReduceOp::Min);
    BOOST_CHECK(parseReduceOp("max", op) && op == ReduceOp::Max);
    BOOST_CHECK(parseReduceOp("count", op) && op == ReduceOp::CountInRange);
    BOOST_CHECK(parseReduceOp("net", op) && op == ReduceOp::NetThickness);
    BOOST_CHECK(!parseReduceOp("SUM", op));
    BOOST_CHECK(!parseReduceOp("median", op));
    BOOST_CHECK(!parseReduceOp("", op));
}

//=============================== Where ======================================

WhereOptions whereOpts(double lo, double hi) {
    WhereOptions o;
    o.ni = NI;
    o.nj = NJ;
    o.range_lo = lo;
    o.range_hi = hi;
    return o;
}

BOOST_AUTO_TEST_CASE(where_no_hit_is_invalid) {
    const std::vector<double> v = rampGrid();
    WhereAccumulator acc(whereOpts(1000.0, 2000.0));
    feedWindows(acc, v, 7);
    WhereResult r = acc.finalize();
    BOOST_CHECK(!r.valid);
    BOOST_CHECK_EQUAL(r.count, 0u);
}

BOOST_AUTO_TEST_CASE(where_bbox_and_centroid_non_square) {
    const std::vector<double> v = rampGrid();
    // Select k=2 layer, j in {1,2}, all i: values 210..214 and 220..224.
    WhereAccumulator acc(whereOpts(210.0, 224.0));
    feedWindows(acc, v, 5);
    WhereResult r = acc.finalize();
    BOOST_REQUIRE(r.valid);
    BOOST_CHECK_EQUAL(r.count, 10u);
    BOOST_CHECK_EQUAL(r.imin, 0u); BOOST_CHECK_EQUAL(r.imax, 4u);
    BOOST_CHECK_EQUAL(r.jmin, 1u); BOOST_CHECK_EQUAL(r.jmax, 2u);
    BOOST_CHECK_EQUAL(r.kmin, 2u); BOOST_CHECK_EQUAL(r.kmax, 2u);
    BOOST_CHECK_CLOSE(r.ci, 2.0, 1e-12);
    BOOST_CHECK_CLOSE(r.cj, 1.5, 1e-12);
    BOOST_CHECK_CLOSE(r.ck, 2.0, 1e-12);
}

BOOST_AUTO_TEST_CASE(where_inclusive_bounds_and_null_exclusion) {
    std::vector<double> v = rampGrid();
    v[flat(0, 0, 0)] = std::numeric_limits<double>::quiet_NaN();
    WhereOptions o = whereOpts(0.0, 1.0); // would be cells 0 and 1 at k=0
    o.has_null_value = true;
    o.null_value = 1.0; // 1.0 is the sentinel -> excluded even though in range
    WhereAccumulator acc(o);
    feedWindows(acc, v, NCELLS);
    WhereResult r = acc.finalize();
    BOOST_CHECK(!r.valid);
    BOOST_CHECK_EQUAL(r.count, 0u);

    WhereAccumulator acc2(whereOpts(0.0, 1.0));
    feedWindows(acc2, rampGrid(), NCELLS);
    BOOST_CHECK_EQUAL(acc2.finalize().count, 2u);
}

BOOST_AUTO_TEST_CASE(where_integer_sentinel) {
    std::vector<int64_t> v(NCELLS, 5);
    v[flat(4, 2, 3)] = std::numeric_limits<int64_t>::min();
    WhereOptions o = whereOpts(-1e300, 1e300);
    o.has_null_value = true;
    o.null_value = static_cast<double>(std::numeric_limits<int64_t>::min());
    WhereAccumulator acc(o);
    acc.accumulate(eAryOfLong, v.data(), NCELLS, 0);
    BOOST_CHECK_EQUAL(acc.finalize().count, NCELLS - 1);
}

BOOST_AUTO_TEST_CASE(where_merge_equals_single_pass) {
    const std::vector<double> v = rampGrid();
    WhereAccumulator whole(whereOpts(15.0, 310.0)), a(whereOpts(15.0, 310.0)), b(whereOpts(15.0, 310.0));
    whole.accumulate(eAryOfDouble, v.data(), NCELLS, 0);
    a.accumulate(eAryOfDouble, v.data(), 17, 0);
    b.accumulate(eAryOfDouble, v.data() + 17, NCELLS - 17, 17);
    a.merge(b);
    const auto rw = whole.finalize(), rm = a.finalize();
    BOOST_CHECK_EQUAL(rw.count, rm.count);
    BOOST_CHECK_EQUAL(rw.imin, rm.imin); BOOST_CHECK_EQUAL(rw.imax, rm.imax);
    BOOST_CHECK_EQUAL(rw.jmin, rm.jmin); BOOST_CHECK_EQUAL(rw.jmax, rm.jmax);
    BOOST_CHECK_EQUAL(rw.kmin, rm.kmin); BOOST_CHECK_EQUAL(rw.kmax, rm.kmax);
    BOOST_CHECK_CLOSE(rw.ci, rm.ci, 1e-12);
    BOOST_CHECK_CLOSE(rw.cj, rm.cj, 1e-12);
    BOOST_CHECK_CLOSE(rw.ck, rm.ck, 1e-12);

    // merging an empty side is a no-op
    WhereAccumulator none(whereOpts(1e6, 2e6));
    a.merge(none);
    BOOST_CHECK_EQUAL(a.finalize().count, rw.count);
}

//============================= MultiWhere ===================================

WherePredicate pred(double lo, double hi) {
    WherePredicate p;
    p.range_lo = lo;
    p.range_hi = hi;
    return p;
}

BOOST_AUTO_TEST_CASE(multiwhere_no_arrays_is_invalid) {
    MultiWhereAccumulator acc(NI, NJ, NK);
    BOOST_CHECK_EQUAL(acc.arraysCommitted(), 0);
    BOOST_CHECK(!acc.finalize().valid);
}

BOOST_AUTO_TEST_CASE(multiwhere_single_array_matches_where) {
    const std::vector<double> v = rampGrid();
    MultiWhereAccumulator acc(NI, NJ, NK);
    acc.beginArray(pred(210.0, 224.0));
    feedWindows(acc, v, 7);
    acc.commitArray();
    WhereResult r = acc.finalize();
    BOOST_REQUIRE(r.valid);
    BOOST_CHECK_EQUAL(r.count, 10u);
    BOOST_CHECK_EQUAL(r.imin, 0u); BOOST_CHECK_EQUAL(r.imax, 4u);
    BOOST_CHECK_EQUAL(r.jmin, 1u); BOOST_CHECK_EQUAL(r.jmax, 2u);
    BOOST_CHECK_EQUAL(r.kmin, 2u); BOOST_CHECK_EQUAL(r.kmax, 2u);
    BOOST_CHECK_CLOSE(r.ci, 2.0, 1e-12);
    // the 2x5 slab is one connected body
    BOOST_CHECK_EQUAL(r.components, 1u);
    BOOST_CHECK_EQUAL(r.body_count, 10u);
    BOOST_CHECK_EQUAL(r.body_imin, 0u);
    BOOST_CHECK_EQUAL(r.body_jmin, 1u);
    BOOST_CHECK_CLOSE(r.body_cj, 1.5, 1e-12);
}

BOOST_AUTO_TEST_CASE(multiwhere_and_of_two_predicates) {
    const std::vector<double> v = rampGrid();
    // second array: cell index itself
    std::vector<double> idx(NCELLS);
    for (uint64_t x = 0; x < NCELLS; ++x) idx[x] = static_cast<double>(x);

    MultiWhereAccumulator acc(NI, NJ, NK);
    acc.beginArray(pred(100.0, 199.0)); // k == 1 (15 cells)
    feedWindows(acc, v, 4);
    acc.commitArray();
    BOOST_CHECK(!acc.empty());
    acc.beginArray(pred(0.0, 19.0)); // flat < 20 -> k=1 only for flat 15..19 (j=0)
    feedWindows(acc, idx, 4);
    acc.commitArray();
    BOOST_CHECK_EQUAL(acc.arraysCommitted(), 2);
    WhereResult r = acc.finalize();
    BOOST_REQUIRE(r.valid);
    BOOST_CHECK_EQUAL(r.count, 5u);
    BOOST_CHECK_EQUAL(r.kmin, 1u); BOOST_CHECK_EQUAL(r.kmax, 1u);
    BOOST_CHECK_EQUAL(r.jmin, 0u); BOOST_CHECK_EQUAL(r.jmax, 0u);
    BOOST_CHECK_EQUAL(r.imin, 0u); BOOST_CHECK_EQUAL(r.imax, 4u);
    BOOST_CHECK_CLOSE(r.ci, 2.0, 1e-12);
}

BOOST_AUTO_TEST_CASE(multiwhere_empty_intersection_short_circuits) {
    const std::vector<double> v = rampGrid();
    MultiWhereAccumulator acc(NI, NJ, NK);
    acc.beginArray(pred(0.0, 99.0)); // k == 0
    feedWindows(acc, v, NCELLS);
    acc.commitArray();
    acc.beginArray(pred(300.0, 399.0)); // k == 3 -> disjoint
    feedWindows(acc, v, NCELLS);
    acc.commitArray();
    BOOST_CHECK(acc.empty());
    WhereResult r = acc.finalize();
    BOOST_CHECK(!r.valid);
    BOOST_CHECK_EQUAL(r.count, 0u);
    BOOST_CHECK_EQUAL(r.components, 0u);
}

BOOST_AUTO_TEST_CASE(multiwhere_connected_bodies_six_connectivity) {
    // Two bodies: a 2x1x3 bar (6 cells) along i at (j=0,k=0..2)... and an
    // isolated single cell at the far corner. Diagonal neighbours must NOT join.
    std::vector<double> v(NCELLS, 0.0);
    for (uint64_t k = 0; k < 3; ++k)
        for (uint64_t i = 0; i < 2; ++i) v[flat(i, 0, k)] = 1.0;
    v[flat(4, 2, 3)] = 1.0;        // isolated
    v[flat(2, 1, 0)] = 1.0;        // diagonal to (1,0,0): separate body

    MultiWhereAccumulator acc(NI, NJ, NK);
    acc.beginArray(pred(1.0, 1.0));
    feedWindows(acc, v, 9);
    acc.commitArray();
    WhereResult r = acc.finalize();
    BOOST_REQUIRE(r.valid);
    BOOST_CHECK_EQUAL(r.count, 8u);
    BOOST_CHECK_EQUAL(r.components, 3u);
    BOOST_CHECK_EQUAL(r.body_count, 6u);
    BOOST_CHECK_EQUAL(r.body_imin, 0u); BOOST_CHECK_EQUAL(r.body_imax, 1u);
    BOOST_CHECK_EQUAL(r.body_jmin, 0u); BOOST_CHECK_EQUAL(r.body_jmax, 0u);
    BOOST_CHECK_EQUAL(r.body_kmin, 0u); BOOST_CHECK_EQUAL(r.body_kmax, 2u);
    BOOST_CHECK_CLOSE(r.body_ci, 0.5, 1e-12);
    BOOST_CHECK_CLOSE(r.body_ck, 1.0, 1e-12);
    // whole-set bbox spans everything
    BOOST_CHECK_EQUAL(r.imax, 4u); BOOST_CHECK_EQUAL(r.jmax, 2u); BOOST_CHECK_EQUAL(r.kmax, 3u);
}

BOOST_AUTO_TEST_CASE(multiwhere_integer_sentinel_and_nan_are_excluded) {
    std::vector<int32_t> v(NCELLS, 1);
    v[flat(0, 0, 0)] = std::numeric_limits<int32_t>::min();
    WherePredicate p = pred(-1e300, 1e300);
    p.has_null_value = true;
    p.null_value = static_cast<double>(std::numeric_limits<int32_t>::min());
    MultiWhereAccumulator acc(NI, NJ, NK);
    acc.beginArray(p);
    acc.accumulate(eAryOfInt, v.data(), NCELLS, 0);
    acc.commitArray();
    BOOST_CHECK_EQUAL(acc.finalize().count, NCELLS - 1);
}

BOOST_AUTO_TEST_CASE(multiwhere_oversized_window_is_clamped_to_grid) {
    std::vector<double> v(NCELLS + 10, 1.0); // caller claims more cells than the grid has
    MultiWhereAccumulator acc(NI, NJ, NK);
    acc.beginArray(pred(1.0, 1.0));
    acc.accumulate(eAryOfDouble, v.data(), NCELLS + 10, 0);
    acc.accumulate(eAryOfDouble, v.data(), 10, NCELLS + 5); // entirely out of range
    acc.commitArray();
    BOOST_CHECK_EQUAL(acc.finalize().count, NCELLS);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
