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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "StoreProto.h"

#include "Repo.h"
#include "Session.h"
#include "SpaceDML.h"
#include "TransProto.h"
#include "AuditLog.h"

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThreadUtils.hpp>
#include <oes/core/utils/Otel.h>

#include <oes/postgresql/Statement.h>

#include "../etp12/Server.hpp"
#include "../etp12/MsgUtils.h"
#include "../etp12/MultiPartReply.hpp"

#include <oes/common/epc/ActivityUtils.h>
#include <oes/common/epc/FilePart.h>
#include <oes/common/AvroUtils.h>
#include <oes/common/etp12/MsgFlags.h>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsHelper.h>
#include <oes/core/utils/MetricsNames.h>

#include <chrono>

using oes::common::etp12::Datatypes::Object::DataObject;
using oes::common::etp12::Datatypes::EINVALID_ARGUMENT;
using oes::common::etp12::Datatypes::EINVALID_OBJECT;
using oes::common::etp12::Datatypes::EINVALID_STATE;
using oes::common::etp12::Datatypes::EINVALID_URI;
using oes::common::etp12::Datatypes::ENOT_FOUND;
using oes::common::etp12::Datatypes::ENOTSUPPORTED;
using oes::common::etp12::Datatypes::EMAX_TRANSACTIONS_EXCEEDED;

using oes::common::etp12::Protocol::EtpResponseStatus;
using oes::common::etp12::Protocol::Store::PutDataObjects;
using oes::common::etp12::Protocol::Store::PutDataObjectsResponse;

using oes::core::thisThreadId;

namespace {

std::map<oes::core::Guid, std::vector<uint8_t>> chunk_buffers_;
std::map<int64_t, oes::common::etp12::Protocol::Store::PutDataObjects> put_requests_;

using namespace oes::eml::openkv;

using oes::common::epc::FilePart;

//============================================================================
/*
  process_activities process the data object and keep data in a transaction class
  With these data we can generate a new import activity just before the commit and avoid
  having any object without any related activity 
*/

const std::set<std::string> ignored_types_in_imports = {
    "obj_EpcExternalPartReference",
    "EpcExternalPartReference",
    "obj_LocalDepth3dCrs",
    "LocalDepth3dCrs",
    "obj_LocalTime3dCrs",
    "LocalTime3dCrs"
};

//! ETP type of a scanned data object, e.g. "resqml20.obj_Grid2dRepresentation".
std::string etp_type_of(const FilePart::ScanData& scan) {
    std::string type_name = scan.name_.local_name_;
    if(!scan.name_.xsi_type_.empty()){
        type_name = scan.name_.xsi_type_.substr(scan.name_.xsi_type_.find(':')+1);
    }

    std::string ml = "resqml";
    if(boost::ends_with(scan.name_.xmlns_uri_,"commonv2")){
        ml = "eml";
    }else if(boost::ends_with(scan.name_.xmlns_uri_,"prodmlv2")){
        ml = "prodml";
    }else if(boost::ends_with(scan.name_.xmlns_uri_,"witsmlv2")){
        ml = "witsml";
    }

    // Same "<ml><major><minor>.<type>" shape as DataObjectReference::QualifiedType_
    const std::string& version = scan.name_.schema_version_;
    ml += (version.size() > 2) ? std::string{version[0], version[2]} : std::string("20");
    return EmlUri::canonicalType(ml + '.' + type_name);
}

//! EML 2.0 names the external part by uuid, EML 2.3 by URI; a file name means the owner itself.
Guid hdf_proxy_guid(const oes::common::epc::xml::Hdf5Dataset& hdf, const Guid& owner) {
    if (!hdf.HdfProxy_.Uuid_.empty()) {
        return Guid::parse(hdf.HdfProxy_.Uuid_);
    }
    const Guid guid = EmlUri::parse(hdf.HdfProxy_.EnergisticsUri_).guid_;
    return guid.isValid() ? guid : owner;
}

void process_activities(
    SpaceWriter& writer,
    const FilePart::ScanData& scan,
    const std::string& obj_xml,
    TransactionActControl& trans_ctl
) {
    Guid uid = Guid::parse(scan.cited_.uuid_);
    if (!uid.isValid()) {
        return;
    }

    std::string type_name = scan.name_.local_name_;
    if(!scan.name_.xsi_type_.empty()){
        type_name = scan.name_.xsi_type_.substr(scan.name_.xsi_type_.find(':')+1);
    }
    const std::string etp_type = etp_type_of(scan);
    
    if (type_name == "Activity" || type_name == "obj_Activity") {
        // find the pair in scan.refs_ where the first element equal to "rsq:ActivityDescriptor"
        auto it = std::find_if( scan.refs_.begin(), scan.refs_.end(),
            [](const std::pair<std::string, oes::common::epc::xml::DataObjectReference>& element){ return element.first == "rsq:ActivityDescriptor";} );

        auto activity_template_uuid = (it == scan.refs_.end()) ? "" : it->second.Uuid_;

        if (!activity_template_uuid.empty()) {
            // Attempt to find the template in DB
            auto template_found = trans_ctl.db_activities_template_.find(activity_template_uuid);
            if (template_found == trans_ctl.db_activities_template_.end() ) {
                auto [activity_template_obj_id, template_xml] = writer.select_obj_id_xml(
                    writer.typ_of(
                        writer.uri_of("http://www.energistics.org/energyml/data/resqmlv2", "rsq", "resqml20", "2.0"), 
                        "obj_ActivityTemplate"
                    ),
                    Guid::parse(activity_template_uuid)
                );
                if( activity_template_obj_id != -1) {
                    // Scanning the xml will not give us anything about the parameters, so we have to resort to string matching.
                    // At least we can respect tscan.name_.xmlns_prefix_. This scan cannot fail, the xml has already been scanned
                    // before being inserted in the database.
                    FilePart::ScanRc tscan_rc;
                    FilePart::ScanData tscan;
                    std::tie(tscan, tscan_rc) = FilePart::scanData(
                        template_xml, // note: copied, not moved, since used below too
                        FilePart::eCited // We only want the base citation.
                    );

                    auto template_titles = oes::common::epc::extract_activities_template_titles(
                        template_xml, tscan.name_.xmlns_prefix_
                    );
                    trans_ctl.db_activities_template_.emplace(
                        std::make_pair(activity_template_uuid, std::move(template_titles))
                    );
                }
            }

            TransactionActControl::Uuid_title_t uuids_and_titles ;
            for( auto ref: scan.refs_) {
                if( ref.first == "rsq:Parameter/rsq:DataObject" && ref.second.isParam_) {
                    uuids_and_titles.emplace(
                        TypedUid{
                            EmlUri::canonicalType(ref.second.QualifiedType_),
                            Guid::parse(ref.second.Uuid_)
                        },
                        ref.second.paramTitle_
                    );
                }
            }

            trans_ctl.activity_obj_parameters_.emplace(
                std::make_pair(
                    //obj.m_resource.m_uri,
                    scan.cited_.uuid_,
                    std::make_pair(activity_template_uuid, std::move(uuids_and_titles))
                )
            );
            
        }

    } else if (type_name == "ActivityTemplate" || type_name == "obj_ActivityTemplate") {
        if (scan.cited_.uuid_ == TransactionActControl::import_activity_template_uuid_) {
            trans_ctl.has_import_template_already_ = true;
        }
        // scan does not contain anything about the parameters, so we have to resort to string matching.
        // At least we can respect scan.name_.xmlns_prefix_
        auto template_titles = oes::common::epc::extract_activities_template_titles(
            obj_xml, scan.name_.xmlns_prefix_
        );
        trans_ctl.activities_template_.emplace(
            std::make_pair(scan.cited_.uuid_, std::move(template_titles))
        );
    } else {
        // only add if they are found as new objects
        if(trans_ctl.new_objs_uuids_.find({etp_type, uid}) == trans_ctl.new_objs_uuids_.end()){
            return;
        }

        if( ignored_types_in_imports.find(type_name) != ignored_types_in_imports.end()) {
            return;
        }

        std::string ml = "resqml";
        if(boost::ends_with(scan.name_.xmlns_uri_,"commonv2")){
            ml = "eml";
        }else if(boost::ends_with(scan.name_.xmlns_uri_,"prodmlv2")){
            ml = "prodml";
        }else if(boost::ends_with(scan.name_.xmlns_uri_,"witsmlv2")){
            ml = "witsml";
        }
        trans_ctl.analyzed_objects_.try_emplace(
            TypedUid{etp_type, uid},
            oes::common::epc::AnalyzedObject(
                scan.cited_.uuid_,
                ml,
                scan.name_.schema_version_,
                type_name,
                scan.cited_.Title_
            )
        );
    }
}

void upsertObjs(
    oes::eml::etp12::MultiPartReply<PutDataObjectsResponse>& reply,
    SpaceWriter& w,
    OpenKVTransaction::OrphanAryMap& orphan_arys,
    OpenKVTransaction::MissingAryMap& missing_arys,
    const std::vector<
        const std::map<std::string, DataObject>::value_type*
    >& entries,
    TransactionActControl* trans_ctl_ptr
) {

    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "upsertObjs",
        {{"entries", std::to_string(entries.size())}});

    // Since we store obj_id for the reference target too,
    // we must wait to have all of them to resolve res_key.
    std::vector<ObjRef> refs;
    
