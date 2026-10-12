#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/osdu/api/EntitlementClient.h>
#include <oes/osdu/api/BaseClient.h>

#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/String.h>



namespace oes::osdu::api {

EntitlementConfigBase::~EntitlementConfigBase() {
}

bool EntitlementConfigBase::isValid() const {
    return !storageUrl().empty() && !entitlementsUrl().empty() && !dataPartitionId().empty();
}

const std::string& EntitlementConfigBase::entitlementsUrl() const {
    return entitlements_url_;
}

const std::string& EntitlementConfigBase::storageUrl() const {
    return storage_url_;
}

const std::string& EntitlementConfigBase::policyDelegateUrl() const {
    return policy_delegate_url_;
}

const std::string& EntitlementConfigBase::recordsPath() const {
    return records_path_;
}

const std::string& EntitlementConfigBase::dataPartitionId() const {
    return data_partition_id_;
}

void EntitlementConfigBase::setDataPartitionId(const std::string& dataPartitionId) {
    data_partition_id_ = dataPartitionId;
}

const std::string& EntitlementConfigBase::schemaId() const {
    return schema_id_;
}

const std::string& EntitlementConfigBase::domainName() const {
    return domain_name_;
}

const std::string& EntitlementConfigBase::legalTags() const {
    return legal_tags_;
}

const std::string& EntitlementConfigBase::legalCountries() const {
    return legal_countries_;
}

const std::string& EntitlementConfigBase::connectivityMode() const {
    return connectivity_mode_;
}

void EntitlementConfigBase::setConnectivityMode(const std::string& mode) {
    connectivity_mode_ = mode;
}

const std::string& EntitlementConfigBase::authorizationMode() const {
    return authorization_mode_;
}
void EntitlementConfigBase::setAuthorizationMode(const std::string& mode) {
    authorization_mode_ = mode;
}

const std::string& EntitlementConfigBase::evaluatePolicyPath() const {
    return evaluatePolicyPath_;
}

const std::string& EntitlementConfigBase::authorizationBulkSize() const {
    return authorization_bulk_size_;
}

EntitlementConfig_ptr EntitlementConfigBase::make(const std::string& delegateUri, const oes::osdu::OsduConfig& app_config, const std::string& conn_mode) {
    return std::make_shared<EntitlementConfigFromEnv>(delegateUri, app_config, conn_mode);    
}

//-- EntitlementConfigFromEnv

bool is_path(const std::string& str) {
    if (str.empty()) {
        return false;
    }
    return str.at(0) == '/';
}

EntitlementConfigFromEnv::EntitlementConfigFromEnv(const std::string& delegateUri, const oes::osdu::OsduConfig& app_config, const std::string& cnn_mode) {
    authorization_mode_ = app_config.authorizationMode();

    if (delegateUri.empty()) { // if the delegate is empty, then use given storage/entitlement urls
        storage_url_ = app_config.storageUrl();
        entitlements_url_ = app_config.entitlementsUrl();
    } else { // otherwise we combine the authZ url with the given path
        auto storage_path = app_config.storageUrl();
        if (storage_path.empty() || storage_path == "osdu-pattern" || !is_path(storage_path)) {
            storage_path = "/api/storage/v2";
        }

        auto entitlements_path = app_config.entitlementsUrl();
        if(entitlements_path.empty() || entitlements_path == "osdu-pattern" || !is_path(entitlements_path)) {
            entitlements_path = "/api/entitlements/v2";
        }
        storage_url_ = delegateUri + storage_path;
        entitlements_url_ = delegateUri + entitlements_path;
    }
    policy_delegate_url_ = delegateUri;
    data_partition_id_ = app_config.dataPartitionID();
    const std::string schema_id = app_config.schemaID();
    schema_id_ = schema_id.empty() ? "osdu" : schema_id;
    domain_name_ = app_config.domainName();
    legal_tags_ = app_config.legalTags();
    legal_countries_ = app_config.legalCountries();
    records_path_ = app_config.recordsPath();
    evaluatePolicyPath_ = app_config.evalutePolicyPath();
    connectivity_mode_ = cnn_mode;
    authorization_bulk_size_ = app_config.authorizationBulkSize();
    
    OES_INFO("EntitlementUrl: {}", entitlements_url_);
    OES_INFO("Storage: {}", storage_url_);
}

EntitlementConfigFromEnv::~EntitlementConfigFromEnv() {
}

//--

void EntitlementError::reset() {
    code_ = -1;
    status_.clear();
}

bool EntitlementError::not_err() const {
    return 0 == code_ || 200 == code_ || 201 == code_ || 204 == code_;
}

bool EntitlementError::equals(const EntitlementError& rhs) const {
    return code_ == rhs.code_ && status_ == rhs.status_;    
}

std::ostream& EntitlementError::print(std::ostream& os) const {
    os << "CODE: " << code_ << " STATUS: " << status_ << std::endl;
    return os;
}

std::string EntitlementError::to_string() const {
    return fmt::format("CODE: {0} STATUS: {1}", code_, status_);
}


} // namespace oes::osdu::api