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

#include <cmath>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <benchmark/benchmark.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/common/CompressUtils.hpp>
#include <oes/testing/eml/TestingMocks.h>


using namespace benchmark;

template<typename ...TArgs>
void Eml_CompressorDeflate(State& state, TArgs&&... args)
{
    // Configure compressor
    auto argsTuple = std::make_tuple(std::move(args)...);
    auto baseCompressor = std::get<0>(argsTuple);
    const auto compressionLevel = static_cast<oes_eml_ztype_e>(state.range(0));
    const oes::common::ZCompressor compressor(baseCompressor.with(compressionLevel));

    // Prepare test data
    const char* testData = std::get<1>(argsTuple);
    const size_t testDataSize = std::get<2>(argsTuple);

    // Configure "Rate" counter (constant for every iteration)
    state.counters["Rate"] = Counter(static_cast<double>(testDataSize), Counter::kIsRate | Counter::kIsIterationInvariant, Counter::kIs1024);

    // Benchmark loop
    size_t compressedSize = testDataSize;
    for (auto _ : state)
    {
        std::string result;
        if (!compressor.deflate(testData, result)) {
            state.SkipWithError("Failed to compress test data");
            break;
        }
        compressedSize = result.size();
    }

    state.counters["CompressRatio"] = std::round(static_cast<double>(compressedSize) / static_cast<double>(testDataSize) * 100 * 10) / 10;
}

// Generates set of tests for ZLib compressor with different compression levels
BENCHMARK_CAPTURE(Eml_CompressorDeflate, zlib, oes::common::ZLibCompressor,
    oes::testing::eml::grid2d_mock().xml_.c_str(), oes::testing::eml::grid2d_mock().xml_.size())
    ->Unit(kMicrosecond)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_FAST)->ArgName("level")
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_MID)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_SMALL);

// Generates set of tests for GZip compressor with different compression levels
BENCHMARK_CAPTURE(Eml_CompressorDeflate, gzip, oes::common::GZipCompressor,
    oes::testing::eml::grid2d_mock().xml_.c_str(), oes::testing::eml::grid2d_mock().xml_.size())
    ->Unit(kMicrosecond)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_FAST)->ArgName("level")
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_MID)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_SMALL);


// ============================================================================

template<typename ...TArgs>
void Eml_CompressorInflate(State& state, TArgs&&... args)
{
    // Configure compressor
    auto argsTuple = std::make_tuple(std::move(args)...);
    auto baseCompressor = std::get<0>(argsTuple);
    const auto compressionLevel = static_cast<oes_eml_ztype_e>(state.range(0));
    const oes::common::ZCompressor compressor(baseCompressor.with(compressionLevel));

    // Prepare test data
    const char* testData = std::get<1>(argsTuple);
    const size_t testDataSize = std::get<2>(argsTuple);
    std::string testCompressedData;
    if (!compressor.deflate(testData, testCompressedData)) {
        state.SkipWithError("Failed to prepare test compressed data");
        return;
    }

    // Configure "Rate" counter (constant for every iteration)
    state.counters["Rate"] = Counter(static_cast<double>(testDataSize),
        Counter::kIsRate | Counter::kIsIterationInvariant, Counter::kIs1024);

    // Benchmark loop
    for (auto _ : state)
    {
        std::string decompressedStr;
        if (!compressor.inflate(testCompressedData, decompressedStr, testDataSize)) {
            state.SkipWithError("Failed to decompress test data");
            break;
        }
    }
}

// Generates set of tests for ZLib compressor with different compression levels
BENCHMARK_CAPTURE(Eml_CompressorInflate, zlib, oes::common::ZLibCompressor,
    oes::testing::eml::grid2d_mock().xml_.c_str(), oes::testing::eml::grid2d_mock().xml_.size())
    ->Unit(kMicrosecond)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_FAST)->ArgName("level")
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_MID)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_SMALL);

// Generates set of tests for GZip compressor with different compression levels
BENCHMARK_CAPTURE(Eml_CompressorInflate, gzip, oes::common::GZipCompressor,
    oes::testing::eml::grid2d_mock().xml_.c_str(), oes::testing::eml::grid2d_mock().xml_.size())
    ->Unit(kMicrosecond)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_FAST)->ArgName("level")
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_MID)
    ->Arg(oes_eml_ztype_e::eOES_EML_Z_SMALL);
