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

#include "ChronoUtils.h"

// From https://howardhinnant.github.io/date/date.html
// which is part of C++20, but usable in C++11/14/17.
#define HAS_STRING_VIEW 1
#define HAS_UNCAUGHT_EXCEPTIONS 1
//#define ONLY_C_LOCALE 1

#include <oes/core/utils/config_enter_3rdparty.h>
#include <date/date.h>
#include <fmt/format.h>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

/*! \file oes/core/utils/ChronoUtils.h
 * \brief Utilities using standard <chrono> header.
 *
 * Additionally, Howard Hinnant's Date-library, now part of C++20 in <chrono>,
 * is supported via <date/date.h>, which can be either used directly, or
 * via the wrapper functions from this header which \em parse and \em format
 * std::chrono::system_clock::time_point inputs. This allows to benefit from
 * these new C++20 APIs, even though we are still on C++17, and avoids depending
 * on Boost Date-Time library, which unlike <date/date.h> is not header-only.
 */

namespace oes::core {

//============================================================================

/*!
 * \class HighResTimer oes/core/utils/ChronoUtils.h
 * \brief High-resolution timer for simple timing needs.
 * \ingroup oes_core_utils
 *
 * Minimalistic on purpose, notably not providing std::ostream operator, or
 * interop with oes::core::Format; simply call .elapsed() to \em print this timer.
 *
 * Also header-only and \em inline on purpose.
 */

/*!
 * \brief Restart this timer.
 */
void HighResTimer::restart() {
    start_ = clock_t::now();
}

/*!
 * \brief Elapsed time in floating-point seconds.
 * \return Elapsed time, since contrustion or last restart().
 */
double HighResTimer::elapsedSec() const {
    clock_t::time_point end = clock_t::now();
    std::chrono::duration<double> sec = end - start_;
    return sec.count();
}

/*!
 * \brief Elapsed time in seconds as a string, with milliseconds precision.
 * \return Elapsed time, since contrustion or last restart().
 */
std::string HighResTimer::elapsed() const {
    return fmt::format("{:.3f}s", elapsedSec());
}

//============================================================================

/*!
 * \brief Parses text consisting of a number followed by a duration unit.
 *
 * Only simple durations made of a single integer, followed by a single duration
 * unit, which must be one of "s" for seconds, "ms" for milliseconds, and "min"
 * for minutes. (if no unit is provided, milliseconds is assumed).
 *
 * Examples of values text: "1min", "30s", "500ms", etc...
 *
 * \param s the string to parse.
 * \return the parsed duration, in milliseconds, on success; -1 otherwise,
 *     typically when \a s is empty, or uses an unknown duration unit, or
 *     is badly formed.
 */
std::chrono::milliseconds parseSimpleDuration(std::string s) {
    s = boost::algorithm::trim_copy(s);

    size_t consumed = 0;
    int val = std::stoi(s, &consumed);
    if (!consumed) {
        return std::chrono::milliseconds{ -1 };
    }
    int factor = 1; // assume ms for backward-compatibility.
    const bool no_uom = (consumed == s.size());
    if (!no_uom) {
        std::string uom = boost::algorithm::trim_copy(s.substr(consumed));
        if (uom == "s") {
            factor = 1000;
        } else if (uom == "ms") {
            factor = 1;
        } else if (uom == "min") {
            factor = 60 * 1000;
        } else {
            return std::chrono::milliseconds{ -1 };
        }
    }
    return std::chrono::milliseconds(val * factor);
}

/*!
 * \brief Parses text consisting of two durations, separated by a comma.
 *
 * Splits the text into two parts around the comma, and uses parseSimpleDuration()
 * for both sides. For convenience, if a single duration is specified (i.e. no
 * comma followed by another duration), returns that duration for both members of
 * the returned pair.
 *
 * Examples of values text: "10s,30s", "500ms", "30s, 1min", etc...
 *
 * \param s the string to parse.
 * \return the parsed durations, in milliseconds, on success; -1 otherwise,
 *     for either or both durations.
 */
std::pair<
    std::chrono::milliseconds,
    std::chrono::milliseconds
> parseCsvSimpleDurationPair(const std::string& s) {
    if (s.empty()) {
        return { std::chrono::milliseconds{ -1 }, std::chrono::milliseconds{ -1 } };
    }

    std::vector<std::string> parts;
    namespace ba = boost::algorithm;
    ba::split(parts, s, ba::is_any_of(",;/|"), ba::token_compress_off);

    switch (parts.size()) {
    case 1: return { parseSimpleDuration(parts[0]), parseSimpleDuration(parts[0]) };
    case 2: return { parseSimpleDuration(parts[0]), parseSimpleDuration(parts[1]) };
    default: return { std::chrono::milliseconds{ -1 } , std::chrono::milliseconds{ -1 } };
    }
}

//============================================================================

//! \brief ISO-8601 date-time format
const std::string& isoDatetimeFormat() {
    static const/*expr*/ std::string fmt__("%Y-%m-%dT%T");
    return fmt__;
}

//! \brief Format used by std::asctime() minus the trailing newline
const std::string& ascDatetimeFormat() {
    static const/*expr*/ std::string fmt__("%a %b %e %H:%M:%S %Y");
    return fmt__;
}

/*!
 * \brief Parses a date-time string, to a time_point.
 * \param format the format specifiers. E.g. "%Y-%m-%dT%T" for ISO date-time.
 *     see https://howardhinnant.github.io/date/date.html#from_stream_formatting
 * \param s the date-time to parse
 * \param[out] tp the resulting time-point
 * \return true on successful parsing; false otherwise
 */
bool parseDateTime(
    const std::string& format,
    const std::string& s,
    std::chrono::system_clock::time_point& tp
) {
    std::istringstream ss{ s };
    ss >> date::parse(format, tp);
    return !ss.fail();
}

/*!
 * \brief Parses an Extended ISO date-time string, to a time_point.
 * \param s the ISO date-time to parse
 * \param[out] tp the resulting time-point
 * \return true on successful parsing; false otherwise
 *
 * \note numeric timezones annotation are accepted,
 *     but do not change the time_point returned
 * \note Uses parseDateTime("%Y-%m-%dT%T", s, tp)
 */
bool parseExtendedIsoDateTime(
    const std::string& s,
    std::chrono::system_clock::time_point& tp
) {
    return parseDateTime(isoDatetimeFormat(), s, tp);
}

/*!
 * \brief Parses an Extended ISO date-time string, to duration.
 * \param s the ISO \em extended date-time to parse
 * \param[out] usec micro-seconds since Epoch
 * \return true on successful parsing; false otherwise
 *
 * \note numeric timezones annotation are accepted,
 *     but do not change the duration returned
 * \note Uses parseExtendedIsoDateTime(s, tp)
 */
bool usecSinceEpochFromExtendedIsoDateTime(
    const std::string& s,
    std::chrono::microseconds& usec
) {
    std::chrono::system_clock::time_point tp;
    if (!parseExtendedIsoDateTime(s, tp)) {
        return false;
    }
    std::chrono::system_clock::duration ts = tp.time_since_epoch();
    usec = std::chrono::duration_cast<std::chrono::microseconds>(ts);
    return true;
}

/*!
 * \brief Converts a given time_point to string, according to a given format.
 * \param format the format specifiers. E.g. "%Y-%m-%dT%T" for ISO date-time.
 *     see https://howardhinnant.github.io/date/date.html#to_stream_formatting
 * \param tp the time-point to output
 * \return string representation of time_point \a tp
 */
std::string formatDateTime(
    const std::string& format,
    const std::chrono::system_clock::time_point& tp
) {
    assert(!format.empty());
    return date::format(format, tp);
}

/*!
 * \brief Converts a given time_point to string, using ISO date-time format.
 *
 * The number of fractional-second digits depends on the platforms
 * (typically 7 on Windows, and 9 on Linux). To control those digits,
 * use one of the formatExtendedIsoDateTime() overloads instead.
 * 
 * \param tp the time-point to output
 * \return string representation of time_point \a tp
 *
 * \note Uses formatDateTime("%Y-%m-%dT%T", tp);
 */
std::string formatIsoDateTime(
    const std::chrono::system_clock::time_point& tp
) {
    return date::format(isoDatetimeFormat(), tp);
}

/*!
 * \brief Converts a given time_point to string, using custom format.
 *
 * Outputs a date-time with a \b fixed number of fractional-second digits,
 * with (whole) seconds, milli-seconds, or micro-seconds resolution, and
 * flexible rounding behavior (\em floor, \em round, or \em ceil).
 *
 * By default the system_clock's time_point has 1/10th of microsecond resolution
 * on Windows, and nanosecond resolution on Linux. Which does not mean the CPU
 * clock \em ticks that fast, but how much resolution each time_point retains.
 *
 * \param tp the time-point to output
 * \param resolution how many fractional-seconds digits to use (0, 3 or 6)
 *     and how to round the value with just those digits.
 * \return string representation of time_point \a tp
 *
 * \note Uses formatIsoDateTime(tp);
 */
std::string formatDateTime(
    const std::string& format,
    const std::chrono::system_clock::time_point& tp,
    FracSecRes resolution
) {
    auto fmt_it = [format](auto dur) {
        return date::format(format, dur);
    };

    switch (resolution) {

    // Whole seconds
    case FracSecRes::eWholeSecFloor:
        return fmt_it(date::floor<std::chrono::seconds>(tp));
    case FracSecRes::eWholeSecRound:
        return fmt_it(date::round<std::chrono::seconds>(tp));
    case FracSecRes::eWholeSecCeil:
        return fmt_it(date::ceil<std::chrono::seconds>(tp));

    // Milli-seconds
    case FracSecRes::eMilliSecFloor:
        return fmt_it(date::floor<std::chrono::milliseconds>(tp));
    case FracSecRes::eMilliSecRound:
        return fmt_it(date::round<std::chrono::milliseconds>(tp));
    case FracSecRes::eMilliSecCeil:
        return fmt_it(date::ceil<std::chrono::milliseconds>(tp));

    // Micro-seconds
    case FracSecRes::eMicroSecFloor:
        return fmt_it(date::floor<std::chrono::microseconds>(tp));
    case FracSecRes::eMicroSecRound:
        return fmt_it(date::round<std::chrono::microseconds>(tp));
    case FracSecRes::eMicroSecCeil:
        return fmt_it(date::ceil<std::chrono::microseconds>(tp));

    } // No default on purpose

    assert(false);
    return {};
}

/*!
 * \brief Converts a given time_point to string, using ISO date-time format.
 *
 * Outputs a date-time with a \b fixed number of fractional-second digits,
 * with (whole) seconds, milli-seconds, or micro-seconds resolution, and
 * flexible rounding behavior (\em floor, \em round, or \em ceil).
 *
 * For a variable number of fractional-second digits, see its overload.
 *
 * \param tp the time-point to output
 * \param resolution how many fractional-seconds digits to use (0, 3 or 6)
 *     and how to round the value with just those digits.
 * \return string representation of time_point \a tp
 *
 * \note Uses formatIsoDateTime(tp);
 */
std::string formatExtendedIsoDateTime(
    const std::chrono::system_clock::time_point& tp,
    FracSecRes resolution
) {
    return formatDateTime(isoDatetimeFormat(), tp, resolution);
}

/*!
 * \brief Converts a given time_point to string, using ISO date-time format.
 *
 * Wrapper around formatIsoDateTime() yielding a variable number of
 * fractional-second digits, for the shortest text representation given
 * \a min_fractional_seconds and \a max_fractional_seconds.
 *
 * Trims trailing fractional-second digits past \a max_fractional_seconds.
 * Trims trailing fractional-second zeros, past \a min_fractional_seconds.
 * Trims trailing fractional-second \em period, for \em whole-second times.
 * These are text-based post-processing on fractional-second only.
 *
 * Unlike the overload taking a \a FracSecRes argument, this API supports
 * only \em floor()-like \em truncation of fractional-seconds, and a variable
 * number of fractional-second digits, depending on the time-point \a tp value,
 * while the overload always yields the same number of digits, and is more
 * flexible in how the fractional-seconds are rounded up or down.
 *
 * \param tp the time-point to output
 * \param min_fractional_seconds (default 0) How many fractional-second digits
 *     to retain, even when they are zeros
 * \param max_fractional_seconds (default 6) How many fractional-second digits
 *     to retain, even when more would be required to \em exactly represent
 *     \a tp's value. I.e. truncation.
 * \return string representation of time_point \a tp
 *
 * \note Uses formatIsoDateTime(tp);
 */
std::string formatExtendedIsoDateTime(
    const std::chrono::system_clock::time_point& tp,
    int min_fractional_seconds,
    int max_fractional_seconds
) {
    std::string s = formatIsoDateTime(tp);

    // Fixup fractional seconds
    // See https://github.com/HowardHinnant/date/issues/621
    size_t dot_pos = s.find('.');
    if (dot_pos != std::string::npos) {
        size_t tail_len = s.size() - dot_pos - 1; // excluding dot

        if (int(tail_len) > max_fractional_seconds) {
            // trim excess fractional seconds ("floor()"-like)
            s.resize(dot_pos + max_fractional_seconds + 1);
            tail_len = size_t(max_fractional_seconds);
        }

        while (tail_len && s.back() == '0') {
            // trim trailing zeros
            s.resize(s.size() - 1);
            --tail_len;
        }

        if (int(tail_len) < min_fractional_seconds) {
            // add trailing zeros
            s.insert(s.size(), size_t(min_fractional_seconds) - tail_len, '0');
        }

        if (s.back() == '.') {
            // trim when no digits behind
            s.resize(s.size() - 1);
        }
    }

    return s;
}

//============================================================================

/*!
 * \brief Offset in seconds between local and UTC (GMT timezone) times.
 * \return offset to \b add to local time to get UTC.
 * \warning Not thread-safe, since relies on std::gmtime and std::localtime.
 */
std::chrono::seconds tzOffset() {
    const std::time_t now = std::time(nullptr);
    std::tm gtm = *std::gmtime(&now);
    std::tm ltm = *std::localtime(&now);
    std::time_t ut = std::mktime(&ltm);
    std::time_t gt = std::mktime(&gtm);
    if (ut == -1 || gt == -1) {
        assert(false);
        return {}; // conversion failed...
    }
    return std::chrono::seconds(ut - gt);
}

/*!
 * \brief Cached offset in seconds between local and UTC (GMT timezone) times.
 *
 * When excluding DST (Daylight Saving Time), the time-zone offset is typically constant.
 * Calling this function early in main(), when the program is not yet multi-threaded
 * works around the tzOffset()'s non-thread-safety, and records the value for the remaining
 * duration of the process.
 *
 * \return cached result of tzOffset()
 */
std::chrono::seconds cachedTzOffset() {
    static const auto tz_offset = tzOffset();
    return tz_offset;
}

//============================================================================

/**
 * @brief Get the Partial Timeout Value to be used for send/recv rpc operations
 * 
 * @param max_msg_size 
 * @return std::chrono::milliseconds 
 */
std::chrono::milliseconds getPartialTimeoutValue(const size_t max_msg_size) {
    // Less than 200mb use always 20 seconds
    if (max_msg_size <= 200'000'000) {
        return std::chrono::milliseconds(20000);
    }
    
    // max_msg_size * 100 / 1'000'000 (mb) -> where 100 is a good multication factor.
    // results in max_msg_size / 10000 in ms
    // 200 mb -> 20000 ms
    // 300 mb -> 30000 ms
    const size_t time_ms = static_cast<size_t>(static_cast<double>(max_msg_size) / 10000);
    return std::chrono::milliseconds(time_ms);
}

} // namespace oes::core