    for (const auto& entry : entries) {
        const std::string& cid = entry->first;
        const DataObject& obj = entry->second;

        std::string partIdXml(obj.m_data.begin(), obj.m_data.end());

        FilePart::ScanRc scan_rc;
        FilePart::ScanData scan;
        std::tie(scan, scan_rc) = FilePart::scanData(
            partIdXml, // note: copied, not moved, since used below too
            FilePart::eCited |
          //FilePart::eXData | No eXData, see insert_res() below
            FilePart::eRefs  |
            FilePart::eHdfs
        );

        if (scan_rc != FilePart::eOK_DONE) {
            reply.addError(cid, EINVALID_OBJECT, "Cannot parse XML");
            continue;
        }

        Guid uid = Guid::parse(scan.cited_.uuid_);
        if (!uid.isValid()) {
            reply.addError(cid, EINVALID_OBJECT, "Invalid UUID");
            continue;
        }

        const eml_int64 uri_id = w.uri_of(scan.name_.xmlns_uri_, {}, {}, scan.name_.schema_version_);
        std::string type_name = scan.name_.local_name_;
        if(!scan.name_.xsi_type_.empty()){
            type_name = scan.name_.xsi_type_.substr(scan.name_.xsi_type_.find(':')+1);
        }
        const int typ_id = w.typ_of(uri_id, type_name);
        eml_int64 old_id = w.select_obj_id(typ_id, uid);
        if( old_id >= 0 ){
            w.null_res_guid(typ_id, uid);
        } else {
            if (trans_ctl_ptr) { // this is a new object
                trans_ctl_ptr->new_objs_uuids_.insert({etp_type_of(scan), uid});
            }
        }

        // WARNING: We mostly ignore obj.m_resource, except for meta-data,
        // and instead trust the information we extracted from the XML instead.
        // TODO: Warn on discrepancies?

        // We do not store the XML's xdata here (which we don't extract in fact, see above),
        // it is meant for the xdata coming via ETP only.
        auto [xdata_json, xdata_avro] = oes::eml::etp12::xdataToJsonAndAvroBinary(
            obj.m_resource.m_customData
        );
        eml_int64 obj_id = w.insert_res(typ_id, scan.cited_, xdata_json, xdata_avro);
        if( old_id >= 0 ){
            w.update_dst_id(old_id, obj_id);
        }

        w.insert_uncompressed_obj(obj_id, typ_id, uid, partIdXml);

        // Convert extracted XML refs, to OpenKV refs
        for (const auto& xml_ref : scan.refs_) {
            // the ref's target is not resolved now, but in .insert_rel() later below
            refs.emplace_back(w.to_obj_ref(obj_id, xml_ref.first, xml_ref.second));
        }

        // Convert extracted XML HDF5 array references, to OpenKV hdfs
        for (auto& xml_hdf : scan.hdfs_) {
            const std::string& xpath = xml_hdf.first;
            const eml_int64 xpa_id = w.xpa_of(xpath);

            std::string ary_path = std::move(xml_hdf.second.PathInHdfFile_);
            if (ary_path[0] == '/') {
                ary_path.erase(0, 1);
            }

            // The dataset lives in the EPC external part this object points at
            const Guid epc_guid = hdf_proxy_guid(xml_hdf.second, uid);
            OpenKVTransaction::AryKey ary_key{ epc_guid, std::move(ary_path) };

            // If array already exists, remove from missing
            SqlAry sql_ary = w.select_ary(ary_key.first, ary_key.second);
            if( sql_ary.valid() ){
                w.update_ary_ids(sql_ary.ary_id_, obj_id, xpa_id);
                auto ary_found = orphan_arys.find(ary_key);
                if (ary_found != orphan_arys.end()) {
                    orphan_arys.erase(ary_found); // no longer an orphan
                }
                continue;
            }

            // Array not yet there
            OpenKVTransaction::MissingAry miss_ary{ -1, obj_id, xpa_id };
            auto add_ok = missing_arys.emplace(std::move(ary_key), miss_ary);
            if (!add_ok.second) {
                // multiple objects referencing the same array. Not normal
            }
        }

        // No need to fill in details about changes in ContainedObjects, as they are currently not supported
        reply.addMapPart(cid);
        
        if (trans_ctl_ptr) {
            process_activities(
                w,
                scan,
                partIdXml,
                *trans_ctl_ptr
            );
        }
        
    }

    // cppcheck-suppress unreadVariable
    auto rels_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__ + ".insertRels",
        {{"count", std::to_string(refs.size())}});
    
    if( refs.size() < 50 ) {
        // There are less refs to be inserted than the bulk beffer size, send them one by one.
        for (const ObjRef& ref : refs) {
            // errors (i.e. missing refs) are tracked inside the writer,
            // and calling-code checks missing_reference_count(), no need
            // to track insert_rel()'s returned code.
            w.insert_rel(ref);
        }
    } else {
        // Attempt to insert by bulk statements to improve performances.
        w.bulk_insert_rels(refs, 50);
    }   
}

