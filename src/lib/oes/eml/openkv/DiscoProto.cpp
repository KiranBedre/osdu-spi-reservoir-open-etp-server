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
#include <oes/core/utils/config_leave_3rdparty.h>

#include "DiscoProto.h"

#include "Repo.h"
#include "Session.h"
#include "SpaceDML.h"

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsHelper.h>
#include <oes/core/utils/MetricsNames.h>
#include <oes/postgresql/Statement.h>

#include <chrono>

#include <oes/core/utils/Otel.h>

#include "../etp12/Server.hpp"
#include "../etp12/MsgUtils.h"
#include "../etp12/MultiPartReply.hpp"


using namespace oes::eml::etp12::Datatypes;

using oes::core::Guid;
using oes::eml::etp12::XDataMap;

namespace {

//============================================================================

Guid getBlobUid(oes::postgresql::Statement& stmt, int pos, bool null_ok = false) {
    OES_UNUSED(null_ok);
    return stmt.getGuid(pos);
}

//============================================================================

void fetchXData(oes::postgresql::Statement& stmt, int pos, XDataMap& xmap) {
    std::string xdata_json = stmt.getString(pos);
    ++pos;

    std::vector<uint8_t> xdata_avro;
    int avro_size=0;
    const auto* avro_bytes = static_cast<const uint8_t*>(
        stmt.getBlob(pos, avro_size)
    );
    if( avro_size!=0 && avro_bytes!=nullptr){
        xdata_avro.assign(avro_bytes, avro_bytes + avro_size);
    }

    const bool ok = oes::eml::etp12::xdataFromJsonAndAvroBinary(
        xmap, xdata_json, xdata_avro
    );
    OES_RELEASE_UNUSED(ok);
    assert(ok);
}

//============================================================================

void fetchRelCounts(
    oes::postgresql::Statement& stmt, int pos, bool countObjects,
    Object::Resource& res, const oes::eml::openkv::SpaceReader_ptr& reader
) {
    // For now, we count relations as a separate query.
    // We can get away with that, because we are on SQLite.
    // (i.e. embedded DB, thus no additional per-Resource round-trip)
    if (countObjects) {
        assert(reader);
        auto& r = *reader;
        eml_int64 obj_id = stmt.getInt64(pos);
        auto [targets, sources] = r.count_dst_src_rels(obj_id);
        res.m_targetCount = targets;
        res.m_sourceCount = sources;
    } else {
        assert(!res.m_sourceCount); // i.e. default
        assert(!res.m_targetCount); // i.e. default
    }
}

//============================================================================

} // namespace

namespace oes::eml::openkv {

//============================================================================

OpenKVDiscovery::OpenKVDiscovery(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

void OpenKVDiscovery::GetResources(
    IRequest& ireq, GetResources_req_t req,
    EtpResponseHandler<GetResources_res_t> process_cb
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDiscovery.") + __func__,
                                    {{"req.uri", req.m_context.m_uri},
                                     {"req.depth", req.m_context.m_depth}});
    auto start = std::chrono::steady_clock::now();

    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
        return;
    }

    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    EmlUri uri = EmlUri::parse(req.m_context.m_uri);

    etp12::MultiPartReply reply(*session, process_cb);

    if (!uri.isValid()) {
        span.setError("Invalid URI: " + req.m_context.m_uri);
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
        return reply.fatalError(EINVALID_URI, "Invalid URI: " + req.m_context.m_uri);
    }

