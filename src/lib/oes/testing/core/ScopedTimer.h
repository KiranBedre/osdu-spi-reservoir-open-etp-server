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

#ifndef OES_TESTING_CORE_SCOPED_TIMER_H
#define OES_TESTING_CORE_SCOPED_TIMER_H

#include <oes/testing/core/export.h>

#include <oes/core/utils/Logger.h>

#include <string>
#include <iosfwd>

namespace oes {
namespace testing {

//*****************************************************************************

struct OES_TESTING_CORE_EXPORT TimerStamp {
    double real_time_sec_ = 0;
    double user_time_sec_ = 0;

    static TimerStamp now();

    TimerStamp& reset();
    TimerStamp& setNow();

    explicit operator bool() const {
        return real_time_sec_ > 0;
    }
};

inline bool operator==(const TimerStamp& lhs, const TimerStamp& rhs) {
    return
        lhs.real_time_sec_ == rhs.real_time_sec_ &&
        lhs.user_time_sec_ == rhs.user_time_sec_
    ;
}

inline bool operator!=(const TimerStamp& lhs, const TimerStamp& rhs) {
    return !(lhs == rhs);
}

inline bool operator<(const TimerStamp& lhs, const TimerStamp& rhs) {
    if (lhs.real_time_sec_ == rhs.real_time_sec_) {
        if (lhs.user_time_sec_ == rhs.user_time_sec_) {
            return false;
        }
        return lhs.user_time_sec_ < rhs.user_time_sec_;
    }
    return lhs.real_time_sec_ < rhs.real_time_sec_;
}

//*****************************************************************************

struct OES_TESTING_CORE_EXPORT TimerDuration {
    double real_time_sec_ = 0;
    double user_time_sec_ = 0;

    TimerDuration() = default;
    explicit TimerDuration(const TimerStamp& start, const TimerStamp& stop = TimerStamp::now());

    TimerDuration& operator+=(const TimerDuration& rhs) {
        real_time_sec_ += rhs.real_time_sec_;
        user_time_sec_ += rhs.user_time_sec_;
        return *this;
    }
    TimerDuration& operator-=(const TimerDuration& rhs) {
        real_time_sec_ -= rhs.real_time_sec_;
        user_time_sec_ -= rhs.user_time_sec_;
        return *this;
    }

    explicit operator bool() const {
        return real_time_sec_ > 0;
    }

    std::string print() const;
};

//*****************************************************************************

inline std::ostream& operator<<(std::ostream& os, const TimerDuration& duration) {
    return os << duration.print();
}

//*****************************************************************************

class OES_TESTING_CORE_EXPORT ScopedTimer {
    TimerStamp start_;
    std::string msg_;
    const bool print_to_stdout_;
    oes::core::Message::Severity level_;

    ScopedTimer& operator=(const ScopedTimer&) = delete;

public:
    explicit ScopedTimer(std::string msg, bool print_to_stdout = false);
    ScopedTimer(std::string msg, oes::core::Message::Severity level);
    ~ScopedTimer();

    TimerStamp startedAt() const {
        return start_;
    }
    ScopedTimer& reset();
    ScopedTimer& restart();
    ScopedTimer& restart(const std::string& msg);

    // elapsed times in seconds
    double realElapsed() const;
    double userElapsed() const;
    double elapsed() const { return realElapsed(); }
    TimerDuration duration() const;

    // backward-compatibility
    double real_elapsed() const { return realElapsed(); }
    double user_elapsed() const { return userElapsed(); }

    std::string print() const;
    void log() const;
};

inline std::ostream& operator<<(std::ostream& os, const ScopedTimer& timer) {
    return os << timer.print();
}

//*****************************************************************************

} // testing
} // oes

#endif
