#pragma once
// WARNING: DO NOT EDIT: SharedComponents/src/bin/etpgencpp Generated File

// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

namespace oes::common::etp12::Datatypes {

ArrayOfBoolean::ArrayOfBoolean(
) : m_values(std::vector<bool>())
{
}

ArrayOfBytes::ArrayOfBytes(
) : m_values(std::vector<std::vector<uint8_t>>())
{
}

ArrayOfDouble::ArrayOfDouble(
) : m_values(std::vector<double>())
{
}

ArrayOfFloat::ArrayOfFloat(
) : m_values(std::vector<float>())
{
}

ArrayOfInt::ArrayOfInt(
) : m_values(std::vector<int32_t>())
{
}

ArrayOfLong::ArrayOfLong(
) : m_values(std::vector<int64_t>())
{
}

ArrayOfNullableBoolean::ArrayOfNullableBoolean(
) : m_values(std::vector<std::optional<bool>>())
{
}

ArrayOfNullableInt::ArrayOfNullableInt(
) : m_values(std::vector<std::optional<int32_t>>())
{
}

ArrayOfNullableLong::ArrayOfNullableLong(
) : m_values(std::vector<std::optional<int64_t>>())
{
}

ArrayOfString::ArrayOfString(
) : m_values(std::vector<std::string>())
{
}

Contact::Contact(
) : m_organizationName(std::string())
  , m_contactName(std::string())
  , m_contactPhone(std::string())
  , m_contactEmail(std::string())
{
}

ErrorInfo::ErrorInfo(
) : m_message(std::string())
  , m_code(int32_t())
{
}

MessageHeader::MessageHeader(
) : m_protocol(int32_t())
  , m_messageType(int32_t())
  , m_correlationId(int64_t())
  , m_messageId(int64_t())
  , m_messageFlags(int32_t())
{
}

Version::Version(
) : m_major(int32_t())
  , m_minor(int32_t())
  , m_revision(int32_t())
  , m_patch(int32_t())
{
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

ChannelChangeRequestInfo::ChannelChangeRequestInfo(
) : m_sinceChangeTime(int64_t())
  , m_channelIds(std::vector<int64_t>())
{
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

DataArrayIdentifier::DataArrayIdentifier(
) : m_uri(std::string())
  , m_pathInResource(std::string())
{
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

ObjectPart::ObjectPart(
) : m_uid(std::string())
  , m_data(std::vector<uint8_t>())
{
}

PutResponse::PutResponse(
) : m_createdContainedObjectUris(std::vector<std::string>())
  , m_deletedContainedObjectUris(std::vector<std::string>())
  , m_joinedContainedObjectUris(std::vector<std::string>())
  , m_unjoinedContainedObjectUris(std::vector<std::string>())
{
}

RemoteServer::RemoteServer(
) : m_sourcePartition(std::string())
  , m_sourceJwt(std::string())
  , m_sourceDataspacePath(std::string())
  , m_sourceServer(std::string())
{
}

TableData::TableData(
) : m_copyStatus(int32_t())
  , m_tableStatus(int32_t())
  , m_tableName(std::string())
  , m_tableData(std::vector<uint8_t>())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

AnyArray::AnyArray(
) : m_item(AnyArray_item_v())
{
}

AttributeMetadataRecord::AttributeMetadataRecord(
) : m_attributeId(int32_t())
  , m_attributeName(std::string())
  , m_dataKind(ChannelData::ChannelDataKind())
  , m_uom(std::string())
  , m_depthDatum(std::string())
  , m_attributePropertyKindUri(std::string())
  , m_axisVectorLengths(std::vector<int32_t>())
{
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

PassIndexedDepth::PassIndexedDepth(
) : m_pass(int64_t())
  , m_direction(PassDirection())
  , m_depth(double())
{
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

GetDataSubarraysType::GetDataSubarraysType(
) : m_uid(DataArrayIdentifier())
  , m_starts(std::vector<int64_t>())
  , m_counts(std::vector<int64_t>())
{
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

ContextInfo::ContextInfo(
) : m_uri(std::string())
  , m_depth(int32_t())
  , m_dataObjectTypes(std::vector<std::string>())
  , m_navigableEdges(RelationshipKind())
  , m_includeSecondaryTargets(bool())
  , m_includeSecondarySources(bool())
{
}

SupportedType::SupportedType(
) : m_dataObjectType(std::string())
  , m_objectCount(std::optional<int32_t>())
  , m_relationshipKind(RelationshipKind())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

AnySubarray::AnySubarray(
) : m_start(int64_t())
  , m_slice(AnyArray())
{
}

IndexValue::IndexValue(
) : m_item(IndexValue_item_v())
{
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::DataArrayTypes {

DataArray::DataArray(
) : m_dimensions(std::vector<int64_t>())
  , m_data(AnyArray())
{
}

PutDataSubarraysType::PutDataSubarraysType(
) : m_uid(DataArrayIdentifier())
  , m_data(AnyArray())
  , m_starts(std::vector<int64_t>())
  , m_counts(std::vector<int64_t>())
{
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

SubscriptionInfo::SubscriptionInfo(
) : m_context(ContextInfo())
  , m_scope(ContextScopeKind())
  , m_requestUuid(std::array<uint8_t, 16>())
  , m_includeObjectData(bool())
  , m_format(std::string())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

AnySparseArray::AnySparseArray(
) : m_slices(std::vector<AnySubarray>())
{
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

ChannelSubscribeInfo::ChannelSubscribeInfo(
) : m_channelId(int64_t())
  , m_startIndex(IndexValue())
  , m_dataChanges(bool())
  , m_requestLatestIndexCount(std::optional<int32_t>())
{
}

TruncateInfo::TruncateInfo(
) : m_channelId(int64_t())
  , m_newEndIndex(IndexValue())
{
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

IndexInterval::IndexInterval(
) : m_startIndex(IndexValue())
  , m_endIndex(IndexValue())
  , m_uom(std::string())
  , m_depthDatum(std::string())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

DataValue::DataValue(
) : m_item(DataValue_item_v())
{
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

ChannelRangeInfo::ChannelRangeInfo(
) : m_channelIds(std::vector<int64_t>())
  , m_interval(Object::IndexInterval())
  , m_secondaryIntervals(std::vector<Object::IndexInterval>())
{
}

IndexMetadataRecord::IndexMetadataRecord(
) : m_indexKind(ChannelIndexKind())
  , m_interval(Object::IndexInterval())
  , m_direction(IndexDirection())
  , m_name(std::string())
  , m_uom(std::string())
  , m_depthDatum(std::string())
  , m_indexPropertyKindUri(std::string())
  , m_filterable(bool())
{
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

ChangeAnnotation::ChangeAnnotation(
) : m_changeTime(int64_t())
  , m_interval(IndexInterval())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

DataAttribute::DataAttribute(
) : m_attributeId(int32_t())
  , m_attributeValue(DataValue())
{
}

MessageHeaderExtension::MessageHeaderExtension(
) : m_extension(std::map<std::string, DataValue>())
{
}

SupportedDataObject::SupportedDataObject(
) : m_qualifiedType(std::string())
  , m_dataObjectCapabilities(std::map<std::string, DataValue>())
{
}

SupportedProtocol::SupportedProtocol(
) : m_protocol(int32_t())
  , m_protocolVersion(Version())
  , m_role(std::string())
  , m_protocolCapabilities(std::map<std::string, DataValue>())
{
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

ChannelMetadataRecord::ChannelMetadataRecord(
) : m_uri(std::string())
  , m_id(int64_t())
  , m_indexes(std::vector<IndexMetadataRecord>())
  , m_channelName(std::string())
  , m_dataKind(ChannelDataKind())
  , m_uom(std::string())
  , m_depthDatum(std::string())
  , m_channelClassUri(std::string())
  , m_status(Object::ActiveStatusKind())
  , m_source(std::string())
  , m_axisVectorLengths(std::vector<int32_t>())
  , m_attributeMetadata(std::vector<AttributeMetadataRecord>())
  , m_customData(std::map<std::string, DataValue>())
{
}

FrameChannelMetadataRecord::FrameChannelMetadataRecord(
) : m_uri(std::string())
  , m_channelName(std::string())
  , m_dataKind(ChannelDataKind())
  , m_uom(std::string())
  , m_depthDatum(std::string())
  , m_channelPropertyKindUri(std::string())
  , m_status(Object::ActiveStatusKind())
  , m_source(std::string())
  , m_axisVectorLengths(std::vector<int32_t>())
  , m_attributeMetadata(std::vector<AttributeMetadataRecord>())
  , m_customData(std::map<std::string, DataValue>())
{
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

DataArrayMetadata::DataArrayMetadata(
) : m_dimensions(std::vector<int64_t>())
  , m_preferredSubarrayDimensions(std::vector<int64_t>())
  , m_transportArrayType(AnyArrayType())
  , m_logicalArrayType(AnyLogicalArrayType())
  , m_storeLastWrite(int64_t())
  , m_storeCreated(int64_t())
  , m_customData(std::map<std::string, DataValue>())
{
}

PutDataArraysType::PutDataArraysType(
) : m_uid(DataArrayIdentifier())
  , m_array(DataArray())
  , m_customData(std::map<std::string, DataValue>())
{
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

ChangeResponseInfo::ChangeResponseInfo(
) : m_responseTimestamp(int64_t())
  , m_changes(std::map<std::string, std::vector<ChangeAnnotation>>())
{
}

Dataspace::Dataspace(
) : m_uri(std::string())
  , m_path(std::string())
  , m_storeLastWrite(int64_t())
  , m_storeCreated(int64_t())
  , m_customData(std::map<std::string, DataValue>())
{
}

DeletedResource::DeletedResource(
) : m_uri(std::string())
  , m_deletedTime(int64_t())
  , m_customData(std::map<std::string, DataValue>())
{
}

Edge::Edge(
) : m_sourceUri(std::string())
  , m_targetUri(std::string())
  , m_relationshipKind(RelationshipKind())
  , m_customData(std::map<std::string, DataValue>())
{
}

PartsMetadataInfo::PartsMetadataInfo(
) : m_uri(std::string())
  , m_name(std::string())
  , m_index(ChannelData::IndexMetadataRecord())
  , m_customData(std::map<std::string, DataValue>())
{
}

Resource::Resource(
) : m_uri(std::string())
  , m_alternateUris(std::vector<std::string>())
  , m_name(std::string())
  , m_sourceCount(std::optional<int32_t>())
  , m_targetCount(std::optional<int32_t>())
  , m_lastChanged(int64_t())
  , m_storeLastWrite(int64_t())
  , m_storeCreated(int64_t())
  , m_activeStatus(ActiveStatusKind())
  , m_customData(std::map<std::string, DataValue>())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

ServerCapabilities::ServerCapabilities(
) : m_applicationName(std::string())
  , m_applicationVersion(std::string())
  , m_contactInformation(Contact())
  , m_supportedCompression(std::vector<std::string>())
  , m_supportedEncodings(std::vector<std::string>())
  , m_supportedFormats(std::vector<std::string>())
  , m_supportedDataObjects(std::vector<SupportedDataObject>())
  , m_supportedProtocols(std::vector<SupportedProtocol>())
  , m_endpointCapabilities(std::map<std::string, DataValue>())
{
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

DataItem::DataItem(
) : m_channelId(int64_t())
  , m_indexes(std::vector<IndexValue>())
  , m_value(DataValue())
  , m_valueAttributes(std::vector<DataAttribute>())
{
}

FramePoint::FramePoint(
) : m_value(DataValue())
  , m_valueAttributes(std::vector<DataAttribute>())
{
}

OpenChannelInfo::OpenChannelInfo(
) : m_metadata(ChannelMetadataRecord())
  , m_preferRealtime(bool())
  , m_dataChanges(bool())
{
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

PutUninitializedDataArrayType::PutUninitializedDataArrayType(
) : m_uid(DataArrayIdentifier())
  , m_metadata(DataArrayMetadata())
{
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

DataObject::DataObject(
) : m_resource(Resource())
  , m_format(std::string())
  , m_blobId(std::optional<std::array<uint8_t, 16>>())
  , m_data(std::vector<uint8_t>())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes::ChannelData {

FrameRow::FrameRow(
) : m_indexes(std::vector<IndexValue>())
  , m_points(std::vector<FramePoint>())
{
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

ObjectChange::ObjectChange(
) : m_changeKind(ObjectChangeKind())
  , m_changeTime(int64_t())
  , m_dataObject(DataObject())
{
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Protocol::Core {

Authorize::Authorize(
) : m_authorization(std::string())
  , m_supplementalAuthorization(std::map<std::string, std::string>())
{
}

AuthorizeResponse::AuthorizeResponse(
) : m_success(bool())
  , m_challenges(std::vector<std::string>())
{
}

CloseSession::CloseSession(
) : m_reason(std::string())
{
}

Ping::Ping(
) : m_currentDateTime(int64_t())
{
}

Pong::Pong(
) : m_currentDateTime(int64_t())
{
}

ProtocolException::ProtocolException(
) : m_error(std::optional<Datatypes::ErrorInfo>())
  , m_errors(std::map<std::string, Datatypes::ErrorInfo>())
{
}

OpenSession::OpenSession(
) : m_applicationName(std::string())
  , m_applicationVersion(std::string())
  , m_serverInstanceId(std::array<uint8_t, 16>())
  , m_supportedProtocols(std::vector<Datatypes::SupportedProtocol>())
  , m_supportedDataObjects(std::vector<Datatypes::SupportedDataObject>())
  , m_supportedCompression(std::string())
  , m_supportedFormats(std::vector<std::string>())
  , m_currentDateTime(int64_t())
  , m_earliestRetainedChangeTime(int64_t())
  , m_sessionId(std::array<uint8_t, 16>())
  , m_endpointCapabilities(std::map<std::string, Datatypes::DataValue>())
{
}

RequestSession::RequestSession(
) : m_applicationName(std::string())
  , m_applicationVersion(std::string())
  , m_clientInstanceId(std::array<uint8_t, 16>())
  , m_requestedProtocols(std::vector<Datatypes::SupportedProtocol>())
  , m_supportedDataObjects(std::vector<Datatypes::SupportedDataObject>())
  , m_supportedCompression(std::vector<std::string>())
  , m_supportedFormats(std::vector<std::string>())
  , m_currentDateTime(int64_t())
  , m_earliestRetainedChangeTime(int64_t())
  , m_serverAuthorizationRequired(bool())
  , m_endpointCapabilities(std::map<std::string, Datatypes::DataValue>())
{
}

} //      oes::common::etp12::Protocol::Core

namespace oes::common::etp12::Protocol::ChannelStreaming {

TruncateChannels::TruncateChannels(
) : m_channels(std::vector<Datatypes::ChannelData::TruncateInfo>())
{
}

ChannelMetadata::ChannelMetadata(
) : m_channels(std::vector<Datatypes::ChannelData::ChannelMetadataRecord>())
{
}

ChannelData::ChannelData(
) : m_data(std::vector<Datatypes::ChannelData::DataItem>())
{
}

} //      oes::common::etp12::Protocol::ChannelStreaming

namespace oes::common::etp12::Protocol::ChannelDataFrame {

GetFrameMetadata::GetFrameMetadata(
) : m_uri(std::string())
  , m_includeAllChannelSecondaryIndexes(bool())
{
}

CancelGetFrame::CancelGetFrame(
) : m_requestUuid(std::array<uint8_t, 16>())
{
}

GetFrame::GetFrame(
) : m_uri(std::string())
  , m_includeAllChannelSecondaryIndexes(bool())
  , m_requestedInterval(Datatypes::Object::IndexInterval())
  , m_requestUuid(std::array<uint8_t, 16>())
  , m_requestedSecondaryIntervals(std::vector<Datatypes::Object::IndexInterval>())
{
}

GetFrameResponseHeader::GetFrameResponseHeader(
) : m_channelUris(std::vector<std::string>())
  , m_indexes(std::vector<Datatypes::ChannelData::IndexMetadataRecord>())
{
}

GetFrameMetadataResponse::GetFrameMetadataResponse(
) : m_uri(std::string())
  , m_indexes(std::vector<Datatypes::ChannelData::IndexMetadataRecord>())
  , m_channels(std::vector<Datatypes::ChannelData::FrameChannelMetadataRecord>())
{
}

GetFrameResponseRows::GetFrameResponseRows(
) : m_frame(std::vector<Datatypes::ChannelData::FrameRow>())
{
}

} //      oes::common::etp12::Protocol::ChannelDataFrame

namespace oes::common::etp12::Protocol::Discovery {

GetDeletedResources::GetDeletedResources(
) : m_dataspaceUri(std::string())
  , m_deleteTimeFilter(std::optional<int64_t>())
  , m_dataObjectTypes(std::vector<std::string>())
{
}

GetResources::GetResources(
) : m_context(Datatypes::Object::ContextInfo())
  , m_scope(Datatypes::Object::ContextScopeKind())
  , m_countObjects(bool())
  , m_storeLastWriteFilter(std::optional<int64_t>())
  , m_activeStatusFilter(std::optional<Datatypes::Object::ActiveStatusKind>())
  , m_includeEdges(bool())
{
}

GetDeletedResourcesResponse::GetDeletedResourcesResponse(
) : m_deletedResources(std::vector<Datatypes::Object::DeletedResource>())
{
}

GetResourcesEdgesResponse::GetResourcesEdgesResponse(
) : m_edges(std::vector<Datatypes::Object::Edge>())
{
}

GetResourcesResponse::GetResourcesResponse(
) : m_resources(std::vector<Datatypes::Object::Resource>())
{
}

} //      oes::common::etp12::Protocol::Discovery

namespace oes::common::etp12::Protocol::Store {

DeleteDataObjects::DeleteDataObjects(
) : m_uris(std::map<std::string, std::string>())
  , m_pruneContainedObjects(bool())
{
}

GetDataObjects::GetDataObjects(
) : m_uris(std::map<std::string, std::string>())
  , m_format(std::string())
{
}

Chunk::Chunk(
) : m_blobId(std::array<uint8_t, 16>())
  , m_data(std::vector<uint8_t>())
  , m_final(bool())
{
}

DeleteDataObjectsResponse::DeleteDataObjectsResponse(
) : m_deletedUris(std::map<std::string, Datatypes::ArrayOfString>())
{
}

PutDataObjectsResponse::PutDataObjectsResponse(
) : m_success(std::map<std::string, Datatypes::Object::PutResponse>())
{
}

GetDataObjectsResponse::GetDataObjectsResponse(
) : m_dataObjects(std::map<std::string, Datatypes::Object::DataObject>())
{
}

PutDataObjects::PutDataObjects(
) : m_dataObjects(std::map<std::string, Datatypes::Object::DataObject>())
  , m_pruneContainedObjects(bool())
{
}

} //      oes::common::etp12::Protocol::Store

namespace oes::common::etp12::Protocol::StoreNotification {

SubscribeNotificationsResponse::SubscribeNotificationsResponse(
) : m_success(std::map<std::string, std::string>())
{
}

Chunk::Chunk(
) : m_blobId(std::array<uint8_t, 16>())
  , m_data(std::vector<uint8_t>())
  , m_final(bool())
{
}

ObjectAccessRevoked::ObjectAccessRevoked(
) : m_uri(std::string())
  , m_changeTime(int64_t())
  , m_requestUuid(std::array<uint8_t, 16>())
{
}

ObjectDeleted::ObjectDeleted(
) : m_uri(std::string())
  , m_changeTime(int64_t())
  , m_requestUuid(std::array<uint8_t, 16>())
{
}

SubscriptionEnded::SubscriptionEnded(
) : m_reason(std::string())
  , m_requestUuid(std::array<uint8_t, 16>())
{
}

UnsubscribeNotifications::UnsubscribeNotifications(
) : m_requestUuid(std::array<uint8_t, 16>())
{
}

SubscribeNotifications::SubscribeNotifications(
) : m_request(std::map<std::string, Datatypes::Object::SubscriptionInfo>())
{
}

UnsolicitedStoreNotifications::UnsolicitedStoreNotifications(
) : m_subscriptions(std::vector<Datatypes::Object::SubscriptionInfo>())
{
}

ObjectActiveStatusChanged::ObjectActiveStatusChanged(
) : m_activeStatus(Datatypes::Object::ActiveStatusKind())
  , m_changeTime(int64_t())
  , m_resource(Datatypes::Object::Resource())
  , m_requestUuid(std::array<uint8_t, 16>())
{
}

ObjectChanged::ObjectChanged(
) : m_change(Datatypes::Object::ObjectChange())
  , m_requestUuid(std::array<uint8_t, 16>())
{
}

} //      oes::common::etp12::Protocol::StoreNotification

namespace oes::common::etp12::Protocol::GrowingObject {

DeleteParts::DeleteParts(
) : m_uri(std::string())
  , m_uids(std::map<std::string, std::string>())
{
}

DeletePartsResponse::DeletePartsResponse(
) : m_success(std::map<std::string, std::string>())
{
}

GetChangeAnnotations::GetChangeAnnotations(
) : m_sinceChangeTime(int64_t())
  , m_uris(std::map<std::string, std::string>())
  , m_latestOnly(bool())
{
}

GetGrowingDataObjectsHeader::GetGrowingDataObjectsHeader(
) : m_uris(std::map<std::string, std::string>())
  , m_format(std::string())
{
}

GetParts::GetParts(
) : m_uri(std::string())
  , m_format(std::string())
  , m_uids(std::map<std::string, std::string>())
{
}

GetPartsMetadata::GetPartsMetadata(
) : m_uris(std::map<std::string, std::string>())
{
}

PutGrowingDataObjectsHeaderResponse::PutGrowingDataObjectsHeaderResponse(
) : m_success(std::map<std::string, std::string>())
{
}

PutPartsResponse::PutPartsResponse(
) : m_success(std::map<std::string, std::string>())
{
}

GetPartsByRangeResponse::GetPartsByRangeResponse(
) : m_uri(std::string())
  , m_format(std::string())
  , m_parts(std::vector<Datatypes::Object::ObjectPart>())
{
}

GetPartsResponse::GetPartsResponse(
) : m_uri(std::string())
  , m_format(std::string())
  , m_parts(std::map<std::string, Datatypes::Object::ObjectPart>())
{
}

PutParts::PutParts(
) : m_uri(std::string())
  , m_format(std::string())
  , m_parts(std::map<std::string, Datatypes::Object::ObjectPart>())
{
}

GetPartsByRange::GetPartsByRange(
) : m_uri(std::string())
  , m_format(std::string())
  , m_indexInterval(Datatypes::Object::IndexInterval())
  , m_includeOverlappingIntervals(bool())
{
}

ReplacePartsByRange::ReplacePartsByRange(
) : m_uri(std::string())
  , m_deleteInterval(Datatypes::Object::IndexInterval())
  , m_includeOverlappingIntervals(bool())
  , m_format(std::string())
  , m_parts(std::vector<Datatypes::Object::ObjectPart>())
{
}

GetChangeAnnotationsResponse::GetChangeAnnotationsResponse(
) : m_changes(std::map<std::string, Datatypes::Object::ChangeResponseInfo>())
{
}

GetPartsMetadataResponse::GetPartsMetadataResponse(
) : m_metadata(std::map<std::string, Datatypes::Object::PartsMetadataInfo>())
{
}

GetGrowingDataObjectsHeaderResponse::GetGrowingDataObjectsHeaderResponse(
) : m_dataObjects(std::map<std::string, Datatypes::Object::DataObject>())
{
}

PutGrowingDataObjectsHeader::PutGrowingDataObjectsHeader(
) : m_dataObjects(std::map<std::string, Datatypes::Object::DataObject>())
{
}

} //      oes::common::etp12::Protocol::GrowingObject

namespace oes::common::etp12::Protocol::GrowingObjectNotification {

SubscribePartNotificationsResponse::SubscribePartNotificationsResponse(
) : m_success(std::map<std::string, std::string>())
{
}

PartSubscriptionEnded::PartSubscriptionEnded(
) : m_reason(std::string())
  , m_requestUuid(std::array<uint8_t, 16>())
{
}

PartsChanged::PartsChanged(
) : m_uri(std::string())
  , m_requestUuid(std::array<uint8_t, 16>())
  , m_changeKind(Datatypes::Object::ObjectChangeKind())
  , m_changeTime(int64_t())
  , m_format(std::string())
  , m_parts(std::vector<Datatypes::Object::ObjectPart>())
{
}

PartsDeleted::PartsDeleted(
) : m_uri(std::string())
  , m_requestUuid(std::array<uint8_t, 16>())
  , m_changeTime(int64_t())
  , m_uids(std::vector<std::string>())
{
}

UnsubscribePartNotification::UnsubscribePartNotification(
) : m_requestUuid(std::array<uint8_t, 16>())
{
}

SubscribePartNotifications::SubscribePartNotifications(
) : m_request(std::map<std::string, Datatypes::Object::SubscriptionInfo>())
{
}

UnsolicitedPartNotifications::UnsolicitedPartNotifications(
) : m_subscriptions(std::vector<Datatypes::Object::SubscriptionInfo>())
{
}

PartsReplacedByRange::PartsReplacedByRange(
) : m_uri(std::string())
  , m_requestUuid(std::array<uint8_t, 16>())
  , m_changeTime(int64_t())
  , m_deletedInterval(Datatypes::Object::IndexInterval())
  , m_includeOverlappingIntervals(bool())
  , m_format(std::string())
  , m_parts(std::vector<Datatypes::Object::ObjectPart>())
{
}

} //      oes::common::etp12::Protocol::GrowingObjectNotification

namespace oes::common::etp12::Protocol::DataArray {

PutDataArraysResponse::PutDataArraysResponse(
) : m_success(std::map<std::string, std::string>())
{
}

PutDataSubarraysResponse::PutDataSubarraysResponse(
) : m_success(std::map<std::string, std::string>())
{
}

PutUninitializedDataArraysResponse::PutUninitializedDataArraysResponse(
) : m_success(std::map<std::string, std::string>())
{
}

GetDataArrayMetadata::GetDataArrayMetadata(
) : m_dataArrays(std::map<std::string, Datatypes::DataArrayTypes::DataArrayIdentifier>())
{
}

GetDataArrays::GetDataArrays(
) : m_dataArrays(std::map<std::string, Datatypes::DataArrayTypes::DataArrayIdentifier>())
{
}

GetDataSubarrays::GetDataSubarrays(
) : m_dataSubarrays(std::map<std::string, Datatypes::DataArrayTypes::GetDataSubarraysType>())
{
}

GetDataArraysResponse::GetDataArraysResponse(
) : m_dataArrays(std::map<std::string, Datatypes::DataArrayTypes::DataArray>())
{
}

GetDataSubarraysResponse::GetDataSubarraysResponse(
) : m_dataSubarrays(std::map<std::string, Datatypes::DataArrayTypes::DataArray>())
{
}

PutDataSubarrays::PutDataSubarrays(
) : m_dataSubarrays(std::map<std::string, Datatypes::DataArrayTypes::PutDataSubarraysType>())
{
}

GetDataArrayMetadataResponse::GetDataArrayMetadataResponse(
) : m_arrayMetadata(std::map<std::string, Datatypes::DataArrayTypes::DataArrayMetadata>())
{
}

PutDataArrays::PutDataArrays(
) : m_dataArrays(std::map<std::string, Datatypes::DataArrayTypes::PutDataArraysType>())
{
}

PutUninitializedDataArrays::PutUninitializedDataArrays(
) : m_dataArrays(std::map<std::string, Datatypes::DataArrayTypes::PutUninitializedDataArrayType>())
{
}

} //      oes::common::etp12::Protocol::DataArray

namespace oes::common::etp12::Protocol::DiscoveryQuery {

FindResources::FindResources(
) : m_context(Datatypes::Object::ContextInfo())
  , m_scope(Datatypes::Object::ContextScopeKind())
  , m_storeLastWriteFilter(std::optional<int64_t>())
  , m_activeStatusFilter(std::optional<Datatypes::Object::ActiveStatusKind>())
{
}

FindResourcesResponse::FindResourcesResponse(
) : m_resources(std::vector<Datatypes::Object::Resource>())
  , m_serverSortOrder(std::string())
{
}

} //      oes::common::etp12::Protocol::DiscoveryQuery

namespace oes::common::etp12::Protocol::StoreQuery {

Chunk::Chunk(
) : m_blobId(std::array<uint8_t, 16>())
  , m_data(std::vector<uint8_t>())
  , m_final(bool())
{
}

FindDataObjects::FindDataObjects(
) : m_context(Datatypes::Object::ContextInfo())
  , m_scope(Datatypes::Object::ContextScopeKind())
  , m_storeLastWriteFilter(std::optional<int64_t>())
  , m_activeStatusFilter(std::optional<Datatypes::Object::ActiveStatusKind>())
  , m_format(std::string())
{
}

FindDataObjectsResponse::FindDataObjectsResponse(
) : m_dataObjects(std::vector<Datatypes::Object::DataObject>())
  , m_serverSortOrder(std::string())
{
}

} //      oes::common::etp12::Protocol::StoreQuery

namespace oes::common::etp12::Protocol::GrowingObjectQuery {

FindParts::FindParts(
) : m_uri(std::string())
  , m_format(std::string())
{
}

FindPartsResponse::FindPartsResponse(
) : m_uri(std::string())
  , m_serverSortOrder(std::string())
  , m_format(std::string())
  , m_parts(std::vector<Datatypes::Object::ObjectPart>())
{
}

} //      oes::common::etp12::Protocol::GrowingObjectQuery

namespace oes::common::etp12::Protocol::Transaction {

StartTransaction::StartTransaction(
) : m_readOnly(bool())
  , m_message(std::string())
  , m_dataspaceUris(std::vector<std::string>())
{
}

CommitTransaction::CommitTransaction(
) : m_transactionUuid(std::array<uint8_t, 16>())
{
}

CommitTransactionResponse::CommitTransactionResponse(
) : m_transactionUuid(std::array<uint8_t, 16>())
  , m_successful(bool())
  , m_failureReason(std::string())
{
}

RollbackTransaction::RollbackTransaction(
) : m_transactionUuid(std::array<uint8_t, 16>())
{
}

RollbackTransactionResponse::RollbackTransactionResponse(
) : m_transactionUuid(std::array<uint8_t, 16>())
  , m_successful(bool())
  , m_failureReason(std::string())
{
}

StartTransactionResponse::StartTransactionResponse(
) : m_transactionUuid(std::array<uint8_t, 16>())
  , m_successful(bool())
  , m_failureReason(std::string())
{
}

} //      oes::common::etp12::Protocol::Transaction

namespace oes::common::etp12::Protocol::ChannelSubscribe {

GetChannelMetadata::GetChannelMetadata(
) : m_uris(std::map<std::string, std::string>())
{
}

SubscribeChannelsResponse::SubscribeChannelsResponse(
) : m_success(std::map<std::string, std::string>())
{
}

SubscriptionsStopped::SubscriptionsStopped(
) : m_reason(std::string())
  , m_channelIds(std::map<std::string, int64_t>())
{
}

UnsubscribeChannels::UnsubscribeChannels(
) : m_channelIds(std::map<std::string, int64_t>())
{
}

CancelGetRanges::CancelGetRanges(
) : m_requestUuid(std::array<uint8_t, 16>())
{
}

GetChangeAnnotations::GetChangeAnnotations(
) : m_channels(std::map<std::string, Datatypes::ChannelData::ChannelChangeRequestInfo>())
  , m_latestOnly(bool())
{
}

ChannelsTruncated::ChannelsTruncated(
) : m_channels(std::vector<Datatypes::ChannelData::TruncateInfo>())
  , m_changeTime(int64_t())
{
}

SubscribeChannels::SubscribeChannels(
) : m_channels(std::map<std::string, Datatypes::ChannelData::ChannelSubscribeInfo>())
{
}

GetRanges::GetRanges(
) : m_requestUuid(std::array<uint8_t, 16>())
  , m_channelRanges(std::vector<Datatypes::ChannelData::ChannelRangeInfo>())
{
}

GetChangeAnnotationsResponse::GetChangeAnnotationsResponse(
) : m_changes(std::map<std::string, Datatypes::Object::ChangeResponseInfo>())
{
}

GetChannelMetadataResponse::GetChannelMetadataResponse(
) : m_metadata(std::map<std::string, Datatypes::ChannelData::ChannelMetadataRecord>())
{
}

ChannelData::ChannelData(
) : m_data(std::vector<Datatypes::ChannelData::DataItem>())
{
}

GetRangesResponse::GetRangesResponse(
) : m_data(std::vector<Datatypes::ChannelData::DataItem>())
{
}

RangeReplaced::RangeReplaced(
) : m_changeTime(int64_t())
  , m_channelIds(std::vector<int64_t>())
  , m_changedInterval(Datatypes::Object::IndexInterval())
  , m_data(std::vector<Datatypes::ChannelData::DataItem>())
{
}

} //      oes::common::etp12::Protocol::ChannelSubscribe

namespace oes::common::etp12::Protocol::ChannelDataLoad {

ChannelsClosed::ChannelsClosed(
) : m_reason(std::string())
  , m_id(std::map<std::string, int64_t>())
{
}

CloseChannels::CloseChannels(
) : m_id(std::map<std::string, int64_t>())
{
}

OpenChannels::OpenChannels(
) : m_uris(std::map<std::string, std::string>())
{
}

ReplaceRangeResponse::ReplaceRangeResponse(
) : m_channelChangeTime(int64_t())
{
}

TruncateChannelsResponse::TruncateChannelsResponse(
) : m_channelsTruncatedTime(std::map<std::string, int64_t>())
{
}

TruncateChannels::TruncateChannels(
) : m_channels(std::map<std::string, Datatypes::ChannelData::TruncateInfo>())
{
}

ChannelData::ChannelData(
) : m_data(std::vector<Datatypes::ChannelData::DataItem>())
{
}

OpenChannelsResponse::OpenChannelsResponse(
) : m_channels(std::map<std::string, Datatypes::ChannelData::OpenChannelInfo>())
{
}

ReplaceRange::ReplaceRange(
) : m_changedInterval(Datatypes::Object::IndexInterval())
  , m_channelIds(std::vector<int64_t>())
  , m_data(std::vector<Datatypes::ChannelData::DataItem>())
{
}

} //      oes::common::etp12::Protocol::ChannelDataLoad

namespace oes::common::etp12::Protocol::Dataspace {

DeleteDataspaces::DeleteDataspaces(
) : m_uris(std::map<std::string, std::string>())
{
}

DeleteDataspacesResponse::DeleteDataspacesResponse(
) : m_success(std::map<std::string, std::string>())
{
}

GetDataspaces::GetDataspaces(
) : m_storeLastWriteFilter(std::optional<int64_t>())
{
}

PutDataspacesResponse::PutDataspacesResponse(
) : m_success(std::map<std::string, std::string>())
{
}

GetDataspacesResponse::GetDataspacesResponse(
) : m_dataspaces(std::vector<Datatypes::Object::Dataspace>())
{
}

PutDataspaces::PutDataspaces(
) : m_dataspaces(std::map<std::string, Datatypes::Object::Dataspace>())
{
}

} //      oes::common::etp12::Protocol::Dataspace

namespace oes::common::etp12::Protocol::SupportedTypes {

GetSupportedTypes::GetSupportedTypes(
) : m_uri(std::string())
  , m_scope(Datatypes::Object::ContextScopeKind())
  , m_returnEmptyTypes(bool())
  , m_countObjects(bool())
{
}

GetSupportedTypesResponse::GetSupportedTypesResponse(
) : m_supportedTypes(std::vector<Datatypes::Object::SupportedType>())
{
}

} //      oes::common::etp12::Protocol::SupportedTypes

namespace oes::common::etp12::Protocol::CoreOSDU {

ResumeSession::ResumeSession(
) : m_applicationName(std::string())
  , m_applicationVersion(std::string())
  , m_clientInstanceId(std::array<uint8_t, 16>())
  , m_currentDateTime(int64_t())
  , m_sessionId(std::array<uint8_t, 16>())
{
}

ResumeSessionResponse::ResumeSessionResponse(
) : m_applicationName(std::string())
  , m_applicationVersion(std::string())
  , m_serverInstanceId(std::array<uint8_t, 16>())
  , m_currentDateTime(int64_t())
  , m_sessionId(std::array<uint8_t, 16>())
  , m_earliestRetainedChangeTime(int64_t())
{
}

} //      oes::common::etp12::Protocol::CoreOSDU

namespace oes::common::etp12::Protocol::StoreOSDU {

CopyDataObjectsByValue::CopyDataObjectsByValue(
) : m_uri(std::string())
  , m_sourcesDepth(int32_t())
  , m_dataObjectTypes(std::vector<std::string>())
{
}

CopyDataObjectsByValueResponse::CopyDataObjectsByValueResponse(
) : m_copiedDataObjects(std::vector<std::string>())
{
}

} //      oes::common::etp12::Protocol::StoreOSDU

namespace oes::common::etp12::Protocol::DataspaceOSDU {

CopyDataspacesContent::CopyDataspacesContent(
) : m_dataspaces(std::map<std::string, std::string>())
  , m_targetDataspace(std::string())
{
}

CopyDataspacesContentResponse::CopyDataspacesContentResponse(
) : m_success(std::map<std::string, std::string>())
{
}

CopyToDataspace::CopyToDataspace(
) : m_uris(std::map<std::string, std::string>())
  , m_dataspaceUri(std::string())
{
}

CopyToDataspaceResponse::CopyToDataspaceResponse(
) : m_success(std::map<std::string, std::string>())
{
}

GetDataspaceInfo::GetDataspaceInfo(
) : m_uris(std::map<std::string, std::string>())
{
}

LockDataspaces::LockDataspaces(
) : m_uris(std::map<std::string, std::string>())
  , m_lock(bool())
{
}

LockDataspacesResponse::LockDataspacesResponse(
) : m_success(std::map<std::string, std::string>())
{
}

CopyDataspaceFromRemote::CopyDataspaceFromRemote(
) : m_action(std::string())
  , m_dataspacePath(std::string())
  , m_sourceServer(Datatypes::Object::RemoteServer())
  , m_extraData(std::map<std::string, std::string>())
{
}

CopyDataspaceFromRemoteResponse::CopyDataspaceFromRemoteResponse(
) : m_tableData(std::vector<Datatypes::Object::TableData>())
{
}

GetDataspaceInfoResponse::GetDataspaceInfoResponse(
) : m_dataspaces(std::map<std::string, Datatypes::Object::Dataspace>())
{
}

} //      oes::common::etp12::Protocol::DataspaceOSDU

namespace oes::common::etp12::Datatypes {

avro::ValidSchema ArrayOfBoolean::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfBoolean"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfBoolean::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfBoolean::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfBytes::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfBytes"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfBytes::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfBytes::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfDouble::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfDouble"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfDouble::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfDouble::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfFloat::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfFloat"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfFloat::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfFloat::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfInt::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfInt"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfInt::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfInt::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfLong::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfLong"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfLong::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfLong::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfNullableBoolean::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfNullableBoolean"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfNullableBoolean::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfNullableBoolean::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfNullableInt::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfNullableInt"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfNullableInt::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfNullableInt::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfNullableLong::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfNullableLong"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfNullableLong::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfNullableLong::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema ArrayOfString::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ArrayOfString"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ArrayOfString::encode(avro::Encoder& e) const {
    avro::encode(e, m_values);
}
void ArrayOfString::decode(avro::Decoder& d) {
    avro::decode(d, m_values);
}

avro::ValidSchema Contact::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Contact"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Contact::encode(avro::Encoder& e) const {
    avro::encode(e, m_organizationName);
    avro::encode(e, m_contactName);
    avro::encode(e, m_contactPhone);
    avro::encode(e, m_contactEmail);
}
void Contact::decode(avro::Decoder& d) {
    avro::decode(d, m_organizationName);
    avro::decode(d, m_contactName);
    avro::decode(d, m_contactPhone);
    avro::decode(d, m_contactEmail);
}

avro::ValidSchema ErrorInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ErrorInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ErrorInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_message);
    avro::encode(e, m_code);
}
void ErrorInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_message);
    avro::decode(d, m_code);
}

avro::ValidSchema MessageHeader::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.MessageHeader"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void MessageHeader::encode(avro::Encoder& e) const {
    avro::encode(e, m_protocol);
    avro::encode(e, m_messageType);
    avro::encode(e, m_correlationId);
    avro::encode(e, m_messageId);
    avro::encode(e, m_messageFlags);
}
void MessageHeader::decode(avro::Decoder& d) {
    avro::decode(d, m_protocol);
    avro::decode(d, m_messageType);
    avro::decode(d, m_correlationId);
    avro::decode(d, m_messageId);
    avro::decode(d, m_messageFlags);
}

avro::ValidSchema Version::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Version"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Version::encode(avro::Encoder& e) const {
    avro::encode(e, m_major);
    avro::encode(e, m_minor);
    avro::encode(e, m_revision);
    avro::encode(e, m_patch);
}
void Version::decode(avro::Decoder& d) {
    avro::decode(d, m_major);
    avro::decode(d, m_minor);
    avro::decode(d, m_revision);
    avro::decode(d, m_patch);
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

avro::ValidSchema ChannelChangeRequestInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.ChannelChangeRequestInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelChangeRequestInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_sinceChangeTime);
    avro::encode(e, m_channelIds);
}
void ChannelChangeRequestInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_sinceChangeTime);
    avro::decode(d, m_channelIds);
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

avro::ValidSchema DataArrayIdentifier::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataArrayTypes.DataArrayIdentifier"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DataArrayIdentifier::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_pathInResource);
}
void DataArrayIdentifier::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_pathInResource);
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema ObjectPart::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.ObjectPart"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ObjectPart::encode(avro::Encoder& e) const {
    avro::encode(e, m_uid);
    avro::encode(e, m_data);
}
void ObjectPart::decode(avro::Decoder& d) {
    avro::decode(d, m_uid);
    avro::decode(d, m_data);
}

avro::ValidSchema PutResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.PutResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_createdContainedObjectUris);
    avro::encode(e, m_deletedContainedObjectUris);
    avro::encode(e, m_joinedContainedObjectUris);
    avro::encode(e, m_unjoinedContainedObjectUris);
}
void PutResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_createdContainedObjectUris);
    avro::decode(d, m_deletedContainedObjectUris);
    avro::decode(d, m_joinedContainedObjectUris);
    avro::decode(d, m_unjoinedContainedObjectUris);
}

avro::ValidSchema RemoteServer::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.RemoteServer"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void RemoteServer::encode(avro::Encoder& e) const {
    avro::encode(e, m_sourcePartition);
    avro::encode(e, m_sourceJwt);
    avro::encode(e, m_sourceDataspacePath);
    avro::encode(e, m_sourceServer);
}
void RemoteServer::decode(avro::Decoder& d) {
    avro::decode(d, m_sourcePartition);
    avro::decode(d, m_sourceJwt);
    avro::decode(d, m_sourceDataspacePath);
    avro::decode(d, m_sourceServer);
}

avro::ValidSchema TableData::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.TableData"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void TableData::encode(avro::Encoder& e) const {
    avro::encode(e, m_copyStatus);
    avro::encode(e, m_tableStatus);
    avro::encode(e, m_tableName);
    avro::encode(e, m_tableData);
}
void TableData::decode(avro::Decoder& d) {
    avro::decode(d, m_copyStatus);
    avro::decode(d, m_tableStatus);
    avro::decode(d, m_tableName);
    avro::decode(d, m_tableData);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

avro::ValidSchema AnyArray::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.AnyArray"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void AnyArray::encode(avro::Encoder& e) const {
    avro::encode(e, m_item);
}
void AnyArray::decode(avro::Decoder& d) {
    avro::decode(d, m_item);
}

avro::ValidSchema AttributeMetadataRecord::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.AttributeMetadataRecord"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void AttributeMetadataRecord::encode(avro::Encoder& e) const {
    avro::encode(e, m_attributeId);
    avro::encode(e, m_attributeName);
    avro::encode(e, m_dataKind);
    avro::encode(e, m_uom);
    avro::encode(e, m_depthDatum);
    avro::encode(e, m_attributePropertyKindUri);
    avro::encode(e, m_axisVectorLengths);
}
void AttributeMetadataRecord::decode(avro::Decoder& d) {
    avro::decode(d, m_attributeId);
    avro::decode(d, m_attributeName);
    avro::decode(d, m_dataKind);
    avro::decode(d, m_uom);
    avro::decode(d, m_depthDatum);
    avro::decode(d, m_attributePropertyKindUri);
    avro::decode(d, m_axisVectorLengths);
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

avro::ValidSchema PassIndexedDepth::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.PassIndexedDepth"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PassIndexedDepth::encode(avro::Encoder& e) const {
    avro::encode(e, m_pass);
    avro::encode(e, m_direction);
    avro::encode(e, m_depth);
}
void PassIndexedDepth::decode(avro::Decoder& d) {
    avro::decode(d, m_pass);
    avro::decode(d, m_direction);
    avro::decode(d, m_depth);
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

avro::ValidSchema GetDataSubarraysType::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataArrayTypes.GetDataSubarraysType"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataSubarraysType::encode(avro::Encoder& e) const {
    avro::encode(e, m_uid);
    avro::encode(e, m_starts);
    avro::encode(e, m_counts);
}
void GetDataSubarraysType::decode(avro::Decoder& d) {
    avro::decode(d, m_uid);
    avro::decode(d, m_starts);
    avro::decode(d, m_counts);
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema ContextInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.ContextInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ContextInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_depth);
    avro::encode(e, m_dataObjectTypes);
    avro::encode(e, m_navigableEdges);
    avro::encode(e, m_includeSecondaryTargets);
    avro::encode(e, m_includeSecondarySources);
}
void ContextInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_depth);
    avro::decode(d, m_dataObjectTypes);
    avro::decode(d, m_navigableEdges);
    avro::decode(d, m_includeSecondaryTargets);
    avro::decode(d, m_includeSecondarySources);
}

avro::ValidSchema SupportedType::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.SupportedType"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SupportedType::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataObjectType);
    avro::encode(e, m_objectCount);
    avro::encode(e, m_relationshipKind);
}
void SupportedType::decode(avro::Decoder& d) {
    avro::decode(d, m_dataObjectType);
    avro::decode(d, m_objectCount);
    avro::decode(d, m_relationshipKind);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

avro::ValidSchema AnySubarray::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.AnySubarray"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void AnySubarray::encode(avro::Encoder& e) const {
    avro::encode(e, m_start);
    avro::encode(e, m_slice);
}
void AnySubarray::decode(avro::Decoder& d) {
    avro::decode(d, m_start);
    avro::decode(d, m_slice);
}

avro::ValidSchema IndexValue::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.IndexValue"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void IndexValue::encode(avro::Encoder& e) const {
    avro::encode(e, m_item);
}
void IndexValue::decode(avro::Decoder& d) {
    avro::decode(d, m_item);
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::DataArrayTypes {

avro::ValidSchema DataArray::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataArrayTypes.DataArray"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DataArray::encode(avro::Encoder& e) const {
    avro::encode(e, m_dimensions);
    avro::encode(e, m_data);
}
void DataArray::decode(avro::Decoder& d) {
    avro::decode(d, m_dimensions);
    avro::decode(d, m_data);
}

avro::ValidSchema PutDataSubarraysType::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataArrayTypes.PutDataSubarraysType"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataSubarraysType::encode(avro::Encoder& e) const {
    avro::encode(e, m_uid);
    avro::encode(e, m_data);
    avro::encode(e, m_starts);
    avro::encode(e, m_counts);
}
void PutDataSubarraysType::decode(avro::Decoder& d) {
    avro::decode(d, m_uid);
    avro::decode(d, m_data);
    avro::decode(d, m_starts);
    avro::decode(d, m_counts);
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema SubscriptionInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.SubscriptionInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscriptionInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_context);
    avro::encode(e, m_scope);
    avro::encode(e, m_requestUuid);
    avro::encode(e, m_includeObjectData);
    avro::encode(e, m_format);
}
void SubscriptionInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_context);
    avro::decode(d, m_scope);
    avro::decode(d, m_requestUuid);
    avro::decode(d, m_includeObjectData);
    avro::decode(d, m_format);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