    Space space = repo_->findSpaceInfo(uri.dataspace_);
    if(!space.isValid()) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
        return reply.fatalError(ENOT_FOUND,fmt::format("Space {} not found", uri.dataspace_));
    }
    uri.dataspace_ = space.uid_;

    ErrorInfo err;
    Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
    if (!spacedb) {
        span.setError("Error: Could not find spacedb.");
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
        return reply.fatalError();
    }

    if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
        return reply.fatalError();
    }

    // TODO: req.m_countObjects support

    switch (uri.type_) {
    case EmlUri::eNO_TYPE:
        span.setAttr("req.uri.type", "NO_TYPE");
        span.setError("Invalid URI: neither dataspace or object.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
        return reply.fatalError(ENOTSUPPORTED, "Invalid URI: Neither Dataspace or Object");

    case EmlUri::eDATA_SPACE:
        span.setAttr("req.uri.type", "DATA_SPACE");
        getSpaceResources(
            uri, spacedb, ireq, std::move(req), reply
        );
        oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_SUCCESS,oes::core::metrics::DISCOVERY_LATENCY_GET_RESOURCES,start);
        return;

    case EmlUri::eOBJ_TYPE:
        span.setAttr("req.uri.type", "OBJ_TYPE");
        span.setError("Invalid URI: Neither dataspace or object");
        return reply.fatalError(ENOTSUPPORTED, "Invalid URI: Neither Dataspace or Object");

    case EmlUri::eOBJ_INSTANCE:
        span.setAttr("req.uri.type", "OBJ_INSTANCE");
        break;
    } // no default on purpose

    bool self = false;
    switch (req.m_scope) {
    case Object::ContextScopeKind::eSelf:
        span.setAttr("req.scope", "Self");
        getObjectSelf(
            uri, spacedb, ireq, std::move(req), reply
        );
        oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_SUCCESS,oes::core::metrics::DISCOVERY_LATENCY_GET_RESOURCES,start);
        return;

    case Object::ContextScopeKind::eTargetsOrSelf:
        span.setAttr("req.scope", "TargetsOrSelf");
        self = true;
        [[fallthrough]];
    case Object::ContextScopeKind::eTargets:
        span.setAttr("req.scope", "Targets");
        getResourcesTargets(
            self, uri, spacedb, ireq, std::move(req), reply
        );
        oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_SUCCESS,oes::core::metrics::DISCOVERY_LATENCY_GET_RESOURCES,start);
        return;

    case Object::ContextScopeKind::eSourcesOrSelf:
        span.setAttr("req.scope", "SourcesOrSelf");
        self = true;
        [[fallthrough]];
    case Object::ContextScopeKind::eSources:
        span.setAttr("req.scope", "Sources");
        getResourcesSources(
            self, uri, spacedb, ireq, std::move(req), reply
        );
        oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_SUCCESS,oes::core::metrics::DISCOVERY_LATENCY_GET_RESOURCES,start);
        return;
    } // no default on purpose

    span.setError("Internal Error: Unreachable");
    reply.fatalError(EINVALID_STATE, "Internal Error: Unreachable");
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
} catch (...) {
    // Record error metrics
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR);
    etp12::handleException(process_cb);
}

/**
 * @brief Retrieves a list of deleted resources from the specified dataspace.
 *
 * @param ireq The request object.
 * @param req The request parameters.
 * @param process_cb The callback function to process the response.
 */
