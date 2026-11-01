// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <algorithm>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "SpaceProto.h"

#include "Repo.h"
#include "Session.h"
#include "AdminDB.h"
#include "SpaceDDL.h"
#include "SpaceDML.h"
#include "AuditLog.h"

#include "../etp12/Server.hpp"
#include "../etp12/MultiPartReply.hpp"
#include "../etp12/MsgUtils.h"

#include <oes/common/auth/AuthN.h>
#include <oes/common/auth/Entitlement.h>

#include <oes/osdu/api/EntitlementClient.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThreadUtils.hpp>

#include <oes/postgresql/Transaction.h>
#include <oes/postgresql/Format.hpp>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsHelper.h>
#include <oes/core/utils/MetricsNames.h>
#include <chrono>


using namespace oes::eml::etp12::Datatypes;

using oes::eml::etp12::errorStatus;
using oes::eml::etp12::addErrorStatus;
using oes::core::thisThreadId;

//============================================================================


namespace oes::eml::openkv {

//============================================================================

OpenKVDataspace::OpenKVDataspace(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

void OpenKVDataspace::GetDataspaces(
    IRequest& ireq, GetDataspaces_req_t req,
    EtpResponseHandler<GetDataspaces_res_t> process_cb
) try {
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_ERROR);
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    // Optional filtering to return only spaces
    // created/modified *after* a given timestamp
    const int64_t last_write_filter = req.m_storeLastWriteFilter.value_or(0);

    // Authorization (1 from 2)
    if (session->entitlement_ && session->entitlement_->isValid()) {
        if (!session->entitlement_->hasServicePermissionRead()) {
            OES_ERROR(
                "User {} has no permissions to get dataspaces",
                session->userMail()
            );
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_ERROR);
            return process_cb(errorStatus(EREQUEST_DENIED, "Not authorized to GetDataspaces"), {});
        }
    } else if (session->entitlement_) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_ERROR);
        return process_cb(errorStatus(EAUTHORIZATION_REQUIRED, "Invalid entitlement."), {});
    }

    std::vector<Space> spaces = repo_->getAdminDb()->getSpaces(last_write_filter);
    // recheck the session after postgres call
    session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_ERROR);
        return;
    }

    std::shared_ptr<std::vector<bool>> read_authorizations (nullptr);
    std::shared_ptr<std::vector<bool>> write_authorizations (nullptr);
    const auto auth_simple_mode = (session->entitlement_ && session->entitlement_->config().osdu_->authorizationMode() == "simple");
    const bool auth_groups_bulk = (session->entitlement_ && session->entitlement_->isGroupBased());
    
    if (auth_simple_mode || auth_groups_bulk) {
        ErrorInfo err;
        read_authorizations = std::make_shared<std::vector<bool>>(std::move(checkDataspacesAccess(spaces, AccessType::CAN_READ, err)));
        if(err.m_code != ErrorCodes::IS_OK) {
            throw std::logic_error(err.m_message);
        }
        write_authorizations = std::make_shared<std::vector<bool>>(std::move(checkDataspacesAccess(spaces, AccessType::CAN_WRITE, err)));
        if(err.m_code != ErrorCodes::IS_OK) {
            throw std::logic_error(err.m_message);
        }
    }

    //for (Space& space : spaces) {
    for (size_t i {0}; i < spaces.size(); i++) {
        Space& space = spaces.at(i);
        // Authorization (2 from 2)
        
        // Dataspace filtered-out since user has no access
        if ((auth_simple_mode || auth_groups_bulk) && read_authorizations) {
            // We check in the authorization vector (we have already checked the read auth for every dataspace)
            if (!read_authorizations->at(i)) {
                continue;
            }
        } else {
            // Check dataspace by dataspace
            ErrorInfo err;
            if(!checkDataspaceAccess(space, AccessType::CAN_READ, err)){
                continue;
            }
        }

        const std::string space_uri = EmlUri::make_space(space.uid_).to_string();
        const oes::eml::etp12::XDataMap data_map = space.xdataAsMap();

        size_t size = space_uri.size()+8; //string size + 8 bytes for length
        size += space.path_.size()+8; //string size + 8 bytes for length
        size += 8; // space.created_
        size += 8; // last_write
        for(const auto& [key, val] : data_map) {
            size += key.size()+8; //string size + 8 bytes for length
            size += 8; // type
            size += 8; // length
            if (std::holds_alternative<std::string>(val.m_item)) {
                size += std::get<std::string>(val.m_item).size()+8; //string size + 8 bytes for length
            }else if (std::holds_alternative<std::vector<uint8_t>>(val.m_item)) {
                size += std::get<std::vector<uint8_t>>(val.m_item).size()+8; //string size + 8 bytes for length
            }else {
                size += 8; // other types
            }
        }
        size += 26; // "read-only" metadata;
        size += 21; // "size" metadata;
        reply.maybeProcess(size);

        auto& etp_space = reply.addVecPart();
        etp_space.m_uri = std::move(space_uri);
        etp_space.m_path = std::move(space.path_);

        etp_space.m_storeCreated = space.created_;
        etp_space.m_storeLastWrite = space.last_write_;
        etp_space.m_customData = std::move(data_map);

        // Set it if actually read-only.
        // Makes xdata roundtrip testing easier.
        bool can_write = false ;
        if ((auth_simple_mode || auth_groups_bulk) && write_authorizations) {
            can_write = write_authorizations->at(i) ;
        } else {
            // Check for write authorization
            ErrorInfo err;
            can_write = checkDataspaceAccess(space, AccessType::CAN_WRITE, err);
        }
        etp_space.m_customData["read-only"].m_item = space.ro_ || !can_write;
        etp_space.m_customData["locked"].m_item = space.ro_;

        // Add the space size to the extra metadata
        etp_space.m_customData["size"].m_item = space.size_str_;

        /*
        ** Direct from field
        t.setTitle(res.m_path);
        t.setModified(std::chrono::system_clock::from_time_t(res.m_lastChanged.get_long()));

        ** Indirect from xdata:
        ** All etp_space.m_customData checked by DX ETP client
        ** and what is done with them
        **
        "created"       t.setCreated(ptimeFromIsoExtendedString(entry.second));
        "creator"       t.setCreator(entry.second);
        "creatorGroup"  t.setCreatorGroup(entry.second);
        "contributor"   t.setContributor(entry.second);
        "description"   t.setDescription(entry.second);
        "extraMetaData" t.swapBulkExtraMetaData(key_values);
        "uuid"          OES_ASSERT(t.uid().toXmlString() == entry.second, ;);
        "dxType"        OES_ASSERT(t.interfaceId()->name() == entry.second, ;);
        "dataSource"    if (auto p = t.parent()) {
                        if (p->interfaceId() == IDataSource::interfaceType()) {
                        OES_ASSERT(p->uid().toXmlString() == entry.second, ;);
        */

        // rough estimate of how big the data is for this "part"
        reply.addBytes( size );
    }

    // Add 15s pause
    // std::this_thread::sleep_for(std::chrono::seconds(15));

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_SUCCESS,oes::core::metrics::DATASPACE_LATENCY_GET,start);
    reply.finalProcess();
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_ERROR);
    etp12::handleException(process_cb);
}

