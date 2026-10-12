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
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/postgresql/PostgreSQLTestsFixture.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Format.hpp>
#include <oes/testing/core/TestMain.h>


using namespace oes::testing::postgresql;
using namespace oes::postgresql;

// ============================================================================

namespace
{
    class PQLibTransactionsFixture : public PQLibTestsFixture
    {
    public:
        static constexpr auto testTable = "UT_Transactions";

        PQLibTransactionsFixture()
        {
            dropTestTable();
            createTestTable();
        }

        ~PQLibTransactionsFixture() override {
            dropTestTable();
        }

        void insertRow() {
            BOOST_REQUIRE(exec_ddl(*connection(), R"sql(INSERT INTO "{}" (id) VALUES ( default ))sql", testTable));
        }

        void createTestTable() {
            BOOST_REQUIRE(exec_ddl(*connection(), R"sql(CREATE TABLE "{}" ( id BIGSERIAL PRIMARY KEY ))sql", testTable));
        }

        void dropTestTable()
        {
            constexpr auto dropTableStmt = R"sql(DROP TABLE IF EXISTS "{}")sql";
            exec_ddl(*connection(), dropTableStmt, testTable);
        }
    };
}

// ============================================================================

BOOST_FIXTURE_TEST_SUITE(TransactionTests, PQLibTransactionsFixture)

BOOST_AUTO_TEST_CASE(test_transaction_commit)
{
    // Act
    auto transaction = connection()->begin_transaction();
    insertRow();
    transaction.commit();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 1);
}

BOOST_AUTO_TEST_CASE(test_transaction_explicit_rollback)
{
    // Act
    auto transaction = connection()->begin_transaction();
    insertRow();
    transaction.rollback();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 0);
}

BOOST_AUTO_TEST_CASE(test_transaction_auto_rollback)
{
    // Act
    {
        auto transaction = connection()->begin_transaction();
        insertRow();
        // unused on purpose, to see if rollback will happen when closing the current scope
        OES_UNUSED(transaction);
    }

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 0);
}

BOOST_AUTO_TEST_CASE(test_transaction_commit_on_different_schemas)
{
    // Arrange
    const auto defaultSchema = connection()->current_schema();
    const auto otherTestSchema = makeScopedSchema("ut_test_schema");
    createTestTable();

    // Act
    auto transaction = connection()->begin_transaction();
    insertRow();
    insertRow();
    connection()->set_current_schema(defaultSchema);
    insertRow();
    transaction.commit();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 1);
    connection()->set_current_schema(otherTestSchema);
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 2);
}

BOOST_AUTO_TEST_CASE(test_transaction_rollback_on_different_schemas)
{
    // Arrange
    const auto defaultSchema = connection()->current_schema();
    const auto otherTestSchema = makeScopedSchema("ut_test_schema");
    createTestTable();

    // Act
    auto transaction = connection()->begin_transaction();
    insertRow();
    insertRow();
    connection()->set_current_schema(defaultSchema);
    insertRow();
    transaction.rollback();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 0);
    connection()->set_current_schema(otherTestSchema);
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 0);
}

BOOST_AUTO_TEST_CASE(test_transaction_multi_nested)
{
    // Act
    auto transaction = connection()->begin_transaction();
    insertRow();
    {
        auto nestedTransaction1 = connection()->begin_transaction();
        insertRow();
        {
            auto nestedTransaction2 = connection()->begin_transaction();
            insertRow();
            nestedTransaction2.commit();
        }
        nestedTransaction1.commit();
    }
    transaction.commit();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 3);
}

BOOST_AUTO_TEST_CASE(test_transaction_rollback_when_nested_commit)
{
    // Act
    auto transaction = connection()->begin_transaction();
    insertRow();
    {
        auto nestedTransaction = connection()->begin_transaction();
        insertRow();
        nestedTransaction.commit();
    }
    insertRow();
    transaction.rollback();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 0);
}

BOOST_AUTO_TEST_CASE(test_transaction_commit_when_nested_rollback)
{
    // Act
    auto transaction = connection()->begin_transaction();
    insertRow();
    {
        auto nestedTransaction = connection()->begin_transaction();
        insertRow();
        nestedTransaction.rollback();
    }
    insertRow();
    transaction.commit();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 2);
}

BOOST_AUTO_TEST_CASE(test_transaction_nested_when_double_commit)
{
    // Act
    auto transaction = connection()->begin_transaction();
    {
        auto nestedTransaction = connection()->begin_transaction();
        insertRow();
        nestedTransaction.commit();
        insertRow();
        nestedTransaction.commit();   // Should no affect
    }
    transaction.rollback();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 0);
}

BOOST_AUTO_TEST_CASE(test_transaction_nested_when_double_rollback)
{
    // Act
    auto transaction = connection()->begin_transaction();
    {
        auto nestedTransaction = connection()->begin_transaction();
        insertRow();
        nestedTransaction.rollback();
        insertRow();
        nestedTransaction.rollback();   // Should no affect
    }
    transaction.commit();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 1);
}

BOOST_AUTO_TEST_CASE(test_transaction_in_read_only_mode)
{
    // Act
    insertRow();
    auto transaction = connection()->begin_transaction(true);
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 1);
    BOOST_CHECK_THROW(insertRow(), PostgreSQLException);    // Can't write in the read only mode
    transaction.commit();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 1);
}

BOOST_AUTO_TEST_CASE(test_transaction_move_constructor)
{
    // Act
    std::shared_ptr<Transaction> transaction;
    {
        auto transaction1 = connection()->begin_transaction();
        insertRow();
        transaction = std::make_shared<Transaction>(std::move(transaction1));
    }
    transaction->commit();

    // Assert
    BOOST_REQUIRE_EQUAL(table_row_count(testTable), 1);
}

/*****************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
