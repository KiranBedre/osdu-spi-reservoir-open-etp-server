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
#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "BaseProto.h"
#include "SpaceOSDUProto.h"
#include "SpaceProto.h"

#include "Repo.h"
#include "Session.h"
#include "AdminDB.h"
#include "SpaceDDL.h"
#include "SpaceDML.h"
#include "TransProto.h"
#include "PGStream.h"

#include <oes/eml/etp12/Server.hpp>
#include <oes/common/auth/AuthN.h>
#include <oes/common/auth/Entitlement.h>
#include <oes/eml/etp12/MsgUtils.h>
#include <oes/eml/etp12/MultiPartReply.hpp>

#include <oes/osdu/api/EntitlementClient.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThreadUtils.h>

#include <oes/client/Client.h>
#include <oes/client/Client.hpp>

#include <oes/postgresql/Transaction.h>
#include <oes/postgresql/Format.hpp>
#include <oes/postgresql/PhysicalName.h>


using namespace oes::eml::etp12::Datatypes;

using oes::eml::etp12::errorStatus;
using oes::eml::etp12::addErrorStatus;

//============================================================================


namespace oes::eml::openkv {

//============================================================================

OpenKVDataspaceOSDU::OpenKVDataspaceOSDU(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

void OpenKVDataspaceOSDU::GetDataspaceInfo(
    IRequest& ireq, GetDataspaceInfo_req_t req,
    EtpResponseHandler<GetDataspaceInfo_res_t> process_cb // messageTypeId 2
) try {
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    // Authorization (1 from 2)
    if (session->entitlement_ && session->entitlement_->isValid()) {
        if (!session->entitlement_->hasServicePermissionRead()) {
            OES_ERROR(
                "User {} has no permissions to get dataspaces",
                session->userMail()
            );
            return process_cb(errorStatus(EREQUEST_DENIED, "Not authorized to GetDataspaces"), {});
        }
    } else if (session->entitlement_) {
        return process_cb(errorStatus(EAUTHORIZATION_REQUIRED, "Invalid Entitlement."), {});
    }

    // recheck the session after postgres call
    session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }
    for (const auto& uri : req.m_uris) {
        EmlUri space_uri = EmlUri::parse(uri.second);
        ErrorInfo err;
        if (!space_uri.isValid()) {
            err.m_code = EINVALID_URI;
            err.m_message = fmt::format( "Invalid URI: {}", uri.second);
            reply.addError(uri.first, err);
            continue;
        }

        if (space_uri.type_ != EmlUri::eDATA_SPACE) {
            err.m_code = EINVALID_URI;
            err.m_message = fmt::format( "Expecting Dataspace URI: {}",  uri.second);
            reply.addError(uri.first, err);
            continue;
        }
        const Space space = repo_->findSpaceInfo(space_uri.dataspace_);
        if(!space.isValid()){
            err.m_code = ENOT_FOUND;
            err.m_message = fmt::format("Dataspace {} not found", space_uri.dataspace_);
            reply.addError(uri.first, err);
            continue;
        }
        space_uri.dataspace_ = space.uid_;
        
        // Authorization (2 from 2)
        if(!checkDataspaceAccess(space, AccessType::CAN_READ, err)){
            err.m_code = EREQUEST_DENIED;
            // more detail message is already set in checkDataspaceAccess
            // err.m_message = fmt::format("No permission to access Dataspace {}", space.uid_);
            reply.addError(uri.first, err);
            continue;
        }

        const oes::eml::etp12::XDataMap data_map = space.xdataAsMap();

        size_t size = space_uri.to_string().size()+8; //string size + 8 bytes for length
        size += space.uid_.size()+8; //string size + 8 bytes for length
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

        oes::eml::etp12::Datatypes::Object::Dataspace etp_space;
        etp_space.m_uri = space_uri.to_string();
        etp_space.m_path = space.path_;

        etp_space.m_storeCreated = space.created_;
        etp_space.m_storeLastWrite = space.last_write_;
        etp_space.m_customData = std::move(data_map);

        // Set it if actually read-only.
        // Makes xdata roundtrip testing easier.
        const bool can_write = checkDataspaceAccess(space, AccessType::CAN_WRITE, err);
        etp_space.m_customData["read-only"].m_item = space.ro_ || !can_write;
        etp_space.m_customData["locked"].m_item = space.ro_;

        // Add the space size to the extra metadata
        etp_space.m_customData["size"].m_item = space.size_str_;
        
        // add acl and legal tags
        ArrayOfString temp;
        temp.m_values = std::move(extractXDataValues(data_map, "viewers"));
        if (!temp.m_values.empty()) {
            etp_space.m_customData["viewers"].m_item = temp;
        }

        temp.m_values = std::move(extractXDataValues(data_map, "owners"));
        if (!temp.m_values.empty()) {
            etp_space.m_customData["owners"].m_item = temp;
        }
        
        temp.m_values = std::move(extractXDataValues(data_map, "legaltags"));
        if (!temp.m_values.empty()) {
            etp_space.m_customData["legaltags"].m_item = temp;
        }



        /*
        "acl": {"legal": {
        "legaltags": [
            "LEGAL_TAGS"
        ],
        "otherRelevantDataCountries": [
            "LEGAL_COUNTRIES"
        ],
        "status": "compliant"
    }
      "owners": [
        "data.rdms-TEST_DATASPACE_PATH.owners@DATA_PARTITION_ID.DOMAIN_NAME"
      ],
      "viewers": [
        "data.rdms-TEST_DATASPACE_PATH.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
      ]


      "legal": {
        "legaltags": [
            "LEGAL_TAGS"
        ],
        "otherRelevantDataCountries": [
            "LEGAL_COUNTRIES"
        ],
        "status": "compliant"
    }
    },
        */

        reply.addMapPart(uri.first, etp_space);

        // rough estimate of how big the data is for this "part"
        reply.addBytes( size );
    }

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

/*!
* \brief Copy the content of dataspaces to a target dataspace using references
* \param ireq ignored
* \param req ETP request
* \param process_cb ETP response callback
*/
void OpenKVDataspaceOSDU::CopyDataspacesContent( // messageTypeId 3
    IRequest& ireq, CopyDataspacesContent_req_t req,
    EtpResponseHandler<CopyDataspacesContent_res_t> process_cb // messageTypeId 4
) try {
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));
    if (req.m_dataspaces.empty()) {
        return process_cb(errorStatus(EINVALID_ARGUMENT, "Nothing to add"), {});
    }

