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

#ifndef OES_OSDU_API_ENTITLEMENTCONFIG_H
#define OES_OSDU_API_ENTITLEMENTCONFIG_H

#include <memory>
#include <string>

#include <oes/osdu/export.h>
#include <oes/osdu/OsduConfig.h>

namespace oes::osdu::api {

/**
 * EntitlementConfigBase: contains all needed configuration for authorization
*/
class OES_OSDU_EXPORT EntitlementConfigBase {
protected:
    EntitlementConfigBase() = default;
public:
    virtual ~EntitlementConfigBase();
    const std::string& authorizationMode() const;
    const std::string& storageUrl() const;
    const std::string& entitlementsUrl() const;
    const std::string& policyDelegateUrl() const;
    const std::string& recordsPath() const;
    const std::string& dataPartitionId() const;
    const std::string& schemaId() const;
    const std::string& domainName() const;
    const std::string& legalTags() const;
    const std::string& legalCountries() const;
    const std::string& connectivityMode() const;
    const std::string& evaluatePolicyPath() const;
    const std::string& authorizationBulkSize() const;

    void setDataPartitionId(const std::string& dataPartitionId);
    void setConnectivityMode(const std::string& mode);
    void setAuthorizationMode(const std::string& mode);

    bool isValid() const;

    static std::shared_ptr<EntitlementConfigBase> make(const std::string& delegateUri, const oes::osdu::OsduConfig& app_config, const std::string& conn_mode);

protected:
    std::string authorization_mode_;
    std::string storage_url_;
    std::string entitlements_url_;
    std::string policy_delegate_url_;
    std::string records_path_;
    std::string data_partition_id_;
    std::string schema_id_;
    std::string domain_name_;
    std::string legal_tags_;
    std::string legal_countries_;
    std::string connectivity_mode_;
    std::string evaluatePolicyPath_;
    std::string authorization_bulk_size_;
};

using EntitlementConfig_ptr = std::shared_ptr<EntitlementConfigBase>;

//--

class OES_OSDU_EXPORT EntitlementConfigFromEnv : public EntitlementConfigBase {
public:
    explicit EntitlementConfigFromEnv(const std::string& delegateUri, const oes::osdu::OsduConfig& app_config, const std::string& cnn_mode);
    EntitlementConfigFromEnv(const EntitlementConfigFromEnv&) = delete;
    EntitlementConfigFromEnv& operator=(const EntitlementConfigFromEnv&) = delete;
    ~EntitlementConfigFromEnv();
};

// -- 

class OES_OSDU_EXPORT EntitlementError {
public:
    void reset();
    bool not_err() const;
    bool equals(const EntitlementError& rhs) const;
    std::ostream& print(std::ostream& os) const;
    std::string to_string() const;
    explicit operator bool() const { return !not_err(); }

    short code_ = -1;
    std::string status_;
    //TODO may be std::string details_; ?
};
inline bool operator==(const EntitlementError& lhs, const EntitlementError& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const EntitlementError& lhs, const EntitlementError& rhs) {
    return !lhs.equals(rhs);
}
inline std::ostream& operator<<(std::ostream& os, const EntitlementError& er) {
    return er.print(os);
}

} // namespace oes::osdu::api

#endif // OES_OSDU_API_ENTITLEMENTCONFIG_H