// ============================================================================
// Copyright 2020-2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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

#include <utility>
#include <cassert>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "BaseProto.h"
#include "Repo.h"

#include <oes/common/auth/AuthN.h>
#include <oes/common/auth/Entitlement.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/osdu/api/EntitlementDataspace.h>
#include <oes/osdu/api/EntitlementClient.h>
#include <oes/core/utils/Logger.h>

namespace oes::eml::openkv {
using namespace oes::common::etp12::Datatypes;
using oes::common::etp12::EmlUri;

/**
 * @brief Extract a DataValue from extradata if exists.
 * 
 * @param xdata metadata
 * @param key key
 * @return std::vector<std::string> 
 */
std::vector<std::string> extractXDataValues(const std::map<std::string, DataValue>& xdata, const std::string& key) {
    const auto it = xdata.find(key);
    if (it == xdata.end()) {
        return {};
    }

    if (std::holds_alternative<ArrayOfString>(it->second.m_item)) {
        try {
            return std::get<ArrayOfString>(it->second.m_item).m_values;
        } catch (std::bad_variant_access const&) {
            return {};
        }
    } else if (std::holds_alternative<std::string>(it->second.m_item)) {
        try {
            auto value = std::get<std::string>(it->second.m_item);
            // check if this string is a JSON stringify array
            // "[\"\tag1\", \"tag2\"]"
            // if so, parse it and return the array of strings
            // else return the string as a single element array
            if (!value.empty() && value.front() == '[' && value.back() == ']') {
                auto json_array = nlohmann::json::parse(value);
                if (json_array.is_array()) {
                    return json_array.get<std::vector<std::string>>();
                }
            }
            return {value};
        } catch (std::bad_variant_access const&) {
            return {};
        }
    }

    OES_WARNING("extractXDataValues: key '{}' exists but has unsupported DataValue type index {}. Expected string (6) or ArrayOfString (15).",
        key, it->second.m_item.index());
    return {};
};

oes::osdu::api::EntitlementDataspace_ptr space_to_record(const oes::eml::openkv::Space& space, oes::common::auth::Entitlement_ptr e, bool getXdata=false) {
    const std::map<std::string,DataValue> xdata = space.xdataAsMap();

    const std::vector<std::string> legalTags = getXdata? extractXDataValues(xdata, "legaltags") : std::vector<std::string>{};
    const std::vector<std::string> legalCountries = getXdata? extractXDataValues(xdata, "otherRelevantDataCountries") : std::vector<std::string>{};

    if (e->isGroupBased()) {
        std::vector<std::string> viewers = getXdata? extractXDataValues(xdata, "viewers") : std::vector<std::string>{};
        std::vector<std::string> owners = getXdata? extractXDataValues(xdata, "owners") : std::vector<std::string>{};
        if(getXdata && !e->isValid()){
            auto e_groups = std::dynamic_pointer_cast<oes::common::auth::EntitlementGroups>(e);
            if (viewers.empty()) {
                viewers.push_back(e_groups->viewersGroupMail());
            }
            if (owners.empty()) {
                owners.push_back(e_groups->ownersGroupMail());
            }
        }
        
        auto tmp = std::make_shared<oes::osdu::api::EntitlementDataspaceGroups>(
            space.uid_, space.path_, legalTags, legalCountries, viewers, owners
        );

        return std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspace>(tmp);

    } // then entitlement_type == "policy"
    oes::osdu::api::PermissionMap permission_map; //TODO fill it?

    auto tmp = std::make_shared<oes::osdu::api::EntitlementDataspacePolicy>(
        space.uid_, space.path_, EmlUri::make_space(space.uid_).to_string(), legalTags, legalCountries, permission_map
    );

    return std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspace>(tmp);
}

using ErrorCodes = etp12::Datatypes::ErrorCodes;

//============================================================================

BaseProto::BaseProto(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : repo_(std::move(repo))
    , session_(session)
{ }

void BaseProto::newTx(bool ro, std::string msg, std::vector<EmlUri> dataspaceUris, bool local)
{
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        return;
    }
    assert(session->tx_ == nullptr);
    session->tx_.reset(new OpenKVTransaction(repo_->getDbConnection(), ro, std::move(msg), std::move(dataspaceUris), local));
}

OpenKVScopedTxApi BaseProto::txApi(ApiType type)
{
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        return OpenKVScopedTxApi(type);
    }
    if (!session->tx_ && session->user_ && session->user_->read_only_)   // Checked at TX start otherwise
    {
        switch (type) {
        case ApiType::eNone:
        case ApiType::eGet:
            break;

        case ApiType::ePut:
        case ApiType::eDel:
            throw etp12::errorStatus(ErrorCodes::EREQUEST_DENIED, "Read-only User");

        case ApiType::eSub:
        case ApiType::eMisc:
            break;
        }
    }
    return OpenKVScopedTxApi(session->tx_, type);
}

