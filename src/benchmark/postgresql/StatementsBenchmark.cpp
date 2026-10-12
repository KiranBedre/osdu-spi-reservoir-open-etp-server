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
// ToDo: Should not be required (hide details of PostgreSql lib from public interface of 'oes_postgresql')
#include <libpq-fe.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "BenchmarkTestData.h"
#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Statement.h>
#include <oes/postgresql/Format.hpp>
#include <oes/postgresql/PhysicalName.h>
#include <oes/testing/postgresql/PostgreSQLTestsFixture.h>

using namespace benchmark;
using namespace oes::benchmark;
using namespace oes::postgresql;

// ============================================================================
// Common setup/teardown
// ============================================================================
namespace
{
    constexpr auto benchmarkTable = "BenchmarkTemp";

    struct TestModel
    {
        uint64_t id{};
        int64_t col_1_int64{}, col_2_int64{}, col_3_int64{}, col_4_int64{}, col_5_int64{};
        float col_6_float{}, col_7_float{}, col_8_float{}, col_9_float{}, col_10_float{};
        double col_11_double{}, col_12_double{}, col_13_double{}, col_14_double{}, col_15_double{};
        std::string col_16_text, col_17_text;
        oes::core::Guid col_18_uuid, col_19_uuid;
    };

    bool CreateTestTable(State& state, const std::shared_ptr<Connection>& dbConnection)
    {
        if (!dbConnection)
        {
            state.SkipWithError("Unable to connect to DB");
            return false;
        }

        if (!exec_ddl(*dbConnection, R"sql(CREATE TABLE IF NOT EXISTS "{}" (
    id BIGSERIAL PRIMARY KEY,
    col_1_int64 int8, col_2_int64 int8, col_3_int64 int8, col_4_int64 int8, col_5_int64 int8,
    col_6_float float4, col_7_float float4, col_8_float float4, col_9_float float4, col_10_float float4,
    col_11_double float8, col_12_double float8, col_13_double float8, col_14_double float8, col_15_double float8,
    col_16_text text not null, col_17_text text not null,
    col_18_uuid uuid not null, col_19_uuid uuid not null
))sql", benchmarkTable))
        {
            state.SkipWithError("Unable to create test table");
            return false;
        }
        return true;
    }

