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

#include "Session.h"

#include <chrono>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/common/etp12/ErrorCodes.h>
#include <oes/common/epc/FilePart.h>
#include "../etp12/MsgUtils.h"

namespace oes::eml::openkv {

using ErrorCodes = ::oes::common::etp12::Datatypes::ErrorCodes;
using oes::common::etp12::Datatypes::Object::ActiveStatusKind;
namespace auth = ::oes::common::auth;
using oes::common::epc::FilePart;

//============================================================================


const std::string TransactionActControl::import_activity_template_uuid_ = "1801db47-12c3-4f8b-8ae3-25baeb528c23";
const std::string TransactionActControl::import_activity_template_title_ = "Import";
const std::string TransactionActControl::import_activity_template_parameter_title_ = "imported";

void TransactionActControl::fill_resource_(
    Resource& res,
    const std::string& name,
    const std::string& uri,
    const int64_t time
    ) {
    res.m_name = name;
    res.m_uri = uri;

    res.m_activeStatus = ActiveStatusKind::eInactive;
    
    res.m_lastChanged = time;
    res.m_storeCreated = time;
    res.m_storeLastWrite = time;
    
}

std::vector<TransactionActControl::ObjMapVal_t> TransactionActControl::get_activities_to_put(SpaceWriter& writer, const std::string& cid) {
    std::string activity_xml;
    size_t index {0};

    OES_INFO("Checking objects from {} activities, original object count: {}.", activity_obj_parameters_.size(), analyzed_objects_.size());
    for (const auto& [activity_uuid, val] : activity_obj_parameters_) {
        const auto& activity_template_uuid = val.first;
        const auto& params = val.second;
        
        bool check_related = true ;
        if (activity_template_uuid == import_activity_template_uuid_) {
            // This is an import activity, do not check the related objects.
            check_related = false ;
            OES_VERBOSE( "Activity {} is an import, using first level objects only.", activity_uuid );
        }

        auto found_template = activities_template_.find(activity_template_uuid);
        if (found_template == activities_template_.end()) {
            // If template is not in the same transaction, see if it's already in the DB
            found_template = db_activities_template_.find(activity_template_uuid);
            if (found_template == db_activities_template_.end()) {
                // We could not find the template, so nothing we can do with that activity.
                OES_VERBOSE( "Cannot find template for activity {}, ignoring.", activity_uuid );
                continue ;
            }
        }

        const auto before = analyzed_objects_.size() ;
        // Gather object identities first.
        std::vector<TypedUid> output_uids ;
        for (const auto& [param_uid, param_title]: params) {
            if (found_template->second.find(param_title) == found_template->second.end()) {
                // Not an output.
                continue;
            }
            if (analyzed_objects_.find(param_uid) == analyzed_objects_.end()) {
                continue;
            }
            output_uids.emplace_back(param_uid);
        }

        // Those are direct dependencies from the activity, we can remove them from
        // the objects that would need an import activity.
        for (const auto& uid: output_uids) {
            analyzed_objects_.erase(uid) ;
        }

        // Get all related in one go.
        if (check_related && !output_uids.empty()) {
            auto related = writer.get_related_uuids(output_uids) ;
            for (const auto& uid: related) {
                analyzed_objects_.erase(uid) ;
            }
        }
        OES_VERBOSE(
            "Activity {} checked, {} output parameters, found {} objects => remaining {}",
            activity_uuid, output_uids.size(), before-analyzed_objects_.size(), analyzed_objects_.size()
        );
    }
    OES_INFO("All activities checked, adding {} objects to an auto-import activity.", analyzed_objects_.size());
    
    if (analyzed_objects_.size() == 0) {
        return {} ;
    }

    for (const auto& kv : analyzed_objects_) {
        const auto& obj_info = kv.second;
               
        activity_xml.append(
            oes::common::epc::get_import_activity_Parameter(
                "resqml20:DataObjectParameter",
                TransactionActControl::import_activity_template_parameter_title_,
                obj_info.ml_,
                obj_info.version_,
                obj_info.type_,
                obj_info.title_,
                obj_info.uuid_,
                "AUTHORITY",
                index++
            )
        );
    }

    const auto creation_time = std::chrono::system_clock::now();
    const auto dt = oes::core::formatDateTime("%Y-%m-%dT%H:%M:%SZ", creation_time, oes::core::FracSecRes::eMilliSecRound);
    const auto creation_time_sec { std::chrono::duration_cast<std::chrono::seconds>(creation_time.time_since_epoch()).count() };
    
    // make the Activity data
    const auto new_activity_uuid = Guid::make().toXmlString();

    auto [act_data, footer] = oes::common::epc::get_xml_header_and_footer(
        "resqml20:Activity",
        "resqml20:obj_Activity",
        new_activity_uuid
    );

    const std::string act_title = "Scenario_automated_import_" + dt;
    act_data.append(oes::common::epc::get_import_activity_Citation(
        act_title,
        cid,
        dt,
        "pdgm:portal-import:1"
    ));
    act_data.append(oes::common::epc::get_import_activity_ADescriptor(
        "eml20:DataObjectReference",
        "obj_ActivityTemplate",
        TransactionActControl::import_activity_template_title_, // title -  as same title in template citation
        TransactionActControl::import_activity_template_uuid_,
        "AUTHORITY"
    ));
    act_data.append(activity_xml);
    act_data.append(oes::common::epc::get_import_activity_Extradata(
        "activityStatus",
        "Done"
    ));
    act_data.append(footer);

    // Activity
    auto act_obj = DataObject();
    act_obj.m_format = "xml";
    act_obj.m_data.assign(act_data.begin(), act_data.end());
    TransactionActControl::fill_resource_(
        act_obj.m_resource,
        act_title,
        new_activity_uuid,
        creation_time_sec
    );

    const auto cid_act = fmt::format(
        "obj_Activity_{}.xml",
        new_activity_uuid
    );
    
    if (!has_import_template_already_) {
        // make the Activity Template data
        const auto act_template_data = oes::common::epc::get_activity_template_data(
            cid,
            dt
        );
        // turn the datas into objects!
        // Activity Template
        auto template_act_obj = DataObject();
        template_act_obj.m_format = "xml";
        template_act_obj.m_data.assign(act_template_data.begin(), act_template_data.end());
        TransactionActControl::fill_resource_(
            template_act_obj.m_resource,
            TransactionActControl::import_activity_template_title_,
            TransactionActControl::import_activity_template_uuid_,
            creation_time_sec
        );

        const auto cid_act_template = fmt::format(
            "obj_ActivityTemplate_{}.xml",
            TransactionActControl::import_activity_template_uuid_
        );
        return {
            std::make_pair(cid_act_template, template_act_obj),
            std::make_pair(cid_act, act_obj),
        };
    }
    
    return {
        std::make_pair(cid_act, act_obj),
    };
}

bool TransactionActControl::upsertActivityObjs(SpaceWriter& writer, const std::string& user_id, const std::string& space_uri) {
    bool has_nullified_guid = false ;
    if( !has_import_template_already_ ){
        // Attempt to find the template
        if(writer.select_obj_id(
            writer.typ_of(
                writer.uri_of("http://www.energistics.org/energyml/data/resqmlv2", "rsq", "resqml20", "2.0"), 
                "obj_ActivityTemplate"
            ),
            Guid::parse(TransactionActControl::import_activity_template_uuid_)
        )!=-1){
            has_import_template_already_ = true;
        }
    }

    auto objects = get_activities_to_put(writer, user_id);
    
    if(objects.size() > 0){
        OES_INFO("Commiting missing activity object for dataspace {}", space_uri);
    }

    std::vector<ObjRef> refs;
    for (const auto& [cid, obj] : objects) {
        std::string xml_str(obj.m_data.begin(), obj.m_data.end());

        FilePart::ScanRc scan_rc;
        FilePart::ScanData scan;

        std::tie(scan, scan_rc) = FilePart::scanData(
            xml_str,
            FilePart::eCited |
            FilePart::eRefs
        );

        if (scan_rc != FilePart::eOK_DONE) {
            continue;
        }

        Guid uid = Guid::parse(scan.cited_.uuid_);
        if (!uid.isValid()) {
            continue;
        }

        const eml_int64 uri_id = writer.uri_of(scan.name_.xmlns_uri_, {}, {}, scan.name_.schema_version_);
        std::string type_name = scan.name_.local_name_;
        if(!scan.name_.xsi_type_.empty()){
            type_name = scan.name_.xsi_type_.substr(scan.name_.xsi_type_.find(':')+1);
        }
        const int typ_id = writer.typ_of(uri_id, type_name);
        eml_int64 old_id = writer.select_obj_id(typ_id, uid);
        if( old_id >= 0 ){
            writer.null_res_guid(typ_id, uid);
            has_nullified_guid = true ;
        }

        auto [xdata_json, xdata_avro] = oes::eml::etp12::xdataToJsonAndAvroBinary(
            obj.m_resource.m_customData
        );
        eml_int64 obj_id = writer.insert_res(typ_id, scan.cited_, xdata_json, xdata_avro);
        if( old_id >= 0 ){
            writer.update_dst_id(old_id, obj_id);
        }

        writer.insert_uncompressed_obj(obj_id, typ_id, uid, xml_str);

        for (const auto& xml_ref : scan.refs_) {
            refs.emplace_back(writer.to_obj_ref(obj_id, xml_ref.first, xml_ref.second));
        }
    }

    if (refs.size() < 50) {
        for (const auto& ref: refs) {
            writer.insert_rel(ref);
        }
    } else {
        writer.bulk_insert_rels(refs, 50);
    }

    return has_nullified_guid ;
}

//============================================================================
// Use to discover which dataspace is engaged in a write transaction
std::set<std::string> OpenKVTransaction::dataspace_being_written_;

OpenKVTransaction::OpenKVTransaction(
    const Connection_ptr& dbConnection, bool ro, std::string msg, std::vector<EmlUri> dataspaceUris, bool local
) : uid_(Guid::make())
  , ro_(ro)
  , in_api_(false)
  , local_(local)
  , msg_(std::move(msg))
  , dataspaceUris_(std::move(dataspaceUris))
  , begin_(std::chrono::high_resolution_clock::now())
  , db_transaction_(dbConnection->begin_transaction(ro))
{
    if(!ro){
        for (auto& dataspaceUri : dataspaceUris_) {
            dataspace_being_written_.emplace(dataspaceUri.to_string());
        }
    }
}

OpenKVTransaction::~OpenKVTransaction(){
    if(!ro_){
        // remove datapaceUris_ from dataspace_writeable_
        for (auto& dataspaceUri : dataspaceUris_) {
            dataspace_being_written_.erase(dataspaceUri.to_string());
        }
    }
}

#ifndef _BullseyeCoverage
std::chrono::high_resolution_clock::time_point OpenKVTransaction::lastApi() const
{
    auto max_last = last_api_[0];
    for (int i = 1; i < nApiType; i++) {
        max_last = std::max(max_last, last_api_[i]);
    }
    return max_last;
}

std::chrono::high_resolution_clock::duration OpenKVTransaction::idleFor() {
    return std::chrono::high_resolution_clock::now() - lastApi();
}
#endif

//============================================================================
OpenKVScopedTxApi::OpenKVScopedTxApi(ApiType api_type)
    : api_type_(api_type){}

OpenKVScopedTxApi::OpenKVScopedTxApi(std::shared_ptr<OpenKVTransaction>& tx, ApiType api_type)
    : tx_(tx)
    , api_type_(api_type)
{
    //warning: comparison is always true due to limited range of data type [-Wtype-limits]
    //assert(0 <= int(api_type) && int(api_type) < nApiType);

    if (tx) {
        // APIs shouldn't nest (not re-entrant) inside a transaction
        // WARNING: FIXME: Above probably false in true ASYNC mode of ETP 1.2
        assert(tx->in_api_ == false);

        if (tx->ro_ && api_type != ApiType::eGet) {
            throw etp12::errorStatus(ErrorCodes::EREQUEST_DENIED, "Read-only Transaction");
        }

        ++tx->api_calls_[static_cast<int>(api_type)];
        tx->last_api_[static_cast<int>(api_type)] = std::chrono::high_resolution_clock::now();
        tx_->in_api_ = true;
    }
}

OpenKVScopedTxApi::~OpenKVScopedTxApi() {
    heartbeat();

    if (tx_) {
        tx_->in_api_ = false;
    }
}

void OpenKVScopedTxApi::heartbeat() {
    if (tx_) {
        assert(tx_->in_api_ == true);
        tx_->last_api_[static_cast<int>(api_type_)] = std::chrono::high_resolution_clock::now();
    }
}

//============================================================================

OpenKVSession::OpenKVSession(etp12::Handshake handshake, auth::AuthenticatedUser_ptr user, auth::Entitlement_ptr entitlement)
    : Session(std::move(handshake)
    , std::move(user)
    , std::move(entitlement))
{ }


void OpenKVSession::resumeFromExistingSession(std::shared_ptr<Session> other) {
    auto openkv_ptr = std::dynamic_pointer_cast<OpenKVSession>(other);
    if (!openkv_ptr) {
        return;
    }

    etp_space_by_path_ = std::move(openkv_ptr->etp_space_by_path_);
    etp_space_by_uid_ = std::move(openkv_ptr->etp_space_by_uid_);
    pending_put_data_objects_ = std::move(openkv_ptr->pending_put_data_objects_);

    tx_ = std::move(openkv_ptr->tx_);
    Session::resumeFromExistingSession(other);
}

//============================================================================

} // namespace oes::eml::openkv
