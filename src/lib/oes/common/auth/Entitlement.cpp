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

//DBG
//#include <iostream>
//DBG
#include <algorithm>
#include <set>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Entitlement.h"
#include "AuthN.h"

#include <oes/common/ErrorMessages.h>
#include <oes/osdu/api/EntitlementClient.h>
#include <oes/osdu/api/EntitlementClientPolicy.h>
#include <oes/osdu/api/BaseClient.h>
#include <oes/osdu/api/DSPermission.h>

#include <oes/core/http/HTTP.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Otel.h>


namespace oes::common::auth {

using oes::osdu::api::DSPermissionFlags;

Entitlement::Entitlement(const EntitlementType& entitlement_type)
    : entitlement_type(entitlement_type) {}

Entitlement::~Entitlement() {}

bool Entitlement::isGroupBased() const {
    return entitlement_type == EntitlementType::Groups;
}

bool Entitlement::isPolicyBased() const {
    return entitlement_type == EntitlementType::Policy;
}

// -- 

class EntitlementGroups::Impl {
public:
    explicit Impl(const EntitlementConfig& config) :
        config_(config){
        assert(config_.HTTP_factory_);
    }

    void init(
        const std::string& dataPartitionId,
        AuthenticatedUser_ptr authenticatedUser,
        const std::string& memberEmail,
        const std::vector<std::string>& groups);

    // uses current_dataspace_
    const std::string& getMemberEmail();
    const std::set<std::string>& listGroups();
    bool createDataspaceRecord();
    bool delDataspaceRecord();
    bool getDataspaceRecord();

    void clearCachedData();

public:
    oes::osdu::api::EntitlementError error_;

    EntitlementConfig config_;
    auth::AuthenticatedUser_wptr auth_user_;
    HTTP_ptr HTTP_;
    oes::osdu::api::BaseClient_ptr base_client_;
    oes::osdu::api::EntitlementClient_ptr entitlement_client_;
    oes::osdu::api::EntitlementDataspaceClient_ptr entitlement_dataspace_client_;

    // memberEmail and member groups
    bool memberEmail_cached_ = false;
    bool groups_cached_ = false;
    std::string memberEmail_;
    std::set<std::string> groups_;

    oes::osdu::api::EntitlementDataspaceGroups_ptr current_dataspace_;
};

void EntitlementGroups::Impl::init(
    const std::string& dataPartitionId,
    AuthenticatedUser_ptr authenticatedUser,
    const std::string& memberEmail,
    const std::vector<std::string>& groups
) {
    error_.reset();
    clearCachedData();
    assert(authenticatedUser);
    assert(config_.HTTP_factory_);
    auth_user_ = authenticatedUser;
    HTTP_ = config_.HTTP_factory_();
    if (!HTTP_) {
        error_.status_ = "Failed to create HTTP object for EntitlementGroups service.";
        return;
    } else {
        error_.code_ = 0;
        error_.status_ = "Inited OK";
    }
    config_.osdu_->setDataPartitionId(dataPartitionId);
    base_client_ = std::make_shared<oes::osdu::api::BaseClient>(config_.osdu_->dataPartitionId(), config_.HTTP_factory_);
    entitlement_client_ = 
        std::make_shared<oes::osdu::api::EntitlementClient>(config_.osdu_, base_client_);
    entitlement_dataspace_client_ = 
        std::make_shared<oes::osdu::api::EntitlementDataspaceClient>(config_.osdu_, base_client_);

    // init memberEmail and member groups
    if(!memberEmail.empty()) {
        memberEmail_ = memberEmail;
        memberEmail_cached_ = true;
    }

    if(groups.size() > 0){
        for(const std::string& g : groups){
            groups_.emplace(g);
        }
        groups_cached_ = true;
    }
}

const std::string& EntitlementGroups::Impl::getMemberEmail() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.Impl.") + __func__);
    if (memberEmail_cached_) {
        return memberEmail_;
    }
    auto au = auth_user_.lock();
    assert(au);

    auto [memberEmail, groups] = entitlement_client_->listGroups(au->header_, error_);
    memberEmail_ = memberEmail;
    memberEmail_cached_ = true;
    groups_ = groups;
    groups_cached_ = (error_.code_ == 200);
    return memberEmail_;
}

