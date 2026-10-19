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

#include "SecretsProviderAzure.h"
#include "Utils.h"

#include <oes/core/http/HTTP.h>
#include <oes/providers/core/TokenProviderFactory.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>

namespace oes::providers::azure {

    SecretsProviderAzure::SecretsProviderAzure(const oes::providers::core::ProvidersConfig& app_config) :
        oes::providers::core::SecretsProvider(app_config),
        keyVaultUri_(app_config.keyVaultUrl()),
        postgresMsiEnabled_(app_config.azureEnablePostgresMsi()),
        keyVaultScope_(app_config.azureKeyVaultAccessTokenScope()),
        postgresScope_(app_config.azurePostgresAccessTokenScope()) {
    }

    std::string
    SecretsProviderAzure::getSecret(const std::string& secretName) const {
        // Ideally this should make use of the Azure SecretsClient instead of manually implementing the logic,
        // but the Azure SecretsClient will not allow http:// endpoints, which breaks compatibility with the OSDU stub server!
        if (keyVaultUri_.empty()) {
            throw std::runtime_error("\n No KeyVaultUri defined\n");
        }

        std::string url;
        oes::core::HTTP request;
        oes::core::HTTP::Response response;

        std::string token = MsiCredentialProvider::instance().getAccessToken(keyVaultScope_);
        if(token.empty()){
            // For local environment testing, use service principal instead of managed identity
            token = oes::providers::core::TokenProviderFactory::GetBearerTokenProvider(app_config_)->GetBearerToken(app_config_);
        } 
        std::vector<std::string> headers{
            "Accept: application/json",
            "Authorization: Bearer " + token};

        url = keyVaultUri_ + "/secrets/" + secretName + "?api-version=7.2";

        const size_t unusedPort = 0;
        response = request.get(url, unusedPort, headers);

        // Enable below code for debugging purpose
        // const std::string message = "Response from URL=\"" + url + "\"\n"
        //     "  code: \"" + std::to_string(response.code) + "\"\n"
        //     "  status: \"" + response.status + "\"\n"
        //     "  body: \"" + response.body + "\"";
        //    OES_INFO(message);

        if (response.code == 200) {
            const auto jsonResponse = nlohmann::json::parse(response.body.c_str());
            OES_INFO("Received success response 200 from Get Secret API");
            return jsonResponse["value"];
        }
        else {
            const auto message = "SecretsProviderAzure Failed with code: " + std::to_string(response.code);
            OES_ERROR(message);
            OES_ERROR(response.body);
            return "";
        }
    }

    oes::postgresql::PostgresConnectParams SecretsProviderAzure::getPostgresConnectionStringSecret(const std::string &secretName) const {
        // Get connection string from KeyVault
        oes::postgresql::PostgresConnectParams connectParams = oes::postgresql::PostgresConnectParams::parse(getSecret(secretName));

        if (postgresMsiEnabled_) {
            // Get the token for the password
            OES_INFO("Postgres MSI auth is enabled");

            // Any references captured by this lambda typically go out of scope before it's called,
            // so captures should be by value.
            auto tokenLambda = [scope = postgresScope_](){
                OES_DEBUG("MSI callback for Postgres password: fetching access token");
                return MsiCredentialProvider::instance().getAccessToken(scope);
            };
            connectParams.registerDynamicParam(oes::postgresql::connect_params::ParamKey::ePASSWORD, tokenLambda);
        } else {
            OES_INFO("Postgres MSI auth is not enabled. Connection string will be used as-is");
        }

        return connectParams;
    }
} // namespace oes::providers::azure
