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

#include "MsiCustomEndpointCredential.h"
#include "Utils.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/http/HTTP.h>
#include <oes/core/utils/Logger.h>

namespace oes::providers::azure {

MsiCustomEndpointCredential::MsiCustomEndpointCredential() :
    TokenCredential("MsiCustomEndpointCredential"),
    identityEndpoint_(getEnv(MsiCustomEndpointCredential::IDENTITY_ENDPOINT_ENV_NAME)),
    identityHeader_(getEnv(MsiCustomEndpointCredential::IDENTITY_HEADER_ENV_NAME)) {
}

Azure::Core::Credentials::AccessToken MsiCustomEndpointCredential::GetToken(Azure::Core::Credentials::TokenRequestContext const& tokenRequestContext, Azure::Core::Context const& /*context*/) const {
    if (identityEndpoint_.empty()) {
        // Throwing this will signal to a chained token credential to try the next auth method
        throw Azure::Core::Credentials::AuthenticationException("Missing environment variable IDENTITY_HEADER required for MsiCustomEndpointCredential");
    }

    std::string url, resourceId;
    oes::core::HTTP request;
    oes::core::HTTP::Response response;
    std::vector<std::string> headers {
        "Accept: application/json",
        "Metadata: true",
        "X-IDENTITY-HEADER: " + identityHeader_ };
    std::map<std::string, std::string> queryParameters {
        {"version", "2018-02-01"}};

    std::string scope;
    if (!tokenRequestContext.Scopes.empty()) {
        if (tokenRequestContext.Scopes.size() > 1) {
            OES_WARNING("MsiCustomEndpointCredential only supports a single scope. Any scopes beyond the first will be ignored.");
        }

        scope = tokenRequestContext.Scopes.front();
        queryParameters.insert({"resource", scope});
    }

    const auto urlEncodedParameters = paramsToString(queryParameters);
    url = identityEndpoint_ + "?" + urlEncodedParameters;

    OES_DEBUG("Get MSI token for scope: {} using url: {}\n and identity header: {}", scope, url, identityHeader_);

    const size_t unusedPort = 0;
    response = request.get(url, unusedPort, headers);

    Azure::Core::Credentials::AccessToken result;
    if (response.code == 200) {
        const auto jsonResponse = nlohmann::json::parse(response.body.c_str());
        result.Token = jsonResponse["access_token"];

        if (jsonResponse.contains("expires_on")) {
            // expires_on property is a number of seconds since the unix epoch
            result.ExpiresOn = Azure::DateTime(1970) + std::chrono::seconds(jsonResponse["expires_on"].get<unsigned int>());
        } else if (jsonResponse.contains("expires_in")) {
            // expires_in property is a number of seconds since the current time
            result.ExpiresOn = std::chrono::system_clock::now() + std::chrono::seconds(jsonResponse["expires_in"].get<unsigned int>());
        } else {
            OES_DEBUG("Unable to determine token expiration, using default");
        }

        return result;
    }
    else {
        const auto message = "MsiCustomEndpointCredential failed with code: " + std::to_string(response.code) + "\n";
        OES_ERROR(message);
        OES_ERROR(response.body);
        // Throwing this will signal to a chained token credential to try the next auth method
        throw Azure::Core::Credentials::AuthenticationException(message);
    }
}

} // namespace oes::providers::azure
