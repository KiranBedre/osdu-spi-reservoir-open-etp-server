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

#include "ArrayReduce.h"
#include "NumericType.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace oes::eml::openkv {

using oes::common::hdf::HdfAryType;

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

//! Incremental (k, j, i) cursor over a flat C-order index into [Nk, Nj, Ni].
//! One div/mod pair at construction, then O(1) carries per step.
struct IjkCursor {
    uint64_t i, j, k;
    const uint64_t ni, nj;

    IjkCursor(uint64_t flat, uint64_t ni_, uint64_t nj_) : ni(ni_), nj(nj_) {
        const uint64_t ncols = ni * nj;
        k = flat / ncols;
        const uint64_t rem = flat - k * ncols;
        j = rem / ni;
        i = rem - j * ni;
    }

    inline void step() {
        if (++i == ni) {
            i = 0;
            if (++j == nj) {
                j = 0;
                ++k;
            }
        }
    }
};

//! Null predicate shared by the reducers: NaN (when skip_nan) or == sentinel.
inline bool isNull(double v, bool skip_nan, bool has_null, double null_value) {
    if (skip_nan && std::isnan(v)) return true;
    return has_null && v == null_value;
}

} // namespace

//============================================================================

bool parseReduceOp(const std::string& token, ReduceOp& out) {
    if (token == "sum") { out = ReduceOp::Sum; return true; }
    if (token == "mean") { out = ReduceOp::Mean; return true; }
    if (token == "min") { out = ReduceOp::Min; return true; }
    if (token == "max") { out = ReduceOp::Max; return true; }
    if (token == "count") { out = ReduceOp::CountInRange; return true; }
    if (token == "net") { out = ReduceOp::NetThickness; return true; }
    return false;
}

//============================================================================
// ColumnReduceAccumulator
//============================================================================

ColumnReduceAccumulator::ColumnReduceAccumulator(const ColumnReduceOptions& opts)
    : opts_(opts), ncols_(opts.ni * opts.nj) {
    switch (opts_.op) {
    case ReduceOp::Sum:
    case ReduceOp::Mean:
        sum_.assign(ncols_, 0.0);
        count_.assign(ncols_, 0);
        break;
    case ReduceOp::Min:
        min_.assign(ncols_, std::numeric_limits<double>::infinity());
        count_.assign(ncols_, 0);
        break;
    case ReduceOp::Max:
        max_.assign(ncols_, -std::numeric_limits<double>::infinity());
        count_.assign(ncols_, 0);
        break;
    case ReduceOp::CountInRange:
    case ReduceOp::NetThickness:
        count_.assign(ncols_, 0);
        break;
    }
}

template <typename T, ReduceOp Op>
void ColumnReduceAccumulator::foldWindow(const T* p, uint64_t n, uint64_t global_start) {
    const bool skip_nan = opts_.skip_nan;
    const bool has_null = opts_.has_null_value;
    const double null_value = opts_.null_value;
    const bool has_k = opts_.has_k_range;
    const uint64_t k0 = opts_.k0, k1 = opts_.k1;
    const double lo = opts_.range_lo, hi = opts_.range_hi;

    IjkCursor cur(global_start, opts_.ni, opts_.nj);
    for (uint64_t x = 0; x < n; ++x, cur.step()) {
        if (has_k && (cur.k < k0 || cur.k >= k1)) continue;
        const double v = static_cast<double>(p[x]);
        if (isNull(v, skip_nan, has_null, null_value)) continue;
        const uint64_t col = cur.j * opts_.ni + cur.i;
        if constexpr (Op == ReduceOp::Sum || Op == ReduceOp::Mean) {
            sum_[col] += v;
            ++count_[col];
        } else if constexpr (Op == ReduceOp::Min) {
            if (v < min_[col]) min_[col] = v;
            ++count_[col];
        } else if constexpr (Op == ReduceOp::Max) {
            if (v > max_[col]) max_[col] = v;
            ++count_[col];
        } else { // CountInRange / NetThickness
            if (v >= lo && v <= hi) ++count_[col];
        }
    }
}