void OpenKVDiscovery::GetDeletedResources(
    IRequest& ireq, GetDeletedResources_req_t req,
    EtpResponseHandler<GetDeletedResources_res_t> process_cb
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDiscovery.") + __func__);
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR);
        return;
    }
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    EmlUri uri = EmlUri::parse(req.m_dataspaceUri);

    OES_DEBUG_ASSERT(uri.type_ == EmlUri::eDATA_SPACE);

    etp12::MultiPartReply<GetDeletedResources_res_t> reply(*session, process_cb);

    if (!uri.isValid()) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR);
        return reply.fatalError(EINVALID_URI, "Invalid URI: " + req.m_dataspaceUri);
    }
    Space space = repo_->findSpaceInfo(uri.dataspace_);
    if(!space.isValid()) {
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR);
        return reply.fatalError(ENOT_FOUND,fmt::format("Space {} not found", uri.dataspace_));
    }
    uri.dataspace_ = space.uid_;

    ErrorInfo err;
    Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
    if (!spacedb) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR);
        return reply.fatalError();
    }

    if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR);
        return reply.fatalError();
    }

    std::string sql = R"sql(
  select guid, name, ml||'.'||xml, xdata_json, xdata_avro,
       (EXTRACT(EPOCH FROM (storeDeleted AT TIME ZONE 'UTC'))*1000000)::bigint
  from del_res
  join typ on typ.id = del_res.typ_id
  join uri on uri.id = typ.uri_id
  where {}
  and {}
    )sql";

    const bool deleteTimeFiltering = req.m_deleteTimeFilter && *req.m_deleteTimeFilter > 0;
    std::string time_filter = deleteTimeFiltering ?
        "(EXTRACT(EPOCH FROM (storeDeleted AT TIME ZONE 'UTC'))*1000000)::bigint > $1":
        "1 = 1";

    std::string type_filter = "1 = 1";
    if (!req.m_dataObjectTypes.empty()) {
        type_filter = fmt::format(
            std::string(R"sql(typ_id =
            (
                select typ.id
                from typ
                join uri on uri.id = typ.uri_id
                where uri.ml || '.' || typ.xml = {}
            ))sql"),
            deleteTimeFiltering ? "$2" : "$1"
         );
    }
    sql = fmt::format(sql, time_filter, type_filter);
    boost::trim(sql);

    oes::postgresql::Statement stmt = spacedb->prepare(std::move(sql));

    int bind_idx = 0;
    if (deleteTimeFiltering) {
        // Warning: m_storeLastWriteFilter in *micro-seconds*.
        stmt.bind(++bind_idx, eml_int64(*req.m_deleteTimeFilter));
    }
    const int type_bind_idx = ++bind_idx;
    const size_t type_count = req.m_dataObjectTypes.size();
    if (req.m_dataObjectTypes.empty()) {
        req.m_dataObjectTypes.emplace_back("*ALL*");
    }

    const std::string dataspace = uri.dataspace_;
    for (const std::string& etp_type : req.m_dataObjectTypes) {
        if (!type_count) {
            assert(etp_type == "*ALL*");
        } else {
            stmt.bind(type_bind_idx, etp_type);
        }
        while (stmt.step()) {
            const Guid uid = getBlobUid(stmt, 0);

            oes::eml::etp12::Datatypes::Object::DeletedResource& res = reply.addVecPart();

            std::string name = stmt.getString(1);
            std::string dataObjectType = stmt.getString(2);
            auto dot = dataObjectType.find('.');
            EmlUri::ML ml = EmlUri::parseML(dataObjectType.c_str(), dot);

            const std::string obj_uri = EmlUri::make_obj(
                dataspace, dataObjectType.substr(dot + 1), uid, ml
            ).to_string();
            XDataMap xdata;
            fetchXData(stmt, 3, xdata); // fetches 3 *and* 4

            size_t size = obj_uri.size()+8; //string size + 8 bytes for length
            size += 8; // deletedTime
            for(const auto& [key, val] : xdata) {
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

            reply.maybeProcess(size);
            res.m_uri = std::move(obj_uri);
            res.m_deletedTime = stmt.getInt64(5);
            res.m_customData = std::move(xdata);
            reply.addBytes(size);
        }
        stmt.reset();
    }

    // Record success metrics
    oes::core::MetricsHelper::recordCounterWithLatency(
        oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_SUCCESS,
        oes::core::metrics::DISCOVERY_LATENCY_GET_DELETED_RESOURCES,
        start
    );

    reply.finalProcess();
} catch (const std::exception& ex) {
    OES_ERROR("GetDeletedResources: {}", ex.what());

    // Record error metrics
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR);
    etp12::handleException(process_cb);
}

//============================================================================
/**
 * Fetches resources from the database and sends them as a multipart reply.
 *
 * @param stmt The statement to execute.
 * @param reply The multipart reply to send the resources in.
 * @param req The request containing the context and filters for the resources.
 * @param repo The OpenKV repository to fetch the resources from.
 * @param dataspace The dataspace to fetch the resources from.
 * @param type_bind_idx The index of the type binding in the statement.
 */
