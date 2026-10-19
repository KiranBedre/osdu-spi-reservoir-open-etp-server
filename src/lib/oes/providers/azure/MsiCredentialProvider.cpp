// ============================================================================
// Copyright © Microsoft Corporation
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

#include "MsiCredentialProvider.h"
#include "MsiCustomEndpointCredential.h"
#include "Utils.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <azure/identity/chained_token_credential.hpp>
#include <azure/identity/default_azure_credential.hpp>
#include <azure/identity/managed_identity_credential.hpp>
#include <azure/identity/workload_identity_credential.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>

namespace oes::providers::azure {

MsiCredentialProvider::MsiCredentialProvider() {
    setupAzureLogging();

    try {
        // This uses a chained credential to support multiple identity sources
        credential_ = std::make_shared<Azure::Identity::ChainedTokenCredential>(
            Azure::Identity::ChainedTokenCredential::Sources {
                // Supports overriding the endpoint for the MSI credential. Mostly used for local development.
                std::make_shared<MsiCustomEndpointCredential>(),
                // Supports AKS workload identity
                std::make_shared<Azure::Identity::WorkloadIdentityCredential>(),
                // Supports AKS pod identity (doesn't need explicit client ID)
                std::make_shared<Azure::Identity::ManagedIdentityCredential>() });
    } catch (const std::exception &e) {
        OES_ERROR("Error getting credentials: {}", e.what());
        throw;
    }
}

std::string MsiCredentialProvider::getAccessToken(const std::string& scope) const {
    Azure::Core::Credentials::TokenRequestContext tokenRequestContext;
    if (!scope.empty()) {
        tokenRequestContext.Scopes.push_back(scope);
    }

    OES_DEBUG("Getting access token for scope {}", scope);

    Azure::Core::Context defaultContext;

    try {
        auto token = credential_->GetToken(tokenRequestContext, defaultContext);
        OES_DEBUG("Fetched token. Expires on {}", token.ExpiresOn.ToString());

        return token.Token;
    } catch (const Azure::Core::Credentials::AuthenticationException &e) {
        OES_ERROR("Failed to retrieve access token for scope '{}': {}", scope, e.what());
        return "";
    } catch (const std::exception &e) {
        OES_ERROR("Unexpected exception while retrieving access token for scope '{}': {}", scope, e.what());
        return "";
    }
}

} // namespace oes::providers::azure
