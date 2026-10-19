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

#include "PostgreSQLTestsFixture.h"

#include <oes/postgresql/Format.hpp>

#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/Json.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/TestConfig.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <stdexcept>

using namespace oes::core;
using namespace oes::postgresql::relational;
using namespace oes::postgresql;

namespace {

using oes::testing::postgresql::UserCredentials;

class JSONPostgreSQLTestHandler : public oes::core::JSONHandler{
public:
    explicit JSONPostgreSQLTestHandler(UserCredentials& cred)
        : oes::core::JSONHandler(true), cred_(cred)
    {
    }

    bool handle_object_begin() override {
        return true;
    }
    bool handle_object_key(const std::string& key) override {
        cur_key_ = key;
        return true;
    }
    bool handle_string(const std::string& value) override {
        if (cur_key_ == "user") {
            cred_.user = PhysicalName::parse(value, IMPLICIT_LOWERCASE);
        }
        else if (cur_key_ == "password") {
            cred_.password = value;
        }
        else if (cur_key_ == "host") {
            cred_.host = value;
        }
        else if (cur_key_ == "port") {
            cred_.port = value;
        }
        else if (cur_key_ == "dbname") {
            cred_.dbname = value;
        }
        return true;
    }

    bool handle_number(int value) override {
        if (cur_key_ == "port") {
            cred_.port = std::to_string(value);
        }
        return true;
    }

    bool handle_object_end() override {
        return true;
    }

    JSONPostgreSQLTestHandler& operator=(const JSONPostgreSQLTestHandler&) = delete;

private:
    std::string cur_key_;
    UserCredentials& cred_;
};

struct UTConfig {
    UTConfig() {
        // Find out where to load the config file from.
        const String envFilename = oes::testing::TestConfig::make()->oesPostgresqlUtconfig();
        String filename;
        if (!envFilename.empty()) {
            // allow setting OES_POSTGRESQL_UTCONFIG to 0 or false or off
            // to explicitly (and silently) disable PostgreSQL-related tests.
            bool enabled = StringUtils::toBool(envFilename, true);
            if (!enabled) {
                enabled_ = false;
                static bool warned = false;
                if (!warned) {
                    OES_WARNING("SKIPPING ALL PostgreSQL-related tests");
                    warned = true;
                }
                return;
            }
            filename = envFilename;
        } else {
            // no env. var., so use the default location instead.
            filename = oes::testing::resolveTestDataFile(
                "data/postgresql", "postgresql_test_config.json", true
            );
        }
        OES_ASSERT(!filename.empty(), return;);

        std::vector<char> buffer;
        BOOST_REQUIRE(oes::testing::readFile(filename, buffer));

        JSONPostgreSQLTestHandler handler(credentials_);
        //JSONReader reader(buffer.data(), buffer.size(), handler);
        if (!oes::core::json_parse(buffer.data(), buffer.size(), handler)) {
            throw std::runtime_error("Wrong JSON format");
        }
        enabled_ = true;
    }

    bool enabled_ = false;
    UserCredentials credentials_;

    UTConfig(const UTConfig&) = delete;
    UTConfig& operator=(const UTConfig&) = delete;
};

//*****************************************************************************

} // namespace

