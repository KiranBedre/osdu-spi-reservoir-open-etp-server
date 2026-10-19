// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
// Copyright (c) Microsoft
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

#include "ProvidersConfig.h"

#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>

namespace {
    bool stringToBool(const std::string &s, bool boolalpha = true) {
        if (s.empty()) {
            return false;
        }

        std::istringstream iss(s);
        if (boolalpha) {
            iss >> std::boolalpha;
        }

        bool result;
        iss >> result;

        // With boolalpha = true, this function returns true if s == "true"
        // With boolalpha = false, this function is equivalent to (bool)stoi(s)
        return result;
    }
}

namespace oes::providers::core {

ProvidersConfig::ProvidersConfig(const oes::core::envvar_map_t& env_map)
    : envMap_(env_map),
      keyVaultUrl_ (value("/osdu/keyvaulturl")),
      cloudProvider_ (value("/osdu/cloudprovider")),
      azureEnableMsi_ (stringToBool(value("/cloudproviders/azure/azureenablemsi"))),
      azurePrincipalSecret_ (value("/cloudproviders/azure/azureprincipalsecret")),
      azurePrincipalID_ (value("/cloudproviders/azure/azureprincipalid")),
      azureAdTenantID_ (value("/cloudproviders/azure/azureadtenantid")),
      azureAdAppResourceID_ (value("/cloudproviders/azure/azureadappresourceid")),
      azureUseEnvCredentials_ (stringToBool(value("/cloudproviders/azure/azureuseenvironmentcredentials"))),
      azureEnablePostgresMsi_(stringToBool(value("/cloudproviders/azure/azureenablepostgresmsi"))),
      azureOSDUAccessTokenScopeOverride_(valueOr("/cloudproviders/azure/azureosduaccesstokenscopeoverride", "")),
      azureKeyVaultAccessTokenScope_(valueOr("/cloudproviders/azure/azurekeyvaultaccesstokenscope", "https://vault.azure.net/.default")),
      azurePostgresAccessTokenScope_(valueOr("/cloudproviders/azure/azurepostgresaccesstokenscope", "https://ossrdbms-aad.database.windows.net/.default")),
      auditLogsEnabled_(stringToBool(valueOr("/logging/auditlogsenabled", "false"))) {}

const oes::core::envvar_map_t& ProvidersConfig::envMap() const noexcept {
    return envMap_;
}

/**
 * Get the value associated with the given key in the environment map
 */
const std::string& ProvidersConfig::value(const std::string& key) const{
    auto it = envMap_.find(key);
    if (it != envMap_.end()) {
        return it->second;
    }
    throw std::runtime_error(fmt::format("Key '{}' not found in config map", key));
}

/**
 * Get the value associated with the given key in the environment map, or a default value if not found
 */
std::string ProvidersConfig::valueOr(const std::string& key, const std::string& default_val) const noexcept {
    auto it = envMap_.find(key);
    if (it == envMap_.end() || it->second.empty()) {
        return default_val;
    }

    return it->second;
}

const std::string& ProvidersConfig::keyVaultUrl() const noexcept {
    return keyVaultUrl_;
}

const std::string& ProvidersConfig::cloudProvider() const noexcept {
    return cloudProvider_;
}

const bool ProvidersConfig::azureEnableMsi() const noexcept {
    return azureEnableMsi_;
}

const std::string& ProvidersConfig::azurePrincipalSecret() const noexcept {
    return azurePrincipalSecret_;
}

const std::string& ProvidersConfig::azurePrincipalID() const noexcept {
    return azurePrincipalID_;
}

const std::string& ProvidersConfig::azureAdTenantID() const noexcept {
    return azureAdTenantID_;
}

const std::string& ProvidersConfig::azureAdAppResourceID() const noexcept {
    return azureAdAppResourceID_;
}

const bool ProvidersConfig::azureUseEnvCredentials() const noexcept {
    return azureUseEnvCredentials_;
}

const bool ProvidersConfig::azureEnablePostgresMsi() const noexcept {
    return azureEnablePostgresMsi_;
}

const std::string& ProvidersConfig::azureOSDUAccessTokenScopeOverride() const noexcept {
    return azureOSDUAccessTokenScopeOverride_;
}

const std::string& ProvidersConfig::azureKeyVaultAccessTokenScope() const noexcept {
    return azureKeyVaultAccessTokenScope_;
}

const std::string& ProvidersConfig::azurePostgresAccessTokenScope() const noexcept {
    return azurePostgresAccessTokenScope_;
}

const bool ProvidersConfig::auditLogsEnabled() const noexcept {
    return auditLogsEnabled_;
}

} // namespace oes::providers::core
