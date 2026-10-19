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

#pragma once
#include <filesystem>
#include <vector>
#include "http/HttpServerStub.h"

namespace oes::testing::osdu
{
    /**
     * Data model of OSDU entitlements group.
     */
    struct EntitlementsGroup
    {
        EntitlementsGroup() = default;
        EntitlementsGroup(std::string name, std::string description, std::string email)
            : name(std::move(name))
            , description(std::move(description))
            , email(std::move(email))
        { }

        std::string name;
        std::string description;
        std::string email;
    };


    /**
     * \brief OSDU entitlements API stub.
     * \warning Supposed that will be used with single-threaded HTTP server (multi-threading is not supported).
     */
    class EntitlementsGroupsStub : public BaseRestRequestHandler
    {
    public:
        static constexpr char resource[] = "groups";
        static constexpr char baseUrl[] = "/api/entitlements/v2/";

        static constexpr char defaultStorageDir[] = "./OsduStubServer/";
        static constexpr char stateFileName[] = "EntitlementsGroups.json";

        static constexpr char serviceName[] = "reservoir-dms";
        static constexpr char testDomain[] = "contoso.com";
        static constexpr char testUser[] = "john_doe@contoso.com";

        /**
         * \brief EntitlementsGroupsStub constructor.
         * \param persistentStorage Pass `true` when need to store data in a file.
         * \param storageDir Directory where the state file is stored.
         */
        explicit EntitlementsGroupsStub(bool persistentStorage, std::string_view storageDir = defaultStorageDir);

        EntitlementsGroupsStub& withReservoirServiceOwnerGroup(const std::string& partitionId, const std::string& domain = testDomain);
        EntitlementsGroupsStub& withReservoirServiceViewerGroup(const std::string& partitionId, const std::string& domain = testDomain);

        bool getEntitiesList(HttpContext& httpContext) override;
        bool getEntityById(HttpContext&, const std::string&) override { return false; }
        bool createEntity(HttpContext& httpContext) override;
        bool updateEntity(HttpContext&) override { return false; }
        bool deleteEntity(HttpContext&, const std::string&) override { return false; }

        [[nodiscard]] const EntitlementsGroup* getEntitlementsGroup(const std::string& memberEmail) const;
        [[nodiscard]] bool isExists(const std::string& memberEmail) const;

    private:
        bool loadFromFile();
        void saveToFile() const;

        std::vector<EntitlementsGroup> entitlementsGroups_;
        bool persistentStorage_ = false;
        std::filesystem::path stateFilePath_;
    };
}
