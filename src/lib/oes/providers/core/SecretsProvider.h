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
#include "ProvidersConfig.h"

#include <oes/postgresql/Connection.h>

#include <string>

namespace oes::providers::core
{
    class OES_PROVIDERS_EXPORT SecretsProvider
    {
    public:
        SecretsProvider(const oes::providers::core::ProvidersConfig& app_config);
        virtual ~SecretsProvider() = default;
        virtual std::string getSecret(const std::string &secretName) const = 0;
        virtual oes::postgresql::PostgresConnectParams getPostgresConnectionStringSecret(const std::string &secretName) const;
    protected:
        const oes::providers::core::ProvidersConfig& app_config_;
    };
} // namespace oes::providers::core
