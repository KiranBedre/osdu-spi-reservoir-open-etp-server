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

#include <oes/testing/postgresql/PostgreSQLTestsFixture.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/DbConnectionPool.h>
#include <oes/testing/core/TestMain.h>


using namespace oes::testing::postgresql;
using namespace oes::postgresql;

// ============================================================================

namespace
{
    constexpr auto TestDbName1 = "Db_1";
    constexpr auto TestDbName2 = "Db_2";
}

// ============================================================================

BOOST_FIXTURE_TEST_SUITE(DbConnectionPoolTests, PQLibTestsFixture)

BOOST_AUTO_TEST_CASE(test_dbpool_register_fail_when_already_exists)
{
    // Arrange
    DbConnectionPool dbConnectionPool(2);
    dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), false);
    dbConnectionPool.registerDatabase(TestDbName2, master_creds_.toConnectionURI(), false);

    // Act / Assert
    BOOST_REQUIRE_EQUAL(false, dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), false));
    BOOST_REQUIRE_EQUAL(2, dbConnectionPool.getRegisteredDatabases().size());
}

BOOST_AUTO_TEST_CASE(test_dbpool_register_without_verify_conn)
{
    // Arrange
    DbConnectionPool dbConnectionPool(2);

    // Act / Assert
    BOOST_REQUIRE_EQUAL(true, dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), false));
    BOOST_REQUIRE_EQUAL(1, dbConnectionPool.getRegisteredDatabases().size());
    BOOST_REQUIRE_EQUAL(TestDbName1, dbConnectionPool.getRegisteredDatabases().begin()->first);
}

BOOST_AUTO_TEST_CASE(test_dbpool_register_with_verify_conn)
{
    // Arrange
    DbConnectionPool dbConnectionPool(2);

    // Act / Assert
    BOOST_REQUIRE_EQUAL(true, dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), false));
    BOOST_REQUIRE_EQUAL(false, dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), false));
    BOOST_REQUIRE_EQUAL(1, dbConnectionPool.getRegisteredDatabases().size());
}

BOOST_AUTO_TEST_CASE(test_dbpool_acquire_connection)
{
    // Arrange
    DbConnectionPool dbConnectionPool(2);
    dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), true);  // Establish connection immediately
    dbConnectionPool.registerDatabase(TestDbName2, master_creds_.toConnectionURI(), false); // Postpone connection

    // Act / Assert
    const auto dbConnection1 = dbConnectionPool.tryAcquireConnection(TestDbName1);
    BOOST_REQUIRE(dbConnection1);
    BOOST_REQUIRE_EQUAL(dbConnection1.use_count(), 2);
    const auto dbConnection2 = dbConnectionPool.tryAcquireConnection(TestDbName2);
    BOOST_REQUIRE(dbConnection2);
    BOOST_REQUIRE_EQUAL(dbConnection2.use_count(), 2);
}

BOOST_AUTO_TEST_CASE(test_dbpool_reuse_connection)
{
    // Arrange
    DbConnectionPool dbConnectionPool(2);
    dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), true);

    // Act / Assert
    auto dbConnection = dbConnectionPool.tryAcquireConnection(TestDbName1);
    const auto dbConnectionPtr = dbConnection.get();
    dbConnection.reset();   // Release DB connection
    BOOST_REQUIRE_EQUAL(dbConnectionPtr, dbConnectionPool.tryAcquireConnection(TestDbName1).get()); // Must be the same connection
}

BOOST_AUTO_TEST_CASE(test_dbpool_acquire_fail_when_exceed_limit)
{
    // Arrange
    DbConnectionPool dbConnectionPool(2);
    dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), true);

    // Act / Assert
    const auto dbConnection1 = dbConnectionPool.tryAcquireConnection(TestDbName1);
    BOOST_REQUIRE(dbConnection1);
    const auto dbConnection2 = dbConnectionPool.tryAcquireConnection(TestDbName1);
    BOOST_REQUIRE(dbConnection2);
    const auto dbConnection3 = dbConnectionPool.tryAcquireConnection(TestDbName1);  // Must return `nullptr` as limit is exceeded
    BOOST_REQUIRE_EQUAL(nullptr, dbConnection3.get());
}

BOOST_AUTO_TEST_CASE(test_dbpool_acquire_with_replace_unused)
{
    // Arrange
    DbConnectionPool dbConnectionPool(2);
    dbConnectionPool.registerDatabase(TestDbName1, master_creds_.toConnectionURI(), true);
    dbConnectionPool.registerDatabase(TestDbName2, master_creds_.toConnectionURI(), true);

    // Act / Assert
    const auto dbConnection1 = dbConnectionPool.tryAcquireConnection(TestDbName2);
    BOOST_REQUIRE(dbConnection1);
    const auto dbConnection2 = dbConnectionPool.tryAcquireConnection(TestDbName2);  // Existing connection to `TestDbName1` should be replaced
    BOOST_REQUIRE(dbConnection2);
    BOOST_REQUIRE_EQUAL(dbConnection2.use_count(), 2);
}

/*****************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
