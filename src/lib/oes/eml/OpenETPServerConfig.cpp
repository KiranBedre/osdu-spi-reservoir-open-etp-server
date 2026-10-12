#include "OpenETPServerConfig.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/AppConfigReader.h>
#include <oes/core/utils/Logger.h>

namespace oes::eml {

std::unique_ptr<oes::core::envvar_map_t> openETPServerConfig::cached_tmp_map_ = nullptr;

std::string openETPServerConfig::default_config_file_ = "openETPServer_config.json";

openETPServerConfig::openETPServerConfig()
    : oes::osdu::OsduConfig(openETPServerConfig::get_env_map_(default_config_file_)),
      oes::core::OtelConfig(*openETPServerConfig::cached_tmp_map_),
      env_map_ptr_(std::move(openETPServerConfig::cached_tmp_map_)) {
    init_members_();
    env_map_ptr_.reset(); // free resources
}

openETPServerConfig::openETPServerConfig(const std::string& default_config_file, const std::string& custom_file_path)
    : oes::osdu::OsduConfig(openETPServerConfig::get_env_map_(default_config_file, custom_file_path)),
      oes::core::OtelConfig(*openETPServerConfig::cached_tmp_map_),
      env_map_ptr_(std::move(openETPServerConfig::cached_tmp_map_)) {
    init_members_();
}

const oes::core::envvar_map_t& openETPServerConfig::get_env_map_(const std::string& default_config_file, const std::string& custom_file_path) {
    auto reader_ptr (std::make_unique<oes::core::AppConfigReader>(default_config_file, custom_file_path));

    if (openETPServerConfig::cached_tmp_map_) {
        openETPServerConfig::cached_tmp_map_.reset();
    }
    openETPServerConfig::cached_tmp_map_ = std::make_unique<oes::core::envvar_map_t>(reader_ptr->getSimpleEnvMap());
    return *openETPServerConfig::cached_tmp_map_;
}

void openETPServerConfig::init_members_() {
    // Server
    enableAnyNumberOfThreads_ = env_map_ptr_->at("/server/enableanynumberofthreads");
    
    auto sec = std::chrono::seconds(120); // 120 is the default value
    try {
        auto sec_str = env_map_ptr_->at("/server/sessionkeepalivedurationseconds");
        if (!sec_str.empty()) {
            auto sec_int = std::stoi(sec_str);
            if (sec_int >= 0) {
                sec = std::chrono::seconds(sec_int);
            } else {
                sec = std::chrono::seconds(0);
            }
        }
    } catch (...) {
        OES_WARNING("Could not convert SESSION_KEEP_ALIVE_DURATION_SECONDS value properly. Using default value.");
    }
    sessionKeepAliveDurationSeconds_ = sec;

    // Data Partitions
    partitionMode_ = env_map_ptr_->at("/datapartitions/partitionmode");
    connectivityMode_ = env_map_ptr_->at("/datapartitions/connectivitymode");
    standaloneConnectionStrings_ = env_map_ptr_->at("/datapartitions/standaloneconnectionstrings/single");
    // Client options
    transactionRetries_ = env_map_ptr_->at("/client/transactionretries");
    // Logging
    logLevel_ = env_map_ptr_->at("/logging/loglevel");
    logMaxSeverity_ = env_map_ptr_->at("/logging/logmaxseverity");
    logMinSeverity_ = env_map_ptr_->at("/logging/logminseverity");
    logFormat_ = env_map_ptr_->at("/logging/logformat");
}

std::shared_ptr<openETPServerConfig> openETPServerConfig::make(const std::string& custom_file_path) {
    if (custom_file_path.empty()) {
        return std::make_shared<openETPServerConfig>();
    }
    return std::make_shared<openETPServerConfig>(default_config_file_, custom_file_path);
}

// Server
const std::string& openETPServerConfig::enableAnyNumberOfThreads() const noexcept {
    return enableAnyNumberOfThreads_;
}

const std::chrono::seconds openETPServerConfig::sessionKeepAliveDuration() const noexcept {
    return sessionKeepAliveDurationSeconds_;
}

// Data Partitions
const std::string& openETPServerConfig::partitionMode() const noexcept {
    return partitionMode_;
}
const std::string& openETPServerConfig::connectivityMode() const noexcept {
    return connectivityMode_;
}
const std::string& openETPServerConfig::standaloneConnectionStrings() const noexcept {
    return standaloneConnectionStrings_;
}
// Client
const std::string& openETPServerConfig::transactionRetries() const noexcept {
    return transactionRetries_;
}
// Logging
const std::string& openETPServerConfig::logLevel() const noexcept {
    return logLevel_;
}
const std::string& openETPServerConfig::logMaxSeverity() const noexcept {
    return logMaxSeverity_;
}
const std::string& openETPServerConfig::logMinSeverity() const noexcept {
    return logMinSeverity_;
}
const std::string& openETPServerConfig::logFormat() const noexcept {
    return logFormat_;
}

} // oes::app::eml