    EmlUri dest_uri = EmlUri::parse(req.m_targetDataspace);
    if( dest_uri.type_ != EmlUri::eDATA_SPACE){
        return process_cb(errorStatus(EINVALID_ARGUMENT, "Invalid target dataspace URI"), {});
    }

    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    etp12::MultiPartReply reply(*session, process_cb);

    Space dest_space = repo_->findSpaceInfo(dest_uri.dataspace_);
    if(!dest_space.isValid()){
        reply.rc_.ok = false;
        ErrorInfo err;
        err.m_code = EINVALID_ARGUMENT;
        err.m_message = fmt::format("Dataspace {} not found", dest_uri.dataspace_);
        reply.rc_.ex.m_error = std::move(err);
        return reply.fatalError();
    }

    dest_uri.dataspace_ = dest_space.uid_;

    // Authorization to write in the target dataspace
    ErrorInfo w_err;
    if(!checkDataspaceAccess(dest_space, AccessType::CAN_WRITE, w_err)){
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(w_err);
        return reply.fatalError();
    }

    std::vector< Space > spaces;
    std::vector< std::string > ids;
    for(auto& sp : req.m_dataspaces){
        EmlUri uri = EmlUri::parse(sp.second);
        if (!uri.isValid()) {
            reply.addError(sp.first, EINVALID_URI, "Invalid URI");
            continue;
        }

        Space space = repo_->findSpaceInfo(uri.dataspace_);
        if(!space.isValid()){
            reply.addError(sp.first, ENOT_FOUND,fmt::format("Space {} not found", uri.dataspace_));
            continue;
        }

        // Authorization to read in the origin dataspace
        ErrorInfo s_err;
        if(!checkDataspaceAccess(space, AccessType::CAN_READ, s_err)){
            reply.addError(sp.first, s_err);
        }
        if(!repo_->getAdminDb()->spaceIsReadOnly(space)){
            ErrorInfo err;
            err.m_code = EINVALID_ARGUMENT;
            err.m_message = fmt::format("Origin dataspace {} is not locked", space.uid_);
            reply.addError(sp.first, err);
            continue;
        }
        spaces.push_back(space);
        ids.push_back(sp.first);
    }
    // Do the copy by reference
    std::vector<ErrorInfo> errors = repo_->getAdminDb()->importSpaces(dest_space, spaces);
    OES_ASSERT(errors.size() == ids.size(), return reply.fatalError(););

