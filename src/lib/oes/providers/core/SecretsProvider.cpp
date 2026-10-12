// ============================================================================
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

#include "SecretsProvider.h"

namespace oes::providers::core {

SecretsProvider::SecretsProvider(const oes::providers::core::ProvidersConfig& app_config)
    : app_config_(app_config) {}

oes::postgresql::PostgresConnectParams SecretsProvider::getPostgresConnectionStringSecret(const std::string &secretName) const {
    // Default implementation
    return oes::postgresql::PostgresConnectParams::parse(getSecret(secretName));
}

} // namespace oes::providers::core
