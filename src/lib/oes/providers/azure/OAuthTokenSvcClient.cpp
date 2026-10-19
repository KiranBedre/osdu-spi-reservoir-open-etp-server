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
#include "OAuthTokenSvcClient.h"
#include "Utils.h"

#include <oes/core/http/HTTP.h>
#include <oes/core/utils/Logger.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <iostream>
#include <vector>
#include <map>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes::providers::azure {

OAuthTokenSvcClient::OAuthTokenSvcClient(const oes::providers::core::ProvidersConfig& app_config) {
    // cppcheck-suppress useInitializationList
    OES_INFO("Getting client configuration from env.");
    clientId_ = app_config.azurePrincipalID();
    clientSecret_ = app_config.azurePrincipalSecret();
    tenantId_ = app_config.azureAdTenantID();
    scope_ = app_config.azureAdAppResourceID() + "/.default";
};

OAuthTokenSvcClient::OAuthTokenSvcClient(const SecretsProviderAzure& kvClient) {
    // cppcheck-suppress useInitializationList
    OES_INFO("Getting client configuration from secrets.");
    clientId_ = kvClient.getSecret("app-dev-sp-username");
    clientSecret_ = kvClient.getSecret("app-dev-sp-password");
    tenantId_ = kvClient.getSecret("app-dev-sp-tenant-id");
    scope_ = kvClient.getSecret("aad-client-id") + "/.default";
};

std::string
OAuthTokenSvcClient::getAccessToken() const {
    if (clientId_.empty() || clientSecret_.empty() || tenantId_.empty() || scope_ == "./default") {
        throw std::runtime_error(
            "\nMissing either: \n"
            "Client ID: " + clientId_ + "\n"
            "Client Secret: " + std::string(clientSecret_.empty() ? "Missing Value": "...") + "\n"
            "Tenant ID: " + tenantId_ + "\n"
            "Scope: " + scope_ + "\n"
        );
    }

    std::string url;
    oes::core::HTTP request;
    oes::core::HTTP::Response response;
    std::vector<std::string> headers{
        "Accept: application/json",
        "Content-Type: application/x-www-form-urlencoded"};

    const std::string baseEndpoint = "https://login.microsoftonline.com";
    url = baseEndpoint + "/" + tenantId_ + "/oauth2/v2.0/token";
    const std::map<std::string, std::string> bodyParameters {
        {"grant_type", "client_credentials"},
        {"client_id", clientId_},
        {"client_secret", clientSecret_},
        {"scope", scope_}};
    auto bodyParams = paramsToString(bodyParameters);

    const size_t unusedPort = 0;
    response = request.post(url, unusedPort, headers, bodyParams);

    if (response.code == 200) {
        const auto jsonResponse = nlohmann::json::parse(response.body.c_str());
        return jsonResponse["access_token"];
    }
    else {
        const auto message = "OAuthTokenSvcClient Failed with code: " + std::to_string(response.code);
        OES_ERROR(message);
        OES_ERROR(response.body);
        return "";
    }
}
} // end namespace oes::providers::azure