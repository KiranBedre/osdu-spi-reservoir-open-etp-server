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
#pragma once
#include <chrono>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <oes/eml/etp12/MsgUtils.h>
#include <oes/testing/eml/TestingMocks.h>

namespace oes::benchmark
{
    // ============================================================================
    // Common constants
    // ============================================================================
    static constexpr char benchmarkTestDataSpace[] = "benchmark/test_space";
    static constexpr char benchmarkWorkDir[] = "benchmark";
#ifdef OES_DEBUG_BUILD
    constexpr std::chrono::milliseconds timeOut(100'000);
#else
    constexpr std::chrono::milliseconds timeOut(3000);
#endif

    // ============================================================================
    // Test data builders
    // ============================================================================
    eml::etp12::XDataMap buildTestXDataMap();
    std::vector<uint8_t> buildTestAvroBinaryXData();

    template <typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
    void buildTestValue(T& value) {
        value = static_cast<T>(std::rand());
    }
    inline void buildTestValue(int64_t& value) {
        value = (static_cast<int64_t>(std::rand()) << std::numeric_limits<unsigned>::digits) + std::rand();
    }
    inline void buildTestValue(uint64_t& value) {
        value = (static_cast<uint64_t>(std::rand()) << std::numeric_limits<unsigned>::digits) + std::rand();
    }
    inline void buildTestValue(bool& value) {
        value = static_cast<bool>(std::rand() % 2);
    }
    inline void buildTestValue(float& value) {
        value = std::rand() / 10.0f;
    }
    inline void buildTestValue(double& value) {
        value = std::rand() / 1000.0;
    }
    inline void buildTestValue(std::string& value) {
        value = "String_" + std::to_string(std::rand());
    }
    inline void buildTestValue(core::Guid& value) {
        value = core::Guid::make();
    }

    template <typename T, std::enable_if_t<!std::is_array_v<T>, int> = 0>
    T buildTestValue()
    {
        T val;
        buildTestValue(val);
        return val;
    }
}
