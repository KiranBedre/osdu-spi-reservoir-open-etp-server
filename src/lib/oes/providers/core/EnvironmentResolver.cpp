// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2023 EPAM Systems
// Copyright 2023 Google LLC
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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "EnvironmentResolver.h"
#include <oes/core/utils/Logger.h>

#include <string>

const std::map<std::string, oes::providers::core::CloudProviderEnum> oes::providers::core::EnvironmentResolver::cspEnumMap =
{
    {"az", CloudProviderEnum::Azure},
    {"gc", CloudProviderEnum::Google},
    {"cimpl", CloudProviderEnum::Community}
};

oes::providers::core::CloudProviderEnum oes::providers::core::EnvironmentResolver::GetProviderFromEnv(const ProvidersConfig& app_config)
{
    std::string envName = app_config.cloudProvider();
    if (cspEnumMap.find(envName) != cspEnumMap.end()) {
        OES_INFO("Cloud Provider Code: {}.", envName);
        return cspEnumMap.at(envName);
    }
    else {
        throw std::runtime_error("Cannot match CLOUD_PROVIDER environment variable.\n");
    }
}