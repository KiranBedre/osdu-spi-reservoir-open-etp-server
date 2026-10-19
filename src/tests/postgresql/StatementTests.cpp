// ============================================================================
// Copyright 2019-2023 Emerson Paradigm Holding LLC. All rights reserved.
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

#define BOOST_TEST_MODULE tests_POSTGRESSTATEMENT

#include <memory>
#include <iostream>
#include <limits>
#include <algorithm>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <libpq-fe.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/ScopedUtils.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/postgresql/PostgreSQLTestsFixture.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Format.hpp>
#include <oes/postgresql/Statement.h>


using namespace oes::postgresql;
using namespace oes::testing::postgresql;
using oes::core::Guid;

BOOST_FIXTURE_TEST_SUITE(StatementTests, PQLibTestsFixture)

namespace
{
    struct TestData1
    {
        TestData1() = default;
        TestData1(std::string name1, int i1, float f1, double d1, const void* blob1, size_t blobSz1, Guid guid1, bool boolVal, unsigned int idRef)
            : name(std::move(name1)), i(i1), f(f1), d(d1), blob(blob1), blobSz(blobSz1), guid(guid1), _boolVal(boolVal), _idRef(idRef)
        { }

        void print() const {
            std::cout << "name = " << name << ", i = " << i << ", f = " << f << ", d = " << d <<
                " blob = [";
            if (blobSz) {
                std::vector<int> ib((int*)blob, (int*)blob + blobSz / sizeof(int));
                for (const auto& curr : ib) {
                    std::cout << curr << " ";
                }
            }
            std::cout << "], boolVal = " << _boolVal << ", idRef = " << _idRef;
        }

        void check(const TestData1& other) const {
            BOOST_REQUIRE_EQUAL(name, other.name);
            BOOST_REQUIRE_EQUAL(i, other.i);
            BOOST_REQUIRE_CLOSE(f, other.f, 0.01f);
            BOOST_REQUIRE_CLOSE(d, other.d, 0.01);
            BOOST_REQUIRE_EQUAL(blobSz, other.blobSz);
            if (blobSz > 0) {
                const size_t nElems = blobSz / sizeof(int);
                const std::vector<int> ib1(static_cast<const int*>(blob), static_cast<const int*>(blob) + nElems);
                const std::vector<int> ib2(static_cast<const int*>(other.blob), static_cast<const int*>(other.blob) + nElems);
                for (size_t idx = 0; idx < nElems; ++idx) {
                    BOOST_REQUIRE_EQUAL(ib1[idx], ib2[idx]);
                }
            }
            BOOST_REQUIRE_EQUAL(_boolVal, other._boolVal);
            BOOST_REQUIRE_EQUAL(_idRef, other._idRef);
        }

        std::string name;
        int i = 0;
        float f = 0.f;
        double d = 0.;
        const void* blob = nullptr;
        size_t blobSz = 0;
        Guid guid;
        bool _boolVal = false;
        unsigned int _idRef = 0;
    };

    std::vector<TestData1> prepareTestData()
    {
        static constexpr int blob1[] = {1, 2, 3};
        static constexpr int blob2[] = {4, 5};
        static constexpr int blob3[] = {6, 7, 8, 9, 10};
        static constexpr int blob4[] = {-6, -7, -8, -9, -10};
        std::vector<TestData1> test_data = {
            { "name1", 0, 2.f, 3., blob1, std::size(blob1) * sizeof(int), oes::core::make_guid(), true, 1 },
            { "name2", 2, 20.f, 30., blob2, std::size(blob1) * sizeof(int), oes::core::make_guid(), false, 2 },
            { "name3", std::numeric_limits<int>::max(), 200.f, 300., blob3, std::size(blob1) * sizeof(int), oes::core::make_guid(), true, 3},
            { "name4", std::numeric_limits<int>::min(), -200.2f, -300.3, blob4, std::size(blob1) * sizeof(int), oes::core::make_guid(), false, 4 }
        };
        return test_data;
    }
}