avro::ValidSchema AnySparseArray::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.AnySparseArray"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void AnySparseArray::encode(avro::Encoder& e) const {
    avro::encode(e, m_slices);
}
void AnySparseArray::decode(avro::Decoder& d) {
    avro::decode(d, m_slices);
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

avro::ValidSchema ChannelSubscribeInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.ChannelSubscribeInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelSubscribeInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelId);
    avro::encode(e, m_startIndex);
    avro::encode(e, m_dataChanges);
    avro::encode(e, m_requestLatestIndexCount);
}
void ChannelSubscribeInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_channelId);
    avro::decode(d, m_startIndex);
    avro::decode(d, m_dataChanges);
    avro::decode(d, m_requestLatestIndexCount);
}

avro::ValidSchema TruncateInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.TruncateInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void TruncateInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelId);
    avro::encode(e, m_newEndIndex);
}
void TruncateInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_channelId);
    avro::decode(d, m_newEndIndex);
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema IndexInterval::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.IndexInterval"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void IndexInterval::encode(avro::Encoder& e) const {
    avro::encode(e, m_startIndex);
    avro::encode(e, m_endIndex);
    avro::encode(e, m_uom);
    avro::encode(e, m_depthDatum);
}
void IndexInterval::decode(avro::Decoder& d) {
    avro::decode(d, m_startIndex);
    avro::decode(d, m_endIndex);
    avro::decode(d, m_uom);
    avro::decode(d, m_depthDatum);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

avro::ValidSchema DataValue::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataValue"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DataValue::encode(avro::Encoder& e) const {
    avro::encode(e, m_item);
}
void DataValue::decode(avro::Decoder& d) {
    avro::decode(d, m_item);
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

avro::ValidSchema ChannelRangeInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.ChannelRangeInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelRangeInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelIds);
    avro::encode(e, m_interval);
    avro::encode(e, m_secondaryIntervals);
}
void ChannelRangeInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_channelIds);
    avro::decode(d, m_interval);
    avro::decode(d, m_secondaryIntervals);
}