const std::set<std::string>& EntitlementGroups::Impl::listGroups() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.Impl.") + __func__);
    if (groups_cached_) {
        return groups_;
    }
    auto au = auth_user_.lock();
    assert(au);

    auto [memberEmail, groups] = entitlement_client_->listGroups(au->header_, error_);
    memberEmail_ = memberEmail;
    memberEmail_cached_ = true;
    groups_ = groups;
    // TODO expiration
    groups_cached_ = (error_.code_ == 200);
    return groups_;
}

bool EntitlementGroups::Impl::createDataspaceRecord() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.Impl.") + __func__);
    auto au = auth_user_.lock();
    assert(au);
    assert(current_dataspace_ && current_dataspace_->isValid());
    error_.reset();

    // Checking if owners and viewrs groups are provided (they must be provided)
    const auto& ds {*current_dataspace_};
    if ( ds.owners_group_.empty() ) {
        error_.status_ = error_messages::OWNERS_FIELD_EMPTY;
        return false;
    }

    return entitlement_dataspace_client_->createDataspaceRecord(
        au->header_, *current_dataspace_, error_);
}

bool EntitlementGroups::Impl::delDataspaceRecord() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.Impl.") + __func__);
    auto au = auth_user_.lock();
    assert(au);
    assert(current_dataspace_ && current_dataspace_->isValid());
    error_.reset();
    return entitlement_dataspace_client_->delDataspaceRecord(
        au->header_, *current_dataspace_, error_);
}

bool EntitlementGroups::Impl::getDataspaceRecord() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.Impl.") + __func__);
    auto au = auth_user_.lock();
    assert(au);
    assert(current_dataspace_ && current_dataspace_->isValid());
    error_.reset();
    // TODO cache dataspace?
    return entitlement_dataspace_client_->getDataspaceRecord(
        au->header_, *current_dataspace_, error_);
}

void EntitlementGroups::Impl::clearCachedData() {
    if (groups_cached_) {
        groups_cached_ = false;
        groups_.clear();
    }
}

//--

const std::string EntitlementGroups::RESERVOIR_DMS_VIEWERS_GROUP("service.reservoir-dms.viewers");
const std::string EntitlementGroups::RESERVOIR_DMS_OWNERS_GROUP("service.reservoir-dms.owners");
const std::string EntitlementGroups::VIEWERS("viewers");
const std::string EntitlementGroups::OWNERS("owners");

EntitlementGroups::EntitlementGroups(const EntitlementConfig& config)
    : Entitlement(EntitlementType::Groups),
      impl_(new Impl(config)) {}

EntitlementGroups::~EntitlementGroups() {
    delete impl_;
}

const oes::osdu::api::EntitlementError& EntitlementGroups::err() const {
    return impl_->error_;
}

void EntitlementGroups::init(
    const std::string& dataPartitionId,
    AuthenticatedUser_ptr authenticatedUser,
    const std::string& memberEmail,
    const std::vector<std::string>& groups
) {
    impl_->init(dataPartitionId, authenticatedUser, memberEmail, groups);
}

bool EntitlementGroups::isValid() const {
    return bool(impl_->HTTP_) && impl_->config_.osdu_ && impl_->config_.osdu_->isValid();
}

std::string EntitlementGroups::viewersGroupMail() const{
    return fmt::format("{0}@{1}.{2}", 
        RESERVOIR_DMS_VIEWERS_GROUP, impl_->config_.osdu_->dataPartitionId(), impl_->config_.osdu_->domainName());
}

bool EntitlementGroups::isUserInViewersGroup() const {
    return isUserInGroup(viewersGroupMail());
}

std::string EntitlementGroups::ownersGroupMail() const{
    return fmt::format("{0}@{1}.{2}", 
        RESERVOIR_DMS_OWNERS_GROUP, impl_->config_.osdu_->dataPartitionId(), impl_->config_.osdu_->domainName());
}

bool EntitlementGroups::isUserInOwnersGroup() const {
    const std::string owners_group_mail(ownersGroupMail());
    OES_DEBUG("Entitlements expects user to have group {0}.", owners_group_mail);
    return isUserInGroup(owners_group_mail);
}

std::string EntitlementGroups::getMemberEmail() const {
    return impl_->getMemberEmail();
}

/*!
 * \brief Check if the user has service authorization to create
 */
bool EntitlementGroups::hasServicePermissionCreate() {
    return isUserInOwnersGroup();
}

/*!
 * \brief Check if the user has service authorization to read
 */
bool EntitlementGroups::hasServicePermissionRead() {
    return isUserInViewersGroup();
}

/*!
 * \brief Check if the user has service authorization to update
 */
bool EntitlementGroups::hasServicePermissionUpdate() {
    return isUserInOwnersGroup();
}

/*!
 * \brief Check if the user has service authorization to delete
 */
bool EntitlementGroups::hasServicePermissionDelete() {
    return isUserInOwnersGroup();
}

bool EntitlementGroups::canCreateDataspace(oes::osdu::api::EntitlementDataspace_ptr /*dataspace*/) {
    const bool ok = isUserInOwnersGroup();
    if (!ok) {
        OES_WARNING("canCreateDataspace: user is not a member of the required owners group");
    }
    return ok;
}

bool EntitlementGroups::canReadDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.") + __func__);

    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspaceGroups>(dataspace);
    if(isValid()){
        if (!impl_->getDataspaceRecord()) {
            OES_WARNING("canReadDataspace: could not load dataspace record for '{}' (record fetch failed, code {})",
                dataspace ? dataspace->path_ : std::string(), impl_->error_.code_);
            return false;
        }
    }else if(impl_->current_dataspace_->viewers_group_.size()>0){
        impl_->error_.code_ = 200;
    }
    const std::set<std::string>& groups = impl_->listGroups();

    const bool ok = impl_->current_dataspace_->hasPermission(VIEWERS, groups) ||
                    impl_->current_dataspace_->hasPermission(OWNERS, groups);
    if (!ok) {
        OES_WARNING("canReadDataspace: user is not a member of the viewers or owners group for dataspace '{}'",
            dataspace ? dataspace->path_ : std::string());
    }
    return ok;
}

bool EntitlementGroups::canUpdateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.") + __func__);
    
    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspaceGroups>(dataspace);
    if(isValid()){
        if (!impl_->getDataspaceRecord()) {
            OES_WARNING("canUpdateDataspace: could not load dataspace record for '{}' (record fetch failed, code {})",
                dataspace ? dataspace->path_ : std::string(), impl_->error_.code_);
            return false;
        }
    }else if(impl_->current_dataspace_->owners_group_.size()>0){
        impl_->error_.code_ = 200;
    }
    const std::set<std::string>& groups = impl_->listGroups();

    const bool ok = impl_->current_dataspace_->hasPermission(OWNERS, groups);
    if (!ok) {
        OES_WARNING("canUpdateDataspace: user is not a member of the owners group for dataspace '{}'",
            dataspace ? dataspace->path_ : std::string());
    }
    return ok;
}

bool EntitlementGroups::canDeleteDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.") + __func__);

    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspaceGroups>(dataspace);
    if(isValid()){
        if (!impl_->getDataspaceRecord()) {
            OES_WARNING("canDeleteDataspace: could not load dataspace record for '{}' (record fetch failed, code {})",
                dataspace ? dataspace->path_ : std::string(), impl_->error_.code_);
            return false;
        }
    }else if(impl_->current_dataspace_->owners_group_.size()>0){
        impl_->error_.code_ = 200;
    }
    const std::set<std::string>& groups = impl_->listGroups();

    const bool ok = impl_->current_dataspace_->hasPermission(OWNERS, groups);
    if (!ok) {
        OES_WARNING("canDeleteDataspace: user is not a member of the owners group for dataspace '{}'",
            dataspace ? dataspace->path_ : std::string());
    }
    return ok;
}