void fetchResourceImpl(
    oes::postgresql::Statement& stmt,
    etp12::MultiPartReply<oes::eml::etp12::Protocol::IDiscovery::GetResources_res_t>& reply,
    oes::eml::etp12::Protocol::Discovery::GetResources& req,
    oes::eml::openkv::OpenKVRepo_ptr repo, const Space& dataspace,
    int type_bind_idx
){
    oes::eml::openkv::SpaceReader_ptr reader;
    if (req.m_countObjects || req.m_includeEdges) {
        ErrorInfo err;
        reader = repo->getSpaceReader(dataspace, err);
        if (!reader) {
            return reply.fatalError(EINVALID_STATE, "Cannot read dataspace");
        }
    }

    const size_t type_count = req.m_context.m_dataObjectTypes.size();
    if (req.m_context.m_dataObjectTypes.empty()) {
        req.m_context.m_dataObjectTypes.emplace_back("*ALL*");
    }

    std::vector<eml_int64> obj_ids;
    std::map<eml_int64, std::string> id2uri;
    for (const std::string& etp_type : req.m_context.m_dataObjectTypes) {
        if (!type_count) {
            assert(etp_type == "*ALL*");
        } else {
            stmt.bind(type_bind_idx, etp_type);
        }
        while (stmt.step()) {
            const std::string name = stmt.getString(1);

            const Guid uid = getBlobUid(stmt, 0);
            std::string dataObjectType = stmt.getString(2);
            auto dot = dataObjectType.find('.');
            EmlUri::ML ml = EmlUri::parseML(dataObjectType.c_str(), dot);
            const std::string uri = EmlUri::make_obj(
                dataspace.uid_, dataObjectType.substr(dot + 1), uid, ml
            ).to_string();
            std::string creator = stmt.getString(3);

            eml_int64 createdAt = stmt.getInt64(4); // Never null, might be 0 though.
            eml_int64 updatedAt = stmt.getInt64(5); // Micro-seconds UTC since Epoch.
            XDataMap xdata;
            fetchXData(stmt, 6, xdata); // fetches 6 *and* 7
            eml_int64 storeCreated = stmt.getInt64(9);
            eml_int64 storeUpdated = stmt.getInt64(10);
            eml_int64 space_rel_id = stmt.getInt64(11);

            const eml_int64 obj_id = stmt.getInt64(8);

            size_t size = name.size()+8; //string size + 8 bytes for length
            size += uri.size()+8; //string size + 8 bytes for length
            size += 8; // lastChanged
            size += 8; // storeCreated
            size += 8; // storeLastWrite
            for(const auto& [key, val] : xdata) {
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
            size += 16; // countObjects

            reply.maybeProcess(size);

            Resource& res = reply.addVecPart();
            res.m_name = std::move(name);
            res.m_uri = std::move(uri);
            res.m_lastChanged = std::max<>(createdAt, updatedAt);
            res.m_storeCreated = storeCreated;
            res.m_storeLastWrite = storeUpdated;
            res.m_customData = std::move(xdata);
            fetchRelCounts(stmt, 8, req.m_countObjects, res, reader);

            obj_ids.push_back(obj_id);
            id2uri[obj_id] = res.m_uri;

            // Make sure "our" creator key "wins"
            if (!etp12::xdataEmplace(res.m_customData, "creator", creator)) {
                res.m_customData["creator"].m_item = std::move(creator);
            }

            // Add or update created date
            DataValue val;
            val.m_item = createdAt;
            res.m_customData["created"] = std::move(val);

            // Add information about the space relation indicating import by reference
            if(space_rel_id > 0) {
                etp12::xdataEmplace(res.m_customData, "reference", "true");
            }

            reply.addBytes(size);
        }
        stmt.reset();
    }

    if(req.m_includeEdges && !obj_ids.empty()) {
        std::vector<EdgeRel> edges = reader->edge_rels(obj_ids);
        if(edges.size() > 0) {
            reply.maybeProcess(true);
            auto edges_reply = std::make_shared<
                oes::common::etp12::Protocol::Discovery::GetResourcesEdgesResponse
            >();
            // Process edges in chunks to avoid exceeding the max_bytes limit
            size_t cur_bytes = 0;
            for(auto& edge : edges) {
                oes::common::etp12::Datatypes::Object::Edge edge_res;
                const std::string& source_uri = id2uri[std::get<0>(edge)];
                const std::string& target_uri = id2uri[std::get<1>(edge)];
                DataValue dv;
                dv.m_item = std::get<2>(edge);

                size_t next_bytes = 8; // Size of array
                next_bytes += source_uri.size()+8; // string size + 8 bytes for length
                next_bytes += target_uri.size()+8; // string size + 8 bytes for length
                next_bytes += 8; // customData map size
                next_bytes += std::string("path").size()+8; // string size + 8 bytes for length
                next_bytes += std::get<std::string>(dv.m_item).size()+16;
                    // 8 bytes for string length + 8 for index

                const bool too_big = (cur_bytes + next_bytes > reply.max_bytes_);
                if(too_big){
                    // Add the current chunk and start a new one
                    reply.processAdditional( std::static_pointer_cast<oes::common::etp12::AbstractMessage>(edges_reply) );
                    edges_reply = std::make_shared<
                        oes::common::etp12::Protocol::Discovery::GetResourcesEdgesResponse
                    >();
                    cur_bytes = 0;
                }

                cur_bytes += next_bytes;
                edge_res.m_sourceUri = source_uri;
                edge_res.m_targetUri = target_uri;
                edge_res.m_customData = XDataMap();
                edge_res.m_customData.emplace("path", std::move(dv));
                edges_reply->m_edges.push_back(edge_res);
            }

            reply.processAdditional( std::static_pointer_cast<oes::common::etp12::AbstractMessage>(edges_reply) );
        }
    }
    reply.finalProcess();
}

void OpenKVDiscovery::getObjectSelf(
    const EmlUri& uri, const Connection_ptr& spacedb,
    IRequest& ireq, GetResources_req_t req,
    MultiPartReply<GetResources_res_t>& reply
){
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDiscovery.") + __func__);
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    OES_DEBUG_ASSERT(uri.type_ == EmlUri::eOBJ_INSTANCE);
    OES_DEBUG_ASSERT(req.m_scope == Object::ContextScopeKind::eSelf);

    std::string sql = R"sql(

select guid, name, ml||'.'||xml, createdBy, createdAt, updatedAt,
       xdata_json, xdata_avro, obj_id,
       (EXTRACT(EPOCH FROM (storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       space_rel_id
  from res
  join typ on typ.id = res.typ_id
  join uri on uri.id = typ.uri_id
 where res.typ_id =
       (
        select typ.id
          from typ
          join uri on uri.id = typ.uri_id
         where uri.ml || '.' || typ.xml = $1
       )
   and res.guid = $2
   and {}
   and {}

    )sql";

    const bool lastChangeFiltering =
        req.m_storeLastWriteFilter &&
        *req.m_storeLastWriteFilter > 0
    ;
    std::string time_filter = "1 = 1";
    if (lastChangeFiltering) {
        // ETP 1.2 wants to compared against the *STORE* timestamps,
        // i.e. when the row was last PutDataObjects into the store.
        // This is why we are not using the DataObject's timestamps (updatedAt or createdAt)
        time_filter = "(EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint > $1";
    }
    std::string type_filter = "1 = 1";
    if (!req.m_context.m_dataObjectTypes.empty()) {
        type_filter = fmt::format(
            R"sql(typ_id =
            (
            select typ.id
                from typ
                join uri on uri.id = typ.uri_id
                where uri.ml || '.' || typ.xml = {}
            ))sql",
            lastChangeFiltering ? "$2" : "$1"
        );
    }
    sql = fmt::format(sql, time_filter, type_filter);
    boost::trim(sql);
    oes::postgresql::Statement stmt = spacedb->prepare(std::move(sql));

    int bind_idx = 0;
    stmt.bind(++bind_idx, uri.to_type());
    const int type_bind_idx = bind_idx;
    stmt.bind(++bind_idx, uri.guid_.begin(), int(Guid::static_size()));

    if (lastChangeFiltering) {
        // Warning: m_storeLastWriteFilter in *micro-seconds*.
        stmt.bind(++bind_idx, eml_int64(*req.m_storeLastWriteFilter));
    }

    Space space = repo_->findSpaceInfo(uri.dataspace_);
    fetchResourceImpl(stmt, reply, req, repo_, space, type_bind_idx);
}

// see also DxInterfaceTypeHandler's accept_get_resources()
void OpenKVDiscovery::getSpaceResources(
    const EmlUri& uri, const Connection_ptr& spacedb,
    IRequest& ireq, GetResources_req_t req,
    MultiPartReply<GetResources_res_t>& reply
){
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDiscovery.") + __func__,
                                    {{"req.uri", req.m_context.m_uri}});

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    OES_DEBUG_ASSERT(uri.type_ == EmlUri::eDATA_SPACE);

    // From 8.2.2 Discovery: General Requirements:
    // When discovering a dataspace URI, the scope and depth fields
    // (in the GetResources message) MUST be ignored.
    //OES_DEBUG_ASSERT(req.m_scope == Object::ContextScopeKind::eSelf);

    std::string sql = R"sql(

select guid, name, ml||'.'||xml, createdBy, createdAt, updatedAt,
       xdata_json, xdata_avro, obj_id,
       (EXTRACT(EPOCH FROM (storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       space_rel_id
  from res
  join typ on typ.id = res.typ_id
  join uri on uri.id = typ.uri_id
 where {}
   and {}

    )sql";

    const bool lastChangeFiltering =
        req.m_storeLastWriteFilter &&
        *req.m_storeLastWriteFilter > 0
    ;
    std::string time_filter = "1 = 1";
    if (lastChangeFiltering) {
        // ETP 1.2 wants to compared against the *STORE* timestamps,
        // i.e. when the row was last PutDataObjects into the store.
        // This is why we are not using the DataObject's timestamps (updatedAt or createdAt)
        time_filter = "(EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint > $1";
    }
    std::string type_filter = "1 = 1";
    if (!req.m_context.m_dataObjectTypes.empty()) {
        type_filter = fmt::format(
            std::string(R"sql(typ_id =
            (
                select typ.id
                from typ
                join uri on uri.id = typ.uri_id
                where uri.ml || '.' || typ.xml = {}
            ))sql"),
            lastChangeFiltering ? "$2" : "$1"
         );
    }
    sql = fmt::format(sql, time_filter, type_filter);
    boost::trim(sql);
    oes::postgresql::Statement stmt = spacedb->prepare(std::move(sql));

    int bind_idx = 0;
    if (lastChangeFiltering) {
        // Warning: m_storeLastWriteFilter in *micro-seconds*.
        stmt.bind(++bind_idx, eml_int64(*req.m_storeLastWriteFilter));
    }
    const int type_bind_idx = ++bind_idx;

    Space space = repo_->findSpaceInfo(uri.dataspace_);
    fetchResourceImpl(stmt, reply, req, repo_, space, type_bind_idx);
}

// see also DiscoveryHandler.hpp's accept_graph_discovery()
/*!
 * \internal
 * \brief Loads \b target resources of a single entity.
 *
 * The ETP terminology \em targets designates the other entities the
 * explored entity itself directly (or indirectly when \a depth > 1)
 * references, which in database-terms means loading the \em foreign-keys
 * of that entity.
 */
void OpenKVDiscovery::getResourcesTargets(
    bool self, const EmlUri& uri, const Connection_ptr& spacedb,
    IRequest& ireq, GetResources_req_t req,
    MultiPartReply<GetResources_res_t>& reply
){
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDiscovery.") + __func__,
                                    {{"req.uri", req.m_context.m_uri},
                                     {"req.depth", req.m_context.m_depth}});
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    std::string sql = R"sql(
select res2.guid, res2.name, ml||'.'||xml,
       res2.createdBy, res2.createdAt, res2.updatedAt,
       res2.xdata_json, res2.xdata_avro, res2.obj_id,
       (EXTRACT(EPOCH FROM (res2.storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (res2.storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       res2.space_rel_id,
       1 as level
  from res
  join rel on rel.obj_id = res.obj_id
  join res res2 on res2.obj_id = rel.dst_id
  join typ on typ.id = res2.typ_id
  join uri on uri.id = typ.uri_id
 where res.typ_id =
       (
        select typ.id
          from typ
          join uri on uri.id = typ.uri_id
         where uri.ml || '.' || typ.xml = $1
       )
   and res.guid = $2
   and {}
   and {}

    )sql";

    std::string cte = R"sql(
select guid, name, ml||'.'||xml, createdBy, createdAt, updatedAt,
       xdata_json, xdata_avro, ids.obj_id,
       (EXTRACT(EPOCH FROM (storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       space_rel_id,
       ids.level
    from (
        with rescache as (
            select obj_id, 0 as level from res
            where res.typ_id = (
                select typ.id from typ
                    join uri on uri.id = typ.uri_id
                    where uri.ml || '.' || typ.xml = $1
            )
            and res.guid = $2
        )
        select obj_id, MIN(level) as level from (
            WITH RECURSIVE children_of(obj_id, level) as (
                select obj_id, 0 as level from rescache
                UNION
                select dst_id, level + 1 from rel
                    join children_of c on c.obj_id = rel.obj_id
                    where level < $3
            )
            select obj_id, level from children_of
            where {}
            {} -- secondary sources
            {} -- secondary targets
        ) c group by obj_id
    ) as ids
    join res on res.obj_id = ids.obj_id
    join typ on typ.id = res.typ_id
    join uri on uri.id = typ.uri_id
    where {}
    and {}
order by ids.level
    )sql";

    // the non-recursive query (sql) does NOT return the self,
    // while the more complex recursive one can (or not, our choice).
    const bool use_cte = (self || req.m_context.m_depth >  1
        || req.m_context.m_includeSecondarySources || req.m_context.m_includeSecondaryTargets);

    const bool lastChangeFiltering =
        req.m_storeLastWriteFilter &&
        *req.m_storeLastWriteFilter > 0
    ;

    std::string level_filter = "1 = 1";
    if (!self) {
        level_filter = "level <> 0";
    }
    std::string time_filter = "1 = 1";
    if (lastChangeFiltering) {
        // FIXME: updatedAt or createdAt are the DataObject's timestamps,
        // while ETP 1.2 wants to compared against the *STORE* timestamps,
        // i.e. when the row was last PutDataObjects into the store
        // (inserted or updated in our case). Which will require keeping
        // track of those (micro-second level) timestamps (via rev table?)
        time_filter = fmt::format(
            "coaleasce({0}.updatedAt, {0}.createdAt, 0) > {1}",
            use_cte ? "c" : "res2",
            use_cte ? "$4" : "$3"
        );
    }
    std::string type_filter = "1 = 1";
    if (!req.m_context.m_dataObjectTypes.empty()) {
        if(lastChangeFiltering){
            type_filter = fmt::format(
                std::string(R"sql({}typ_id =
                (
                    select typ.id
                    from typ
                    join uri on uri.id = typ.uri_id
                    where uri.ml || '.' || typ.xml = {}
                ))sql"),
                use_cte ? "" : "res2.",
                use_cte ? "$5" : "$4"
            );
        } else {
            type_filter = fmt::format(
                R"sql({}typ_id =
                (
                    select typ.id
                    from typ
                    join uri on uri.id = typ.uri_id
                    where uri.ml || '.' || typ.xml = {}
                ))sql",
                use_cte ? "" : "res2.",
                use_cte ? "$4" : "$3"
            );
        }
    }

    if (use_cte) {
        const std::string secondary_sources = req.m_context.m_includeSecondarySources ?
        fmt::format(R"sql(
            UNION distinct
            select rel.obj_id, cast ({} as bigint) as level
            from rel
            join children_of c on c.obj_id = rel.dst_id
        )sql", req.m_context.m_depth+10) : "";
        const std::string secondary_targets = req.m_context.m_includeSecondaryTargets ?
        fmt::format(R"sql(
            UNION distinct
            select rel.dst_id, cast ({} as bigint) as level
            from rel
            join children_of c on c.obj_id = rel.obj_id
        )sql", req.m_context.m_depth+10) : "";
        cte = fmt::format(cte, level_filter, secondary_sources, secondary_targets, time_filter, type_filter);
        boost::trim(cte);
    } else {
        sql = fmt::format(sql, time_filter, type_filter);
        boost::trim(sql);
    }
    oes::postgresql::Statement stmt = spacedb->prepare(use_cte ? cte : sql);

    int bind_idx = 0;
    stmt.bind(++bind_idx, uri.to_type());
    stmt.bind(++bind_idx, uri.guid_.begin(), int(Guid::static_size()));
    if (use_cte) {
        stmt.bind(++bind_idx, req.m_context.m_depth);
    }
    if (lastChangeFiltering) {
        // Warning: m_storeLastWriteFilter in *micro-seconds*.
        stmt.bind(++bind_idx, eml_int64(*req.m_storeLastWriteFilter));
    }
    const int type_bind_idx = ++bind_idx;
    Space space = repo_->findSpaceInfo(uri.dataspace_);
    fetchResourceImpl(stmt, reply, req, repo_, space, type_bind_idx);
}

// see also DiscoveryHandler.hpp's accept_graph_discovery()
void OpenKVDiscovery::getResourcesSources(
    bool self, const EmlUri& uri, const Connection_ptr& spacedb,
    IRequest& ireq, GetResources_req_t req,
    MultiPartReply<GetResources_res_t>& reply
){
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDiscovery.") + __func__,
                                    {{"req.uri", req.m_context.m_uri},
                                     {"req.depth", req.m_context.m_depth}});
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    std::string cte = R"sql(
select guid, name, ml||'.'||xml, createdBy, createdAt, updatedAt,
       xdata_json, xdata_avro, ids.obj_id,
       (EXTRACT(EPOCH FROM (storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       space_rel_id,
       ids.level
    from (
        with rescache as (
            select obj_id, 0 as level from res
                where res.typ_id = (
                    select typ.id from typ
                        join uri on uri.id = typ.uri_id
                        where uri.ml || '.' || typ.xml = $1
                )
                and res.guid = $2
        )
        select obj_id, MIN(level) as level from (
            WITH RECURSIVE children_of(obj_id, level) as (
                select obj_id, 0 as level from rescache
                UNION
                select rel.obj_id, level + 1 from rel
                    join children_of c on c.obj_id = rel.dst_id
                    where level < $3
            )
            select obj_id, level from children_of
                where {}
                {} -- secondary sources
                {} -- secondary targets
       ) c group by obj_id
    ) as ids
    join res on res.obj_id = ids.obj_id
    join typ on typ.id = res.typ_id
    join uri on uri.id = typ.uri_id
    where {}
    and {}
order by ids.level
    )sql";

    const bool lastChangeFiltering =
        req.m_storeLastWriteFilter &&
        *req.m_storeLastWriteFilter > 0
    ;

    std::string level_filter = "1 = 1";
    if (!self) {
        level_filter = "level <> 0";
    }
    std::string time_filter = "1 = 1";
    if (lastChangeFiltering) {
        // FIXME: updatedAt or createdAt are the DataObject's timestamps,
        // while ETP 1.2 wants to compared against the *STORE* timestamps,
        // i.e. when the row was last PutDataObjects into the store
        // (inserted or updated in our case). Which will require keeping
        // track of those (micro-second level) timestamps (via rev table?)
        time_filter = "coalesce(c.updatedAt, c.createdAt, 0) > ?";
    }
    std::string type_filter = "1 = 1";
    if (!req.m_context.m_dataObjectTypes.empty()) {
        type_filter = R"sql(typ_id =
            (
            select typ.id
              from typ
              join uri on uri.id = typ.uri_id
             where uri.ml || '.' || typ.xml = $4
            ))sql"
        ;
    }

    const std::string secondary_sources = req.m_context.m_includeSecondarySources ?
    fmt::format(R"sql(
        UNION distinct
        select rel.obj_id, cast ({} as bigint) as level
        from rel
        join children_of c on c.obj_id = rel.dst_id
    )sql", req.m_context.m_depth+10) : "";
    const std::string secondary_targets = req.m_context.m_includeSecondaryTargets ?
    fmt::format(R"sql(
        UNION distinct
        select rel.dst_id, cast ({} as bigint) as level
        from rel
        join children_of c on c.obj_id = rel.obj_id
    )sql", req.m_context.m_depth+10) : "";
    cte = fmt::format(cte, level_filter, secondary_sources, secondary_targets, time_filter, type_filter);
    boost::trim(cte);
    oes::postgresql::Statement stmt = spacedb->prepare(std::move(cte));

    int bind_idx = 0;
    stmt.bind(++bind_idx, uri.to_type());
    stmt.bind(++bind_idx, uri.guid_.begin(), int(Guid::static_size()));
    stmt.bind(++bind_idx, req.m_context.m_depth);
    if (lastChangeFiltering) {
        // Warning: m_storeLastWriteFilter in *micro-seconds*.
        stmt.bind(++bind_idx, eml_int64(*req.m_storeLastWriteFilter));
    }
    const int type_bind_idx = ++bind_idx;
    Space space = repo_->findSpaceInfo(uri.dataspace_);
    fetchResourceImpl(stmt, reply, req, repo_, space, type_bind_idx);
}

//============================================================================

} // namespace oes::eml::openkv
