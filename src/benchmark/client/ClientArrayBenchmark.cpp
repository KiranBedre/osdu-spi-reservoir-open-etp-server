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
#include <oes/common/etp12/EmlUri.h>
#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>
#include "BenchmarkTestData.h"


using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::eml::etp12;
using namespace oes::client;
using namespace oes::testing::eml;

// ============================================================================
// Common setup/teardown
// ============================================================================
namespace
{
    // Shared variables (used for multi threaded tests)
    std::shared_ptr<OpenKVClientServerFixture> g_clientServerFixture;
    std::shared_ptr<OpenKVServerFixture> g_openKVServerFixture;
    std::shared_ptr<Datatypes::DataArrayTypes::PutDataArraysType> g_grid2d;
    Client_uptr g_client;

    bool SetupSharedServer(State& state)
    {
        // Configure server only once for all threads
        if (state.thread_index() == 0)
        {
            // Configure server/client
            g_clientServerFixture = std::make_shared<OpenKVClientServerFixture>();
            g_clientServerFixture->startServer();

            // Prepare test data
            g_openKVServerFixture = std::make_shared<OpenKVServerFixture>();
            auto space = g_openKVServerFixture->putSpace(benchmarkTestDataSpace);
            g_grid2d = std::make_shared<Datatypes::DataArrayTypes::PutDataArraysType>(g_openKVServerFixture->putGrid2dAry(space));

            g_client = g_clientServerFixture->connectClient();
            if (!g_client) {
                state.SkipWithError("Unable to connect to server");
                return false;
            }
        }
        return true;
    }

    void TeardownSharedServer(State& state)
    {
        if (state.thread_index() == 0)
        {
            g_client.reset();
            g_grid2d.reset();
            g_openKVServerFixture.reset();
            g_clientServerFixture.reset();
        }
    }
}