namespace oes::testing::postgresql {

//*****************************************************************************

std::string UserCredentials::toHostPortDb() const {
    return host + (port.empty() ? "" : ":" + port) + "/" + dbname;
}

std::string UserCredentials::toConnectionURI() const {
    //return "postgresql://" + user.sqlText() + ":" + password + "@" + toHostPortDb();
    return "host="+host+" port="+port+" dbname="+dbname+" user="+user.sqlText()+" password="+password;
}

//*****************************************************************************
ScopedSchema::ScopedSchema(Connection_ptr conn, PhysicalName schema, bool switchToSchema)
    : conn_(std::move(conn))
    , schema_(std::move(schema))
{
    BOOST_REQUIRE(exec_ddl(*conn_,
        "CREATE SCHEMA IF NOT EXISTS {}",
        schema_.sqlName()
    ));

    if (switchToSchema) {
        setAsCurrent();
    }
}

ScopedSchema::ScopedSchema(Connection_ptr conn, const std::string& schemaName, bool switchToSchema)
    : ScopedSchema(std::move(conn), PhysicalName(schemaName, IMPLICIT_LOWERCASE), switchToSchema)
{ }

ScopedSchema::~ScopedSchema()
{
    BOOST_REQUIRE(exec_ddl(*conn_,
        "DROP SCHEMA IF EXISTS {} CASCADE",
        schema_.sqlName()
    ));
}

ScopedSchema::operator const PhysicalName&() const {
    return schema_;
}

void ScopedSchema::setAsCurrent() const {
    conn_->set_current_schema(schema_);
}

//*****************************************************************************

PQLibTestsFixture::PQLibTestsFixture() {
    UTConfig conf;

    if (!conf.enabled_) {
        pq_tests_are_disabled_ = true;
        return; // silently do nothing
    }

    master_creds_ = conf.credentials_;

    BOOST_REQUIRE(!master_creds_.user.empty());
    BOOST_REQUIRE(!master_creds_.password.empty());
    BOOST_REQUIRE(!master_creds_.host.empty());
    BOOST_REQUIRE(!master_creds_.dbname.empty());

    connection_string_ = master_creds_.toConnectionURI();

    PGconn_uptr c(PQconnectdb(connection_string_.c_str()));
    connection_ = std::make_shared<Connection>(std::move(c));
}

PQLibTestsFixture::~PQLibTestsFixture() {
    for (const auto& user : created_users_) {
        // needed before the drop
        std::string revoke = "REVOKE ALL ON SCHEMA " + master_creds_.user.sqlText() + " FROM " + user;
        connection_->execute(revoke);

        std::string revoke_db = "REVOKE ALL ON DATABASE " + master_creds_.dbname + " FROM " + user;
        connection_->execute(revoke_db);

        connection_->execute("DROP USER " + user);
    }
}

ScopedSchema PQLibTestsFixture::makeScopedSchema(PhysicalName schema, bool switchToSchema) {
    return { connection_, std::move(schema), switchToSchema };
}

ScopedSchema PQLibTestsFixture::makeScopedSchema(const std::string& schemaName, bool switchToSchema) {
    return { connection_, schemaName, switchToSchema };
}

//*****************************************************************************

const UserCredentials& PQLibTestsFixture::masterUserCredentials() const {
    return master_creds_;
}

std::string PQLibTestsFixture::createUser(
    const std::string& short_name,
    const std::string& schema,
    const char* prefix
) {
    // Convert names to upper-case
    std::string name = prefix + short_name;

    std::string db(PQdb(connection_->pq_connection()));

    // needed before the drop
    { // may fail if user does not exist
        auto mt_guard = connection_->make_multi_threads_guard();
        std::string revoke = "REVOKE ALL ON SCHEMA " + schema + " FROM " + name;
        PGresult_uptr res_revoke(PQexec(connection_->pq_connection(), revoke.c_str())); // cppcheck-suppress unreadVariable

        std::string revoke_db = "REVOKE ALL ON DATABASE " + db + " FROM " + name;
        PGresult_uptr res_revoke_db(PQexec(connection_->pq_connection(), revoke_db.c_str())); // cppcheck-suppress unreadVariable
    }
    std::string drop = "DROP USER IF EXISTS " + name;
    connection_->execute(drop);

    // Create the project user. (all in default tablespace USERS for now)

    std::string sql =
        "CREATE USER " + name + " WITH LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE INHERIT NOREPLICATION CONNECTION LIMIT -1 PASSWORD '" + name + "'";
    connection_->execute(sql);

    created_users_.push_back(name);

    std::string grant_db = "GRANT CONNECT ON DATABASE " + db + " TO " + name;
    connection_->execute(grant_db);

    std::string grant = "GRANT ALL ON SCHEMA " + schema + " TO " + name;
    connection_->execute(grant);

    return name;
}

Connection_ptr PQLibTestsFixture::connection() {
    return connection_;
}

int PQLibTestsFixture::table_row_count(const char* name) const {
    return exec_scalar_select_int(*connection_, R"sql(select count(*)::int4 from "{}")sql", name);
}

} // oes::testing::postgresql
