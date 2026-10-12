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
#include "../core/BaseTokenClient.h"
#include "../core/ProvidersConfig.h"
#include "SecretsProviderAzure.h"

namespace oes {
    namespace providers {
        namespace azure {
            class OES_PROVIDERS_EXPORT OAuthTokenSvcClient final : public oes::providers::core::BaseTokenClient {
                public:
                    explicit OAuthTokenSvcClient(const oes::providers::core::ProvidersConfig& app_config);
                    explicit OAuthTokenSvcClient(const SecretsProviderAzure& kvClient);

                    std::string getAccessToken() const override;

                private:
                    std::string clientId_;
                    std::string clientSecret_;
                    std::string tenantId_;
                    std::string scope_;
                };
        }
    }
}