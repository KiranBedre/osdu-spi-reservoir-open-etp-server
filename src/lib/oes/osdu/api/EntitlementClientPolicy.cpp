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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/osdu/api/EntitlementClientPolicy.h>
#include <oes/osdu/api/EntitlementClient.h>

#include <oes/core/utils/Json.hpp>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/String.h>
#include <oes/core/utils/Otel.h>

#include <oes/osdu/api/BaseClient.h>


using oes::core::Guid;

//DBG
//#include <iostream>
//DBG
namespace oes::osdu::api {

class EntitlementClientPolicy::Impl {
public:
    Impl(EntitlementConfig_ptr config, BaseClient_ptr baseClient);

    std::set<std::string> parseResponseBodyToList(
        const oes::core::HTTP::Response& res,
        const char* listRootKey, 
        const char* listItemKey,
        EntitlementError& err) const;

    std::string prepareBodyForPutGroup(
        const std::string& groupName, const std::string& permission) const;

    std::string parseEmailFromResponseBody(
        const std::string& body,
        const char* emailKey) const;

public:
    EntitlementConfig_ptr config_;
    BaseClient_ptr base_client_;

public:
    DSPermission tmp_getGroupsPermission(const std::string& header, EntitlementError& err); //TODO remove it after policy based entitlement api designed
    oes::osdu::api::EntitlementClient_ptr tmp_entitlement_client_groups_; //TODO remove it after OSDU policy based entitlement service is designed/developed
};

EntitlementClientPolicy::Impl::Impl(EntitlementConfig_ptr config, BaseClient_ptr baseClient) :
    config_(config),
    base_client_(baseClient),
    tmp_entitlement_client_groups_ { std::make_shared<EntitlementClient>(config_, base_client_) } {
}

std::set<std::string> EntitlementClientPolicy::Impl::parseResponseBodyToList(
    const oes::core::HTTP::Response& res,
    const char* listRootKey, 
    const char* listItemKey,
    EntitlementError& err) const {
    //DBG
    //std::cout << "CODE: " << res.code << " STATUS: " << res.status << "\n";
    //std::cout << "BODY (" << res.body.size() << " bytes):" << std::endl;
    //DBG
    if (!oes::core::json_validate_syntax(res.body.c_str(), res.body.size())) {
        err.code_ = -1;
        err.status_ = "Failed to get list of groups. Invalid JSON.";
        return {};
    }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(res.body);
    } catch (nlohmann::json::parse_error& ex) {
        err.code_ = -1;
        err.status_ = fmt::format(
            "Failed to get list of groups.\nParse json  error at byte {}.", 
            ex.byte);
        return {};
    }

    if (!j.contains(listRootKey)) {
        return {};
    }
    auto j_list_root = j[listRootKey];
    if (!j_list_root.is_array() || j_list_root.empty()) {
        return {};
    }
    std::set<std::string> group_list;
    for (auto it = j_list_root.begin(); it != j_list_root.end(); ++it) {
        if (it->contains(listItemKey)) {
            auto j_item_key = (*it)[listItemKey];
            if (j_item_key.is_string()) {
                const std::string group_name(j_item_key.get<std::string>());
                OES_DEBUG("Entitlements received Group Name: {}.", group_name);
                group_list.emplace(group_name);
            }
        }
    }
    return group_list;
}

std::string EntitlementClientPolicy::Impl::prepareBodyForPutGroup(
    const std::string& groupName, const std::string& permission) const {
    nlohmann::json j;
    j["name"] = groupName;
    j["description"] = std::string(fmt::format("Dataspace {} group", permission));
    return j.dump();
}

std::string EntitlementClientPolicy::Impl::parseEmailFromResponseBody(
    const std::string& body,
    const char* emailKey = "email"
) const {
    if (body.empty()) {
        return {};
    }
    nlohmann::json j = nlohmann::json::parse(body);
    if (j.contains(emailKey)) {
        return j[emailKey];
    }
    return {};
}

DSPermission EntitlementClientPolicy::Impl::tmp_getGroupsPermission(const std::string& header, EntitlementError& err) {
    auto [mail, groups] = tmp_entitlement_client_groups_->listGroups(header, err);

    if (err.code_ != 200 || groups.empty()) {
        return oes::osdu::api::DSPermission::makeNonePermission(false);
    }

    const std::string owner_mail = fmt::format("{0}@{1}.{2}", 
        "service.reservoir-dms.owners", config_->dataPartitionId(), config_->domainName());
    
    const std::string viewer_mail = fmt::format("{0}@{1}.{2}", 
        "service.reservoir-dms.viewers", config_->dataPartitionId(), config_->domainName());
    
    bool is_owner { groups.find(owner_mail) != groups.end() };
    bool is_viewer { groups.find(viewer_mail) != groups.end() };

    if (is_owner) { // if owner, we can do everything
        return oes::osdu::api::DSPermission::makeAllPermission(true);
    } else if (is_viewer) { // if viewer, we can just read
        return oes::osdu::api::DSPermission::makeReadPermission(true, false);
    } // no permission at all

    return oes::osdu::api::DSPermission::makeNonePermission(true, false);
}

