// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
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
#ifndef OES_COMMON_AUTH_ENTITLEMENT_H
#define OES_COMMON_AUTH_ENTITLEMENT_H

#include "../export.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <iosfwd>
#include <optional>

namespace oes::core {
    class HTTP;
}

namespace oes::osdu::api {
    class DSPermission;
    class EntitlementConfigBase;
    using EntitlementOSDUConfig_ptr = std::shared_ptr<EntitlementConfigBase>;
    class EntitlementDataspace;
    using EntitlementDataspace_ptr = std::shared_ptr<EntitlementDataspace>;
    class EntitlementDataspacePolicy;
    using EntitlementDataspacePolicy_ptr = std::shared_ptr<EntitlementDataspacePolicy>;
    class EntitlementError;
}

namespace oes::common::auth {

struct AuthenticatedUser;
using AuthenticatedUser_ptr = std::shared_ptr<AuthenticatedUser>;
using AuthenticatedUser_wptr = std::weak_ptr<AuthenticatedUser>;

using HTTP_ptr = std::shared_ptr<oes::core::HTTP>;
using HTTPFactory = std::function<HTTP_ptr()>;

struct OES_COMMON_EXPORT EntitlementConfig {
    HTTPFactory HTTP_factory_;
    oes::osdu::api::EntitlementOSDUConfig_ptr osdu_;
};

//
enum class EntitlementType {
    Groups,
    Policy
};

// Entitlement base
/**
 * The Entitlement class is a base class for Groups and Policy based authorizations.
*/
class OES_COMMON_EXPORT Entitlement {
public:
    ~Entitlement();
protected:
    explicit Entitlement(const EntitlementType& entitlement_type);

    Entitlement(const Entitlement& rhs) = delete;
    Entitlement& operator=(const Entitlement& rhs) = delete;
    Entitlement(Entitlement&& rhs) = delete;
    Entitlement& operator=(Entitlement&& rhs) = delete;

public:
    virtual const oes::osdu::api::EntitlementError& err() const = 0;

    virtual bool isValid() const = 0;
    virtual std::string getMemberEmail() const = 0;

    virtual bool hasServicePermissionCreate() = 0;
    virtual bool hasServicePermissionRead() = 0;
    virtual bool hasServicePermissionUpdate() = 0;
    virtual bool hasServicePermissionDelete() = 0;

    virtual bool canCreateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) = 0;
    virtual bool canReadDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) = 0;
    virtual bool canUpdateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) = 0;
    virtual bool canDeleteDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) = 0;

    virtual bool registerDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) = 0;
    virtual bool unregisterDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) = 0;
   
    bool isGroupBased() const;
    bool isPolicyBased() const;

    virtual const EntitlementConfig& config() const = 0;
public:
    const EntitlementType entitlement_type;
};

using Entitlement_ptr = std::shared_ptr<Entitlement>;

// Entitlement for group based authorization
/**
 * EntitlementGroups: manages groups based authorization (viewers and owners)
*/
class OES_COMMON_EXPORT EntitlementGroups: public Entitlement {
public:
    explicit EntitlementGroups(const EntitlementConfig& config);
    ~EntitlementGroups();

    EntitlementGroups(const EntitlementGroups& rhs) = delete;
    EntitlementGroups& operator=(const EntitlementGroups& rhs) = delete;
    EntitlementGroups(EntitlementGroups&& rhs) = delete;
    EntitlementGroups& operator=(EntitlementGroups&& rhs) = delete;

    const oes::osdu::api::EntitlementError& err() const;

    void init(
        const std::string& dataPartitionId,
        AuthenticatedUser_ptr authenticatedUser,
        const std::string& memberEmail,
        const std::vector<std::string>& groups);

    bool isValid() const;

    std::string viewersGroupMail() const;
    std::string ownersGroupMail() const;
    bool isUserInViewersGroup() const;
    bool isUserInOwnersGroup() const;

    std::string getMemberEmail() const override;

    bool hasServicePermissionCreate() override;
    bool hasServicePermissionRead() override;
    bool hasServicePermissionUpdate() override;
    bool hasServicePermissionDelete() override;

    bool canCreateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool canReadDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool canUpdateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool canDeleteDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;

    bool registerDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool unregisterDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;

    const EntitlementConfig& config() const override;
private:
    bool isUserInGroup(const std::string& group) const; // entitlement service groups

public:
    static const std::string RESERVOIR_DMS_VIEWERS_GROUP;
    static const std::string RESERVOIR_DMS_OWNERS_GROUP;
    static const std::string VIEWERS;
    static const std::string OWNERS;

private:
    class Impl;
    Impl* impl_ = nullptr;
};

using EntitlementGroups_ptr = std::shared_ptr<EntitlementGroups>;

// Policy based authorzation
/**
 * EntitlementPolicy: Manages the policy based authorization.
 * userId can be any user key and it will be used to store the permissions that user has.
*/
class OES_COMMON_EXPORT EntitlementPolicy: public Entitlement {
public:
    explicit EntitlementPolicy(const EntitlementConfig& config);
    ~EntitlementPolicy();

    EntitlementPolicy(const EntitlementPolicy& rhs) = delete;
    EntitlementPolicy& operator=(const EntitlementPolicy& rhs) = delete;
    EntitlementPolicy(EntitlementPolicy&& rhs) = delete;
    EntitlementPolicy& operator=(EntitlementPolicy&& rhs) = delete;

    const oes::osdu::api::EntitlementError& err() const override;
        
    void init(
        const std::string& dataPartitionId,
        AuthenticatedUser_ptr authenticatedUser,
        const std::string& userId,
        const std::string& client_id,
        const oes::osdu::api::DSPermission& permission);

    bool isValid() const override;

    const std::string& getUserId() const; // user identification to keep the permissions
    std::string getMemberEmail() const override;

    bool hasServicePermissionCreate() override;
    bool hasServicePermissionRead() override;
    bool hasServicePermissionUpdate() override;
    bool hasServicePermissionDelete() override;

    bool canCreateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool canReadDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool canUpdateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool canDeleteDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;

    std::vector<bool> canCreateDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace);
    std::vector<bool> canReadDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace);
    std::vector<bool> canUpdateDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace);
    std::vector<bool> canDeleteDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace);

    bool registerDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;
    bool unregisterDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) override;

    const EntitlementConfig& config() const override;
private:
    class Impl;
    Impl* impl_ = nullptr;
};

using EntitlementPolicy_ptr = std::shared_ptr<EntitlementPolicy>;

OES_COMMON_EXPORT
Entitlement_ptr entitlement_factory(const EntitlementType& authZ_mode, const EntitlementConfig& config);

} // namespace oes::common::auth

#endif // OES_COMMON_AUTH_ENTITLEMENT_H