void BaseProto::resetTx()
{
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        return;
    }
    session->tx_.reset();
}

/**
 * @brief Check if user has access to the dataspace
 * 
 * @param space The dataspace to check against
 * @param ro Access type (read-only or read-write)
 * @param err Error info if access is denied
 * @return true if access is granted
*/
bool BaseProto::checkDataspaceAccess(const Space& space, AccessType ro, ErrorInfo& err, bool unlock_op) const{
    
    // recheck the session after postgres call
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        return false;
    }

    if(!space.isValid()){
        err.m_code = ErrorCodes::ENOT_FOUND;
        err.m_message = fmt::format("No dataspace found for {}\n", space.uid_);
        return false;
    }

    auto it = session->etp_space_by_path_.find(space.path_);
    if(it==session->etp_space_by_path_.end()){
        session->etp_space_by_uid_.emplace(space.uid_, space.path_);
        session->etp_space_by_path_.emplace(space.path_,space);
        it = session->etp_space_by_path_.find(space.path_);
    }
    const Space& cachedSpace = it->second;

    // Locked modifier should be ignored if in unlock operation.
    const bool isNotLocked = !space.ro_ || unlock_op ;

    if(ro==AccessType::CAN_READ){
        std::optional<bool> canRead = cachedSpace.canRead();
        if(canRead.has_value()){
            const bool val = canRead.value();
            if(!val){
                err.m_code = ErrorCodes::EAUTHORIZATION_REQUIRED;
                err.m_message = fmt::format(
                    "Cache- User '{}' has no permissions to view '{}' ({}) space\n", session->userMail(), space.uid_, space.path_
                );
            }
            return val;
        }
    }else if(ro==AccessType::CAN_WRITE){
        std::optional<bool> canWrite = cachedSpace.canWrite();
        if(canWrite.has_value()){
            // If the operation is not to unlock the dataspace, then space.ro_
            // should take precedence.
            bool val = canWrite.value() && isNotLocked ;
            if(!val){
                err.m_code = ErrorCodes::EAUTHORIZATION_REQUIRED;
                err.m_message = fmt::format(
                    "Cache- User '{}' has no permissions to modify '{}' ({}) space\n", session->userMail(), space.uid_, space.path_
                );
            }
            return val;
        }
    }

    // If session entitlement is not set or not valid, assume all dataspaces are accessible,
    // but still respect the locked state if asking RW access.
    if(!session->entitlement_ || !session->entitlement_->isValid()){
        cachedSpace.setReadAccess(true);
        cachedSpace.setWriteAccess(true);
        if( ro==AccessType::CAN_WRITE && isNotLocked == false ) {
            err.m_code = ErrorCodes::EINVALID_ARGUMENT;
            err.m_message = fmt::format( "Dataspace '{}' is locked.\n", space.uid_);
            return false ;
        }
        return true;
    }

    // Check if user is in viewers group OR read permission, if not no access at all
    if (!session->entitlement_->hasServicePermissionRead()) {
        cachedSpace.setReadAccess(false);
        err.m_code = ErrorCodes::EAUTHORIZATION_REQUIRED;
        err.m_message = fmt::format( "Service- User '{}' has no permissions to view dataspaces\n", session->userMail());
        return false;
    }

    // If not read-only, check if user is in owners group OR update permission, if not no access
    if(ro==AccessType::CAN_WRITE && !session->entitlement_->hasServicePermissionUpdate()) {
        cachedSpace.setWriteAccess(false);
        err.m_code = ErrorCodes::EAUTHORIZATION_REQUIRED;
        err.m_message = fmt::format( "Service- User '{}' has no permissions to modify dataspaces\n", session->userMail());
        return false;
    }

    // Get EntitlementDataspace from dataspace
    const oes::osdu::api::EntitlementDataspace_ptr entitlementDataspace = space_to_record(space, session->entitlement_);
    if(!entitlementDataspace){
        cachedSpace.setReadAccess(false);
        cachedSpace.setWriteAccess(false);
        err.m_code = ErrorCodes::EAUTHORIZATION_REQUIRED;
        err.m_message = fmt::format("No Record- No entitlement found for {} ({})\n", space.uid_, space.path_);
        return false;
    }
    // If read-only, check that the session entitlement allow dataspace view
    if(ro==AccessType::CAN_READ){
        const bool read = session->entitlement_->canReadDataspace(entitlementDataspace);
        cachedSpace.setReadAccess(read);
        if(!read){
            err.m_code = ErrorCodes::EAUTHORIZATION_REQUIRED;
            err.m_message = fmt::format(
                "Direct- User '{}' has no permissions to view '{}' ({}) space\n", session->userMail(), space.uid_, space.path_
            );
            return false;
        }
    }// If read-write, check that the session entitlement allow dataspace edit
    else{
        bool write = session->entitlement_->canUpdateDataspace(entitlementDataspace);
        cachedSpace.setWriteAccess(write);
        write = write && isNotLocked ;
        if( !write ){
            err.m_code = ErrorCodes::EAUTHORIZATION_REQUIRED;
            err.m_message = fmt::format(
                "Direct- User '{}' has no permissions to modify '{}' ({}) space\n", session->userMail(), space.uid_, space.path_
            );
            return false;
        }
    }
    return true;
}

/**
 * @brief Check if user has access to the dataspace
 * 
 * @param space The dataspace to check against
 * @param ro Access type (read-only or read-write)
 * @param err Error info if access is denied
 * @return true if access is granted
*/
std::vector<bool> BaseProto::checkDataspacesAccess(const std::vector<Space>& spaces, AccessType ro, ErrorInfo& err, bool unlock_op) const {
    std::vector<bool> res;
    
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        return res;
    }

    std::vector<std::string> uris_to_check;
    std::vector<size_t> index_map;

    const bool entl_valid = session->entitlement_ && session->entitlement_->isValid();
    const bool service_read = entl_valid && session->entitlement_->hasServicePermissionRead();
    const bool service_update = entl_valid && ro==AccessType::CAN_WRITE && session->entitlement_->hasServicePermissionUpdate();

    for(size_t i{0}; i < spaces.size(); i++) {
        auto& space = spaces.at(i);
        if(!space.isValid()){
            res.push_back(false);
            continue;
        }

        auto it = session->etp_space_by_path_.find(space.path_);
        if(it==session->etp_space_by_path_.end()){
            session->etp_space_by_uid_.emplace(space.uid_, space.path_);
            session->etp_space_by_path_.emplace(space.path_,space);
            it = session->etp_space_by_path_.find(space.path_);
        }
        const Space& cachedSpace = it->second;

        // Locked modifier should be ignored if in unlock operation.
        const bool isNotLocked = !space.ro_ || unlock_op ;

        if(ro==AccessType::CAN_READ){
            std::optional<bool> canRead = cachedSpace.canRead();
            if(canRead.has_value()){
                const bool val = canRead.value();
                res.push_back(val);
                continue;
            }
        } else if (ro==AccessType::CAN_WRITE){
            std::optional<bool> canWrite = cachedSpace.canWrite();
            if(canWrite.has_value()){
                // If the operation is not to unlock the dataspace, then space.ro_
                // should take precedence.
                bool val = canWrite.value() && isNotLocked ;
                res.push_back(val);
                continue;
            }
        }

        // If session entitlement is not set or not valid, assume all dataspaces are accessible
        if(!session->entitlement_ || !session->entitlement_->isValid()){
            cachedSpace.setReadAccess(true);
            cachedSpace.setWriteAccess(true);
            res.push_back(true);
            continue;
        }

        // Check if user is in viewers group OR read permission, if not no access at all
        if (!service_read) {
            cachedSpace.setReadAccess(false);
            res.push_back(false);
            continue;
        }

        // If not read-only, check if user is in owners group OR update permission, if not no access
        if(ro==AccessType::CAN_WRITE && !service_update) {
            cachedSpace.setWriteAccess(false);
            res.push_back(false);
            continue;
        }

        res.push_back(false);
        auto uri = EmlUri::make_space(space.uid_).to_string();
        uris_to_check.push_back(uri);
        index_map.push_back(i);
        
    }

    if (session->entitlement_ && session->entitlement_->isGroupBased()) {
        if (index_map.empty()) {
            return res;
        }
        auto entl_groups = std::dynamic_pointer_cast<oes::common::auth::EntitlementGroups>(session->entitlement_);
        if (!entl_groups) {
            err.m_code = ErrorCodes::ENOTSUPPORTED;
            err.m_message = "Entitlement logic error.";
            return {};
        }

        std::vector<oes::osdu::api::EntitlementDataspaceGroups_ptr> groups_ds;
        groups_ds.reserve(index_map.size());
        for (const size_t index : index_map) {
            groups_ds.push_back(std::dynamic_pointer_cast<oes::osdu::api::EntitlementDataspaceGroups>(
                space_to_record(spaces.at(index), session->entitlement_)));
        }
        auto groups_bulk_ds = std::make_shared<oes::osdu::api::EntitlementDataspaceGroupsBulk>(std::move(groups_ds));

        std::vector<bool> can_read;
        std::vector<bool> can_update;
        const bool bulk_ok = entl_groups->canReadUpdateDataspaceBulk(groups_bulk_ds, can_read, can_update);
        for(size_t i {0}; i < index_map.size(); i++) {
            const auto& index = index_map.at(i);
            const auto& space = spaces.at(index);

            if (!bulk_ok) {
                // Listing records may not be supported by this storage service
                ErrorInfo space_err;
                res[index] = checkDataspaceAccess(space, ro, space_err, unlock_op);
                continue;
            }

            auto it = session->etp_space_by_path_.find(space.path_);
            if(it==session->etp_space_by_path_.end()){
                session->etp_space_by_uid_.emplace(space.uid_, space.path_);
                session->etp_space_by_path_.emplace(space.path_,space);
                it = session->etp_space_by_path_.find(space.path_);
            }
            const Space& cachedSpace = it->second;
            cachedSpace.setReadAccess(can_read.at(i));
            cachedSpace.setWriteAccess(can_update.at(i));

            const bool isNotLocked = !space.ro_ || unlock_op ;
            res[index] = ro == AccessType::CAN_READ ? can_read.at(i) : can_update.at(i) && isNotLocked;
        }
        return res;
    }

    auto bulk_ds = std::make_shared<oes::osdu::api::EntitlementDataspacePolicyBulk>(
        uris_to_check,
        std::vector<std::string>(),
        std::vector<std::string>()
    );

    auto entl_ptr = std::dynamic_pointer_cast<oes::common::auth::EntitlementPolicy>(session->entitlement_);
    if (!entl_ptr) {
        err.m_code = ErrorCodes::ENOTSUPPORTED;
            err.m_message = "Entitlement logic error.";
        return {};
    }

    std::vector<bool> res_vec;
    if (ro == AccessType::CAN_READ) {
        res_vec = entl_ptr->canReadDataspaceBulk(bulk_ds);
    } else {
        res_vec = entl_ptr->canUpdateDataspaceBulk(bulk_ds);
    }

    if (res_vec.size() != uris_to_check.size()) {
        err.m_code = ErrorCodes::EREQUEST_DENIED;
        err.m_message = "Number of requested authorizations is different from those received. Authorization failed.";
        return {};
    }

    for(size_t i {0}; i < uris_to_check.size(); i++) {
        const auto& index = index_map.at(i);
        const auto& space = spaces.at(index);

        // Get the cache to update.
        auto it = session->etp_space_by_path_.find(space.path_);
        if(it==session->etp_space_by_path_.end()){
            session->etp_space_by_uid_.emplace(space.uid_, space.path_);
            session->etp_space_by_path_.emplace(space.path_,space);
            it = session->etp_space_by_path_.find(space.path_);
        }
        const Space& cachedSpace = it->second;

        // Locked modifier should be ignored if in unlock operation.
        const bool isNotLocked = !space.ro_ || unlock_op ;

        if( ro == AccessType::CAN_READ) {
            cachedSpace.setReadAccess(res_vec.at(i)) ;
            res[index] = res_vec.at(i);
        } else {
            cachedSpace.setWriteAccess(res_vec.at(i)) ;
            res[index] = res_vec.at(i) && isNotLocked;
        }
    }

    return res;
}

