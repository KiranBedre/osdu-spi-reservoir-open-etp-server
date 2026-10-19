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

#include <oes/eml/openkv/SpaceProto.h>
#include <oes/testing/eml/ServerFixture.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml;
using namespace oes::eml::etp12;
using oes::common::etp12::EmlUri;

void Openkv_SpacePut(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);

    // Prepare RPCs
    auto createSpaceRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataspace::PutDataspaces);
    data12::Object::Dataspace space;
    space.m_path = benchmarkTestDataSpace;
    openKVServerFixture.spaces_.push_back(space.m_path);
    createSpaceRpc.req_.m_dataspaces.emplace("1", space);
    auto deleteSpaceRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataspace::DeleteDataspaces);
    deleteSpaceRpc.req_.m_uris.emplace("1", EmlUri::make_space(benchmarkTestDataSpace).to_string());

    // Benchmark loop
    for (auto _ : state)
    {
        if (!createSpaceRpc.call()) {
            state.SkipWithError("Unable to create data space");
            break;
        }

        // Clean up the data space (do not count)
        state.PauseTiming();
        if (!deleteSpaceRpc.call()) {
            state.SkipWithError("Unable to delete data space");
            break;
        }
        state.ResumeTiming();
    }
}
BENCHMARK(Openkv_SpacePut)->Unit(kMillisecond);


void Openkv_SpaceGet(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.putSpace(benchmarkTestDataSpace, openKVServerFixture.session_);

    // Prepare RPC
    auto getSpaceRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataspace::GetDataspaces);

    // Benchmark loop
    for (auto _ : state)
    {
        if (!getSpaceRpc.call()) {
            state.SkipWithError("Unable to get data spaces");
            break;
        }
    }
}
BENCHMARK(Openkv_SpaceGet)->Unit(kMillisecond);


void Openkv_SpaceDelete(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);

    // Prepare RPCs
    auto createSpaceRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataspace::PutDataspaces);
    data12::Object::Dataspace space;
    space.m_path = benchmarkTestDataSpace;
    openKVServerFixture.spaces_.push_back(space.m_path);
    createSpaceRpc.req_.m_dataspaces.emplace("1", space);
    auto deleteSpaceRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataspace::DeleteDataspaces);
    deleteSpaceRpc.req_.m_uris.emplace("1", EmlUri::make_space(benchmarkTestDataSpace).to_string());

    // Benchmark loop
    for (auto _ : state)
    {
        // Create test data space (do not count)
        state.PauseTiming();
        if (!createSpaceRpc.call()) {
            state.SkipWithError(("Unable to create test data space: " +
                (createSpaceRpc.rc_.ex.m_error ? createSpaceRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }
        state.ResumeTiming();

        if (!deleteSpaceRpc.call()) {
            state.SkipWithError(("Unable to delete data space: " +
                (deleteSpaceRpc.rc_.ex.m_error ? deleteSpaceRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }
    }
}
BENCHMARK(Openkv_SpaceDelete)->Unit(kMillisecond);