    // Create the response
    int i = 0;
    for( auto err : errors){
        if(err.m_code == IS_OK){
            reply.addMapPart(ids[i], "true");
        }else{
            reply.addError(ids[i], err);
        }
        i++;
    }

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

/*!
 * \brief Lock or unlock a list of dataspaces
 * \param ireq ignored
 * \param req ETP request
 * \param process_cb ETP response callback
*/
 void OpenKVDataspaceOSDU::LockDataspaces( // messageTypeId 5
    IRequest& ireq, LockDataspaces_req_t req,
    EtpResponseHandler<LockDataspaces_res_t> process_cb // messageTypeId 6
) try {
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    if (req.m_uris.empty()) {
        return process_cb(errorStatus(EINVALID_ARGUMENT, "Nothing to lock/unlock"), {});
    }

    LockDataspaces_res_t resp;
    etp12::Protocol::EtpResponseStatus rc;
    rc.ok = true;
    for(const auto& ds : req.m_uris){
        EmlUri uri = EmlUri::parse(ds.second);
        Space space = repo_->findSpaceInfo(uri.dataspace_);
        if(!space.isValid()){
            addErrorStatus(rc, ds.first, ENOT_FOUND, "Dataspace not found");
            continue;
        }
        if(space.ro_==req.m_lock){
            addErrorStatus(rc, ds.first, EINVALID_ARGUMENT, fmt::format("Dataspace is already {}",space.ro_?"locked":"unlocked"));
            continue;
        }
        // recheck the session after postgres call
        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            rc = errorStatus(EINVALID_STATE ,"SVR Session has been forcefully terminated.");
            break;
        }
        ErrorInfo err;
        if( session->entitlement_ &&
            session->entitlement_->isValid() &&
            !checkDataspaceAccess(space, AccessType::CAN_WRITE, err, !req.m_lock)
        ){
            err.m_code = EAUTHORIZATION_REQUIRED;
            err.m_message = fmt::format("No permission to {} Dataspace {}", req.m_lock ? "lock" : "unlock", space.uid_);
            
            addErrorStatus(rc, ds.first, err);
            continue;
        } else
        if( session->entitlement_&&
            !session->entitlement_->isValid()
        ){
            err.m_code = EAUTHORIZATION_REQUIRED;
            err.m_message = "Invalid entitlement";

            addErrorStatus(rc, ds.first, err);
            continue;
        }

        err = repo_->getAdminDb()->lockSpace(space, req.m_lock);
        if(err.m_code==IS_OK){
            // Update the space in the session cache
            space.ro_ = req.m_lock;
            const auto it = session->etp_space_by_path_.emplace(space.path_, std::move(space));
            if(it.second){
                // Not cached yet, add it
                session->etp_space_by_uid_.emplace(space.uid_, space.path_);
            }else{
                // Already cached, update the ro_ flag
                it.first->second.ro_ = req.m_lock;
            }

            // Update the response
            resp.m_success.insert({ds.first, ""});
        }else{
            addErrorStatus(rc, ds.first, err);
        }
    }
    process_cb(rc, resp);
} catch (...) {
    etp12::handleException(process_cb);
}

