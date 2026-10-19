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

#include "../export.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <azure/core/credentials/credentials.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes::providers::azure {
    class OES_PROVIDERS_EXPORT MsiCustomEndpointCredential final : public Azure::Core::Credentials::TokenCredential {
        public:
            explicit MsiCustomEndpointCredential();
            Azure::Core::Credentials::AccessToken GetToken(
                Azure::Core::Credentials::TokenRequestContext const& tokenRequestContext,
                Azure::Core::Context const& context) const override;
        private:
            inline static const std::string IDENTITY_ENDPOINT_ENV_NAME = "IDENTITY_ENDPOINT";
            inline static const std::string IDENTITY_HEADER_ENV_NAME = "IDENTITY_HEADER";
            std::string identityEndpoint_;
            std::string identityHeader_;
    };
} // namespace oes::providers::azure
