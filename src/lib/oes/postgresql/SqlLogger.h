// ============================================================================
// Copyright 2024 Microsoft Corporation. All rights reserved.
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

#ifndef oes_postgresql_sqllogger_h
#define oes_postgresql_sqllogger_h

#include <chrono>

#include <oes/core/utils/Logger.h>

namespace oes {
namespace postgresql {

// RAII helper for consistent SQL logging across all execute methods.
// Logs errors and slow-query warnings — but never the SQL query text
// itself, to prevent potential PII exposure in logs.
//
// Usage:
//   SqlLogger log("Execute");
//   // ... run query ...
//   if (error) log.logError(errorMsg);  // sets hadError flag
//   // destructor auto-emits slow-query WARNING on success path

struct SqlLogger {
    static constexpr long long SLOW_QUERY_THRESHOLD_MS = 1000;

    const char* method;
    std::chrono::steady_clock::time_point startTime;
    bool hadError = false;

    explicit SqlLogger(const char* methodName)
        : method(methodName)
        , startTime(std::chrono::steady_clock::now())
    {
    }

    ~SqlLogger() noexcept {
        try {
            if (!hadError) {
                auto ms = elapsedMs();
                if (ms > SLOW_QUERY_THRESHOLD_MS) {
                    OES_WARNING("SQL {} Slow: duration={}ms", method, ms);
                }
            }
        } catch (...) {
            // Suppress exceptions to guarantee noexcept during stack unwinding
        }
    }

    long long elapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    }

    void logError(const char* errorMsg) {
        hadError = true;
        OES_ERROR("SQL {} Failed: duration={}ms, error={}",
                  method, elapsedMs(), errorMsg);
    }
};

} // namespace postgresql
} // namespace oes

#endif // oes_postgresql_sqllogger_h
