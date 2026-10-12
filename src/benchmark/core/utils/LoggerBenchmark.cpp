// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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
#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>

// ============================================================================
// Benchmarks
// ============================================================================
static void Core_LogSimpleString(benchmark::State& state)
{
    oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::info);

    for (auto _ : state) {
        OES_INFO("Test simple string without formatting");
    }
}
BENCHMARK(Core_LogSimpleString);


static void Core_LogWithFormatting(benchmark::State& state)
{
    oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::info);

    for (auto _ : state) {
        OES_INFO("Test formatting string: {}, date: {:%F %T}, int: {}, float: {}, bool: {}",
            "hello", std::chrono::system_clock::now(), 100, 3.1415f, true);
    }
}
BENCHMARK(Core_LogWithFormatting);


static void Core_LogWithSkipBySeverityFilter(benchmark::State& state)
{
    oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::info);

    for (auto _ : state) {
        OES_DEBUG("Test formatting string: {}, date: {:%F %T}, int: {}, float: {}, bool: {}",
            "hello", std::chrono::system_clock::now(), 100, 3.1415f, true);
    }
}
BENCHMARK(Core_LogWithSkipBySeverityFilter);
