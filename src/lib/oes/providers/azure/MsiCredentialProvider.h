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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <oes/core/utils/Singleton.h>
#include <oes/providers/export.h>
#include <oes/providers/core/ProvidersConfig.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <azure/core/credentials/credentials.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes::providers::azure {

class OES_PROVIDERS_EXPORT MsiCredentialProvider final : public oes::core::Singleton<MsiCredentialProvider>{
public:
    explicit MsiCredentialProvider();
    ~MsiCredentialProvider() override = default;

    std::string getAccessToken(const std::string& scope) const;

private:
    std::shared_ptr<Azure::Core::Credentials::TokenCredential const> credential_;
};

} // namespace oes::providers::azure
