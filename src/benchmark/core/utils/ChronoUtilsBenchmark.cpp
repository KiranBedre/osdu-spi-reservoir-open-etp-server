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
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/ChronoUtils.h>


/**
 * \brief Benchmark for oes::core::usecSinceEpochFromExtendedIsoDateTime().
 * \details Uses for parse Creation/LastUpdate datetime from EPC files.
 */
static void Core_ChronoParseExtendedIsoDateTime(benchmark::State& state)
{
    const std::string testIsoDateTime = "2016-06-21T15:24:38Z";
    std::chrono::microseconds epochDateTime;

    for (auto _ : state)
    {
        if (!oes::core::usecSinceEpochFromExtendedIsoDateTime(testIsoDateTime, epochDateTime))
        {
            state.SkipWithError("Failed to parse extended ISO datetime");
            break;
        }
    }
}
BENCHMARK(Core_ChronoParseExtendedIsoDateTime);
