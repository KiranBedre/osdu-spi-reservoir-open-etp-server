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
#include <vector>
#include <memory>

namespace oes::testing {

class osduStubServerConfig {
public:
    osduStubServerConfig();
    ~osduStubServerConfig() = default;

    static std::shared_ptr<osduStubServerConfig> make();
private:
    // Data Partitions
    std::string standaloneConnectionStrings_;
    std::vector<std::string> partitionStubIds_;
    int stubPort_;

    static std::vector<std::string> getPartitionIds(const std::string& stubIds);
    
public:
    // Data Partitions
    const std::string& standaloneConnectionStrings() const noexcept;
    const std::vector<std::string>& partitionStubIds() const noexcept;
    int stubPort() const noexcept;
};

} // oes::app::eml