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

#pragma once

#include "../export.h"
#include <map>
#include <string>

#include "ProvidersConfig.h"

namespace oes::providers::core
{
    enum class CloudProviderEnum
    {
        Azure,
        Google,
        Community,
    };
    class OES_PROVIDERS_EXPORT EnvironmentResolver
    {
    private:
        static const std::map<std::string, CloudProviderEnum> cspEnumMap;

    public:
        // will throw if the env not found in the enum map
        static CloudProviderEnum GetProviderFromEnv(const ProvidersConfig& app_config);
    };
}