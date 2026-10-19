// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/ChronoUtils.h>

// To experiment here, independent of our wrapping code
#define PLAY_WITH_DATE_DATE_H 0

#if PLAY_WITH_DATE_DATE_H
#define HAS_STRING_VIEW 1
#define HAS_UNCAUGHT_EXCEPTIONS 1
#include <oes/core/utils/ChronoDate.h>
#endif // PLAY_WITH_DATE_DATE_H

#include <oes/testing/core/TestMain.h>

#include <chrono>
#include <ctime>

using namespace oes::core;

namespace {

BOOST_AUTO_TEST_SUITE(ChronoUtilsTests)

BOOST_AUTO_TEST_CASE(test_parseCsvSimpleDurationPair) {
    struct {
        const char* spec; int t1; int t2;
    } timeout_cases[] = {
        { "10s,30s",    10*1000,   30*1000 },
        { "10s",        10*1000,   10*1000 }, // sets both
        { "3000",        3*1000,    3*1000 }, // legacy case
        { "500ms,10s",      500,   10*1000 },
        { "500 ms, 10 s ",  500,   10*1000 }, // spaces OK
        { "1000,3min",     1000, 3*60*1000 },
        { "3s,3h",       3*1000,        -1 }, // hour not supported
        { "3z,3",            -1,         3 }, // z invalid. Assume ms
    };
    for (const auto& tcase : timeout_cases) {
        auto actual = parseCsvSimpleDurationPair(tcase.spec);
        std::pair<
            std::chrono::milliseconds,
            std::chrono::milliseconds
        > expected{ tcase.t1, tcase.t2 };
        BOOST_CHECK(actual == expected);
    }
}

const struct {
    std::string input;
    int64_t expected_val; // -1 for expected to fail
    std::string expected_out_0_0;
    std::string expected_out_0_6;
    std::string expected_out_3_3;
    std::string expected_out_6_6;
} test_cases[] {
    // From https://sqlite.org/forum/forumpost/4a658e3adc?t=h
    { "2016-06-13T09:36:34",            1'465'810'594'000'000
    , "2016-06-13T09:36:34"
    , "2016-06-13T09:36:34"
    , "2016-06-13T09:36:34.000"
    , "2016-06-13T09:36:34.000000" },
    { "2016-06-13T09:36:34Z",           1'465'810'594'000'000
    , "2016-06-13T09:36:34"
    , "2016-06-13T09:36:34"
    , "2016-06-13T09:36:34.000"
    , "2016-06-13T09:36:34.000000" },
    { "2016-06-13T09:36:34.123Z",       1'465'810'594'123'000
    , "2016-06-13T09:36:34"
    , "2016-06-13T09:36:34.123"
    , "2016-06-13T09:36:34.123"
    , "2016-06-13T09:36:34.123000" },
    { "2016-06-13T09:36:34.123456Z",    1'465'810'594'123'456
    , "2016-06-13T09:36:34"
    , "2016-06-13T09:36:34.123456"
    , "2016-06-13T09:36:34.123"
    , "2016-06-13T09:36:34.123456" },

    // From http://books.xmlschemata.org/relaxng/ch19-77049.html
    { "2001-10-26T21:32:52",            1'004'131'972'000'000
    , "2001-10-26T21:32:52"
    , "2001-10-26T21:32:52"
    , "2001-10-26T21:32:52.000"
    , "2001-10-26T21:32:52.000000" },
    { "2001-10-26T21:32:52+02:00",      1'004'131'972'000'000 // TZ info "lost" I guess
    , "2001-10-26T21:32:52"
    , "2001-10-26T21:32:52"
    , "2001-10-26T21:32:52.000"
    , "2001-10-26T21:32:52.000000" },
    { "2001-10-26T21:32:52.12679",      1'004'131'972'126'790
    , "2001-10-26T21:32:52"
    , "2001-10-26T21:32:52.12679"
    , "2001-10-26T21:32:52.126"
    , "2001-10-26T21:32:52.126790" },

    { "2001-10-26T19:32:52Z",           1'004'124'772'000'000
    , "2001-10-26T19:32:52"
    , "2001-10-26T19:32:52"
    , "2001-10-26T19:32:52.000"
    , "2001-10-26T19:32:52.000000" },
    { "2001-10-26T19:32:52+00:00",      1'004'124'772'000'000
    , "2001-10-26T19:32:52"
    , "2001-10-26T19:32:52"
    , "2001-10-26T19:32:52.000"
    , "2001-10-26T19:32:52.000000" },

    { "-2001-10-26T21:32:52",        -125'286'834'428'000'000
    , "-2001-10-26T21:32:52"
    , "-2001-10-26T21:32:52"
    , "-2001-10-26T21:32:52.000"
    , "-2001-10-26T21:32:52.000000" },

    // The following values are invalid:
    { "2001-10-26",                 -1, "", "", "", "" }, // all the parts must be specified
    { "2001-10-26T21:32",           -1, "", "", "", "" }, // all the parts must be specified
    { "2001-10-26T25:32:52+02:00",  -1, "", "", "", "" }, // the hours part (25) is out of range
    { "01-10-26T21:32",             -1, "", "", "", "" }, // all the parts must be specified
};

#if PLAY_WITH_DATE_DATE_H
BOOST_AUTO_TEST_CASE(test_date_parse_and_format) {
    for (const auto& tc : test_cases) {
        std::chrono::system_clock::time_point tp;
        std::istringstream ss{ tc.input };
        ss >> date::parse("%Y-%m-%dT%T", tp);
        if (tc.expected_val == -1) {
            BOOST_CHECK_MESSAGE(ss.fail(), tc.input); // failed to parse
            continue;
        }
        BOOST_CHECK(!ss.fail());
        std::chrono::system_clock::duration ts = tp.time_since_epoch();
        std::chrono::microseconds usec = std::chrono::duration_cast<std::chrono::microseconds>(ts);
        BOOST_CHECK_MESSAGE(
            usec.count() == tc.expected_val,
            tc.input << ": " << usec.count() << " != " << tc.expected_val
        );

        std::string out_ms = date::format("%Y-%m-%dT%T", date::floor<std::chrono::milliseconds>(tp));
        BOOST_CHECK_EQUAL(out_ms, tc.expected_out_3_3);

        std::string out_us = date::format("%Y-%m-%dT%T", date::floor<std::chrono::microseconds>(tp));
        BOOST_CHECK_EQUAL(out_us, tc.expected_out_6_6);

        size_t len = 19;
        if (tc.expected_val < 0) {
            BOOST_CHECK(!out_ms.empty() && out_ms[0] == '-');
            BOOST_CHECK(!out_us.empty() && out_ms[0] == '-');
            ++len; // - sign in front
        }

        // We round-trip correctly (in text) up to the second at least
        BOOST_CHECK_EQUAL(out_ms.substr(0, len), tc.input.substr(0, len));
        BOOST_CHECK_EQUAL(out_us.substr(0, len), tc.input.substr(0, len));

        std::string out_sec = date::format("%Y-%m-%dT%T", date::floor<std::chrono::seconds>(tp));
        BOOST_CHECK_EQUAL(out_ms.substr(0, len), out_sec);
    }
}
#endif // PLAY_WITH_DATE_DATE_H

BOOST_AUTO_TEST_CASE(test_usecSinceEpochFromExtendedIsoDateTime) {
    for (const auto& tc : test_cases) {
#ifndef _WIN32
        // Parsing "negative date" fails on Linux.
        // See https://github.com/HowardHinnant/date/issues/621
        if (tc.expected_val < -1) {
            continue;
        }
#endif
        std::chrono::microseconds usec;
        bool parse_ok = oes::core::usecSinceEpochFromExtendedIsoDateTime(tc.input, usec);
        if (tc.expected_val == -1) {
            BOOST_CHECK_MESSAGE(!parse_ok, tc.input); // failed to parse
            continue;
        }
        BOOST_CHECK(parse_ok);

        BOOST_CHECK_MESSAGE(
            usec.count() == tc.expected_val,
            tc.input << ": " << usec.count() << " != " << tc.expected_val
        );
    }
}

BOOST_AUTO_TEST_CASE(test_roundTripExtendedIsoDateTime) {
    for (const auto& tc : test_cases) {
#ifndef _WIN32
        // Parsing "negative date" fails on Linux.
        // See https://github.com/HowardHinnant/date/issues/621
        if (tc.expected_val < -1) {
            continue;
        }
#endif
        std::chrono::system_clock::time_point tp;
        bool parse_ok = oes::core::parseExtendedIsoDateTime(tc.input, tp);
        if (tc.expected_val == -1) {
            BOOST_CHECK_MESSAGE(!parse_ok, tc.input); // failed to parse
            continue;
        }
        BOOST_CHECK(parse_ok);

        // default params, at-least 0 digits, at most 6
        std::string out_0_6 = oes::core::formatExtendedIsoDateTime(tp);
        BOOST_CHECK_EQUAL(out_0_6, tc.expected_out_0_6);

        // at-least 0 digits, at most 0, i.e. no fractional-seconds digits at all
        std::string out_0_0 = oes::core::formatExtendedIsoDateTime(tp, 0, 0);
        std::string out_0_0bis = oes::core::formatExtendedIsoDateTime(tp, FracSecRes::eWholeSecFloor);
        BOOST_CHECK_EQUAL(out_0_0, tc.expected_out_0_0);
        BOOST_CHECK_EQUAL(out_0_0, out_0_0bis);

        // at-least 3 digits, at most 3
        std::string out_3_3 = oes::core::formatExtendedIsoDateTime(tp, 3, 3);
        std::string out_3_3bis = oes::core::formatExtendedIsoDateTime(tp, FracSecRes::eMilliSecFloor);
        BOOST_CHECK_EQUAL(out_3_3, tc.expected_out_3_3);
        BOOST_CHECK_EQUAL(out_3_3, out_3_3bis);

        // at-least 6 digits, at most 6
        std::string out_6_6 = oes::core::formatExtendedIsoDateTime(tp, 6, 6);
        std::string out_6_6bis = oes::core::formatExtendedIsoDateTime(tp, FracSecRes::eMicroSecFloor);
        BOOST_CHECK_EQUAL(out_6_6, tc.expected_out_6_6);
        BOOST_CHECK_EQUAL(out_6_6, out_6_6bis);
    }
}

BOOST_AUTO_TEST_CASE(test_roundTripExtendedIsoDateTime_rounding) {
    const std::string dt = "2016-06-13T09:36:34.7651239";
    std::chrono::system_clock::time_point tp;
    BOOST_REQUIRE(oes::core::parseExtendedIsoDateTime(dt, tp));

    // returns the seconds and fractional seconds parts, as text
    auto test_me = [&dt, &tp](FracSecRes resolution) {
        std::string out = oes::core::formatExtendedIsoDateTime(tp, resolution);
        BOOST_CHECK(out.find(dt.substr(0, 17)) == 0);
        return out.substr(17);
    };

    BOOST_CHECK_EQUAL(test_me(FracSecRes::eWholeSecFloor), "34");
    BOOST_CHECK_EQUAL(test_me(FracSecRes::eMilliSecFloor), "34.765");
    BOOST_CHECK_EQUAL(test_me(FracSecRes::eMicroSecFloor), "34.765123");

    BOOST_CHECK_EQUAL(test_me(FracSecRes::eWholeSecRound), "35");
    BOOST_CHECK_EQUAL(test_me(FracSecRes::eMilliSecRound), "34.765");
    BOOST_CHECK_EQUAL(test_me(FracSecRes::eMicroSecRound), "34.765124");

    BOOST_CHECK_EQUAL(test_me(FracSecRes::eWholeSecCeil),  "35");
    BOOST_CHECK_EQUAL(test_me(FracSecRes::eMilliSecCeil),  "34.766");
    BOOST_CHECK_EQUAL(test_me(FracSecRes::eMicroSecCeil),  "34.765124");
}

BOOST_AUTO_TEST_CASE(test_roundTripExtendedIsoDateTime_ceil) {
}

using sys_clock = std::chrono::system_clock;

BOOST_AUTO_TEST_CASE(test_local_to_utc_and_back) {
    const auto now_tp = sys_clock::now(); // UTC
    const auto now_tt = sys_clock::to_time_t(now_tp);

    const auto local_tp = utcToLocal(now_tp);
  //const auto local_tt = sys_clock::to_time_t(local_tp);

    const auto global_tp = localToUtc(local_tp);
    const auto global_tt = sys_clock::to_time_t(global_tp);

    BOOST_CHECK(now_tp == global_tp);
    BOOST_CHECK_EQUAL(now_tt, global_tt);
}

BOOST_AUTO_TEST_CASE(test_local_aware_printing) {
    using namespace std::chrono;

    const auto now_tp = sys_clock::now(); // UTC
    const auto now_tt = sys_clock::to_time_t(now_tp);

    const auto local_tp = utcToLocal(now_tp);
  //const auto local_tt = sys_clock::to_time_t(local_tp);

    const std::tm local_tm = *std::localtime(&now_tt);
    const std::tm global_tm = *std::gmtime(&now_tt);

    const auto now_tp_sec = time_point_cast<seconds>(now_tp);
  //const auto local_tp_sec = time_point_cast<seconds>(local_tp);

    // std::asctime
    // Fixed C-Locale printing of local and global dates (NOT thread-safe)
    // Fixed 25-character form: Www Mmm dd hh:mm:ss yyyy\n
    const std::string local_asc(std::asctime(&local_tm)); // has trailing \n
    const std::string global_asc(std::asctime(&global_tm));

    char buf[64];
    size_t written = 0;

    // std::strftime
    // Local-aware local and global dates (thread-safe)
    written = std::strftime(buf, sizeof buf, "%c\n", &local_tm);
    BOOST_REQUIRE(written != 0);
  //const std::string local_str(buf);

    written = std::strftime(buf, sizeof buf, "%c\n", &global_tm);
    BOOST_REQUIRE(written != 0);
  //const std::string global_str(buf);

    // Date-time format equivalent to std::asctime()
    const auto asc_dtf = ascDatetimeFormat() + '\n';
    const char* asc_fmt = asc_dtf.c_str();

    written = std::strftime(buf, sizeof buf, asc_fmt, &local_tm);
    BOOST_REQUIRE(written != 0);
    const std::string local_asc_str(buf);

    written = std::strftime(buf, sizeof buf, asc_fmt, &global_tm);
    BOOST_REQUIRE(written != 0);
    const std::string global_asc_str(buf);

    const auto asSec = FracSecRes::eWholeSecFloor;
    const auto asUSec = FracSecRes::eMicroSecFloor;

    // formatDateTime() based on Howard Hinnant's date.h (now in C++20)
    // Local-aware local and global dates (thread-safe)
  //const std::string local_hhd(formatDateTime("%c\n", local_tp_sec, asSec));
  //const std::string global_hhd(formatDateTime("%c\n", now_tp_sec, asSec));

  //const std::string local_asc_hhd(formatDateTime(asc_dtf, local_tp_sec, asSec));
    const std::string global_asc_hhd(formatDateTime(asc_dtf, now_tp_sec, asSec));

    // All local-aware local-time printing should agree
    BOOST_CHECK_EQUAL(local_asc, local_asc_str);
    /*
    ** FIXME: Take cachedTzOffset() into account
    ** Also search for "daylight savings" in
    ** https://howardhinnant.github.io/date/date.html
    BOOST_CHECK_EQUAL(local_asc, local_asc_hhd);
    BOOST_CHECK_EQUAL(local_asc_str, local_asc_hhd);
    */

    // All global-time printing should agree
    BOOST_CHECK_EQUAL(global_asc, global_asc_str);
    BOOST_CHECK_EQUAL(global_asc, global_asc_hhd);
    BOOST_CHECK_EQUAL(global_asc_str, global_asc_hhd);

    // Full precisions: 1/10th microseconds on Windows, nanoseconds on Linux
    const std::string local_hhd_iso(formatIsoDateTime(local_tp));
    const std::string global_hhd_iso(formatIsoDateTime(now_tp));

    // Limit to microseconds
    const std::string local_hhd_isox(formatExtendedIsoDateTime(local_tp, asUSec));
    const std::string global_hhd_isox(formatExtendedIsoDateTime(now_tp, asUSec));
    BOOST_CHECK_LT(local_hhd_isox, local_hhd_iso); // since not as long
    BOOST_CHECK_LT(global_hhd_isox, global_hhd_iso);
}

BOOST_AUTO_TEST_SUITE_END()

} // anonymous namespace