avro::ValidSchema IndexMetadataRecord::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.IndexMetadataRecord"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void IndexMetadataRecord::encode(avro::Encoder& e) const {
    avro::encode(e, m_indexKind);
    avro::encode(e, m_interval);
    avro::encode(e, m_direction);
    avro::encode(e, m_name);
    avro::encode(e, m_uom);
    avro::encode(e, m_depthDatum);
    avro::encode(e, m_indexPropertyKindUri);
    avro::encode(e, m_filterable);
}
void IndexMetadataRecord::decode(avro::Decoder& d) {
    avro::decode(d, m_indexKind);
    avro::decode(d, m_interval);
    avro::decode(d, m_direction);
    avro::decode(d, m_name);
    avro::decode(d, m_uom);
    avro::decode(d, m_depthDatum);
    avro::decode(d, m_indexPropertyKindUri);
    avro::decode(d, m_filterable);
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema ChangeAnnotation::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.ChangeAnnotation"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChangeAnnotation::encode(avro::Encoder& e) const {
    avro::encode(e, m_changeTime);
    avro::encode(e, m_interval);
}
void ChangeAnnotation::decode(avro::Decoder& d) {
    avro::decode(d, m_changeTime);
    avro::decode(d, m_interval);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

avro::ValidSchema DataAttribute::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataAttribute"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DataAttribute::encode(avro::Encoder& e) const {
    avro::encode(e, m_attributeId);
    avro::encode(e, m_attributeValue);
}
void DataAttribute::decode(avro::Decoder& d) {
    avro::decode(d, m_attributeId);
    avro::decode(d, m_attributeValue);
}

