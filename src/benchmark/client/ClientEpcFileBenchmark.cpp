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

#include <filesystem>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/client/Client.hpp>
#include <oes/client/ImportEpc.h>
#include <oes/client/ExportEpc.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>
#include <oes/testing/core/TestMain.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml::etp12;
using namespace oes::client;
using namespace oes::testing::eml;
using namespace oes::common::etp12;

// ============================================================================
// Test data
// ============================================================================
namespace 
{
    constexpr char testEpcFile[] = "PillarGrid.epc";
    constexpr char testH5File[] = "PillarGrid.h5";
    constexpr char testOutputEpcFile[] = "TestExport.epc";
    constexpr char testOutputH5File[] = "TestExport.h5";
}

// ============================================================================
// Benchmarks
// ============================================================================
void Client_EpcImport(State& state)
{
    // Check prerequisites
    const std::string epcInput = oes::testing::resolveTestDataFile("data/eml", testEpcFile, true);
    const std::string hdfInput = oes::testing::resolveTestDataFile("data/eml", testH5File, true);
    if (epcInput.empty() || !std::filesystem::exists(epcInput) || hdfInput.empty() || !std::filesystem::exists(hdfInput)) {
        return state.SkipWithError("Test EPC file does not exist");
    }

    // Configure server/client
    OpenKVClientServerFixture clientServerFixture;
    clientServerFixture.startServer();
    const auto client = clientServerFixture.connectClient();
    if (!client) {
        return state.SkipWithError("Unable to connect to server");
    }

    // Prepare RPCs
    auto createSpaceRpc = rpcFor(client->space_proto(), &Protocol::IDataspace::PutDataspaces);
    data12::Object::Dataspace space;
    space.m_path = benchmarkTestDataSpace;
    createSpaceRpc.req_.m_dataspaces.emplace("1", space);
    auto deleteSpaceRpc = rpcFor(client->space_proto(), &Protocol::IDataspace::DeleteDataspaces);
    deleteSpaceRpc.req_.m_uris.emplace("1", EmlUri::make_space(benchmarkTestDataSpace).to_string());
    std::ostringstream oss;

    // Benchmark loop
    for (auto _ : state)
    {
        // Create data space (do not count)
        state.PauseTiming();
        if (!call_succeeded(createSpaceRpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to create data space: " + oss.str()).c_str());
            break;
        }
        state.ResumeTiming();

        // Import
        ImportEpcOptions options;
        options.timeout_ = timeOut;
        const bool result = importEpc(client->getConfig(), benchmarkTestDataSpace, epcInput, options);
        if (!result) {
            state.SkipWithError("Unable to import data");
            break;
        }

        // Clean up the data space (do not count)
        state.PauseTiming();
        if (!call_succeeded(deleteSpaceRpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to delete data space: " + oss.str()).c_str());
            break;
        }
        state.ResumeTiming();
    }
}
BENCHMARK(Client_EpcImport)->Iterations(50)->Unit(kMillisecond);


void Client_EpcExport(State& state)
{
    // Check prerequisites
    const std::string epcInput = oes::testing::resolveTestDataFile("data/eml", testEpcFile, true);
    const std::string hdfInput = oes::testing::resolveTestDataFile("data/eml", testH5File, true);
    if (epcInput.empty() || !std::filesystem::exists(epcInput) || hdfInput.empty() || !std::filesystem::exists(hdfInput)) {
        return state.SkipWithError("Test EPC file does not exist");
    }
    const std::string epcOutput = oes::testing::resolveTestDataFile(benchmarkWorkDir, testOutputEpcFile, false);
    const std::string hdfOutput = oes::testing::resolveTestDataFile(benchmarkWorkDir, testOutputH5File, false);

    // Configure server/client
    OpenKVClientServerFixture clientServerFixture;
    clientServerFixture.startServer();
    const auto client = clientServerFixture.connectClient();
    if (!client) {
        return state.SkipWithError("Unable to connect to server");
    }

    // Prepare test data
    if (!clientServerFixture.directImport(epcInput, benchmarkTestDataSpace)) {
        return state.SkipWithError(fmt::format("Unable to import test data (try to manually delete data space '{}')", benchmarkTestDataSpace).c_str());
    }
    const auto workDir = std::filesystem::path(epcOutput).parent_path();
    if (!exists(workDir))
    {
        if (!create_directories(workDir)) {
            return state.SkipWithError(("Unable to create directory: " + workDir.generic_string()).c_str());
        }
    }

    ExportEpcOptions exportEpcOptions;
    exportEpcOptions.timeout_ = std::chrono::milliseconds(timeOut);
    exportEpcOptions.overwrite_ = true;

    // Benchmark loop
    for (auto _ : state)
    {
        // Export
        const bool result = exportEpc(*client, benchmarkTestDataSpace, epcOutput, hdfOutput, exportEpcOptions);
        if (!result) {
            state.SkipWithError("Unable to export data (false result)");
            break;
        }
        if (!std::filesystem::exists(epcOutput) || !std::filesystem::exists(hdfOutput)) {
            state.SkipWithError("Unable to export data (no files)");
            break;
        }
    }
}
BENCHMARK(Client_EpcExport)->Iterations(50)->Unit(kMillisecond);


void Client_EpcExportMt(State& state)
{
    // Check prerequisites
    const std::string epcInput = oes::testing::resolveTestDataFile("data/eml", testEpcFile, true);
    if (epcInput.empty() || !std::filesystem::exists(epcInput)) {
        return state.SkipWithError("Test EPC file does not exist");
    }
    const std::string epcOutput = oes::testing::resolveTestDataFile(benchmarkWorkDir, testOutputEpcFile, false);
    const std::string hdfOutput = oes::testing::resolveTestDataFile(benchmarkWorkDir, testOutputH5File, false);

    // Configure server/client
    OpenKVClientServerFixture clientServerFixture;
    clientServerFixture.startServer();
    const auto client = clientServerFixture.connectClient();
    if (!client) {
        return state.SkipWithError("Unable to connect to server");
    }

    // Prepare test data
    if (!clientServerFixture.directImport(epcInput, benchmarkTestDataSpace)) {
        return state.SkipWithError(fmt::format("Unable to import test data (try to manually delete data space '{}')", benchmarkTestDataSpace).c_str());
    }
    const auto workDir = std::filesystem::path(epcOutput).parent_path();
    if (!exists(workDir))
    {
        if (!create_directories(workDir)) {
            return state.SkipWithError(("Unable to create directory: " + workDir.generic_string()).c_str());
        }
    }

    ExportEpcOptions exportEpcOptions;
    exportEpcOptions.timeout_ = std::chrono::milliseconds(timeOut);
    exportEpcOptions.overwrite_ = true;
    exportEpcOptions.thread_count_ = static_cast<unsigned>(state.range(0));

    // Benchmark loop
    for (auto _ : state)
    {
        // Export
        const bool result = exportEpcMt(*client, benchmarkTestDataSpace, epcOutput, hdfOutput, exportEpcOptions);
        if (!result) {
            state.SkipWithError("Unable to export data (false result)");
            break;
        }
        if (!std::filesystem::exists(epcOutput) || !std::filesystem::exists(hdfOutput)) {
            state.SkipWithError("Unable to export data (no files)");
            break;
        }
    }
}
BENCHMARK(Client_EpcExportMt)->Unit(kMillisecond)->DenseRange(1, 4, 1)->Iterations(50)->ArgName("threads");