void bulkUpsertObjs(
    oes::eml::etp12::MultiPartReply<PutDataObjectsResponse>& reply,
    SpaceWriter& w,
    OpenKVTransaction::OrphanAryMap& orphan_arys,
    OpenKVTransaction::MissingAryMap& missing_arys,
    const std::vector<
        const std::map<std::string, DataObject>::value_type*
    >& entries,
    TransactionActControl* trans_ctl_ptr,
    const size_t bulk_size
) {

    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "upsertObjs",
        {{"entries", std::to_string(entries.size())}});

    // Data collection structures for bulk operations
    struct ResInsertData {
        int typ_id;
        oes::common::epc::xml::AbstractCitedDataObject cited;
        std::string xdata_json;
        std::vector<uint8_t> xdata_avro;
    };
    struct ObjInsertData {
        eml_int64 typ_id;
        oes::core::Guid uid;
        std::string xml;
    };
    struct EntryProcessing {
        std::string cid;
        eml_int64 old_id;
        ResInsertData res_data;
        ObjInsertData obj_data;
        FilePart::ScanData scan;
        std::string partIdXml;
    };

    std::vector<EntryProcessing> processed_entries;
    std::vector<ObjRef> refs;
    
    // PHASE 1: Parse all data and collect metadata
    struct ParsedEntry {
        std::string cid;
        std::string xmlns_uri;
        std::string type_name;
        oes::common::epc::xml::AbstractCitedDataObject cited;
        oes::core::Guid uid;
        std::string xdata_json;
        std::vector<uint8_t> xdata_avro;
        std::string partIdXml;
        FilePart::ScanData scan;
    };
    std::vector<ParsedEntry> parsed_entries;
    parsed_entries.reserve(entries.size());
    
    for (const auto& entry : entries) {
        const std::string& cid = entry->first;
        const DataObject& obj = entry->second;

        std::string partIdXml(obj.m_data.begin(), obj.m_data.end());


        auto& pe = parsed_entries.emplace_back(ParsedEntry{});
        FilePart::ScanRc scan_rc;
        // FilePart::ScanData scan;
        std::tie(pe.scan, scan_rc) = FilePart::scanData(
            partIdXml,
            FilePart::eCited |
            FilePart::eRefs  |
            FilePart::eHdfs
        );

        if (scan_rc != FilePart::eOK_DONE) {
            parsed_entries.pop_back();
            reply.addError(cid, EINVALID_OBJECT, "Cannot parse XML");
            continue;
        }

        Guid uid = Guid::parse(pe.scan.cited_.uuid_);
        if (!uid.isValid()) {
            parsed_entries.pop_back();
            reply.addError(cid, EINVALID_OBJECT, "Invalid UUID");
            continue;
        }

        std::string type_name = pe.scan.name_.local_name_;
        if(!pe.scan.name_.xsi_type_.empty()){
            type_name = pe.scan.name_.xsi_type_.substr(pe.scan.name_.xsi_type_.find(':')+1);
        }

        auto [xdata_json, xdata_avro] = oes::eml::etp12::xdataToJsonAndAvroBinary(
            obj.m_resource.m_customData
        );

        pe.cid = std::move(cid);
        pe.xmlns_uri = pe.scan.name_.xmlns_uri_;
        pe.type_name = std::move(type_name);
        pe.cited = pe.scan.cited_;
        pe.uid = std::move(uid);
        pe.xdata_json = std::move(xdata_json);
        pe.xdata_avro = std::move(xdata_avro);
        pe.partIdXml = std::move(partIdXml);
    }

    if (parsed_entries.empty()) {
        return;
    }

    // PHASE 1b: Bulk resolve URIs and types
    // Map unique (xmlns_uri, schema_version) -> uri_id
    std::map<std::pair<std::string, std::string>, eml_int64> uri_cache;
    for (const auto& parsed : parsed_entries) {
        auto key = std::make_pair(parsed.xmlns_uri, parsed.scan.name_.schema_version_);
        if (uri_cache.find(key) == uri_cache.end()) {
            eml_int64 uri_id = w.uri_of(parsed.xmlns_uri, {}, {}, parsed.scan.name_.schema_version_);
            uri_cache[key] = uri_id;
        }
    }

    // Map unique (uri_id, type_name) -> typ_id
    std::map<std::pair<eml_int64, std::string>, int> typ_cache;
    for (const auto& parsed : parsed_entries) {
        auto uri_key = std::make_pair(parsed.xmlns_uri, parsed.scan.name_.schema_version_);
        eml_int64 uri_id = uri_cache[uri_key];
        auto typ_key = std::make_pair(uri_id, parsed.type_name);
        if (typ_cache.find(typ_key) == typ_cache.end()) {
            int typ_id = w.typ_of(uri_id, parsed.type_name);
            typ_cache[typ_key] = typ_id;
        }
    }

    // Same (typ_id, uid) more than once: keep the last in cid order, like successive upserts in upsertObjs()
    std::vector<std::string> superseded_cids;
    {
        auto res_key_of = [&uri_cache, &typ_cache](const ParsedEntry& p) {
            const eml_int64 uri_id = uri_cache.at({ p.xmlns_uri, p.scan.name_.schema_version_ });
            return ResKey{ typ_cache.at({ uri_id, p.type_name }), p.uid };
        };

        std::map<ResKey, size_t> last_of;
        for (size_t i = 0; i < parsed_entries.size(); ++i) {
            last_of[res_key_of(parsed_entries[i])] = i;
        }

        if (last_of.size() < parsed_entries.size()) {
            std::vector<ParsedEntry> kept;
            kept.reserve(last_of.size());
            for (size_t i = 0; i < parsed_entries.size(); ++i) {
                const size_t last = last_of.at(res_key_of(parsed_entries[i]));
                if (last == i) {
                    kept.push_back(std::move(parsed_entries[i]));
                } else {
                    OES_DEBUG(
                        "PutDataObjects: cid {} superseded by cid {} (same object {})",
                        parsed_entries[i].cid, parsed_entries[last].cid, parsed_entries[i].uid.toXmlString()
                    );
                    superseded_cids.push_back(std::move(parsed_entries[i].cid));
                }
            }
            parsed_entries = std::move(kept);
        }
    }

    // PHASE 1c: Bulk query old object IDs
    std::vector<int> all_types; all_types.reserve(parsed_entries.size());
    std::vector<oes::core::Guid> all_guids; all_guids.reserve(parsed_entries.size());
    std::vector<int> all_indexes; all_indexes.reserve(parsed_entries.size());
    int i_idx{0};

    for (const auto& parsed : parsed_entries) {
        auto uri_key = std::make_pair(parsed.xmlns_uri, parsed.scan.name_.schema_version_);
        eml_int64 uri_id = uri_cache[uri_key];
        auto typ_key = std::make_pair(uri_id, parsed.type_name);
        int typ_id = typ_cache[typ_key];

        all_types.push_back(typ_id);
        all_guids.push_back(parsed.uid);
        all_indexes.push_back(i_idx++);
    }

    auto bulk_query_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__ + ".bulkSelectObjId",
        {{"count", std::to_string(all_guids.size())}});
    
    auto old_ids_map = w.bulk_select_obj_ids(all_types, all_guids, all_indexes, bulk_size);

    // PHASE 1d: Bulk null old guids that need updating
    std::vector<int> typs_to_null; typs_to_null.reserve(parsed_entries.size());
    std::vector<oes::core::Guid> guids_to_null; guids_to_null.reserve(parsed_entries.size());
    std::vector<int> indexes_to_null; indexes_to_null.reserve(parsed_entries.size());
    
    for (size_t i = 0; i < parsed_entries.size(); ++i) {
        if (old_ids_map.at(i) >= 0) {
            auto uri_key = std::make_pair(parsed_entries.at(i).xmlns_uri, parsed_entries.at(i).scan.name_.schema_version_);
            eml_int64 uri_id = uri_cache[uri_key];
            auto typ_key = std::make_pair(uri_id, parsed_entries.at(i).type_name);
            int typ_id = typ_cache[typ_key];

            guids_to_null.push_back(parsed_entries.at(i).uid);
            typs_to_null.push_back(typ_id);
            indexes_to_null.push_back(i);
        } else {
            // This is a new object
            if (trans_ctl_ptr) {
                const auto& parsed = parsed_entries.at(i);
                trans_ctl_ptr->new_objs_uuids_.insert({etp_type_of(parsed.scan), parsed.uid});
            }
        }
    }

    if (!guids_to_null.empty()) {
        auto bulk_null_span = oes::core::OtelSpan(
            std::string("SpaceWriter.") + __func__ + ".bulkNullResGuid",
            {{"count", std::to_string(guids_to_null.size())}});
        w.bulk_null_res_guids(typs_to_null, guids_to_null, indexes_to_null, bulk_size);
    }

    // PHASE 1e: Build final processed_entries with resolved IDs
    processed_entries.reserve(parsed_entries.size());
    for (size_t i = 0; i < parsed_entries.size(); ++i) {
        auto& parsed = parsed_entries.at(i);
        
        ResInsertData res;
        ObjInsertData obj;
        auto uri_key = std::make_pair(parsed.xmlns_uri, parsed.scan.name_.schema_version_);
        eml_int64 uri_id = uri_cache.at(uri_key);
        auto typ_key = std::make_pair(uri_id, parsed.type_name);
        // int typ_id = typ_cache.at(typ_key);

        res.typ_id = typ_cache.at(typ_key);
        res.cited = std::move(parsed.cited);
        res.xdata_json = std::move(parsed.xdata_json);
        res.xdata_avro = std::move(parsed.xdata_avro);

        obj.typ_id = res.typ_id;
        obj.uid = parsed.uid;
        obj.xml = parsed.partIdXml;

        auto& entry_proc = processed_entries.emplace_back(EntryProcessing{});
        entry_proc.cid = parsed.cid;
        entry_proc.old_id = old_ids_map.at(i);
        entry_proc.res_data = std::move(res);
        entry_proc.obj_data = std::move(obj);
        entry_proc.scan = std::move(parsed.scan);
        entry_proc.partIdXml = parsed.partIdXml;

        // processed_entries.emplace_back(std::move(entry_proc));
    }

    // PHASE 2: Collect all resource data for bulk insert
    std::vector<ResData> res_batch; res_batch.reserve(processed_entries.size());
    std::vector<eml_int64> old_ids; old_ids.reserve(processed_entries.size());
    std::vector<size_t> index_map; index_map.reserve(processed_entries.size());
    
    for (size_t i{0}; i < processed_entries.size(); i++) {
        auto& entry_proc = processed_entries.at(i);
        ResData data;
        
        data.typ_id = entry_proc.res_data.typ_id;
        data.cited = std::move(entry_proc.res_data.cited);
        data.xdata_json = std::move(entry_proc.res_data.xdata_json);
        data.xdata_avro = std::move(entry_proc.res_data.xdata_avro);

        res_batch.push_back(data);
        old_ids.push_back(entry_proc.old_id);
        index_map.push_back(i);
    }

    // PHASE 2b: Bulk insert all resources at once
    auto res_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__ + ".bulkInsertRes",
        {{"count", std::to_string(res_batch.size())}});
    
    // Passing res_batch as rvalue
    std::vector<eml_int64> obj_ids(old_ids.size(), -1);
    {
        auto obj_ids_map = w.bulk_insert_res(std::move(res_batch), index_map, bulk_size);
        for(auto im : index_map) {
            obj_ids.at(im) = obj_ids_map.at(im);
        }
        res_batch.clear(); // explicitly clear the array!
    }
    
    w.bulk_update_dst_id(old_ids, obj_ids, bulk_size);

    // PHASE 3: Collect all uncompressed object data for bulk insert
    std::vector<eml_int64> typ_ids; typ_ids.reserve(processed_entries.size());
    std::vector<oes::core::Guid> guids; guids.reserve(processed_entries.size());
    std::vector<std::string> xmls; xmls.reserve(processed_entries.size());
    
    for (size_t i = 0; i < processed_entries.size(); ++i) {
        const auto& entry = processed_entries.at(i);

        typ_ids.push_back(entry.obj_data.typ_id);
        guids.push_back(entry.obj_data.uid);
        xmls.push_back(std::move(entry.obj_data.xml));
    }

    // PHASE 3b: Bulk insert all uncompressed objects at once
    auto uncomp_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__ + ".bulkInsertUncompressedObj",
        {{"count", std::to_string(typ_ids.size())}});
    
    w.bulk_insert_uncompressed_objs(
        obj_ids,
        xmls,
        all_indexes,
        bulk_size
    );

    // PHASE 3c: insert rels
    w.bulk_insert_missing_refs(
        obj_ids,
        typ_ids,
        guids,
        bulk_size // change to 50
    );

    // PHASE 4: Collect and bulk insert all references
    for (size_t i = 0; i < processed_entries.size(); ++i) {
        auto& entry_proc = processed_entries.at(i);
        eml_int64 obj_id = obj_ids.at(i);

        // Convert extracted XML refs, to OpenKV refs
        for (const auto& xml_ref : entry_proc.scan.refs_) {
            // the ref's target is not resolved now, but in .insert_rel() later below
            refs.emplace_back(w.to_obj_ref(obj_id, xml_ref.first, xml_ref.second));
        }
    }

    // PRASE 5a: Select arrays to check orphan
    std::vector<oes::core::Guid> epc_guids;
    std::vector<std::string> paths;
    std::vector<int> indexes;
    std::vector<std::string> xpaths;
    std::vector<int> xpath_indexes;
    for (size_t i = 0; i < processed_entries.size(); i++){
        auto& entry_proc = processed_entries.at(i);
        //?const eml_int64 obj_id = obj_ids.at(i);

        for (auto& xml_hdf : entry_proc.scan.hdfs_) {
            std::string ary_path = xml_hdf.second.PathInHdfFile_;
            if (ary_path[0] == '/') {
                ary_path.erase(0, 1);
            }

            epc_guids.push_back(hdf_proxy_guid(xml_hdf.second, entry_proc.obj_data.uid));
            paths.push_back(ary_path);
            indexes.push_back(static_cast<int>(i));
            xpath_indexes.push_back(static_cast<int>(xpaths.size()));
            xpaths.push_back(xml_hdf.first);
        }
    }
    // Key by array (container, path) (UNIQUE in the ary table), NOT by object index:
    // a single object may reference several arrays, all sharing the same
    // object index, and keying by that index would drop every array but the
    // first. Any array left un-relinked keeps pointing at the old (about to be
    // deleted) object row and is cascade-deleted at commit.
    std::map<AryKey, SqlAry> arys_map;
    {
        auto arys = w.bulk_get_ary(
            epc_guids, paths, indexes, bulk_size
        );
        for (auto& a: arys) {
            arys_map.emplace(AryKey{ a.epc_guid_, a.path_ }, a);
        }
    }
    const auto xpa_info = w.xpa_of(
        xpaths, xpath_indexes, bulk_size
    );

    // PHASE 5: Collect and bulk update all array IDs
    std::vector<eml_int64> ary_ids_to_update;
    std::vector<eml_int64> obj_ids_to_update;
    std::vector<eml_int64> xpa_ids_to_update;
    int xpath_idx{0}; // same traversal order as xpath_indexes above
    for (size_t i = 0; i < processed_entries.size(); i++) {
        auto& entry_proc = processed_entries.at(i);
        eml_int64 obj_id = obj_ids.at(i);

        // Convert extracted XML HDF5 array references, to OpenKV hdfs
        for (auto& xml_hdf : entry_proc.scan.hdfs_) {
            const eml_int64 xpa_id = xpa_info.at(xpath_idx++);

            std::string ary_path = xml_hdf.second.PathInHdfFile_;
            if (ary_path[0] == '/') {
                ary_path.erase(0, 1);
            }
            AryKey ary_key{ hdf_proxy_guid(xml_hdf.second, entry_proc.obj_data.uid), std::move(ary_path) };

            // If array already exists, re-link it to the (possibly new) object
            // and drop it from the orphan set. Look it up by its unique (container, path).
            if (auto found = arys_map.find(ary_key);
                found != arys_map.end() && found->second.valid()) {
                const SqlAry& sql_ary = found->second;
                ary_ids_to_update.push_back(sql_ary.ary_id_);
                obj_ids_to_update.push_back(obj_id);
                xpa_ids_to_update.push_back(xpa_id);

                auto ary_found = orphan_arys.find(ary_key);
                if (ary_found != orphan_arys.end()) {
                    orphan_arys.erase(ary_found); // no longer an orphan
                }
                continue;
            }

            // Array not yet there
            OpenKVTransaction::MissingAry miss_ary{ -1, obj_id, xpa_id };
            assert(!ary_key.second.empty());
            auto add_ok = missing_arys.emplace(std::move(ary_key), miss_ary);
            if (!add_ok.second) {
                // multiple objects referencing the same array. Not normal
            }
        }
    }

    // PHASE 6: Process reply and activities
    for (const auto& cid : superseded_cids) {
        reply.addMapPart(cid);
    }
    for (size_t i = 0; i < processed_entries.size(); ++i) {
        auto& entry_proc = processed_entries[i];
        // No need to fill in details about changes in ContainedObjects, as they are currently not supported
        reply.addMapPart(entry_proc.cid);
        
        if (trans_ctl_ptr) {
            process_activities(
                w,
                entry_proc.scan,
                entry_proc.partIdXml,
                *trans_ctl_ptr
            );
        }
    }

    // PHASE 7: Bulk update all array IDs at once
    if (!ary_ids_to_update.empty()) {
        auto ary_span = oes::core::OtelSpan(
            std::string("SpaceWriter.") + __func__ + ".bulkUpdateAryIds",
            {{"count", std::to_string(ary_ids_to_update.size())}});
        
        w.bulk_update_ary_ids(
            obj_ids_to_update,
            xpa_ids_to_update,
            ary_ids_to_update,
            bulk_size
        );
    }

    // PHASE 8: Always use bulk insert for relations (no conditional logic)
    if (!refs.empty()) {
        // cppcheck-suppress unreadVariable
        auto rels_span = oes::core::OtelSpan(
            std::string("SpaceWriter.") + __func__ + ".bulkInsertRels",
            {{"count", std::to_string(refs.size())}});
        w.bulk_insert_rels(refs, bulk_size);
    }
}