bool EntitlementGroups::registerDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.") + __func__);
    if( !dataspace ){
        OES_ERROR("registerDataspace: dataspace pointer is null");
        impl_->error_.reset();
        impl_->error_.status_ = error_messages::NULL_DATASPACE_REFERENCE;
        return false;
    }

    // Start from a clean error state so a stale reason from a previous
    // dataspace in the same batch cannot leak into this one.
    impl_->error_.reset();

    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspaceGroups>(dataspace);

    // Checking if groups are already in OSDU
    if( impl_->current_dataspace_ &&
        !impl_->current_dataspace_->checkGroups(impl_->listGroups())
    ) {
        OES_ERROR("registerDataspace: checkGroups failed for dataspace '{}' - "
            "required owner group not found in user's {} groups",
            dataspace->path_, impl_->listGroups().size());
        impl_->error_.status_ = error_messages::NOT_MEMBER_OWNER_GROUP;
        return false;
    }

    // Validate ACL fields before creating the dataspace record
    if (impl_->config_.osdu_->authorizationMode() == "osdu" && impl_->current_dataspace_) {
        oes::osdu::api::EntitlementDataspaceGroups_ptr ds = impl_->current_dataspace_;
        if (ds->legal_countries_.empty() ||
            ds->legal_tags_.empty() ||
            ds->viewers_group_.empty() ||
            ds->owners_group_.empty()
        ) {
            OES_ERROR("registerDataspace: ACL validation failed for dataspace '{}' - "
                "legal_countries:{}, legal_tags:{}, viewers:{}, owners:{}",
                dataspace->path_,
                ds->legal_countries_.empty() ? "EMPTY" : std::to_string(ds->legal_countries_.size()),
                ds->legal_tags_.empty() ? "EMPTY" : std::to_string(ds->legal_tags_.size()),
                ds->viewers_group_.empty() ? "EMPTY" : std::to_string(ds->viewers_group_.size()),
                ds->owners_group_.empty() ? "EMPTY" : std::to_string(ds->owners_group_.size()));
            impl_->error_.status_ = error_messages::REQUIRED_FIELDS_EMPTY;
            return false;
        }
    }

    // Already exists, remove current occurrence
    auto result = impl_->getDataspaceRecord();
    if (impl_->error_.code_ == 200) {
            if(!unregisterDataspace(dataspace)){
                OES_ERROR("registerDataspace: failed to unregister existing dataspace record for '{}'", dataspace->path_);
                impl_->error_.status_ = error_messages::REPLACE_EXISTING_RECORD_FAILED;
                return false;
            }
    } else if (impl_->error_.code_ == 403) {// exists but does not have access to it
        OES_ERROR("registerDataspace: user lacks access to existing dataspace record for '{}' (HTTP 403)", dataspace->path_);
        impl_->error_.status_ = error_messages::DATASPACE_EXISTS_NO_ACCESS;
        return false;
    } // 404 is passed, can create record
    

    // create storage dataspace record
    if (!impl_->createDataspaceRecord()) {
        if (impl_->error_.status_.empty()) {
            impl_->error_.status_ = error_messages::STORAGE_CREATE_RECORD_FAILED;
        }
        OES_ERROR("registerDataspace: createDataspaceRecord failed for '{}': {}",
            dataspace->path_, impl_->error_.status_);
        return false;
    }
    return true;
}

bool EntitlementGroups::unregisterDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.") + __func__);
    if (dataspace) {
        impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspaceGroups>(dataspace);
    }

    // Start from a clean error state so a stale reason cannot leak into this call.
    impl_->error_.reset();

    const std::string ds_path = dataspace ? dataspace->path_ : std::string("(current dataspace)");

    // checking if dataspace belongs to owners
    const auto& groups {impl_->listGroups()};
    if (!impl_->current_dataspace_->hasPermission(OWNERS, groups)) {
        OES_ERROR("unregisterDataspace: user is not a member of the required owner group for dataspace '{}'", ds_path);
        impl_->error_.status_ = error_messages::NOT_MEMBER_OWNER_GROUP;
        return false;
    }

    // delete storage dataspace record
    if (!impl_->delDataspaceRecord()) {
        OES_ERROR("unregisterDataspace: delDataspaceRecord failed for '{}': {}", ds_path, impl_->error_.status_);
        if (impl_->error_.status_.empty()) {
            impl_->error_.status_ = error_messages::STORAGE_DELETE_RECORD_FAILED;
        }
        return false;
    }

    return true;
}

bool EntitlementGroups::isUserInGroup(const std::string& theGroup) const {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementGroups.") + __func__);
    const auto& groups = impl_->listGroups();
    if (groups.empty()) {
        return false;
    }
    return groups.find(theGroup) != groups.end();
}

