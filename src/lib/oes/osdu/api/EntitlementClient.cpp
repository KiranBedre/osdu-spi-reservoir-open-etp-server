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

class EntitlementClient::Impl {
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
    static std::string GROUPS_QUERY;
};

std::string EntitlementClient::Impl::GROUPS_QUERY("/groups");

EntitlementClient::Impl::Impl(EntitlementConfig_ptr config, BaseClient_ptr baseClient) :
    config_(config),
    base_client_(baseClient) {
}

std::set<std::string> EntitlementClient::Impl::parseResponseBodyToList(
    const oes::core::HTTP::Response& res,
    const char* listRootKey, 
    const char* listItemKey,
    EntitlementError& err) const {
    
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

std::string EntitlementClient::Impl::prepareBodyForPutGroup(
    const std::string& groupName, const std::string& permission) const {
    nlohmann::json j;
    j["name"] = groupName;
    j["description"] = std::string(fmt::format("Dataspace {} group", permission));
    return j.dump();
}

std::string EntitlementClient::Impl::parseEmailFromResponseBody(
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

//-

EntitlementClient::EntitlementClient(EntitlementConfig_ptr config, BaseClient_ptr baseClient) :
    impl_(new Impl(config, baseClient)) {
}

EntitlementClient::~EntitlementClient() {
    delete impl_;
}

std::tuple<std::string, std::set<std::string>> EntitlementClient::listGroups(
    const std::string& bearerToken,
    EntitlementError& err
) {
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(!impl_->config_->entitlementsUrl().empty());
    std::string req_url = impl_->config_->entitlementsUrl() + Impl::GROUPS_QUERY;
    const std::vector<std::string> additional_headers = {
        "Accept: application/json"
    };
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
    if (res.code != 200) {
        return {};
    }

    const char* MEMBER_EMAIL = "memberEmail";
    const std::string memberEmail = impl_->parseEmailFromResponseBody(res.body, MEMBER_EMAIL);

    const char* GROUPS = "groups";
    const char* EMAIL = "email";
    const std::set<std::string> groups = impl_->parseResponseBodyToList(res, GROUPS, EMAIL, err);

    return std::make_tuple(memberEmail, groups);
}

//--

class EntitlementDataspaceClient::Impl {
public:
    Impl(EntitlementConfig_ptr config, BaseClient_ptr baseClient);

public:
    EntitlementConfig_ptr config_;
    BaseClient_ptr base_client_;
};

EntitlementDataspaceClient::Impl::Impl(EntitlementConfig_ptr config, BaseClient_ptr baseClient) :
    config_(config),
    base_client_(baseClient) {
}

EntitlementDataspaceClient::EntitlementDataspaceClient(
    EntitlementConfig_ptr config, BaseClient_ptr baseClient) : 
    impl_(new Impl(config, baseClient)) {
}

EntitlementDataspaceClient::~EntitlementDataspaceClient() {
    delete impl_;
}

bool EntitlementDataspaceClient::createDataspaceRecord(
    const std::string& bearerToken, 
    const EntitlementDataspaceGroups& dataspace,
    EntitlementError& err) {
    assert(dataspace.isValid());
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(!impl_->config_->storageUrl().empty());
    std::string req_url = 
        impl_->config_->storageUrl() + impl_->config_->recordsPath();
    std::string body = dataspace.serialize(*(impl_->config_));
    // telemetry
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                    {{"http.method", "PUT"},
                                     {"http.url", req_url}});
    // make request
    oes::core::HTTP::Response res = impl_->base_client_->makeRequest(
        oes::core::HTTP::Method::ePUT,
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

bool EntitlementDataspaceClient::delDataspaceRecord(
    const std::string& bearerToken, 
    const EntitlementDataspaceGroups& dataspace,
    EntitlementError& err) {
    assert(dataspace.isValid());
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(!impl_->config_->storageUrl().empty());
    // Logical delete: a DELETE (purge) requires the storage admin role
    std::string req_url = 
        impl_->config_->storageUrl() + impl_->config_->recordsPath() + "/" + dataspace.idForQuery(*(impl_->config_)) + ":delete";
    // telemetry
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                    {{"http.method", "POST"},
                                     {"http.url", req_url}});
    // make request
    oes::core::HTTP::Response res = impl_->base_client_->makeRequest(
        oes::core::HTTP::Method::ePOST,
        bearerToken,
        req_url,
        {}/*queryParams*/,
        {}/*additionalHeaders*/,
        {}/*Fields params*/,
        nullptr/*body*/
    );
    span.setAttr("response.code", res.code);

    err.code_ = res.code;
    err.status_ = res.status;
    if (res.code != 200 &&
        res.code != 204 &&
        res.code != 404 // not found, proceed with deletion
    ) {
        return false;
    }
    return true;
}

bool EntitlementDataspaceClient::getDataspaceRecord(
    const std::string& bearerToken, 
    EntitlementDataspaceGroups& dataspace,
    EntitlementError& err) const {
    assert(dataspace.isValid());
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(!impl_->config_->storageUrl().empty());

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
        OES_ERROR("Error on parsing getDataspaceRecord body.");
        err.code_ = -1;
        err.status_ = fmt::format("Failed to parse response for dataspace record: {} {}", 
            dataspace.uid_, dataspace.path_);
        return false;
    }
    return true;
}

bool EntitlementDataspaceClient::getDataspaceRecordsBulk(
    const std::string& bearerToken,
    const EntitlementDataspaceGroupsBulk& dataspaces,
    std::vector<bool>& found,
    EntitlementError& err) const {
    assert(impl_->base_client_);
    assert(impl_->config_);
    assert(!impl_->config_->storageUrl().empty());

    found.assign(dataspaces.dataspaces_.size(), false);
    if (dataspaces.dataspaces_.empty()) {
        return true;
    }

    const std::string req_url = impl_->config_->storageUrl() + impl_->config_->recordsPath();
    const std::vector<std::string> additional_headers = {
        "Accept: application/json"
    };
    const auto index_by_id = dataspaces.indexById(*(impl_->config_));

    std::string cursor;
    do {
        const std::string query = dataspaces.queryParams(*(impl_->config_), cursor);
        // cppcheck-suppress unreadVariable
        auto span = oes::core::OtelSpan("EntitlementClient." + std::string(__func__),
                                        {{"http.method", "GET"},
                                         {"http.url", req_url + query}});
        oes::core::HTTP::Response res = impl_->base_client_->makeRequest(
            oes::core::HTTP::Method::eGET,
            bearerToken,
            req_url,
            query,
            additional_headers,
            {}/*Fields params*/,
            nullptr/*body*/
        );
        span.setAttr("response.code", res.code);
        err.code_ = res.code;
        err.status_ = res.status;
        if (res.code == 403) { // no storage access at all
            found.assign(dataspaces.dataspaces_.size(), false);
            return true;
        }
        if (res.code != 200) {
            err.status_ += " " + res.body;
            return false;
        }
        if (!dataspaces.deserialize(res.body, index_by_id, found, cursor)) {
            err.code_ = -1;
            err.status_ = "Failed to parse response while listing dataspace records.";
            return false;
        }
    } while (!cursor.empty());

    return true;
}

} // namespace oes::osdu::api