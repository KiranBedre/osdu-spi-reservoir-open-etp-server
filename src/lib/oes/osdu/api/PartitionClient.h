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

#ifndef OES_OSDU_API_PARTITIONCLIENT_H
#define OES_OSDU_API_PARTITIONCLIENT_H

#include <string>
#include <vector>
#include <map>

#include <oes/core/http/HTTP.h>

#include <oes/osdu/export.h>
#include <oes/osdu/api/BaseClient.h>
#include <oes/osdu/OsduConfig.h>

namespace oes {
    namespace osdu {
        namespace api {
            class OES_OSDU_EXPORT PartitionClient : public BaseClient
            {
            public:
                struct PartitionCell {
                    PartitionCell() {}
                    std::string key;
                    std::string value;
                    bool sensitive = false;
                };

                explicit PartitionClient(const oes::osdu::OsduConfig& app_config);
                ~PartitionClient() override;

                virtual std::vector<std::string> getPartitionList(const std::string& bearerToken);

                virtual std::map<std::string, PartitionCell> getPartitionInfo(
                    const std::string& bearerToken,
                    const std::string& dataPartitionId = std::string()
                );

            private:
                std::string partitionUrl_;
            };
        }
    }
}

#endif //OES_OSDU_API_PARTITIONCLIENT_H