template <typename T>
void ColumnReduceAccumulator::foldWindowDispatch(const T* p, uint64_t n, uint64_t global_start) {
    // Operator selected once per window, not per element.
    switch (opts_.op) {
    case ReduceOp::Sum: foldWindow<T, ReduceOp::Sum>(p, n, global_start); break;
    case ReduceOp::Mean: foldWindow<T, ReduceOp::Mean>(p, n, global_start); break;
    case ReduceOp::Min: foldWindow<T, ReduceOp::Min>(p, n, global_start); break;
    case ReduceOp::Max: foldWindow<T, ReduceOp::Max>(p, n, global_start); break;
    case ReduceOp::CountInRange: foldWindow<T, ReduceOp::CountInRange>(p, n, global_start); break;
    case ReduceOp::NetThickness: foldWindow<T, ReduceOp::NetThickness>(p, n, global_start); break;
    }
}

void ColumnReduceAccumulator::accumulate(
    HdfAryType type, const void* data, uint64_t n_elems, uint64_t global_start
) {
    if (data == nullptr || n_elems == 0 || ncols_ == 0) return;
    visitNumericType(type, [&](auto tag) {
        using T = decltype(tag);
        foldWindowDispatch<T>(static_cast<const T*>(data), n_elems, global_start);
    });
}

void ColumnReduceAccumulator::merge(const ColumnReduceAccumulator& other) {
    if (other.ncols_ != ncols_ || other.opts_.op != opts_.op) return;
    for (uint64_t c = 0; c < ncols_; ++c) count_[c] += other.count_[c];
    switch (opts_.op) {
    case ReduceOp::Sum:
    case ReduceOp::Mean:
        for (uint64_t c = 0; c < ncols_; ++c) sum_[c] += other.sum_[c];
        break;
    case ReduceOp::Min:
        for (uint64_t c = 0; c < ncols_; ++c) min_[c] = std::min(min_[c], other.min_[c]);
        break;
    case ReduceOp::Max:
        for (uint64_t c = 0; c < ncols_; ++c) max_[c] = std::max(max_[c], other.max_[c]);
        break;
    case ReduceOp::CountInRange:
    case ReduceOp::NetThickness:
        break;
    }
}

ColumnReduceResult ColumnReduceAccumulator::finalize() const {
    ColumnReduceResult r;
    r.ni = opts_.ni;
    r.nj = opts_.nj;
    if (ncols_ == 0) return r;
    r.valid = true;
    r.map.resize(ncols_);
    for (uint64_t c = 0; c < ncols_; ++c) {
        const uint64_t cnt = count_[c];
        if (cnt > 0) ++r.columns_with_data;
        switch (opts_.op) {
        case ReduceOp::Sum: r.map[c] = sum_[c]; break;
        case ReduceOp::Mean: r.map[c] = cnt ? sum_[c] / static_cast<double>(cnt) : kNaN; break;
        case ReduceOp::Min: r.map[c] = cnt ? min_[c] : kNaN; break;
        case ReduceOp::Max: r.map[c] = cnt ? max_[c] : kNaN; break;
        case ReduceOp::CountInRange: r.map[c] = static_cast<double>(cnt); break;
        case ReduceOp::NetThickness: r.map[c] = static_cast<double>(cnt) * opts_.cell_dz; break;
        }
    }
    return r;
}

//============================================================================
// WhereAccumulator
//============================================================================

WhereAccumulator::WhereAccumulator(const WhereOptions& opts)
    : opts_(opts), ncols_(opts.ni * opts.nj) {}

inline void WhereAccumulator::hit(uint64_t i, uint64_t j, uint64_t k) {
    if (count_ == 0) {
        imin_ = imax_ = i;
        jmin_ = jmax_ = j;
        kmin_ = kmax_ = k;
    } else {
        if (i < imin_) imin_ = i;
        if (i > imax_) imax_ = i;
        if (j < jmin_) jmin_ = j;
        if (j > jmax_) jmax_ = j;
        if (k < kmin_) kmin_ = k;
        if (k > kmax_) kmax_ = k;
    }
    ++count_;
    si_ += static_cast<long double>(i);
    sj_ += static_cast<long double>(j);
    sk_ += static_cast<long double>(k);
}

