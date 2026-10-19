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
     * Data model of OSDU storage record.
     */
    struct StorageRecord
    {
        struct Acl
        {
            std::vector<std::string> owners;
            std::vector<std::string> viewers;
        };

        struct Data
        {
            std::map<std::string, std::string> datasetProperties;
        };

        struct Legal
        {
            std::vector<std::string> legaltags;
            std::vector<std::string> otherRelevantDataCountries;
            std::string Status;
        };

        std::string id;
        std::string kind;
        Acl acl;
        Data data;
        Legal legal;
    };


    /**
     * \brief OSDU storage API stub.
     * \warning Supposed that will be used with single-threaded HTTP server (multi-threading is not supported).
     */
    class StorageStub : public BaseRestRequestHandler
    {
    public:
        static constexpr char resource[] = "records";
        static constexpr char baseUrl[] = "/api/storage/v2";

        static constexpr char defaultStorageDir[] = "./OsduStubServer/";
        static constexpr char stateFileName[] = "StorageRecords.json";

        /**
         * \brief StorageStub constructor.
         * \param persistentStorage Pass `true` when need to store data in a file.
         * \param storageDir Directory where the state file is stored.
         */
        explicit StorageStub(bool persistentStorage, std::string_view storageDir = defaultStorageDir);

        bool getEntitiesList(HttpContext& httpContext) override;
        bool getEntityById(HttpContext& httpContext, const std::string& id) override;
        bool createEntity(HttpContext&) override { return false;}   // OSDU API uses only PUT for Create/Update storage records
        bool updateEntity(HttpContext& httpContext) override;
        bool deleteEntity(HttpContext& httpContext, const std::string& id) override;

        [[nodiscard]] const StorageRecord* getStorageRecord(std::string_view id) const;
        [[nodiscard]] StorageRecord* getStorageRecord(std::string_view id);
        [[nodiscard]] bool isExists(std::string_view id) const;

    private:
        bool loadFromFile();
        void saveToFile() const;

        std::vector<StorageRecord> storageRecords_;
        bool persistentStorage_ = false;
        std::filesystem::path stateFilePath_;
    };
}
