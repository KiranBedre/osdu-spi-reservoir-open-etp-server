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
#include <regex>

#include <fmt/format.h>

#include "ArrayProto.h"
#include "AuditLog.h"
#include "DiscoProto.h"
#include "Repo.h"
#include "StoreProto.h"
#include "StoreOSDUProto.h"
#include "TransProto.h"

#include "Session.h"

#include <oes/common/epc/FilePart.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThreadUtils.h>
#include <oes/eml/etp12/MultiPartReply.hpp>

#include "../etp12/Server.hpp"

using namespace oes::eml::etp12::Datatypes;

using oes::eml::etp12::errorStatus;
using oes::eml::etp12::addErrorStatus;

//============================================================================


namespace oes::eml::openkv {

//============================================================================

OpenKVStoreOSDU::OpenKVStoreOSDU(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

void OpenKVStoreOSDU::CopyDataObjectsByValue(
    IRequest& ireq, CopyDataObjectsByValue_req_t req,
    EtpResponseHandler<CopyDataObjectsByValue_res_t> process_cb // messageTypeId 2
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

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    oes::eml::etp12::MultiPartReply reply(*session, process_cb);

    EmlUri reqDataspaceUri = EmlUri::parse(req.m_uri);
    std::vector<EmlUri> txSpaceUris;
    std::string txSpaceUrisStr = reqDataspaceUri.dataspace_;
    txSpaceUris.emplace_back(EmlUri::make_space(txSpaceUrisStr)); // To replace by space.uri()

    // Create a local transaction if no transaction is ongoing. Record the session Id in case it goes away in the meantime.
    auto sessionId = session->id() ;
    LocalTransactionWriteRaii<OpenKVStoreOSDU> local_tx(*this, txSpaceUrisStr, txSpaceUris);

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
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COPY_DATA_OBJECTS_BY_VALUE, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, req.m_uri, fmt::format("Failed to get transaction after {} tries over {}s", local_tx.numTries(), local_tx.tryDuration()/1000));
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

    // Get Resources to copy
    oes::common::etp12::Protocol::IDiscovery::GetResources_req_t getResourcesRequest;
    getResourcesRequest.m_context.m_uri = req.m_uri;
    getResourcesRequest.m_context.m_depth = req.m_sourcesDepth;
    getResourcesRequest.m_context.m_dataObjectTypes = req.m_dataObjectTypes;
    getResourcesRequest.m_context.m_navigableEdges = Object::RelationshipKind::eBoth;
    getResourcesRequest.m_context.m_includeSecondaryTargets = false;
    getResourcesRequest.m_context.m_includeSecondarySources = false;
    getResourcesRequest.m_scope = Object::ContextScopeKind::eSourcesOrSelf;
    getResourcesRequest.m_countObjects = false;
    getResourcesRequest.m_includeEdges = false;
    const auto getResourcesReply = rpcCall(&OpenKVDiscovery::GetResources, getResourcesRequest);
    if (!getResourcesReply.first.ok) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COPY_DATA_OBJECTS_BY_VALUE, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, req.m_uri, "GetResources failed");
        reply.rc_ = std::move(getResourcesReply.first);
        return reply.fatalError();
    }

    // Get DataObjects to copy according to the resources
    std::unordered_map< std::string, std::string > uuidsToReplace;
    oes::common::etp12::Protocol::IStore::GetDataObjects_req_t getDataObjectsRequest;
    size_t index = 0;
    for (const Resource& resource : getResourcesReply.second.m_resources) {
        getDataObjectsRequest.m_uris.emplace(std::to_string(index++), resource.m_uri);
        uuidsToReplace.emplace(EmlUri::parse(resource.m_uri).guid_.toXmlString(), Guid::make().toXmlString());
    }
    getDataObjectsRequest.m_format = "xml";
    auto getDataObjectsReply = rpcCall(&OpenKVStore::GetDataObjects, getDataObjectsRequest);
    if (!getDataObjectsReply.first.ok) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COPY_DATA_OBJECTS_BY_VALUE, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, req.m_uri, "GetDataObjects failed");
        reply.rc_ = std::move(getDataObjectsReply.first);
        return reply.fatalError();
    }

    // Modify UUIDs in DataObjects and Put duplicated data arrays
    for (auto& [key, dataobj] : getDataObjectsReply.second.m_dataObjects) {
        std::string xmlData(dataobj.m_data.begin(), dataobj.m_data.end());
        oes::common::epc::FilePart::ScanRc scan_rc;
        oes::common::epc::FilePart::ScanData scan;
        std::tie(scan, scan_rc) = oes::common::epc::FilePart::scanData(xmlData, oes::common::epc::FilePart::eHdfs);
        assert(scan_rc == oes::common::epc::FilePart::ScanRc::eOK_DONE);
        for (auto& uuidMapping : uuidsToReplace) {
            for (auto& xml_hdf : scan.hdfs_) {
                // TODO : we assume for now that PathInHdfFile MUST contain the UUID of its related dataobject
                if (xml_hdf.second.PathInHdfFile_.find(uuidMapping.first) == std::string::npos) { continue; }
                oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier daIdentifier;
                daIdentifier.m_uri = EmlUri::make_obj(reqDataspaceUri.dataspace_, "obj_ExternalPartReference", oes::core::Guid::parse(xml_hdf.second.HdfProxy_.Uuid_)).to_string();
                daIdentifier.m_pathInResource = xml_hdf.second.PathInHdfFile_;
                oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier daIdentifierTarget = daIdentifier;
                daIdentifierTarget.m_pathInResource = std::regex_replace(daIdentifier.m_pathInResource, std::regex(uuidMapping.first), uuidMapping.second);

                auto response = copyDataArray(daIdentifier, daIdentifierTarget);
                if (!response.ok) {
                    AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COPY_DATA_OBJECTS_BY_VALUE, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, req.m_uri, "Array copy failed");
                    reply.rc_ = std::move(response);
                    return reply.fatalError();
                }
            }

            xmlData = std::regex_replace(xmlData, std::regex(uuidMapping.first), uuidMapping.second);
            dataobj.m_data = std::vector<uint8_t>(xmlData.begin(), xmlData.end());
            dataobj.m_resource.m_uri = std::regex_replace(dataobj.m_resource.m_uri, std::regex(uuidMapping.first), uuidMapping.second);
        }
    }

    // Put the modified DataObjects into the store
    oes::common::etp12::Protocol::IStore::PutDataObjects_req_t putDataObjectsRequest;
    putDataObjectsRequest.m_dataObjects = getDataObjectsReply.second.m_dataObjects;
    const auto putDataObjectsReply = rpcCall(&OpenKVStore::PutDataObjects, putDataObjectsRequest);
    if (!putDataObjectsReply.first.ok) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COPY_DATA_OBJECTS_BY_VALUE, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, req.m_uri, "PutDataObjects failed");
        reply.rc_ = std::move(putDataObjectsReply.first);
        return reply.fatalError();
    }

    // Build response
    for (const auto& [key, value] : getDataObjectsReply.second.m_dataObjects) {
        const size_t size = value.m_resource.m_uri.size()+8; //string size + 8 bytes for length
        reply.maybeProcess(size);
        std::string& res = reply.addVecPart();
        res.assign(value.m_resource.m_uri);
        reply.addBytes(size);
    }

    AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COPY_DATA_OBJECTS_BY_VALUE, AuditLogConstants::RESULT_SUCCESS, AuditLogConstants::ACTION_CREATE, req.m_uri, fmt::format("{} object(s) copied successfully", getDataObjectsReply.second.m_dataObjects.size()));

    reply.finalProcess();
} catch (...) {
    OpenKVSession* session = session_.lock().get();
    if (session != nullptr) {
        try {
            AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COPY_DATA_OBJECTS_BY_VALUE, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, req.m_uri, "Unexpected exception during CopyDataObjectsByValue");
        } catch (...) {
            // Ignore audit log errors in exception handler
        }
    }
    etp12::handleException(process_cb);
}

