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

#ifndef OES_CORE_UTILS_CHRONOUTILS_H
#define OES_CORE_UTILS_CHRONOUTILS_H

#include <oes/core/utils/export.h>

#include <chrono>
#include <string>

namespace oes::core {

//============================================================================

struct OES_CORE_UTILS_EXPORT HighResTimer {
    using clock_t = std::chrono::high_resolution_clock;
    clock_t::time_point start_ = clock_t::now();

    void restart();
    double elapsedSec() const;
    std::string elapsed() const;
};

//============================================================================

OES_CORE_UTILS_EXPORT
std::chrono::milliseconds parseSimpleDuration(std::string s);

OES_CORE_UTILS_EXPORT
std::pair<
    std::chrono::milliseconds,
    std::chrono::milliseconds
> parseCsvSimpleDurationPair(const std::string& s);

//============================================================================

OES_CORE_UTILS_EXPORT
const std::string& isoDatetimeFormat();

OES_CORE_UTILS_EXPORT
const std::string& ascDatetimeFormat();

OES_CORE_UTILS_EXPORT
bool parseDateTime(
    const std::string& format,
    const std::string& s,
    std::chrono::system_clock::time_point& tp
);

OES_CORE_UTILS_EXPORT
bool parseExtendedIsoDateTime(
     const std::string& s,
     std::chrono::system_clock::time_point& tp
);

OES_CORE_UTILS_EXPORT
bool usecSinceEpochFromExtendedIsoDateTime(
    const std::string& s,
    std::chrono::microseconds& usec
);

OES_CORE_UTILS_EXPORT
std::string formatDateTime(
    const std::string& format,
    const std::chrono::system_clock::time_point& tp
);

OES_CORE_UTILS_EXPORT
std::string formatIsoDateTime(
    const std::chrono::system_clock::time_point& tp
);

enum class FracSecRes { // Fractional-second Resolution
    eWholeSecFloor, eWholeSecRound, eWholeSecCeil,
    eMilliSecFloor, eMilliSecRound, eMilliSecCeil,
    eMicroSecFloor, eMicroSecRound, eMicroSecCeil,
};
OES_CORE_UTILS_EXPORT
std::string formatDateTime(
    const std::string& format,
    const std::chrono::system_clock::time_point& tp,
    FracSecRes resolution
);
OES_CORE_UTILS_EXPORT
std::string formatExtendedIsoDateTime(
    const std::chrono::system_clock::time_point& tp,
    FracSecRes resolution
);

OES_CORE_UTILS_EXPORT
std::string formatExtendedIsoDateTime(
    const std::chrono::system_clock::time_point& tp,
    int min_fractional_seconds = 0,
    int max_fractional_seconds = 6 // micro-seconds
);

//============================================================================

OES_CORE_UTILS_EXPORT
std::chrono::seconds tzOffset();

OES_CORE_UTILS_EXPORT
std::chrono::seconds cachedTzOffset();

inline
std::chrono::system_clock::time_point localToUtc(std::chrono::system_clock::time_point local) {
    return local - cachedTzOffset();
}

inline
std::chrono::system_clock::time_point utcToLocal(std::chrono::system_clock::time_point utc) {
    return utc + cachedTzOffset();
}

//============================================================================

OES_CORE_UTILS_EXPORT
std::chrono::milliseconds getPartialTimeoutValue(const size_t max_msg_size);

//============================================================================

} // namespace oes::core

#endif
