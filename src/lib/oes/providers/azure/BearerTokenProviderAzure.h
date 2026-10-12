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

#pragma once

#include <string>

#include "../export.h"
#include "../core/BearerTokenProvider.h"
#include "../core/ProvidersConfig.h"
#include "SecretsProviderAzure.h"

namespace oes::providers::azure {

class OES_PROVIDERS_EXPORT BearerTokenProviderAzure final : public oes::providers::core::BearerTokenProvider {
    public:
        std::string GetBearerToken(const oes::providers::core::ProvidersConfig& app_config) override;
    private:
        std::string GetScope(const oes::providers::core::ProvidersConfig& app_config, const SecretsProviderAzure &secretsProvider);
};

} // namespace oes::providers::azure