//============================================================================
eml_int64 ml_id(SpaceWriter& w, EmlUri::ML ml){
    const std::string RSQ("http://www.energistics.org/energyml/data/resqmlv2");
    const std::string WIT("http://www.energistics.org/energyml/data/witsmlv2");
    const std::string PRD("http://www.energistics.org/energyml/data/prodmlv2");
    const std::string EML("http://www.energistics.org/energyml/data/commonv2");

    switch (ml) {
        case EmlUri::eEML20: return w.uri_of(EML, "eml", "eml20", "2.0"); break;
        case EmlUri::eEML23: return w.uri_of(EML, "eml23", "eml23", "2.3"); break;
        case EmlUri::eRESQML20: return w.uri_of(RSQ, "rsq", "resqml20", "2.0"); break;
        case EmlUri::eRESQML22: return w.uri_of(RSQ, "rsq22", "resqml22", "2.2"); break;
        case EmlUri::eWITSML21: return w.uri_of(WIT, "wit21", "witsml21", "2.1"); break;
        case EmlUri::ePRODML22: return w.uri_of(PRD, "prd22", "prodml22", "2.2"); break;
        case EmlUri::ePRODML23: return w.uri_of(PRD, "prd23", "prodml23", "2.3"); break;
        default: return w.uri_of(RSQ, "rsq", "resqml20", "2.0"); break;
    }
}

} // namespace

namespace oes::eml::openkv {

//============================================================================

OpenKVStore::OpenKVStore(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

using cid_uri_typeIndex_tuple_t = std::tuple<std::string&, EmlUri&, unsigned&>;
using c_cid_uri_typeIndex_tuple_t = std::tuple<const std::string&, const EmlUri&, const unsigned&>;
using typeIndexes_guids_indexes_tuple_t = std::tuple<
    std::vector<unsigned>,
    std::vector<oes::core::Guid>,
    std::vector<int>
>;
struct ProcessBulkObjectHelper {
private:
    std::vector<std::string> client_ids_;
    std::vector<EmlUri> uris_;
    std::vector<unsigned> type_indexes_;

public:
    ProcessBulkObjectHelper() = default;

    size_t size() const {return type_indexes_.size();}

    cid_uri_typeIndex_tuple_t emplace_back(const std::string& cid, const EmlUri& uri, unsigned type_index);
    cid_uri_typeIndex_tuple_t at(size_t index);
    c_cid_uri_typeIndex_tuple_t c_at(size_t index) const;

    std::vector<std::string>& client_ids() { return client_ids_; }
    std::vector<EmlUri>& uris() { return uris_; }
    std::vector<unsigned>& type() { return type_indexes_; }

    typeIndexes_guids_indexes_tuple_t get_typeId_guid_vectors() const;
};

cid_uri_typeIndex_tuple_t ProcessBulkObjectHelper::emplace_back(const std::string& cid, const EmlUri& uri, unsigned type_index) {
    return std::tie(
        client_ids_.emplace_back(cid),
        uris_.emplace_back(uri),
        type_indexes_.emplace_back(type_index)
    );
}

cid_uri_typeIndex_tuple_t ProcessBulkObjectHelper::at(size_t index) {
    if (index < type_indexes_.size()) {
        return std::tie(client_ids_.at(index), uris_.at(index), type_indexes_.at(index));
    }

    throw std::runtime_error("ProcessBulkObjectHelper index out of scope");
}

c_cid_uri_typeIndex_tuple_t ProcessBulkObjectHelper::c_at(size_t index) const {
    // returns guid, type_index, client_id, uri_str
    if (index < type_indexes_.size()) {
        return std::tie(client_ids_.at(index), uris_.at(index), type_indexes_.at(index));
    }

    throw std::runtime_error("ProcessBulkObjectHelper index out of scope");
}

typeIndexes_guids_indexes_tuple_t ProcessBulkObjectHelper::get_typeId_guid_vectors() const {
    using tuple_t = std::tuple<unsigned, oes::core::Guid, int>;
    std::vector<tuple_t> vec;
    for (int i {0}; i < static_cast<int>(size()); ++i) {
        vec.push_back(std::make_tuple(type_indexes_.at(i), uris_.at(i).guid_, i));
    }

    std::sort(vec.begin(), vec.end(), [](const auto& lhs, const auto& rhs){
        return
            std::tie(std::get<0>(lhs), std::get<1>(lhs)) <
            std::tie(std::get<0>(rhs), std::get<1>(rhs));
    });

    std::vector<unsigned> type_ids; type_ids.reserve(vec.size());
    std::vector<oes::core::Guid> guids; guids.reserve(vec.size());
    std::vector<int> indexes; indexes.reserve(vec.size());

    for (size_t i {0}; i < vec.size(); ++i) {
        type_ids.push_back(std::get<0>(vec.at(i)));
        guids.push_back(std::get<1>(vec.at(i)));
        indexes.push_back(std::get<2>(vec.at(i)));
    }

    return std::make_tuple(type_ids, guids, indexes);
}

void OpenKVStore::GetDataObjects( // messageTypeId 1
    IRequest& ireq, GetDataObjects_req_t req,
    EtpResponseHandler<GetDataObjects_res_t> process_cb // messageTypeId 4
) try {
    char * use_bulk_p = std::getenv("USE_BULK_READ");
    std::string use_bulk;
    
    if (use_bulk_p) {
        use_bulk = std::string(use_bulk_p);
    } else {
        use_bulk = "true";
    }
    if (use_bulk == "true") {
        return GetDataObjectsBULK(ireq, req, process_cb);
    }

    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("OpenKVStore." + std::string(__func__));
     auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setAttr("thread_id", oes::core::thisThreadId());
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.", oes::core::thisThreadId());
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_GET_ERROR);
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_uris.empty()) {
        span.setError("Empty URI list");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_GET_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    size_t nbObjects = req.m_uris.size();

    // Assume XML if empty. E.g. DX 1.2 client uses ""
    if (!req.m_format.empty() && req.m_format != "xml") {
        span.setError("Only XML supported");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_GET_ERROR);
        return reply.fatalError(ENOTSUPPORTED, "Only XML supported");
    }

    // Sort URIs, so that same ETP-space, and same-type URIs are collocated.
    // URIs are sent as a map(id, uri) in recent ETP 1.2 draft, no longer a vector
    using MLType = std::pair<EmlUri::ML, std::string>;
    using TypeIdx = unsigned; // idx into type_vec below
    std::map<MLType, TypeIdx> type2idx;
    std::vector<std::string> type_vec;
    type_vec.reserve(64);
    auto lookupTypeIdx = [&](const EmlUri& uri) {
        MLType key{ uri.ml_, uri.obj_type_ };
        auto found = type2idx.find(key);
        if (found != type2idx.end()) {
            return TypeIdx(found->second);
        }
        if(uri.obj_type_.compare(0, 4, "obj_") == 0){
            type_vec.emplace_back(uri.obj_type_.substr(4)); // drop obj_ prefix
        }else{
            type_vec.emplace_back(uri.obj_type_);
        }
        TypeIdx idx = TypeIdx(type_vec.size() - 1);
        type2idx.emplace(key, idx);
        return idx;
    };
    auto reverseLookupTypeIdx = [&type2idx](TypeIdx type_idx) {
        for (const auto& entry : type2idx) {
            if (entry.second == type_idx) {
                return entry.first;
            }
        }
        return MLType();
    };

