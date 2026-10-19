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
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/client/Client.hpp>
#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>
#include "BenchmarkTestData.h"

// ============================================================================
// Set of tests for simulating high-load client-server interaction.
// ============================================================================

using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml::etp12;
using namespace oes::client;
using namespace oes::testing::eml;

namespace
{
    // ============================================================================
    // Configuration
    // ============================================================================
    constexpr size_t botClientsAmount = 200;        // Number of bot clients that will establish a connection with the ETP server and simulate active work
    constexpr uint32_t serverThreadCount = 8;
    constexpr int serverMaxConnections = -1;

    // ============================================================================
    // Common variables
    // ============================================================================
    std::shared_ptr<OpenKVClientServerFixture> g_clientServerFixture;
    std::shared_ptr<OpenKVServerFixture> g_openKVServerFixture;
    std::vector<Client_uptr> g_clients;
    std::shared_ptr<Datatypes::DataArrayTypes::PutDataArraysType> g_grid2d;
}


// ============================================================================
// Benchmarks
// ============================================================================

/**
 * \brief Test simulating high load reading data arrays by a large number of clients.
 */
void ClientHighLoad_ArrayGet(State& state)
{
    // Configure server only once for all threads
    if (state.thread_index() == 0)
    {
        // Prepare test data
        g_openKVServerFixture = std::make_shared<OpenKVServerFixture>();
        auto space = g_openKVServerFixture->putSpace(benchmarkTestDataSpace);
        g_grid2d = std::make_shared<Datatypes::DataArrayTypes::PutDataArraysType>(g_openKVServerFixture->putGrid2dAry(space));

        g_clientServerFixture = std::make_shared<OpenKVClientServerFixture>();
        g_clientServerFixture->startServer(serverThreadCount, serverMaxConnections);
        g_clients.resize(state.threads());

        for (auto& client : g_clients)
        {
            client = g_clientServerFixture->connectClient();
            if (!client) {
                return state.SkipWithError("Can't connect to server");
            }
        }
    }

    // Benchmark loop
    for (auto _ : state)
    {
        const auto& client = g_clients[state.thread_index()];

        // Prepare RPC (do not count)
        state.PauseTiming();
        auto getArrayRpc = rpcFor(client->array_proto(), &Protocol::IDataArray::GetDataArrays);
        static constexpr char testPartId[] = "FullArray";
        getArrayRpc.req_.m_dataArrays.emplace(testPartId, g_grid2d->m_uid);
        std::ostringstream oss;
        state.ResumeTiming();

        // Tested API call
        if (!call_succeeded(getArrayRpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to get array: " + oss.str()).c_str());
            break;
        }

        // Perform a simple response check (do not count)
        state.PauseTiming();
        auto it = getArrayRpc.res_.m_dataArrays.find(testPartId);
        if (it == getArrayRpc.res_.m_dataArrays.end()) {
            state.SkipWithError(fmt::format("Missed requested part in the response: {}", testPartId).c_str());
            break;
        }
        if (it->second.m_dimensions != g_grid2d->m_array.m_dimensions) {
            state.SkipWithError("Mismatch dimensions of array");
            break;
        }
        if (!std::holds_alternative<Datatypes::ArrayOfDouble>(it->second.m_data.m_item)) {
            state.SkipWithError("Mismatch array type");
            break;
        }
        const auto& receivedArray = std::get<Datatypes::ArrayOfDouble>(it->second.m_data.m_item);
        const auto& sourceArray = std::get<Datatypes::ArrayOfDouble>(g_grid2d->m_array.m_data.m_item);
        if (receivedArray.m_values.size() != sourceArray.m_values.size()) {
            state.SkipWithError("Mismatch array size");
            break;
        }
        if (receivedArray.m_values != sourceArray.m_values) {
            state.SkipWithError("Corrupted data in the received array");
            break;
        }

        // Cleanup
        getArrayRpc.res_.m_dataArrays.clear();
        state.ResumeTiming();
    }

    // Teardown once for all threads
    if (state.thread_index() == 0)
    {
        g_clients.clear();
        g_clientServerFixture.reset();
        g_openKVServerFixture.reset();
    }
}
BENCHMARK(ClientHighLoad_ArrayGet)->Threads(botClientsAmount)->Unit(kMillisecond)->MeasureProcessCPUTime()->UseRealTime();
