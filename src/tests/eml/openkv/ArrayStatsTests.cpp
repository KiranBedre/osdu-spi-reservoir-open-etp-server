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

#include <oes/eml/openkv/ArrayStats.h>
#include <oes/common/HdfAry.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cmath>
#include <limits>
#include <cstdint>
#include <vector>

namespace
{

    using namespace oes::eml::openkv;
    using oes::common::hdf::HdfAry;
    using oes::common::hdf::HdfAryType;

    // Reference sample: count=8, min=2, max=9, sum=40, mean=5,
    // population variance = 4, stddev = 2.
    static const double kSample[] = {2, 4, 4, 4, 5, 5, 7, 9};
    static const uint64_t kSampleN = 8;

    BOOST_AUTO_TEST_SUITE(ArrayStatsTests)

    BOOST_AUTO_TEST_CASE(empty_is_invalid)
    {
        ArrayStatsAccumulator acc;
        ArrayStatsResult r = acc.finalize();
        BOOST_CHECK(!r.valid);
        BOOST_CHECK_EQUAL(r.count, 0u);
        BOOST_CHECK_EQUAL(r.null_count, 0u);
    }

    BOOST_AUTO_TEST_CASE(double_basic)
    {
        ArrayStatsAccumulator acc;
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult r = acc.finalize();
        BOOST_CHECK(r.valid);
        BOOST_CHECK_EQUAL(r.count, 8u);
        BOOST_CHECK_CLOSE(r.minimum, 2.0, 1e-9);
        BOOST_CHECK_CLOSE(r.maximum, 9.0, 1e-9);
        BOOST_CHECK_CLOSE(r.sum, 40.0, 1e-9);
        BOOST_CHECK_CLOSE(r.mean, 5.0, 1e-9);
        BOOST_CHECK_CLOSE(r.variance, 4.0, 1e-9);
        BOOST_CHECK_CLOSE(r.stddev, 2.0, 1e-9);
    }

    BOOST_AUTO_TEST_CASE(int32_and_int64)
    {
        const int32_t i32[] = {2, 4, 4, 4, 5, 5, 7, 9};
        ArrayStatsAccumulator a32;
        a32.accumulate(oes::common::hdf::eAryOfInt, i32, kSampleN);
        BOOST_CHECK_CLOSE(a32.finalize().mean, 5.0, 1e-9);

        const int64_t i64[] = {2, 4, 4, 4, 5, 5, 7, 9};
        ArrayStatsAccumulator a64;
        a64.accumulate(oes::common::hdf::eAryOfLong, i64, kSampleN);
        BOOST_CHECK_CLOSE(a64.finalize().mean, 5.0, 1e-9);
    }

    BOOST_AUTO_TEST_CASE(float_and_boolean)
    {
        const float f[] = {2, 4, 4, 4, 5, 5, 7, 9};
        ArrayStatsAccumulator af;
        af.accumulate(oes::common::hdf::eAryOfFloat, f, kSampleN);
        BOOST_CHECK_CLOSE(af.finalize().sum, 40.0, 1e-6);

        const int8_t b[] = {1, 0, 1, 1, 0, 1}; // 4 ones -> mean 4/6
        ArrayStatsAccumulator ab;
        ab.accumulate(oes::common::hdf::eAryOfBoolean, b, 6);
        ArrayStatsResult r = ab.finalize();
        BOOST_CHECK_EQUAL(r.count, 6u);
        BOOST_CHECK_CLOSE(r.sum, 4.0, 1e-9);
    }

    BOOST_AUTO_TEST_CASE(null_value_is_excluded)
    {
        ArrayStatsOptions opts;
        opts.has_null_value = true;
        opts.null_value = 4.0; // exclude the three 4s
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult r = acc.finalize();
        BOOST_CHECK_EQUAL(r.count, 5u);
        BOOST_CHECK_EQUAL(r.null_count, 3u);
        BOOST_CHECK_CLOSE(r.sum, 28.0, 1e-9); // 40 - 12
    }