    using StrPair = std::pair<const std::string, std::string>;
    using ObjNK = std::tuple<TypeIdx, Guid, const StrPair*>;

    std::map<std::string, std::vector<ObjNK>> init_space_nks;

    for (const auto& entry : req.m_uris) {
        const std::string& cid = entry.first; // client ID
        const std::string& uri_str = entry.second; // ETP URI

        EmlUri uri = EmlUri::parse(uri_str);
        if (!uri.isObj()) {
            reply.addError(cid, EINVALID_URI, "Expected eOBJ_INSTANCE URI");
            continue;
        }

        TypeIdx idx = lookupTypeIdx(uri);
        init_space_nks[uri.dataspace_].emplace_back(idx, uri.guid_, &entry);
    }

    std::map<std::string, std::vector<ObjNK>> per_space_nks;
    std::map<std::string, std::string> initial_uris;
    for (auto& entry : init_space_nks) {
        std::vector<ObjNK>& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()) {
            for (const ObjNK& nk : nks) {
                const StrPair* id2uri = std::get<2>(nk);
                reply.addError(id2uri->first, ENOT_FOUND,fmt::format("Space {} not found", entry.first));
                nbObjects --;
            }
            continue;
        }

        ErrorInfo err;
        // The dataspace that matters is the one of the initial URI
        if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const ObjNK& nk : nks) {
                const StrPair* id2uri = std::get<2>(nk);
                reply.addError(id2uri->first, err);
                nbObjects --;
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const ObjNK& nk : nks) {
                const StrPair* id2uri = std::get<2>(nk);
                reply.addError(id2uri->first, err);
                nbObjects --;
            }
            continue;
        }

        // Check that there is at least one row in admin.space_rel that has the dataspace as origin
        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            std::vector<TypedUid> ids;
            for(const ObjNK& nk : nks){
                std::string i_uri= std::get<2>(nk)->second;
                EmlUri uri = EmlUri::parse(i_uri);
                if(uri.isObj() && uri.guid_.isValid()){
                    ids.emplace_back(stored_uid(uri));
                }
            }
            const std::map<TypedUid, eml_int64> ml_ids = reader.containing_dataspace_ids(ids);
            std::map<eml_int64,std::string> id2ds;
            for(const ObjNK& nk : nks){
                std::string i_uri= std::get<2>(nk)->second;
                EmlUri uri = EmlUri::parse(i_uri);
                const auto ml_id = ml_ids.find(stored_uid(uri));
                if(ml_id != ml_ids.end() && ml_id->second!=0){
                    eml_int64 rel_id = ml_id->second;
                    if(id2ds.find(rel_id) == id2ds.end()){
                        id2ds[rel_id] = reader.get_destination_dataspace(rel_id);
                    }
                    const std::string id = std::get<2>(nk)->first;
                    initial_uris[id] = i_uri;
                    uri.dataspace_ = id2ds[ml_id->second];
                    const_cast<std::string&>(std::get<2>(nk)->second) = uri.to_string();
                }
                per_space_nks[uri.dataspace_].emplace_back(nk);
            }
        }else{
            per_space_nks[space.uid_] = std::move(nks);
        }
    }

    auto processType = [&reply, &type_vec, &reverseLookupTypeIdx, &nbObjects, &initial_uris](
        Connection_ptr spacedb, const std::string& dataspace, TypeIdx type_idx,
        const ObjNK* begin, const ObjNK* end
    ) {
        const std::string& type = type_vec[type_idx];
        OES_UNUSED(type); // FIXME
        MLType ml_type = reverseLookupTypeIdx(type_idx);
        const auto type_uri = EmlUri::make_type(dataspace, ml_type.second, ml_type.first);
        const std::string etp_type = type_uri.to_canonical_type();

        std::string sql = 
        R"sql(
select xml, name, createdBy,
       coalesce(createdAt, 0),
       coalesce(updatedAt, 0),
       (EXTRACT(EPOCH FROM (storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       space_rel_id
  from res
  join obj on obj.id = res.obj_id
 where typ_id =
       (
        select typ.id
          from typ
          join uri on uri.id = typ.uri_id
         where uri.ml || '.' || typ.xml = $1
       )
   and guid = $2

    )sql";

        boost::trim(sql);
        oes::postgresql::Statement stmt = spacedb->prepare(std::move(sql));

        unsigned count = 0;
        for (auto it = begin; it < end; ++it, ++count) {
            nbObjects --;

            const ObjNK& nk = *it;
            const Guid& uid = std::get<1>(nk);
            const std::string& cid = std::get<2>(nk)->first;

            stmt.bind(1, etp_type);
            stmt.bind(2, uid);

            if (!stmt.step()) {
                reply.addError(cid, ENOT_FOUND, "Object not Found");
                continue;
            }

            const std::string xml = stmt.getString(0);

            Resource res;

            size_t resource_bytes = 0;

            if(initial_uris.find(cid) != initial_uris.end()){
                res.m_uri = initial_uris[cid];
            }else{
                res.m_uri = type_uri.child_obj(uid).to_string();
            }
            resource_bytes += res.m_uri.length()+8;
            res.m_name = stmt.getString(1);
            resource_bytes += res.m_name.length()+8;

            std::string creator = stmt.getString(2);
            resource_bytes += creator.length()+16;
            etp12::xdataEmplace(res.m_customData, "creator", std::move(creator));

            eml_int64 createdAt = stmt.getInt64(3); // Never null, might be 0 though.
            eml_int64 updatedAt = stmt.getInt64(4); // Micro-seconds UTC since Epoch.
            res.m_lastChanged = std::max<>(createdAt, updatedAt);
            resource_bytes += 8;

            const std::string created = std::to_string(createdAt);
            etp12::xdataEmplace(res.m_customData, "created", created);
            resource_bytes += created.length()+16;


            eml_int64 storeCreated = stmt.getInt64(5);
            eml_int64 storeUpdated = stmt.getInt64(6);
            res.m_storeCreated = storeCreated;
            res.m_storeLastWrite = storeUpdated;
            resource_bytes += 16;

            eml_int64 space_rel_id = stmt.getInt64(7);
            if(space_rel_id > 0){
                etp12::xdataEmplace(res.m_customData, "reference", "true");
                resource_bytes += 22;
            }

            if(resource_bytes + xml.length() > reply.max_bytes_) {
                reply.maybeProcess(resource_bytes + 16);
                // Cannot fit in one message, need to Use chunks

                // Create dataobject with blob uuid
                auto it_obj = reply.addMapPart(cid);
                DataObject& obj = it_obj.first->second;
                obj.m_resource = std::move(res);
                oes::common::etp12::Datatypes::Uuid uuid;
                oes::common::assignUid(uuid, Guid::make());
                obj.m_blobId = uuid;
                obj.m_format = "xml"; // should come from DB itself, eventually

                // Create chunks for the uuid
                std::vector<uint8_t> chunks(xml.begin(), xml.end());
                reply.addChunks(uuid, std::move(chunks));
                // force the send of current object
                reply.maybeProcess(resource_bytes + 16, true);
            }else{
                reply.maybeProcess(resource_bytes + xml.length() + 8);
                auto it_obj = reply.addMapPart(cid);
                DataObject& obj = it_obj.first->second;
                obj.m_resource = std::move(res);
                obj.m_data.assign(xml.begin(), xml.end()); // FIXME: Avoid copy
                obj.m_format = "xml"; // should come from DB itself, eventually
            }

            stmt.reset();

        }
    };

    for (auto& entry : per_space_nks) {
        std::vector<ObjNK>& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const ObjNK& nk : nks) {
                const StrPair* id2uri = std::get<2>(nk);
                reply.addError(id2uri->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
                nbObjects --;
            }
            continue;
        }


        ErrorInfo err;
        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const ObjNK& nk : nks) {
                const StrPair* id2uri = std::get<2>(nk);
                reply.addError(id2uri->first, err);
                nbObjects --;
            }
            continue;
        }
        // Sort by type, then guid (to match future part's table index,
        // for better page-cache-locality)
        std::sort(nks.begin(), nks.end(), [](const ObjNK& lhs, const ObjNK& rhs) {
            return
                std::tie(std::get<0>(lhs), std::get<1>(lhs)) <
                std::tie(std::get<0>(rhs), std::get<1>(rhs))
            ;
        });

        const ObjNK* nk_begin = &nks[0];
        const ObjNK* nk_end = nk_begin;
        for (const ObjNK& nk : nks) {
            const TypeIdx prev_idx = std::get<0>(*nk_begin);
            const TypeIdx curr_idx = std::get<0>(nk);
            if (prev_idx != curr_idx) {
                nk_end = &nk;
                processType(spacedb, space.uid_, prev_idx, nk_begin, nk_end);
                nk_begin = nk_end;
            }
        }
        if (nk_begin <= &nks.back()) {
            const TypeIdx type_idx = std::get<0>(*nk_begin);
            processType(spacedb, space.uid_, type_idx, nk_begin, &nks.back() + 1);
        }
    }

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::STORE_OPS_COUNTER_GET_SUCCESS,oes::core::metrics::STORE_LATENCY_GET,start);

    reply.finalProcess();
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_GET_ERROR);
    etp12::handleException(process_cb);
}