avro::ValidSchema MessageHeaderExtension::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.MessageHeaderExtension"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void MessageHeaderExtension::encode(avro::Encoder& e) const {
    avro::encode(e, m_extension);
}
void MessageHeaderExtension::decode(avro::Decoder& d) {
    avro::decode(d, m_extension);
}

avro::ValidSchema SupportedDataObject::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.SupportedDataObject"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SupportedDataObject::encode(avro::Encoder& e) const {
    avro::encode(e, m_qualifiedType);
    avro::encode(e, m_dataObjectCapabilities);
}
void SupportedDataObject::decode(avro::Decoder& d) {
    avro::decode(d, m_qualifiedType);
    avro::decode(d, m_dataObjectCapabilities);
}

avro::ValidSchema SupportedProtocol::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.SupportedProtocol"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SupportedProtocol::encode(avro::Encoder& e) const {
    avro::encode(e, m_protocol);
    avro::encode(e, m_protocolVersion);
    avro::encode(e, m_role);
    avro::encode(e, m_protocolCapabilities);
}
void SupportedProtocol::decode(avro::Decoder& d) {
    avro::decode(d, m_protocol);
    avro::decode(d, m_protocolVersion);
    avro::decode(d, m_role);
    avro::decode(d, m_protocolCapabilities);
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

avro::ValidSchema ChannelMetadataRecord::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.ChannelMetadataRecord"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelMetadataRecord::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_id);
    avro::encode(e, m_indexes);
    avro::encode(e, m_channelName);
    avro::encode(e, m_dataKind);
    avro::encode(e, m_uom);
    avro::encode(e, m_depthDatum);
    avro::encode(e, m_channelClassUri);
    avro::encode(e, m_status);
    avro::encode(e, m_source);
    avro::encode(e, m_axisVectorLengths);
    avro::encode(e, m_attributeMetadata);
    avro::encode(e, m_customData);
}
void ChannelMetadataRecord::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_id);
    avro::decode(d, m_indexes);
    avro::decode(d, m_channelName);
    avro::decode(d, m_dataKind);
    avro::decode(d, m_uom);
    avro::decode(d, m_depthDatum);
    avro::decode(d, m_channelClassUri);
    avro::decode(d, m_status);
    avro::decode(d, m_source);
    avro::decode(d, m_axisVectorLengths);
    avro::decode(d, m_attributeMetadata);
    avro::decode(d, m_customData);
}