//-

EntitlementClientPolicy::EntitlementClientPolicy(EntitlementConfig_ptr config, BaseClient_ptr baseClient) :
    impl_(new Impl(config, baseClient)) {
}

EntitlementClientPolicy::~EntitlementClientPolicy() {
    delete impl_;
}

DSPermission EntitlementClientPolicy::getUserPermission(
    const std::string& bearerToken,
    EntitlementError& err
) {
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(!impl_->config_->entitlementsUrl().empty());
    if(impl_->config_->authorizationMode() == "simple") {
        return DSPermission::makeAllPermission(true);
    }
    
    return impl_->tmp_getGroupsPermission(bearerToken, err);
}

//--

class EntitlementDataspaceClientPolicy::Impl {
public:
    Impl(EntitlementConfig_ptr config, BaseClient_ptr baseClient);

public:
    EntitlementConfig_ptr config_;
    BaseClient_ptr base_client_;
};

EntitlementDataspaceClientPolicy::Impl::Impl(EntitlementConfig_ptr config, BaseClient_ptr baseClient) :
    config_(config),
    base_client_(baseClient) {
}

EntitlementDataspaceClientPolicy::EntitlementDataspaceClientPolicy(
    EntitlementConfig_ptr config, BaseClient_ptr baseClient) : 
    impl_(new Impl(config, baseClient)) {
}

EntitlementDataspaceClientPolicy::~EntitlementDataspaceClientPolicy() {
    delete impl_;
}

bool EntitlementDataspaceClientPolicy::getDataspaceUserPermission(
        const std::string& user_id,
        std::string& bearerToken,
        const EntitlementDataspacePolicy& dataspace,
        const DSPermissionFlags& operation,
        EntitlementError& err,
        const std::string& client_id
    ) {
    assert(dataspace.isValid());
    assert(impl_->base_client_);
    assert(impl_->config_);
    
    auto simple_mode = impl_->config_->authorizationMode() == "simple";

    std::string req_url = impl_->config_->policyDelegateUrl() + impl_->config_->evaluatePolicyPath();
    
    std::string query;
    std::string body;
    std::vector<std::string> headers;
    headers.emplace_back("x-user-id: " + user_id);
    if (simple_mode) {
        headers.emplace_back("X-Etp-ClientId: " + client_id);
        body = dataspace.serializeSimplePolicy(operation);
        query = ""; // empty
    } else { // osdu codification
        query = fmt::format("?policy_id={}&include_auth=true", "policy_id");
        body = dataspace.serializePolicy(*(impl_->config_), operation);
    }

    
    auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                {{"http.method", "POST"},
                                    {"http.url", req_url},
                                    {"http.body", body},
                                    {"operation", getPermissionName(operation)}});

    oes::core::HTTP::Response res = impl_->base_client_->makeRequest(
        oes::core::HTTP::Method::ePOST,
        bearerToken,
        req_url,
        query,
        headers,
        {},
        &body
    );

    span.setAttr("response.code", res.code);
    err.code_ = res.code;
    err.status_ = res.status;
    if (res.code != 200) {
        err.status_ += " " + res.body;
        return false;
    }

    if (simple_mode) {
        return dataspace.deserializeSimplePolicy(res.body);
    }
    return dataspace.deserializePolicy(res.body);
}

std::vector<bool> EntitlementDataspaceClientPolicy::getAllDataspacesUserPermission(
        const std::string& user_id,
        std::string& bearerToken,
        const EntitlementDataspacePolicyBulk& dataspace,
        const DSPermissionFlags& operation,
        EntitlementError& err,
        const std::string& client_id
    ) {
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(impl_->config_->authorizationMode() == "simple");
    assert(dataspace.getEntitlementType() == EntitlementDSType::ePolicyBulk);

    std::string req_url = impl_->config_->policyDelegateUrl() + impl_->config_->evaluatePolicyPath();
    
    std::string query;
    std::vector<std::string> bodies;
    std::vector<std::string> headers;

    headers.emplace_back("x-user-id: " + user_id);
    headers.emplace_back("X-Etp-ClientId: " + client_id);
    
    bodies = dataspace.serialize(*(impl_->config_), operation);
    query = ""; // empty
    std::vector<bool> authorizations;

    for (auto& body : bodies) {
        auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                    {{"http.method", "POST"},
                                        {"http.url", req_url},
                                        {"http.body", body},
                                        {"operation", getPermissionName(operation)}});

        oes::core::HTTP::Response res = impl_->base_client_->makeRequest( // async?
            oes::core::HTTP::Method::ePOST,
            bearerToken,
            req_url,
            query,
            headers,
            {},
            &body
        );

        span.setAttr("response.code", res.code);
        err.code_ = res.code;
        err.status_ = res.status;
        if (res.code != 200) {
            err.status_ += " " + res.body;
            return {};
        }

        auto bulk_res = dataspace.deserialize(res.body);
        authorizations.insert(authorizations.end(), bulk_res.begin(), bulk_res.end());
    }

    return authorizations;
}

