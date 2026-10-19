#include "OsduConfig.h"

namespace oes::osdu {

OsduConfig::OsduConfig(const oes::core::envvar_map_t& env_map)
    : oes::providers::core::ProvidersConfig(env_map),
      authorizationMode_ (env_map.at("/osdu/authorizationmode")),
      domainName_ (env_map.at("/osdu/domainname")),
      partitionUrl_ (env_map.at("/osdu/partitionurl")),
      entitlementsUrl_ (env_map.at("/osdu/entitlementsurl")),
      storageUrl_ (env_map.at("/osdu/storageurl")),
      dataPartitionID_ (env_map.at("/osdu/datapartitionid")),
      schemaID_ (env_map.at("/osdu/schemaid")),
      legalTags_ (env_map.at("/osdu/legaltags")),
      legalCountries_ (env_map.at("/osdu/legalcountries")),
      recordsPath_ (env_map.at("/osdu/recordspath")),
      evalutePolicyPath_ (env_map.at("/osdu/evaluatepolicypath")),
      authorizationBulkSize_ (env_map.at("/osdu/authorizationbulksize")) {
        // Default values
        if(authorizationMode_.empty() || 
           (authorizationMode_ != "osdu" && authorizationMode_ != "simple") ) {
            authorizationMode_ = "osdu";
        }

        if (recordsPath_.empty()) {
            recordsPath_ = "/records";
        }

        if (evalutePolicyPath_.empty()) {
            evalutePolicyPath_ = "/api/policy/v1/evaluations/query";
        }
      }

const std::string& OsduConfig::authorizationMode() const noexcept {
    return authorizationMode_;
}
const std::string& OsduConfig::domainName() const noexcept {
    return domainName_;
}
const std::string& OsduConfig::partitionUrl() const noexcept {
    return partitionUrl_;
}
const std::string& OsduConfig::entitlementsUrl() const noexcept {
    return entitlementsUrl_;
}
const std::string& OsduConfig::storageUrl() const noexcept {
    return storageUrl_;
}
const std::string& OsduConfig::dataPartitionID() const noexcept {
    return dataPartitionID_;
}
const std::string& OsduConfig::schemaID() const noexcept {
    return schemaID_;
}
const std::string& OsduConfig::legalTags() const noexcept {
    return legalTags_;
}
const std::string& OsduConfig::legalCountries() const noexcept {
    return legalCountries_;
}
const std::string& OsduConfig::recordsPath() const noexcept {
    return recordsPath_;
}
const std::string& OsduConfig::evalutePolicyPath() const noexcept {
    return evalutePolicyPath_;
}
const std::string& OsduConfig::authorizationBulkSize() const noexcept {
    return authorizationBulkSize_;
}

} // namespace oes::osdu 