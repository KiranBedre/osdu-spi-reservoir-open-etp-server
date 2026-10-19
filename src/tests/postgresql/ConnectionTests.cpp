// ============================================================================
// Copyright 2018-2023 Emerson Paradigm Holding LLC. All rights reserved.
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
#include <boost/endian/conversion.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/postgresql/PostgreSQLTestsFixture.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Format.hpp>
#include <oes/postgresql/Statement.h>
#include <oes/postgresql/PhysicalName.h>

#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Assert.h>

#include <oes/testing/core/TestMain.h>


using namespace oes::core;
using namespace oes::postgresql::relational;
using namespace oes::testing;
using namespace oes::testing::postgresql;
namespace endian = boost::endian;
using namespace oes::postgresql;

/*****************************************************************************/

namespace {

BOOST_FIXTURE_TEST_SUITE(ConnectionTests, PQLibTestsFixture)

BOOST_AUTO_TEST_CASE(test_PostgresConnectParams) {
    const UserCredentials& creds = masterUserCredentials();

    PostgresConnectParams params;
    params.add(connect_params::eUSER, creds.user.getRaw());
    params.add(connect_params::ePASSWORD, creds.password);
    params.add(connect_params::eHOST, creds.host);
    params.add(connect_params::ePORT, creds.port);
    params.add(connect_params::eDBNAME, creds.dbname);
    Connection_ptr new_conn = params.createConnection();
    BOOST_REQUIRE(new_conn);

    int warnings = 0;
    int notices = 0;
    int others = 0;
    new_conn->set_notice_handler(
        [&](const PGresult*, NoticeSeverity sev, const char* msg, const char* details) {
            OES_UNUSED(msg);
            OES_UNUSED(details);
            if (sev == eWARNING) {
                ++warnings;
            } else if (sev == eNOTICE) {
                ++notices;
            } else {
                ++others;
            }
        }
    );

    BOOST_REQUIRE_EQUAL(warnings, 0);
    BOOST_REQUIRE_EQUAL(notices, 0);
    BOOST_REQUIRE_EQUAL(others, 0);

    // will produce a warning, since no begin
    new_conn->execute("COMMIT");

    BOOST_REQUIRE_EQUAL(warnings, 1);
    BOOST_REQUIRE_EQUAL(notices, 0);
    BOOST_REQUIRE_EQUAL(others, 0);

    // no warning, just a NOTICE
    new_conn->execute("DROP SCHEMA IF EXISTS foo");

    BOOST_REQUIRE_EQUAL(warnings, 1);
    BOOST_REQUIRE_EQUAL(notices, 1);
    BOOST_REQUIRE_EQUAL(others, 0);
}

BOOST_AUTO_TEST_CASE(test_execute) {
    Connection_ptr conn = connection();

    BOOST_REQUIRE_EQUAL(conn->user_name(), master_creds_.user);
    BOOST_REQUIRE_EQUAL(conn->user_pname(), master_creds_.user);
    BOOST_REQUIRE_EQUAL(conn->host(), master_creds_.host);
    BOOST_REQUIRE_EQUAL(conn->port(), master_creds_.port);
    BOOST_REQUIRE_EQUAL(conn->db(), master_creds_.dbname);

    PGresult_uptr res = conn->execute("SELECT typname, oid::oid, typlen::smallint, typelem::oid, typarray::oid FROM pg_type");
    BOOST_REQUIRE(res);
    BOOST_REQUIRE(PQntuples(res.get()) > 0);
}

BOOST_AUTO_TEST_CASE(test_execute_syntax_error) {
    try {
        Connection_ptr conn = connection();
        PGresult_uptr res = conn->execute("THIS IS A SYNTAX ERROR");
        BOOST_FAIL("Should have thrown an error");
    }
    catch (const PostgreSQLException& ex) {
        BOOST_REQUIRE_EQUAL(ex.error_code, PG_42601);
    }
}

BOOST_AUTO_TEST_CASE(test_execute_with_params) {
    Connection_ptr conn = connection();

    const auto selectSql = "SELECT typname, oid::oid, typlen::smallint, typelem::oid, typarray::oid FROM pg_type WHERE typlen = $1";
    uint32_t val = endian::native_to_big((uint32_t)4);
    PGresult_uptr res = conn->execute(selectSql,
        { { reinterpret_cast<const char*>(&val), sizeof(uint32_t), true } }, { 23/*=int4*/ }, false
    );
    BOOST_REQUIRE(res);
    BOOST_REQUIRE(PQntuples(res.get()) > 0);

    val = endian::native_to_big((uint32_t)10090); // does not exist
    PGresult_uptr res1 = conn->execute(selectSql,
        { { reinterpret_cast<const char*>(&val), sizeof(uint32_t), true } }, { 23/*=int4*/ }, false
    );
    BOOST_REQUIRE(res1);
    BOOST_REQUIRE_EQUAL(PQntuples(res1.get()), 0);

    try {
        PGresult_uptr res2 = conn->execute("SELECT typname, oid::oid, typlen::smallint, typelem::oid, typarray::oid FROM pg_type WHERE typlen = $1",
            { { reinterpret_cast<const char*>(&val), sizeof(uint32_t), true } }, { 0/*=guess*/ }, false
        );
        BOOST_FAIL("Should have thrown an error");
    }
    catch (const PostgreSQLException& ex) {
        // incorrect binary data (smallint is expected)
        BOOST_REQUIRE_EQUAL(ex.error_code, PG_22P03);
    }
}

BOOST_AUTO_TEST_CASE(test_prepare) {
    Connection_ptr conn = connection(); 

    BOOST_REQUIRE_THROW(conn->getPrepared("test_prepare_query"), std::runtime_error);
    BOOST_REQUIRE_EQUAL(false, conn->tryGetPrepared("test_prepare_query").has_value());

    const auto testSql = "SELECT typname, oid::oid, typlen::smallint, typelem::oid, typarray::oid FROM pg_type WHERE typlen = $1";
    BOOST_REQUIRE_NO_THROW(conn->prepare(testSql, "test_prepare_query"));
    BOOST_REQUIRE_NO_THROW(conn->getPrepared("test_prepare_query"));
    BOOST_REQUIRE_EQUAL(true, conn->tryGetPrepared("test_prepare_query").has_value());

    // Prepare statement with the same SQL, but with different name
    BOOST_REQUIRE_NO_THROW(conn->prepare(testSql, "test_prepare_query2"));
    BOOST_REQUIRE_EQUAL("test_prepare_query2", conn->getPrepared("test_prepare_query2").getName());
    BOOST_REQUIRE_EQUAL(testSql, conn->getPrepared("test_prepare_query2").getSql());
    BOOST_REQUIRE_EQUAL("test_prepare_query2", conn->tryGetPrepared("test_prepare_query2").value().getName());

    // Should get cached statement when try to prepare WITH THE SAME SQL
    BOOST_REQUIRE_NO_THROW(conn->prepare(testSql, "test_prepare_query2"));

    // Should throw exception when try to overwrite existing 'test_prepare_query2' but WITH DIFFERENT SQL
    BOOST_CHECK_THROW(conn->prepare(
        "SELECT typname FROM pg_type WHERE typlen = $1::smallint",
        "test_prepare_query2"
    ), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_prepare_with_auto_names) {
    const Connection_ptr conn = connection();

    // Try to prepare with auto generated name
    const auto testSql = "SELECT typname, oid::oid, typlen::smallint, typelem::oid, typarray::oid FROM pg_type WHERE typlen = $1";
    BOOST_REQUIRE_NO_THROW(conn->prepare(testSql));

    // Should get cached statement
    BOOST_REQUIRE_NO_THROW(conn->prepare(testSql));
}

BOOST_AUTO_TEST_CASE(test_execute_prepared) {
    Connection_ptr conn = connection();

    auto stmt = conn->prepare("SELECT typname, oid::oid, typlen::smallint, typelem::oid, typarray::oid FROM pg_type WHERE typlen = $1");
    stmt.bind(1, short{ 4 });
    BOOST_REQUIRE(stmt.execute() > 0);

    stmt.bind(1, short{ 10090 });    // does not exist
    BOOST_REQUIRE_EQUAL(0, stmt.execute());
}

BOOST_AUTO_TEST_CASE(test_execute_prepared_on_different_schemas)
{
    // Arrange
    constexpr auto testTable = "TestTable";
    const std::string createTableSql = fmt::format(R"sql(CREATE TABLE "{}" ( id BIGSERIAL PRIMARY KEY ))sql", testTable);
    const std::string insertTableSql = fmt::format(R"sql(INSERT INTO "{}" (id) VALUES ( default ))sql", testTable);
    const std::string selectTableSql = fmt::format(R"sql(SELECT COUNT(id) FROM "{}")sql", testTable);
    const Connection_ptr conn = connection();
    const auto schemaScope1 = makeScopedSchema("ut_test_schema1");
    conn->execute(createTableSql);
    const auto schemaScope2 = makeScopedSchema("ut_test_schema2");
    conn->execute(createTableSql);
    Statement insertStmt = conn->prepare(insertTableSql);
    Statement selectStmt = conn->prepare(selectTableSql);

    // Act
    insertStmt.execute();
    insertStmt.execute();
    selectStmt.execute();
    const auto table2Size = selectStmt.getInt(0);

    conn->set_current_schema(schemaScope1);
    insertStmt.execute();
    selectStmt.execute();
    const auto table1Size = selectStmt.getInt(0);

    // Assert
    BOOST_REQUIRE_EQUAL(table1Size, 1);
    BOOST_REQUIRE_EQUAL(table2Size, 2);
}

BOOST_AUTO_TEST_CASE(test_exec_ddl) {
    Connection_ptr conn = connection();

    const std::string tname = "ut_test1";

    BOOST_REQUIRE(exec_ddl(*conn, "DROP TABLE IF EXISTS {}", tname));
    BOOST_REQUIRE(exec_ddl(*conn, "DROP TABLE IF EXISTS {}", tname));

    BOOST_REQUIRE(exec_ddl(*conn, "CREATE TABLE {} (id numeric NOT NULL, name varchar(256))", tname));
    try {
        exec_ddl(*conn, "CREATE TABLE {} (id numeric NOT NULL, name varchar(256))", tname);
        BOOST_FAIL("Expected throw");
    } catch (const PostgreSQLException& ex) {
        BOOST_REQUIRE_EQUAL(ex.error_code, PG_42P07);
    }

    BOOST_REQUIRE(exec_ddl(*conn, "DROP TABLE IF EXISTS {0}; CREATE TABLE {0} (id numeric NOT NULL, name varchar(256));", tname));

    try {
        // will throw even though the last instruction is legit.
        exec_ddl(*conn, "DROP TABLE IF EXISTS {0}; CREATE TABLE {0} (id numeric NOT NULL, name varchar(256));"
                        "CREATE TABLE {0} (id numeric NOT NULL, name varchar(256)); DROP TABLE IF EXISTS {0};", tname);
        BOOST_FAIL("Expected throw");
    } catch (const PostgreSQLException& ex) {
        BOOST_REQUIRE_EQUAL(ex.error_code, PG_42P07);
    }
}



BOOST_AUTO_TEST_CASE(test_current_schema) {
    Connection_ptr conn = connection();

    {
        const auto schema_scoped = makeScopedSchema(PhysicalName("ut_test_schema", IMPLICIT_LOWERCASE));
        BOOST_REQUIRE_EQUAL(conn->current_schema(), schema_scoped.schema_);
    }

    { // test with a case-sensitive name
        const auto schema_scoped = makeScopedSchema(PhysicalName("Ut_Test_Schema", NONE_CASE_SENSITIVE));
        BOOST_REQUIRE_EQUAL(conn->current_schema(), schema_scoped.schema_);
    }

    { // test with an inexistent schema
        PhysicalName schema_name("bazinga", NONE_CASE_SENSITIVE);

        BOOST_REQUIRE(!conn->set_current_schema(schema_name)); // fails
        BOOST_REQUIRE_EQUAL(conn->current_schema(), ""); // empty, not null
    }
}

/*****************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
}