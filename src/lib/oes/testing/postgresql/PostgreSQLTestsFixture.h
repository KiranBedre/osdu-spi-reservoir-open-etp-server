// ============================================================================
// Copyright 2017-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef POSTGRESQL_TEST_FIXTURE__H
#define POSTGRESQL_TEST_FIXTURE__H

#include <oes/testing/postgresql/export.h>

#include <oes/postgresql/fwd.h>
#include <oes/postgresql/Connection.h>
#include <oes/postgresql/PhysicalName.h>


namespace oes::testing::postgresql {

struct OES_TESTING_POSTGRESQL_EXPORT UserCredentials {
    oes::postgresql::relational::PhysicalName user;
    std::string password;
    std::string host;
    std::string port;
    std::string dbname;

    std::string toHostPortDb() const;
    std::string toConnectionURI() const;
};

//*****************************************************************************

struct OES_TESTING_POSTGRESQL_EXPORT ScopedSchema
{
    ScopedSchema(oes::postgresql::Connection_ptr conn, oes::postgresql::PhysicalName schema, bool switchToSchema = true);
    ScopedSchema(oes::postgresql::Connection_ptr conn, const std::string& schemaName, bool switchToSchema = true);
    ~ScopedSchema();

    ScopedSchema(const ScopedSchema&) = delete;
    ScopedSchema(ScopedSchema&&) = delete;
    ScopedSchema& operator=(const ScopedSchema&) = delete;
    ScopedSchema& operator=(ScopedSchema&&) = delete;

    operator const oes::postgresql::PhysicalName& () const;

    void setAsCurrent() const;

    oes::postgresql::Connection_ptr conn_;
    oes::postgresql::PhysicalName schema_;
};

//*****************************************************************************

class OES_TESTING_POSTGRESQL_EXPORT PQLibTestsFixture {
public:
    PQLibTestsFixture();
    virtual ~PQLibTestsFixture();

    const UserCredentials& masterUserCredentials() const;

    std::string createUser(
        const std::string& short_name,
        const std::string& schema,
        const char* prefix = "sec_"
    );

    oes::postgresql::Connection_ptr connection();

    int table_row_count(const char* name) const;

    ScopedSchema makeScopedSchema(oes::postgresql::PhysicalName schema, bool switchToSchema = true);
    ScopedSchema makeScopedSchema(const std::string& schemaName, bool switchToSchema = true);

    bool pq_tests_are_disabled_ = false;

    UserCredentials master_creds_;
    std::string connection_string_;
    oes::postgresql::Connection_ptr connection_;
    std::vector<std::string> created_users_;
};

// Use this macro in test exes that have a mix of tests,
// some dependent on PostgreSQL, run only if configured,
// while others independent of it, to always be executed.
#define OES_SKIP_POSTGRESQL_TEST_IF_DISABLED \
    if (pq_tests_are_disabled_) \
        return // no ; on purpose

} // namespace oes::testing::postgresql

#endif