void OpenKVStore::GetDataObjectsBULK( // messageTypeId 1
    IRequest& ireq, GetDataObjects_req_t req,
    EtpResponseHandler<GetDataObjects_res_t> process_cb // messageTypeId 4
) try {

    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("OpenKVStore." + std::string(__func__));
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setAttr("thread_id", oes::core::thisThreadId());
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.", oes::core::thisThreadId());
        return;
    }

    const size_t BULK_SIZE = session->pg_bulk_size_;

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_uris.empty()) {
        span.setError("Empty URI list");
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    size_t nbObjects = req.m_uris.size();

    // Assume XML if empty. E.g. DX 1.2 client uses ""
    if (!req.m_format.empty() && req.m_format != "xml") {
        span.setError("Only XML supported");
        return reply.fatalError(ENOTSUPPORTED, "Only XML supported");
    }

    // Sort URIs, so that same ETP-space, and same-type URIs are collocated.
    // URIs are sent as a map(id, uri) in recent ETP 1.2 draft, no longer a vector
    using MLType = std::pair<EmlUri::ML, std::string>;
    using TypeIdx = unsigned; // idx into type_vec below
    std::map<MLType, TypeIdx> type2idx;
    std::vector<std::string> type_vec;
    type_vec.reserve(64);
    auto lookupTypeIdx = [&](const EmlUri& uri) {
        MLType key{ uri.ml_, uri.obj_type_ };
        auto found = type2idx.find(key);
        if (found != type2idx.end()) {
            return TypeIdx(found->second);
        }
        if(uri.obj_type_.compare(0, 4, "obj_") == 0){
            type_vec.emplace_back(uri.obj_type_.substr(4)); // drop obj_ prefix
        }else{
            type_vec.emplace_back(uri.obj_type_);
        }
        TypeIdx idx = TypeIdx(type_vec.size() - 1);
        type2idx.emplace(key, idx);
        return idx;
    };
    auto reverseLookupTypeIdx = [&type2idx](TypeIdx type_idx) {
        for (const auto& entry : type2idx) {
            if (entry.second == type_idx) {
                return entry.first;
            }
        }
        return MLType();
    };

    std::map<std::string, ProcessBulkObjectHelper> init_per_space_map;
    for (const auto& entry : req.m_uris ) {
        const std::string& cid = entry.first; // client ID
        const std::string& uri_str = entry.second; // ETP URI

        EmlUri uri = EmlUri::parse(uri_str);
        if (!uri.isObj()) {
            reply.addError(cid, EINVALID_URI, "Expected eOBJ_INSTANCE URI");
            continue;
        }

        TypeIdx idx = lookupTypeIdx(uri);

        const auto& [it, /*ok*/ _] = init_per_space_map.try_emplace(
            uri.dataspace_, ProcessBulkObjectHelper());
        auto& [ds, gti] = *it;

        gti.emplace_back(cid, uri, idx);

    }

    std::map<std::string, ProcessBulkObjectHelper> per_space_map;
    std::map<std::string, std::string> ref_cid_uri_map; // cid => uri_str
    for (auto& entry : init_per_space_map) {
        const auto& dataspace_str = entry.first;
        auto& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()) {
            for (const auto& cid : nks.client_ids()) {
                reply.addError(cid, ENOT_FOUND,fmt::format("Space {} not found", dataspace_str));
                nbObjects --;
            }
            continue;
        }

        ErrorInfo err;
        // The dataspace that matters is the one of the initial URI
        if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const auto& cid : nks.client_ids()) {
                reply.addError(cid, err);
                nbObjects --;
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const auto& cid : nks.client_ids()) {
                // const StrPair* id2uri = std::get<2>(nk);
                reply.addError(cid, err);
                nbObjects --;
            }
            continue;
        }

        // get references!
        // Check that there is at least one row in admin.space_rel that has the dataspace as origin
        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            
            // check if asked abjects belongs to another dataspace
            std::vector<TypedUid> ids;
            for(const auto& uri : nks.uris()){
                if(uri.isObj() && uri.guid_.isValid()){
                    ids.emplace_back(stored_uid(uri));
                }
            }

            const std::map<TypedUid, eml_int64> ml_ids = reader.containing_dataspace_ids(ids); //TODO optimize it?
            std::map<eml_int64,std::string> id2ds;
            
            for (size_t i{0}; i < nks.size(); ++i) {
                auto tup = nks.c_at(i);
                const auto& cid = std::get<0>(tup);
                const auto& dest_uri = std::get<1>(tup);
                const auto& type_index = std::get<2>(tup);

                EmlUri source_uri = EmlUri::parse(dest_uri.to_string());
                const auto ml_id = ml_ids.find(stored_uid(source_uri));

                if(ml_id != ml_ids.end() && ml_id->second != 0){
                    eml_int64 rel_id = ml_id->second;
                    if(id2ds.find(rel_id) == id2ds.end()){
                        id2ds[rel_id] = reader.get_destination_dataspace(rel_id); //optimize???
                    }
                    source_uri.dataspace_ = id2ds[rel_id];
                }
                const auto& [it, _] = per_space_map.try_emplace(source_uri.dataspace_, ProcessBulkObjectHelper());
                auto& [ds, sgti] = *it;
                sgti.emplace_back(cid, source_uri, type_index);

                ref_cid_uri_map.emplace(std::make_pair(cid, dest_uri.to_string()));
            }
        }else{
            per_space_map[space.uid_] = std::move(nks);
        }
    }

    for (auto& entry : per_space_map) { // here
        auto& [space_uri, gti] = entry;

        std::vector<unsigned> type_ids;
        std::vector<oes::core::Guid> guids;
        std::vector<int> indexes;

        std::tie(type_ids, guids, indexes) = gti.get_typeId_guid_vectors();

        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const auto& id : gti.client_ids()) {
                // const StrPair* id2uri = std::get<2>(nk);
                reply.addError(id, ENOT_FOUND, fmt::format("Dataspace not found: {}", space_uri));
                nbObjects --;
            }
            continue;
        }

        ErrorInfo err;
        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const auto& id : gti.client_ids()) {
                reply.addError(id, err);
                nbObjects --;
            }
            continue;
        }

        std::vector<std::string> types; types.reserve(type_ids.size());

        for (auto id : type_ids) {
            MLType ml_type = reverseLookupTypeIdx(id);
            const auto type_uri = EmlUri::make_type(space_uri, ml_type.second, ml_type.first);
            types.push_back(type_uri.to_canonical_type());
        }

        SpaceReader reader(spacedb);
        auto res_arr = reader.bulk_get_obj(guids, types, indexes, BULK_SIZE);

        for (auto& entry : res_arr) {
            Resource res;
            size_t res_bytes = 0;

            auto item = gti.c_at(entry.idx);
            const std::string& cid = std::get<0>(item);
            //?const EmlUri& uri = std::get<1>(item);
            const unsigned& typ_idx = std::get<2>(item);

            if (!entry.found) {
                reply.addError(cid, ENOT_FOUND, "Object not Found");
                continue;
            }

            if (ref_cid_uri_map.find(cid) != ref_cid_uri_map.end()) {
                res.m_uri = ref_cid_uri_map.at(cid);
            } else {
                MLType ml_type = reverseLookupTypeIdx(typ_idx);
                const auto type_uri = EmlUri::make_type(space_uri, ml_type.second, ml_type.first);
                res.m_uri = type_uri.child_obj(entry.guid).to_string();
            }
            res_bytes += res.m_uri.length() + 8;

            res.m_name = std::move(entry.name);
            res_bytes += res.m_name.length() + 8;

            res_bytes += entry.createdBy.length() + 16;
            etp12::xdataEmplace(res.m_customData, "creator", std::move(entry.createdBy));
            
            res.m_lastChanged = std::max<>(entry.createdAt, entry.updatedAt);
            res_bytes += 8;

            const std::string created = std::to_string(entry.createdAt);
            etp12::xdataEmplace(res.m_customData, "created", created);
            res_bytes += 16;

            res.m_storeCreated = entry.storeCreated;
            res.m_storeLastWrite = entry.storeUpdated;
            res_bytes += 16;

            if (entry.space_rel_id > 0) {
                etp12::xdataEmplace(res.m_customData, "reference", "true");
                res_bytes += 22;
            }

            if (res_bytes + entry.xml.length() > reply.max_bytes_) {
                // Cannot fit in one message, need to Use chunks

                // Create dataobject with blob uuid
                auto it_obj = reply.addMapPart(cid);
                DataObject& obj = it_obj.first->second;
                obj.m_resource = std::move(res);
                oes::common::etp12::Datatypes::Uuid uuid;
                oes::common::assignUid(uuid, Guid::make());
                obj.m_blobId = uuid;
                obj.m_format = "xml"; // should come from DB itself, eventually

                // Create chunks for the uuid
                std::vector<uint8_t> chunks(entry.xml.begin(), entry.xml.end());
                reply.addChunks(uuid, std::move(chunks));
                // force the send of current object
                reply.maybeProcess(res_bytes + 16, true);
            } else {
                reply.maybeProcess(res_bytes + entry.xml.length() + 8);
                auto it_obj = reply.addMapPart(cid);
                DataObject& obj = it_obj.first->second;
                obj.m_resource = std::move(res);
                obj.m_data.assign(entry.xml.begin(), entry.xml.end()); // TODO: Avoid copy
                obj.m_format = "xml"; // should come from DB itself, eventually
            }
        }
    }

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

void OpenKVStore::PutDataObjects( // messageTypeId 2
    IRequest& ireq, PutDataObjects_req_t req,
    EtpResponseHandler<PutDataObjects_res_t> process_cb // messageTypeId 9
){
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const int64_t messageId = ireq.header().m_messageId;
    return PutDataObjectsImpl(messageId, req, process_cb);
}