// ============================================================================
// Benchmarks
// ============================================================================
void Client_ArrayPut(State& state)
{
    // Configure server/client
    OpenKVClientServerFixture clientServerFixture;
    clientServerFixture.startServer();
    const auto client = clientServerFixture.connectClient();
    if (!client) {
        return state.SkipWithError("Unable to connect to server");
    }
    OpenKVServerFixture openKVServerFixture;
    const auto space = openKVServerFixture.putSpace(benchmarkTestDataSpace);

    // Prepare test data
    const auto emlUri = oes::common::etp12::EmlUri::make_obj(
        space.m_path,
        "EpcExternalPartReference",
        Guid::make(),
        oes::common::etp12::EmlUri::eEML20);
    const auto epcUriStr = emlUri.to_string();
    const auto& grid2dMock = grid2d_mock();
    const auto& arrayMock = grid2dMock.arrays_[0];
    const auto& expectedSize = arrayMock.dims_[0] * arrayMock.dims_[1] * sizeof(double);

    // Prepare RPCs
    auto putArrayRpc = rpcFor(client->array_proto(), &Protocol::IDataArray::PutDataArrays);
    Datatypes::DataArrayTypes::PutDataArraysType dataArray;
    dataArray.m_uid.m_uri = epcUriStr;
    dataArray.m_uid.m_pathInResource = arrayMock.path_;
    dataArray.m_array.m_dimensions = arrayMock.dims_;
    arrayMock.init_(dataArray.m_array.m_data.m_item);
    putArrayRpc.req_.m_dataArrays.emplace("ary#1", std::move(dataArray));
    std::ostringstream oss;

    // Configure counter (constant for each iteration, since the size of test array is fixed)
    state.counters["Rate"] = Counter(static_cast<double>(expectedSize), Counter::kAvgThreadsRate | Counter::kIsIterationInvariant, Counter::kIs1024);

    // Benchmark loop
    for (auto _ : state)
    {
        if (!call_succeeded(putArrayRpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to put array: " + oss.str()).c_str());
            break;
        }
    }
}
BENCHMARK(Client_ArrayPut)->Unit(kMillisecond)->Iterations(100)->UseRealTime();


void Client_ArrayGetMt(State& state)
{
    // Configure server only once for all threads
    if (SetupSharedServer(state) && state.thread_index() == 0)
    {
        // Configure counters (constant for each iteration, since the size of test array is fixed)
        const auto& arrayRef = std::get<Datatypes::ArrayOfDouble>(g_grid2d->m_array.m_data.m_item);
        state.counters["AvgRate"] = Counter(static_cast<double>(arrayRef.m_values.size() * sizeof(double)), Counter::kAvgThreadsRate | Counter::kIsIterationInvariant, Counter::kIs1024);
        state.counters["TotalRate"] = Counter(static_cast<double>(arrayRef.m_values.size() * sizeof(double)), Counter::kIsRate | Counter::kIsIterationInvariant, Counter::kIs1024);
    }

    // Benchmark loop
    for (auto _ : state)
    {
        // Prepare RPC (do not count)
        state.PauseTiming();
        auto getArrayRpc = rpcFor(g_client->array_proto(), &Protocol::IDataArray::GetDataArrays);
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

    TeardownSharedServer(state);
}
BENCHMARK(Client_ArrayGetMt)->ThreadRange(1, 16)->Unit(kMillisecond)->Iterations(50)->UseRealTime();


void Client_ArrayPutSubPartMt(State& state)
{
    // Configure server only once for all threads
    if (SetupSharedServer(state))
    {
        // Init counters
        state.counters["AvgRate"] = Counter(0, Counter::kAvgThreadsRate, Counter::kIs1024);
        state.counters["TotalRate"] = Counter(0, Counter::kIsRate, Counter::kIs1024);
    }

    // Benchmark loop
    for (auto _ : state)
    {
        // Prepare RPC (do not count)
        state.PauseTiming();
        auto putSubArrayRpc = rpcFor(g_client->array_proto(), &Protocol::IDataArray::PutDataSubarrays);
        // Split full array to number of chunks that equal to the number of threads
        const auto dim1ChunkSize = g_grid2d->m_array.m_dimensions[0] / state.threads();
        const auto dim2ChunkSize = g_grid2d->m_array.m_dimensions[1];
        const auto fullChunkSize = dim1ChunkSize * dim2ChunkSize;
        // Each thread sends its chunk
        Datatypes::DataArrayTypes::PutDataSubarraysType part;
        part.m_uid = g_grid2d->m_uid;
        part.m_starts = { state.thread_index() * dim1ChunkSize, 0 };
        part.m_counts = { dim1ChunkSize, dim2ChunkSize };
        Datatypes::ArrayOfDouble arrayOfDouble;
        const auto& sourceArray = std::get<Datatypes::ArrayOfDouble>(g_grid2d->m_array.m_data.m_item);
        const auto startChunkIt = sourceArray.m_values.begin() + part.m_starts[0] * dim2ChunkSize;
        arrayOfDouble.m_values.assign(startChunkIt, startChunkIt + fullChunkSize);
        part.m_data.m_item = std::move(arrayOfDouble);
        const auto partId = "part#" + std::to_string(state.thread_index());
        putSubArrayRpc.req_.m_dataSubarrays.emplace(partId, std::move(part));
        std::ostringstream oss;
        state.ResumeTiming();

        // Tested API call
        if (!call_succeeded(putSubArrayRpc.call(oss, timeOut), oss)) {
            state.SkipWithError(std::string("Unable to put sub array: " + oss.str()).c_str());
            break;
        }

        // Perform a simple response check (do not count)
        state.PauseTiming();
        auto it = putSubArrayRpc.res_.m_success.find(partId);
        if (it == putSubArrayRpc.res_.m_success.end()) {
            state.SkipWithError(fmt::format("Missed requested part in the response: {}", partId).c_str());
            break;
        }
        // Counters
        state.counters["AvgRate"] += static_cast<double>(fullChunkSize);
        state.counters["TotalRate"] += static_cast<double>(fullChunkSize);
        state.ResumeTiming();
    }

    // Post-validation of array integrity
    if (!state.error_occurred() && state.thread_index() == 0)
    {
        // Get full array
        auto getArrayRpc = rpcFor(g_client->array_proto(), &Protocol::IDataArray::GetDataArrays);
        static constexpr char testPartId[] = "FullArray";
        getArrayRpc.req_.m_dataArrays.emplace(testPartId, g_grid2d->m_uid);
        std::ostringstream oss;
        if (call_succeeded(getArrayRpc.call(oss, timeOut), oss))
        {
            auto it = getArrayRpc.res_.m_dataArrays.find(testPartId);
            if (it != getArrayRpc.res_.m_dataArrays.end())
            {
                const auto& receivedArray = std::get<Datatypes::ArrayOfDouble>(it->second.m_data.m_item);
                const auto& sourceArray = std::get<Datatypes::ArrayOfDouble>(g_grid2d->m_array.m_data.m_item);
                if (receivedArray.m_values != sourceArray.m_values) {
                    state.SkipWithError("Corrupted data in the received array");
                }
            }
            else {
                state.SkipWithError(fmt::format("Missed requested part in the response: {}", testPartId).c_str());
            }
        }
        else {
            state.SkipWithError(std::string("Unable to get full array: " + oss.str()).c_str());
        }
    }

    TeardownSharedServer(state);
}
BENCHMARK(Client_ArrayPutSubPartMt)->ThreadRange(1, 8)->Unit(kMillisecond)->Iterations(50)->UseRealTime();
