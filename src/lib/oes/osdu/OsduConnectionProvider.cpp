// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2023 Google LLC
// Copyright 2023 EPAM Systems
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

#include "OsduConnectionProvider.h"

#include "oes/osdu/api/PartitionClient.h"

#include <oes/core/utils/Logger.h>
#include <oes/providers/core/BearerTokenProvider.h>
#include <oes/providers/core/SecretsProvider.h>
#include <oes/providers/core/TokenProviderFactory.h>
#include <oes/providers/core/SecretProviderFactory.h>


constexpr char PARTITION_RESERVOIR_KEY[] = "reservoir-connection";

std::map<std::string, oes::postgresql::PostgresConnectParams> oes::osdu::OsduConnectionInfoProvider::getOsduConnectionInfo(
    const oes::osdu::OsduConfig& app_config)
{
    OES_INFO("Server connecting to OSDU Data Platform & getting OSDU bearer token.");

    std::string bearerToken = oes::providers::core::TokenProviderFactory::GetBearerTokenProvider(app_config)->GetBearerToken(app_config);

    oes::osdu::api::PartitionClient partitionClient(app_config);
    std::vector<std::string> partitions = partitionClient.getPartitionList(bearerToken);
    OES_INFO("Server getting OSDU data partition information.");

    std::map<std::string, oes::postgresql::PostgresConnectParams> partitionConnections;

    std::unique_ptr<oes::providers::core::SecretsProvider> secretsProvider = oes::providers::core::SecretProviderFactory::GetSecretsProvider(app_config);

    for (const auto &partitionName : partitions)
    {
        const auto partitionKeyVault = partitionClient.getPartitionInfo(bearerToken, partitionName);
        const auto partitionCell = partitionKeyVault.find(PARTITION_RESERVOIR_KEY);
        if (partitionCell == partitionKeyVault.end())
        {
            OES_WARNING(
                "Reservoir connection key not found for partition: {}."
                , partitionName
            );
            continue;
        }

        OES_INFO(
            "Reservoir connection key {} for partition: {}"
            , partitionCell->second.value
            , partitionName
        );

        const oes::postgresql::PostgresConnectParams partitionPsqlConnParams = secretsProvider->getPostgresConnectionStringSecret(partitionCell->second.value);

        if (partitionPsqlConnParams.empty())
        {
            OES_WARNING(
                "Reservoir connection key empty for partition: {}."
                , partitionName
            );
            continue;
        }

        partitionConnections.emplace(partitionName, partitionPsqlConnParams);
        OES_INFO(
            "Successful connection string retrieved for partition: {}.",
            partitionName
        );
    }

    return partitionConnections;
}