void OpenKVDataspace::PutDataspaces(
    IRequest& ireq, PutDataspaces_req_t req,
    EtpResponseHandler<PutDataspaces_res_t> process_cb
) try {
    OpenKVSession* session = session_.lock().get();
    auto start = std::chrono::steady_clock::now();
    if(session == nullptr) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    if (req.m_dataspaces.empty()) {
        // Audit log: Empty request
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_SPACES,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_CREATE,
            "",
            "Empty request - no dataspaces provided"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
        return process_cb(errorStatus(EINVALID_ARGUMENT, "Nothing to add"), {});
    }

    // Authorization (1 from 3)
    if (session->entitlement_ && session->entitlement_->isValid()) {
        if (!session->entitlement_->hasServicePermissionCreate()) {
            OES_ERROR(
                "User {} has no permissions to put dataspaces", 
                session->userMail()
            );
            // Audit log: Authorization failure
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_SPACES,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                "",
                "Not authorized to create dataspaces"
            );
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
            return process_cb(errorStatus(EREQUEST_DENIED, "Not authorized to PutDataspaces"), {});
        }
    } else if (session->entitlement_) {
        // Audit log: Invalid entitlement
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_SPACES,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_CREATE,
            "",
            "Invalid entitlement"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
        return process_cb(errorStatus(EAUTHORIZATION_REQUIRED, "Invalid entitlement."), {});
    }

    // TODO: Put is both INSERT or UPDATE. Look into UPSERT.
    //       Code below deals with INSERT only for now.
    //       Conflicts in the case of UPDATE are more muddy,
    //       since both the UID and the PATH must be unique in OpenKV's impl.
    oes::common::etp12::Protocol::EtpResponseStatus rc;

    // phase 1, look for conflicts
    // TODO: Make this transactional, notably REMOVE transaction inside addSpaces()
    std::vector<std::string> keys;
    std::vector<Space> spaces_to_make;
    for (auto& entry : req.m_dataspaces) {
        const std::string& cid = entry.first; // client ID
        Object::Dataspace& etp_space = entry.second;

        // Default the URI from the Path, if empty (used by tests)
        if (etp_space.m_uri.empty() && !etp_space.m_path.empty()) {
            etp_space.m_uri = EmlUri::make_space(etp_space.m_path).to_string();
        }

        Space space;

        EmlUri uri = EmlUri::parse(etp_space.m_uri);
        if (uri.isRoot()) {
            addErrorStatus(rc, cid, EINVALID_ARGUMENT, "Cannot add default space");
            continue;
        }

        // ETP transaction: check that space is in the list of allowed
        if (session->tx_ && !repo_->checkIsSpaceInvolvedInTheTransaction(uri))
        {
            addErrorStatus(rc, cid, EREQUEST_DENIED, "Writing to dataspaces not specified in `StartTransaction` is not allowed");
            continue;
        }

        /*
        ** A test expects empty-Path to fail inside Space::validatePath()
        // Default the Path from the URI's space, if empty
        if (etp_space.m_path.empty()) {
            etp_space.m_path = uri.dataspace_;
        }
        */

        const std::string& uid = uri.dataspace_; // NOT a guid, just a unique arbitrary *string*
        const std::string& path = etp_space.m_path;
        const bool uid_and_path_match = (uid == path);

        // The "normal" ETP rules, is that they should match.
        // But OpenKV allows using a Guid as the uid too, as an extension.
        if (!uid_and_path_match) {
            Guid guid;
            const bool is_guid = guid.reparse(uid.c_str(), uid.size(), /*lax*/true);
            const bool is_nil_guid = is_guid && !guid.isValid();
            if (is_nil_guid) {
                addErrorStatus(rc, cid, EINVALID_ARGUMENT, "Space URI uses all-zero UUID");
                continue;
            }
            if (!is_guid) {
                addErrorStatus(rc, cid, EINVALID_ARGUMENT, "Space URI neither matches PATH, nor uses UUID");
                continue;
            }
        }

        std::string folder, name;
        std::string out_path, db_path, err_msg;
        const bool path_ok = Space::validatePath(
            etp_space.m_path, out_path, db_path, folder, name, err_msg
        );
        if (!path_ok) {
            addErrorStatus(
                rc, cid, EINVALID_ARGUMENT, fmt::format(
                    "Invalid space path: '{}': {}",
                    etp_space.m_path, err_msg
                )
            );
            continue;
        }
        assert(boost::starts_with(path, folder));

        space = repo_->getAdminDb()->findSpaceByUid(uid);
        if (space.isValid()) {
            addErrorStatus(rc, cid, EINVALID_ARGUMENT, fmt::format("Space already exists: URI conflict with {}", uid));
            continue;
        }

        space = repo_->getAdminDb()->findSpaceByPath(out_path);
        if (space.isValid()) {
            addErrorStatus(rc, cid, EINVALID_ARGUMENT, fmt::format("Space already exists: PATH conflict with {}", out_path));
            continue;
        }

        // recheck the session after postgres call
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
            return;
        }

        space.uid_ = uid;
        space.path_ = out_path;
        space.dbfile_ = db_path;
        space.setXDdataFromMap(etp_space.m_customData);

        spaces_to_make.emplace_back(std::move(space));
        keys.emplace_back(cid);
    }
    assert(spaces_to_make.size() == keys.size());

    // Lambda to log all failures from rc.ex.m_errors
    auto logFailures = [&]() {
        for (const auto& error_entry : rc.ex.m_errors) {
            const std::string& cid = error_entry.first;
            const ErrorInfo& error = error_entry.second;
            
            // Find URI for this cid from original request
            auto it = req.m_dataspaces.find(cid);
            if (it != req.m_dataspaces.end()) {
                std::string uri = it->second.m_uri;
                if (uri.empty() && !it->second.m_path.empty()) {
                    uri = EmlUri::make_space(it->second.m_path).to_string();
                }
                
                AuditLog::log(
                    *repo_->getServerConfig(),
                    session,
                    AuditLogConstants::OP_PUT_DATA_SPACES,
                    AuditLogConstants::RESULT_FAILURE,
                    AuditLogConstants::ACTION_CREATE,
                    uri,
                    error.m_message
                );
            }
        }
    };

    if (spaces_to_make.empty()) {
        rc.ok = false;
        logFailures(); // Audit logging: Phase 1 validation - all dataspaces rejected
        return process_cb(rc, {});
    }

    int magic_cookie = 0;
    const auto& schema_history = space_schema_versions(magic_cookie);

    // phase 2, physically create the DBs on disk
    PutDataspaces_res_t resp;
    std::vector<Space> spaces_to_add;
    std::vector<std::pair<std::string, std::string> > post_processings;


    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string& cid = keys[i];
        Space& space = spaces_to_make[i];

        // Authorization (2 from 3)
        // - create groups for dataspace using entitlement service
        // - add record for dataspace with ACL using storage service
        // - add ACL to dataspace extra metadata
        if (session->entitlement_ && session->entitlement_->isValid()) {
            // the creation of the dataset records should block the creation of the dataspace for osdu
            // else dataspace will be visible to everyone
            if (!registerDataspace(space)) {
                const auto& entl_err = session->entitlement_->err();
                // Prefer the specific reason recorded by the entitlement layer.
                // Fall back to a generic hint only if none was provided.
                const std::string reason = entl_err.status_.empty()
                    ? std::string("authorization or ACL validation failed. Ensure customData contains valid 'viewers', 'owners', 'legaltags', and 'otherRelevantDataCountries' fields")
                    : entl_err.status_;
                addErrorStatus(rc, cid, EREQUEST_DENIED, fmt::format("Could not register dataspace {}: {}", space.uri().to_string(), reason));
                OES_ERROR(
                    "[{}] SVR Thread({}) Session({}) Dataspace(24).PutDataspaces: Failed to register dataspace {} ({}) in entitlement service: {}",
                    session->client_identification_, thisThreadId(), session->id(),
                    space.uid_, space.path_, reason);
                continue;
            }
        }

        ErrorInfo err;
        oes::postgresql::relational::PhysicalName schema_name(space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);

        etp12::XDataMap jsonMap = etp12::xdataFromJson(space.xdata_json_);
        const etp12::XDataMap::iterator it = jsonMap.find("fromDataspace");
        bool copied = false;
        if(it != jsonMap.end() && it->second.m_item.index() == size_t(etp12::Datatypes::DataValue_item_e::eString)){
            oes::common::etp12::EmlUri origin_uri = EmlUri::parse(std::get<std::string>(it->second.m_item));
            if(origin_uri.isSpace()){
                Space origin = repo_->findSpaceInfo(origin_uri.dataspace_);
                if(!origin.isValid()){
                    addErrorStatus(rc, cid, ENOT_FOUND, fmt::format("Dataspace not found: {}", origin_uri.dataspace_));
                    continue;
                }
                if( !checkDataspaceAccess(origin, AccessType::CAN_READ, err) ){
                    addErrorStatus(rc, cid, err);
                    continue;

                }
                // The clone is labelled with the latest version below, so its source must be at it
                if (!repo_->getSpaceConnection(origin, err)) {
                    addErrorStatus(rc, cid, err);
                    continue;
                }
                
                copied = true;
                const std::string origin_id = EmlUri::make_space(origin.uid_).to_string(); // FIXME
                oes::postgresql::relational::PhysicalName origin_schema_name(origin.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
                exec_ddl(*repo_->getDbConnection(),
                    "SELECT admin.clone_schema({}, {});",
                    origin_schema_name.singleQuotedName(),
                    schema_name.singleQuotedName()
                );

                post_processings.emplace_back(std::pair(
                    std::string("\'") + origin.uid_ + std::string("\'"),
                    std::string("\'") + space.uid_ + std::string("\'")
                ));
            }
        }
        
        // TODO Protect against sql injection
        if(!copied){
            exec_ddl(*repo_->getDbConnection(),
                "DROP SCHEMA IF EXISTS {} CASCADE",
                schema_name.sqlName()
            );
            exec_ddl(*repo_->getDbConnection(),
                "CREATE SCHEMA IF NOT EXISTS {}",
                schema_name.quotedName()
            );
        }
        exec_ddl(*repo_->getDbConnection(),
            "COMMENT ON SCHEMA {} is '{}:{}'",
            schema_name.quotedName(),
            magic_cookie,
            schema_history.back().version
        );

        Connection_ptr space_conn = repo_->switchCurrentSpace(space, err);

        // recheck the session after postgres call
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
            return;
        }

        if (!space_conn) {
            addErrorStatus(rc, cid, err);            
            // Authorization (3 from 3)
            // try to un-register dataspace in storage
            if (session->entitlement_ && session->entitlement_->isValid()) {
                if (!unregisterDataspace(space)) {
                    const auto& entl_err = session->entitlement_->err();
                    err.m_code = entl_err.code_;
                    err.m_message = entl_err.status_;
                    addErrorStatus(rc, cid, err);
                    OES_ERROR(
                    "[{}] SVR Thread({}) Session({}) Dataspace(24).PutDataspaces: Failed to unregister dataspace {} in entitlement service after trying to put Dataspace",
                    session->client_identification_ ,thisThreadId(), session->id(), 
                    space.uid_);
                }
            }            
        } else{
             if(!copied){
                const oes::postgresql::relational::PhysicalName phys(space.dbfile_,oes::postgresql::relational::IMPLICIT_LOWERCASE);
                std::shared_ptr<oes::postgresql::ScopedSetCurrentSchema> scope(new oes::postgresql::ScopedSetCurrentSchema(*space_conn,phys));
                new_empty_eml_db(space_conn);
            }
            resp.m_success.emplace(cid, std::string()); // What to put as value?
            spaces_to_add.emplace_back(std::move(space));
        }
    }

    if (spaces_to_add.empty()) {
        rc.ok = false; // Total failure.
    } else if (!repo_->getAdminDb()->addSpaces(spaces_to_add)) {
        rc.ok = false; // TODO: What about partial success? (or partial failure)
    } else {
        rc.ok = true;
    }

    if( rc.ok && !post_processings.empty()) {
        // Reset relations properly, if any.
        // recheck the session again
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
            return;
        }

        // Fix the import refs. No need to check again if access to the space is granted, this has already
        // been done earlier.
        for( auto post_processing: post_processings){
            rc.ok = rc.ok && exec_ddl(*repo_->getDbConnection(),
                "SELECT admin.update_space_rels({}, {});",
                post_processing.first,
                post_processing.second
            );
        }
    }
    
    // 1. Log all failures in audit logs from rc.ex.m_errors phase2
    logFailures();
    
    // Audit Log successes (only if rc.ok is true)
    if (rc.ok) {
        for (const auto& space : spaces_to_add) {
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_SPACES,
                AuditLogConstants::RESULT_SUCCESS,
                AuditLogConstants::ACTION_CREATE,
                space.uri().to_string(),
                "Dataspace successfully created"
            );
        }
    }
    
    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_SUCCESS,oes::core::metrics::DATASPACE_LATENCY_PUT,start);

    process_cb(rc, resp);
} catch (...) {
    // Audit log: Exception handling - log for all attempted dataspaces
    OpenKVSession* session = session_.lock().get();
    if (session != nullptr) {
        for (const auto& entry : req.m_dataspaces) {
            std::string uri = entry.second.m_uri;
            if (uri.empty() && !entry.second.m_path.empty()) {
                uri = EmlUri::make_space(entry.second.m_path).to_string();
            }
            
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_SPACES,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                uri,
                "Exception occurred during dataspace creation"
            );
        }
    }
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR);
    etp12::handleException(process_cb);
}

