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

#include <oes/common/etp12/EmlUri.h>


void Etp_EmlUriParse(benchmark::State& state)
{
    const auto emlUriStr = oes::common::etp12::EmlUri::make_obj("foo/bar", "feat",
        oes::core::Guid("2a114208-a4b7-4c02-bb4d-3e15c68d1674"), oes::common::etp12::EmlUri::eRESQML20, "1")
        .to_string();

    for (auto _ : state) {
        oes::common::etp12::EmlUri emlUri = oes::common::etp12::EmlUri::parse(emlUriStr);
        if (!emlUri.isValid()) {
            state.SkipWithError("Unable to parse EmlUri");
            break;
        }
        benchmark::ClobberMemory();
    }
}
BENCHMARK(Etp_EmlUriParse);


void Etp_EmlUriToString(benchmark::State& state)
{
    const auto emlUri = oes::common::etp12::EmlUri::make_obj("foo/bar", "feat",
        oes::core::Guid("2a114208-a4b7-4c02-bb4d-3e15c68d1674"), oes::common::etp12::EmlUri::eRESQML20, "1");

    for (auto _ : state) {
        benchmark::DoNotOptimize(emlUri.to_string());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(Etp_EmlUriToString); 
