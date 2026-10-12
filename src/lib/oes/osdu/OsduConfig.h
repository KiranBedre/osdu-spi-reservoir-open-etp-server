// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
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
#pragma once

#include <string>

#include <oes/core/utils/AppConfigReader.h>
#include <oes/providers/core/ProvidersConfig.h>
#include "export.h"

namespace oes::osdu {

class OES_OSDU_EXPORT OsduConfig: public oes::providers::core::ProvidersConfig {
public:
    OsduConfig() = delete;
    explicit OsduConfig(const oes::core::envvar_map_t& env_map);
    virtual ~OsduConfig() = default;

    const std::string& authorizationMode() const noexcept;
    const std::string& domainName() const noexcept;
    const std::string& partitionUrl() const noexcept;
    const std::string& entitlementsUrl() const noexcept;
    const std::string& storageUrl() const noexcept;
    const std::string& dataPartitionID() const noexcept;
    const std::string& schemaID() const noexcept;
    const std::string& legalTags() const noexcept;
    const std::string& legalCountries() const noexcept;
    const std::string& recordsPath() const noexcept;
    const std::string& evalutePolicyPath() const noexcept;
    const std::string& authorizationBulkSize() const noexcept;
    
private:
    std::string authorizationMode_; // simple or osdu -> default is osdu
    std::string domainName_;
    std::string partitionUrl_;
    std::string entitlementsUrl_;
    std::string storageUrl_;
    std::string dataPartitionID_;
    std::string schemaID_;
    std::string legalTags_;
    std::string legalCountries_;
    std::string recordsPath_;
    std::string evalutePolicyPath_;
    std::string authorizationBulkSize_;
};

} // namespace oes::osdu