    std::string PrepareInsertSql()
    {
        auto sql = fmt::format(R"sql(insert into "{}" (
    col_1_int64, col_2_int64, col_3_int64, col_4_int64, col_5_int64,
    col_6_float, col_7_float, col_8_float, col_9_float, col_10_float,
    col_11_double, col_12_double, col_13_double, col_14_double, col_15_double,
    col_16_text, col_17_text,
    col_18_uuid, col_19_uuid
) values ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19) returning id)sql", benchmarkTable);
        return sql;
    }

    void BindStatementArguments(Statement& stmt)
    {
        stmt.bind(1, buildTestValue<int64_t>());
        stmt.bind(2, buildTestValue<int64_t>());
        stmt.bind(3, buildTestValue<int64_t>());
        stmt.bind(4, buildTestValue<int64_t>());
        stmt.bind(5, buildTestValue<int64_t>());

        stmt.bind(6, buildTestValue<float>());
        stmt.bind(7, buildTestValue<float>());
        stmt.bind(8, buildTestValue<float>());
        stmt.bind(9, buildTestValue<float>());
        stmt.bind(10, buildTestValue<float>());

        stmt.bind(11, buildTestValue<double>());
        stmt.bind(12, buildTestValue<double>());
        stmt.bind(13, buildTestValue<double>());
        stmt.bind(14, buildTestValue<double>());
        stmt.bind(15, buildTestValue<double>());

        stmt.bind(16, buildTestValue<std::string>());
        stmt.bind(17, buildTestValue<std::string>());

        stmt.bind(18, buildTestValue<oes::core::Guid>());
        stmt.bind(19, buildTestValue<oes::core::Guid>());
    }

    bool PrepareTestDataSet(State& state, const std::shared_ptr<Connection>& dbConnection, size_t rowsCount = 1000)
    {
        auto stmt = dbConnection->prepare(PrepareInsertSql());
        for (size_t i = 0; i < rowsCount; ++i)
        {
            BindStatementArguments(stmt);
            if (!stmt.step() || stmt.getInt64(0) == 0) {
                state.SkipWithError("Unable to execute prepared statement");
                return false;
            }
            stmt.close();
        }
        return true;
    }

    Statement PrepareSelectStatement(const std::shared_ptr<Connection>& dbConnection, size_t rowsCount)
    {
        auto sql = fmt::format(R"sql(SELECT id,
    col_1_int64, col_2_int64, col_3_int64, col_4_int64, col_5_int64,
    col_6_float, col_7_float, col_8_float, col_9_float, col_10_float,
    col_11_double, col_12_double, col_13_double, col_14_double, col_15_double,
    col_16_text, col_17_text, col_18_uuid, col_19_uuid
FROM "{}"
LIMIT {})sql", benchmarkTable, rowsCount);
        return dbConnection->prepare(sql, "bench_select_stmt");
    }

    TestModel FetchRow(Statement stmt)
    {
        TestModel testModel;
        testModel.id = stmt.getInt64(0);

        testModel.col_1_int64 = stmt.getInt64(1);
        testModel.col_2_int64 = stmt.getInt64(2);
        testModel.col_3_int64 = stmt.getInt64(3);
        testModel.col_4_int64 = stmt.getInt64(4);
        testModel.col_5_int64 = stmt.getInt64(5);

        testModel.col_6_float = stmt.getFloat(6);
        testModel.col_7_float = stmt.getFloat(7);
        testModel.col_8_float = stmt.getFloat(8);
        testModel.col_9_float = stmt.getFloat(9);
        testModel.col_10_float = stmt.getFloat(10);

        testModel.col_11_double = stmt.getDouble(11);
        testModel.col_12_double = stmt.getDouble(12);
        testModel.col_13_double = stmt.getDouble(13);
        testModel.col_14_double = stmt.getDouble(14);
        testModel.col_15_double = stmt.getDouble(15);

        testModel.col_16_text = stmt.getString(16);
        testModel.col_17_text = stmt.getString(17);

        testModel.col_18_uuid = stmt.getGuid(18);
        testModel.col_19_uuid = stmt.getGuid(19);

        return testModel;
    }

    void DropTestTable(const std::shared_ptr<Connection>& dbConnection)
    {
        if (dbConnection) {
            constexpr auto dropTableStmt = R"sql(DROP TABLE IF EXISTS "{}")sql";
            exec_ddl(*dbConnection, dropTableStmt, benchmarkTable);
        }
    }
}

// ============================================================================
// Benchmarks
// ============================================================================

/**
 * \brief Test of new statement preparation.
 */
void PostgreSql_PrepareNewStatement(State& state)
{
    const oes::testing::postgresql::PQLibTestsFixture pqLibTestsFixture;
    if (!CreateTestTable(state, pqLibTestsFixture.connection_)) {
        return;
    }

    // Benchmark loop
    for (auto _ : state)
    {
        // Use new connection every time for avoid caching statements
        state.PauseTiming();
        Connection_ptr dbConnection (std::make_shared<Connection>(
            PGconn_uptr(PQconnectdb(pqLibTestsFixture.connection_string_.c_str()))));
        state.ResumeTiming();

        DoNotOptimize(pqLibTestsFixture.connection_->prepare(PrepareInsertSql(), "benchmark_insert_stmt"));
    }

    DropTestTable(pqLibTestsFixture.connection_);
}
BENCHMARK(PostgreSql_PrepareNewStatement)->Unit(kMillisecond)->Iterations(200)->MeasureProcessCPUTime();


/**
 * \brief Test of getting already prepared statement.
 */
void PostgreSql_GetPreparedStatement(State& state)
{
    const oes::testing::postgresql::PQLibTestsFixture pqLibTestsFixture;
    if (!CreateTestTable(state, pqLibTestsFixture.connection_)) {
        return;
    }
    const std::string sql = PrepareInsertSql();
    DoNotOptimize(pqLibTestsFixture.connection_->prepare(sql, "benchmark_insert_stmt"));

    // Benchmark loop
    for (auto _ : state)
    {
        DoNotOptimize(pqLibTestsFixture.connection_->getPrepared("benchmark_insert_stmt"));
    }

    DropTestTable(pqLibTestsFixture.connection_);
}
BENCHMARK(PostgreSql_GetPreparedStatement)->Unit(kMillisecond)->Iterations(1000)->MeasureProcessCPUTime();


