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

#ifndef OES_OSDU_API_ENTITLEMENTCLIENTPOLICY_H
#define OES_OSDU_API_ENTITLEMENTCLIENTPOLICY_H

#include <memory>
#include <set>
#include <tuple>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include <oes/osdu/export.h>
#include <oes/osdu/OsduConfig.h>
#include <oes/osdu/api/DSPermission.h>
#include <oes/osdu/api/EntitlementUtils.h>
#include <oes/osdu/api/EntitlementDataspace.h>

namespace oes::osdu::api {

class BaseClient;
using BaseClient_ptr = std::shared_ptr<BaseClient>;

/**
 * EntitlementClientPolicy: class to handle requests to policy based authorization to entitlement service.
 * 
 * An import remark is that the impl class has an instance to EntitlementClient. This is done since the osdu entitlement service
 * is not yet done. This way we access the group based authorization api and "convert" the permissions using the rule bellow:
 * 
 * If a user has viewer permission, then this user has only read permission.
 * If a user has owners permission, then this user has read, create, delete and update permissions.
 * If a user has neither viewer or owners permissions, then this user has not permission at all.
 * 
 * TODO this conversion is a temporary approach. As soon as the policy based authorization is entitlemented on osdu entitlement
 * TODO service, this class need to be reviewed!
*/
class OES_OSDU_EXPORT EntitlementClientPolicy {
public:
    EntitlementClientPolicy(EntitlementConfig_ptr config, BaseClient_ptr baseClient);
    ~EntitlementClientPolicy();

    EntitlementClientPolicy(const EntitlementClientPolicy&) = delete;
    EntitlementClientPolicy& operator=(const EntitlementClientPolicy&) = delete;

    DSPermission getUserPermission(
        const std::string& bearerToken,
        EntitlementError& err);

private:
    class Impl;
    Impl* impl_;
};

using EntitlementClientPolicy_ptr = std::shared_ptr<EntitlementClientPolicy>;

//--

/**
 * EntitlementDataspaceClientPolicy: class that handles requests to policy based authorization dataspace service.
 * 
 * Using osdu authorization mode, then it sends requests following the policy api: https://community.opengroup.org/osdu/platform/security-and-compliance/policy/-/blob/master/docs/openapi.yaml
 * Using the simple authorization mode, then it sends simpler body containing only the operation and dataspace url.
*/
class OES_OSDU_EXPORT EntitlementDataspaceClientPolicy {
public:
    EntitlementDataspaceClientPolicy(EntitlementConfig_ptr config, BaseClient_ptr baseClient);
    ~EntitlementDataspaceClientPolicy();

    EntitlementDataspaceClientPolicy(const EntitlementDataspaceClientPolicy&) = delete;
    EntitlementDataspaceClientPolicy& operator=(const EntitlementDataspaceClientPolicy&) = delete;

    bool getDataspaceUserPermission(
        const std::string& user_id,
        std::string& bearerToken,
        const EntitlementDataspacePolicy& dataspace,
        const DSPermissionFlags& operation,
        EntitlementError& err,
        const std::string& client_id = ""
    );
    std::vector<bool> getAllDataspacesUserPermission(
        const std::string& user_id,
        std::string& bearerToken,
        const EntitlementDataspacePolicyBulk& dataspace,
        const DSPermissionFlags& operation,
        EntitlementError& err,
        const std::string& client_id = ""
    );

    bool createDataspaceRecord(
        const std::string& bearerToken, 
        const EntitlementDataspacePolicy& dataspace,
        EntitlementError& err);
    bool delDataspaceRecord(
        const std::string& bearerToken, 
        const EntitlementDataspacePolicy& dataspace,
        EntitlementError& err);
    bool getDataspaceRecord(
        const std::string& bearerToken, 
        EntitlementDataspacePolicy& dataspace,
        EntitlementError& err) const;

private:
    class Impl;
    Impl* impl_;
};

using EntitlementDataspaceClientPolicy_ptr = std::shared_ptr<EntitlementDataspaceClientPolicy>;

} // namespace oes::osdu::api

#endif // OES_OSDU_API_ENTITLEMENTCLIENTPOLICY_H