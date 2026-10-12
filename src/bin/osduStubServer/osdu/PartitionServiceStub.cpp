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
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "PartitionServiceStub.h"


namespace oes::testing::osdu
{
    static constexpr char reservoirConnectionIdSuffix[] = "-reservoir-conn";

    PartitionServiceStub::PartitionServiceStub()
        : BaseRestRequestHandler(resource)
    { }

    void PartitionServiceStub::withPartitions(std::initializer_list<std::string> partitions)
    {
        partitions_.insert(partitions_.end(), partitions);
    }
    
    void PartitionServiceStub::withPartitions(std::vector<std::string> partitions)
    {
        partitions_.insert(partitions_.end(), partitions.begin(), partitions.end());
    }

    std::string PartitionServiceStub::getConnectionIdForPartition(const std::string& partition)
    {
        return partition + reservoirConnectionIdSuffix;
    }

    bool PartitionServiceStub::getEntitiesList(HttpContext& httpContext)
    {
        auto jsonArray = nlohmann::json::array();
        for (const auto& partition : partitions_)
        {
            jsonArray.push_back(partition);
        }

        httpContext.response.body = jsonArray.dump();
        httpContext.response.statusCode = 200;
        return true;
    }

    bool PartitionServiceStub::getEntityById(HttpContext& httpContext, const std::string& id)
    {
        const auto it = std::find(partitions_.cbegin(), partitions_.cend(), id);
        if (it != partitions_.cend())
        {
            auto jsonObject = nlohmann::json::object();
            jsonObject["reservoir-connection"] = { {"sensitive", true}, {"value", getConnectionIdForPartition(id)} };

            httpContext.response.body = jsonObject.dump(4);
            httpContext.response.statusCode = 200;
        }
        else
        {
            httpContext.response.body = fmt::format("Not found partition with Id '{}'", id);
        }
        return true;
    }
}
