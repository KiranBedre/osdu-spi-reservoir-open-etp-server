// ============================================================================
// Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
#include "../core/SecretsProvider.h"
#include "../core/ProvidersConfig.h"

#include <string>

namespace oes::providers::cimpl
{
    class OES_PROVIDERS_EXPORT SecretsProviderCImpl final : public oes::providers::core::SecretsProvider {
    public:
        explicit SecretsProviderCImpl(const oes::providers::core::ProvidersConfig& app_config);

        std::string getSecret(const std::string &secretName) const override;
    };
}
