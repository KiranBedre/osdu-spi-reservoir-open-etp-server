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

#include <oes/eml/etp12/MsgUtils.h>
#include "BenchmarkTestData.h"


using namespace oes::benchmark;

void Etp_AvroToXDataBinary(benchmark::State& state)
{
    const auto xDataMap = buildTestXDataMap();

    for (auto _ : state)
    {
        benchmark::DoNotOptimize(oes::eml::etp12::xdataToAvroBinary(xDataMap));
        benchmark::ClobberMemory();
    }
}
BENCHMARK(Etp_AvroToXDataBinary);


void Etp_AvroFromXDataBinary(benchmark::State& state)
{
    const std::vector<uint8_t> avroBin = buildTestAvroBinaryXData();

    for (auto _ : state)
    {
        oes::eml::etp12::XDataMap out;
        oes::eml::etp12::xdataFromAvroBinary(out, avroBin);
        benchmark::DoNotOptimize(true);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(Etp_AvroFromXDataBinary);