//============================================================================

/**
 * @brief Register a dataspace
 * Add the dataspace to the entitlement database
 * @param space The dataspace to register
 * @return true if the dataspace was registered
*/
bool BaseProto::registerDataspace(Space& space){
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        return false;
    }

    oes::osdu::api::EntitlementDataspace_ptr entl_ds = space_to_record(space, session->entitlement_, true);
    if (session->entitlement_->config().osdu_->authorizationMode() == "simple" &&
        !session->entitlement_->canCreateDataspace(entl_ds)) {
        OES_ERROR("registerDataspace: canCreateDataspace denied for dataspace '{}' in simple auth mode", space.path_);
        return false;
    }

    bool ok = session->entitlement_->registerDataspace(entl_ds);
    if(!ok){
        const auto& entl_err = session->entitlement_->err();
        OES_ERROR("registerDataspace: entitlement registerDataspace failed for '{}': {}",
            space.path_, entl_err.status_);
    }
    if(ok){
        // The storage record is now the only source for these values
        if (session->entitlement_->config().osdu_->authorizationMode() != "simple") {
            std::vector<std::string> keys {"legaltags", "otherRelevantDataCountries"};
            if (session->entitlement_->isGroupBased()) {
                keys.insert(keys.end(), {"viewers", "owners"});
            }
            auto xdata = space.xdataAsMap();
            for (const auto& key : keys) {
                xdata.erase(key);
            }
            space.setXDdataFromMap(xdata);
        }
        space.setReadAccess(true);
        space.setWriteAccess(true);
        session->etp_space_by_uid_.emplace(space.uid_, space.path_);
        session->etp_space_by_path_.emplace(space.path_,space);
    }
    return ok;
}

/**
 * @brief Unregister a dataspace
 * Remove the dataspace from the entitlement database
 * @param space The dataspace to unregister
 * @return true if the dataspace was unregistered
*/
bool BaseProto::unregisterDataspace(const Space& space){
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        return false;
    }

    oes::osdu::api::EntitlementDataspace_ptr entl_ds = space_to_record(space, session->entitlement_);
    // For groups, this also loads the ACLs from the storage record (no longer kept in xdata)
    if ((session->entitlement_->config().osdu_->authorizationMode() == "simple" || session->entitlement_->isGroupBased()) &&
        !session->entitlement_->canDeleteDataspace(entl_ds)) {
        return false;
    }

    return session->entitlement_->unregisterDataspace(entl_ds);
}

} // namespace oes::eml::openkv