    BOOST_AUTO_TEST_CASE(nan_counts_as_null)
    {
        const double d[] = {1.0, std::nan(""), 3.0};
        ArrayStatsAccumulator acc;
        acc.accumulate(oes::common::hdf::eAryOfDouble, d, 3);
        ArrayStatsResult r = acc.finalize();
        BOOST_CHECK_EQUAL(r.count, 2u);
        BOOST_CHECK_EQUAL(r.null_count, 1u);
        BOOST_CHECK_CLOSE(r.sum, 4.0, 1e-9);
    }

    BOOST_AUTO_TEST_CASE(streaming_equals_single_pass)
    {
        ArrayStatsAccumulator single;
        single.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult sr = single.finalize();

        ArrayStatsAccumulator streamed;
        for (uint64_t i = 0; i < kSampleN; ++i)
        {
            streamed.accumulate(oes::common::hdf::eAryOfDouble, &kSample[i], 1);
        }
        ArrayStatsResult tr = streamed.finalize();
        BOOST_CHECK_CLOSE(sr.mean, tr.mean, 1e-9);
        BOOST_CHECK_CLOSE(sr.variance, tr.variance, 1e-9);
        BOOST_CHECK_CLOSE(sr.sum, tr.sum, 1e-9);
        BOOST_CHECK_EQUAL(sr.count, tr.count);
    }

    BOOST_AUTO_TEST_CASE(merge_equals_single_pass)
    {
        ArrayStatsAccumulator single;
        single.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult sr = single.finalize();

        ArrayStatsAccumulator a;
        a.accumulate(oes::common::hdf::eAryOfDouble, &kSample[0], 3); // {2,4,4}
        ArrayStatsAccumulator b;
        b.accumulate(oes::common::hdf::eAryOfDouble, &kSample[3], 5); // {4,5,5,7,9}
        a.merge(b);
        ArrayStatsResult mr = a.finalize();

        BOOST_CHECK_CLOSE(sr.mean, mr.mean, 1e-9);
        BOOST_CHECK_CLOSE(sr.variance, mr.variance, 1e-9);
        BOOST_CHECK_CLOSE(sr.sum, mr.sum, 1e-9);
        BOOST_CHECK_EQUAL(sr.count, mr.count);
        BOOST_CHECK_CLOSE(sr.minimum, mr.minimum, 1e-9);
        BOOST_CHECK_CLOSE(sr.maximum, mr.maximum, 1e-9);
    }

    BOOST_AUTO_TEST_CASE(merge_with_empty)
    {
        ArrayStatsAccumulator a;
        a.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsAccumulator empty;
        a.merge(empty);
        empty.merge(a); // empty adopts a
        BOOST_CHECK_CLOSE(a.finalize().mean, 5.0, 1e-9);
        BOOST_CHECK_CLOSE(empty.finalize().mean, 5.0, 1e-9);
        BOOST_CHECK_EQUAL(empty.finalize().count, 8u);
    }

