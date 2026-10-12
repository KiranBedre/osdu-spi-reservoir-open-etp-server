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

#include <oes/core/utils/Guid.h>


static constexpr char testGuid[] = "67441a8a-f2b0-4ff8-acf5-619547863a1e";

static void Core_GuidParse(benchmark::State& state)
{
    for (auto _ : state) {
        benchmark::DoNotOptimize(oes::core::Guid::parse(testGuid));
    }
}
BENCHMARK(Core_GuidParse);

static void Core_GuidRender(benchmark::State& state)
{
    const oes::core::Guid guid(testGuid);
    for (auto _ : state) {
        benchmark::DoNotOptimize(guid.toXmlString());
    }
}
BENCHMARK(Core_GuidRender);