/*!
 * \brief Copy the content of a list of objects to a target dataspace using references
 * \param ireq ignored
 * \param req ETP request
 * \param process_cb ETP response callback
*/
void OpenKVDataspaceOSDU::CopyToDataspace( // messageTypeId 7
    IRequest& ireq, CopyToDataspace_req_t req,
    EtpResponseHandler<CopyToDataspace_res_t> process_cb // messageTypeId 8
) try {
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    if (req.m_uris.empty()) {
        return process_cb(errorStatus(EINVALID_ARGUMENT, "Nothing to copy"), {});
    }

    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }
    etp12::MultiPartReply reply(*session, process_cb);

    // Authorization to write in the target dataspace
    EmlUri target_uri = EmlUri::parse(req.m_dataspaceUri);

    Space target_space = repo_->findSpaceInfo(target_uri.dataspace_);
    if(!target_space.isValid()) {
        reply.rc_.ok = false;
        ErrorInfo err;
        err.m_code = ENOT_FOUND;
        err.m_message = fmt::format("Dataspace {} not found", target_uri.dataspace_);
        reply.rc_.ex.m_error = err;
        return reply.fatalError();
    }
    ErrorInfo w_err;
    if(!checkDataspaceAccess(target_space, AccessType::CAN_WRITE, w_err)){
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = w_err;
        return reply.fatalError();
    }

    etp12::Protocol::EtpResponseStatus rc;
    rc.ok = true;

    using Entry = std::map<std::string, std::string>::value_type;
    using Entries = std::vector<const Entry*>;

    std::map<std::string, Entries> init_space_entries;

    // Group entries by dataspace
    for (const auto& entry : req.m_uris) {
        EmlUri uri = EmlUri::parse(entry.second);
        if (!uri.isValid()) {
            reply.addError(entry.first, EINVALID_URI, "Invalid URI");
            continue;
        }
        if (!uri.isObj()) {
            reply.addError(entry.first, EINVALID_URI, "Expected eOBJ_INSTANCE URI");
            continue;
        }
        if (!uri.guid_.isValid()) {
            reply.addError(entry.first, EINVALID_URI, "Invalid Object UID");
            continue;
        }
        Space space = repo_->findSpaceInfo(uri.dataspace_);
        if(!space.isValid()) {
            reply.addError(entry.first, ENOT_FOUND,fmt::format("Space {} not found", uri.dataspace_));
        }
        uri.dataspace_ = space.uid_;
        init_space_entries[space.uid_].emplace_back(&entry);
    }

    // Assign the entries to the dataspace that matters
    std::map<std::string, Entries> entries_by_space;
    for (auto& entry : init_space_entries) {
        Entries& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()) {
            for (const Entry* nk : nks) {
                reply.addError(nk->first, ENOT_FOUND,fmt::format("Space {} not found", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        // The dataspace that matters is the one of the initial URI
        // so test it here, not later
        if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const Entry* nk : nks) {
                reply.addError(nk->first, err);
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const Entry* nk : nks) {
                reply.addError(nk->first, err);
            }
            continue;
        }

        // Check that there is at least one row in admin.space_rel that has the dataspace as origin
        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            for (const Entry* nk : nks) {
                std::string i_uri= nk->second;
                EmlUri uri = reader.containing_dataspace_uri(EmlUri::parse(i_uri));
                if(uri.to_string()!=i_uri){
                    const_cast<std::string&>(nk->second) = uri.to_string();
                    entries_by_space[uri.dataspace_].emplace_back(nk);
                }else{
                    entries_by_space[space.uid_].emplace_back(nk);
                }
            }
        }else{
            entries_by_space[space.uid_] = std::move(nks);
        }
    }

    CopyToDataspace_res_t resp;
    std::vector<ResourceRef> all_refs;
    std::vector<ReferenceRel> all_rels;
    std::set<TypedUid> all_ids;

    // Copy the content of entries
    for (auto& entry : entries_by_space) {
        Space space = repo_->findSpaceInfo(entry.first);
        Entries& nks = entry.second;
        ErrorInfo err;
        Connection_ptr space_db = repo_->getSpaceConnection(space, err);
        if (!space_db) {
            for (const Entry* nk : nks) {
                reply.addError(nk->first, err);
            }
            continue;
        }
        if(!repo_->getAdminDb()->spaceIsReadOnly(space)){
            for (const Entry* nk : nks) {
                reply.addError(nk->first, EINVALID_ARGUMENT, fmt::format("Source dataspace {} is not locked", space.uid_));
            }
            continue;
        }

        // Create space to space reference if not already there
        SpaceReader reader(space_db);
        int64_t rel_id = repo_->getAdminDb()->getSpaceReference(target_space,space);
        if(!rel_id){
            rel_id = repo_->getAdminDb()->createSpaceReference(target_space,space);
        }
        for (const Entry* nk : nks) {
            reply.addMapPart(nk->first, "true");
            // Look if the info is already in the map
            EmlUri uri_init = EmlUri::parse(nk->second);
            if(all_ids.find(stored_uid(uri_init)) != all_ids.end()){
                continue;
            }
            std::vector<ResourceRef> refs = reader.get_dependencies(nk->second);
            // Add the referenced objects to all_refs if not already there
            for (ResourceRef& ref : refs) {
                const TypedUid ref_uid{
                    EmlUri::canonicalType(ref.uri_etp_ml + '.' + ref.xml_type), ref.guid
                };
                if(all_ids.find(ref_uid) == all_ids.end()){
                    if(!ref.space_rel_id){
                        ref.space_rel_id = rel_id;
                    }
                    all_refs.push_back(ref);
                    all_ids.insert(ref_uid);
                }
                std::vector<ReferenceRel> rels = reader.get_relations(ref_uid);
                all_rels.insert(all_rels.end(), rels.begin(), rels.end());
            }
        }
    }

    Connection_ptr dest_space_db = repo_->getSpaceConnection(target_space, w_err);
    if (!dest_space_db) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = w_err;
        return reply.fatalError();
    }

    SpaceWriter writer(dest_space_db);
    std::vector<int64_t> ids = writer.insert_references(all_refs);
    writer.insert_relations(all_rels);

    repo_->updateSpaceInfo(target_space);
    
    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