    BOOST_AUTO_TEST_CASE(non_numeric_is_ignored)
    {
        const unsigned char bytes[] = {1, 2, 3, 4};
        ArrayStatsAccumulator acc;
        acc.accumulate(oes::common::hdf::eAryOfBytes, bytes, 4);
        acc.accumulate(oes::common::hdf::eAryOfString, bytes, 4);
        acc.accumulate(oes::common::hdf::eAryOfVoid, bytes, 4);
        BOOST_CHECK(!acc.finalize().valid);
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfBytes), 0u);
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfString), 0u);
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfVoid), 0u);
    }

    BOOST_AUTO_TEST_CASE(numeric_elem_size_mapping)
    {
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfInt), sizeof(int32_t));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfLong), sizeof(int64_t));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfFloat), sizeof(float));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfDouble), sizeof(double));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfBoolean), sizeof(int8_t));
    }

    BOOST_AUTO_TEST_CASE(range_count_inclusive)
    {
        ArrayStatsOptions opts;
        opts.count_in_range = true;
        opts.range_lo = 4.0;
        opts.range_hi = 5.0;
        opts.range_inclusive = true; // [4,5] -> {4,4,4,5,5} = 5
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult r = acc.finalize();
        BOOST_CHECK_EQUAL(r.in_range_count, 5u);
        BOOST_CHECK_EQUAL(r.count, 8u); // range predicate does not affect the summary
    }

    BOOST_AUTO_TEST_CASE(range_count_exclusive)
    {
        ArrayStatsOptions opts;
        opts.count_in_range = true;
        opts.range_lo = 4.0;
        opts.range_hi = 5.0;
        opts.range_inclusive = false; // (4,5) -> {} = 0
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        BOOST_CHECK_EQUAL(acc.finalize().in_range_count, 0u);
    }

    BOOST_AUTO_TEST_CASE(range_count_survives_merge)
    {
        ArrayStatsOptions opts;
        opts.count_in_range = true;
        opts.range_lo = 4.0;
        opts.range_hi = 9.0;
        opts.range_inclusive = true; // [4,9] -> {4,4,4,5,5,7,9} = 7
        ArrayStatsAccumulator a(opts);
        a.accumulate(oes::common::hdf::eAryOfDouble, &kSample[0], 3);
        ArrayStatsAccumulator b(opts);
        b.accumulate(oes::common::hdf::eAryOfDouble, &kSample[3], 5);
        a.merge(b);
        BOOST_CHECK_EQUAL(a.finalize().in_range_count, 7u);
    }

    BOOST_AUTO_TEST_CASE(histogram_bins_and_tails)
    {
        // Domain [0,10], 10 unit-width bins. Sample {2,4,4,4,5,5,7,9}.
        ArrayStatsOptions opts;
        opts.histogram_bins = 10;
        opts.hist_lo = 0.0;
        opts.hist_hi = 10.0;
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult r = acc.finalize();
        BOOST_REQUIRE_EQUAL(r.histogram.size(), 10u);
        BOOST_CHECK_EQUAL(r.histogram[2], 1u);
        BOOST_CHECK_EQUAL(r.histogram[4], 3u);
        BOOST_CHECK_EQUAL(r.histogram[5], 2u);
        BOOST_CHECK_EQUAL(r.histogram[7], 1u);
        BOOST_CHECK_EQUAL(r.histogram[9], 1u);
        BOOST_CHECK_EQUAL(r.hist_underflow, 0u);
        BOOST_CHECK_EQUAL(r.hist_overflow, 0u);
        BOOST_CHECK_CLOSE(r.hist_lo, 0.0, 1e-9);
        BOOST_CHECK_CLOSE(r.hist_hi, 10.0, 1e-9);
    }

    BOOST_AUTO_TEST_CASE(histogram_underflow_overflow)
    {
        // Domain [4,6], 2 unit-width bins: bin0=[4,5), bin1=[5,6].
        ArrayStatsOptions opts;
        opts.histogram_bins = 2;
        opts.hist_lo = 4.0;
        opts.hist_hi = 6.0;
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult r = acc.finalize();
        BOOST_REQUIRE_EQUAL(r.histogram.size(), 2u);
        BOOST_CHECK_EQUAL(r.histogram[0], 3u); // {4,4,4}
        BOOST_CHECK_EQUAL(r.histogram[1], 2u); // {5,5}
        BOOST_CHECK_EQUAL(r.hist_underflow, 1u); // {2}
        BOOST_CHECK_EQUAL(r.hist_overflow, 2u);  // {7,9}
        // Summary is unaffected by the histogram domain.
        BOOST_CHECK_EQUAL(r.count, 8u);
        BOOST_CHECK_CLOSE(r.sum, 40.0, 1e-9);
    }

    BOOST_AUTO_TEST_CASE(histogram_survives_merge)
    {
        ArrayStatsOptions opts;
        opts.histogram_bins = 10;
        opts.hist_lo = 0.0;
        opts.hist_hi = 10.0;
        ArrayStatsAccumulator a(opts);
        a.accumulate(oes::common::hdf::eAryOfDouble, &kSample[0], 3); // {2,4,4}
        ArrayStatsAccumulator b(opts);
        b.accumulate(oes::common::hdf::eAryOfDouble, &kSample[3], 5); // {4,5,5,7,9}
        a.merge(b);
        ArrayStatsResult r = a.finalize();
        BOOST_REQUIRE_EQUAL(r.histogram.size(), 10u);
        BOOST_CHECK_EQUAL(r.histogram[4], 3u);
        BOOST_CHECK_EQUAL(r.histogram[5], 2u);
    }

    BOOST_AUTO_TEST_CASE(approx_percentile_from_histogram)
    {
        ArrayStatsOptions opts;
        opts.histogram_bins = 10;
        opts.hist_lo = 0.0;
        opts.hist_hi = 10.0;
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult r = acc.finalize();

        const double p10 = approxPercentile(r, 0.10);
        const double p50 = approxPercentile(r, 0.50);
        const double p90 = approxPercentile(r, 0.90);
        BOOST_CHECK(!std::isnan(p50));
        // Monotonic and clamped to [min,max].
        BOOST_CHECK(p10 <= p50 && p50 <= p90);
        BOOST_CHECK(p10 >= r.minimum && p90 <= r.maximum);
        // Median mass falls in the [4,5) / [5,6) region; within one bin width of 4.5.
        BOOST_CHECK(p50 >= 4.0 && p50 <= 6.0);
        // q<=0 / q>=1 return the extremes.
        BOOST_CHECK_CLOSE(approxPercentile(r, 0.0), r.minimum, 1e-9);
        BOOST_CHECK_CLOSE(approxPercentile(r, 1.0), r.maximum, 1e-9);
    }

    // Review finding: a quantile that lands in the underflow tail used to be
    // clamped UP to max(minimum, hist_lo) and one in the overflow tail DOWN to
    // min(maximum, hist_hi) -- i.e. the wrong side of the domain edge. The
    // underflow tail must never report a value above hist_lo, the overflow tail
    // never a value below hist_hi, and the result stays monotonic in q.
    BOOST_AUTO_TEST_CASE(approx_percentile_tails_land_outside_domain)
    {
        // 6 of 10 values below the domain, 1 inside, 3 above.
        const double vals[] = {-50, -40, -30, -20, -10, -5, 5, 20, 30, 40};
        ArrayStatsOptions opts;
        opts.histogram_bins = 10;
        opts.hist_lo = 0.0;
        opts.hist_hi = 10.0;
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, vals, 10);
        ArrayStatsResult r = acc.finalize();
        BOOST_REQUIRE_EQUAL(r.hist_underflow, 6u);
        BOOST_REQUIRE_EQUAL(r.hist_overflow, 3u);

        const double p10 = approxPercentile(r, 0.10); // underflow tail
        const double p50 = approxPercentile(r, 0.50); // underflow tail (6/10)
        const double p65 = approxPercentile(r, 0.65); // the single in-domain value
        const double p90 = approxPercentile(r, 0.90); // overflow tail

        BOOST_CHECK(p10 <= r.hist_lo);
        BOOST_CHECK(p50 <= r.hist_lo);
        BOOST_CHECK_CLOSE(p10, r.minimum, 1e-9); // min(-50, 0) == minimum
        BOOST_CHECK(p65 >= r.hist_lo && p65 <= r.hist_hi);
        BOOST_CHECK(p90 >= r.hist_hi);
        BOOST_CHECK_CLOSE(p90, r.maximum, 1e-9); // max(40, 10) == maximum
        BOOST_CHECK(p10 <= p50 && p50 <= p65 && p65 <= p90);
    }

    // Tails when the domain is wider than the data: the tail edge is then the
    // domain edge, so the tail value must still not cross into the domain.
    BOOST_AUTO_TEST_CASE(approx_percentile_tails_when_domain_exceeds_data)
    {
        const double vals[] = {-1.0, 0.5, 11.0}; // one under, one in, one over
        ArrayStatsOptions opts;
        opts.histogram_bins = 10;
        opts.hist_lo = 0.0;
        opts.hist_hi = 10.0;
        ArrayStatsAccumulator acc(opts);
        acc.accumulate(oes::common::hdf::eAryOfDouble, vals, 3);
        ArrayStatsResult r = acc.finalize();
        BOOST_CHECK(approxPercentile(r, 0.2) <= 0.0);  // underflow: min(-1, 0) = -1
        BOOST_CHECK(approxPercentile(r, 0.95) >= 10.0); // overflow: max(11, 10) = 11
    }

    // Integer null sentinels (INT32_MIN / INT64_MIN) must be excluded when the
    // caller opts in, exactly as GetDataArrayStats does for discrete arrays.
    BOOST_AUTO_TEST_CASE(integer_null_sentinel_is_excluded)
    {
        const int32_t kNull32 = std::numeric_limits<int32_t>::min();
        const int32_t i32[] = {1, kNull32, 2, 3, kNull32};
        ArrayStatsOptions o32;
        o32.has_null_value = true;
        o32.null_value = static_cast<double>(kNull32);
        ArrayStatsAccumulator a32(o32);
        a32.accumulate(oes::common::hdf::eAryOfInt, i32, 5);
        ArrayStatsResult r32 = a32.finalize();
        BOOST_CHECK_EQUAL(r32.count, 3u);
        BOOST_CHECK_EQUAL(r32.null_count, 2u);
        BOOST_CHECK_CLOSE(r32.minimum, 1.0, 1e-12);
        BOOST_CHECK_CLOSE(r32.mean, 2.0, 1e-12);

        const int64_t kNull64 = std::numeric_limits<int64_t>::min();
        const int64_t i64[] = {kNull64, 10, 20};
        ArrayStatsOptions o64;
        o64.has_null_value = true;
        o64.null_value = static_cast<double>(kNull64);
        ArrayStatsAccumulator a64(o64);
        a64.accumulate(oes::common::hdf::eAryOfLong, i64, 3);
        ArrayStatsResult r64 = a64.finalize();
        BOOST_CHECK_EQUAL(r64.count, 2u);
        BOOST_CHECK_EQUAL(r64.null_count, 1u);
        BOOST_CHECK_CLOSE(r64.minimum, 10.0, 1e-12);

        // Without the opt-in the sentinel is an ordinary (very negative) value.
        ArrayStatsAccumulator plain;
        plain.accumulate(oes::common::hdf::eAryOfInt, i32, 5);
        BOOST_CHECK_EQUAL(plain.finalize().null_count, 0u);
    }

    BOOST_AUTO_TEST_CASE(approx_percentile_without_histogram_is_nan)
    {
        ArrayStatsAccumulator acc;
        acc.accumulate(oes::common::hdf::eAryOfDouble, kSample, kSampleN);
        ArrayStatsResult r = acc.finalize();
        BOOST_CHECK(r.histogram.empty());
        BOOST_CHECK(std::isnan(approxPercentile(r, 0.5)));
    }

    BOOST_AUTO_TEST_CASE(compute_over_hdf_ary)
    {
        HdfAry ary;
        ary.elem_type = oes::common::hdf::eAryOfDouble;
        ary.uncompressed_size = kSampleN * sizeof(double);
        ary.refd_data = kSample; // borrowed storage
        ary.owned_data = nullptr;
        ArrayStatsResult r = computeArrayStats(ary);
        BOOST_CHECK(r.valid);
        BOOST_CHECK_EQUAL(r.count, 8u);
        BOOST_CHECK_CLOSE(r.mean, 5.0, 1e-9);
        ary.refd_data = nullptr; // avoid ~HdfAry touching borrowed storage
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace
