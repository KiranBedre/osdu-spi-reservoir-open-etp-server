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
#include "BenchmarkTestData.h"

namespace oes::benchmark
{
    eml::etp12::XDataMap buildTestXDataMap()
    {
        eml::etp12::XDataMap xmap;
        eml::etp12::XDataMapBuilder xmb{ xmap };

        xmb.emplace("B", std::vector<uint8_t>{ 0x00, 0xFF, 0x12, 0x90, 0xCA, 0xFE, 0xBA, 0xBE, 0x0D, 0x0C, 0x20 });
        xmb.emplace("b", std::vector<bool>{ true, false, false, false, true, true, true, false, true, true, false });
        xmb.emplace("i", std::vector<int>{ -5000, -1000, -500, -100, -1, 0, 1, 100, 500, 1000, 5000 });
        xmb.emplace("l", std::vector<int64_t>{ -5000, -1000, -500, -100, -1, 0, 1, 100, 500, 1000, 5000 });
        xmb.emplace("f", std::vector<float>{ -1000.1f - 500.5f, -100.1f, -3.14159f, -1.0f, 0.0f, 1.0f, 3.14159f, 100.1f, 500.5f, 1000.1f });
        xmb.emplace("d", std::vector<double>{ -1000.1, -500.5, -100.1, -3.14159, -1.0, 0.0, 1.0, 3.14159, 100.1, 500.5, 1000.1 });
        xmb.emplace("s", std::vector<std::string>{
            "The quick brown fox jumps over the lazy dog.",
            "A quivering Texas zombie fought republic linked jewelry.",
            "Fox dwarves chop my talking quiz job."
        });

        return xmap;
    }

    std::vector<uint8_t> buildTestAvroBinaryXData()
    {
        return eml::etp12::xdataToAvroBinary(buildTestXDataMap());
    }
}