/*!
 * \brief Setup a client to a source server and receive tables binary data
 * \param ireq ignored
 * \param req ETP request
 * \param process_cb ETP response callback
*/
void OpenKVDataspaceOSDU::CopyDataspaceFromRemote_trigger(
    CopyDataspaceFromRemote_req_t req,
    EtpResponseHandler<CopyDataspaceFromRemote_res_t> process_cb
) try {
    OpenKVSession* session = session_.lock().get();
    if (session == nullptr) {
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.", oes::core::thisThreadId());
        return;
    }

    oes::eml::etp12::MultiPartReply reply(*session, process_cb);

    // Authorization (1 from 2)
    if (session->entitlement_ && session->entitlement_->isValid()) {
        if (!session->entitlement_->hasServicePermissionUpdate()) {
            OES_ERROR(
                "User {} has no permissions to modify dataspace content",
                session->userMail()
            );
            return reply.fatalError(EREQUEST_DENIED, "Not authorized to modify dataspace");
        }
    } else if (session->entitlement_) {
        return reply.fatalError(EAUTHORIZATION_REQUIRED, "Invalid entitlement.");
    }

    auto target_space = repo_->findSpaceInfo(req.m_dataspacePath);
    if (!target_space.isValid()) {
        return reply.fatalError(ENOT_FOUND, "Target space not found or invalid");
    }

    // recheck the session
    session = session_.lock().get();
    if (session == nullptr) {
        OES_ERROR("SVR Thread({}): Sessio has been forcefully terminated.", oes::core::thisThreadId());
        return;
    }

    ErrorInfo err;
    // Authorization (2 from 2)
    if (!checkDataspaceAccess(target_space, AccessType::CAN_WRITE, err)) {
        OES_ERROR(
            "[{}] SVR Thread({}) Session({}) DataspaceOSDU(2424).CopyDataspaceFromRemote_trigger(9): User {} has no permissions to update dataspace {} {}",
            session->client_identification_,
            oes::core::thisThreadId(),
            session->id(),
            session->userMail(),
            target_space.path_,
            target_space.uid_
        );
        return reply.fatalError(EAUTHORIZATION_REQUIRED, err.m_message);
    }

    // A new target space is at its binary's latest version, and the copy keeps the source ids
    const Connection_ptr target_conn = repo_->getSpaceConnection(target_space, err);
    if (!target_conn) {
        return reply.fatalError(EINVALID_STATE, err.m_message);
    }
    const oes::postgresql::relational::PhysicalName target_phys(target_space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    if (target_conn->executeInt64(fmt::format("SELECT count(*) FROM (SELECT 1 FROM {}.res LIMIT 1) AS r", target_phys.quotedName())) > 0) {
        return reply.fatalError(EINVALID_STATE, fmt::format("Target dataspace {} is not empty", target_space.path_));
    }
    const std::string target_version = spaceSchemaVersion(target_conn, target_space.dbfile_);

    // New client to connect to source server
    oes::client::ClientConfig cfg;
    cfg.server_url_ = req.m_sourceServer.m_sourceServer;
    cfg.withJwtAuth(req.m_sourceServer.m_sourceJwt);
    cfg.data_partition_id_hdr_ = req.m_sourceServer.m_sourcePartition;
    cfg.max_mgs_size_ = session->negotiated_max_msg_size();
    cfg.requested_protocols_.assign({
        oes::client::data12::Protocol::eDataspaceOSDU,
        oes::client::data12::Protocol::eSupportedTypes
    });

    auto timeout = std::chrono::milliseconds(10000);
    if (req.m_extraData.find("timeout_ms") != req.m_extraData.end()) {
        timeout = std::chrono::milliseconds( std::stoull(req.m_extraData.at("timeout_ms")) );
    }
    

    auto client = std::make_unique<oes::client::Client>(cfg);
    err.m_message.clear();
    err.m_code = 0;
    if (!client->connect(timeout, &err)) {
        std::string msg = fmt::format("Failed to connect to source server: {} # {}: {}", cfg.server_url_, err.m_code, err.m_message);
        return reply.fatalError(EINVALID_ARGUMENT, msg);
    }

    auto* proto = client->space_osdu_proto();
    if (!proto) {
        return reply.fatalError(EINVALID_ARGUMENT, "Can not get space_osdu_proto");
    }

    // A server before the version exchange dumps a space as it finds it: opening it first upgrades it to 15
    auto* types_proto = client->types_proto();
    if (!types_proto) {
        return reply.fatalError(EINVALID_ARGUMENT, "The source server does not support protocol 25 (SupportedTypes)");
    }
    auto types_rpc = oes::client::rpcFor(types_proto, &oes::client::proto12::ISupportedTypes::GetSupportedTypes);
    types_rpc.req_.m_uri = EmlUri::make_space(req.m_sourceServer.m_sourceDataspacePath).to_string();
    types_rpc.req_.m_scope = oes::client::data12::Object::ContextScopeKind::eSelf;
    types_rpc.req_.m_countObjects = false;
    types_rpc.req_.m_returnEmptyTypes = false;
    std::ostringstream types_err;
    if (!types_rpc.callForEachValue(types_err, timeout, [](auto&) {})) {
        return reply.fatalError(EINVALID_ARGUMENT, "Cannot open the source dataspace: " + types_err.str());
    }

    auto rpc = oes::client::rpcFor(proto, &oes::client::proto12::IDataspaceOSDU::CopyDataspaceFromRemote);
    rpc.req_.m_dataspacePath = req.m_sourceServer.m_sourceDataspacePath;
    rpc.req_.m_action = "dump";
    rpc.req_.m_extraData["schema_version"] = target_version;

    // start local transaction
    std::vector<EmlUri> txSpaceUris;
    txSpaceUris.emplace_back(EmlUri::make_space(target_space.uid_));

    LocalTransactionWriteRaii<OpenKVDataspaceOSDU> local_tx(*this, target_space.uid_, txSpaceUris);

    session = session_.lock().get();
    if (session == nullptr) {
        OES_ERROR("SVR Thread({}): Sessio has been forcefully terminated.", oes::core::thisThreadId());
        return;
    }

    if (!local_tx.ready()) {
        auto msg = fmt::format("SVR Thread({}) Session({}) Local Tx: Failed to get transaction for {} after {} tries over {}s",
            oes::core::thisThreadId(), session->id(), txSpaceUris.at(0).to_string(), local_tx.numTries(), local_tx.tryDuration()/1000);
        return reply.fatalError(EMAX_TRANSACTIONS_EXCEEDED, msg);
    } else if(local_tx.isLocal()){
        OES_INFO(
            "SVR Thread({}) Session({}) Local Tx: Got transaction for {} after {} tries over {}s",
            oes::core::thisThreadId(), session->id(), txSpaceUris.at(0).to_string(), local_tx.numTries(), local_tx.tryDuration()/1000
        );
    }
    
    // The incoming stream overwrites the target xdata with the source one
    const oes::eml::etp12::XDataMap target_xdata = target_space.xdataAsMap();

    PGConsumer pg_consumer(target_space, repo_->getDbConnection(), target_version);
    
    std::ostringstream oss_err;
    if (!rpc.callForEachValue(
        oss_err, timeout,
        [&](auto& entry){ //CopyDataspaceFromRemoteResponse
            const auto res_copy_status = static_cast<SpaceStreamStatus>(entry.m_copyStatus);
            if (res_copy_status == SpaceStreamStatus::eOngoing ||
                res_copy_status == SpaceStreamStatus::eDone) {
                auto st = pg_consumer.consume(entry); //TODO cancel on failed
                
                if (st == SpaceStreamStatus::eFailed) {
                    throw std::runtime_error("Dataspace stream failed: " + pg_consumer.error());
                }

                if (hasTableStreamStatus(entry.m_tableStatus, SpaceTableStreamStatus::eDone)) {
                    TableData& td = reply.addVecPart();
                    td.m_copyStatus = entry.m_copyStatus;
                    td.m_tableName = entry.m_tableName;

                    reply.maybeProcess(0, true);
                }
            } else {
                OES_ERROR("Failed receiving dataspace binary data");
            }
        })) {
        OES_ERROR("Error receiving table binary data");
        return reply.fatalError(EINVALID_ARGUMENT, "Error receiving table binary data: " + oss_err.str());
    }
    // A copy from an older source is at the source's version: upgrade it like any space
    if (!repo_->getSpaceConnection(target_space, err)) {
        return reply.fatalError(EINVALID_STATE, "Cannot upgrade the copy: " + err.m_message);
    }
    if (!pg_consumer.finish()) {
        return reply.fatalError(EINVALID_STATE, pg_consumer.error());
    }
    local_tx.commit();

    // Read from the admin table, not from the session cache, to get the xdata written by the stream
    Space merged_space = repo_->getAdminDb()->findSpace(target_space.uid_);
    if (!merged_space.isValid()) {
        return reply.fatalError(ENOT_FOUND, "Target space not found after copy");
    }
    oes::eml::etp12::XDataMap merged_xdata = merged_space.xdataAsMap();
    // The original target entries win over the dumped ones
    for (const auto& [key, value] : target_xdata) {
        merged_xdata.insert_or_assign(key, value);
    }
    merged_space.setXDdataFromMap(merged_xdata);
    if (!repo_->getAdminDb()->updateSpaceXDataJson(merged_space)) {
        return reply.fatalError(EINVALID_STATE, "Failed to update the target space meta-data");
    }

    // Also update the session cache.
    session = session_.lock().get();
    if (session == nullptr) {
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.", oes::core::thisThreadId());
        return;
    }
    const auto cached_space = session->etp_space_by_path_.find(merged_space.path_);
    if (cached_space != session->etp_space_by_path_.end()) {
        cached_space->second.xdata_json_ = merged_space.xdata_json_;
    }

    // oes::client::proto12::DataspaceOSDU::CopyDataspaceFromRemoteResponse final;
    // oes::common::etp12::Protocol::EtpResponseStatus rc_final;
    // rc_final.final_part = true;
    // rc_final.ok = true;

    // return process_cb(std::move(rc_final), std::move(final));

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

/*!
 * \brief Streams tables binary to the client
 * \param ireq ignored
 * \param req ETP request
 * \param process_cb ETP response callback
*/
void OpenKVDataspaceOSDU::CopyDataspaceFromRemote_dump(
    CopyDataspaceFromRemote_req_t req,
    EtpResponseHandler<CopyDataspaceFromRemote_res_t> process_cb
) try {
    OpenKVSession* session = session_.lock().get();
    if (session == nullptr) {
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.", oes::core::thisThreadId());
        return;
    }

    oes::eml::etp12::MultiPartReply reply(*session, process_cb);

    // Authorization (1 from 2)
    if (session->entitlement_ && session->entitlement_->isValid()) {
        if (!session->entitlement_->hasServicePermissionRead()) {
            OES_ERROR(
                "User {} has no permissions to read dataspace content",
                session->userMail()
            );
            return reply.fatalError(EREQUEST_DENIED, "Not authorized to dump to another server");
        }
    } else if (session->entitlement_) {
        return reply.fatalError(EAUTHORIZATION_REQUIRED, "Invalid entitlement.");
    }

    auto source_space = repo_->findSpaceInfo(req.m_dataspacePath);
    if (!source_space.isValid()) {
        return reply.fatalError(ENOT_FOUND, "Space not found or invalid");
    }
    const auto has_ref = repo_->getAdminDb()->spaceIncludesReference(source_space);

    // recheck the session
    session = session_.lock().get();
    if (session == nullptr) {
        OES_ERROR("SVR Thread({}): Sessio has been forcefully terminated.", oes::core::thisThreadId());
        return;
    }
    
    ErrorInfo err;
    // Authorization (2 from 2)
    if (!checkDataspaceAccess(source_space, AccessType::CAN_READ, err)) {
        OES_ERROR(
            "[{}] SVR Thread({}) Session({}) DataspaceOSDU(2424).CopyDataspaceFromRemote_dump(9): User {} has no permissions to read dataspace {} {}",
            session->client_identification_,
            oes::core::thisThreadId(),
            session->id(),
            session->userMail(),
            source_space.path_,
            source_space.uid_
        );
        return reply.fatalError(EAUTHORIZATION_REQUIRED, err.m_message);
    }

    auto max_msg_size = session->negotiated_max_msg_size();
    if (max_msg_size < 1'000'000) {
        return reply.fatalError(EINVALID_ARGUMENT, "Max messagem size too small");
    }

    // Dump at the binary's latest version, never at whatever version the space was left
    if (!repo_->getSpaceConnection(source_space, err)) {
        return reply.fatalError(EINVALID_STATE, err.m_message);
    }
    const std::string source_version = spaceSchemaVersion(repo_->getDbConnection(), source_space.dbfile_);

    // A target that sends no version is a server before the version exchange, i.e. at 15
    const auto requested = req.m_extraData.find("schema_version");
    const bool exchange = requested != req.m_extraData.end();
    int magic_cookie = 0;
    space_schema_versions(magic_cookie);
    const std::string target_version = exchange ? requested->second : fmt::format("{}:15", magic_cookie);
    // A newer target takes its empty space down to the source's version; an older one gets the space projected to its own
    const int target_number = schemaVersionNumber(target_version);
    const int source_number = schemaVersionNumber(source_version);
    if (target_number < 0) {
        return reply.fatalError(EINVALID_STATE, fmt::format(
            "Source dataspace {}: the target sent an unknown schema version {}", source_space.path_, target_version
        ));
    }
    const bool project = source_number > target_number;
    if (project) {
        try {
            check_space_projection(repo_->getDbConnection(), source_space.dbfile_, source_number, target_number);
        } catch (const std::exception& e) {
            return reply.fatalError(EINVALID_STATE, fmt::format(
                "Source dataspace {} at schema {} cannot be copied to a target at {}{}: {}",
                source_space.path_, source_number, target_number,
                exchange ? "" : " (the target sent no schema version)", e.what()
            ));
        }
    }

    PGStreamer pg_streamer(
        source_space, repo_->getDbConnection(), has_ref, session->negotiated_max_msg_size(),
        exchange ? (project ? target_version : source_version) : std::string(),
        project ? target_number : 0
    );
    
    SpaceStreamStatus status = SpaceStreamStatus::eOngoing;
    while ( status == SpaceStreamStatus::eOngoing) {
        
        TableData table_data;// = reply.addVecPart();
        status = pg_streamer.stream(table_data);

        table_data.m_copyStatus = static_cast<uint8_t>(status);

        size_t next_bytes = 2 + table_data.m_tableName.size() + table_data.m_tableData.size();

        reply.maybeProcess(next_bytes, false);

        auto& td = reply.addVecPart();
        td.m_tableName = std::move(table_data.m_tableName);
        td.m_tableData= std::move(table_data.m_tableData);
        td.m_tableStatus = table_data.m_tableStatus;
        td.m_copyStatus = table_data.m_copyStatus;

        reply.addBytes(next_bytes);

    }

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

/*!
 * \brief Copy the content of a dataspace from a remote server. Behavior depends on action option
 * \param ireq ignored
 * \param req ETP request
 * \param process_cb ETP response callback
*/
void OpenKVDataspaceOSDU::CopyDataspaceFromRemote( // messageTypeId 9
    IRequest& ireq, CopyDataspaceFromRemote_req_t req,
    EtpResponseHandler<CopyDataspaceFromRemote_res_t> process_cb // messageTypeId 10
) try {
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    if (req.m_action == "trigger") {
        return CopyDataspaceFromRemote_trigger(req, process_cb);
    } else if (req.m_action == "dump") {
        return CopyDataspaceFromRemote_dump(req, process_cb);
    }
    
    return process_cb(errorStatus(EINVALID_ARGUMENT, "requested action must be trigger or dump"), {});

} catch (...) {
    etp12::handleException(process_cb);
}


//============================================================================

} // namespace oes::eml::openkv
