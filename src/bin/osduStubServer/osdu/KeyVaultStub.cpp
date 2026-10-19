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

#include "KeyVaultStub.h"


namespace oes::testing::osdu
{
    KeyVaultStub::KeyVaultStub(std::string_view resourceName)
        : BaseRestRequestHandler(resourceName)
    { }

    void KeyVaultStub::withSecrets(std::initializer_list<std::pair<const std::string, std::string>> secrets)
    {
        secrets_.insert(secrets);
    }
    
    void KeyVaultStub::withSecrets(std::vector<std::pair<const std::string, std::string>> secrets)
    {
        secrets_.insert(secrets.begin(), secrets.end());
    }

    bool KeyVaultStub::getEntityById(HttpContext& httpContext, const std::string& id)
    {
        const auto it = secrets_.find(id);
        if (it != secrets_.cend())
        {
            auto jsonObject = nlohmann::json::object();
            jsonObject["value"] = it->second;

            httpContext.response.body = jsonObject.dump(4);
            httpContext.response.statusCode = 200;
        }
        else
        {
            httpContext.response.body = fmt::format("Not found secret with Id '{}'", id);
        }
        return true;
    }
}