template <typename T>
void WhereAccumulator::foldWindow(const T* p, uint64_t n, uint64_t global_start) {
    const bool skip_nan = opts_.skip_nan;
    const bool has_null = opts_.has_null_value;
    const double null_value = opts_.null_value;
    const double lo = opts_.range_lo, hi = opts_.range_hi;

    IjkCursor cur(global_start, opts_.ni, opts_.nj);
    for (uint64_t x = 0; x < n; ++x, cur.step()) {
        const double v = static_cast<double>(p[x]);
        if (isNull(v, skip_nan, has_null, null_value)) continue;
        if (v >= lo && v <= hi) hit(cur.i, cur.j, cur.k);
    }
}

void WhereAccumulator::accumulate(
    HdfAryType type, const void* data, uint64_t n_elems, uint64_t global_start
) {
    if (data == nullptr || n_elems == 0 || ncols_ == 0) return;
    visitNumericType(type, [&](auto tag) {
        using T = decltype(tag);
        foldWindow<T>(static_cast<const T*>(data), n_elems, global_start);
    });
}

void WhereAccumulator::merge(const WhereAccumulator& other) {
    if (other.count_ == 0) return;
    if (count_ == 0) {
        *this = other;
        return;
    }
    imin_ = std::min(imin_, other.imin_);
    imax_ = std::max(imax_, other.imax_);
    jmin_ = std::min(jmin_, other.jmin_);
    jmax_ = std::max(jmax_, other.jmax_);
    kmin_ = std::min(kmin_, other.kmin_);
    kmax_ = std::max(kmax_, other.kmax_);
    count_ += other.count_;
    si_ += other.si_;
    sj_ += other.sj_;
    sk_ += other.sk_;
}

WhereResult WhereAccumulator::finalize() const {
    WhereResult r;
    r.count = count_;
    if (count_ == 0) return r;
    r.valid = true;
    r.imin = imin_; r.imax = imax_;
    r.jmin = jmin_; r.jmax = jmax_;
    r.kmin = kmin_; r.kmax = kmax_;
    const long double n = static_cast<long double>(count_);
    r.ci = static_cast<double>(si_ / n);
    r.cj = static_cast<double>(sj_ / n);
    r.ck = static_cast<double>(sk_ / n);
    return r;
}

//============================================================================
// MultiWhereAccumulator
//============================================================================

MultiWhereAccumulator::MultiWhereAccumulator(uint64_t ni, uint64_t nj, uint64_t nk)
    : ni_(ni), nj_(nj), nk_(nk), ncells_(ni * nj * nk) {
    const size_t words = static_cast<size_t>((ncells_ + 63) / 64);
    mask_.assign(words, 0);
    scratch_.assign(words, 0);
}

void MultiWhereAccumulator::beginArray(const WherePredicate& pred) {
    pred_ = pred;
    std::fill(scratch_.begin(), scratch_.end(), uint64_t{0});
}

template <typename T>
void MultiWhereAccumulator::foldWindow(const T* p, uint64_t n, uint64_t global_start) {
    const bool skip_nan = pred_.skip_nan;
    const bool has_null = pred_.has_null_value;
    const double null_value = pred_.null_value;
    const double lo = pred_.range_lo, hi = pred_.range_hi;

    // Clamp to the grid so a misdeclared window can never write past the mask.
    if (global_start >= ncells_) return;
    const uint64_t end = std::min(ncells_, global_start + n);
    for (uint64_t idx = global_start; idx < end; ++idx) {
        const double v = static_cast<double>(p[idx - global_start]);
        if (isNull(v, skip_nan, has_null, null_value)) continue;
        if (v >= lo && v <= hi) scratch_[idx >> 6] |= (uint64_t{1} << (idx & 63));
    }
}

void MultiWhereAccumulator::accumulate(
    HdfAryType type, const void* data, uint64_t n_elems, uint64_t global_start
) {
    if (data == nullptr || n_elems == 0 || ncells_ == 0) return;
    visitNumericType(type, [&](auto tag) {
        using T = decltype(tag);
        foldWindow<T>(static_cast<const T*>(data), n_elems, global_start);
    });
}

void MultiWhereAccumulator::commitArray() {
    if (arrays_committed_ == 0) {
        mask_.swap(scratch_);
    } else {
        for (size_t w = 0; w < mask_.size(); ++w) mask_[w] &= scratch_[w];
    }
    ++arrays_committed_;
}

bool MultiWhereAccumulator::empty() const {
    if (arrays_committed_ == 0) return ncells_ == 0;
    for (uint64_t w : mask_) {
        if (w != 0) return false;
    }
    return true;
}

