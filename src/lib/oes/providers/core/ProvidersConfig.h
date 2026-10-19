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
#pragma once

#include <string>
#include <memory>

#include "../export.h"

#include <oes/core/utils/AppConfigReader.h>

namespace oes::providers::core {

class OES_PROVIDERS_EXPORT ProvidersConfig {
public:
    ProvidersConfig() = delete;
    explicit ProvidersConfig(const oes::core::envvar_map_t& env_map);
    virtual ~ProvidersConfig() = default;
    
    const oes::core::envvar_map_t& envMap() const noexcept;
    const std::string& keyVaultUrl() const noexcept;
    const std::string& cloudProvider() const noexcept;
    const bool azureEnableMsi() const noexcept;
    const std::string& azurePrincipalSecret() const noexcept;
    const std::string& azurePrincipalID() const noexcept;
    const std::string& azureAdTenantID() const noexcept;
    const std::string& azureAdAppResourceID() const noexcept;
    const bool azureUseEnvCredentials() const noexcept;
    const bool azureEnablePostgresMsi() const noexcept;
    const std::string& azureOSDUAccessTokenScopeOverride() const noexcept;
    const std::string& azureKeyVaultAccessTokenScope() const noexcept;
    const std::string& azurePostgresAccessTokenScope() const noexcept;
    const bool auditLogsEnabled() const noexcept;

    const std::string& value(const std::string& key) const;
    std::string valueOr(const std::string& key, const std::string& default_val) const noexcept;

protected:
    oes::core::envvar_map_t envMap_;

private:
    std::string keyVaultUrl_;
    std::string cloudProvider_;
    bool azureEnableMsi_;
    std::string azurePrincipalSecret_;
    std::string azurePrincipalID_;
    std::string azureAdTenantID_;
    std::string azureAdAppResourceID_;
    bool azureUseEnvCredentials_;
    bool azureEnablePostgresMsi_;
    std::string azureOSDUAccessTokenScopeOverride_;
    std::string azureKeyVaultAccessTokenScope_;
    std::string azurePostgresAccessTokenScope_;
    bool auditLogsEnabled_;
};

using providersConfig_ptr = std::shared_ptr<ProvidersConfig>;

} // namespace oes::providers::core