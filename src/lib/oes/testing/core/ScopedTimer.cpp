// ============================================================================
// Copyright 2013-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <iostream>

#include <oes/testing/core/ScopedTimer.h>

#include <oes/core/utils/System.h>

namespace oes {
namespace testing {

/*******************************************************************************/

/*! \class TimerStamp oes/testing/core/ScopedTimer.h
 * \brief A point in time, recording both the real and user clocks, in seconds.
 *
 * Zero by default. Use TimerStamp::now() to get the current time point.
 * The zero time-point is considered to be \em false.
 *
 * \ingroup oes_testing_core
 */

TimerStamp TimerStamp::now() {
    TimerStamp zero;
    return zero.setNow();
}

TimerStamp& TimerStamp::reset() {
    user_time_sec_ = 0;
    real_time_sec_ = 0;
    return *this;
}

TimerStamp& TimerStamp::setNow() {
    user_time_sec_ = oes::core::System::getUserTime();
    real_time_sec_ = oes::core::System::getRealTime();
    return *this;
}

//*****************************************************************************

/*! \class TimerDuration oes/testing/core/ScopedTimer.h
 * \brief A duration of time, of both the real and user clocks, in seconds.
 *
 * Zero by default. The zero duration is considered to be \em false.
 * Can be used to accumulate durations of several timers.
 * \code
 * TimerDuration total;
 * for (const auto& entry : entries) {
 *     ScopedTimer timer;
 *     ...
 *     total += timer.duration();
 * }
 * std::cout << "Took " << total << std::endl;
 * \endcode
 *
 * \ingroup oes_testing_core
 */

TimerDuration::TimerDuration(
    const TimerStamp& start, const TimerStamp& stop
) : real_time_sec_(stop.real_time_sec_ - start.real_time_sec_)
  , user_time_sec_(stop.user_time_sec_ - start.user_time_sec_)
{
}

std::string TimerDuration::print() const {
    return fmt::format("{:.3f}s (user: {:.3f}s)", real_time_sec_, user_time_sec_);
}

/*******************************************************************************/

/*! \class ScopedTimer oes/testing/core/ScopedTimer.h
 * \brief A RAAI timer helper.
 *
 * Prints the provided message, appended with the timer's duration when it goes
 * out-of-scope, in a format similar to " in 1.234s (user: 0.987s)" showing both
 * the real and user durations, in seconds.
 *
 * Prints to either std::cout, or via OES_INFO, which is then routed or not to
 * the console, depending on the -log_level command line argument.
 *
 * \note The \em user duration can exceed the \em real (or wall clock) duration
 *     in case of parallelism.
 *
 * \ingroup oes_testing_core
 */

ScopedTimer::ScopedTimer(
    std::string msg, bool print_to_stdout
) : start_(TimerStamp::now())
  , msg_(std::move(msg))
  , print_to_stdout_(print_to_stdout)
  , level_(oes::core::Message::Severity::info)
{}

ScopedTimer::ScopedTimer(
    std::string msg, oes::core::Message::Severity level
) : start_(TimerStamp::now())
  , msg_(std::move(msg))
  , print_to_stdout_(false)
  , level_(level)
{}

ScopedTimer::~ScopedTimer() {
    if (start_) {
        log();
    }
}

ScopedTimer& ScopedTimer::reset() {
    start_.reset();
    return *this;
}

ScopedTimer& ScopedTimer::restart() {
    start_.setNow();
    return *this;
}

ScopedTimer& ScopedTimer::restart(const std::string& msg) {
    msg_ = msg;
    return restart();
}

//! \return elapsed real time in seconds (i.e. wall clock)
double ScopedTimer::realElapsed() const {
    return oes::core::System::getRealTime() - start_.real_time_sec_;
}

//! \return elapsed user time in seconds (i.e. user-space CPU time)
double ScopedTimer::userElapsed() const {
    return oes::core::System::getUserTime() - start_.user_time_sec_;
}

TimerDuration ScopedTimer::duration() const {
    return TimerDuration(start_, TimerStamp::now());
}

std::string ScopedTimer::print() const {
    const TimerDuration delta(start_);
    return fmt::format("{} in {} {}", msg_, delta.print(), oes::core::System::getMemoryUsage().peakMB());
}

void ScopedTimer::log() const {
    const std::string msg = print();
    if (print_to_stdout_) {
        std::cout << msg << std::endl;
    } else {
        switch (level_) {
        case oes::core::Message::Severity::debug:
            OES_DEBUG(msg);
            break;
        case oes::core::Message::Severity::verbose:
            OES_VERBOSE(msg);
            break; 
        case oes::core::Message::Severity::info:
            OES_INFO(msg);
            break;
        case oes::core::Message::Severity::warning:
            OES_WARNING(msg);
            break;
        case oes::core::Message::Severity::error:
            OES_ERROR(msg);
            break;
        case oes::core::Message::Severity::alert:
            OES_ALERT(msg);
            break;
        case oes::core::Message::Severity::critical:
            OES_CRITICAL(msg);
            break;
        case oes::core::Message::Severity::fatal:
            OES_FATAL(msg);
            break;
        case oes::core::Message::Severity::status:
            OES_STATUS(msg);
            break;
        case oes::core::Message::Severity::notice:
            OES_NOTICE(msg);
            break;
        default:
            OES_INFO(msg);
            break;
        }
    }
}

/*******************************************************************************/

} // testing
} // oes
