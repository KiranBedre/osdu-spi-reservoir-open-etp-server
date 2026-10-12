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

#include <oes/eml/openkv/DiscoProto.h>
#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>
#include <oes/testing/core/TestMain.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml;
using namespace oes::eml::etp12;
using oes::common::etp12::EmlUri;

// ============================================================================
// Test data
// ============================================================================
static constexpr char testEpcFile[] = "PillarGrid.epc";
static constexpr char testH5File[] = "PillarGrid.h5";
static constexpr size_t expectedEpcResourcesCount = 105;

// ============================================================================
// Benchmarks
// ============================================================================
void Openkv_DiscoGetResources(State& state)
{
    // Check prerequisites
    const std::string epcInput = oes::testing::resolveTestDataFile("data/eml", testEpcFile, true);
    if (epcInput.empty()) {
        return state.SkipWithError("Test EPC file does not exist");
    }

    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);

    // Prepare test data
    oes::testing::eml::OpenKVClientServerFixture serverFixture;
    serverFixture.startServer();
    if (!serverFixture.directImport(epcInput, benchmarkTestDataSpace)) {
        return state.SkipWithError("Unable to import test data");
    }

    // Prepare RPC
    auto getResourcesRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDiscovery::GetResources);
    getResourcesRpc.req_.m_context.m_uri = EmlUri::make_space(benchmarkTestDataSpace).to_string();
    getResourcesRpc.req_.m_context.m_depth = 3;
    getResourcesRpc.req_.m_scope = Datatypes::Object::ContextScopeKind::eSelf;
    getResourcesRpc.req_.m_countObjects = true;

    // Benchmark loop
    for (auto _ : state)
    {
        const auto result = getResourcesRpc.call();
        if (!result) {
            state.SkipWithError(("Unable to get resources: " +
                (getResourcesRpc.rc_.ex.m_error ? getResourcesRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }
        if (getResourcesRpc.res_.m_resources.size() != expectedEpcResourcesCount) {
            state.SkipWithError(fmt::format("Resources size doesn't match, expected: {}, actual: {}",
                expectedEpcResourcesCount, getResourcesRpc.res_.m_resources.size()).c_str());
            break;
        }
    }
}
BENCHMARK(Openkv_DiscoGetResources)->Unit(kMillisecond);