/**
 * @brief Put data objects into the store
 * 
 * Insert object in the store at the end of a single or multi-chunk message.
 * 
 * @param messageId Message ID
 * @param req Request
 * @param process_cb Callback
 * @return void
*/
void OpenKVStore::PutDataObjectsImpl(
    int64_t messageId,
    PutDataObjects_req_t req,
    EtpResponseHandler<PutDataObjects_res_t> process_cb
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "OpenKVStore." + std::string(__func__),
        {{"dataobjects.size", req.m_dataObjects.size()}});
    OpenKVSession* session = session_.lock().get();
    auto start = std::chrono::steady_clock::now();
    if(session == nullptr) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR);
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.",oes::core::thisThreadId());
        return;
    }

    const size_t BULK_SIZE = session->pg_bulk_size_;

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataObjects.empty()) {
        span.setError("Nothing to add");
        
        // Audit log: Empty request is a validation failure
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_OBJECTS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_CREATE,
            "unknown",
            "Empty data objects list"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to add");
    }

    // Check if we're in an ETP transaction (Protocol 18) for audit logging
    bool is_etp_transaction = (session->tx_ != nullptr);
    std::string commit_status = is_etp_transaction ? "COMMIT_PENDING" : "COMMITTED";

    // First, check entries, and sort them by space
    using Entry = std::map<std::string, DataObject>::value_type;
    using Entries = std::vector<const Entry*>;
    std::map<std::string, Entries> entries_by_space;
    bool is_chunked = false;
    for (const auto& entry : req.m_dataObjects) {
        const std::string& cid = entry.first; // client ID
        const DataObject& obj = entry.second;

        if (obj.m_format != "xml") {
            reply.addError(cid, EINVALID_OBJECT, "Expected XML format");
            continue;
        }
        if (obj.m_data.empty()) {
            if(obj.m_blobId){
                is_chunked = true;
                continue;
            }else{
                reply.addError(cid, EINVALID_OBJECT, "No XML content");
                continue;
            }
        }

        EmlUri uri = EmlUri::parse(obj.m_resource.m_uri);
        if (!uri.isObj()) {
            reply.addError(cid, EINVALID_URI, "Expected eOBJ_INSTANCE URI");
            continue;
        }
        if (!uri.guid_.isValid()) {
            reply.addError(cid, EINVALID_URI, "Invalid Object UID");
            continue;
        }

        entries_by_space[uri.dataspace_].emplace_back(&entry);
    }

    if(is_chunked) {
        // Store the current request for later processing once chunks are received
        session->pending_put_data_objects_.emplace(
            messageId, std::move(req)
        );
        return;
    }

    if (entries_by_space.empty() ||
        (reply.errorCount() == req.m_dataObjects.size())
    ) {
        // all entries are invalid, bail out now
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: All entries failed validation
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_OBJECTS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_CREATE,
            "unknown",
            "All data objects failed validation"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR);
        return reply.finalProcess();
    }

    // Handle multi-spaces in parallel? Needs parallel transactions then?
    for (auto& entry : entries_by_space) {
        Entries& obj_entries = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const Entry* obj_entry : obj_entries) {
                reply.addError(obj_entry->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        std::vector<EmlUri> txSpaceUris;
        std::string txSpaceUrisStr;
        txSpaceUris.emplace_back(EmlUri::make_space(space.uid_)); // To replace by space.uri()
        txSpaceUrisStr = space.uid_ ;

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            for (const Entry* obj_entry : obj_entries) {
                reply.addError(obj_entry->first, err);
            }
            continue;
        }

        // Create a local transaction if no transaction is ongoing. Record the session Id in case it goes away in the meantime.
        auto sessionId = session->id() ;
        LocalTransactionWriteRaii<OpenKVStore> local_tx(*this, txSpaceUrisStr, txSpaceUris);

        // If this is a local transaction this might have taken enough time for the client to give up.
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR(
                "SVR Thread({}) Session({}): Session has been forcefully terminated while waiting for transaction, {} tries over {}s",
                oes::core::thisThreadId(), sessionId, local_tx.numTries(), local_tx.tryDuration()
            );
            throw std::runtime_error("SVR Session has been forcefully terminated while waiting for transaction.");
        }

        if(!local_tx.ready()){
            // This can only fail if this is a local transaction.
            
            // Audit log: Transaction timeout
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_OBJECTS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                space.uid_,
                fmt::format("Failed to get transaction after {} tries over {}s", local_tx.numTries(), local_tx.tryDuration()/1000)
            );
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR);
            return reply.fatalError(
                EMAX_TRANSACTIONS_EXCEEDED, fmt::format("SVR Thread({}) Session({}) Local Tx: Failed to get transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000)
            );
        } else if(local_tx.isLocal()){
            OES_INFO(
                "SVR Thread({}) Session({}) Local Tx: Got transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000
            );
        }
  
        auto writer = repo_->getSpaceWriter(space, err);
        if (!writer) {
            for (const Entry* obj_entry : obj_entries) {
                reply.addError(obj_entry->first, err);
            }
            continue;
        }

        OpenKVTransaction::OrphanAryMap orphan_arys;
        OpenKVTransaction::MissingAryMap missing_arys;
        OpenKVTransaction::OrphanAryMap* p_orphan_arys = &orphan_arys;
        OpenKVTransaction::MissingAryMap* p_missing_arys = &missing_arys;

        const auto tx = this->session()->tx_;
        if (tx!=nullptr) {
            p_orphan_arys = &tx->orphan_arys_;
            p_missing_arys = &tx->missing_arys_;
        }
        
        TransactionActControl* ctl_ptr (nullptr);
        if (session->check_activities_) {
            auto [ctl, status] = local_tx.get_trans_control()->emplace(std::make_pair(entry.first, TransactionActControl()) );
            ctl_ptr = &(ctl->second);
        }

        char * use_bulk_p = std::getenv("USE_BULK_WRITE");
        std::string use_bulk;
        
        if (use_bulk_p) {
            use_bulk = std::string(use_bulk_p);
        } else {
            use_bulk = "true";
        }
        if (use_bulk == "true") {
            bulkUpsertObjs(
                reply, *writer, *p_orphan_arys, *p_missing_arys, obj_entries, ctl_ptr, BULK_SIZE
            );
        } else {
            // Orphan arrays will be reconnected to their owner objects
            // (and removed from tx->orphan_arys_) inside insertObjs().
            // This may lead to interleaved pages for obj and ary tables.
            // Accumulate arys to update instead, to do it in one go afterwards???
            upsertObjs(
                reply, *writer, *p_orphan_arys, *p_missing_arys, obj_entries, ctl_ptr
            );
        }

        // Check if the session has not been terminated, since above call can take time
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Thread({}): Session has been forcefully terminated.",oes::core::thisThreadId());
            throw std::runtime_error("SVR Session has been forcefully terminated.");
        }

        // A transaction allows to "postpone" missing-refs errors to commit-time.
        // They are remembered (and cleared!) by the TX-wide writer.
        // But in the case of a local transaction, we error out immediately.
        if (local_tx.isLocal()) {
            writer->delete_null_res();
            if(repo_->updateSpaceInfo(space)){
                session = session_.lock().get();
                if(session != nullptr) {
                    OES_INFO(
                        "[{}] SVR Thread({}) Session({}) Updated dataspace info: {}",
                        session->client_identification_, oes::core::thisThreadId(), session->id(), space.uid_
                    );
                }
            }
            if (auto N = writer->missing_reference_count()) {
                const std::string& error_str = fmt::format(
                    "{} Missing reference(s) in space {}", N, space.uid_
                ); 
                const std::vector<ReferenceInfo>& missing_refs = writer->get_references_infos();
                for (const auto& ref : missing_refs) {
                    OES_ERROR(
                        "Missing reference without transaction: {}", ref.to_string()
                    );
                }

                // Audit log: Missing references
                AuditLog::log(
                    *repo_->getServerConfig(),
                    session,
                    AuditLogConstants::OP_PUT_DATA_OBJECTS,
                    AuditLogConstants::RESULT_FAILURE,
                    AuditLogConstants::ACTION_CREATE,
                    space.uid_,
                    error_str
                );
                oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR);
                span.setError(error_str);
                return reply.fatalError(EINVALID_STATE, error_str);
            }
        }

        if (local_tx.isLocal() && session->check_activities_ && ctl_ptr != nullptr) {
            if(ctl_ptr->upsertActivityObjs(*writer, session->userName(), entry.first)) {
                writer->delete_null_res();
            }
            local_tx.get_trans_control()->erase(entry.first);
        }
        local_tx.commit();
    }

    if (reply.errorCount() == req.m_dataObjects.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: Log failure for all objects
        if (!entries_by_space.empty()) {
            auto first_space = entries_by_space.begin()->first;
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_OBJECTS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                first_space,
                "All objects failed to put"
            );
        }
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR);
        return reply.finalProcess();
    }

    // Audit log: Log success for successfully inserted/updated objects
    size_t success_count = req.m_dataObjects.size() - reply.errorCount();
    if (success_count > 0) {
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_OBJECTS,
            AuditLogConstants::RESULT_SUCCESS,
            AuditLogConstants::ACTION_CREATE,
            "",
            fmt::format("{} object(s) successfully put, commit_status: {}", success_count, commit_status)
        );
    }

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::STORE_OPS_COUNTER_PUT_SUCCESS,oes::core::metrics::STORE_LATENCY_PUT,start);
    return reply.finalProcess();
} catch (...) {
    // Audit log: Log exception/failure
    try {
        if (auto session = session_.lock()) {
            AuditLog::log(
                *repo_->getServerConfig(),
                session.get(),
                AuditLogConstants::OP_PUT_DATA_OBJECTS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                "unknown",
                "Exception occurred during PutDataObjects"
            );
        }
    } catch (...) {
        // Ignore audit log errors
    }
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR);
    etp12::handleException(process_cb);
}

