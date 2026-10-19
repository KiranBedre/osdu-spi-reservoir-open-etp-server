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
#include <fstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "EntitlementsStub.h"


namespace oes::testing::osdu
{
    void to_json(nlohmann::json& j, const EntitlementsGroup& obj)
    {
        j = nlohmann::ordered_json{
            {"name", obj.name},
            {"description", obj.description},
            {"email", obj.email}
        };
    }

    void from_json(const nlohmann::json& j, EntitlementsGroup& obj)
    {
        j.at("name").get_to(obj.name);
        j.at("description").get_to(obj.description);
        j.at("email").get_to(obj.email);
    }

    // ============================================================================

    EntitlementsGroupsStub::EntitlementsGroupsStub(bool persistentStorage, std::string_view storageDir)
        : BaseRestRequestHandler(resource, baseUrl)
        , persistentStorage_(persistentStorage)
        , stateFilePath_(std::filesystem::path(storageDir) / stateFileName)
    {
        if (persistentStorage_ && exists(stateFilePath_) && !loadFromFile())
        {
            std::cerr << "Failed to load state file with EntitlementsGroups" << std::endl;
        }
    }

    EntitlementsGroupsStub& EntitlementsGroupsStub::withReservoirServiceOwnerGroup(const std::string& partitionId, const std::string& domain)
    {
        const std::string email = fmt::format("service.{}.viewers@{}.{}", serviceName, partitionId, domain);
        if (!isExists(email))
        {
            entitlementsGroups_.emplace_back("service.reservoir.user", "Reservoir Legal owners", email);
            saveToFile();
        }
        return *this;
    }

    EntitlementsGroupsStub& EntitlementsGroupsStub::withReservoirServiceViewerGroup(const std::string& partitionId, const std::string& domain)
    {
        const std::string email = fmt::format("service.{}.owners@{}.{}", serviceName, partitionId, domain);
        if (!isExists(email))
        {
            entitlementsGroups_.emplace_back("service.reservoir.user", "Reservoir Legal viewers", email);
            saveToFile();
        }
        return *this;
    }

    bool EntitlementsGroupsStub::getEntitiesList(HttpContext& httpContext)
    {
        auto jsonRoot = nlohmann::ordered_json::object();
        jsonRoot["desId"] = testUser;
        jsonRoot["memberEmail"] = testUser;
        jsonRoot["groups"] = nlohmann::json(entitlementsGroups_);

        httpContext.response.body = jsonRoot.dump(4);
        httpContext.response.statusCode = 200;

        return true;
    }

    bool EntitlementsGroupsStub::createEntity(HttpContext& httpContext)
    {
        try
        {
            const auto& partitionIt = httpContext.request.headers.find("data-partition-id");
            if (partitionIt == httpContext.request.headers.cend())
            {
                httpContext.response.statusCode = 403;
                return true;
            }

            nlohmann::json jsonObj = nlohmann::json::parse(httpContext.request.body);
            if (jsonObj.is_object())
            {
                const std::string name = jsonObj["name"];
                const std::string email = fmt::format("{}@{}.{}", name, partitionIt->second, testDomain);

                // Create data group if not exists
                const EntitlementsGroup* existingGroup = getEntitlementsGroup(email);
                if (existingGroup == nullptr)
                {
                    EntitlementsGroup group;
                    group.name = jsonObj["name"];
                    group.description = name;
                    group.email = email;

                    // Prepare response
                    const nlohmann::json jResponse = group;
                    httpContext.response.body = jResponse.dump(4);
                    httpContext.response.statusCode = 201;
                    entitlementsGroups_.push_back(std::move(group));

                    // Save state
                    saveToFile();
                }
                else {
                    httpContext.response.statusCode = 409;
                }
            }
            else {
                httpContext.response.statusCode = 403;
            }
        }
        catch (const std::exception& ex) {
            httpContext.response.statusCode = 403;
            std::cerr << "Failed to create entitlement group: " << ex.what() << std::endl;
        }
        catch (...) {
            httpContext.response.statusCode = 403;
            std::cerr << "Failed to create entitlement group: " << std::endl;
        }
        return true;
    }

    const EntitlementsGroup* EntitlementsGroupsStub::getEntitlementsGroup(const std::string& email) const
    {
        const auto it = std::find_if(entitlementsGroups_.cbegin(), entitlementsGroups_.cend(), [&email](const EntitlementsGroup& item) {
            return email == item.email;
        });
        return it == entitlementsGroups_.cend() ? nullptr : &(*it);
    }

    bool EntitlementsGroupsStub::isExists(const std::string& email) const
    {
        return getEntitlementsGroup(email) != nullptr;
    }

    bool EntitlementsGroupsStub::loadFromFile()
    {
        std::fstream stream;
        stream.open(stateFilePath_, std::ios::in | std::ios::binary);
        if (stream.is_open())
        {
            const std::string str((std::istreambuf_iterator(stream)), std::istreambuf_iterator<char>());
            try
            {
                const nlohmann::json jsonRoot = nlohmann::json::parse(str);
                std::vector<EntitlementsGroup> entitlementsGroups = jsonRoot;
                std::swap(entitlementsGroups_, entitlementsGroups);
                return true;
            }
            catch (...) { }
        }
        return false;
    }

    void EntitlementsGroupsStub::saveToFile() const
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
            const nlohmann::json json = entitlementsGroups_;
            stream << json.dump(4);
            stream.close();
            return;
        }

        std::cerr << "EntitlementsGroupsStub: Failed to save state to file: " << absolute(stateFilePath_) << std::endl;
    }
}