const EntitlementConfig& EntitlementGroups::config() const {
    return impl_->config_;
}

// Entitlement for policy based authorization

using DSPermission = oes::osdu::api::DSPermission;

class EntitlementPolicy::Impl {
public:
    explicit Impl(const EntitlementConfig& config) :
        config_(config),
        userPermissions_ent_ {oes::osdu::api::DSPermission::makeNonePermission(false)}{
        assert(config_.HTTP_factory_);
    }

    void init(
        const std::string& dataPartitionId,
        AuthenticatedUser_ptr authenticatedUser,
        const std::string& userId,
        const std::string& client_id,
        const oes::osdu::api::DSPermission& permission);

    // uses current_dataspace_
    std::string getMemberEmail();
    const std::string& getUserId();
    const DSPermission& getUserPermission();
    bool hasPermission(const DSPermissionFlags& flag);

    bool createDataspaceRecord();
    bool delDataspaceRecord();
    bool getDataspaceRecord();

    void clearCachedData();

public:
    oes::osdu::api::EntitlementError error_;

    EntitlementConfig config_;
    auth::AuthenticatedUser_wptr auth_user_;
    HTTP_ptr HTTP_;
    oes::osdu::api::BaseClient_ptr base_client_;
    oes::osdu::api::EntitlementClientPolicy_ptr entitlement_client_;
    oes::osdu::api::EntitlementDataspaceClientPolicy_ptr entitlement_dataspace_client_;

    std::string userId_;
    DSPermission userPermissions_ent_;
    std::string client_id_;

    oes::osdu::api::EntitlementDataspacePolicy_ptr current_dataspace_;
};

void EntitlementPolicy::Impl::init(
    const std::string& dataPartitionId,
    AuthenticatedUser_ptr authenticatedUser,
    const std::string& userId,
    const std::string& client_id,
    const oes::osdu::api::DSPermission& permission
) {
    error_.reset();
    clearCachedData();
    assert(authenticatedUser);
    assert(config_.HTTP_factory_);
    assert(!userId.empty());
    auth_user_ = authenticatedUser;
    HTTP_ = config_.HTTP_factory_();
    if (!HTTP_) {
        error_.status_ = "Failed to create HTTP object for EntitlementPolicy service.";
        return;
    } else {
        error_.code_ = 0;
        error_.status_ = "Inited OK";
    }
    config_.osdu_->setDataPartitionId(dataPartitionId);
    base_client_ = std::make_shared<oes::osdu::api::BaseClient>(config_.osdu_->dataPartitionId(), config_.HTTP_factory_);
    entitlement_client_ = 
        std::make_shared<oes::osdu::api::EntitlementClientPolicy>(config_.osdu_, base_client_);
    entitlement_dataspace_client_ = 
        std::make_shared<oes::osdu::api::EntitlementDataspaceClientPolicy>(config_.osdu_, base_client_);

    userId_ = userId;

    userPermissions_ent_ = permission;
    client_id_ = client_id;
}

std::string EntitlementPolicy::Impl::getMemberEmail() {
    auto au = auth_user_.lock();
    if (au) {
        return au->email_;
    }
    return "";
}

const std::string& EntitlementPolicy::Impl::getUserId() {
    return userId_;
}

const DSPermission& EntitlementPolicy::Impl::getUserPermission() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.Impl.") + __func__);
    
    if (config_.osdu_->authorizationMode() == "simple") {
        userPermissions_ent_ = oes::osdu::api::DSPermission::makeAllPermission(true);
        return userPermissions_ent_;
    }

    auto au = auth_user_.lock();
    assert(au);

    // Using groups based request
    userPermissions_ent_ = entitlement_client_->getUserPermission(au->header_, error_);    

    return userPermissions_ent_;
}

bool EntitlementPolicy::Impl::hasPermission(const DSPermissionFlags& flag) {
    if (!userPermissions_ent_.isValid(flag)) {
        getUserPermission();
    }

    return userPermissions_ent_.hasPermission(flag);
}