void OpenKVStore::DeleteDataObjects( // messageTypeId 3
    IRequest& ireq, DeleteDataObjects_req_t req,
    EtpResponseHandler<DeleteDataObjects_res_t> process_cb // messageTypeId 10
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("OpenKVStore." + std::string(__func__));
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setAttr("thread_id", oes::core::thisThreadId());
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.", oes::core::thisThreadId());
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR);
        return;
    }

    etp12::MultiPartReply reply(*session, process_cb);

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    // Check if we're in an ETP transaction (Protocol 18) for audit logging
    bool is_etp_transaction = (session->tx_ != nullptr);
    std::string commit_status = is_etp_transaction ? "COMMIT_PENDING" : "COMMITTED";

    if (req.m_uris.empty()) {
        span.setError("Nothing to delete");
        
        // Audit log: Empty request is a validation failure
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_DELETE_DATA_OBJECTS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_DELETE,
            "unknown",
            "Empty URI list"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to delete");
    }

    // First, check entries, and sort them by space
    using Entry = std::map<std::string, std::string>::value_type;
    using Entries = std::vector<const Entry*>;
    std::map<std::string, Entries> entries_by_space;
    for (const auto& entry : req.m_uris) {
        const std::string& cid = entry.first; // client ID
        const std::string& uri_str = entry.second;

        EmlUri uri = EmlUri::parse(uri_str);
        if (!uri.isObj()) {
            reply.addError(cid, EINVALID_URI, "Expected eOBJ_INSTANCE URI");
            continue;
        }
        if (!uri.guid_.isValid()) {
            reply.addError(cid, EINVALID_URI, "Invalid Object UID");
            continue;
        }

        entries_by_space[uri.dataspace_].emplace_back(&entry);
    }

    if (entries_by_space.empty() ||
        (reply.errorCount() == req.m_uris.size())
    ) {
        // all entries are invalid, bail out now
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: All URIs failed validation
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_DELETE_DATA_OBJECTS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_DELETE,
            "unknown",
            "All URIs failed validation"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR);
        return reply.finalProcess();
    }

    DeleteDataObjects_res_t resp;

    // Handle multi-spaces in parallel? Needs parallel transactions then?
    for (auto& entry : entries_by_space) {
        Entries& uri_entries = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const Entry* uri_entry : uri_entries) {
                reply.addError(uri_entry->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        std::vector<EmlUri> txSpaceUris;
        std::string txSpaceUrisStr;
        txSpaceUris.emplace_back(EmlUri::make_space(space.uid_)); // To replace by space.uri()
        txSpaceUrisStr = space.uid_ ;

        // Create a local transaction if no transaction is ongoing. Record the session Id in case it goes away in the meantime.
        auto sessionId = session->id() ;
        LocalTransactionWriteRaii<OpenKVStore> local_tx(*this, txSpaceUrisStr, txSpaceUris);

        // If this is a local transaction this might have taken enough time for the client to give up.
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR(
                "SVR Thread({}) Session({}): Session has been forcefully terminated while waiting for transaction, {} tries over {}s",
                oes::core::thisThreadId(), sessionId, local_tx.numTries(), local_tx.tryDuration()
            );
            throw std::runtime_error("SVR Session has been forcefully terminated while waiting for transaction.");
        }

        if(!local_tx.ready()){
            // This can only fail if this is a local transaction.
            
            // Audit log: Transaction timeout
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_DELETE_DATA_OBJECTS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_DELETE,
                space.uid_,
                fmt::format("Failed to get transaction after {} tries over {}s", local_tx.numTries(), local_tx.tryDuration()/1000)
            );
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR);
            return reply.fatalError(
                EMAX_TRANSACTIONS_EXCEEDED, fmt::format("SVR Thread({}) Session({}) Local Tx: Failed to get transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000)
            );
        } else if(local_tx.isLocal()){
            OES_INFO(
                "SVR Thread({}) Session({}) Local Tx: Got transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000
            );
        }

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            for (const Entry* uri_entry : uri_entries) {
                reply.addError(uri_entry->first, err);
            }
            continue;
        }

        auto writer = repo_->getSpaceWriter(space, err);
        if (!writer) {
            for (const Entry* uri_entry : uri_entries) {
                reply.addError(uri_entry->first, err);
            }
            continue;
        }

        auto& w = *writer;

        // Nullify the guid of the resource to delete it later
        std::vector<std::pair<int, oes::core::Guid> > batch ;
        std::unordered_map<std::string, std::pair<std::string, std::string> > cids ;
        for (const Entry* uri_entry : uri_entries) {
            const std::string& cid = uri_entry->first;
            const std::string& uri_str = uri_entry->second;
            EmlUri uri = EmlUri::parse(uri_str);
            assert(uri.isObj());

            const eml_int64 uri_id = ml_id(w, uri.ml_);
            const int typ_id = w.typ_of(uri_id, uri.obj_type_);

            if (uri_entries.empty()) {
                // Less than 0 objects to delete, do it one by one.
                if (w.null_res_guid(typ_id, uri.guid_)) {
                    auto it_ary = reply.addMapPart(cid);
                    auto& ary = it_ary.first->second;
                    ary.m_values.push_back(uri_str);
                }else{
                    reply.addError(cid, ENOT_FOUND, "Object not Found");
                }
            } else {
                // delete in a single batch to minimize the number of queries.
                batch.emplace_back(std::make_pair( typ_id, uri.guid_)) ;
                cids[uri.guid_.toXmlString()] = {cid, uri_str} ;
            }
        }

        if (!batch.empty()) {
            auto results = w.delete_objects(batch) ;
            for (const auto& [obj_id, typ_id, uid]: results) {
                if ( obj_id == -1) {
                    reply.addError(cids[uid.toXmlString()].first, ENOT_FOUND, "Object not Found");
                    continue ;
                }
                const auto key = uid.toXmlString();
                auto it_ary = reply.addMapPart(cids[key].first);
                auto& ary = it_ary.first->second;
                ary.m_values.push_back(cids[key].second);
            }
        }

        // Check if the session has not been terminated, since above call can take time
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Thread({}): Session has been forcefully terminated.",oes::core::thisThreadId());
            throw std::runtime_error("SVR Session has been forcefully terminated.");
        }

        // A transaction allows to "postpone" missing-refs errors to commit-time.
        // They are remembered (and cleared!) by the TX-wide writer.
        // But w/o one, we error out immediately.
        if (local_tx.isLocal()) {
            w.delete_null_res();
            if(repo_->updateSpaceInfo(space)){
                session = session_.lock().get();
                if(session != nullptr) {
                    OES_INFO(
                        "[{}] SVR Session({}) Updated dataspace info: {}",
                        session->client_identification_, session->id(), space.uid_
                    );
                }
            }
            if (auto N = w.orphan_relation_count()) {
                const std::string& error_str = fmt::format(
                    "{} dangling reference(s) in space {}", N, space.uid_
                );
                span.setError(error_str);
                
                // Audit log: Dangling references
                AuditLog::log(
                    *repo_->getServerConfig(),
                    session,
                    AuditLogConstants::OP_DELETE_DATA_OBJECTS,
                    AuditLogConstants::RESULT_FAILURE,
                    AuditLogConstants::ACTION_DELETE,
                    space.uid_,
                    error_str
                );
                oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR);
                return reply.fatalError(EINVALID_STATE, error_str);
            }
        }
        local_tx.commit();
    }

    assert((reply.errorCount() + reply.partCount()) == req.m_uris.size()); // cppcheck-suppress assertWithSideEffect

    if (reply.errorCount() == req.m_uris.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: Log failure for all delete operations
        if (!entries_by_space.empty()) {
            auto first_space = entries_by_space.begin()->first;
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_DELETE_DATA_OBJECTS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_DELETE,
                first_space,
                "All objects failed to delete"
            );
        }
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR);
        return reply.finalProcess();
    }

    // Audit log: Log success for successfully deleted objects
    size_t success_count = req.m_uris.size() - reply.errorCount();
    if (success_count > 0) {
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_DELETE_DATA_OBJECTS,
            AuditLogConstants::RESULT_SUCCESS,
            AuditLogConstants::ACTION_DELETE,
            "",
            fmt::format("{} object(s) successfully deleted, commit_status: {}", success_count, commit_status)
        );
    }

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::STORE_OPS_COUNTER_DELETE_SUCCESS,oes::core::metrics::STORE_LATENCY_DELETE,start);
    reply.finalProcess();
} catch (...) {
    // Audit log: Log exception/failure
    try {
        if (auto session = session_.lock()) {
            AuditLog::log(
                *repo_->getServerConfig(),
                session.get(),
                AuditLogConstants::OP_DELETE_DATA_OBJECTS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_DELETE,
                "unknown",
                "Exception occurred during DeleteDataObjects"
            );
        }
    } catch (...) {
        // Ignore audit log errors
    }
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR);
    etp12::handleException(process_cb);
}


void OpenKVStore::Chunk( // messageTypeId 8
    IRequest& ireq, Chunk_req_t req,
    EtpResponseHandler<PutDataObjects_res_t> process_cb // messageTypeId 9
) try {
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Thread({}): Session has been forcefully terminated.",oes::core::thisThreadId());
        return;
    }
    etp12::MultiPartReply reply(*session, process_cb);

    const int64_t messageId = ireq.header().m_correlationId;
    if(messageId == 0) {
        return reply.fatalError(EINVALID_ARGUMENT, fmt::format("Chunk must contains parent ID as correllation ID {}", messageId));
    }

    const auto it = session->pending_put_data_objects_.find(messageId);
    if(it == session->pending_put_data_objects_.end()) {
        return reply.fatalError(EINVALID_ARGUMENT, fmt::format("Chunk must be associated with existing parent {}", messageId));
    }
    struct PutDataObjects& pending_put_data_objects = it->second;

    bool found = false;
    for(auto& data_object : pending_put_data_objects.m_dataObjects) {
        if(data_object.second.m_blobId == req.m_blobId) {
            data_object.second.m_data.insert(data_object.second.m_data.end(), req.m_data.begin(), req.m_data.end());
        }
        found = true;
    }
    if(!found) {
        return reply.fatalError(EINVALID_ARGUMENT, fmt::format("Blob ID not found in parent message {}", messageId));
    }

    if(ireq.has_FIN_bit()) {
        auto put = std::move(pending_put_data_objects);
        session->pending_put_data_objects_.erase(messageId);
        for(auto& it1 : put.m_dataObjects) {
            it1.second.m_blobId.reset();
        }
        return PutDataObjectsImpl(messageId, std::move(put), process_cb);
    }
} catch (...) {
    etp12::handleException(process_cb);
}

//============================================================================

} // namespace oes::eml::openkv
