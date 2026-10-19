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

#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>
#include <string>
#include <initializer_list>

#include "http/HttpServerStub.h"
#include "osdu/PartitionServiceStub.h"
#include "osdu/KeyVaultStub.h"
#include "osdu/EntitlementsStub.h"
#include "osdu/StorageStub.h"
#include "osduStubServerConfig.h"


//static constexpr char localPartition[] = "local";
static constexpr char osduStateDir[] = "./OsduStubServer/";
std::atomic_bool exitFlag = false;

int main(int, char*[])
{
    std::cout << "-= OSDU stub server =-" << std::endl;

    auto config = oes::testing::osduStubServerConfig::make();
    // Check requirements
    std::string connection_string = config->standaloneConnectionStrings();
    if (connection_string.empty())
    {
        std::cerr << "Environment variable 'POSTGRESQL_CONN_STRING' should contain connection string to PostgreSQL" << std::endl;
        return 1;
    }

    // Initialize HTTP server
    oes::testing::HttpServerStub server(config->stubPort());
#ifdef _DEBUG
    server.emplaceRequestHandler<oes::testing::DebugPrintRequestHandler>();
#endif
    auto partitions_ids = config->partitionStubIds();
    // Initialize OSDU service stubs
    //!server.emplaceRequestHandler<oes::testing::osdu::PartitionServiceStub>()
    //!    .withPartitions({ localPartition });
    server.emplaceRequestHandler<oes::testing::osdu::PartitionServiceStub>()
        .withPartitions(partitions_ids);

    std::vector<std::pair<const std::string, std::string>> secrets {
        { "app-dev-sp-username", "test-user"},
        { "app-dev-sp-password", "test-password"},
        { "app-dev-sp-tenant-id", "test-tenant-id"},
        { "aad-client-id", "test-scope-id"}
        };
    

    for (const auto& part : partitions_ids) {
        const std::string secretConnectionId = oes::testing::osdu::PartitionServiceStub::getConnectionIdForPartition(part);
        secrets.push_back({secretConnectionId, connection_string});
    }

    server.emplaceRequestHandler<oes::testing::osdu::KeyVaultStub>()
        .withSecrets(secrets);

    server.addRequestHandlerMock()
        .withHttpMethod("GET")
        .withUrlPath("/identity/oauth2/token")
        .willReturn(R"({"access_token": "jwt token"})");

    for (const auto& part : partitions_ids ){

        server.emplaceRequestHandler<oes::testing::osdu::EntitlementsGroupsStub>(true, osduStateDir)
            .withReservoirServiceOwnerGroup(part)
            .withReservoirServiceViewerGroup(part);

    }
    server.emplaceRequestHandler<oes::testing::osdu::StorageStub>(true, osduStateDir);

    // Start server
    if (server.start())
    {
        std::cout << "Listening HTTP on port " << server.getPort() << std::endl;

        // Main loop
        auto sigHandler = [](int) { exitFlag = true; };
        std::signal(SIGINT, sigHandler);
#ifdef SIGBREAK
        std::signal(SIGBREAK, sigHandler);
#endif
        std::signal(SIGABRT, sigHandler);
        std::signal(SIGTERM, sigHandler);
        while (!exitFlag)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Stop server
        server.stop();
        std::cout << "HTTP server has been stopped" << std::endl;
    }

    return 0;
}