WhereResult MultiWhereAccumulator::finalize() const {
    WhereResult r;
    if (arrays_committed_ == 0 || ncells_ == 0) return r;

    const auto bit = [&](uint64_t idx) -> bool {
        return (mask_[idx >> 6] >> (idx & 63)) & 1u;
    };

    // --- pass 1: whole passing set (count / bbox / centroid) ---
    long double si = 0, sj = 0, sk = 0;
    IjkCursor cur(0, ni_, nj_);
    for (uint64_t idx = 0; idx < ncells_; ++idx, cur.step()) {
        if (!bit(idx)) continue;
        if (r.count == 0) {
            r.imin = r.imax = cur.i;
            r.jmin = r.jmax = cur.j;
            r.kmin = r.kmax = cur.k;
        } else {
            if (cur.i < r.imin) r.imin = cur.i;
            if (cur.i > r.imax) r.imax = cur.i;
            if (cur.j < r.jmin) r.jmin = cur.j;
            if (cur.j > r.jmax) r.jmax = cur.j;
            if (cur.k < r.kmin) r.kmin = cur.k;
            if (cur.k > r.kmax) r.kmax = cur.k;
        }
        ++r.count;
        si += cur.i; sj += cur.j; sk += cur.k;
    }
    if (r.count == 0) return r;
    r.valid = true;
    {
        const long double n = static_cast<long double>(r.count);
        r.ci = static_cast<double>(si / n);
        r.cj = static_cast<double>(sj / n);
        r.ck = static_cast<double>(sk / n);
    }

    // --- pass 2: connected bodies (6-connectivity) over a visited bitmap ---
    std::vector<uint64_t> visited(mask_.size(), 0);
    const auto seen = [&](uint64_t idx) -> bool {
        return (visited[idx >> 6] >> (idx & 63)) & 1u;
    };
    const auto mark = [&](uint64_t idx) { visited[idx >> 6] |= (uint64_t{1} << (idx & 63)); };
    const uint64_t ncols = ni_ * nj_;

    std::vector<uint64_t> stack;
    IjkCursor seed(0, ni_, nj_);
    for (uint64_t idx = 0; idx < ncells_; ++idx, seed.step()) {
        if (!bit(idx) || seen(idx)) continue;
        ++r.components;

        uint64_t bcount = 0;
        uint64_t bimin = seed.i, bimax = seed.i, bjmin = seed.j, bjmax = seed.j;
        uint64_t bkmin = seed.k, bkmax = seed.k;
        long double bsi = 0, bsj = 0, bsk = 0;

        stack.clear();
        stack.push_back(idx);
        mark(idx);
        while (!stack.empty()) {
            const uint64_t c = stack.back();
            stack.pop_back();
            const uint64_t k = c / ncols;
            const uint64_t rem = c - k * ncols;
            const uint64_t j = rem / ni_;
            const uint64_t i = rem - j * ni_;

            ++bcount;
            if (i < bimin) bimin = i;
            if (i > bimax) bimax = i;
            if (j < bjmin) bjmin = j;
            if (j > bjmax) bjmax = j;
            if (k < bkmin) bkmin = k;
            if (k > bkmax) bkmax = k;
            bsi += i; bsj += j; bsk += k;

            const auto visit = [&](uint64_t nb) {
                if (bit(nb) && !seen(nb)) {
                    mark(nb);
                    stack.push_back(nb);
                }
            };
            if (i > 0) visit(c - 1);
            if (i + 1 < ni_) visit(c + 1);
            if (j > 0) visit(c - ni_);
            if (j + 1 < nj_) visit(c + ni_);
            if (k > 0) visit(c - ncols);
            if (k + 1 < nk_) visit(c + ncols);
        }

        if (bcount > r.body_count) {
            r.body_count = bcount;
            r.body_imin = bimin; r.body_imax = bimax;
            r.body_jmin = bjmin; r.body_jmax = bjmax;
            r.body_kmin = bkmin; r.body_kmax = bkmax;
            const long double n = static_cast<long double>(bcount);
            r.body_ci = static_cast<double>(bsi / n);
            r.body_cj = static_cast<double>(bsj / n);
            r.body_ck = static_cast<double>(bsk / n);
        }
    }
    return r;
}

} // namespace oes::eml::openkv