bool EntitlementPolicy::Impl::createDataspaceRecord() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.Impl.") + __func__);

    auto au = auth_user_.lock();
    assert(au);
    assert(current_dataspace_ && current_dataspace_->isValid());
    error_.reset();

    const auto& ds {*current_dataspace_};
    if ( ds.usersPermissions_.empty() || !ds.hasPermission(userId_, DSPermissionFlags::eCreate) ) {
        return false;
    }

    return entitlement_dataspace_client_->createDataspaceRecord(
        au->header_, *current_dataspace_, error_);
}

bool EntitlementPolicy::Impl::delDataspaceRecord() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.Impl.") + __func__);

    auto au = auth_user_.lock();
    assert(au);
    assert(current_dataspace_ && current_dataspace_->isValid());

    error_.reset();

    const auto& ds {*current_dataspace_};
    if ( ds.usersPermissions_.empty() || !ds.hasPermission(userId_, DSPermissionFlags::eDelete) ) {
        return false;
    }

    return entitlement_dataspace_client_->delDataspaceRecord(
        au->header_, *current_dataspace_, error_);
}

bool EntitlementPolicy::Impl::getDataspaceRecord() {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.Impl.") + __func__);
    
    // If the authorization mode is simple, then we are not allowed to get any record
    if (config_.osdu_->authorizationMode() == "simple") {
        const std::string msg("Entitlement Policy getDataspaceRecord is not implemented for simple authorization mode");
        span.setError(msg);
        throw std::logic_error(msg);
        return false;
    }

    auto au = auth_user_.lock();
    assert(au);
    assert(current_dataspace_ && current_dataspace_->isValid());
    error_.reset();

    const auto& ds {*current_dataspace_};
    if ( ds.usersPermissions_.empty() || !ds.hasPermission(userId_, DSPermissionFlags::eRead) ) {
        return false;
    }
    
    return entitlement_dataspace_client_->getDataspaceRecord(
        au->header_, *current_dataspace_, error_);
}

void EntitlementPolicy::Impl::clearCachedData() {
    userPermissions_ent_ = oes::osdu::api::DSPermission::makeNonePermission(false);
    if (current_dataspace_) {
        current_dataspace_->removeUser(userId_);
    }

    userId_ = "";
}

//--

EntitlementPolicy::EntitlementPolicy(const EntitlementConfig& config)
    : Entitlement(EntitlementType::Policy),
      impl_(new Impl(config)) {}

EntitlementPolicy::~EntitlementPolicy() {
    delete impl_;
}

const oes::osdu::api::EntitlementError& EntitlementPolicy::err() const {
    return impl_->error_;
}

void EntitlementPolicy::init(
    const std::string& dataPartitionId,
    AuthenticatedUser_ptr authenticatedUser,
    const std::string& userId,
    const std::string& client_id,
    const oes::osdu::api::DSPermission& permission
) {
    impl_->init(dataPartitionId, authenticatedUser, userId, client_id, permission);
}

bool EntitlementPolicy::isValid() const {
    return bool(impl_->HTTP_) && impl_->config_.osdu_ && impl_->config_.osdu_->isValid();
}

const std::string& EntitlementPolicy::getUserId() const {
    return impl_->userId_;
}

std::string EntitlementPolicy::getMemberEmail() const {
    return impl_->getMemberEmail(); 
}

/*!
 * \brief Check if the user has service authorization to create
 */
bool EntitlementPolicy::hasServicePermissionCreate() {
    return impl_->hasPermission(DSPermissionFlags::eCreate);
}

/*!
 * \brief Check if the user has service authorization to read
 */
bool EntitlementPolicy::hasServicePermissionRead() {
    return impl_->hasPermission(DSPermissionFlags::eRead);
}

/*!
 * \brief Check if the user has service authorization to update
 */
bool EntitlementPolicy::hasServicePermissionUpdate() {
    return impl_->hasPermission(DSPermissionFlags::eUpdate);
}

/*!
 * \brief Check if the user has service authorization to delete
 */
bool EntitlementPolicy::hasServicePermissionDelete() {
    return impl_->hasPermission(DSPermissionFlags::eDelete);
}

