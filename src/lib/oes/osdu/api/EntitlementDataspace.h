// ============================================================================
// Copyright 2024 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2024 EPAM Systems
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
#ifndef OES_OSDU_API_ENTITLEMENTDATASPACE_H
#define OES_OSDU_API_ENTITLEMENTDATASPACE_H

#include <memory>
#include <set>
#include <tuple>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include "../export.h"
//* OES_OSDU_EXPORT

#include <oes/osdu/api/EntitlementUtils.h>
#include <oes/osdu/api/DSPermission.h>

namespace oes::osdu::api {

enum class EntitlementDSType: unsigned short {
    eBase,
    eGroups,
    ePolicy,
    ePolicyBulk
};

/**
 * EntitlementDataspace: base entitlement dataspace class 
 * 
 * It is designed to contain EntitlementDataspace client, that deals the requests/responses itself while the EntitlementDataspace
 * class are designed to serialzie/deserialize body, define headers, and so on.
*/
class OES_OSDU_EXPORT EntitlementDataspace {
public:
    EntitlementDataspace();
    EntitlementDataspace(const EntitlementDSType& type);
    EntitlementDataspace(
        const std::string& uid, const std::string& path,
        const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries
    );

    bool isValid() const;
    explicit operator bool() const { return isValid(); }

    std::string urlId() const;
    EntitlementDSType getEntitlementType() const;
    
    virtual std::string idForQuery(const EntitlementConfigBase& config_osdu) const = 0;

    std::string uid_;
    std::string path_;
    std::string uri_; // uri ready to be used
    std::vector<std::string> legal_tags_;
    std::vector<std::string> legal_countries_;
    int64_t created_ = 0; // Microseconds since epoch
    std::optional<int64_t> last_write_; // Microseconds since epoch

    EntitlementDSType entl_type_;

protected:
    EntitlementDataspace(
        const std::string& uid, const std::string& path,
        const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries,
        const EntitlementDSType& type
    );

};
using EntitlementDataspace_ptr = std::shared_ptr<EntitlementDataspace>;

// -- 
/**
 * EntitlementDataspaceGroups: entitlement dataspace for groups based authorization
*/
class OES_OSDU_EXPORT EntitlementDataspaceGroups: public EntitlementDataspace {
public:
    EntitlementDataspaceGroups();
    EntitlementDataspaceGroups(
        const std::string& uid, const std::string& path,
        const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries,
        const std::vector<std::string>& viewers={}, const std::vector<std::string>& owners={}
    );

    bool equals(const EntitlementDataspaceGroups& rhs) const;
    bool lessThan(const EntitlementDataspaceGroups& rhs) const;

    std::string idForQuery(const EntitlementConfigBase& config_osdu) const override;

    std::string serialize(const EntitlementConfigBase& config_osdu) const;
    bool deserialize(const std::string& res_body);
    bool hasPermission(const std::string& permission, const std::set<std::string>& listGroups) const;
    void addGroup(const std::string& permission, const std::string& groupName);
    bool checkGroups(const std::set<std::string>& osduGroups) const;

    static const std::string DATASET_TYPE;
    static const std::string VERSION;

    // groups for dataspace managed via entitlement service
    std::set<std::string> viewers_group_;
    std::set<std::string> owners_group_;
};
inline bool operator==(const EntitlementDataspaceGroups& lhs, const EntitlementDataspaceGroups& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const EntitlementDataspaceGroups& lhs, const EntitlementDataspaceGroups& rhs) {
    return !lhs.equals(rhs);
}
inline bool operator<(const EntitlementDataspaceGroups& lhs, const EntitlementDataspaceGroups& rhs) {
    return lhs.lessThan(rhs);
}
using EntitlementDataspaceGroups_ptr = std::shared_ptr<EntitlementDataspaceGroups>;

EntitlementDataspaceGroups_ptr dataspaceGroups_factory(const std::string& uid, const std::string& path,
        const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries);

// -- 

using PermissionMap = std::unordered_map<std::string, DSPermission>;

/**
 * EntitlementDataspacePolicy: entitlement dataspace for policy based authorization.
 * Has serialize methods for both authorization modes (osdu or simple).
*/
class OES_OSDU_EXPORT EntitlementDataspacePolicy: public EntitlementDataspace {
public:
    EntitlementDataspacePolicy();
    EntitlementDataspacePolicy(
        const std::string& uid,
        const std::string& path,
        const std::string& uri,
        const std::vector<std::string>& legal_tags,
        const std::vector<std::string>& legal_countries,
        const PermissionMap& usersPermissions_map = {}
    );

    bool equals(const EntitlementDataspacePolicy& rhs) const;
    bool lessThan(const EntitlementDataspacePolicy& rhs) const;

    std::string idForQuery(const EntitlementConfigBase& config_osdu) const override;
    
    std::string serialize(const EntitlementConfigBase& config_osdu) const;
    std::string serializePolicy(
        const EntitlementConfigBase& config_osdu, 
        const DSPermissionFlags& operation) const;
    std::string serializeSimplePolicy(
        const DSPermissionFlags& operation) const;
    std::string serializeSimpleRecord() const;

    bool deserialize(const std::string& res_body);
    bool deserializePolicy(const std::string& res_body) const;
    bool deserializeSimplePolicy(const std::string& res_body) const;

    bool hasValidPermission(const std::string& user_key, const DSPermissionFlags& permission) const;
    bool hasPermission(const std::string& user_key, const DSPermissionFlags& permission) const;
    void addUserPermission(const std::string& user_key, const DSPermissionFlags& permission);
    void removeUser(const std::string& user_key);
    bool checkUserPermissions(const std::string& user_key) const;

    static const std::string DATASET_TYPE;
    static const std::string VERSION;

    PermissionMap usersPermissions_;
};
inline bool operator==(const EntitlementDataspacePolicy& lhs, const EntitlementDataspacePolicy& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const EntitlementDataspacePolicy& lhs, const EntitlementDataspacePolicy& rhs) {
    return !lhs.equals(rhs);
}
inline bool operator<(const EntitlementDataspacePolicy& lhs, const EntitlementDataspacePolicy& rhs) {
    return lhs.lessThan(rhs);
}
using EntitlementDataspacePolicy_ptr = std::shared_ptr<EntitlementDataspacePolicy>;

// --
/**
 * EntitlementDataspacePolicyBulk: entitlement dataspace for policy based authorization.
 * Has serialize methods for simple authorization mode and does not keep any cache on it.
*/
using urisVector_ptr = std::shared_ptr<std::vector<std::string>>;

class OES_OSDU_EXPORT EntitlementDataspacePolicyBulk: public EntitlementDataspace {
public:
    EntitlementDataspacePolicyBulk();
    EntitlementDataspacePolicyBulk(
        std::vector<std::string> uris,
        const std::vector<std::string>& legal_tags,
        const std::vector<std::string>& legal_countries
    );

    std::string idForQuery(const EntitlementConfigBase& config_osdu) const override;
    
    std::vector<std::string> serialize(
        const EntitlementConfigBase& config_osdu,
        const DSPermissionFlags& operation
        ) const;

    std::vector<bool> deserialize(const std::string& res_body) const;

    urisVector_ptr uris_ptr_;
};

using EntitlementDataspacePolicyBulk_ptr = std::shared_ptr<EntitlementDataspacePolicyBulk>;

} // namespace oes::osdu::api

#endif // OES_OSDU_API_ENTITLEMENTDATASPACE_H