BOOST_AUTO_TEST_CASE(test_bind_get_scalars) {
    Connection_ptr conn = connection();

    // Create temp table
    std::string drop_it = "drop table if exists \"UT_Stmt_Scalars\"";
    BOOST_REQUIRE(exec_ddl(*conn, drop_it));
    BOOST_REQUIRE(exec_ddl(*conn, R"sql(

CREATE TABLE "UT_Stmt_Scalars"
( id BIGSERIAL PRIMARY KEY
, name text
, f REAL
, d DOUBLE PRECISION
, i INTEGER
, b boolean
, guid uuid
, blobv bytea
, idref bigint
)

    )sql"));

    oes::core::ScopedFunctor scoped_delete_schema([conn, drop_it] {
        exec_ddl(*conn, drop_it); // otherwise other tests fail...
    });

    using namespace std::string_literals;

    const float some_f32 = 1.2345f;
    const double some_f64 = 2.345678901;
    const int some_int = 1'025;
    const int64_t some_ref = 23'234'687;
    const std::string some_txt("The brown fox jumps");
    const std::string some_bin("The brown fox jumps\0as BLOB,\n\tkinda"s);
    const Guid some_uid = Guid::make();

    // Check embedded NULLs OK in blobs. Requires ""s suffix.
    BOOST_REQUIRE(some_txt.length() + 10 < some_bin.length());

    // Insert a row in that table
    Statement ins = conn->prepare(R"sql(

INSERT INTO "UT_Stmt_Scalars"
( name, f, d, i, b, guid, blobv, idref
) VALUES
( $1, $2, $3, $4, $5, $6, $7, $8
)

    )sql");
    ins.bind(1, some_txt);
    ins.bind(2, some_f32);
    ins.bind(3, some_f64);
    ins.bind(4, some_int);
    ins.bind(5, true);
    ins.bind(6, some_uid);
    ins.bind(7, static_cast<const void*>(some_bin.data()), static_cast<int>(some_bin.size()));
    ins.bind(8, some_ref);
    ins.step();
    ins.close();

    // Select that row, in that table
    Statement sel = conn->prepare(R"sql(

SELECT id, name, f, d, i, b, guid, blobv, idref
  FROM "UT_Stmt_Scalars"

    )sql");

    BOOST_REQUIRE(sel.step());
    const int64_t id = sel.getInt64(0);
    const std::string name = sel.getString(1);
    const float f = sel.getFloat(2);
    const double d = sel.getDouble(3);
    const int i = sel.getInt(4);
    const bool b = sel.getBoolean(5);
    const Guid uid = sel.getGuid(6);

    int bin_len;
    const char* bin_ptr = static_cast<const char*>(sel.getBlob(7, bin_len));
    const std::string bin(bin_ptr, bin_ptr + bin_len);

    const int64_t ref = sel.getInt64(8);
    BOOST_CHECK(!sel.step()); // should be a single row
    sel.close();

    // Check what we selected, is what we inserted
    BOOST_CHECK_EQUAL(id, 1); // implicit IDs start at 1, not 0
    BOOST_CHECK_EQUAL(name, some_txt);
    BOOST_CHECK_EQUAL(f, some_f32); // No CHECK_CLOSE, there should be no rounding
    BOOST_CHECK_EQUAL(d, some_f64);
    BOOST_CHECK_EQUAL(i, some_int);
    BOOST_CHECK_EQUAL(b, true);
    BOOST_CHECK_EQUAL(uid, some_uid);
    BOOST_CHECK_EQUAL(bin, some_bin);
    BOOST_CHECK_EQUAL(ref, some_ref);
}

BOOST_AUTO_TEST_CASE(statement_CRUD_test) {
    auto rw_conn_ptr = connection();
    BOOST_REQUIRE(rw_conn_ptr);

    // Crete table
    std::string table_name;
    table_name = "table_test";

    std::string query;
    query = "DROP TABLE IF EXISTS " + table_name + ";";
    auto result1 = rw_conn_ptr->execute(query);
    BOOST_REQUIRE(PQresultStatus(result1.get()) == PGRES_COMMAND_OK);

    const std::string table_columns = R"ee(
 (id BIGSERIAL PRIMARY KEY,
  name varchar(100),
  f REAL,
  d DOUBLE PRECISION,
  i INTEGER,
  blobv bytea,
  guid uuid,
  boolval boolean,
  idref bigint
  )
)ee";
    query = "CREATE TABLE IF NOT EXISTS " + table_name + table_columns;
    auto result2 = rw_conn_ptr->execute(query);
    BOOST_REQUIRE(PQresultStatus(result2.get()) == PGRES_COMMAND_OK);

    // Test data
    const std::vector<TestData1> test_data = prepareTestData();

    // Create data
    std::string sql("INSERT INTO ");
    sql += table_name + " (name, f, d, i, blobv, guid, boolval, idref) VALUES ($1, $2, $3, $4, $5, $6, $7, $8);";
    try {
        auto stmt1 = rw_conn_ptr->prepare(sql);
        for (const auto& row : test_data)
        {
            stmt1.bind(1, row.name);
            stmt1.bind(2, row.f);
            stmt1.bind(3, row.d);
            stmt1.bind(4, row.i);
            stmt1.bind(5, row.blob, static_cast<int>(row.blobSz));
            stmt1.bind(6, row.guid);
            stmt1.bind(7, row._boolVal);
            stmt1.bind(8, static_cast<long long>(row._idRef));
            stmt1.execute();
        }
        stmt1.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }

    // Read
    sql = "SELECT id, name, f, d, i, blobv, boolval, idref FROM " + table_name + ";";
    try {
        TestData1 out_data;
        auto stmt2 = rw_conn_ptr->prepare(sql);
        while (stmt2.step()) {
            BOOST_REQUIRE(stmt2.getInt64(0) != 0);
            out_data.name = stmt2.getString(1);
            out_data.f = stmt2.getFloat(2);
            out_data.d = stmt2.getDouble(3);
            out_data.i = stmt2.getInt(4);
            int sz;
            out_data.blob = stmt2.getBlob(5, sz);
            out_data.blobSz = sz;
            out_data._boolVal = stmt2.getBoolean(6);
            long long id_ref = stmt2.getInt64(7);
            out_data._idRef = static_cast<unsigned int>(id_ref);

            const auto& it = std::find_if(test_data.begin(), test_data.end(),
                [&out_data](const TestData1& pt) {
                return pt.name == out_data.name;
            });
            BOOST_REQUIRE(it != test_data.end());
            it->check(out_data);
        }
        stmt2.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (const std::exception& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }

    // Update
    sql = "UPDATE " + table_name + " SET name = $1, blobv = $2 WHERE id = $3;";
    int64_t id_to_update = 1;
    TestData1 data_updated(test_data[0]);
    data_updated.name = test_data[0].name + "_updated";
    std::vector<int> blob_updated = { 1, -1, 2, -2 };
    data_updated.blobSz = static_cast<int>(blob_updated.size() * sizeof(int));
    data_updated.blob = static_cast<void*>(blob_updated.data());
    try {
        TestData1 out_data;
        auto stmt3 = rw_conn_ptr->prepare(sql);
        stmt3.bind(1, data_updated.name);
        stmt3.bind(2, data_updated.blob, static_cast<int>(data_updated.blobSz));
        stmt3.bind(3, id_to_update);
        stmt3.step();

        stmt3.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }

    sql = "SELECT id, name, f, d, i, blobv, boolval, idref FROM " + table_name + " WHERE id = $1;";
    try {
        TestData1 out_data;
        auto stmt2 = rw_conn_ptr->prepare(sql);
        stmt2.bind(1, id_to_update);
        int count = 0;
        while (stmt2.step()) {
            int64_t id1 = stmt2.getInt64(0);
            BOOST_REQUIRE_EQUAL(id1, id_to_update);
            out_data.name = stmt2.getString(1);
            out_data.f = stmt2.getFloat(2);
            out_data.d = stmt2.getDouble(3);
            out_data.i = stmt2.getInt(4);
            int sz;
            out_data.blob = stmt2.getBlob(5, sz);
            out_data.blobSz = sz;
            out_data._boolVal = stmt2.getBoolean(6);
            long long id_ref = stmt2.getInt64(7);
            out_data._idRef = static_cast<unsigned int>(id_ref);

            out_data.check(data_updated);

            count++;
        }
        BOOST_REQUIRE_EQUAL(count, 1);
        stmt2.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }

    // Delete
    sql = "DELETE FROM " + table_name + " WHERE id = $1;";
    int64_t id_to_delete = 1;
    try {
        auto stmt3 = rw_conn_ptr->prepare(sql);
        stmt3.bind(1, id_to_delete);
        stmt3.step();

        stmt3.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }

    sql = "SELECT id, name, f, d, i, blobv, boolval, idref  FROM " + table_name + ";";
    try {
        TestData1 out_data;
        auto stmt2 = rw_conn_ptr->prepare(sql);
        int count = 0;
        while (stmt2.step()) {
            const int64_t id1 = stmt2.getInt64(0);
            BOOST_REQUIRE(id1 != id_to_delete);
            out_data.name = stmt2.getString(1);
            out_data.f = stmt2.getFloat(2);
            out_data.d = stmt2.getDouble(3);
            out_data.i = stmt2.getInt(4);
            int sz;
            out_data.blob = stmt2.getBlob(5, sz);
            out_data.blobSz = sz;
            out_data._boolVal = stmt2.getBoolean(6);
            long long id_ref = stmt2.getInt64(7);
            out_data._idRef = static_cast<unsigned int>(id_ref);

            count++;

            const auto& it = std::find_if(test_data.begin(), test_data.end(),
                [&out_data](const TestData1& pt) {
                return pt.name == out_data.name;
            });
            BOOST_REQUIRE(it != test_data.end());
            it->check(out_data);
        }
        BOOST_REQUIRE_EQUAL(count, test_data.size() - 1);
        stmt2.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }
}

BOOST_AUTO_TEST_CASE(statement_caching) {
    auto rw_conn_ptr = connection();
    BOOST_REQUIRE(rw_conn_ptr);

    const std::string table_name = "table_test";

    std::string query = "DROP TABLE IF EXISTS " + table_name + ";";
    const auto result1 = rw_conn_ptr->execute(query);
    BOOST_REQUIRE(PQresultStatus(result1.get()) == PGRES_COMMAND_OK);

    const std::string table_columns = R"ee(
 (id BIGSERIAL PRIMARY KEY,
  name varchar(100),
  f REAL,
  d DOUBLE PRECISION,
  i INTEGER,
  blobv bytea,
  guid uuid,
  boolval boolean,
  idref bigint
  )
)ee";
    query = "CREATE TABLE IF NOT EXISTS " + table_name + table_columns;
    const auto result2 = rw_conn_ptr->execute(query);
    BOOST_REQUIRE(PQresultStatus(result2.get()) == PGRES_COMMAND_OK);

    // Test data
    const std::vector<TestData1> test_data = prepareTestData();

    // Create data
    std::string sql("INSERT INTO ");
    sql += table_name + " (name, f, d, i, blobv, guid, boolval, idref) VALUES ($1, $2, $3, $4, $5, $6, $7, $8);";
    try {
        auto stmt1 = rw_conn_ptr->prepare(sql);
        // Should be the same internal instance
        auto stmt2 = rw_conn_ptr->prepare(sql);
        for (auto& row : test_data)
        {
            stmt1.bind(1, row.name);
            stmt1.bind(2, row.f);
            stmt1.bind(3, row.d);
            stmt1.bind(4, row.i);
            stmt1.bind(5, row.blob, static_cast<int>(row.blobSz));
            stmt1.bind(6, row.guid);
            stmt1.bind(7, row._boolVal);
            stmt1.bind(8, static_cast<long long>(row._idRef));
            stmt2.execute();
        }
        stmt2.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }
}