bool EntitlementPolicy::canCreateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable=
    assert(dataspace);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);

    if (!hasServicePermissionCreate()) { // Entitlement service permission 1/2
        return false;
    }
    
    const auto flag = DSPermissionFlags::eCreate;
    
    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicy>(dataspace);
    
    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check the cache
    if (!impl_->current_dataspace_->isValid() || !impl_->current_dataspace_->hasValidPermission(user_id, flag) ) {
        auto res = impl_->entitlement_dataspace_client_->getDataspaceUserPermission(
            user_id,
            au->header_,
            *(impl_->current_dataspace_),
            flag,
            impl_->error_,
            impl_->client_id_
        );

        if (res) {
            impl_->current_dataspace_->addUserPermission(user_id, flag);
        }
    }
    
    return impl_->current_dataspace_->hasPermission(user_id, flag); // dataspace permission 2/2
}

bool EntitlementPolicy::canReadDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    assert(dataspace);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);

    if (!hasServicePermissionRead()) { // Entitlement service permission 1/2
        return false;
    }

    const auto flag = DSPermissionFlags::eRead;
    
    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicy>(dataspace);

    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check the cache
    if (!impl_->current_dataspace_->isValid() || !impl_->current_dataspace_->hasValidPermission(user_id, flag)) {
        auto res = impl_->entitlement_dataspace_client_->getDataspaceUserPermission(
            user_id,
            au->header_,
            *(impl_->current_dataspace_),
            flag,
            impl_->error_,
            impl_->client_id_
        );

        if (res) {
            impl_->current_dataspace_->addUserPermission(user_id, flag);
        }
    }

    return impl_->current_dataspace_->hasPermission(user_id, flag); // Dataspace permission 2/2
}

bool EntitlementPolicy::canUpdateDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    assert(dataspace);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);
    
    if (!hasServicePermissionUpdate()) { // Entitlement service permission 1/2
        return false;
    }
    
    const auto flag = DSPermissionFlags::eUpdate;
    
    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicy>(dataspace);
    
    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check the cache
    if (!impl_->current_dataspace_->isValid() || !impl_->current_dataspace_->hasValidPermission(user_id, flag)) {
        auto res = impl_->entitlement_dataspace_client_->getDataspaceUserPermission(
            user_id,
            au->header_,
            *(impl_->current_dataspace_),
            flag,
            impl_->error_,
            impl_->client_id_
        );

        if (res) {
            impl_->current_dataspace_->addUserPermission(user_id, flag);
        }
    }

    return impl_->current_dataspace_->hasPermission(user_id, flag); // Dataspace permission 2/2
}

bool EntitlementPolicy::canDeleteDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    assert(dataspace);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);

    if (!hasServicePermissionDelete()) { // Entitlement service permission 1/2
        return false;
    }
    
    const auto flag = DSPermissionFlags::eDelete;
        
    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicy>(dataspace);
    
    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check for cache
    if (!impl_->current_dataspace_->isValid() || !impl_->current_dataspace_->hasValidPermission(user_id, flag)) {
        auto res = impl_->entitlement_dataspace_client_->getDataspaceUserPermission(
            user_id,
            au->header_,
            *(impl_->current_dataspace_),
            flag,
            impl_->error_,
            impl_->client_id_
        );

        if (res) {
            impl_->current_dataspace_->addUserPermission(user_id, flag);
        }
    }

    return impl_->current_dataspace_->hasPermission(user_id, flag); // Dataspace permission 2/2
}

std::vector<bool> EntitlementPolicy::canCreateDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable=
    assert(dataspace);
    assert(dataspace->getEntitlementType() == oes::osdu::api::EntitlementDSType::ePolicyBulk);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);

    if (!hasServicePermissionCreate()) { // Entitlement service permission 1/2
        return {};
    }
    
    const auto flag = DSPermissionFlags::eCreate;
    
    auto ds_ptr = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicyBulk>(dataspace);
    
    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check the cache
    
    auto res = impl_->entitlement_dataspace_client_->getAllDataspacesUserPermission(
        user_id,
        au->header_,
        *(ds_ptr),
        flag,
        impl_->error_,
        impl_->client_id_
    );
    
    
    return res; // dataspace permission 2/2
}

