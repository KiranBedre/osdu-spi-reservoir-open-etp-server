#include <sstream>

#include "osduStubServerConfig.h"

#include <oes/core/utils/AppConfigReader.h>

namespace oes::testing {

osduStubServerConfig::osduStubServerConfig() {
    oes::core::AppConfigReader reader("osduStubServer_config.json");
    auto env_map = reader.getSimpleEnvMap();

    // Data Partitions
    standaloneConnectionStrings_ = env_map.at("/datapartitions/standaloneconnectionstrings/single");
    partitionStubIds_ = std::move(osduStubServerConfig::getPartitionIds(env_map.at("/datapartitions/partitionstubids")));
    
    auto tmp = env_map.at("/stubserver/stubport");
    if (tmp.empty()) {
        stubPort_= 9080;
    } else {
        stubPort_ = std::stoi(tmp);
    }
}

std::shared_ptr<osduStubServerConfig> osduStubServerConfig::make() {
    return std::make_shared<osduStubServerConfig>();
}

// Data Partitions
const std::string& osduStubServerConfig::standaloneConnectionStrings() const noexcept {
    return standaloneConnectionStrings_;
}
// Stub Data Partitions
const std::vector<std::string>& osduStubServerConfig::partitionStubIds() const noexcept {
    return partitionStubIds_;
}

std::vector<std::string> osduStubServerConfig::getPartitionIds(const std::string& stubIds) {
    if (stubIds.empty()) {
        return {"local"};
    }

    std::vector<std::string> ids;
    std::stringstream s_ids(stubIds);
    std::string tmp_id;

    while (std::getline(s_ids, tmp_id, ',')) {
        if (!tmp_id.empty()){
            ids.push_back(tmp_id);
        }
    }
    return ids;
}

int osduStubServerConfig::stubPort() const noexcept {
    return stubPort_;
}

} // oes::app::eml