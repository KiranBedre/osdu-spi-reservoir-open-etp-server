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

#include <oes/eml/openkv/StoreProto.h>
#include <oes/testing/eml/ServerFixture.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml;
using namespace oes::eml::etp12;
using oes::common::etp12::EmlUri;

template <bool TWithXData = false>
void Openkv_StorePutObject(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);
    const auto space = openKVServerFixture.putSpace(benchmarkTestDataSpace, std::weak_ptr<openkv::OpenKVSession>(), true);

    // Prepare RPC
    auto putStoreObjectsRpc = openKVServerFixture.makeRpc(&openkv::OpenKVStore::PutDataObjects);
    const auto& crs = oes::testing::eml::crs_mock();
    const auto spaceUri = EmlUri::parse(space.m_uri);
    const EmlUri crsUri = spaceUri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_);
    data12::Object::DataObject crsObj;
    crsObj.m_resource.m_uri = crsUri.to_string();
    crsObj.m_resource.m_name = crs.name_;
    if constexpr (TWithXData) {
        crsObj.m_resource.m_customData = buildTestXDataMap();
    }
    crsObj.m_format = "xml";
    crsObj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
    putStoreObjectsRpc.req_.m_dataObjects.emplace("crs", std::move(crsObj));

    // Benchmark loop
    for (auto _ : state)
    {
        if (!putStoreObjectsRpc.call()) {
            state.SkipWithError(("Unable to create store object: " +
                (putStoreObjectsRpc.rc_.ex.m_error ? putStoreObjectsRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }
    }
}
BENCHMARK(Openkv_StorePutObject<false>)->Name("Openkv_StorePutObject")->Unit(kMillisecond);
BENCHMARK(Openkv_StorePutObject<true>)->Name("Openkv_StorePutObjectWithXData")->Unit(kMillisecond);


void Openkv_StoreGetObject(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);
    auto space = openKVServerFixture.putSpace(benchmarkTestDataSpace, std::weak_ptr<openkv::OpenKVSession>(), true);
    auto crsObj = openKVServerFixture.putMockObj(space, oes::testing::eml::crs_mock());

    // Prepare RPC
    static constexpr char testObjId[] = "obj#1";
    auto getStoreObjectsRpc = openKVServerFixture.makeRpc(&openkv::OpenKVStore::GetDataObjects);
    getStoreObjectsRpc.req_.m_format = "xml";
    getStoreObjectsRpc.req_.m_uris.emplace(testObjId, crsObj.m_resource.m_uri);

    // Benchmark loop
    for (auto _ : state)
    {
        if (!getStoreObjectsRpc.call()) {
            state.SkipWithError(("Unable to get store object: " +
                (getStoreObjectsRpc.rc_.ex.m_error ? getStoreObjectsRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }

        // Perform a simple response check (do not count)
        state.PauseTiming();
        auto it = getStoreObjectsRpc.res_.m_dataObjects.find(testObjId);
        if (it == getStoreObjectsRpc.res_.m_dataObjects.end()) {
            state.SkipWithError(fmt::format("Missed requested object in the response: {}", testObjId).c_str());
            break;
        }
        // Cleanup
        getStoreObjectsRpc.res_.m_dataObjects.clear();
        state.ResumeTiming();
    }
}
BENCHMARK(Openkv_StoreGetObject)->Unit(kMillisecond);


void Openkv_StoreDeleteObject(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);
    auto space = openKVServerFixture.putSpace(benchmarkTestDataSpace);

    // Prepare test data
    const auto& crs = oes::testing::eml::crs_mock();

    // Prepare RPCs
    auto deleteStoreObjectRpc = openKVServerFixture.makeRpc(&openkv::OpenKVStore::DeleteDataObjects);

    // Benchmark loop
    for (auto _ : state)
    {
        // Create test store object (do not count)
        state.PauseTiming();
        static constexpr char testObjId[] = "obj#1";
        const auto crsObj = openKVServerFixture.putMockObj(space, crs);
        deleteStoreObjectRpc.req_.m_uris[testObjId] = crsObj.m_resource.m_uri;
        state.ResumeTiming();

        if (!deleteStoreObjectRpc.call()) {
            state.SkipWithError(("Unable to delete store object: " +
                (deleteStoreObjectRpc.rc_.ex.m_error ? deleteStoreObjectRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }

        // Perform a simple response check (do not count)
        state.PauseTiming();
        auto it = deleteStoreObjectRpc.res_.m_deletedUris.find(testObjId);
        if (it == deleteStoreObjectRpc.res_.m_deletedUris.end()) {
            state.SkipWithError(fmt::format("Missed requested object in the response: {}", testObjId).c_str());
            break;
        }
        // Cleanup
        deleteStoreObjectRpc.res_.m_deletedUris.clear();
        state.ResumeTiming();
    }
}
BENCHMARK(Openkv_StoreDeleteObject)->Unit(kMillisecond);