std::vector<bool> EntitlementPolicy::canReadDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    assert(dataspace);
    assert(dataspace->getEntitlementType() == oes::osdu::api::EntitlementDSType::ePolicyBulk);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);

    if (!hasServicePermissionRead()) { // Entitlement service permission 1/2
        return {};
    }

    const auto flag = DSPermissionFlags::eRead;
    
    auto ds_ptr = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicyBulk>(dataspace);

    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check the cache
    
    auto res = impl_->entitlement_dataspace_client_->getAllDataspacesUserPermission(
        user_id,
        au->header_,
        *(ds_ptr),
        flag,
        impl_->error_,
        impl_->client_id_
    );    

    return res; // Dataspace permission 2/2
}

std::vector<bool> EntitlementPolicy::canUpdateDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    assert(dataspace);
    assert(dataspace->getEntitlementType() == oes::osdu::api::EntitlementDSType::ePolicyBulk);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);
    
    if (!hasServicePermissionUpdate()) { // Entitlement service permission 1/2
        return {};
    }
    
    const auto flag = DSPermissionFlags::eUpdate;
    
    auto ds_ptr = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicyBulk>(dataspace);
    
    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check the cache
    
    auto res = impl_->entitlement_dataspace_client_->getAllDataspacesUserPermission(
        user_id,
        au->header_,
        *(ds_ptr),
        flag,
        impl_->error_,
        impl_->client_id_
    );

    return res; // Dataspace permission 2/2
}

std::vector<bool> EntitlementPolicy::canDeleteDataspaceBulk(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    assert(dataspace);
    assert(dataspace->getEntitlementType() == oes::osdu::api::EntitlementDSType::ePolicyBulk);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);

    if (!hasServicePermissionDelete()) { // Entitlement service permission 1/2
        return {};
    }
    
    const auto flag = DSPermissionFlags::eDelete;
        
    auto ds_ptr = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicyBulk>(dataspace);
    
    auto au = impl_->auth_user_.lock();
    auto user_id = impl_->getUserId();
    // Check for cache
    
    auto res = impl_->entitlement_dataspace_client_->getAllDataspacesUserPermission(
        user_id,
        au->header_,
        *(ds_ptr),
        flag,
        impl_->error_,
        impl_->client_id_
    );
    

    return res; // Dataspace permission 2/2
}

bool EntitlementPolicy::registerDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);
    if( !dataspace ){
        return false;
    }
    
    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicy>(dataspace);
    
    // Checking if user permission is already there
    auto userId = impl_->getUserId();
    if(!impl_->current_dataspace_->checkUserPermissions(userId)) {
        return false;
    }

    // Already exists, remove current occurrence
    if (impl_->config_.osdu_->authorizationMode() != "simple" && impl_->getDataspaceRecord()) {
        if(!unregisterDataspace(dataspace)){
            return false;
        }
    }

    // create storage dataspace record
    if (!impl_->createDataspaceRecord()) {
        return false;
    }
    return true;
}

bool EntitlementPolicy::unregisterDataspace(oes::osdu::api::EntitlementDataspace_ptr dataspace) {
    // cppcheck-suppress unreadVariable
    assert(dataspace);
    auto span = oes::core::OtelSpan(std::string("EntitlementPolicy.") + __func__);
    
    impl_->current_dataspace_ = std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspacePolicy>(dataspace);

    // checking if dataspace belongs to owners
    const auto& userId = impl_->getUserId();
    if (!impl_->current_dataspace_->hasPermission(userId, DSPermissionFlags::eDelete)) {
        return false;
    }

    // delete storage dataspace record
    if (!impl_->delDataspaceRecord()) {
        return false;
    }

    return true;
}

const EntitlementConfig& EntitlementPolicy::config() const {
    return impl_->config_;
}

// --

Entitlement_ptr entitlement_factory(const EntitlementType& authZ_mode, const EntitlementConfig& config) {
    // upcast the right entitlement pointer to entitlement base class
    if (authZ_mode == EntitlementType::Groups) {
        auto tmp = std::make_shared<auth::EntitlementGroups>(config);
        return std::dynamic_pointer_cast<auth::Entitlement>(tmp);
    } else {
        auto tmp = std::make_shared<auth::EntitlementPolicy>(config);
        return std::dynamic_pointer_cast<auth::Entitlement>(tmp);
    }
}

} // namespace oes::common::auth