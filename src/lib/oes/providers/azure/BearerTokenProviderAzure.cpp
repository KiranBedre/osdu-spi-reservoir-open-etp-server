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

#include <sstream>
#include "oes/core/utils/Logger.h"
#include "BearerTokenProviderAzure.h"
#include "MsiCredentialProvider.h"
#include "OAuthTokenSvcClient.h"
#include "SecretsProviderAzure.h"

std::string oes::providers::azure::BearerTokenProviderAzure::GetScope(
    const oes::providers::core::ProvidersConfig& app_config,
    const oes::providers::azure::SecretsProviderAzure &secretsProvider) {
    std::string scopeOverride = app_config.azureOSDUAccessTokenScopeOverride();
    if (!scopeOverride.empty()) {
        return scopeOverride;
    } else {
        std::string clientId = secretsProvider.getSecret("aad-client-id");
        // Ensure scope has proper OAuth2 format for Workload Identity
        // Append /.default if not already present
        if (clientId.find("/.default") == std::string::npos) {
            // Remove trailing slash if present before appending /.default
            if (!clientId.empty() && clientId.back() == '/') {
                clientId.pop_back();
            }
            return clientId + "/.default";
        }
        return clientId;
    }
}

std::string oes::providers::azure::BearerTokenProviderAzure::GetBearerToken(const oes::providers::core::ProvidersConfig& app_config){
    oes::providers::azure::SecretsProviderAzure secretsProvider(app_config);
    if (app_config.azureEnableMsi()) {
        OES_INFO("Using MSI token.");
        std::string scope = GetScope(app_config, secretsProvider);
        std::string token = oes::providers::azure::MsiCredentialProvider::instance().getAccessToken(scope);
        if (token.empty()) {
            OES_ERROR("Failed to get bearer token via MSI for scope: {}", scope);
        }
        return token;
    } else {
        OES_INFO("Using Service Principal token.");

        if (app_config.azureUseEnvCredentials()) {
            OES_INFO("Create token from environment credentials.");
            oes::providers::azure::OAuthTokenSvcClient client(app_config);
            return client.getAccessToken();
        } else {
            OES_INFO("Create token from secrets credentials.");
            oes::providers::azure::OAuthTokenSvcClient client(secretsProvider);
            return client.getAccessToken();
        }
    }
}