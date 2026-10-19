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
#include <oes/common/etp12/EmlUri.h>
#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml::etp12;
using namespace oes::client;
using namespace oes::testing::eml;
using namespace oes::common::etp12;

void Client_SpacePut(State& state)
{
    // Configure server/client
    OpenKVClientServerFixture clientServerFixture;
    clientServerFixture.startServer();
    const auto client = clientServerFixture.connectClient();
    if (!client) {
        return state.SkipWithError("Unable to connect to server");
    }

    // Prepare RPCs
    auto createSpaceRpc = rpcFor(client->space_proto(), &Protocol::IDataspace::PutDataspaces);
    Datatypes::Object::Dataspace space;
    space.m_path = benchmarkTestDataSpace;
    createSpaceRpc.req_.m_dataspaces.emplace("1", space);
    auto deleteSpaceRpc = rpcFor(client->space_proto(), &Protocol::IDataspace::DeleteDataspaces);
    deleteSpaceRpc.req_.m_uris.emplace("1", EmlUri::make_space(benchmarkTestDataSpace).to_string());
    std::ostringstream oss;

    // Benchmark loop
    for (auto _ : state)
    {
        if (!call_succeeded(createSpaceRpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to create data space: " + oss.str()).c_str());
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
BENCHMARK(Client_SpacePut)->Unit(kMillisecond);


void Client_SpaceGet(State& state)
{
    // Configure server/client
    OpenKVClientServerFixture clientServerFixture;
    clientServerFixture.startServer();
    OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.putSpace(benchmarkTestDataSpace);


    const auto client = clientServerFixture.connectClient();
    if (!client) {
        return state.SkipWithError("Unable to connect to server");
    }

    // Prepare RPCs
    auto getSpaceRpc = rpcFor(client->space_proto(), &Protocol::IDataspace::GetDataspaces);
    std::ostringstream oss;

    // Benchmark loop
    for (auto _ : state)
    {
        if (!call_succeeded(getSpaceRpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to get data space: " + oss.str()).c_str());
            break;
        }
    }
}
BENCHMARK(Client_SpaceGet)->Unit(kMillisecond);
