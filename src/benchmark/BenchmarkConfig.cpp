#include "BenchmarkConfig.h"

#include <oes/core/utils/AppConfigReader.h>

namespace oes::benchmark {

BenchmarkConfig::BenchmarkConfig() {
    oes::core::AppConfigReader reader("oesBenchmark_config.json");
    auto env_map = reader.getSimpleEnvMap();

    // Data Partitions
    partitionMode_ = env_map.at("/datapartitions/partitionmode");
}

std::shared_ptr<BenchmarkConfig> BenchmarkConfig::make() {
    return std::make_shared<BenchmarkConfig>();
}

// Data Partitions
const std::string& BenchmarkConfig::partitionMode() const noexcept {
    return partitionMode_;
}

} // oes::app::eml