/**
 * \brief Test prepare cached statement.
 */
void PostgreSql_GetCachedStatement(State& state)
{
    const oes::testing::postgresql::PQLibTestsFixture pqLibTestsFixture;
    if (!CreateTestTable(state, pqLibTestsFixture.connection_)) {
        return;
    }
    const std::string sql = PrepareInsertSql();
    DoNotOptimize(pqLibTestsFixture.connection_->prepare(sql, "benchmark_insert_stmt"));

    // Benchmark loop
    for (auto _ : state)
    {
        // Statement should be get from cache
        DoNotOptimize(pqLibTestsFixture.connection_->prepare(sql, "benchmark_insert_stmt"));
    }

    DropTestTable(pqLibTestsFixture.connection_);
}
BENCHMARK(PostgreSql_GetCachedStatement)->Unit(kMillisecond)->Iterations(1000)->MeasureProcessCPUTime();


/**
 * \brief Test of arguments binding for the prepared statement.
 */
void PostgreSql_BindMixedArgumentsSet(State& state)
{
    const oes::testing::postgresql::PQLibTestsFixture pqLibTestsFixture;
    if (!CreateTestTable(state, pqLibTestsFixture.connection_)) {
        return;
    }
    const std::string sql = PrepareInsertSql();
    auto stmt = pqLibTestsFixture.connection_->prepare(sql, "benchmark_insert_stmt");

    // Benchmark loop
    for (auto _ : state)
    {
        BindStatementArguments(stmt);
    }

    DropTestTable(pqLibTestsFixture.connection_);
}
BENCHMARK(PostgreSql_BindMixedArgumentsSet)->Unit(kMillisecond)->MeasureProcessCPUTime();;


/**
 * \brief Test of execution prepared statement.
 */
void PostgreSql_ExecutePrepared(State& state)
{
    const oes::testing::postgresql::PQLibTestsFixture pqLibTestsFixture;
    if (!CreateTestTable(state, pqLibTestsFixture.connection_)) {
        return;
    }
    auto stmt = pqLibTestsFixture.connection_->prepare(PrepareInsertSql(), "benchmark_insert_stmt");

    // Benchmark loop
    for (auto _ : state)
    {
        state.PauseTiming();
        BindStatementArguments(stmt);
        state.ResumeTiming();

        if (stmt.execute() == 0) {
            state.SkipWithError("Unable to execute prepared statement");
            break;
        }
    }

    DropTestTable(pqLibTestsFixture.connection_);
}
BENCHMARK(PostgreSql_ExecutePrepared)->Unit(kMillisecond)->Iterations(1000)->MeasureProcessCPUTime();


/**
 * \brief Test of data fetching via prepared statement.
 */
void PostgreSql_FetchMixedData(State& state)
{
    constexpr size_t testRowsCount = 500;

    const oes::testing::postgresql::PQLibTestsFixture pqLibTestsFixture;
    if (!CreateTestTable(state, pqLibTestsFixture.connection_)) {
        return;
    }
    if (!PrepareTestDataSet(state, pqLibTestsFixture.connection_, testRowsCount)) {
        return;
    }
    auto selectStmt = PrepareSelectStatement(pqLibTestsFixture.connection_, testRowsCount);

    // Benchmark loop
    TestModel testModels[testRowsCount];
    for (auto _ : state)
    {
        for (auto& testModel : testModels)
        {
            if (!selectStmt.step()) {
                state.SkipWithError("Unable to fetch next row");
                break;
            }
            testModel = FetchRow(selectStmt);
        }
        selectStmt.reset();
    }

    DropTestTable(pqLibTestsFixture.connection_);
}
BENCHMARK(PostgreSql_FetchMixedData)->Unit(kMillisecond)->Iterations(1000)->MeasureProcessCPUTime();