void OpenKVDataspace::DeleteDataspaces(
    IRequest& ireq, DeleteDataspaces_req_t req,
    EtpResponseHandler<DeleteDataspaces_res_t> process_cb // messageTypeId 5
) try {
    OpenKVSession* session = session_.lock().get();
    auto start = std::chrono::steady_clock::now();
    if(session == nullptr) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.",thisThreadId());
        return;
    }

    etp12::MultiPartReply reply(*session, process_cb);
    if (!session->tx_) {
        for (const auto& entry : req.m_uris) {
            if(!OpenKVTransaction::dataspaceWriteable(entry.second)){
                OES_WARNING("[{}] SVR Thread({}) Session({}) Dataspace(24).DeleteDataspaces(4): Attempt to delete dataspace {} while it is being written to", session->client_identification_ ,thisThreadId(), session->id(), entry.second);
                
                // Audit log: Transaction conflict
                AuditLog::log(
                    *repo_->getServerConfig(),
                    session,
                    AuditLogConstants::OP_DELETE_DATA_SPACES,
                    AuditLogConstants::RESULT_FAILURE,
                    AuditLogConstants::ACTION_DELETE,
                    entry.second,
                    fmt::format("Cannot delete dataspace {} while it is being written to", entry.second)
                );
                oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
                return reply.fatalError(EMAX_TRANSACTIONS_EXCEEDED, fmt::format("Cannot delete dataspace {} while it is being written to", entry.second));
            }
        }
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));


    if (req.m_uris.empty()) {
        // Audit log: Empty request
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_DELETE_DATA_SPACES,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_DELETE,
            "",
            "Empty request - no dataspaces provided"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to delete");
    }

    // Authorization (1 from 2)
    if (session->entitlement_ && session->entitlement_->isValid()) {
        if (!session->entitlement_->hasServicePermissionDelete()) {
            OES_ERROR(
                "[{}] SVR Thread({}) Session({}) Dataspace(24).DeleteDataspaces(4): User {} has no permissions to delete dataspaces",
                session->client_identification_ ,thisThreadId(), session->id(), session->userMail()
            );
            // Audit log: Authorization failure
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_DELETE_DATA_SPACES,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_DELETE,
                "",
                "Not authorized to delete dataspaces"
            );
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
            return reply.fatalError(EAUTHORIZATION_REQUIRED, fmt::format("User {} has no permissions to delete dataspaces", session->user_->name_));
        }
    } else if (session->entitlement_) {
        // Audit log: Invalid entitlement
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_DELETE_DATA_SPACES,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_DELETE,
            "",
            "Invalid entitlement"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
        return reply.fatalError(EAUTHORIZATION_REQUIRED, "Invalid entitlement.");
    }

    // phase 1, look for not-founds
    // TODO: Make this transactional, notably REMOVE transaction inside dropSpaces()
    std::vector<std::string> keys;
    std::vector<std::string> uids_to_drop;
    std::vector<Space> spaces_to_drop;

    // Lambda to log all failures from reply.rc_.ex.m_errors
    auto logFailures = [&]() {
        for (const auto& error_entry : reply.rc_.ex.m_errors) {
            const std::string& cid = error_entry.first;
            const ErrorInfo& error = error_entry.second;
            
            // Find URI for this cid from original request
            auto it = req.m_uris.find(cid);
            if (it != req.m_uris.end()) {
                const std::string& uri = it->second;
                
                AuditLog::log(
                    *repo_->getServerConfig(),
                    session,
                    AuditLogConstants::OP_DELETE_DATA_SPACES,
                    AuditLogConstants::RESULT_FAILURE,
                    AuditLogConstants::ACTION_DELETE,
                    uri,
                    error.m_message
                );
            }
        }
    };

    for (const auto& entry : req.m_uris) {
        const std::string& cid = entry.first; // client ID
        const std::string& uri_txt = entry.second;
        EmlUri uri = EmlUri::parse(uri_txt);
        if (uri.isRoot()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Cannot delete default space");
            continue;
        }

        // ETP transaction: check that space is in the list of allowed
        if (session->tx_ && !repo_->checkIsSpaceInvolvedInTheTransaction(uri))
        {
            reply.addError(cid, EREQUEST_DENIED, "Writing to dataspaces not specified in `StartTransaction` is not allowed");
            continue;
        }

        const Space space = repo_->findSpaceInfo(uri.dataspace_);

        // recheck the session after postgres call
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Thread({}) Session has been forcefully terminated.",thisThreadId());
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
            return;
        }

        if (!space) {
            reply.addError(cid, ENOT_FOUND, "Space " + uri.dataspace_);
            continue;
        }

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            OES_ERROR(
                "[{}] SVR Thread({}) Session({}) Dataspace(24).DeleteDataspaces(4): User {} has no permissions to delete dataspace {} {}",
                session->client_identification_ ,thisThreadId(), session->id(), 
                session->userMail(), space.path_, space.uid_);
            reply.addError(cid, err);
            continue;
        }

        // Authorization (2 from 2)
        // - delete groups for dataspace using entitlement service
        // - remove record for dataspace with ACL using storage service
        if (session->entitlement_ && session->entitlement_->isValid()) {
            if(!unregisterDataspace(space)) {
                const auto& entl_error = session->entitlement_->err();
                err.m_code = entl_error.code_;
                err.m_message = entl_error.status_;
                reply.addError(cid, err);
                OES_ERROR(
                    "[{}] SVR Thread({}) Session({}) Dataspace(24).DeleteDataspaces(4): Failed to unregister dataspace {} in entitlement service",
                    session->client_identification_ ,thisThreadId(), session->id(), 
                    space.uid_);
                continue;
            }
        }

        OES_DEBUG(
            "[{}] SVR Thread({}) Session({}) Dataspace(24).DeleteDataspaces(4): Deleting Dataspace {}({}) on behalf of User {}.",
            session->client_identification_ ,thisThreadId(), session->id(),
            space.uid_, space.path_, session->userMail()
        );

        keys.emplace_back(cid);
        uids_to_drop.emplace_back(uri.dataspace_);
        spaces_to_drop.emplace_back(std::move(space));
    }

    if (uids_to_drop.empty()) {
        logFailures(); // Audit logging: Phase 1 validation - all dataspaces rejected
        reply.fatalError();
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
        return;
    }

    // phase 2, drop them from master, so that later sessions/messages
    // can no longer lookup those spaces. BUT, there could be existing
    // sessions/transactions using those spaces still ongoing. So should
    // do "delete from res/obj/ary/etc..."
    // reply.rc_.ok = repo_->getAdminDb()->dropSpaces(uids_to_drop);

    // recheck the session after postgres call
    session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Thread({}) Session has been forcefully terminated.",thisThreadId());
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
        return;
    }

    // phase 3, get rid of DB spaces.
    for (Space& space : spaces_to_drop) {
        const auto cid_idx = std::distance(&spaces_to_drop[0],&space);
        if(cid_idx<0){
            OES_ERROR("[{}] SVR Thread({}) Session({}) Dataspace(24).DeleteDataspaces(4): Invalid space index {}", 
            session->client_identification_ ,thisThreadId(), session->id(), cid_idx);
            continue;
        }
        const std::string& cid = keys[cid_idx];

        ErrorInfo err;
        oes::postgresql::relational::PhysicalName schema_name(space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
        auto conn = repo_->getDbConnection();
        conn->set_current_schema(schema_name);

        std::vector<std::string> dsp;
        dsp.emplace_back(space.uid_);
        bool ok = exec_ddl(*conn, "DROP SCHEMA {} CASCADE", schema_name.sqlName()) &&
            repo_->getAdminDb()->dropSpaces(dsp);
        if(ok){
            reply.addMapPart(cid);
        }else{
            OES_ERROR(
                "[{}] SVR Thread({}) Session({}) Dataspace(24).DeleteDataspaces(4): Failed to drop schema '{}' space '{}'",
                session->client_identification_ ,thisThreadId(), session->id(),
                schema_name.sqlName(), space.path_
            );
            reply.addError(cid, ErrorCodes::EINVALID_STATE, fmt::format("Failed to delete schema {}", space.uid_));
        }

        // Remove the space from the cache
        session->etp_space_by_path_.erase(space.path_);
        session->etp_space_by_uid_.erase(space.uid_);
    }

    // Audit logging: Single point for all failures and successes
    // 1. Log all failures from reply.rc_.ex.m_errors (Phase 1 & Phase 2 validation failures)
    logFailures();
    
    // 2. Log successes (iterate spaces_to_drop and check if in success map)
    if (reply.partCount() > 0) {
        for (size_t i = 0; i < spaces_to_drop.size(); ++i) {
            const std::string& cid = keys[i];
            const Space& space = spaces_to_drop[i];
            
            // Check if this cid is in the success map (reply.resp_.range())
            if (reply.resp_.range().find(cid) != reply.resp_.range().end()) {
                AuditLog::log(
                    *repo_->getServerConfig(),
                    session,
                    AuditLogConstants::OP_DELETE_DATA_SPACES,
                    AuditLogConstants::RESULT_SUCCESS,
                    AuditLogConstants::ACTION_DELETE,
                    space.uri().to_string(),
                    "Dataspace successfully deleted"
                );
            }
        }
    }

    /*
    ** We should have an error or "part" for all Dataspaces,
    ** we simply return that status.
    if (conn_err == spaces_to_drop.size()) {
        rc.ok = false; // Total failure.
    } else if (conn_err > 0) {
        rc.ok = false; // TODO: What about partial success? (or partial failure)
    }
    */

   // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_SUCCESS,oes::core::metrics::DATASPACE_LATENCY_DELETE,start);

    reply.finalProcess();
} catch (...) {
    // Audit log: Exception handling - log for all attempted dataspaces
    OpenKVSession* session = session_.lock().get();
    if (session != nullptr) {
        for (const auto& entry : req.m_uris) {
            const std::string& uri = entry.second;
            
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_DELETE_DATA_SPACES,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_DELETE,
                uri,
                "Exception occurred during dataspace deletion"
            );
        }
    }
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR);
    etp12::handleException(process_cb);
}

//============================================================================

} // namespace oes::eml::openkv
