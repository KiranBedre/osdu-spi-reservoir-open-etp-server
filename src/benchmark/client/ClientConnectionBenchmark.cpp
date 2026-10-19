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

#include <oes/client/Client.hpp>
#include <oes/testing/eml/OpenKVClientServerFixture.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml::etp12;
using namespace oes::client;
using namespace oes::testing::eml;

// ============================================================================
// Common variables
// ============================================================================
namespace
{
    std::shared_ptr<OpenKVClientServerFixture> g_openKVServerFixture;
    std::vector<Client_uptr> g_clients;
    Client_uptr g_client;
}


// ============================================================================
// Benchmarks
// ============================================================================

/**
 * \brief Benchmark for testing connections from multiple clients.
  */
void Client_ConnectionEstablishMt(State& state)
{
    // Configure server only once for all threads
    if (state.thread_index() == 0)
    {
        g_openKVServerFixture = std::make_shared<OpenKVClientServerFixture>();
        g_openKVServerFixture->startServer();
        g_clients.resize(state.threads());
    }

    // Benchmark loop
    for (auto _ : state)
    {
        auto& client = g_clients[state.thread_index()] = g_openKVServerFixture->connectClient();
        if (!client) {
            state.SkipWithError("Unable to connect to server");
            break;
        }

        state.PauseTiming();
        client.reset();
        state.ResumeTiming();
    }

    // Teardown once for all threads
    if (state.thread_index() == 0)
    {
        g_clients.clear();
        g_openKVServerFixture.reset();
    }
}
BENCHMARK(Client_ConnectionEstablishMt)->ThreadRange(1, 16)->Unit(kMillisecond)->MeasureProcessCPUTime()->UseRealTime();


/**
 * \brief Benchmark for ping server using single client.
 * \details Good for measurement the performance and stability of client/server interactions without any DB calls and business logic.
 */
void Client_ConnectionPingMt(State& state)
{
    // Configure server only once for all threads
    if (state.thread_index() == 0)
    {
        g_openKVServerFixture = std::make_shared<OpenKVClientServerFixture>();
        g_openKVServerFixture->startServer();

        g_client = g_openKVServerFixture->connectClient();
        if (!g_client) {
            return state.SkipWithError("Unable to connect to server");
        }
    }

    // Benchmark loop
    for (auto _ : state)
    {
        // Prepare RPC (do not count)
        state.PauseTiming();
        auto rpc = oes::client::rpcFor(g_client->core_proto(), &oes::common::etp12::Protocol::ICore::Ping);
        std::ostringstream oss;
        state.ResumeTiming();

        if (!call_succeeded(rpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to ping server: " + oss.str()).c_str());
            break;
        }
    }

    // Teardown once for all threads
    if (state.thread_index() == 0)
    {
        g_client.reset();
        g_openKVServerFixture.reset();
    }
}
BENCHMARK(Client_ConnectionPingMt)->ThreadRange(1, 16)->Unit(kMillisecond)->MeasureProcessCPUTime()->UseRealTime();
