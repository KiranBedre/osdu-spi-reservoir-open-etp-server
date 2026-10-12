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

#include <oes/eml/openkv/ArrayProto.h>
#include <oes/testing/eml/ServerFixture.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml;
using namespace oes::eml::etp12;
using oes::common::etp12::EmlUri;

// ============================================================================
// Benchmarks
// ============================================================================
void Openkv_ArrayPut(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);
    const auto space = openKVServerFixture.putSpace(benchmarkTestDataSpace);

    // Prepare test data
    const auto emlUri = EmlUri::make_obj(space.m_path, "EpcExternalPartReference", oes::core::Guid::make(), EmlUri::eEML20);
    const auto epcUriStr = emlUri.to_string();
    const auto& grid2dMock = oes::testing::eml::grid2d_mock();
    const auto& arrayMock = grid2dMock.arrays_[0];

    // Prepare RPC
    auto putArrayRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataArray::PutDataArrays);
    Datatypes::DataArrayTypes::PutDataArraysType dataArray;
    dataArray.m_uid.m_uri = epcUriStr;
    dataArray.m_uid.m_pathInResource = arrayMock.path_;
    dataArray.m_array.m_dimensions = arrayMock.dims_;
    arrayMock.init_(dataArray.m_array.m_data.m_item);
    putArrayRpc.req_.m_dataArrays.emplace("ary#1", std::move(dataArray));

    // Benchmark loop
    for (auto _ : state)
    {
        if (!putArrayRpc.call()) {
            state.SkipWithError(("Unable to put array: " +
                (putArrayRpc.rc_.ex.m_error ? putArrayRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }
    }
}
BENCHMARK(Openkv_ArrayPut)->Unit(kMillisecond);


void Openkv_ArrayGet(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);
    const auto space = openKVServerFixture.putSpace(benchmarkTestDataSpace);

    // Prepare test data
    const auto emlUri = EmlUri::make_obj(space.m_path, "EpcExternalPartReference", oes::core::Guid::make(), EmlUri::eEML20);
    const auto epcUriStr = emlUri.to_string();
    const auto& grid2dMock = oes::testing::eml::grid2d_mock();
    const auto& arrayMock = grid2dMock.arrays_[0];
    openKVServerFixture.putMockAry(space, epcUriStr, arrayMock);

    // Prepare RPC
    auto getArrayRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataArray::GetDataArrays);
    static constexpr char testPartId[] = "FullArray";
    Datatypes::DataArrayTypes::DataArrayIdentifier part;
    part.m_uri = epcUriStr;
    part.m_pathInResource = arrayMock.path_;
    const size_t expectedSize = arrayMock.dims_[0] * arrayMock.dims_[1];
    getArrayRpc.req_.m_dataArrays.emplace(testPartId, std::move(part));

    // Benchmark loop
    for (auto _ : state)
    {
        if (!getArrayRpc.call()) {
            state.SkipWithError(("Unable to get array: " +
                (getArrayRpc.rc_.ex.m_error ? getArrayRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }

        // Perform a simple response check (do not count)
        state.PauseTiming();
        auto it = getArrayRpc.res_.m_dataArrays.find(testPartId);
        if (it == getArrayRpc.res_.m_dataArrays.end()) {
            state.SkipWithError(fmt::format("Missed requested part in the response: {}", testPartId).c_str());
            break;
        }
        if (it->second.m_dimensions != arrayMock.dims_) {
            state.SkipWithError("Mismatch dimensions of array");
            break;
        }
        if (!std::holds_alternative<Datatypes::ArrayOfDouble>(it->second.m_data.m_item)) {
            state.SkipWithError("Mismatch array type");
            break;
        }
        if (std::get<Datatypes::ArrayOfDouble>(it->second.m_data.m_item).m_values.size() != expectedSize) {
            state.SkipWithError("Mismatch array size");
            break;
        }
        // Cleanup
        getArrayRpc.res_.m_dataArrays.clear();
        state.ResumeTiming();
    }
}
BENCHMARK(Openkv_ArrayGet)->Unit(kMillisecond);


void Openkv_ArrayGetSubPart(State& state)
{
    // Configure server fixture
    oes::testing::eml::OpenKVServerFixture openKVServerFixture;
    openKVServerFixture.clearDataSpace(benchmarkTestDataSpace);
    const auto space = openKVServerFixture.putSpace(benchmarkTestDataSpace);

    // Prepare test data
    const auto emlUri = EmlUri::make_obj(space.m_path, "EpcExternalPartReference", oes::core::Guid::make(), EmlUri::eEML20);
    const auto epcUriStr = emlUri.to_string();
    const auto& grid2dMock = oes::testing::eml::grid2d_mock();
    const auto& arrayMock = grid2dMock.arrays_[0];
    openKVServerFixture.putMockAry(space, epcUriStr, arrayMock);

    // Prepare RPC
    auto getSubArrayRpc = openKVServerFixture.makeRpc(&openkv::OpenKVDataArray::GetDataSubarrays);
    static constexpr char testPartId[] = "HalfOfArray";
    Datatypes::DataArrayTypes::GetDataSubarraysType part;
    part.m_uid.m_uri = epcUriStr;
    part.m_uid.m_pathInResource = arrayMock.path_;
    part.m_starts = { arrayMock.dims_[0] / 2, 0 };       // Get half of array
    part.m_counts = { arrayMock.dims_[0] / 2, arrayMock.dims_[1] };
    const size_t expectedSize = part.m_counts[0] * part.m_counts[1];
    getSubArrayRpc.req_.m_dataSubarrays.emplace(testPartId, part);

    // Benchmark loop
    for (auto _ : state)
    {
        if (!getSubArrayRpc.call()) {
            state.SkipWithError(("Unable to get array: " +
                (getSubArrayRpc.rc_.ex.m_error ? getSubArrayRpc.rc_.ex.m_error.value().m_message : "unknown error")).c_str());
            break;
        }

        // Perform a simple response check (do not count)
        state.PauseTiming();
        auto it = getSubArrayRpc.res_.m_dataSubarrays.find(testPartId);
        if (it == getSubArrayRpc.res_.m_dataSubarrays.end()) {
            state.SkipWithError(fmt::format("Missed requested part in the response: {}", testPartId).c_str());
            break;
        }
        if (it->second.m_dimensions != part.m_counts) {
            state.SkipWithError("Mismatch dimensions of array");
            break;
        }
        if (!std::holds_alternative<Datatypes::ArrayOfDouble>(it->second.m_data.m_item)) {
            state.SkipWithError("Mismatch array type");
            break;
        }
        if (std::get<Datatypes::ArrayOfDouble>(it->second.m_data.m_item).m_values.size() != expectedSize) {
            state.SkipWithError("Mismatch array size");
            break;
        }
        // Cleanup
        getSubArrayRpc.res_.m_dataSubarrays.clear();
        state.ResumeTiming();
    }
}
BENCHMARK(Openkv_ArrayGetSubPart)->Unit(kMillisecond);