avro::ValidSchema FrameChannelMetadataRecord::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.FrameChannelMetadataRecord"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FrameChannelMetadataRecord::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_channelName);
    avro::encode(e, m_dataKind);
    avro::encode(e, m_uom);
    avro::encode(e, m_depthDatum);
    avro::encode(e, m_channelPropertyKindUri);
    avro::encode(e, m_status);
    avro::encode(e, m_source);
    avro::encode(e, m_axisVectorLengths);
    avro::encode(e, m_attributeMetadata);
    avro::encode(e, m_customData);
}
void FrameChannelMetadataRecord::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_channelName);
    avro::decode(d, m_dataKind);
    avro::decode(d, m_uom);
    avro::decode(d, m_depthDatum);
    avro::decode(d, m_channelPropertyKindUri);
    avro::decode(d, m_status);
    avro::decode(d, m_source);
    avro::decode(d, m_axisVectorLengths);
    avro::decode(d, m_attributeMetadata);
    avro::decode(d, m_customData);
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

avro::ValidSchema DataArrayMetadata::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataArrayTypes.DataArrayMetadata"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DataArrayMetadata::encode(avro::Encoder& e) const {
    avro::encode(e, m_dimensions);
    avro::encode(e, m_preferredSubarrayDimensions);
    avro::encode(e, m_transportArrayType);
    avro::encode(e, m_logicalArrayType);
    avro::encode(e, m_storeLastWrite);
    avro::encode(e, m_storeCreated);
    avro::encode(e, m_customData);
}
void DataArrayMetadata::decode(avro::Decoder& d) {
    avro::decode(d, m_dimensions);
    avro::decode(d, m_preferredSubarrayDimensions);
    avro::decode(d, m_transportArrayType);
    avro::decode(d, m_logicalArrayType);
    avro::decode(d, m_storeLastWrite);
    avro::decode(d, m_storeCreated);
    avro::decode(d, m_customData);
}

avro::ValidSchema PutDataArraysType::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataArrayTypes.PutDataArraysType"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataArraysType::encode(avro::Encoder& e) const {
    avro::encode(e, m_uid);
    avro::encode(e, m_array);
    avro::encode(e, m_customData);
}
void PutDataArraysType::decode(avro::Decoder& d) {
    avro::decode(d, m_uid);
    avro::decode(d, m_array);
    avro::decode(d, m_customData);
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema ChangeResponseInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.ChangeResponseInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChangeResponseInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_responseTimestamp);
    avro::encode(e, m_changes);
}
void ChangeResponseInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_responseTimestamp);
    avro::decode(d, m_changes);
}

avro::ValidSchema Dataspace::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.Dataspace"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Dataspace::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_path);
    avro::encode(e, m_storeLastWrite);
    avro::encode(e, m_storeCreated);
    avro::encode(e, m_customData);
}
void Dataspace::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_path);
    avro::decode(d, m_storeLastWrite);
    avro::decode(d, m_storeCreated);
    avro::decode(d, m_customData);
}

avro::ValidSchema DeletedResource::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.DeletedResource"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DeletedResource::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_deletedTime);
    avro::encode(e, m_customData);
}
void DeletedResource::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_deletedTime);
    avro::decode(d, m_customData);
}

avro::ValidSchema Edge::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.Edge"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Edge::encode(avro::Encoder& e) const {
    avro::encode(e, m_sourceUri);
    avro::encode(e, m_targetUri);
    avro::encode(e, m_relationshipKind);
    avro::encode(e, m_customData);
}
void Edge::decode(avro::Decoder& d) {
    avro::decode(d, m_sourceUri);
    avro::decode(d, m_targetUri);
    avro::decode(d, m_relationshipKind);
    avro::decode(d, m_customData);
}

avro::ValidSchema PartsMetadataInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.PartsMetadataInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PartsMetadataInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_name);
    avro::encode(e, m_index);
    avro::encode(e, m_customData);
}
void PartsMetadataInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_name);
    avro::decode(d, m_index);
    avro::decode(d, m_customData);
}

avro::ValidSchema Resource::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.Resource"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Resource::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_alternateUris);
    avro::encode(e, m_name);
    avro::encode(e, m_sourceCount);
    avro::encode(e, m_targetCount);
    avro::encode(e, m_lastChanged);
    avro::encode(e, m_storeLastWrite);
    avro::encode(e, m_storeCreated);
    avro::encode(e, m_activeStatus);
    avro::encode(e, m_customData);
}
void Resource::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_alternateUris);
    avro::decode(d, m_name);
    avro::decode(d, m_sourceCount);
    avro::decode(d, m_targetCount);
    avro::decode(d, m_lastChanged);
    avro::decode(d, m_storeLastWrite);
    avro::decode(d, m_storeCreated);
    avro::decode(d, m_activeStatus);
    avro::decode(d, m_customData);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

avro::ValidSchema ServerCapabilities::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ServerCapabilities"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ServerCapabilities::encode(avro::Encoder& e) const {
    avro::encode(e, m_applicationName);
    avro::encode(e, m_applicationVersion);
    avro::encode(e, m_contactInformation);
    avro::encode(e, m_supportedCompression);
    avro::encode(e, m_supportedEncodings);
    avro::encode(e, m_supportedFormats);
    avro::encode(e, m_supportedDataObjects);
    avro::encode(e, m_supportedProtocols);
    avro::encode(e, m_endpointCapabilities);
}
void ServerCapabilities::decode(avro::Decoder& d) {
    avro::decode(d, m_applicationName);
    avro::decode(d, m_applicationVersion);
    avro::decode(d, m_contactInformation);
    avro::decode(d, m_supportedCompression);
    avro::decode(d, m_supportedEncodings);
    avro::decode(d, m_supportedFormats);
    avro::decode(d, m_supportedDataObjects);
    avro::decode(d, m_supportedProtocols);
    avro::decode(d, m_endpointCapabilities);
}

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

avro::ValidSchema DataItem::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.DataItem"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DataItem::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelId);
    avro::encode(e, m_indexes);
    avro::encode(e, m_value);
    avro::encode(e, m_valueAttributes);
}
void DataItem::decode(avro::Decoder& d) {
    avro::decode(d, m_channelId);
    avro::decode(d, m_indexes);
    avro::decode(d, m_value);
    avro::decode(d, m_valueAttributes);
}

avro::ValidSchema FramePoint::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.FramePoint"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FramePoint::encode(avro::Encoder& e) const {
    avro::encode(e, m_value);
    avro::encode(e, m_valueAttributes);
}
void FramePoint::decode(avro::Decoder& d) {
    avro::decode(d, m_value);
    avro::decode(d, m_valueAttributes);
}

avro::ValidSchema OpenChannelInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.OpenChannelInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void OpenChannelInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_metadata);
    avro::encode(e, m_preferRealtime);
    avro::encode(e, m_dataChanges);
}
void OpenChannelInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_metadata);
    avro::decode(d, m_preferRealtime);
    avro::decode(d, m_dataChanges);
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

avro::ValidSchema PutUninitializedDataArrayType::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.DataArrayTypes.PutUninitializedDataArrayType"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutUninitializedDataArrayType::encode(avro::Encoder& e) const {
    avro::encode(e, m_uid);
    avro::encode(e, m_metadata);
}
void PutUninitializedDataArrayType::decode(avro::Decoder& d) {
    avro::decode(d, m_uid);
    avro::decode(d, m_metadata);
}

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema DataObject::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.DataObject"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DataObject::encode(avro::Encoder& e) const {
    avro::encode(e, m_resource);
    avro::encode(e, m_format);
    avro::encode(e, m_blobId);
    avro::encode(e, m_data);
}
void DataObject::decode(avro::Decoder& d) {
    avro::decode(d, m_resource);
    avro::decode(d, m_format);
    avro::decode(d, m_blobId);
    avro::decode(d, m_data);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes::ChannelData {

avro::ValidSchema FrameRow::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.ChannelData.FrameRow"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FrameRow::encode(avro::Encoder& e) const {
    avro::encode(e, m_indexes);
    avro::encode(e, m_points);
}
void FrameRow::decode(avro::Decoder& d) {
    avro::decode(d, m_indexes);
    avro::decode(d, m_points);
}

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

avro::ValidSchema ObjectChange::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.Object.ObjectChange"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ObjectChange::encode(avro::Encoder& e) const {
    avro::encode(e, m_changeKind);
    avro::encode(e, m_changeTime);
    avro::encode(e, m_dataObject);
}
void ObjectChange::decode(avro::Decoder& d) {
    avro::decode(d, m_changeKind);
    avro::decode(d, m_changeTime);
    avro::decode(d, m_dataObject);
}

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Protocol::Core {

avro::ValidSchema Acknowledge::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.Acknowledge"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Acknowledge::encode(avro::Encoder&) const {
}
void Acknowledge::decode(avro::Decoder&) {
}

avro::ValidSchema Authorize::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.Authorize"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Authorize::encode(avro::Encoder& e) const {
    avro::encode(e, m_authorization);
    avro::encode(e, m_supplementalAuthorization);
}
void Authorize::decode(avro::Decoder& d) {
    avro::decode(d, m_authorization);
    avro::decode(d, m_supplementalAuthorization);
}

avro::ValidSchema AuthorizeResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.AuthorizeResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void AuthorizeResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
    avro::encode(e, m_challenges);
}
void AuthorizeResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
    avro::decode(d, m_challenges);
}

avro::ValidSchema CloseSession::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.CloseSession"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CloseSession::encode(avro::Encoder& e) const {
    avro::encode(e, m_reason);
}
void CloseSession::decode(avro::Decoder& d) {
    avro::decode(d, m_reason);
}

avro::ValidSchema Ping::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.Ping"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Ping::encode(avro::Encoder& e) const {
    avro::encode(e, m_currentDateTime);
}
void Ping::decode(avro::Decoder& d) {
    avro::decode(d, m_currentDateTime);
}

avro::ValidSchema Pong::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.Pong"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Pong::encode(avro::Encoder& e) const {
    avro::encode(e, m_currentDateTime);
}
void Pong::decode(avro::Decoder& d) {
    avro::decode(d, m_currentDateTime);
}

avro::ValidSchema ProtocolException::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.ProtocolException"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ProtocolException::encode(avro::Encoder& e) const {
    avro::encode(e, m_error);
    avro::encode(e, m_errors);
}
void ProtocolException::decode(avro::Decoder& d) {
    avro::decode(d, m_error);
    avro::decode(d, m_errors);
}

avro::ValidSchema OpenSession::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.OpenSession"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void OpenSession::encode(avro::Encoder& e) const {
    avro::encode(e, m_applicationName);
    avro::encode(e, m_applicationVersion);
    avro::encode(e, m_serverInstanceId);
    avro::encode(e, m_supportedProtocols);
    avro::encode(e, m_supportedDataObjects);
    avro::encode(e, m_supportedCompression);
    avro::encode(e, m_supportedFormats);
    avro::encode(e, m_currentDateTime);
    avro::encode(e, m_earliestRetainedChangeTime);
    avro::encode(e, m_sessionId);
    avro::encode(e, m_endpointCapabilities);
}
void OpenSession::decode(avro::Decoder& d) {
    avro::decode(d, m_applicationName);
    avro::decode(d, m_applicationVersion);
    avro::decode(d, m_serverInstanceId);
    avro::decode(d, m_supportedProtocols);
    avro::decode(d, m_supportedDataObjects);
    avro::decode(d, m_supportedCompression);
    avro::decode(d, m_supportedFormats);
    avro::decode(d, m_currentDateTime);
    avro::decode(d, m_earliestRetainedChangeTime);
    avro::decode(d, m_sessionId);
    avro::decode(d, m_endpointCapabilities);
}

