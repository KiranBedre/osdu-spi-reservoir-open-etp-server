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

#include "StorageStub.h"

#include <iostream>
#include <fstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>


namespace oes::testing::osdu
{
    void to_json(nlohmann::json& j, const StorageRecord::Acl& obj)
    {
        j = nlohmann::ordered_json{
            {"owners", obj.owners},
            {"viewers", obj.viewers}
        };
    }

    void from_json(const nlohmann::json& j, StorageRecord::Acl& obj)
    {
        j.at("owners").get_to(obj.owners);
        j.at("viewers").get_to(obj.viewers);
    }

    // ============================================================================

    void to_json(nlohmann::json& j, const StorageRecord::Data& obj)
    {
        j = nlohmann::ordered_json{
            {"DatasetProperties", obj.datasetProperties},
        };
    }

    void from_json(const nlohmann::json& j, StorageRecord::Data& obj)
    {
        j.at("DatasetProperties").get_to(obj.datasetProperties);
    }

    // ============================================================================

    void to_json(nlohmann::json& j, const StorageRecord::Legal& obj)
    {
        j = nlohmann::ordered_json{
            {"legaltags", obj.legaltags},
            {"otherRelevantDataCountries", obj.otherRelevantDataCountries},
            {"status", obj.Status},
        };
    }

    void from_json(const nlohmann::json& j, StorageRecord::Legal& obj)
    {
        j.at("legaltags").get_to(obj.legaltags);
        j.at("otherRelevantDataCountries").get_to(obj.otherRelevantDataCountries);
        j.at("status").get_to(obj.Status);
    }

    // ============================================================================

    void to_json(nlohmann::json& j, const StorageRecord& obj)
    {
        j = nlohmann::json{
            {"id", obj.id},
            {"kind", obj.kind},
            {"acl", obj.acl},
            {"data", obj.data},
            {"legal", obj.legal}
        };
    }

    void from_json(const nlohmann::json& j, StorageRecord& obj)
    {
        j.at("id").get_to(obj.id);
        j.at("kind").get_to(obj.kind);
        j.at("acl").get_to(obj.acl);
        j.at("data").get_to(obj.data);
        j.at("legal").get_to(obj.legal);
    }

    // ============================================================================

    StorageStub::StorageStub(bool persistentStorage, std::string_view storageDir)
        : BaseRestRequestHandler(resource, baseUrl)
        , persistentStorage_(persistentStorage)
        , stateFilePath_(std::filesystem::path(storageDir) / stateFileName)
    {
        if (persistentStorage_ && exists(stateFilePath_) && !loadFromFile())
        {
            std::cerr << "Failed to load state file with StorageRecords" << std::endl;
        }
    }

    bool StorageStub::getEntitiesList(HttpContext& httpContext)
    {
        // Single page, no limit check: the cursor is always null
        const auto kindIt = httpContext.request.queryArgs.find("kind");
        nlohmann::json results = nlohmann::json::array();
        for (const auto& storageRecord : storageRecords_)
        {
            if (kindIt == httpContext.request.queryArgs.cend() || kindIt->second == storageRecord.kind)
            {
                results.push_back(storageRecord);
            }
        }
        const nlohmann::json jsonRoot = {{"cursor", nullptr}, {"results", std::move(results)}};
        httpContext.response.body = jsonRoot.dump(4);
        httpContext.response.statusCode = 200;
        return true;
    }

    bool StorageStub::getEntityById(HttpContext& httpContext, const std::string& id)
    {
        const StorageRecord* storageRecord = getStorageRecord(id);
        if (storageRecord)
        {
            const auto jsonRoot = nlohmann::json(*storageRecord);
            httpContext.response.body = jsonRoot.dump(4);
            httpContext.response.statusCode = 200;
            return true;
        }
        return false;
    }

    bool StorageStub::updateEntity(HttpContext& httpContext)
    {
        // Note: OSDU API uses only PUT for Create AND Update storage records
        try
        {
            const auto& partitionIt = httpContext.request.headers.find("data-partition-id");
            if (partitionIt == httpContext.request.headers.cend())
            {
                httpContext.response.statusCode = 403;
                return true;
            }

            const nlohmann::json jsonObj = nlohmann::json::parse(httpContext.request.body);
            std::vector<StorageRecord> storageRecords = jsonObj.get<std::vector<StorageRecord>>();
            for (auto& storageRecord : storageRecords)
            {
                // Update or create storage record if not exists
                StorageRecord* existingRecord = getStorageRecord(storageRecord.id);
                if (existingRecord == nullptr)
                {
                    storageRecords_.push_back(std::move(storageRecord));
                    httpContext.response.statusCode = 201;
                }
                else
                {
                    *existingRecord = std::move(storageRecord);
                    httpContext.response.statusCode = 200;
                }
            }

            // Prepare response
            httpContext.response.body = httpContext.request.body;

            // Save state
            saveToFile();
        }
        catch (const std::exception& ex) {
            httpContext.response.statusCode = 403;
            std::cerr << "Failed to create/update storage record: " << ex.what() << std::endl;
        }
        catch (...) {
            httpContext.response.statusCode = 403;
            std::cerr << "Failed to create/update storage record: " << std::endl;
        }
        return true;
    }

    bool StorageStub::deleteEntity(HttpContext& httpContext, const std::string& id)
    {
        const auto it = std::find_if(storageRecords_.cbegin(), storageRecords_.cend(), [id](const StorageRecord& item) {
            return id == item.id;
        });
        if (it != storageRecords_.cend())
        {
            storageRecords_.erase(it);
            httpContext.response.statusCode = 204;

            // Save state
            saveToFile();
        }
        else {
            httpContext.response.statusCode = 200;
        }
        return true;
    }

    const StorageRecord* StorageStub::getStorageRecord(std::string_view id) const
    {
        const auto it = std::find_if(storageRecords_.cbegin(), storageRecords_.cend(), [id](const StorageRecord& item) {
            return id == item.id;
        });
        return it == storageRecords_.cend() ? nullptr : &(*it);
    }

    StorageRecord* StorageStub::getStorageRecord(std::string_view id)
    {
        const auto it = std::find_if(storageRecords_.begin(), storageRecords_.end(), [id](const StorageRecord& item) {
            return id == item.id;
        });
        return it == storageRecords_.end() ? nullptr : &(*it);
    }

    bool StorageStub::isExists(std::string_view id) const
    {
        return getStorageRecord(id) != nullptr;
    }

    bool StorageStub::loadFromFile()
    {
        std::fstream stream;
        stream.open(stateFilePath_, std::ios::in | std::ios::binary);
        if (stream.is_open())
        {
            const std::string str((std::istreambuf_iterator(stream)), std::istreambuf_iterator<char>());
            try
            {
                const nlohmann::json jsonRoot = nlohmann::json::parse(str);
                std::vector<StorageRecord> storageRecords = jsonRoot;
                std::swap(storageRecords_, storageRecords);
                return true;
            }
            catch (...) { }
        }
        return false;
    }

    void StorageStub::saveToFile() const
    {
        if (!persistentStorage_) {
            return;
        }

        const std::filesystem::path dir = stateFilePath_.parent_path();
        if (!exists(dir)) {
            create_directory(dir);
        }

        std::ofstream stream;
        stream.open(stateFilePath_, std::ios::out | std::ios::binary);
        if (stream.is_open())
        {
            const nlohmann::json json = storageRecords_;
            stream << json.dump(4);
            stream.close();
            return;
        }

        std::cerr << "StorageStub: Failed to save state to file: " << absolute(stateFilePath_) << std::endl;
    }
}