//============================================================================

/*!
 * \brief Copy a DataArray
 * \param dataArrraySource The information about the data array to be copied
 * \param dataArrrayTarget The information about the copied data array
*/
oes::eml::etp12::Protocol::EtpResponseStatus OpenKVStoreOSDU::copyDataArray(const oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier& dataArrraySource,
        const oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier& dataArrrayTarget) {
    // Get the metadata of the DataArray source in order to find the (preferred subarray) dimensions.
    oes::common::etp12::Protocol::IDataArray::GetDataArrayMetadata_req_t getDataArrayMetadataRequest;
    getDataArrayMetadataRequest.m_dataArrays.emplace("0", dataArrraySource);
    const auto getDataArrayMetadataReply = rpcCall(&OpenKVDataArray::GetDataArrayMetadata, getDataArrayMetadataRequest);
    if (!getDataArrayMetadataReply.first.ok) {
        return getDataArrayMetadataReply.first;
    }

    // Put the DataArray target on the store initialized at the correct dimensions but without any meaning value
    oes::common::etp12::Protocol::IDataArray::PutUninitializedDataArrays_req_t putUninitializedDataArraysRequest;
    oes::common::etp12::Datatypes::DataArrayTypes::PutUninitializedDataArrayType pudt;
    pudt.m_uid = dataArrrayTarget;
    pudt.m_metadata = getDataArrayMetadataReply.second.m_arrayMetadata.at("0");
    putUninitializedDataArraysRequest.m_dataArrays.emplace("0", pudt);
    const auto putUninitializedDataArraysReply = rpcCall(&OpenKVDataArray::PutUninitializedDataArrays, putUninitializedDataArraysRequest);
    if (!putUninitializedDataArraysReply.first.ok) {
        return putUninitializedDataArraysReply.first;
    }

    // TODO : support more than 4 dimensions
    // Assume that m_preferredSubarrayDimensions is always given if the array is enough big to be chunked.

    // Get and put subArrays
    assert (pudt.m_metadata.m_dimensions.size() < 5);
    std::array<int64_t, 4> dims;
    std::array<int64_t, 4> preferredSubarrayDims;
    for (size_t i = 0; i < 4; ++i) {
        dims[i] = pudt.m_metadata.m_dimensions.size() > i
            ? pudt.m_metadata.m_dimensions[i]
            : 1;
        preferredSubarrayDims[i] = pudt.m_metadata.m_preferredSubarrayDimensions.size() > i
            ? pudt.m_metadata.m_preferredSubarrayDimensions[i]
            : dims[i];
    }
    std::array<int64_t, 4> indexStarts{0,0,0,0};
    std::array<int64_t, 4> indexCounts = preferredSubarrayDims;

    while (indexStarts[3] < dims[3]) {
        if (indexStarts[3] + indexCounts[3] > dims[3]) {
            indexCounts[3] = dims[3] - indexStarts[3];
        }
        while (indexStarts[2] < dims[2]) {
            if (indexStarts[2] + indexCounts[2] > dims[2]) {
                indexCounts[2] = dims[2] - indexStarts[2];
            }
            while (indexStarts[1] < dims[1]) {
                if (indexStarts[1] + indexCounts[1] > dims[1]) {
                    indexCounts[1] = dims[1] - indexStarts[1];
                }
                while (indexStarts[0] < dims[0]) {
                    if (indexStarts[0] + indexCounts[0] > dims[0]) {
                        indexCounts[0] = dims[0] - indexStarts[0];
                    }
                    oes::common::etp12::Protocol::IDataArray::GetDataSubarrays_req_t getDataSubarraysRequest;
                    oes::common::etp12::Datatypes::DataArrayTypes::GetDataSubarraysType gdst;
                    gdst.m_uid = dataArrraySource;
                    gdst.m_starts = std::vector<int64_t>(indexStarts.begin(), indexStarts.begin() + pudt.m_metadata.m_dimensions.size());
                    gdst.m_counts = std::vector<int64_t>(indexCounts.begin(), indexCounts.begin() + pudt.m_metadata.m_dimensions.size());
                    getDataSubarraysRequest.m_dataSubarrays.emplace("0", gdst);
                    const auto getDataSubarraysReply = rpcCall(&OpenKVDataArray::GetDataSubarrays, getDataSubarraysRequest);
                    if (!getDataSubarraysReply.first.ok) {
                        return getDataSubarraysReply.first;
                    }

                    oes::common::etp12::Protocol::IDataArray::PutDataSubarrays_req_t putDataSubarraysRequest;
                    oes::common::etp12::Datatypes::DataArrayTypes::PutDataSubarraysType pdst;
                    pdst.m_uid = dataArrrayTarget;
                    pdst.m_data = getDataSubarraysReply.second.m_dataSubarrays.begin()->second.m_data;
                    pdst.m_starts = std::vector<int64_t>(indexStarts.begin(), indexStarts.begin() + pudt.m_metadata.m_dimensions.size());
                    pdst.m_counts = std::vector<int64_t>(indexCounts.begin(), indexCounts.begin() + pudt.m_metadata.m_dimensions.size());
                    putDataSubarraysRequest.m_dataSubarrays.emplace("0", pdst);
                    const auto putDataSubarraysReply = rpcCall(&OpenKVDataArray::PutDataSubarrays, putDataSubarraysRequest);
                    if (!putDataSubarraysReply.first.ok) {
                        return putDataSubarraysReply.first;
                    }

                    indexStarts[0] += indexCounts[0];
                }
                indexStarts[0] = 0;
                indexCounts[0] = preferredSubarrayDims[0];
                indexStarts[1] += indexCounts[1];
            }
            indexStarts[1] = 0;
            indexCounts[1] = preferredSubarrayDims[1];
            indexStarts[2] += indexCounts[2];
        }
        indexStarts[2] = 0;
        indexCounts[2] = preferredSubarrayDims[2];
        indexStarts[3] += indexCounts[3];
    }

    oes::eml::etp12::Protocol::EtpResponseStatus response;
    response.ok = true;
    return response;
}

} // namespace oes::eml::openkv