avro::ValidSchema RequestSession::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Core.RequestSession"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void RequestSession::encode(avro::Encoder& e) const {
    avro::encode(e, m_applicationName);
    avro::encode(e, m_applicationVersion);
    avro::encode(e, m_clientInstanceId);
    avro::encode(e, m_requestedProtocols);
    avro::encode(e, m_supportedDataObjects);
    avro::encode(e, m_supportedCompression);
    avro::encode(e, m_supportedFormats);
    avro::encode(e, m_currentDateTime);
    avro::encode(e, m_earliestRetainedChangeTime);
    avro::encode(e, m_serverAuthorizationRequired);
    avro::encode(e, m_endpointCapabilities);
}
void RequestSession::decode(avro::Decoder& d) {
    avro::decode(d, m_applicationName);
    avro::decode(d, m_applicationVersion);
    avro::decode(d, m_clientInstanceId);
    avro::decode(d, m_requestedProtocols);
    avro::decode(d, m_supportedDataObjects);
    avro::decode(d, m_supportedCompression);
    avro::decode(d, m_supportedFormats);
    avro::decode(d, m_currentDateTime);
    avro::decode(d, m_earliestRetainedChangeTime);
    avro::decode(d, m_serverAuthorizationRequired);
    avro::decode(d, m_endpointCapabilities);
}

} //      oes::common::etp12::Protocol::Core

namespace oes::common::etp12::Protocol::ChannelStreaming {

avro::ValidSchema StartStreaming::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelStreaming.StartStreaming"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void StartStreaming::encode(avro::Encoder&) const {
}
void StartStreaming::decode(avro::Decoder&) {
}

avro::ValidSchema StopStreaming::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelStreaming.StopStreaming"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void StopStreaming::encode(avro::Encoder&) const {
}
void StopStreaming::decode(avro::Decoder&) {
}

avro::ValidSchema TruncateChannels::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelStreaming.TruncateChannels"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void TruncateChannels::encode(avro::Encoder& e) const {
    avro::encode(e, m_channels);
}
void TruncateChannels::decode(avro::Decoder& d) {
    avro::decode(d, m_channels);
}

avro::ValidSchema ChannelMetadata::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelStreaming.ChannelMetadata"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelMetadata::encode(avro::Encoder& e) const {
    avro::encode(e, m_channels);
}
void ChannelMetadata::decode(avro::Decoder& d) {
    avro::decode(d, m_channels);
}

avro::ValidSchema ChannelData::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelStreaming.ChannelData"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelData::encode(avro::Encoder& e) const {
    avro::encode(e, m_data);
}
void ChannelData::decode(avro::Decoder& d) {
    avro::decode(d, m_data);
}

} //      oes::common::etp12::Protocol::ChannelStreaming

namespace oes::common::etp12::Protocol::ChannelDataFrame {

avro::ValidSchema GetFrameMetadata::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataFrame.GetFrameMetadata"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetFrameMetadata::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_includeAllChannelSecondaryIndexes);
}
void GetFrameMetadata::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_includeAllChannelSecondaryIndexes);
}

avro::ValidSchema CancelGetFrame::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataFrame.CancelGetFrame"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CancelGetFrame::encode(avro::Encoder& e) const {
    avro::encode(e, m_requestUuid);
}
void CancelGetFrame::decode(avro::Decoder& d) {
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema GetFrame::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataFrame.GetFrame"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetFrame::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_includeAllChannelSecondaryIndexes);
    avro::encode(e, m_requestedInterval);
    avro::encode(e, m_requestUuid);
    avro::encode(e, m_requestedSecondaryIntervals);
}
void GetFrame::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_includeAllChannelSecondaryIndexes);
    avro::decode(d, m_requestedInterval);
    avro::decode(d, m_requestUuid);
    avro::decode(d, m_requestedSecondaryIntervals);
}

avro::ValidSchema GetFrameResponseHeader::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataFrame.GetFrameResponseHeader"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetFrameResponseHeader::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelUris);
    avro::encode(e, m_indexes);
}
void GetFrameResponseHeader::decode(avro::Decoder& d) {
    avro::decode(d, m_channelUris);
    avro::decode(d, m_indexes);
}

avro::ValidSchema GetFrameMetadataResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataFrame.GetFrameMetadataResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetFrameMetadataResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_indexes);
    avro::encode(e, m_channels);
}
void GetFrameMetadataResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_indexes);
    avro::decode(d, m_channels);
}

avro::ValidSchema GetFrameResponseRows::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataFrame.GetFrameResponseRows"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetFrameResponseRows::encode(avro::Encoder& e) const {
    avro::encode(e, m_frame);
}
void GetFrameResponseRows::decode(avro::Decoder& d) {
    avro::decode(d, m_frame);
}

} //      oes::common::etp12::Protocol::ChannelDataFrame

namespace oes::common::etp12::Protocol::Discovery {

avro::ValidSchema GetDeletedResources::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Discovery.GetDeletedResources"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDeletedResources::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataspaceUri);
    avro::encode(e, m_deleteTimeFilter);
    avro::encode(e, m_dataObjectTypes);
}
void GetDeletedResources::decode(avro::Decoder& d) {
    avro::decode(d, m_dataspaceUri);
    avro::decode(d, m_deleteTimeFilter);
    avro::decode(d, m_dataObjectTypes);
}

avro::ValidSchema GetResources::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Discovery.GetResources"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetResources::encode(avro::Encoder& e) const {
    avro::encode(e, m_context);
    avro::encode(e, m_scope);
    avro::encode(e, m_countObjects);
    avro::encode(e, m_storeLastWriteFilter);
    avro::encode(e, m_activeStatusFilter);
    avro::encode(e, m_includeEdges);
}
void GetResources::decode(avro::Decoder& d) {
    avro::decode(d, m_context);
    avro::decode(d, m_scope);
    avro::decode(d, m_countObjects);
    avro::decode(d, m_storeLastWriteFilter);
    avro::decode(d, m_activeStatusFilter);
    avro::decode(d, m_includeEdges);
}

avro::ValidSchema GetDeletedResourcesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Discovery.GetDeletedResourcesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDeletedResourcesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_deletedResources);
}
void GetDeletedResourcesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_deletedResources);
}

avro::ValidSchema GetResourcesEdgesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Discovery.GetResourcesEdgesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetResourcesEdgesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_edges);
}
void GetResourcesEdgesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_edges);
}

avro::ValidSchema GetResourcesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Discovery.GetResourcesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetResourcesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_resources);
}
void GetResourcesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_resources);
}

} //      oes::common::etp12::Protocol::Discovery

namespace oes::common::etp12::Protocol::Store {

avro::ValidSchema DeleteDataObjects::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Store.DeleteDataObjects"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DeleteDataObjects::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
    avro::encode(e, m_pruneContainedObjects);
}
void DeleteDataObjects::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
    avro::decode(d, m_pruneContainedObjects);
}

avro::ValidSchema GetDataObjects::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Store.GetDataObjects"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataObjects::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
    avro::encode(e, m_format);
}
void GetDataObjects::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
    avro::decode(d, m_format);
}

avro::ValidSchema Chunk::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Store.Chunk"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Chunk::encode(avro::Encoder& e) const {
    avro::encode(e, m_blobId);
    avro::encode(e, m_data);
    avro::encode(e, m_final);
}
void Chunk::decode(avro::Decoder& d) {
    avro::decode(d, m_blobId);
    avro::decode(d, m_data);
    avro::decode(d, m_final);
}

avro::ValidSchema DeleteDataObjectsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Store.DeleteDataObjectsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DeleteDataObjectsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_deletedUris);
}
void DeleteDataObjectsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_deletedUris);
}

avro::ValidSchema PutDataObjectsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Store.PutDataObjectsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataObjectsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void PutDataObjectsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema GetDataObjectsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Store.GetDataObjectsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataObjectsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataObjects);
}
void GetDataObjectsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_dataObjects);
}

avro::ValidSchema PutDataObjects::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Store.PutDataObjects"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataObjects::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataObjects);
    avro::encode(e, m_pruneContainedObjects);
}
void PutDataObjects::decode(avro::Decoder& d) {
    avro::decode(d, m_dataObjects);
    avro::decode(d, m_pruneContainedObjects);
}

} //      oes::common::etp12::Protocol::Store

namespace oes::common::etp12::Protocol::StoreNotification {

avro::ValidSchema SubscribeNotificationsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.SubscribeNotificationsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscribeNotificationsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void SubscribeNotificationsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema Chunk::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.Chunk"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Chunk::encode(avro::Encoder& e) const {
    avro::encode(e, m_blobId);
    avro::encode(e, m_data);
    avro::encode(e, m_final);
}
void Chunk::decode(avro::Decoder& d) {
    avro::decode(d, m_blobId);
    avro::decode(d, m_data);
    avro::decode(d, m_final);
}

avro::ValidSchema ObjectAccessRevoked::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.ObjectAccessRevoked"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ObjectAccessRevoked::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_changeTime);
    avro::encode(e, m_requestUuid);
}
void ObjectAccessRevoked::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_changeTime);
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema ObjectDeleted::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.ObjectDeleted"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ObjectDeleted::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_changeTime);
    avro::encode(e, m_requestUuid);
}
void ObjectDeleted::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_changeTime);
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema SubscriptionEnded::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.SubscriptionEnded"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscriptionEnded::encode(avro::Encoder& e) const {
    avro::encode(e, m_reason);
    avro::encode(e, m_requestUuid);
}
void SubscriptionEnded::decode(avro::Decoder& d) {
    avro::decode(d, m_reason);
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema UnsubscribeNotifications::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.UnsubscribeNotifications"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void UnsubscribeNotifications::encode(avro::Encoder& e) const {
    avro::encode(e, m_requestUuid);
}
void UnsubscribeNotifications::decode(avro::Decoder& d) {
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema SubscribeNotifications::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.SubscribeNotifications"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscribeNotifications::encode(avro::Encoder& e) const {
    avro::encode(e, m_request);
}
void SubscribeNotifications::decode(avro::Decoder& d) {
    avro::decode(d, m_request);
}

avro::ValidSchema UnsolicitedStoreNotifications::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.UnsolicitedStoreNotifications"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void UnsolicitedStoreNotifications::encode(avro::Encoder& e) const {
    avro::encode(e, m_subscriptions);
}
void UnsolicitedStoreNotifications::decode(avro::Decoder& d) {
    avro::decode(d, m_subscriptions);
}

avro::ValidSchema ObjectActiveStatusChanged::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.ObjectActiveStatusChanged"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ObjectActiveStatusChanged::encode(avro::Encoder& e) const {
    avro::encode(e, m_activeStatus);
    avro::encode(e, m_changeTime);
    avro::encode(e, m_resource);
    avro::encode(e, m_requestUuid);
}
void ObjectActiveStatusChanged::decode(avro::Decoder& d) {
    avro::decode(d, m_activeStatus);
    avro::decode(d, m_changeTime);
    avro::decode(d, m_resource);
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema ObjectChanged::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreNotification.ObjectChanged"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ObjectChanged::encode(avro::Encoder& e) const {
    avro::encode(e, m_change);
    avro::encode(e, m_requestUuid);
}
void ObjectChanged::decode(avro::Decoder& d) {
    avro::decode(d, m_change);
    avro::decode(d, m_requestUuid);
}

} //      oes::common::etp12::Protocol::StoreNotification

namespace oes::common::etp12::Protocol::GrowingObject {

avro::ValidSchema DeleteParts::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.DeleteParts"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DeleteParts::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_uids);
}
void DeleteParts::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_uids);
}

avro::ValidSchema DeletePartsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.DeletePartsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DeletePartsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void DeletePartsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema GetChangeAnnotations::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetChangeAnnotations"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetChangeAnnotations::encode(avro::Encoder& e) const {
    avro::encode(e, m_sinceChangeTime);
    avro::encode(e, m_uris);
    avro::encode(e, m_latestOnly);
}
void GetChangeAnnotations::decode(avro::Decoder& d) {
    avro::decode(d, m_sinceChangeTime);
    avro::decode(d, m_uris);
    avro::decode(d, m_latestOnly);
}

avro::ValidSchema GetGrowingDataObjectsHeader::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetGrowingDataObjectsHeader"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetGrowingDataObjectsHeader::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
    avro::encode(e, m_format);
}
void GetGrowingDataObjectsHeader::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
    avro::decode(d, m_format);
}

avro::ValidSchema GetParts::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetParts"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetParts::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_format);
    avro::encode(e, m_uids);
}
void GetParts::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_format);
    avro::decode(d, m_uids);
}

avro::ValidSchema GetPartsMetadata::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetPartsMetadata"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetPartsMetadata::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
}
void GetPartsMetadata::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
}

avro::ValidSchema PutGrowingDataObjectsHeaderResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.PutGrowingDataObjectsHeaderResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutGrowingDataObjectsHeaderResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void PutGrowingDataObjectsHeaderResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema PutPartsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.PutPartsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutPartsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void PutPartsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema ReplacePartsByRangeResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.ReplacePartsByRangeResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ReplacePartsByRangeResponse::encode(avro::Encoder&) const {
}
void ReplacePartsByRangeResponse::decode(avro::Decoder&) {
}