BOOST_AUTO_TEST_CASE(cursor_statement_test) {
    // Create table
    std::string table_name = "table_test";
    const std::string table_columns = R"ee(
 (id BIGSERIAL PRIMARY KEY,
  name varchar(100),
  f REAL,
  d DOUBLE PRECISION,
  i INTEGER,
  blobv bytea,
  guid uuid,
  boolval boolean,
  idref bigint)
)ee";

    auto rw_conn_ptr = connection();
    BOOST_REQUIRE(rw_conn_ptr);

    const std::string query[] = {
        fmt::format("DROP TABLE IF EXISTS {};", table_name),
        fmt::format("CREATE TABLE IF NOT EXISTS {} {};", table_name, table_columns)
    };
    for (const auto& q : query) {
        auto result = rw_conn_ptr->execute(q);
        BOOST_REQUIRE(PQresultStatus(result.get()) == PGRES_COMMAND_OK);
    }

    // Test data
    const std::vector<TestData1> test_data = prepareTestData();

    // Create data
    Transaction tnx = connection()->begin_transaction();
    std::string sql("INSERT INTO ");
    sql += table_name + " (name, f, d, i, blobv, guid, boolval, idref) VALUES ($1, $2, $3, $4, $5, $6, $7, $8);";
    try {
        auto stmt1 = rw_conn_ptr->prepare(sql);
        for (const auto& row : test_data)
        {
            stmt1.bind(1, row.name);
            stmt1.bind(2, row.f);
            stmt1.bind(3, row.d);
            stmt1.bind(4, row.i);
            stmt1.bind(5, row.blob, static_cast<int>(row.blobSz));
            stmt1.bind(6, row.guid);
            stmt1.bind(7, row._boolVal);
            stmt1.bind(8, static_cast<long long>(row._idRef));
            stmt1.execute();
        }
        stmt1.close();
    }
    catch (oes::postgresql::PostgreSQLException& e) {
        BOOST_REQUIRE_MESSAGE(false, e.what());
    }
    catch (...) {
        BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
    }
    tnx.commit();

    // Read
    sql = "SELECT id, name, f, d, i, blobv, boolval, idref FROM " + table_name + ";";
    const std::vector<unsigned> chunk_size_in_rows = { 1000, 1, 2, 3, 4 };
    for (auto csz : chunk_size_in_rows) {
        //DBG
        //std::cout << "N ROWS IN CHUNK = " << csz << " ==================\n";
        //DBG
        try {
            TestData1 out_data;
            auto stmt2 = rw_conn_ptr->prepare(sql, StatementType::PreparedWithCursor);
            stmt2.setChunkSizeInRows(csz);
            const auto expectedRows = std::min<size_t>(test_data.size(), csz);
            const auto selectedRows = stmt2.execute();
            BOOST_REQUIRE_EQUAL(expectedRows, selectedRows);
            do {
                BOOST_REQUIRE(stmt2.getInt64(0) != 0);
                out_data.name = stmt2.getString(1);
                out_data.f = stmt2.getFloat(2);
                out_data.d = stmt2.getDouble(3);
                out_data.i = stmt2.getInt(4);
                int sz;
                out_data.blob = stmt2.getBlob(5, sz);
                out_data.blobSz = sz;
                out_data._boolVal = stmt2.getBoolean(6);
                long long id_ref = stmt2.getInt64(7);
                out_data._idRef = static_cast<unsigned int>(id_ref);

                const auto& it = std::find_if(test_data.begin(), test_data.end(),
                    [&out_data](const TestData1& pt) {
                    return pt.name == out_data.name;
                });
                BOOST_REQUIRE(it != test_data.end());
                it->check(out_data);
            } while (stmt2.step());

            stmt2.close();
        }
        catch (oes::postgresql::PostgreSQLException& e) {
            BOOST_REQUIRE_MESSAGE(false, e.what());
        }
        catch (const std::exception& e) {
            BOOST_REQUIRE_MESSAGE(false, e.what());
        }
        catch (...) {
            BOOST_REQUIRE_MESSAGE(false, "Unspecified exception");
        }
    }
    // Check with WHERE clause to check parameters binding; filter out row with id = 1
    {
        //DBG
        //std::cout << "filter out row with id = 1 =================\n";
        //DBG
        sql = "SELECT id, name, f, d, i, blobv, boolval, idref FROM " + table_name + " WHERE id != $1;";
        for (int csz : chunk_size_in_rows)
        {
            TestData1 out_data;
            auto stmt2 = rw_conn_ptr->prepare(sql, StatementType::PreparedWithCursor);
            int64_t id_skip = 1;
            stmt2.bind(1, id_skip);
            int count = 0;
            stmt2.setChunkSizeInRows(csz);
            while (stmt2.step()) {
                BOOST_REQUIRE(stmt2.getInt64(0) != id_skip);
                out_data.name = stmt2.getString(1);
                out_data.f = stmt2.getFloat(2);
                out_data.d = stmt2.getDouble(3);
                out_data.i = stmt2.getInt(4);
                int sz;
                out_data.blob = stmt2.getBlob(5, sz);
                out_data.blobSz = sz;
                out_data._boolVal = stmt2.getBoolean(6);
                long long id_ref = stmt2.getInt64(7);
                out_data._idRef = static_cast<unsigned int>(id_ref);

                const auto& it = std::find_if(test_data.begin(), test_data.end(),
                    [&out_data](const TestData1& pt) {
                    return pt.name == out_data.name;
                });
                BOOST_REQUIRE(it != test_data.end());
                it->check(out_data);

                count++;
            }
            stmt2.close();
            BOOST_REQUIRE_EQUAL(count, test_data.size() - 1);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
