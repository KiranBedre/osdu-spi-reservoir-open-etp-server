// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2022 EPAM Systems
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

#include "OsduOpenKVClientServerFixture.h"

#include <oes/testing/core/TestMain.h>

#include <oes/eml/openkv/AdminDB.h>
#include <oes/postgresql/DbConnectionPool.h>


namespace oes::testing::eml {
    OsduOpenKVClientServerFixture::OsduOpenKVClientServerFixture() {}
    OsduOpenKVClientServerFixture::~OsduOpenKVClientServerFixture() {
        stopServer();
        OsduOpenKVClientServerFixture::clearImports();
        OsduOpenKVClientServerFixture::clearAdmin();
    }

    void OsduOpenKVClientServerFixture::initServer() {
        BOOST_REQUIRE(!init_done_);
        const char* connection_string = std::getenv("POSTGRESQL_CONN_STRING");
        BOOST_CHECK(connection_string != nullptr);
        const std::string connection_str(connection_string);
        BOOST_CHECK(!connection_str.empty());

        constexpr const char* partitions[] {
            "osdu_test_data_partition",
            "opendes_test_data_partition"
        };

        for (const auto& partitionName: partitions)
        {
            const bool isRegistered = connection_pool_->registerDatabase(partitionName, connection_str, true);
            BOOST_CHECK_MESSAGE(isRegistered, "Error: Can't connect to DB");

            auto dbConnection = connection_pool_->tryAcquireConnection(partitionName);
            BOOST_CHECK_MESSAGE(dbConnection, "Error: Can't acquire DB connection");
            const auto admin_db = std::make_shared<oes::eml::openkv::OpenKVAdminDB>(dbConnection);

            bool schema_ok = false;
            if (!admin_db->isSchemaCreated()) {
                schema_ok = admin_db->createSchema();
            } else {
                schema_ok = true;
            }
            BOOST_REQUIRE(schema_ok);

            if (create_default_space_) {
                oes::eml::openkv::Space space;
                space.uid_.clear(); // No uid
                space.path_.clear(); // No name either
                space.dbfile_ = "OSDUDefaultSpace";

                const bool space_ok = admin_db->addSpaces({ &space, &space + 1 });
                BOOST_REQUIRE(space_ok);
            }
        }

        init_done_ = true;
    }

    void OsduOpenKVClientServerFixture::clearImports()
    {
        for (auto const& connectionElement : connection_pool_->getRegisteredDatabases())
        {
            const oes::postgresql::relational::PhysicalName phys(
                    "OSDUDefaultSpace",
                    oes::postgresql::relational::IMPLICIT_LOWERCASE
            );
            const auto dbConnection = connection_pool_->tryAcquireConnection(connectionElement.first);
            dbConnection->execute(fmt::format("DROP SCHEMA IF EXISTS {} CASCADE", phys.quotedName()));
        }
    }

    void OsduOpenKVClientServerFixture::clearAdmin()
    {
        for (auto const& connectionElement : connection_pool_->getRegisteredDatabases())
        {
            const auto dbConnection = connection_pool_->tryAcquireConnection(connectionElement.first);
            if (dbConnection) {
                const auto adminDb = std::make_shared<oes::eml::openkv::OpenKVAdminDB>(dbConnection);
                adminDb->dropAdmin();
            }
        }
    }

    oes::client::ClientConfig& OsduOpenKVClientServerFixture::initClientConfig(
        const std::string& data_partition_id
    ) {
        OpenKVClientServerFixture::initClientConfig();
        clt_conf_.data_partition_id_hdr_ = data_partition_id;
        return clt_conf_;
    }

    Client_uptr OsduOpenKVClientServerFixture::connectClient(const std::string& data_partition_id) {
        auto& conf = initClientConfig(data_partition_id);
        BOOST_REQUIRE_EQUAL(&conf, &clt_conf_);
        Client_uptr clt(new oes::client::Client(clt_conf_));
        const bool clt_ok = clt->connect(std::chrono::milliseconds(5000));
        BOOST_REQUIRE(clt_ok);
        return clt;
    }
}