avro::ValidSchema GetPartsByRangeResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetPartsByRangeResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetPartsByRangeResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_format);
    avro::encode(e, m_parts);
}
void GetPartsByRangeResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_format);
    avro::decode(d, m_parts);
}

avro::ValidSchema GetPartsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetPartsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetPartsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_format);
    avro::encode(e, m_parts);
}
void GetPartsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_format);
    avro::decode(d, m_parts);
}

avro::ValidSchema PutParts::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.PutParts"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutParts::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_format);
    avro::encode(e, m_parts);
}
void PutParts::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_format);
    avro::decode(d, m_parts);
}

avro::ValidSchema GetPartsByRange::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetPartsByRange"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetPartsByRange::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_format);
    avro::encode(e, m_indexInterval);
    avro::encode(e, m_includeOverlappingIntervals);
}
void GetPartsByRange::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_format);
    avro::decode(d, m_indexInterval);
    avro::decode(d, m_includeOverlappingIntervals);
}

avro::ValidSchema ReplacePartsByRange::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.ReplacePartsByRange"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ReplacePartsByRange::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_deleteInterval);
    avro::encode(e, m_includeOverlappingIntervals);
    avro::encode(e, m_format);
    avro::encode(e, m_parts);
}
void ReplacePartsByRange::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_deleteInterval);
    avro::decode(d, m_includeOverlappingIntervals);
    avro::decode(d, m_format);
    avro::decode(d, m_parts);
}

avro::ValidSchema GetChangeAnnotationsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetChangeAnnotationsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetChangeAnnotationsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_changes);
}
void GetChangeAnnotationsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_changes);
}

avro::ValidSchema GetPartsMetadataResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetPartsMetadataResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetPartsMetadataResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_metadata);
}
void GetPartsMetadataResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_metadata);
}

avro::ValidSchema GetGrowingDataObjectsHeaderResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.GetGrowingDataObjectsHeaderResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetGrowingDataObjectsHeaderResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataObjects);
}
void GetGrowingDataObjectsHeaderResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_dataObjects);
}

avro::ValidSchema PutGrowingDataObjectsHeader::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObject.PutGrowingDataObjectsHeader"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutGrowingDataObjectsHeader::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataObjects);
}
void PutGrowingDataObjectsHeader::decode(avro::Decoder& d) {
    avro::decode(d, m_dataObjects);
}

} //      oes::common::etp12::Protocol::GrowingObject

namespace oes::common::etp12::Protocol::GrowingObjectNotification {

avro::ValidSchema SubscribePartNotificationsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.SubscribePartNotificationsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscribePartNotificationsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void SubscribePartNotificationsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema PartSubscriptionEnded::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.PartSubscriptionEnded"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PartSubscriptionEnded::encode(avro::Encoder& e) const {
    avro::encode(e, m_reason);
    avro::encode(e, m_requestUuid);
}
void PartSubscriptionEnded::decode(avro::Decoder& d) {
    avro::decode(d, m_reason);
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema PartsChanged::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.PartsChanged"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PartsChanged::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_requestUuid);
    avro::encode(e, m_changeKind);
    avro::encode(e, m_changeTime);
    avro::encode(e, m_format);
    avro::encode(e, m_parts);
}
void PartsChanged::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_requestUuid);
    avro::decode(d, m_changeKind);
    avro::decode(d, m_changeTime);
    avro::decode(d, m_format);
    avro::decode(d, m_parts);
}

avro::ValidSchema PartsDeleted::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.PartsDeleted"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PartsDeleted::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_requestUuid);
    avro::encode(e, m_changeTime);
    avro::encode(e, m_uids);
}
void PartsDeleted::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_requestUuid);
    avro::decode(d, m_changeTime);
    avro::decode(d, m_uids);
}

avro::ValidSchema UnsubscribePartNotification::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.UnsubscribePartNotification"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void UnsubscribePartNotification::encode(avro::Encoder& e) const {
    avro::encode(e, m_requestUuid);
}
void UnsubscribePartNotification::decode(avro::Decoder& d) {
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema SubscribePartNotifications::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.SubscribePartNotifications"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscribePartNotifications::encode(avro::Encoder& e) const {
    avro::encode(e, m_request);
}
void SubscribePartNotifications::decode(avro::Decoder& d) {
    avro::decode(d, m_request);
}

avro::ValidSchema UnsolicitedPartNotifications::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.UnsolicitedPartNotifications"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void UnsolicitedPartNotifications::encode(avro::Encoder& e) const {
    avro::encode(e, m_subscriptions);
}
void UnsolicitedPartNotifications::decode(avro::Decoder& d) {
    avro::decode(d, m_subscriptions);
}

avro::ValidSchema PartsReplacedByRange::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectNotification.PartsReplacedByRange"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PartsReplacedByRange::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_requestUuid);
    avro::encode(e, m_changeTime);
    avro::encode(e, m_deletedInterval);
    avro::encode(e, m_includeOverlappingIntervals);
    avro::encode(e, m_format);
    avro::encode(e, m_parts);
}
void PartsReplacedByRange::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_requestUuid);
    avro::decode(d, m_changeTime);
    avro::decode(d, m_deletedInterval);
    avro::decode(d, m_includeOverlappingIntervals);
    avro::decode(d, m_format);
    avro::decode(d, m_parts);
}

} //      oes::common::etp12::Protocol::GrowingObjectNotification

namespace oes::common::etp12::Protocol::DataArray {

avro::ValidSchema PutDataArraysResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.PutDataArraysResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataArraysResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void PutDataArraysResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema PutDataSubarraysResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.PutDataSubarraysResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataSubarraysResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void PutDataSubarraysResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema PutUninitializedDataArraysResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.PutUninitializedDataArraysResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutUninitializedDataArraysResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void PutUninitializedDataArraysResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema GetDataArrayMetadata::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.GetDataArrayMetadata"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataArrayMetadata::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataArrays);
}
void GetDataArrayMetadata::decode(avro::Decoder& d) {
    avro::decode(d, m_dataArrays);
}

avro::ValidSchema GetDataArrays::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.GetDataArrays"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataArrays::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataArrays);
}
void GetDataArrays::decode(avro::Decoder& d) {
    avro::decode(d, m_dataArrays);
}

avro::ValidSchema GetDataSubarrays::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.GetDataSubarrays"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataSubarrays::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataSubarrays);
}
void GetDataSubarrays::decode(avro::Decoder& d) {
    avro::decode(d, m_dataSubarrays);
}

avro::ValidSchema GetDataArraysResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.GetDataArraysResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataArraysResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataArrays);
}
void GetDataArraysResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_dataArrays);
}

avro::ValidSchema GetDataSubarraysResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.GetDataSubarraysResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataSubarraysResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataSubarrays);
}
void GetDataSubarraysResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_dataSubarrays);
}

avro::ValidSchema PutDataSubarrays::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.PutDataSubarrays"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataSubarrays::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataSubarrays);
}
void PutDataSubarrays::decode(avro::Decoder& d) {
    avro::decode(d, m_dataSubarrays);
}

avro::ValidSchema GetDataArrayMetadataResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.GetDataArrayMetadataResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataArrayMetadataResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_arrayMetadata);
}
void GetDataArrayMetadataResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_arrayMetadata);
}

avro::ValidSchema PutDataArrays::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.PutDataArrays"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataArrays::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataArrays);
}
void PutDataArrays::decode(avro::Decoder& d) {
    avro::decode(d, m_dataArrays);
}

avro::ValidSchema PutUninitializedDataArrays::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataArray.PutUninitializedDataArrays"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutUninitializedDataArrays::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataArrays);
}
void PutUninitializedDataArrays::decode(avro::Decoder& d) {
    avro::decode(d, m_dataArrays);
}

} //      oes::common::etp12::Protocol::DataArray

namespace oes::common::etp12::Protocol::DiscoveryQuery {

avro::ValidSchema FindResources::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DiscoveryQuery.FindResources"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FindResources::encode(avro::Encoder& e) const {
    avro::encode(e, m_context);
    avro::encode(e, m_scope);
    avro::encode(e, m_storeLastWriteFilter);
    avro::encode(e, m_activeStatusFilter);
}
void FindResources::decode(avro::Decoder& d) {
    avro::decode(d, m_context);
    avro::decode(d, m_scope);
    avro::decode(d, m_storeLastWriteFilter);
    avro::decode(d, m_activeStatusFilter);
}

avro::ValidSchema FindResourcesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DiscoveryQuery.FindResourcesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FindResourcesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_resources);
    avro::encode(e, m_serverSortOrder);
}
void FindResourcesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_resources);
    avro::decode(d, m_serverSortOrder);
}

} //      oes::common::etp12::Protocol::DiscoveryQuery

namespace oes::common::etp12::Protocol::StoreQuery {

avro::ValidSchema Chunk::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreQuery.Chunk"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void Chunk::encode(avro::Encoder& e) const {
    avro::encode(e, m_blobId);
    avro::encode(e, m_data);
    avro::encode(e, m_final);
}
void Chunk::decode(avro::Decoder& d) {
    avro::decode(d, m_blobId);
    avro::decode(d, m_data);
    avro::decode(d, m_final);
}

avro::ValidSchema FindDataObjects::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreQuery.FindDataObjects"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FindDataObjects::encode(avro::Encoder& e) const {
    avro::encode(e, m_context);
    avro::encode(e, m_scope);
    avro::encode(e, m_storeLastWriteFilter);
    avro::encode(e, m_activeStatusFilter);
    avro::encode(e, m_format);
}
void FindDataObjects::decode(avro::Decoder& d) {
    avro::decode(d, m_context);
    avro::decode(d, m_scope);
    avro::decode(d, m_storeLastWriteFilter);
    avro::decode(d, m_activeStatusFilter);
    avro::decode(d, m_format);
}

avro::ValidSchema FindDataObjectsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreQuery.FindDataObjectsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FindDataObjectsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataObjects);
    avro::encode(e, m_serverSortOrder);
}
void FindDataObjectsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_dataObjects);
    avro::decode(d, m_serverSortOrder);
}

} //      oes::common::etp12::Protocol::StoreQuery

namespace oes::common::etp12::Protocol::GrowingObjectQuery {

avro::ValidSchema FindParts::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectQuery.FindParts"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FindParts::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_format);
}
void FindParts::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_format);
}

avro::ValidSchema FindPartsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.GrowingObjectQuery.FindPartsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void FindPartsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_serverSortOrder);
    avro::encode(e, m_format);
    avro::encode(e, m_parts);
}
void FindPartsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_serverSortOrder);
    avro::decode(d, m_format);
    avro::decode(d, m_parts);
}

} //      oes::common::etp12::Protocol::GrowingObjectQuery

namespace oes::common::etp12::Protocol::Transaction {

avro::ValidSchema StartTransaction::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Transaction.StartTransaction"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void StartTransaction::encode(avro::Encoder& e) const {
    avro::encode(e, m_readOnly);
    avro::encode(e, m_message);
    avro::encode(e, m_dataspaceUris);
}
void StartTransaction::decode(avro::Decoder& d) {
    avro::decode(d, m_readOnly);
    avro::decode(d, m_message);
    avro::decode(d, m_dataspaceUris);
}

avro::ValidSchema CommitTransaction::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Transaction.CommitTransaction"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CommitTransaction::encode(avro::Encoder& e) const {
    avro::encode(e, m_transactionUuid);
}
void CommitTransaction::decode(avro::Decoder& d) {
    avro::decode(d, m_transactionUuid);
}

avro::ValidSchema CommitTransactionResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Transaction.CommitTransactionResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CommitTransactionResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_transactionUuid);
    avro::encode(e, m_successful);
    avro::encode(e, m_failureReason);
}
void CommitTransactionResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_transactionUuid);
    avro::decode(d, m_successful);
    avro::decode(d, m_failureReason);
}

avro::ValidSchema RollbackTransaction::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Transaction.RollbackTransaction"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void RollbackTransaction::encode(avro::Encoder& e) const {
    avro::encode(e, m_transactionUuid);
}
void RollbackTransaction::decode(avro::Decoder& d) {
    avro::decode(d, m_transactionUuid);
}

avro::ValidSchema RollbackTransactionResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Transaction.RollbackTransactionResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void RollbackTransactionResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_transactionUuid);
    avro::encode(e, m_successful);
    avro::encode(e, m_failureReason);
}
void RollbackTransactionResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_transactionUuid);
    avro::decode(d, m_successful);
    avro::decode(d, m_failureReason);
}

avro::ValidSchema StartTransactionResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Transaction.StartTransactionResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void StartTransactionResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_transactionUuid);
    avro::encode(e, m_successful);
    avro::encode(e, m_failureReason);
}
void StartTransactionResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_transactionUuid);
    avro::decode(d, m_successful);
    avro::decode(d, m_failureReason);
}

} //      oes::common::etp12::Protocol::Transaction

namespace oes::common::etp12::Protocol::ChannelSubscribe {

avro::ValidSchema GetChannelMetadata::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.GetChannelMetadata"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetChannelMetadata::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
}
void GetChannelMetadata::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
}

