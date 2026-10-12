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
#include <map>
#include "http/HttpServerStub.h"

namespace oes::testing::osdu
{
    /**
     * Key vault service stub.
     */
    class KeyVaultStub : public BaseRestRequestHandler
    {
    public:
        static constexpr char defaultSecretsResource[] = "secrets";

        explicit KeyVaultStub(std::string_view resourceName = defaultSecretsResource);

        void withSecrets(std::initializer_list<std::pair<const std::string, std::string>> secrets);
        void withSecrets(std::vector<std::pair<const std::string, std::string>> secrets);

        bool getEntitiesList(HttpContext&) override { return false; }
        bool getEntityById(HttpContext& httpContext, const std::string& id) override;
        bool createEntity(HttpContext&) override { return false; }
        bool updateEntity(HttpContext&) override { return false; }
        bool deleteEntity(HttpContext&, const std::string&) override { return false; }

    private:
        std::string_view resourceName_;
        std::map<std::string, std::string> secrets_;
    };
}