bool EntitlementDataspaceClientPolicy::createDataspaceRecord(
    const std::string& bearerToken, 
    const EntitlementDataspacePolicy& dataspace,
    EntitlementError& err) {
    assert(dataspace.isValid());
    assert(impl_->base_client_);
    assert(impl_->config_);

    std::string req_url;
    std::string body;
    oes::core::HTTP::Method method;
    if (impl_->config_->authorizationMode() == "simple") {
        req_url = impl_->config_->policyDelegateUrl() + impl_->config_->evaluatePolicyPath() + "/register";
        body = dataspace.serializeSimpleRecord();
        method = oes::core::HTTP::Method::ePOST;
    } else {
        assert(!impl_->config_->storageUrl().empty());
        req_url = impl_->config_->storageUrl() + impl_->config_->recordsPath();
        body = dataspace.serialize(*(impl_->config_));
        method = oes::core::HTTP::Method::ePUT;
    }

    
    // telemetry
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                    {{"http.method", "PUT"},
                                     {"http.url", req_url}});
    // make request
    oes::core::HTTP::Response res = impl_->base_client_->makeRequest(
        method,
        bearerToken,
        req_url,
        {}/*queryParams*/,
        {}/*additionalHeaders*/,
        {}/*Fields params*/,
        &body
    );
    span.setAttr("response.code", res.code);    
    err.code_ = res.code;
    err.status_ = res.status;
    if (res.code != 201) {
        err.status_ += " " + res.body;
        return false;
    }
    return true;
}

bool EntitlementDataspaceClientPolicy::delDataspaceRecord(
    const std::string& bearerToken, 
    const EntitlementDataspacePolicy& dataspace,
    EntitlementError& err) {
    assert(dataspace.isValid());
    assert(impl_->base_client_);
    assert(impl_->config_);

    std::string req_url;
    std::unique_ptr<std::string> body(nullptr);
    oes::core::HTTP::Method method;
    const bool simple_mode = impl_->config_->authorizationMode() == "simple";
    if (simple_mode) {
        req_url = impl_->config_->policyDelegateUrl() + impl_->config_->evaluatePolicyPath() + "/unregister";
        body = std::make_unique<std::string>(dataspace.serializeSimpleRecord());
        method = oes::core::HTTP::Method::ePOST;
    } else {
        assert(!impl_->config_->storageUrl().empty());
        // Logical delete: a DELETE (purge) requires the storage admin role
        req_url = impl_->config_->storageUrl() + impl_->config_->recordsPath() + "/" + dataspace.idForQuery(*(impl_->config_)) + ":delete";
        // body is nullptr
        method = oes::core::HTTP::Method::ePOST;
    }
    
    // telemetry
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                    {{"http.method", (method == oes::core::HTTP::Method::eDELETE) ? "DELETE" : "POST"},
                                     {"http.url", req_url}});
    // make request
    oes::core::HTTP::Response res = impl_->base_client_->makeRequest(
        method,
        bearerToken,
        req_url,
        {}/*queryParams*/,
        {}/*additionalHeaders*/,
        {}/*Fields params*/,
        body.get()/*body*/
    );
    span.setAttr("response.code", res.code);
    
    err.code_ = res.code;
    err.status_ = res.status;

    if (simple_mode && res.code == 423) {
        err.status_ += " Resource is locked. " + res.body;
        return false;
    }
    else if (res.code != 200 && res.code != 204) {
        return false;
    }
    return true;
}

bool EntitlementDataspaceClientPolicy::getDataspaceRecord(
    const std::string& bearerToken, 
    EntitlementDataspacePolicy& dataspace,
    EntitlementError& err) const {
    assert(dataspace.isValid());
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(!impl_->config_->storageUrl().empty());

    if (impl_->config_->authorizationMode() == "simple") {
        OES_INFO("Can not get dataspace record for simple authorzion mode");
        return false;
    }

    std::string req_url = 
        impl_->config_->storageUrl() + impl_->config_->recordsPath() + "/" + dataspace.idForQuery(*(impl_->config_));

    const std::vector<std::string> additional_headers = {
        "Accept: application/json"
    };
    // telemetry
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                    {{"http.method", "GET"},
                                     {"http.url", req_url}});
    oes::core::HTTP::Response res = impl_->base_client_->makeRequest(
        oes::core::HTTP::Method::eGET,
        bearerToken,
        req_url,
        {}/*queryParams*/,
        additional_headers,
        {}/*Fields params*/,
        nullptr/*body*/
    );
    span.setAttr("response.code", res.code);
    err.code_ = res.code;
    err.status_ = res.status;
    switch (res.code) {
        case 200:
            break; // continue
        case 403: // record exists but not accessible
            return false;
        case 404: // record not found, give access
            break; // continue
        default:
            return false;
    }
    if (!dataspace.deserialize(res.body)) {
        err.code_ = -1;
        err.status_ = fmt::format("Failed to parse response for dataspace record: {} {}", 
            dataspace.uid_, dataspace.path_);
        return false;
    }
    return true;
}

} // namespace oes::osdu::api