avro::ValidSchema SubscribeChannelsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.SubscribeChannelsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscribeChannelsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void SubscribeChannelsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema SubscriptionsStopped::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.SubscriptionsStopped"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscriptionsStopped::encode(avro::Encoder& e) const {
    avro::encode(e, m_reason);
    avro::encode(e, m_channelIds);
}
void SubscriptionsStopped::decode(avro::Decoder& d) {
    avro::decode(d, m_reason);
    avro::decode(d, m_channelIds);
}

avro::ValidSchema UnsubscribeChannels::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.UnsubscribeChannels"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void UnsubscribeChannels::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelIds);
}
void UnsubscribeChannels::decode(avro::Decoder& d) {
    avro::decode(d, m_channelIds);
}

avro::ValidSchema CancelGetRanges::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.CancelGetRanges"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CancelGetRanges::encode(avro::Encoder& e) const {
    avro::encode(e, m_requestUuid);
}
void CancelGetRanges::decode(avro::Decoder& d) {
    avro::decode(d, m_requestUuid);
}

avro::ValidSchema GetChangeAnnotations::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.GetChangeAnnotations"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetChangeAnnotations::encode(avro::Encoder& e) const {
    avro::encode(e, m_channels);
    avro::encode(e, m_latestOnly);
}
void GetChangeAnnotations::decode(avro::Decoder& d) {
    avro::decode(d, m_channels);
    avro::decode(d, m_latestOnly);
}

avro::ValidSchema ChannelsTruncated::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.ChannelsTruncated"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelsTruncated::encode(avro::Encoder& e) const {
    avro::encode(e, m_channels);
    avro::encode(e, m_changeTime);
}
void ChannelsTruncated::decode(avro::Decoder& d) {
    avro::decode(d, m_channels);
    avro::decode(d, m_changeTime);
}

avro::ValidSchema SubscribeChannels::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.SubscribeChannels"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void SubscribeChannels::encode(avro::Encoder& e) const {
    avro::encode(e, m_channels);
}
void SubscribeChannels::decode(avro::Decoder& d) {
    avro::decode(d, m_channels);
}

avro::ValidSchema GetRanges::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.GetRanges"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetRanges::encode(avro::Encoder& e) const {
    avro::encode(e, m_requestUuid);
    avro::encode(e, m_channelRanges);
}
void GetRanges::decode(avro::Decoder& d) {
    avro::decode(d, m_requestUuid);
    avro::decode(d, m_channelRanges);
}

avro::ValidSchema GetChangeAnnotationsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.GetChangeAnnotationsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetChangeAnnotationsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_changes);
}
void GetChangeAnnotationsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_changes);
}

avro::ValidSchema GetChannelMetadataResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.GetChannelMetadataResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetChannelMetadataResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_metadata);
}
void GetChannelMetadataResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_metadata);
}

avro::ValidSchema ChannelData::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.ChannelData"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelData::encode(avro::Encoder& e) const {
    avro::encode(e, m_data);
}
void ChannelData::decode(avro::Decoder& d) {
    avro::decode(d, m_data);
}

avro::ValidSchema GetRangesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.GetRangesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetRangesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_data);
}
void GetRangesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_data);
}

avro::ValidSchema RangeReplaced::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelSubscribe.RangeReplaced"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void RangeReplaced::encode(avro::Encoder& e) const {
    avro::encode(e, m_changeTime);
    avro::encode(e, m_channelIds);
    avro::encode(e, m_changedInterval);
    avro::encode(e, m_data);
}
void RangeReplaced::decode(avro::Decoder& d) {
    avro::decode(d, m_changeTime);
    avro::decode(d, m_channelIds);
    avro::decode(d, m_changedInterval);
    avro::decode(d, m_data);
}

} //      oes::common::etp12::Protocol::ChannelSubscribe

namespace oes::common::etp12::Protocol::ChannelDataLoad {

avro::ValidSchema ChannelsClosed::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.ChannelsClosed"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelsClosed::encode(avro::Encoder& e) const {
    avro::encode(e, m_reason);
    avro::encode(e, m_id);
}
void ChannelsClosed::decode(avro::Decoder& d) {
    avro::decode(d, m_reason);
    avro::decode(d, m_id);
}

avro::ValidSchema CloseChannels::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.CloseChannels"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CloseChannels::encode(avro::Encoder& e) const {
    avro::encode(e, m_id);
}
void CloseChannels::decode(avro::Decoder& d) {
    avro::decode(d, m_id);
}

avro::ValidSchema OpenChannels::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.OpenChannels"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void OpenChannels::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
}
void OpenChannels::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
}

avro::ValidSchema ReplaceRangeResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.ReplaceRangeResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ReplaceRangeResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelChangeTime);
}
void ReplaceRangeResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_channelChangeTime);
}

avro::ValidSchema TruncateChannelsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.TruncateChannelsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void TruncateChannelsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_channelsTruncatedTime);
}
void TruncateChannelsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_channelsTruncatedTime);
}

avro::ValidSchema TruncateChannels::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.TruncateChannels"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void TruncateChannels::encode(avro::Encoder& e) const {
    avro::encode(e, m_channels);
}
void TruncateChannels::decode(avro::Decoder& d) {
    avro::decode(d, m_channels);
}

avro::ValidSchema ChannelData::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.ChannelData"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ChannelData::encode(avro::Encoder& e) const {
    avro::encode(e, m_data);
}
void ChannelData::decode(avro::Decoder& d) {
    avro::decode(d, m_data);
}

avro::ValidSchema OpenChannelsResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.OpenChannelsResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void OpenChannelsResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_channels);
}
void OpenChannelsResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_channels);
}

avro::ValidSchema ReplaceRange::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.ChannelDataLoad.ReplaceRange"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ReplaceRange::encode(avro::Encoder& e) const {
    avro::encode(e, m_changedInterval);
    avro::encode(e, m_channelIds);
    avro::encode(e, m_data);
}
void ReplaceRange::decode(avro::Decoder& d) {
    avro::decode(d, m_changedInterval);
    avro::decode(d, m_channelIds);
    avro::decode(d, m_data);
}

} //      oes::common::etp12::Protocol::ChannelDataLoad

namespace oes::common::etp12::Protocol::Dataspace {

avro::ValidSchema DeleteDataspaces::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Dataspace.DeleteDataspaces"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DeleteDataspaces::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
}
void DeleteDataspaces::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
}

avro::ValidSchema DeleteDataspacesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Dataspace.DeleteDataspacesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void DeleteDataspacesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void DeleteDataspacesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema GetDataspaces::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Dataspace.GetDataspaces"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataspaces::encode(avro::Encoder& e) const {
    avro::encode(e, m_storeLastWriteFilter);
}
void GetDataspaces::decode(avro::Decoder& d) {
    avro::decode(d, m_storeLastWriteFilter);
}

avro::ValidSchema PutDataspacesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Dataspace.PutDataspacesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataspacesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void PutDataspacesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema GetDataspacesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Dataspace.GetDataspacesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataspacesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataspaces);
}
void GetDataspacesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_dataspaces);
}

avro::ValidSchema PutDataspaces::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.Dataspace.PutDataspaces"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void PutDataspaces::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataspaces);
}
void PutDataspaces::decode(avro::Decoder& d) {
    avro::decode(d, m_dataspaces);
}

} //      oes::common::etp12::Protocol::Dataspace

namespace oes::common::etp12::Protocol::SupportedTypes {

avro::ValidSchema GetSupportedTypes::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.SupportedTypes.GetSupportedTypes"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetSupportedTypes::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_scope);
    avro::encode(e, m_returnEmptyTypes);
    avro::encode(e, m_countObjects);
}
void GetSupportedTypes::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_scope);
    avro::decode(d, m_returnEmptyTypes);
    avro::decode(d, m_countObjects);
}

avro::ValidSchema GetSupportedTypesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.SupportedTypes.GetSupportedTypesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetSupportedTypesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_supportedTypes);
}
void GetSupportedTypesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_supportedTypes);
}

} //      oes::common::etp12::Protocol::SupportedTypes

namespace oes::common::etp12::Protocol::CoreOSDU {

avro::ValidSchema ResumeSession::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.CoreOSDU.ResumeSession"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ResumeSession::encode(avro::Encoder& e) const {
    avro::encode(e, m_applicationName);
    avro::encode(e, m_applicationVersion);
    avro::encode(e, m_clientInstanceId);
    avro::encode(e, m_currentDateTime);
    avro::encode(e, m_sessionId);
}
void ResumeSession::decode(avro::Decoder& d) {
    avro::decode(d, m_applicationName);
    avro::decode(d, m_applicationVersion);
    avro::decode(d, m_clientInstanceId);
    avro::decode(d, m_currentDateTime);
    avro::decode(d, m_sessionId);
}

avro::ValidSchema ResumeSessionResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.CoreOSDU.ResumeSessionResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void ResumeSessionResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_applicationName);
    avro::encode(e, m_applicationVersion);
    avro::encode(e, m_serverInstanceId);
    avro::encode(e, m_currentDateTime);
    avro::encode(e, m_sessionId);
    avro::encode(e, m_earliestRetainedChangeTime);
}
void ResumeSessionResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_applicationName);
    avro::decode(d, m_applicationVersion);
    avro::decode(d, m_serverInstanceId);
    avro::decode(d, m_currentDateTime);
    avro::decode(d, m_sessionId);
    avro::decode(d, m_earliestRetainedChangeTime);
}

} //      oes::common::etp12::Protocol::CoreOSDU

namespace oes::common::etp12::Protocol::StoreOSDU {

avro::ValidSchema CopyDataObjectsByValue::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreOSDU.CopyDataObjectsByValue"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyDataObjectsByValue::encode(avro::Encoder& e) const {
    avro::encode(e, m_uri);
    avro::encode(e, m_sourcesDepth);
    avro::encode(e, m_dataObjectTypes);
}
void CopyDataObjectsByValue::decode(avro::Decoder& d) {
    avro::decode(d, m_uri);
    avro::decode(d, m_sourcesDepth);
    avro::decode(d, m_dataObjectTypes);
}

avro::ValidSchema CopyDataObjectsByValueResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.StoreOSDU.CopyDataObjectsByValueResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyDataObjectsByValueResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_copiedDataObjects);
}
void CopyDataObjectsByValueResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_copiedDataObjects);
}

} //      oes::common::etp12::Protocol::StoreOSDU

namespace oes::common::etp12::Protocol::DataspaceOSDU {

avro::ValidSchema CopyDataspacesContent::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.CopyDataspacesContent"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyDataspacesContent::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataspaces);
    avro::encode(e, m_targetDataspace);
}
void CopyDataspacesContent::decode(avro::Decoder& d) {
    avro::decode(d, m_dataspaces);
    avro::decode(d, m_targetDataspace);
}

avro::ValidSchema CopyDataspacesContentResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.CopyDataspacesContentResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyDataspacesContentResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void CopyDataspacesContentResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema CopyToDataspace::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.CopyToDataspace"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyToDataspace::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
    avro::encode(e, m_dataspaceUri);
}
void CopyToDataspace::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
    avro::decode(d, m_dataspaceUri);
}

avro::ValidSchema CopyToDataspaceResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.CopyToDataspaceResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyToDataspaceResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void CopyToDataspaceResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema GetDataspaceInfo::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.GetDataspaceInfo"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataspaceInfo::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
}
void GetDataspaceInfo::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
}

avro::ValidSchema LockDataspaces::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.LockDataspaces"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void LockDataspaces::encode(avro::Encoder& e) const {
    avro::encode(e, m_uris);
    avro::encode(e, m_lock);
}
void LockDataspaces::decode(avro::Decoder& d) {
    avro::decode(d, m_uris);
    avro::decode(d, m_lock);
}

avro::ValidSchema LockDataspacesResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.LockDataspacesResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void LockDataspacesResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_success);
}
void LockDataspacesResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_success);
}

avro::ValidSchema CopyDataspaceFromRemote::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.CopyDataspaceFromRemote"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyDataspaceFromRemote::encode(avro::Encoder& e) const {
    avro::encode(e, m_action);
    avro::encode(e, m_dataspacePath);
    avro::encode(e, m_sourceServer);
    avro::encode(e, m_extraData);
}
void CopyDataspaceFromRemote::decode(avro::Decoder& d) {
    avro::decode(d, m_action);
    avro::decode(d, m_dataspacePath);
    avro::decode(d, m_sourceServer);
    avro::decode(d, m_extraData);
}

avro::ValidSchema CopyDataspaceFromRemoteResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.CopyDataspaceFromRemoteResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void CopyDataspaceFromRemoteResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_tableData);
}
void CopyDataspaceFromRemoteResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_tableData);
}

avro::ValidSchema GetDataspaceInfoResponse::schema() {
#ifndef OES_EML_ETP12_NO_AVRO_JSON
    static const auto schema__(::oes::common::getValidSchemaOfType12(
        "Energistics.Etp.v12.Protocol.DataspaceOSDU.GetDataspaceInfoResponse"
    ));
    return schema__;
#else
    throw avro::Exception("Unsupported Operation in this build");
#endif
}
void GetDataspaceInfoResponse::encode(avro::Encoder& e) const {
    avro::encode(e, m_dataspaces);
}
void GetDataspaceInfoResponse::decode(avro::Decoder& d) {
    avro::decode(d, m_dataspaces);
}

} //      oes::common::etp12::Protocol::DataspaceOSDU
