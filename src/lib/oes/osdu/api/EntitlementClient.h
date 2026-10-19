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

#ifndef OES_OSDU_API_ENTITLEMENTCLIENT_H
#define OES_OSDU_API_ENTITLEMENTCLIENT_H

#include <memory>
#include <set>
#include <tuple>
#include <string>
#include <vector>
#include <optional>

#include <oes/osdu/export.h>
#include <oes/osdu/OsduConfig.h>
#include <oes/osdu/api/EntitlementUtils.h>
#include <oes/osdu/api/EntitlementDataspace.h>

namespace oes::osdu::api {

class BaseClient;
using BaseClient_ptr = std::shared_ptr<BaseClient>;

/**
 * EntitlementClient: class to handle client side requests when using groups based authorization.
 * It sends requests to entitlement service
*/
class OES_OSDU_EXPORT EntitlementClient {
public:
    EntitlementClient(EntitlementConfig_ptr config, BaseClient_ptr baseClient);
    ~EntitlementClient();

    EntitlementClient(const EntitlementClient&) = delete;
    EntitlementClient& operator=(const EntitlementClient&) = delete;

    std::tuple<std::string, std::set<std::string>> listGroups(
        const std::string& bearerToken,
        EntitlementError& err);
    
private:
    class Impl;
    Impl* impl_;
};

using EntitlementClient_ptr = std::shared_ptr<EntitlementClient>;

//--

/**
 * EntitlementDataspaceClient: class to handle client side requests when using groups based authorization.
 * It sends requests to dataspace entitlement service
*/
class OES_OSDU_EXPORT EntitlementDataspaceClient {
public:
    EntitlementDataspaceClient(EntitlementConfig_ptr config, BaseClient_ptr baseClient);
    ~EntitlementDataspaceClient();

    EntitlementDataspaceClient(const EntitlementDataspaceClient&) = delete;
    EntitlementDataspaceClient& operator=(const EntitlementDataspaceClient&) = delete;

    bool createDataspaceRecord(
        const std::string& bearerToken, 
        const EntitlementDataspaceGroups& dataspace,
        EntitlementError& err);
    bool delDataspaceRecord(
        const std::string& bearerToken, 
        const EntitlementDataspaceGroups& dataspace,
        EntitlementError& err);
    bool getDataspaceRecord(
        const std::string& bearerToken, 
        EntitlementDataspaceGroups& dataspace,
        EntitlementError& err) const;

private:
    class Impl;
    Impl* impl_;
};

using EntitlementDataspaceClient_ptr = std::shared_ptr<EntitlementDataspaceClient>;

} // namespace oes::osdu::api

#endif // OES_OSDU_API_ENTITLEMENTCLIENT_H