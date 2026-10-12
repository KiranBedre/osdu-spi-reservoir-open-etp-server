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
#include <memory>
#include <map>
#include <chrono>

#include <oes/core/utils/AppConfigReader.h>
#include <oes/core/utils/Otel.h>
#include <oes/osdu/OsduConfig.h>
#include <oes/eml/export.h>

namespace oes::eml {

//using Config_map_t = std::map<std::string, std::string>;

class OES_EML_EXPORT openETPServerConfig: public oes::osdu::OsduConfig, public oes::core::OtelConfig {
public:
    openETPServerConfig();
    explicit openETPServerConfig(const std::string& default_config_file, const std::string& custom_file_path = "");

    virtual ~openETPServerConfig() = default;

    static std::shared_ptr<openETPServerConfig> make(const std::string& custom_file_path = "");
    
    // Server
    const std::string& enableAnyNumberOfThreads() const noexcept;
    const std::chrono::seconds sessionKeepAliveDuration() const noexcept;
    // Data Partitions
    const std::string& partitionMode() const noexcept;
    const std::string& connectivityMode() const noexcept;
    const std::string& standaloneConnectionStrings() const noexcept;
    // Client
    const std::string& transactionRetries() const noexcept;
    // Logging
    const std::string& logLevel() const noexcept;
    const std::string& logMaxSeverity() const noexcept;
    const std::string& logMinSeverity() const noexcept;
    const std::string& logFormat() const noexcept;

protected:
    void init_members_();
    static const oes::core::envvar_map_t& get_env_map_(const std::string& default_config_file, const std::string& custom_file_path = "");

protected:
    std::unique_ptr<oes::core::envvar_map_t> env_map_ptr_;
    static std::unique_ptr<oes::core::envvar_map_t> cached_tmp_map_;

private:
    // Server
    std::string enableAnyNumberOfThreads_;
    std::chrono::seconds sessionKeepAliveDurationSeconds_; // 120 seconds by default

    // Data Partitions
    std::string partitionMode_;
    std::string connectivityMode_;
    std::string standaloneConnectionStrings_;
    // Client options
    std::string transactionRetries_;
    // Logging
    std::string logLevel_;
    std::string logMaxSeverity_;
    std::string logMinSeverity_;
    std::string logFormat_;

    static std::string default_config_file_;
};

using openETPServerConfig_ptr = std::shared_ptr<openETPServerConfig>;

} // oes::app::eml