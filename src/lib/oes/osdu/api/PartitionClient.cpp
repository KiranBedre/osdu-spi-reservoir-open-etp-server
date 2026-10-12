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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/osdu/api/PartitionClient.h>

#include <oes/core/http/HTTP.h>
#include <oes/core/utils/Logger.h>

#include <oes/osdu/api/OsduClientException.h>


namespace oes {
    namespace osdu {
        namespace api {
            PartitionClient::PartitionClient(const oes::osdu::OsduConfig& app_config) : BaseClient(std::string())  {
                std::string tmp (app_config.partitionUrl());
                if (tmp.empty()) {
                    throw OsduClientException("Parition url cannot be empty\n");
                }

                partitionUrl_ = tmp;
            }

            PartitionClient::~PartitionClient() {}

            void validateDependencies(const std::string& bearerToken, const std::string& partitionUrl)
            {
                if (bearerToken.empty()) {
                    throw OsduClientException("Bearer token cannot be empty.\n");
                }

                if (partitionUrl.empty()) {
                    throw OsduClientException("Partition url cannot be empty.\n");
                }
            }

            std::vector<std::string> PartitionClient::getPartitionList(const std::string& bearerToken)
            {
                validateDependencies(bearerToken, partitionUrl_);

                oes::core::HTTP::Response res;
                const std::string url = partitionUrl_ + "/partitions";

                res = BaseClient::makeRequest(oes::core::HTTP::Method::eGET, bearerToken, url);

                if (res.code != 200) {
                    const std::string errorStr = "HTTP GET: " + url +
                                                 "ERROR RESPONSE STATUS CODE: " + std::to_string(res.code) +
                                                 ' ' + res.status;
                    OES_INFO(errorStr);
                    throw OsduClientException(errorStr.data());
                }

                std::vector<std::string> partitions;

                const auto jsonResponse = nlohmann::json::parse(res.body.c_str());

                for (auto& element : jsonResponse) {
                    partitions.push_back(std::string(element));
                }

                return partitions;
            }

            std::map<std::string, PartitionClient::PartitionCell> PartitionClient::getPartitionInfo(
                const std::string& bearerToken,
                const std::string& dataPartitionId
            )
            {
                validateDependencies(bearerToken, partitionUrl_);
                if (dataPartitionId.empty()) {
                    throw OsduClientException("Data partition id cannot be empty.\n");
                }

                oes::core::HTTP::Response res;
                const std::string url = partitionUrl_ + "/partitions/" + dataPartitionId;

                res = BaseClient::makeRequest(oes::core::HTTP::Method::eGET, bearerToken, url);

                if (res.code != 200) {
                    const std::string errorStr = "HTTP GET: " + url +
                                                 "ERROR RESPONSE STATUS CORE: " + std::to_string(res.code) +
                                                 ' ' + res.status;
                    OES_INFO(errorStr);
                    throw OsduClientException(errorStr.data());
                }

                std::map<std::string, PartitionClient::PartitionCell> partitionKeyVault;

                const auto jsonResponse = nlohmann::json::parse(res.body.c_str());

                for (auto& element : jsonResponse.items()) {
                    PartitionCell partitionCell;
                    partitionCell.key = element.key();
                    const auto innerObj = element.value();

                    if (innerObj["value"].type() == nlohmann::json::value_t::string) {
                        partitionCell.value =  innerObj["value"];
                    } else {
                        partitionCell.value = innerObj["value"].dump();
                        OES_WARNING("Partition cell value '{}' converted to string", partitionCell.value);
                    }

                    if (innerObj["sensitive"].type() == nlohmann::json::value_t::boolean) {
                        partitionCell.sensitive = innerObj["sensitive"];
                    } else {
                        if (innerObj["sensitive"] == "true") {
                           partitionCell.sensitive = true;
                        } else if (innerObj["sensitive"] == "false") {
                           partitionCell.sensitive = false;
                        } else {
                            OES_WARNING("Invalid partition cell sensitive '{}'", innerObj["sensitive"].dump());
                        }
                    }
                    partitionKeyVault.insert({partitionCell.key, partitionCell});
                }

                return partitionKeyVault;
            }
        }
    }
}
