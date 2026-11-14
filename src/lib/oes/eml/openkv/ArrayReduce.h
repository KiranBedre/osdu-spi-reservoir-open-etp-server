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

#ifndef OES_EML_OPENKV_ARRAYREDUCE_H
#define OES_EML_OPENKV_ARRAYREDUCE_H

#include "../export.h"

#include <oes/common/HdfAry.h>

#include <cstdint>
#include <string>
#include <vector>

namespace oes::eml::openkv {

//============================================================================
// ArrayReduce — streaming, mergeable *spatial* reductions over a grid
// property array, the structural generalisation of ArrayStats (which folds
// the whole array into one scalar summary). Three reducers live here:
//
//   * ColumnReduceAccumulator — collapse a 3-D grid property stored in C
//     order `[Nk, Nj, Ni]` *over the K axis* into a 2-D `[Nj, Ni]` column map
//     (sum / mean / min / max / count-in-range / net-thickness), optionally
//     restricted to a `[k0, k1)` zone.
//   * WhereAccumulator — a masked predicate over a single property: count the
//     cells whose value falls in `[lo, hi]` and return their ijk bounding box
//     and ijk centroid, in O(1) state.
//   * MultiWhereAccumulator — the AND of N such predicates over arrays of the
//     same grid, streamed one array at a time against a one-bit-per-cell mask,
//     plus the largest 6-connected body of the surviving cells.
//
// Shared contract (as ArrayStats):
//   * Bounded memory: O(Nj*Ni) for the column map, O(1) for WHERE, O(cells/64)
//     words for WHEREN — never the O(Nk*Nj*Ni) value list.
//   * Streaming: accumulate() is called once per element-aligned read window
//     with the flat C-order index of the window's first element; the (k, j, i)
//     position is tracked incrementally across the window (one div/mod per
//     window, not per element) and the operator is selected once per window.
//   * Mergeable: merge() combines partial accumulators over disjoint index
//     ranges (column-wise / tally-wise).
//   * Null-aware: an explicit null sentinel and IEEE NaN are excluded.
//============================================================================

//! Column-reduction operator. CountInRange / NetThickness consult the range
//! predicate; the rest fold every finite value in the (optional) K-zone.
enum class ReduceOp {
    Sum,
    Mean,
    Min,
    Max,
    CountInRange,
    NetThickness
};

//! Parse a ReduceOp token ("sum"/"mean"/"min"/"max"/"count"/"net"); false for
//! an unrecognised token (caller decides the error).
OES_EML_EXPORT bool parseReduceOp(const std::string& token, ReduceOp& out);

//! Immutable result of a K-axis column reduction.
struct OES_EML_EXPORT ColumnReduceResult {
    bool valid = false;             //!< false when ncols == 0.
    uint64_t ni = 0;                //!< fastest grid axis.
    uint64_t nj = 0;                //!< middle grid axis (map is [nj, ni]).
    std::vector<double> map;        //!< row-major [nj, ni]; empty columns -> NaN
                                    //!< (Min/Max/Mean) or 0 (Sum/Count/Net).
    uint64_t columns_with_data = 0; //!< columns that folded at least one value.
};

//! Caller-supplied knobs for the K-axis column reduction.
struct OES_EML_EXPORT ColumnReduceOptions {
    uint64_t ni = 0; //!< fastest axis; ncols = ni * nj.
    uint64_t nj = 0; //!< middle axis.
    ReduceOp op = ReduceOp::Sum;

    bool has_null_value = false; //!< when true, values == null_value are null.
    double null_value = 0.0;
    bool skip_nan = true;

    //! Optional K-zone restriction [k0, k1): fold only cells with k0 <= k < k1.
    bool has_k_range = false;
    uint64_t k0 = 0;
    uint64_t k1 = 0;

    //! Inclusive range predicate for CountInRange / NetThickness.
    double range_lo = 0.0;
    double range_hi = 0.0;

    //! Per-cell thickness used by NetThickness (map = in-range-count * cell_dz).
    double cell_dz = 1.0;
};

//! Streaming, mergeable K-axis column reducer. Not thread-safe on its own:
//! give each worker its own instance over a disjoint flat-index range, then
//! merge() the results.
class OES_EML_EXPORT ColumnReduceAccumulator {
public:
    explicit ColumnReduceAccumulator(const ColumnReduceOptions& opts);

    //! Fold one contiguous window of `n_elems` values whose first element is
    //! at flat C-order index `global_start` in `[Nk, Nj, Ni]`. `type` selects
    //! the element interpretation; non-numeric types are ignored.
    void accumulate(
        oes::common::hdf::HdfAryType type, const void* data, uint64_t n_elems, uint64_t global_start
    );

    //! Combine a partial accumulator produced over a disjoint index range.
    void merge(const ColumnReduceAccumulator& other);

    //! Snapshot the current column map as an immutable result.
    ColumnReduceResult finalize() const;

private:
    template <typename T, ReduceOp Op>
    void foldWindow(const T* p, uint64_t n, uint64_t global_start);
    template <typename T>
    void foldWindowDispatch(const T* p, uint64_t n, uint64_t global_start);

