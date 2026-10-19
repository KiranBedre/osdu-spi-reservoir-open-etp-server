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

#include "../google/TokenRefresherGoogle.h"
#include "../azure/BearerTokenProviderAzure.h"
#include "../cimpl/BearerTokenProviderCImpl.h"

#include "TokenProviderFactory.h"
#include "EnvironmentResolver.h"

#include <stdexcept>

std::unique_ptr<oes::providers::core::BearerTokenProvider> oes::providers::core::TokenProviderFactory::GetBearerTokenProvider(
    const ProvidersConfig& app_config)
{
    switch (EnvironmentResolver::GetProviderFromEnv(app_config))
    {
    case CloudProviderEnum::Azure:
        return std::make_unique<oes::providers::azure::BearerTokenProviderAzure>();
        break;
    case CloudProviderEnum::Google:
        return std::make_unique<oes::providers::google::TokenRefresherGoogle>();
        break;
    case CloudProviderEnum::Community:
        return std::make_unique<oes::providers::cimpl::BearerTokenProviderCImpl>();
        break;
    default:
        throw std::runtime_error("Undefined Type for BearerTokenProvider");
        break;
    }
}