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

#ifndef OES_EML_OPENKV_ARRAYSTATS_H
#define OES_EML_OPENKV_ARRAYSTATS_H

#include "../export.h"

#include <oes/common/HdfAry.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace oes
{
    namespace eml
    {
        namespace openkv
        {

            //============================================================================
            // ArrayStats — streaming, mergeable summary statistics over a numeric array.
            //
            // This is the server-side counterpart to the "compute stats where the bytes
            // already live" principle. Rather than shipping millions of cell values up to
            // the gateway to be reduced in single-threaded JavaScript, the store folds an
            // array into a fixed-size summary in a single pass.
            //
            // Design goals (mirrors the established openkv patterns):
            //   * Bounded memory: the accumulator holds O(1) state regardless of array
            //     size, so a 100M-cell property never materialises as a value list.
            //   * Streaming: accumulate() is called once per out-of-row blob chunk; the
            //     caller drives the SpaceReader chunk loop and never holds the whole array.
            //   * Mergeable: merge() combines partial accumulators using the parallel
            //     Welford recurrence (Chan, Golub & LeVeque), enabling a multi-threaded
            //     reduction across disjoint chunk ranges.
            //   * Null-aware: an explicit null sentinel and IEEE NaN are excluded from the
            //     numeric summary and counted separately.
            //   * Numerically stable: mean/variance use Welford's online algorithm rather
            //     than the naive sum-of-squares, avoiding catastrophic cancellation.
            //
            // A single streaming pass can also answer a range predicate ("how many cells
            // fall in [lo, hi]?") for free via ArrayStatsOptions::count_in_range.
            //
            // Extension points (deliberately kept out of the core to stay lean):
            //   * Fixed-bin histogram / percentile sketch (e.g. t-digest): add a companion
            //     accumulator that shares the same accumulate()/merge()/finalize() shape.
            //
            // The fixed-bin histogram IS now folded in the same pass (see
            // ArrayStatsOptions::histogram_bins). Percentiles (p10/p50/p90 …) are derived
            // from the cumulative histogram by linear interpolation within the containing
            // bin (see approxPercentile), so their error is bounded by one bin width and
            // the whole thing stays O(bins) memory, single-pass and mergeable.
            //============================================================================

            //! Immutable summary produced by ArrayStatsAccumulator::finalize().
            struct OES_EML_EXPORT ArrayStatsResult
            {
                bool valid = false;      //!< false when no finite value was folded in.
                uint64_t count = 0;      //!< finite (non-null) values in the summary.
                uint64_t null_count = 0; //!< explicit-null + NaN values that were skipped.
                double minimum = 0.0;
                double maximum = 0.0;
                double mean = 0.0;
                double variance = 0.0; //!< population variance (M2 / count).
                double stddev = 0.0;   //!< sqrt(variance).
                double sum = 0.0;
                uint64_t in_range_count = 0; //!< finite values inside the query range (0 unless requested).

                // Fixed-bin histogram (empty unless ArrayStatsOptions::histogram_bins > 0).
                // Bin i covers [hist_lo + i*w, hist_lo + (i+1)*w) with w = (hist_hi-hist_lo)/bins;
                // the top edge (== hist_hi) folds into the last bin. Out-of-domain finite
                // values are tallied in hist_underflow / hist_overflow but still contribute to
                // count/min/max/mean/sum.
                std::vector<uint64_t> histogram;
                uint64_t hist_underflow = 0; //!< finite values < hist_lo.
                uint64_t hist_overflow = 0;  //!< finite values > hist_hi.
                double hist_lo = 0.0;
                double hist_hi = 0.0;
            };

            //! Caller-supplied knobs controlling how values are folded in.
            struct OES_EML_EXPORT ArrayStatsOptions
            {
                bool has_null_value = false; //!< when true, values == null_value are treated as null.
                double null_value = 0.0;     //!< sentinel used by discrete/region properties.
                bool skip_nan = true;        //!< count IEEE NaN as null instead of folding it in.

                // Range predicate, folded in the SAME streaming pass as the summary so a
                // question like "how many cells fall in [0.1, 0.2]?" costs no extra scan.
                bool count_in_range = false; //!< when true, count finite values in the range below.
                double range_lo = 0.0;
                double range_hi = 0.0;
                bool range_inclusive = true; //!< true => [lo, hi]; false => (lo, hi).

                // Fixed-bin histogram over an explicit [hist_lo, hist_hi] domain, folded in
                // the SAME streaming pass as the summary. An explicit domain is required so
                // binning is O(1) per element and mergeable without a pre-scan; finite values
                // outside the domain go to underflow/overflow (and still count toward the
                // numeric summary). histogram_bins == 0 disables the histogram and percentiles.
                uint32_t histogram_bins = 0;
                double hist_lo = 0.0;
                double hist_hi = 0.0;
            };

            //! Byte width of one element for a numeric HdfAryType, or 0 for non-numeric
            //! types (bytes/string/void). Kept in step with SqlAry::elem_size() and the
            //! HdfAryType -> C++ mapping in ImportEpc.cpp so streaming callers can size
            //! element-aligned read windows.
            OES_EML_EXPORT size_t numericElemSize(oes::common::hdf::HdfAryType type);

            //! Single-pass, mergeable accumulator. Not thread-safe on its own: give each
            //! worker its own instance over a disjoint chunk range, then merge() the results.
            class OES_EML_EXPORT ArrayStatsAccumulator
            {
            public:
                ArrayStatsAccumulator() = default;
                explicit ArrayStatsAccumulator(const ArrayStatsOptions &opts) : opts_(opts)
                {
                    if (opts_.histogram_bins > 0 && opts_.hist_hi > opts_.hist_lo)
                    {
                        hist_.assign(opts_.histogram_bins, 0);
                        hist_inv_w_ = static_cast<double>(opts_.histogram_bins) /
                                      (opts_.hist_hi - opts_.hist_lo);
                    }
                }

                //! Fold a single value. Hot path — kept inline. Null/NaN handling here keeps
                //! the typed accumulate() loop below branch-light per element.
                void add(double x)
                {
                    if (opts_.skip_nan && x != x)
                    {
                        ++null_count_;
                        return;
                    } // NaN
                    if (opts_.has_null_value && x == opts_.null_value)
                    {
                        ++null_count_;
                        return;
                    }
                    if (count_ == 0 || x < minimum_)
                        minimum_ = x;
                    if (count_ == 0 || x > maximum_)
                        maximum_ = x;
                    ++count_;
                    const double delta = x - mean_;
                    mean_ += delta / static_cast<double>(count_);
                    m2_ += delta * (x - mean_);
                    sum_ += x;
                    if (opts_.count_in_range)
                    {
                        const bool in = opts_.range_inclusive
                                            ? (x >= opts_.range_lo && x <= opts_.range_hi)
                                            : (x > opts_.range_lo && x < opts_.range_hi);
                        if (in)
                            ++in_range_;
                    }
                    if (!hist_.empty())
                    {
                        if (x < opts_.hist_lo)
                            ++hist_underflow_;
                        else if (x > opts_.hist_hi)
                            ++hist_overflow_;
                        else
                        {
                            // Map [lo, hi] -> [0, bins); clamp the top edge into the last bin.
                            int64_t b = static_cast<int64_t>((x - opts_.hist_lo) * hist_inv_w_);
                            if (b < 0)
                                b = 0;
                            else if (b >= static_cast<int64_t>(hist_.size()))
                                b = static_cast<int64_t>(hist_.size()) - 1;
                            ++hist_[static_cast<size_t>(b)];
                        }
                    }
                }

                //! Fold one contiguous chunk of `n_elems` values of the given element type.
                //! `data` is interpreted according to the store's HdfAryType -> C++ mapping
                //! (int32/int64/float/double/int8). Non-numeric types (bytes/string/void)
                //! are ignored so the caller never has to special-case them.
                //!
                //! `global_start` (the flat index of the first element in `data`) is accepted
                //! to keep a uniform accumulate() signature for position-aware accumulators;
                //! summary statistics are position-independent, so it is ignored here.
                void accumulate(oes::common::hdf::HdfAryType type, const void *data,
                                uint64_t n_elems, uint64_t global_start = 0);

                //! Combine a partial accumulator produced over a disjoint chunk range.
                void merge(const ArrayStatsAccumulator &other);

                //! Snapshot the current state as an immutable result.
                ArrayStatsResult finalize() const;

                uint64_t count() const { return count_; }
                uint64_t nullCount() const { return null_count_; }

            private:
                ArrayStatsOptions opts_{};
                uint64_t count_ = 0;
                uint64_t null_count_ = 0;
                uint64_t in_range_ = 0;
                double minimum_ = 0.0;
                double maximum_ = 0.0;
                double mean_ = 0.0;
                double m2_ = 0.0; //!< running sum of squared deviations (Welford).
                double sum_ = 0.0;
                std::vector<uint64_t> hist_{};    //!< fixed-bin counts (empty = histogram off).
                uint64_t hist_underflow_ = 0;     //!< finite values < hist_lo.
                uint64_t hist_overflow_ = 0;      //!< finite values > hist_hi.
                double hist_inv_w_ = 0.0;         //!< bins / (hist_hi - hist_lo), precomputed.
            };

            //! Convenience over an already-materialised in-memory array (e.g. the HdfAry a
            //! GetDataArrays read produces). Element count is derived from the array's
            //! uncompressed size and element type. For very large arrays prefer driving
            //! ArrayStatsAccumulator::accumulate() chunk-by-chunk so the whole array is
            //! never resident at once.
            OES_EML_EXPORT ArrayStatsResult computeArrayStats(
                const oes::common::hdf::HdfAry &ary,
                const ArrayStatsOptions &opts = {});

            //! Approximate the q-quantile (q in [0, 1]) from a finalized histogram result by
            //! linear interpolation within the containing bin. The absolute error is bounded
            //! by one bin width, (hist_hi - hist_lo) / bins, inside the domain. A quantile
            //! that falls in the underflow tail yields min(minimum, hist_lo); one in the
            //! overflow tail yields max(maximum, hist_hi). Returns NaN when the result carries
            //! no histogram or no finite value. q <= 0 returns the minimum; q >= 1 the maximum.
            OES_EML_EXPORT double approxPercentile(const ArrayStatsResult &r, double q);

        } // namespace openkv
    } // namespace eml
} // namespace oes

#endif // OES_EML_OPENKV_ARRAYSTATS_H