    ColumnReduceOptions opts_;
    uint64_t ncols_ = 0;
    std::vector<uint64_t> count_; //!< per-column folded/in-range count.
    std::vector<double> sum_;     //!< per-column sum (Sum/Mean).
    std::vector<double> min_;     //!< per-column min (Min).
    std::vector<double> max_;     //!< per-column max (Max).
};

//============================================================================

//! Immutable result of a masked where() query.
//!
//! Carries two distinct answers:
//!  * the *whole passing set* (count / bbox / centroid) — every cell that
//!    satisfied the predicate(s), regardless of whether the cells touch; and
//!  * (WHEREN only) the *largest connected body* (body_*) — the biggest
//!    6-connected cluster of passing cells, plus the number of such bodies.
struct OES_EML_EXPORT WhereResult {
    bool valid = false; //!< true when at least one cell passed the predicate.
    uint64_t count = 0; //!< cells with value in [lo, hi] (whole passing set).
    // ijk bounding box of the whole passing set (inclusive); valid-only.
    uint64_t imin = 0, imax = 0;
    uint64_t jmin = 0, jmax = 0;
    uint64_t kmin = 0, kmax = 0;
    // ijk centroid of the whole passing set (mean index); valid-only.
    double ci = 0.0, cj = 0.0, ck = 0.0;

    // --- largest connected body (6-connectivity over the AND-mask) ---
    uint64_t components = 0; //!< number of connected bodies in the passing set.
    uint64_t body_count = 0; //!< cell count (volume) of the LARGEST body.
    uint64_t body_imin = 0, body_imax = 0;
    uint64_t body_jmin = 0, body_jmax = 0;
    uint64_t body_kmin = 0, body_kmax = 0;
    double body_ci = 0.0, body_cj = 0.0, body_ck = 0.0;
};

//! One inclusive `[lo, hi]` predicate (shared by WHERE and WHEREN).
struct OES_EML_EXPORT WherePredicate {
    double range_lo = 0.0;
    double range_hi = 0.0;

    bool has_null_value = false;
    double null_value = 0.0;
    bool skip_nan = true;
};

//! Caller-supplied knobs for the single-array masked where() query.
struct OES_EML_EXPORT WhereOptions : WherePredicate {
    uint64_t ni = 0; //!< fastest axis.
    uint64_t nj = 0; //!< middle axis.
};

//! Streaming, mergeable masked-predicate reducer returning count + ijk bbox
//! + ijk centroid. O(1) state regardless of grid size.
class OES_EML_EXPORT WhereAccumulator {
public:
    explicit WhereAccumulator(const WhereOptions& opts);

    //! Fold one contiguous window starting at flat index `global_start`.
    void accumulate(
        oes::common::hdf::HdfAryType type, const void* data, uint64_t n_elems, uint64_t global_start
    );

    //! Combine a partial accumulator over a disjoint index range.
    void merge(const WhereAccumulator& other);

    //! Snapshot the current mask summary as an immutable result.
    WhereResult finalize() const;

private:
    template <typename T>
    void foldWindow(const T* p, uint64_t n, uint64_t global_start);
    void hit(uint64_t i, uint64_t j, uint64_t k);

    WhereOptions opts_;
    uint64_t ncols_ = 0;
    uint64_t count_ = 0;
    uint64_t imin_ = 0, imax_ = 0, jmin_ = 0, jmax_ = 0, kmin_ = 0, kmax_ = 0;
    long double si_ = 0.0L, sj_ = 0.0L, sk_ = 0.0L; //!< index sums for the centroid.
};

//============================================================================

//! Multi-array masked predicate: AND of N per-array `[lo, hi]` predicates over
//! the *same* grid, returning the passing-cell count + ijk bbox + ijk centroid
//! and the largest 6-connected body.
//!
//! Keeps one bit per cell so the N property arrays can be streamed
//! **sequentially** (one full pass each) and intersected without holding any
//! array's values: O(Nk*Nj*Ni / 64) words, never the value list.
//!
//! Usage: for each property array, beginArray(pred), accumulate(...) over its
//! read windows, commitArray(); finalize() scans the surviving mask once. When
//! empty() turns true after a commit the remaining arrays need not be read.
class OES_EML_EXPORT MultiWhereAccumulator {
public:
    MultiWhereAccumulator(uint64_t ni, uint64_t nj, uint64_t nk);

    //! Start a fresh pass for the next property array with its predicate.
    void beginArray(const WherePredicate& pred);

    //! Fold one contiguous window starting at flat index `global_start`.
    void accumulate(
        oes::common::hdf::HdfAryType type, const void* data, uint64_t n_elems, uint64_t global_start
    );

    //! AND the just-streamed array's in-range mask into the running result.
    void commitArray();

    //! Number of arrays committed so far (a valid query needs >= 1).
    int arraysCommitted() const { return arrays_committed_; }

    //! True once no cell survives the committed predicates (short-circuit).
    bool empty() const;

    //! Scan the surviving mask into the whole-set summary and the largest
    //! connected body.
    WhereResult finalize() const;

private:
    template <typename T>
    void foldWindow(const T* p, uint64_t n, uint64_t global_start);

    uint64_t ni_ = 0, nj_ = 0, nk_ = 0, ncells_ = 0;
    int arrays_committed_ = 0;
    WherePredicate pred_;
    std::vector<uint64_t> mask_;    //!< AND of committed arrays (bit per cell).
    std::vector<uint64_t> scratch_; //!< current array's in-range bits.
};

} // namespace oes::eml::openkv

#endif // OES_EML_OPENKV_ARRAYREDUCE_H
