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

#include "ArrayStats.h"
#include "NumericType.h"

#include <cmath>
#include <limits>

namespace oes
{
    namespace eml
    {
        namespace openkv
        {

            using oes::common::hdf::HdfAry;
            using oes::common::hdf::HdfAryType;

            size_t numericElemSize(HdfAryType type)
            {
                // Single source of truth for the numeric type set: ask the shared visitor
                // for the mapped C++ type and report its width (0 for non-numeric types).
                size_t elem = 0;
                visitNumericType(type, [&](auto tag) { elem = sizeof(decltype(tag)); });
                return elem;
            }

            namespace
            {

                //! Fold a typed contiguous run of `n` values into `acc` via the inline add().
                //! Processed in fixed-size cache-friendly blocks with an 8-way unroll. The
                //! element visitation ORDER is preserved exactly (add() runs a loop-carried
                //! Welford update, so any reordering would change the floating-point result);
                //! the unroll only improves instruction locality, results are bit-identical.
                template <typename T>
                inline void foldTyped(ArrayStatsAccumulator &acc, const void *data, uint64_t n)
                {
                    const T *p = static_cast<const T *>(data);
                    uint64_t i = 0;
                    const uint64_t n8 = n & ~static_cast<uint64_t>(7);
                    for (; i < n8; i += 8)
                    {
                        acc.add(static_cast<double>(p[i + 0]));
                        acc.add(static_cast<double>(p[i + 1]));
                        acc.add(static_cast<double>(p[i + 2]));
                        acc.add(static_cast<double>(p[i + 3]));
                        acc.add(static_cast<double>(p[i + 4]));
                        acc.add(static_cast<double>(p[i + 5]));
                        acc.add(static_cast<double>(p[i + 6]));
                        acc.add(static_cast<double>(p[i + 7]));
                    }
                    for (; i < n; ++i)
                        acc.add(static_cast<double>(p[i]));
                }

            } // namespace

            void ArrayStatsAccumulator::accumulate(HdfAryType type, const void *data, uint64_t n_elems,
                                                   uint64_t /*global_start*/)
            {
                if (data == nullptr || n_elems == 0)
                    return;
                // Non-numeric types (bytes/string/void) are skipped by the visitor returning
                // false; numeric types fold via the unrolled foldTyped<T> hot loop.
                visitNumericType(type, [&](auto tag) {
                    foldTyped<decltype(tag)>(*this, data, n_elems);
                });
            }

            void ArrayStatsAccumulator::merge(const ArrayStatsAccumulator &other)
            {
                // Nulls and in-range counts are simple additive tallies regardless of the
                // numeric state.
                null_count_ += other.null_count_;
                in_range_ += other.in_range_;

                // Histogram bins and out-of-domain tallies add element-wise.
                hist_underflow_ += other.hist_underflow_;
                hist_overflow_ += other.hist_overflow_;
                if (!other.hist_.empty())
                {
                    if (hist_.empty())
                        hist_ = other.hist_;
                    else if (hist_.size() == other.hist_.size())
                        for (size_t i = 0; i < hist_.size(); ++i)
                            hist_[i] += other.hist_[i];
                }

                if (other.count_ == 0)
                    return; // nothing numeric to merge
                if (count_ == 0)
                { // adopt the other side wholesale
                    count_ = other.count_;
                    minimum_ = other.minimum_;
                    maximum_ = other.maximum_;
                    mean_ = other.mean_;
                    m2_ = other.m2_;
                    sum_ = other.sum_;
                    return;
                }

                // Parallel Welford combine (Chan, Golub & LeVeque).
                const double na = static_cast<double>(count_);
                const double nb = static_cast<double>(other.count_);
                const double nab = na + nb;
                const double delta = other.mean_ - mean_;

                mean_ = (na * mean_ + nb * other.mean_) / nab;
                m2_ = m2_ + other.m2_ + delta * delta * (na * nb / nab);
                sum_ += other.sum_;
                if (other.minimum_ < minimum_)
                    minimum_ = other.minimum_;
                if (other.maximum_ > maximum_)
                    maximum_ = other.maximum_;
                count_ = static_cast<uint64_t>(nab);
            }

            ArrayStatsResult ArrayStatsAccumulator::finalize() const
            {
                ArrayStatsResult r;
                r.null_count = null_count_;
                r.in_range_count = in_range_;
                // Histogram payload is independent of the numeric-validity early-out below so
                // an all-null array still reports its (empty) histogram domain honestly.
                r.hist_lo = opts_.hist_lo;
                r.hist_hi = opts_.hist_hi;
                r.hist_underflow = hist_underflow_;
                r.hist_overflow = hist_overflow_;
                r.histogram = hist_;
                if (count_ == 0)
                {
                    r.valid = false;
                    return r;
                }
                r.valid = true;
                r.count = count_;
                r.minimum = minimum_;
                r.maximum = maximum_;
                r.mean = mean_;
                r.sum = sum_;
                r.variance = m2_ / static_cast<double>(count_); // population variance
                r.stddev = std::sqrt(r.variance);
                return r;
            }

            ArrayStatsResult computeArrayStats(const HdfAry &ary, const ArrayStatsOptions &opts)
            {
                ArrayStatsAccumulator acc(opts);
                const size_t elem = numericElemSize(ary.elem_type);
                if (elem != 0 && ary.data() != nullptr && ary.uncompressed_size != 0)
                {
                    const uint64_t n_elems = ary.uncompressed_size / static_cast<uint64_t>(elem);
                    acc.accumulate(ary.elem_type, ary.data(), n_elems);
                }
                return acc.finalize();
            }

            double approxPercentile(const ArrayStatsResult &r, double q)
            {
                if (!r.valid || r.histogram.empty())
                    return std::numeric_limits<double>::quiet_NaN();
                if (q <= 0.0)
                    return r.minimum;
                if (q >= 1.0)
                    return r.maximum;

                // Total mass folded into the histogram domain, including the out-of-domain
                // tails so a percentile that lands in a tail clamps to the domain edge.
                uint64_t total = r.hist_underflow + r.hist_overflow;
                for (uint64_t c : r.histogram)
                    total += c;
                if (total == 0)
                    return std::numeric_limits<double>::quiet_NaN();

                const double target = q * static_cast<double>(total);
                const size_t bins = r.histogram.size();
                const double w = (r.hist_hi - r.hist_lo) / static_cast<double>(bins);

                // The quantile lands in the underflow tail (mass strictly below hist_lo).
                // Its true value is somewhere in [minimum, hist_lo); the histogram cannot
                // locate it more precisely, so report the lower edge of that interval,
                // i.e. the smaller of the two -- never a value above hist_lo.
                double cum = static_cast<double>(r.hist_underflow);
                if (target <= cum)
                    return r.minimum < r.hist_lo ? r.minimum : r.hist_lo;

                for (size_t i = 0; i < bins; ++i)
                {
                    const double c = static_cast<double>(r.histogram[i]);
                    if (c > 0.0 && target <= cum + c)
                    {
                        const double frac = (target - cum) / c; // position within the bin
                        double v = r.hist_lo + (static_cast<double>(i) + frac) * w;
                        if (v < r.minimum)
                            v = r.minimum;
                        if (v > r.maximum)
                            v = r.maximum;
                        return v;
                    }
                    cum += c;
                }
                // The quantile lands in the overflow tail (mass strictly above hist_hi).
                // Its true value is in (hist_hi, maximum]; report the upper edge of that
                // interval -- never a value below hist_hi.
                return r.maximum > r.hist_hi ? r.maximum : r.hist_hi;
            }

        } // namespace openkv
    } // namespace eml
} // namespace oes
