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

enum class AnyArrayType : uint8_t  {
    eArrayOfBoolean = 0,
    eArrayOfInt = 1,
    eArrayOfLong = 2,
    eArrayOfFloat = 3,
    eArrayOfDouble = 4,
    eArrayOfString = 5,
    eBytes = 6,
};

enum class AnyLogicalArrayType : uint8_t  {
    eArrayOfBoolean = 0,
    eArrayOfInt8 = 1,
    eArrayOfUInt8 = 2,
    eArrayOfInt16LE = 3,
    eArrayOfInt32LE = 4,
    eArrayOfInt64LE = 5,
    eArrayOfUInt16LE = 6,
    eArrayOfUInt32LE = 7,
    eArrayOfUInt64LE = 8,
    eArrayOfFloat32LE = 9,
    eArrayOfDouble64LE = 10,
    eArrayOfInt16BE = 11,
    eArrayOfInt32BE = 12,
    eArrayOfInt64BE = 13,
    eArrayOfUInt16BE = 14,
    eArrayOfUInt32BE = 15,
    eArrayOfUInt64BE = 16,
    eArrayOfFloat32BE = 17,
    eArrayOfDouble64BE = 18,
    eArrayOfString = 19,
    eArrayOfCustom = 20,
};

enum class DataObjectCapabilityKind : uint8_t  {
    eActiveTimeoutPeriod = 0,
    eMaxContainedDataObjectCount = 1,
    eMaxDataObjectSize = 2,
    eOrphanedChildrenPrunedOnDelete = 3,
    eSupportsGet = 4,
    eSupportsPut = 5,
    eSupportsDelete = 6,
    eMaxSecondaryIndexCount = 7,
};

enum class EndpointCapabilityKind : uint8_t  {
    eActiveTimeoutPeriod = 0,
    eAuthorizationDetails = 1,
    eChangePropagationPeriod = 2,
    eChangeRetentionPeriod = 3,
    eMaxConcurrentMultipart = 4,
    eMaxDataObjectSize = 5,
    eMaxPartSize = 6,
    eMaxSessionClientCount = 7,
    eMaxSessionGlobalCount = 8,
    eMaxWebSocketFramePayloadSize = 9,
    eMaxWebSocketMessagePayloadSize = 10,
    eMultipartMessageTimeoutPeriod = 11,
    eResponseTimeoutPeriod = 12,
    eRequestSessionTimeoutPeriod = 13,
    eSessionEstablishmentTimeoutPeriod = 14,
    eSupportsAlternateRequestUris = 15,
    eSupportsMessageHeaderExtensions = 16,
};

enum class Protocol : uint16_t  {
    eCore = 0,
    eChannelStreaming = 1,
    eChannelDataFrame = 2,
    eDiscovery = 3,
    eStore = 4,
    eStoreNotification = 5,
    eGrowingObject = 6,
    eGrowingObjectNotification = 7,
    eDEPRECATED_8 = 8,
    eDataArray = 9,
    eRESERVED_10 = 10,
    eRESERVED_11 = 11,
    eRESERVED_12 = 12,
    eDiscoveryQuery = 13,
    eStoreQuery = 14,
    eRESERVED_15 = 15,
    eGrowingObjectQuery = 16,
    eRESERVED_17 = 17,
    eTransaction = 18,
    eRESERVED_19 = 19,
    eRESERVED_20 = 20,
    eChannelSubscribe = 21,
    eChannelDataLoad = 22,
    eRESERVED_23 = 23,
    eDataspace = 24,
    eSupportedTypes = 25,
    eCoreOSDU = 2400,
    eStoreOSDU = 2404,
    eDataspaceOSDU = 2424,
};

enum class ProtocolCapabilityKind : uint8_t  {
    eFrameChangeDetectionPeriod = 0,
    eMaxDataArraySize = 1,
    eMaxDataObjectSize = 2,
    eMaxFrameResponseRowCount = 3,
    eMaxIndexCount = 4,
    eMaxRangeChannelCount = 5,
    eMaxRangeDataItemCount = 6,
    eMaxResponseCount = 7,
    eMaxStreamingChannelsSessionCount = 8,
    eMaxSubscriptionSessionCount = 9,
    eMaxTransactionCount = 10,
    eSupportsSecondaryIndexFiltering = 11,
    eTransactionTimeoutPeriod = 12,
};

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

enum class ChannelDataKind : uint8_t  {
    eDateTime = 0,
    eElapsedTime = 1,
    eMeasuredDepth = 2,
    ePassIndexedDepth = 3,
    eTrueVerticalDepth = 4,
    eTypeBoolean = 5,
    eTypeInt = 6,
    eTypeLong = 7,
    eTypeFloat = 8,
    eTypeDouble = 9,
    eTypeString = 10,
    eTypeBytes = 11,
};

enum class ChannelIndexKind : uint8_t  {
    eDateTime = 0,
    eElapsedTime = 1,
    eMeasuredDepth = 2,
    eTrueVerticalDepth = 3,
    ePassIndexedDepth = 4,
    ePressure = 5,
    eTemperature = 6,
    eScalar = 7,
};

enum class IndexDirection : uint8_t  {
    eIncreasing = 0,
    eDecreasing = 1,
    eUnordered = 2,
};

enum class PassDirection : uint8_t  {
    eUp = 0,
    eHoldingSteady = 1,
    eDown = 2,
};

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

enum class ActiveStatusKind : uint8_t  {
    eActive = 0,
    eInactive = 1,
};

enum class ContextScopeKind : uint8_t  {
    eSelf = 0,
    eSources = 1,
    eTargets = 2,
    eSourcesOrSelf = 3,
    eTargetsOrSelf = 4,
};

enum class ObjectChangeKind : uint8_t  {
    eInsert = 0,
    eUpdate = 1,
    eAuthorized = 2,
    eJoined = 3,
    eUnjoined = 4,
    eJoinedSubscription = 5,
    eUnjoinedSubscription = 6,
};

enum class RelationshipKind : uint8_t  {
    ePrimary = 0,
    eSecondary = 1,
    eBoth = 2,
};

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

typedef std::array<uint8_t, 16> Uuid;
struct OES_COMMON_EXPORT ArrayOfBoolean {
    std::vector<bool> m_values;

    ArrayOfBoolean();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfBoolean_ptr = std::shared_ptr<ArrayOfBoolean>;

struct OES_COMMON_EXPORT ArrayOfBytes {
    std::vector<std::vector<uint8_t>> m_values;

    ArrayOfBytes();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfBytes_ptr = std::shared_ptr<ArrayOfBytes>;

struct OES_COMMON_EXPORT ArrayOfDouble {
    std::vector<double> m_values;

    ArrayOfDouble();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfDouble_ptr = std::shared_ptr<ArrayOfDouble>;

struct OES_COMMON_EXPORT ArrayOfFloat {
    std::vector<float> m_values;

    ArrayOfFloat();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfFloat_ptr = std::shared_ptr<ArrayOfFloat>;

struct OES_COMMON_EXPORT ArrayOfInt {
    std::vector<int32_t> m_values;

    ArrayOfInt();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfInt_ptr = std::shared_ptr<ArrayOfInt>;

struct OES_COMMON_EXPORT ArrayOfLong {
    std::vector<int64_t> m_values;

    ArrayOfLong();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfLong_ptr = std::shared_ptr<ArrayOfLong>;

struct OES_COMMON_EXPORT ArrayOfNullableBoolean {
    std::vector<std::optional<bool>> m_values;

    ArrayOfNullableBoolean();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfNullableBoolean_ptr = std::shared_ptr<ArrayOfNullableBoolean>;

struct OES_COMMON_EXPORT ArrayOfNullableInt {
    std::vector<std::optional<int32_t>> m_values;

    ArrayOfNullableInt();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfNullableInt_ptr = std::shared_ptr<ArrayOfNullableInt>;

struct OES_COMMON_EXPORT ArrayOfNullableLong {
    std::vector<std::optional<int64_t>> m_values;

    ArrayOfNullableLong();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfNullableLong_ptr = std::shared_ptr<ArrayOfNullableLong>;

struct OES_COMMON_EXPORT ArrayOfString {
    std::vector<std::string> m_values;

    ArrayOfString();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ArrayOfString_ptr = std::shared_ptr<ArrayOfString>;

struct OES_COMMON_EXPORT Contact {
    std::string m_organizationName;
    std::string m_contactName;
    std::string m_contactPhone;
    std::string m_contactEmail;

    Contact();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using Contact_ptr = std::shared_ptr<Contact>;

struct OES_COMMON_EXPORT ErrorInfo {
    std::string m_message;
    int32_t m_code;

    ErrorInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ErrorInfo_ptr = std::shared_ptr<ErrorInfo>;

struct OES_COMMON_EXPORT MessageHeader {
    int32_t m_protocol;
    int32_t m_messageType;
    int64_t m_correlationId;
    int64_t m_messageId;
    int32_t m_messageFlags;

    MessageHeader();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using MessageHeader_ptr = std::shared_ptr<MessageHeader>;

struct OES_COMMON_EXPORT Version {
    int32_t m_major;
    int32_t m_minor;
    int32_t m_revision;
    int32_t m_patch;

    Version();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using Version_ptr = std::shared_ptr<Version>;

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

struct OES_COMMON_EXPORT ChannelChangeRequestInfo {
    int64_t m_sinceChangeTime;
    std::vector<int64_t> m_channelIds;

    ChannelChangeRequestInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ChannelChangeRequestInfo_ptr = std::shared_ptr<ChannelChangeRequestInfo>;

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

struct OES_COMMON_EXPORT DataArrayIdentifier {
    std::string m_uri;
    std::string m_pathInResource;

    DataArrayIdentifier();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DataArrayIdentifier_ptr = std::shared_ptr<DataArrayIdentifier>;

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT ObjectPart {
    std::string m_uid;
    std::vector<uint8_t> m_data;

    ObjectPart();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ObjectPart_ptr = std::shared_ptr<ObjectPart>;

struct OES_COMMON_EXPORT PutResponse {
    std::vector<std::string> m_createdContainedObjectUris;
    std::vector<std::string> m_deletedContainedObjectUris;
    std::vector<std::string> m_joinedContainedObjectUris;
    std::vector<std::string> m_unjoinedContainedObjectUris;

    PutResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using PutResponse_ptr = std::shared_ptr<PutResponse>;

struct OES_COMMON_EXPORT RemoteServer {
    std::string m_sourcePartition;
    std::string m_sourceJwt;
    std::string m_sourceDataspacePath;
    std::string m_sourceServer;

    RemoteServer();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using RemoteServer_ptr = std::shared_ptr<RemoteServer>;

struct OES_COMMON_EXPORT TableData {
    int32_t m_copyStatus;
    int32_t m_tableStatus;
    std::string m_tableName;
    std::vector<uint8_t> m_tableData;

    TableData();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using TableData_ptr = std::shared_ptr<TableData>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

using AnyArray_item_v = std::variant<
    ArrayOfBoolean,
    ArrayOfInt,
    ArrayOfLong,
    ArrayOfFloat,
    ArrayOfDouble,
    ArrayOfString,
    std::vector<uint8_t>
>;
enum class AnyArray_item_e : uint8_t {
    eArrayOfBoolean,
    eArrayOfInt,
    eArrayOfLong,
    eArrayOfFloat,
    eArrayOfDouble,
    eArrayOfString,
    eBytes,
};

struct OES_COMMON_EXPORT AnyArray {
    AnyArray_item_v m_item;

    AnyArray();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using AnyArray_ptr = std::shared_ptr<AnyArray>;

struct OES_COMMON_EXPORT AttributeMetadataRecord {
    int32_t m_attributeId;
    std::string m_attributeName;
    ChannelData::ChannelDataKind m_dataKind;
    std::string m_uom;
    std::string m_depthDatum;
    std::string m_attributePropertyKindUri;
    std::vector<int32_t> m_axisVectorLengths;

    AttributeMetadataRecord();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using AttributeMetadataRecord_ptr = std::shared_ptr<AttributeMetadataRecord>;

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

struct OES_COMMON_EXPORT PassIndexedDepth {
    int64_t m_pass;
    PassDirection m_direction;
    double m_depth;

    PassIndexedDepth();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using PassIndexedDepth_ptr = std::shared_ptr<PassIndexedDepth>;

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

struct OES_COMMON_EXPORT GetDataSubarraysType {
    DataArrayIdentifier m_uid;
    std::vector<int64_t> m_starts;
    std::vector<int64_t> m_counts;

    GetDataSubarraysType();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using GetDataSubarraysType_ptr = std::shared_ptr<GetDataSubarraysType>;

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT ContextInfo {
    std::string m_uri;
    int32_t m_depth;
    std::vector<std::string> m_dataObjectTypes;
    RelationshipKind m_navigableEdges;
    bool m_includeSecondaryTargets;
    bool m_includeSecondarySources;

    ContextInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ContextInfo_ptr = std::shared_ptr<ContextInfo>;

struct OES_COMMON_EXPORT SupportedType {
    std::string m_dataObjectType;
    std::optional<int32_t> m_objectCount;
    RelationshipKind m_relationshipKind;

    SupportedType();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using SupportedType_ptr = std::shared_ptr<SupportedType>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

struct OES_COMMON_EXPORT AnySubarray {
    int64_t m_start;
    AnyArray m_slice;

    AnySubarray();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using AnySubarray_ptr = std::shared_ptr<AnySubarray>;

using IndexValue_item_v = std::variant<
    std::monostate,
    int64_t,
    double,
    ChannelData::PassIndexedDepth
>;
enum class IndexValue_item_e : uint8_t {
    eNull,
    eLong,
    eDouble,
    ePassIndexedDepth,
};

struct OES_COMMON_EXPORT IndexValue {
    IndexValue_item_v m_item;

    IndexValue();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using IndexValue_ptr = std::shared_ptr<IndexValue>;

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::DataArrayTypes {

struct OES_COMMON_EXPORT DataArray {
    std::vector<int64_t> m_dimensions;
    AnyArray m_data;

    DataArray();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DataArray_ptr = std::shared_ptr<DataArray>;

struct OES_COMMON_EXPORT PutDataSubarraysType {
    DataArrayIdentifier m_uid;
    AnyArray m_data;
    std::vector<int64_t> m_starts;
    std::vector<int64_t> m_counts;

    PutDataSubarraysType();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using PutDataSubarraysType_ptr = std::shared_ptr<PutDataSubarraysType>;

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT SubscriptionInfo {
    ContextInfo m_context;
    ContextScopeKind m_scope;
    std::array<uint8_t, 16> m_requestUuid;
    bool m_includeObjectData;
    std::string m_format;

    SubscriptionInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using SubscriptionInfo_ptr = std::shared_ptr<SubscriptionInfo>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

struct OES_COMMON_EXPORT AnySparseArray {
    std::vector<AnySubarray> m_slices;

    AnySparseArray();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using AnySparseArray_ptr = std::shared_ptr<AnySparseArray>;

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

struct OES_COMMON_EXPORT ChannelSubscribeInfo {
    int64_t m_channelId;
    IndexValue m_startIndex;
    bool m_dataChanges;
    std::optional<int32_t> m_requestLatestIndexCount;

    ChannelSubscribeInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ChannelSubscribeInfo_ptr = std::shared_ptr<ChannelSubscribeInfo>;

struct OES_COMMON_EXPORT TruncateInfo {
    int64_t m_channelId;
    IndexValue m_newEndIndex;

    TruncateInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using TruncateInfo_ptr = std::shared_ptr<TruncateInfo>;

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT IndexInterval {
    IndexValue m_startIndex;
    IndexValue m_endIndex;
    std::string m_uom;
    std::string m_depthDatum;

    IndexInterval();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using IndexInterval_ptr = std::shared_ptr<IndexInterval>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

using DataValue_item_v = std::variant<
    std::monostate,
    bool,
    int32_t,
    int64_t,
    float,
    double,
    std::string,
    ArrayOfBoolean,
    ArrayOfNullableBoolean,
    ArrayOfInt,
    ArrayOfNullableInt,
    ArrayOfLong,
    ArrayOfNullableLong,
    ArrayOfFloat,
    ArrayOfDouble,
    ArrayOfString,
    ArrayOfBytes,
    std::vector<uint8_t>,
    AnySparseArray
>;
enum class DataValue_item_e : uint8_t {
    eNull,
    eBool,
    eInt,
    eLong,
    eFloat,
    eDouble,
    eString,
    eArrayOfBoolean,
    eArrayOfNullableBoolean,
    eArrayOfInt,
    eArrayOfNullableInt,
    eArrayOfLong,
    eArrayOfNullableLong,
    eArrayOfFloat,
    eArrayOfDouble,
    eArrayOfString,
    eArrayOfBytes,
    eBytes,
    eAnySparseArray,
};

struct OES_COMMON_EXPORT DataValue {
    DataValue_item_v m_item;

    DataValue();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DataValue_ptr = std::shared_ptr<DataValue>;

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

struct OES_COMMON_EXPORT ChannelRangeInfo {
    std::vector<int64_t> m_channelIds;
    Object::IndexInterval m_interval;
    std::vector<Object::IndexInterval> m_secondaryIntervals;

    ChannelRangeInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ChannelRangeInfo_ptr = std::shared_ptr<ChannelRangeInfo>;

struct OES_COMMON_EXPORT IndexMetadataRecord {
    ChannelIndexKind m_indexKind;
    Object::IndexInterval m_interval;
    IndexDirection m_direction;
    std::string m_name;
    std::string m_uom;
    std::string m_depthDatum;
    std::string m_indexPropertyKindUri;
    bool m_filterable;

    IndexMetadataRecord();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using IndexMetadataRecord_ptr = std::shared_ptr<IndexMetadataRecord>;

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT ChangeAnnotation {
    int64_t m_changeTime;
    IndexInterval m_interval;

    ChangeAnnotation();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ChangeAnnotation_ptr = std::shared_ptr<ChangeAnnotation>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

struct OES_COMMON_EXPORT DataAttribute {
    int32_t m_attributeId;
    DataValue m_attributeValue;

    DataAttribute();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DataAttribute_ptr = std::shared_ptr<DataAttribute>;

struct OES_COMMON_EXPORT MessageHeaderExtension {
    std::map<std::string, DataValue> m_extension;

    MessageHeaderExtension();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using MessageHeaderExtension_ptr = std::shared_ptr<MessageHeaderExtension>;

struct OES_COMMON_EXPORT SupportedDataObject {
    std::string m_qualifiedType;
    std::map<std::string, DataValue> m_dataObjectCapabilities;

    SupportedDataObject();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using SupportedDataObject_ptr = std::shared_ptr<SupportedDataObject>;

struct OES_COMMON_EXPORT SupportedProtocol {
    int32_t m_protocol;
    Version m_protocolVersion;
    std::string m_role;
    std::map<std::string, DataValue> m_protocolCapabilities;

    SupportedProtocol();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using SupportedProtocol_ptr = std::shared_ptr<SupportedProtocol>;

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

struct OES_COMMON_EXPORT ChannelMetadataRecord {
    std::string m_uri;
    int64_t m_id;
    std::vector<IndexMetadataRecord> m_indexes;
    std::string m_channelName;
    ChannelDataKind m_dataKind;
    std::string m_uom;
    std::string m_depthDatum;
    std::string m_channelClassUri;
    Object::ActiveStatusKind m_status;
    std::string m_source;
    std::vector<int32_t> m_axisVectorLengths;
    std::vector<AttributeMetadataRecord> m_attributeMetadata;
    std::map<std::string, DataValue> m_customData;

    ChannelMetadataRecord();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ChannelMetadataRecord_ptr = std::shared_ptr<ChannelMetadataRecord>;

struct OES_COMMON_EXPORT FrameChannelMetadataRecord {
    std::string m_uri;
    std::string m_channelName;
    ChannelDataKind m_dataKind;
    std::string m_uom;
    std::string m_depthDatum;
    std::string m_channelPropertyKindUri;
    Object::ActiveStatusKind m_status;
    std::string m_source;
    std::vector<int32_t> m_axisVectorLengths;
    std::vector<AttributeMetadataRecord> m_attributeMetadata;
    std::map<std::string, DataValue> m_customData;

    FrameChannelMetadataRecord();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using FrameChannelMetadataRecord_ptr = std::shared_ptr<FrameChannelMetadataRecord>;

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

struct OES_COMMON_EXPORT DataArrayMetadata {
    std::vector<int64_t> m_dimensions;
    std::vector<int64_t> m_preferredSubarrayDimensions;
    AnyArrayType m_transportArrayType;
    AnyLogicalArrayType m_logicalArrayType;
    int64_t m_storeLastWrite;
    int64_t m_storeCreated;
    std::map<std::string, DataValue> m_customData;

    DataArrayMetadata();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DataArrayMetadata_ptr = std::shared_ptr<DataArrayMetadata>;

struct OES_COMMON_EXPORT PutDataArraysType {
    DataArrayIdentifier m_uid;
    DataArray m_array;
    std::map<std::string, DataValue> m_customData;

    PutDataArraysType();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using PutDataArraysType_ptr = std::shared_ptr<PutDataArraysType>;

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT ChangeResponseInfo {
    int64_t m_responseTimestamp;
    std::map<std::string, std::vector<ChangeAnnotation>> m_changes;

    ChangeResponseInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ChangeResponseInfo_ptr = std::shared_ptr<ChangeResponseInfo>;

struct OES_COMMON_EXPORT Dataspace {
    std::string m_uri;
    std::string m_path;
    int64_t m_storeLastWrite;
    int64_t m_storeCreated;
    std::map<std::string, DataValue> m_customData;

    Dataspace();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using Dataspace_ptr = std::shared_ptr<Dataspace>;

struct OES_COMMON_EXPORT DeletedResource {
    std::string m_uri;
    int64_t m_deletedTime;
    std::map<std::string, DataValue> m_customData;

    DeletedResource();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DeletedResource_ptr = std::shared_ptr<DeletedResource>;

struct OES_COMMON_EXPORT Edge {
    std::string m_sourceUri;
    std::string m_targetUri;
    RelationshipKind m_relationshipKind;
    std::map<std::string, DataValue> m_customData;

    Edge();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using Edge_ptr = std::shared_ptr<Edge>;

struct OES_COMMON_EXPORT PartsMetadataInfo {
    std::string m_uri;
    std::string m_name;
    ChannelData::IndexMetadataRecord m_index;
    std::map<std::string, DataValue> m_customData;

    PartsMetadataInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using PartsMetadataInfo_ptr = std::shared_ptr<PartsMetadataInfo>;

struct OES_COMMON_EXPORT Resource {
    std::string m_uri;
    std::vector<std::string> m_alternateUris;
    std::string m_name;
    std::optional<int32_t> m_sourceCount;
    std::optional<int32_t> m_targetCount;
    int64_t m_lastChanged;
    int64_t m_storeLastWrite;
    int64_t m_storeCreated;
    ActiveStatusKind m_activeStatus;
    std::map<std::string, DataValue> m_customData;

    Resource();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using Resource_ptr = std::shared_ptr<Resource>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes {

struct OES_COMMON_EXPORT ServerCapabilities {
    std::string m_applicationName;
    std::string m_applicationVersion;
    Contact m_contactInformation;
    std::vector<std::string> m_supportedCompression;
    std::vector<std::string> m_supportedEncodings;
    std::vector<std::string> m_supportedFormats;
    std::vector<SupportedDataObject> m_supportedDataObjects;
    std::vector<SupportedProtocol> m_supportedProtocols;
    std::map<std::string, DataValue> m_endpointCapabilities;

    ServerCapabilities();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ServerCapabilities_ptr = std::shared_ptr<ServerCapabilities>;

} //      oes::common::etp12::Datatypes

namespace oes::common::etp12::Datatypes::ChannelData {

struct OES_COMMON_EXPORT DataItem {
    int64_t m_channelId;
    std::vector<IndexValue> m_indexes;
    DataValue m_value;
    std::vector<DataAttribute> m_valueAttributes;

    DataItem();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DataItem_ptr = std::shared_ptr<DataItem>;

struct OES_COMMON_EXPORT FramePoint {
    DataValue m_value;
    std::vector<DataAttribute> m_valueAttributes;

    FramePoint();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using FramePoint_ptr = std::shared_ptr<FramePoint>;

struct OES_COMMON_EXPORT OpenChannelInfo {
    ChannelMetadataRecord m_metadata;
    bool m_preferRealtime;
    bool m_dataChanges;

    OpenChannelInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using OpenChannelInfo_ptr = std::shared_ptr<OpenChannelInfo>;

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::DataArrayTypes {

struct OES_COMMON_EXPORT PutUninitializedDataArrayType {
    DataArrayIdentifier m_uid;
    DataArrayMetadata m_metadata;

    PutUninitializedDataArrayType();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using PutUninitializedDataArrayType_ptr = std::shared_ptr<PutUninitializedDataArrayType>;

} //      oes::common::etp12::Datatypes::DataArrayTypes

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT DataObject {
    Resource m_resource;
    std::string m_format;
    std::optional<std::array<uint8_t, 16>> m_blobId;
    std::vector<uint8_t> m_data;

    DataObject();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using DataObject_ptr = std::shared_ptr<DataObject>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Datatypes::ChannelData {

struct OES_COMMON_EXPORT FrameRow {
    std::vector<IndexValue> m_indexes;
    std::vector<FramePoint> m_points;

    FrameRow();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using FrameRow_ptr = std::shared_ptr<FrameRow>;

} //      oes::common::etp12::Datatypes::ChannelData

namespace oes::common::etp12::Datatypes::Object {

struct OES_COMMON_EXPORT ObjectChange {
    ObjectChangeKind m_changeKind;
    int64_t m_changeTime;
    DataObject m_dataObject;

    ObjectChange();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const;
    void decode(avro::Decoder&);
};
using ObjectChange_ptr = std::shared_ptr<ObjectChange>;

} //      oes::common::etp12::Datatypes::Object

namespace oes::common::etp12::Protocol::Core {

struct OES_COMMON_EXPORT Acknowledge final : AbstractMessage {
    // Warning: No state

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, AuxResponse,
        Core,(0), Acknowledge,(1001)
    );
};
using Acknowledge_ptr = std::shared_ptr<Acknowledge>;

struct OES_COMMON_EXPORT Authorize final : AbstractMessage {
    std::string m_authorization;
    std::map<std::string, std::string> m_supplementalAuthorization;

    Authorize();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        Core,(0), Authorize,(6)
    );
};
using Authorize_ptr = std::shared_ptr<Authorize>;

struct OES_COMMON_EXPORT AuthorizeResponse final : AbstractMessage {
    bool m_success;
    std::vector<std::string> m_challenges;

    AuthorizeResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        Core,(0), AuthorizeResponse,(7)
    );
};
using AuthorizeResponse_ptr = std::shared_ptr<AuthorizeResponse>;

struct OES_COMMON_EXPORT CloseSession final : AbstractMessage {
    std::string m_reason;

    CloseSession();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        Core,(0), CloseSession,(5)
    );
};
using CloseSession_ptr = std::shared_ptr<CloseSession>;

struct OES_COMMON_EXPORT Ping final : AbstractMessage {
    int64_t m_currentDateTime;

    Ping();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        Core,(0), Ping,(8)
    );
};
using Ping_ptr = std::shared_ptr<Ping>;

struct OES_COMMON_EXPORT Pong final : AbstractMessage {
    int64_t m_currentDateTime;

    Pong();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        Core,(0), Pong,(9)
    );
};
using Pong_ptr = std::shared_ptr<Pong>;

struct OES_COMMON_EXPORT ProtocolException final : AbstractMessage {
    std::optional<Datatypes::ErrorInfo> m_error;
    std::map<std::string, Datatypes::ErrorInfo> m_errors;

    ProtocolException();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, AuxResponse,
        Core,(0), ProtocolException,(1000)
    );
};
using ProtocolException_ptr = std::shared_ptr<ProtocolException>;

struct OES_COMMON_EXPORT OpenSession final : AbstractMessage {
    std::string m_applicationName;
    std::string m_applicationVersion;
    std::array<uint8_t, 16> m_serverInstanceId;
    std::vector<Datatypes::SupportedProtocol> m_supportedProtocols;
    std::vector<Datatypes::SupportedDataObject> m_supportedDataObjects;
    std::string m_supportedCompression;
    std::vector<std::string> m_supportedFormats;
    int64_t m_currentDateTime;
    int64_t m_earliestRetainedChangeTime;
    std::array<uint8_t, 16> m_sessionId;
    std::map<std::string, Datatypes::DataValue> m_endpointCapabilities;

    OpenSession();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        Core,(0), OpenSession,(2)
    );
};
using OpenSession_ptr = std::shared_ptr<OpenSession>;

struct OES_COMMON_EXPORT RequestSession final : AbstractMessage {
    std::string m_applicationName;
    std::string m_applicationVersion;
    std::array<uint8_t, 16> m_clientInstanceId;
    std::vector<Datatypes::SupportedProtocol> m_requestedProtocols;
    std::vector<Datatypes::SupportedDataObject> m_supportedDataObjects;
    std::vector<std::string> m_supportedCompression;
    std::vector<std::string> m_supportedFormats;
    int64_t m_currentDateTime;
    int64_t m_earliestRetainedChangeTime;
    bool m_serverAuthorizationRequired;
    std::map<std::string, Datatypes::DataValue> m_endpointCapabilities;

    RequestSession();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        Core,(0), RequestSession,(1)
    );
};
using RequestSession_ptr = std::shared_ptr<RequestSession>;

} //      oes::common::etp12::Protocol::Core

namespace oes::common::etp12::Protocol::ChannelStreaming {

struct OES_COMMON_EXPORT StartStreaming final : AbstractMessage {
    // Warning: No state

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelStreaming,(1), StartStreaming,(3)
    );
};
using StartStreaming_ptr = std::shared_ptr<StartStreaming>;

struct OES_COMMON_EXPORT StopStreaming final : AbstractMessage {
    // Warning: No state

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelStreaming,(1), StopStreaming,(4)
    );
};
using StopStreaming_ptr = std::shared_ptr<StopStreaming>;

struct OES_COMMON_EXPORT TruncateChannels final : AbstractMessage {
    std::vector<Datatypes::ChannelData::TruncateInfo> m_channels;

    TruncateChannels();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Notification,
        ChannelStreaming,(1), TruncateChannels,(5)
    );
};
using TruncateChannels_ptr = std::shared_ptr<TruncateChannels>;

struct OES_COMMON_EXPORT ChannelMetadata final : AbstractMessage {
    std::vector<Datatypes::ChannelData::ChannelMetadataRecord> m_channels;

    ChannelMetadata();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        ChannelStreaming,(1), ChannelMetadata,(1)
    );
};
using ChannelMetadata_ptr = std::shared_ptr<ChannelMetadata>;

struct OES_COMMON_EXPORT ChannelData final : AbstractMessage {
    std::vector<Datatypes::ChannelData::DataItem> m_data;

    ChannelData();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Notification,
        ChannelStreaming,(1), ChannelData,(2)
    );
};
using ChannelData_ptr = std::shared_ptr<ChannelData>;

} //      oes::common::etp12::Protocol::ChannelStreaming

namespace oes::common::etp12::Protocol::ChannelDataFrame {

struct OES_COMMON_EXPORT GetFrameMetadata final : AbstractMessage {
    std::string m_uri;
    bool m_includeAllChannelSecondaryIndexes;

    GetFrameMetadata();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        ChannelDataFrame,(2), GetFrameMetadata,(1)
    );
};
using GetFrameMetadata_ptr = std::shared_ptr<GetFrameMetadata>;

struct OES_COMMON_EXPORT CancelGetFrame final : AbstractMessage {
    std::array<uint8_t, 16> m_requestUuid;

    CancelGetFrame();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelDataFrame,(2), CancelGetFrame,(5)
    );
};
using CancelGetFrame_ptr = std::shared_ptr<CancelGetFrame>;

struct OES_COMMON_EXPORT GetFrame final : AbstractMessage {
    std::string m_uri;
    bool m_includeAllChannelSecondaryIndexes;
    Datatypes::Object::IndexInterval m_requestedInterval;
    std::array<uint8_t, 16> m_requestUuid;
    std::vector<Datatypes::Object::IndexInterval> m_requestedSecondaryIntervals;

    GetFrame();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        ChannelDataFrame,(2), GetFrame,(3)
    );
};
using GetFrame_ptr = std::shared_ptr<GetFrame>;

struct OES_COMMON_EXPORT GetFrameResponseHeader final : AbstractMessage {
    std::vector<std::string> m_channelUris;
    std::vector<Datatypes::ChannelData::IndexMetadataRecord> m_indexes;

    GetFrameResponseHeader();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        ChannelDataFrame,(2), GetFrameResponseHeader,(4)
    );
};
using GetFrameResponseHeader_ptr = std::shared_ptr<GetFrameResponseHeader>;

struct OES_COMMON_EXPORT GetFrameMetadataResponse final : AbstractMessage {
    std::string m_uri;
    std::vector<Datatypes::ChannelData::IndexMetadataRecord> m_indexes;
    std::vector<Datatypes::ChannelData::FrameChannelMetadataRecord> m_channels;

    GetFrameMetadataResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        ChannelDataFrame,(2), GetFrameMetadataResponse,(2)
    );
};
using GetFrameMetadataResponse_ptr = std::shared_ptr<GetFrameMetadataResponse>;

struct OES_COMMON_EXPORT GetFrameResponseRows final : AbstractMessage {
    std::vector<Datatypes::ChannelData::FrameRow> m_frame;

    GetFrameResponseRows();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_frame; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, AuxResponse,
        ChannelDataFrame,(2), GetFrameResponseRows,(6)
    );
};
using GetFrameResponseRows_ptr = std::shared_ptr<GetFrameResponseRows>;

} //      oes::common::etp12::Protocol::ChannelDataFrame

namespace oes::common::etp12::Protocol::Discovery {

struct OES_COMMON_EXPORT GetDeletedResources final : AbstractMessage {
    std::string m_dataspaceUri;
    std::optional<int64_t> m_deleteTimeFilter;
    std::vector<std::string> m_dataObjectTypes;

    GetDeletedResources();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        Discovery,(3), GetDeletedResources,(5)
    );
};
using GetDeletedResources_ptr = std::shared_ptr<GetDeletedResources>;

struct OES_COMMON_EXPORT GetResources final : AbstractMessage {
    Datatypes::Object::ContextInfo m_context;
    Datatypes::Object::ContextScopeKind m_scope;
    bool m_countObjects;
    std::optional<int64_t> m_storeLastWriteFilter;
    std::optional<Datatypes::Object::ActiveStatusKind> m_activeStatusFilter;
    bool m_includeEdges;

    GetResources();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        Discovery,(3), GetResources,(1)
    );
};
using GetResources_ptr = std::shared_ptr<GetResources>;

struct OES_COMMON_EXPORT GetDeletedResourcesResponse final : AbstractMessage {
    std::vector<Datatypes::Object::DeletedResource> m_deletedResources;

    GetDeletedResourcesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_deletedResources; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        Discovery,(3), GetDeletedResourcesResponse,(6)
    );
};
using GetDeletedResourcesResponse_ptr = std::shared_ptr<GetDeletedResourcesResponse>;

struct OES_COMMON_EXPORT GetResourcesEdgesResponse final : AbstractMessage {
    std::vector<Datatypes::Object::Edge> m_edges;

    GetResourcesEdgesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_edges; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, AuxResponse,
        Discovery,(3), GetResourcesEdgesResponse,(7)
    );
};
using GetResourcesEdgesResponse_ptr = std::shared_ptr<GetResourcesEdgesResponse>;

struct OES_COMMON_EXPORT GetResourcesResponse final : AbstractMessage {
    std::vector<Datatypes::Object::Resource> m_resources;

    GetResourcesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_resources; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        Discovery,(3), GetResourcesResponse,(4)
    );
};
using GetResourcesResponse_ptr = std::shared_ptr<GetResourcesResponse>;

} //      oes::common::etp12::Protocol::Discovery

namespace oes::common::etp12::Protocol::Store {

struct OES_COMMON_EXPORT DeleteDataObjects final : AbstractMessage {
    std::map<std::string, std::string> m_uris;
    bool m_pruneContainedObjects;

    DeleteDataObjects();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Del, Request,
        Store,(4), DeleteDataObjects,(3)
    );
};
using DeleteDataObjects_ptr = std::shared_ptr<DeleteDataObjects>;

struct OES_COMMON_EXPORT GetDataObjects final : AbstractMessage {
    std::map<std::string, std::string> m_uris;
    std::string m_format;

    GetDataObjects();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        Store,(4), GetDataObjects,(1)
    );
};
using GetDataObjects_ptr = std::shared_ptr<GetDataObjects>;

struct OES_COMMON_EXPORT Chunk final : AbstractMessage {
    std::array<uint8_t, 16> m_blobId;
    std::vector<uint8_t> m_data;
    bool m_final;

    Chunk();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Chunk,
        Store,(4), Chunk,(8)
    );
};
using Chunk_ptr = std::shared_ptr<Chunk>;

struct OES_COMMON_EXPORT DeleteDataObjectsResponse final : AbstractMessage {
    std::map<std::string, Datatypes::ArrayOfString> m_deletedUris;

    DeleteDataObjectsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_deletedUris; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Del, Response,
        Store,(4), DeleteDataObjectsResponse,(10)
    );
};
using DeleteDataObjectsResponse_ptr = std::shared_ptr<DeleteDataObjectsResponse>;

struct OES_COMMON_EXPORT PutDataObjectsResponse final : AbstractMessage {
    std::map<std::string, Datatypes::Object::PutResponse> m_success;

    PutDataObjectsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Response,
        Store,(4), PutDataObjectsResponse,(9)
    );
};
using PutDataObjectsResponse_ptr = std::shared_ptr<PutDataObjectsResponse>;

struct OES_COMMON_EXPORT GetDataObjectsResponse final : AbstractMessage {
    std::map<std::string, Datatypes::Object::DataObject> m_dataObjects;

    GetDataObjectsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_dataObjects; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        Store,(4), GetDataObjectsResponse,(4)
    );
};
using GetDataObjectsResponse_ptr = std::shared_ptr<GetDataObjectsResponse>;

struct OES_COMMON_EXPORT PutDataObjects final : AbstractMessage {
    std::map<std::string, Datatypes::Object::DataObject> m_dataObjects;
    bool m_pruneContainedObjects;

    PutDataObjects();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Request,
        Store,(4), PutDataObjects,(2)
    );
};
using PutDataObjects_ptr = std::shared_ptr<PutDataObjects>;

} //      oes::common::etp12::Protocol::Store

namespace oes::common::etp12::Protocol::StoreNotification {

struct OES_COMMON_EXPORT SubscribeNotificationsResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    SubscribeNotificationsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Sub, Response,
        StoreNotification,(5), SubscribeNotificationsResponse,(10)
    );
};
using SubscribeNotificationsResponse_ptr = std::shared_ptr<SubscribeNotificationsResponse>;

struct OES_COMMON_EXPORT Chunk final : AbstractMessage {
    std::array<uint8_t, 16> m_blobId;
    std::vector<uint8_t> m_data;
    bool m_final;

    Chunk();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Chunk,
        StoreNotification,(5), Chunk,(9)
    );
};
using Chunk_ptr = std::shared_ptr<Chunk>;

struct OES_COMMON_EXPORT ObjectAccessRevoked final : AbstractMessage {
    std::string m_uri;
    int64_t m_changeTime;
    std::array<uint8_t, 16> m_requestUuid;

    ObjectAccessRevoked();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        StoreNotification,(5), ObjectAccessRevoked,(5)
    );
};
using ObjectAccessRevoked_ptr = std::shared_ptr<ObjectAccessRevoked>;

struct OES_COMMON_EXPORT ObjectDeleted final : AbstractMessage {
    std::string m_uri;
    int64_t m_changeTime;
    std::array<uint8_t, 16> m_requestUuid;

    ObjectDeleted();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        StoreNotification,(5), ObjectDeleted,(3)
    );
};
using ObjectDeleted_ptr = std::shared_ptr<ObjectDeleted>;

struct OES_COMMON_EXPORT SubscriptionEnded final : AbstractMessage {
    std::string m_reason;
    std::array<uint8_t, 16> m_requestUuid;

    SubscriptionEnded();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        StoreNotification,(5), SubscriptionEnded,(7)
    );
};
using SubscriptionEnded_ptr = std::shared_ptr<SubscriptionEnded>;

struct OES_COMMON_EXPORT UnsubscribeNotifications final : AbstractMessage {
    std::array<uint8_t, 16> m_requestUuid;

    UnsubscribeNotifications();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Sub, Request,
        StoreNotification,(5), UnsubscribeNotifications,(4)
    );
};
using UnsubscribeNotifications_ptr = std::shared_ptr<UnsubscribeNotifications>;

struct OES_COMMON_EXPORT SubscribeNotifications final : AbstractMessage {
    std::map<std::string, Datatypes::Object::SubscriptionInfo> m_request;

    SubscribeNotifications();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Sub, Request,
        StoreNotification,(5), SubscribeNotifications,(6)
    );
};
using SubscribeNotifications_ptr = std::shared_ptr<SubscribeNotifications>;

struct OES_COMMON_EXPORT UnsolicitedStoreNotifications final : AbstractMessage {
    std::vector<Datatypes::Object::SubscriptionInfo> m_subscriptions;

    UnsolicitedStoreNotifications();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        StoreNotification,(5), UnsolicitedStoreNotifications,(8)
    );
};
using UnsolicitedStoreNotifications_ptr = std::shared_ptr<UnsolicitedStoreNotifications>;

struct OES_COMMON_EXPORT ObjectActiveStatusChanged final : AbstractMessage {
    Datatypes::Object::ActiveStatusKind m_activeStatus;
    int64_t m_changeTime;
    Datatypes::Object::Resource m_resource;
    std::array<uint8_t, 16> m_requestUuid;

    ObjectActiveStatusChanged();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        StoreNotification,(5), ObjectActiveStatusChanged,(11)
    );
};
using ObjectActiveStatusChanged_ptr = std::shared_ptr<ObjectActiveStatusChanged>;

struct OES_COMMON_EXPORT ObjectChanged final : AbstractMessage {
    Datatypes::Object::ObjectChange m_change;
    std::array<uint8_t, 16> m_requestUuid;

    ObjectChanged();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, None, Notification,
        StoreNotification,(5), ObjectChanged,(2)
    );
};
using ObjectChanged_ptr = std::shared_ptr<ObjectChanged>;

} //      oes::common::etp12::Protocol::StoreNotification

namespace oes::common::etp12::Protocol::GrowingObject {

struct OES_COMMON_EXPORT DeleteParts final : AbstractMessage {
    std::string m_uri;
    std::map<std::string, std::string> m_uids;

    DeleteParts();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Del, Request,
        GrowingObject,(6), DeleteParts,(1)
    );
};
using DeleteParts_ptr = std::shared_ptr<DeleteParts>;

struct OES_COMMON_EXPORT DeletePartsResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    DeletePartsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Del, Response,
        GrowingObject,(6), DeletePartsResponse,(11)
    );
};
using DeletePartsResponse_ptr = std::shared_ptr<DeletePartsResponse>;

struct OES_COMMON_EXPORT GetChangeAnnotations final : AbstractMessage {
    int64_t m_sinceChangeTime;
    std::map<std::string, std::string> m_uris;
    bool m_latestOnly;

    GetChangeAnnotations();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        GrowingObject,(6), GetChangeAnnotations,(19)
    );
};
using GetChangeAnnotations_ptr = std::shared_ptr<GetChangeAnnotations>;

struct OES_COMMON_EXPORT GetGrowingDataObjectsHeader final : AbstractMessage {
    std::map<std::string, std::string> m_uris;
    std::string m_format;

    GetGrowingDataObjectsHeader();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        GrowingObject,(6), GetGrowingDataObjectsHeader,(14)
    );
};
using GetGrowingDataObjectsHeader_ptr = std::shared_ptr<GetGrowingDataObjectsHeader>;

struct OES_COMMON_EXPORT GetParts final : AbstractMessage {
    std::string m_uri;
    std::string m_format;
    std::map<std::string, std::string> m_uids;

    GetParts();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        GrowingObject,(6), GetParts,(3)
    );
};
using GetParts_ptr = std::shared_ptr<GetParts>;

struct OES_COMMON_EXPORT GetPartsMetadata final : AbstractMessage {
    std::map<std::string, std::string> m_uris;

    GetPartsMetadata();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        GrowingObject,(6), GetPartsMetadata,(8)
    );
};
using GetPartsMetadata_ptr = std::shared_ptr<GetPartsMetadata>;

struct OES_COMMON_EXPORT PutGrowingDataObjectsHeaderResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    PutGrowingDataObjectsHeaderResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Response,
        GrowingObject,(6), PutGrowingDataObjectsHeaderResponse,(17)
    );
};
using PutGrowingDataObjectsHeaderResponse_ptr = std::shared_ptr<PutGrowingDataObjectsHeaderResponse>;

struct OES_COMMON_EXPORT PutPartsResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    PutPartsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Response,
        GrowingObject,(6), PutPartsResponse,(13)
    );
};
using PutPartsResponse_ptr = std::shared_ptr<PutPartsResponse>;

struct OES_COMMON_EXPORT ReplacePartsByRangeResponse final : AbstractMessage {
    // Warning: No state

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        GrowingObject,(6), ReplacePartsByRangeResponse,(18)
    );
};
using ReplacePartsByRangeResponse_ptr = std::shared_ptr<ReplacePartsByRangeResponse>;

struct OES_COMMON_EXPORT GetPartsByRangeResponse final : AbstractMessage {
    std::string m_uri;
    std::string m_format;
    std::vector<Datatypes::Object::ObjectPart> m_parts;

    GetPartsByRangeResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        GrowingObject,(6), GetPartsByRangeResponse,(10)
    );
};
using GetPartsByRangeResponse_ptr = std::shared_ptr<GetPartsByRangeResponse>;

struct OES_COMMON_EXPORT GetPartsResponse final : AbstractMessage {
    std::string m_uri;
    std::string m_format;
    std::map<std::string, Datatypes::Object::ObjectPart> m_parts;

    GetPartsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        GrowingObject,(6), GetPartsResponse,(6)
    );
};
using GetPartsResponse_ptr = std::shared_ptr<GetPartsResponse>;

struct OES_COMMON_EXPORT PutParts final : AbstractMessage {
    std::string m_uri;
    std::string m_format;
    std::map<std::string, Datatypes::Object::ObjectPart> m_parts;

    PutParts();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Put, Request,
        GrowingObject,(6), PutParts,(5)
    );
};
using PutParts_ptr = std::shared_ptr<PutParts>;

struct OES_COMMON_EXPORT GetPartsByRange final : AbstractMessage {
    std::string m_uri;
    std::string m_format;
    Datatypes::Object::IndexInterval m_indexInterval;
    bool m_includeOverlappingIntervals;

    GetPartsByRange();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        GrowingObject,(6), GetPartsByRange,(4)
    );
};
using GetPartsByRange_ptr = std::shared_ptr<GetPartsByRange>;

struct OES_COMMON_EXPORT ReplacePartsByRange final : AbstractMessage {
    std::string m_uri;
    Datatypes::Object::IndexInterval m_deleteInterval;
    bool m_includeOverlappingIntervals;
    std::string m_format;
    std::vector<Datatypes::Object::ObjectPart> m_parts;

    ReplacePartsByRange();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Request,
        GrowingObject,(6), ReplacePartsByRange,(7)
    );
};
using ReplacePartsByRange_ptr = std::shared_ptr<ReplacePartsByRange>;

struct OES_COMMON_EXPORT GetChangeAnnotationsResponse final : AbstractMessage {
    std::map<std::string, Datatypes::Object::ChangeResponseInfo> m_changes;

    GetChangeAnnotationsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_changes; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        GrowingObject,(6), GetChangeAnnotationsResponse,(20)
    );
};
using GetChangeAnnotationsResponse_ptr = std::shared_ptr<GetChangeAnnotationsResponse>;

struct OES_COMMON_EXPORT GetPartsMetadataResponse final : AbstractMessage {
    std::map<std::string, Datatypes::Object::PartsMetadataInfo> m_metadata;

    GetPartsMetadataResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_metadata; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        GrowingObject,(6), GetPartsMetadataResponse,(9)
    );
};
using GetPartsMetadataResponse_ptr = std::shared_ptr<GetPartsMetadataResponse>;

struct OES_COMMON_EXPORT GetGrowingDataObjectsHeaderResponse final : AbstractMessage {
    std::map<std::string, Datatypes::Object::DataObject> m_dataObjects;

    GetGrowingDataObjectsHeaderResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_dataObjects; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        GrowingObject,(6), GetGrowingDataObjectsHeaderResponse,(15)
    );
};
using GetGrowingDataObjectsHeaderResponse_ptr = std::shared_ptr<GetGrowingDataObjectsHeaderResponse>;

struct OES_COMMON_EXPORT PutGrowingDataObjectsHeader final : AbstractMessage {
    std::map<std::string, Datatypes::Object::DataObject> m_dataObjects;

    PutGrowingDataObjectsHeader();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Put, Request,
        GrowingObject,(6), PutGrowingDataObjectsHeader,(16)
    );
};
using PutGrowingDataObjectsHeader_ptr = std::shared_ptr<PutGrowingDataObjectsHeader>;

} //      oes::common::etp12::Protocol::GrowingObject

namespace oes::common::etp12::Protocol::GrowingObjectNotification {

struct OES_COMMON_EXPORT SubscribePartNotificationsResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    SubscribePartNotificationsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Sub, Response,
        GrowingObjectNotification,(7), SubscribePartNotificationsResponse,(10)
    );
};
using SubscribePartNotificationsResponse_ptr = std::shared_ptr<SubscribePartNotificationsResponse>;

struct OES_COMMON_EXPORT PartSubscriptionEnded final : AbstractMessage {
    std::string m_reason;
    std::array<uint8_t, 16> m_requestUuid;

    PartSubscriptionEnded();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Response,
        GrowingObjectNotification,(7), PartSubscriptionEnded,(8)
    );
};
using PartSubscriptionEnded_ptr = std::shared_ptr<PartSubscriptionEnded>;

struct OES_COMMON_EXPORT PartsChanged final : AbstractMessage {
    std::string m_uri;
    std::array<uint8_t, 16> m_requestUuid;
    Datatypes::Object::ObjectChangeKind m_changeKind;
    int64_t m_changeTime;
    std::string m_format;
    std::vector<Datatypes::Object::ObjectPart> m_parts;

    PartsChanged();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        GrowingObjectNotification,(7), PartsChanged,(2)
    );
};
using PartsChanged_ptr = std::shared_ptr<PartsChanged>;

struct OES_COMMON_EXPORT PartsDeleted final : AbstractMessage {
    std::string m_uri;
    std::array<uint8_t, 16> m_requestUuid;
    int64_t m_changeTime;
    std::vector<std::string> m_uids;

    PartsDeleted();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        GrowingObjectNotification,(7), PartsDeleted,(3)
    );
};
using PartsDeleted_ptr = std::shared_ptr<PartsDeleted>;

struct OES_COMMON_EXPORT UnsubscribePartNotification final : AbstractMessage {
    std::array<uint8_t, 16> m_requestUuid;

    UnsubscribePartNotification();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Sub, Request,
        GrowingObjectNotification,(7), UnsubscribePartNotification,(4)
    );
};
using UnsubscribePartNotification_ptr = std::shared_ptr<UnsubscribePartNotification>;

struct OES_COMMON_EXPORT SubscribePartNotifications final : AbstractMessage {
    std::map<std::string, Datatypes::Object::SubscriptionInfo> m_request;

    SubscribePartNotifications();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Sub, Request,
        GrowingObjectNotification,(7), SubscribePartNotifications,(7)
    );
};
using SubscribePartNotifications_ptr = std::shared_ptr<SubscribePartNotifications>;

struct OES_COMMON_EXPORT UnsolicitedPartNotifications final : AbstractMessage {
    std::vector<Datatypes::Object::SubscriptionInfo> m_subscriptions;

    UnsolicitedPartNotifications();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        GrowingObjectNotification,(7), UnsolicitedPartNotifications,(9)
    );
};
using UnsolicitedPartNotifications_ptr = std::shared_ptr<UnsolicitedPartNotifications>;

struct OES_COMMON_EXPORT PartsReplacedByRange final : AbstractMessage {
    std::string m_uri;
    std::array<uint8_t, 16> m_requestUuid;
    int64_t m_changeTime;
    Datatypes::Object::IndexInterval m_deletedInterval;
    bool m_includeOverlappingIntervals;
    std::string m_format;
    std::vector<Datatypes::Object::ObjectPart> m_parts;

    PartsReplacedByRange();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Notification,
        GrowingObjectNotification,(7), PartsReplacedByRange,(6)
    );
};
using PartsReplacedByRange_ptr = std::shared_ptr<PartsReplacedByRange>;

} //      oes::common::etp12::Protocol::GrowingObjectNotification

namespace oes::common::etp12::Protocol::DataArray {

struct OES_COMMON_EXPORT PutDataArraysResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    PutDataArraysResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Response,
        DataArray,(9), PutDataArraysResponse,(10)
    );
};
using PutDataArraysResponse_ptr = std::shared_ptr<PutDataArraysResponse>;

struct OES_COMMON_EXPORT PutDataSubarraysResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    PutDataSubarraysResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Response,
        DataArray,(9), PutDataSubarraysResponse,(11)
    );
};
using PutDataSubarraysResponse_ptr = std::shared_ptr<PutDataSubarraysResponse>;

struct OES_COMMON_EXPORT PutUninitializedDataArraysResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    PutUninitializedDataArraysResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Response,
        DataArray,(9), PutUninitializedDataArraysResponse,(12)
    );
};
using PutUninitializedDataArraysResponse_ptr = std::shared_ptr<PutUninitializedDataArraysResponse>;

struct OES_COMMON_EXPORT GetDataArrayMetadata final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::DataArrayIdentifier> m_dataArrays;

    GetDataArrayMetadata();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        DataArray,(9), GetDataArrayMetadata,(6)
    );
};
using GetDataArrayMetadata_ptr = std::shared_ptr<GetDataArrayMetadata>;

struct OES_COMMON_EXPORT GetDataArrays final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::DataArrayIdentifier> m_dataArrays;

    GetDataArrays();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        DataArray,(9), GetDataArrays,(2)
    );
};
using GetDataArrays_ptr = std::shared_ptr<GetDataArrays>;

struct OES_COMMON_EXPORT GetDataSubarrays final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::GetDataSubarraysType> m_dataSubarrays;

    GetDataSubarrays();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        DataArray,(9), GetDataSubarrays,(3)
    );
};
using GetDataSubarrays_ptr = std::shared_ptr<GetDataSubarrays>;

struct OES_COMMON_EXPORT GetDataArraysResponse final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::DataArray> m_dataArrays;

    GetDataArraysResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_dataArrays; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        DataArray,(9), GetDataArraysResponse,(1)
    );
};
using GetDataArraysResponse_ptr = std::shared_ptr<GetDataArraysResponse>;

struct OES_COMMON_EXPORT GetDataSubarraysResponse final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::DataArray> m_dataSubarrays;

    GetDataSubarraysResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_dataSubarrays; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        DataArray,(9), GetDataSubarraysResponse,(8)
    );
};
using GetDataSubarraysResponse_ptr = std::shared_ptr<GetDataSubarraysResponse>;

struct OES_COMMON_EXPORT PutDataSubarrays final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::PutDataSubarraysType> m_dataSubarrays;

    PutDataSubarrays();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Put, Request,
        DataArray,(9), PutDataSubarrays,(5)
    );
};
using PutDataSubarrays_ptr = std::shared_ptr<PutDataSubarrays>;

struct OES_COMMON_EXPORT GetDataArrayMetadataResponse final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::DataArrayMetadata> m_arrayMetadata;

    GetDataArrayMetadataResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_arrayMetadata; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        DataArray,(9), GetDataArrayMetadataResponse,(7)
    );
};
using GetDataArrayMetadataResponse_ptr = std::shared_ptr<GetDataArrayMetadataResponse>;

struct OES_COMMON_EXPORT PutDataArrays final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::PutDataArraysType> m_dataArrays;

    PutDataArrays();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Put, Request,
        DataArray,(9), PutDataArrays,(4)
    );
};
using PutDataArrays_ptr = std::shared_ptr<PutDataArrays>;

struct OES_COMMON_EXPORT PutUninitializedDataArrays final : AbstractMessage {
    std::map<std::string, Datatypes::DataArrayTypes::PutUninitializedDataArrayType> m_dataArrays;

    PutUninitializedDataArrays();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Put, Request,
        DataArray,(9), PutUninitializedDataArrays,(9)
    );
};
using PutUninitializedDataArrays_ptr = std::shared_ptr<PutUninitializedDataArrays>;

} //      oes::common::etp12::Protocol::DataArray

namespace oes::common::etp12::Protocol::DiscoveryQuery {

struct OES_COMMON_EXPORT FindResources final : AbstractMessage {
    Datatypes::Object::ContextInfo m_context;
    Datatypes::Object::ContextScopeKind m_scope;
    std::optional<int64_t> m_storeLastWriteFilter;
    std::optional<Datatypes::Object::ActiveStatusKind> m_activeStatusFilter;

    FindResources();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        DiscoveryQuery,(13), FindResources,(1)
    );
};
using FindResources_ptr = std::shared_ptr<FindResources>;

struct OES_COMMON_EXPORT FindResourcesResponse final : AbstractMessage {
    std::vector<Datatypes::Object::Resource> m_resources;
    std::string m_serverSortOrder;

    FindResourcesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        DiscoveryQuery,(13), FindResourcesResponse,(2)
    );
};
using FindResourcesResponse_ptr = std::shared_ptr<FindResourcesResponse>;

} //      oes::common::etp12::Protocol::DiscoveryQuery

namespace oes::common::etp12::Protocol::StoreQuery {

struct OES_COMMON_EXPORT Chunk final : AbstractMessage {
    std::array<uint8_t, 16> m_blobId;
    std::vector<uint8_t> m_data;
    bool m_final;

    Chunk();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Chunk,
        StoreQuery,(14), Chunk,(3)
    );
};
using Chunk_ptr = std::shared_ptr<Chunk>;

struct OES_COMMON_EXPORT FindDataObjects final : AbstractMessage {
    Datatypes::Object::ContextInfo m_context;
    Datatypes::Object::ContextScopeKind m_scope;
    std::optional<int64_t> m_storeLastWriteFilter;
    std::optional<Datatypes::Object::ActiveStatusKind> m_activeStatusFilter;
    std::string m_format;

    FindDataObjects();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        StoreQuery,(14), FindDataObjects,(1)
    );
};
using FindDataObjects_ptr = std::shared_ptr<FindDataObjects>;

struct OES_COMMON_EXPORT FindDataObjectsResponse final : AbstractMessage {
    std::vector<Datatypes::Object::DataObject> m_dataObjects;
    std::string m_serverSortOrder;

    FindDataObjectsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        StoreQuery,(14), FindDataObjectsResponse,(2)
    );
};
using FindDataObjectsResponse_ptr = std::shared_ptr<FindDataObjectsResponse>;

} //      oes::common::etp12::Protocol::StoreQuery

namespace oes::common::etp12::Protocol::GrowingObjectQuery {

struct OES_COMMON_EXPORT FindParts final : AbstractMessage {
    std::string m_uri;
    std::string m_format;

    FindParts();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        GrowingObjectQuery,(16), FindParts,(1)
    );
};
using FindParts_ptr = std::shared_ptr<FindParts>;

struct OES_COMMON_EXPORT FindPartsResponse final : AbstractMessage {
    std::string m_uri;
    std::string m_serverSortOrder;
    std::string m_format;
    std::vector<Datatypes::Object::ObjectPart> m_parts;

    FindPartsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        GrowingObjectQuery,(16), FindPartsResponse,(2)
    );
};
using FindPartsResponse_ptr = std::shared_ptr<FindPartsResponse>;

} //      oes::common::etp12::Protocol::GrowingObjectQuery

namespace oes::common::etp12::Protocol::Transaction {

struct OES_COMMON_EXPORT StartTransaction final : AbstractMessage {
    bool m_readOnly;
    std::string m_message;
    std::vector<std::string> m_dataspaceUris;

    StartTransaction();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        Transaction,(18), StartTransaction,(1)
    );
};
using StartTransaction_ptr = std::shared_ptr<StartTransaction>;

struct OES_COMMON_EXPORT CommitTransaction final : AbstractMessage {
    std::array<uint8_t, 16> m_transactionUuid;

    CommitTransaction();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        Transaction,(18), CommitTransaction,(3)
    );
};
using CommitTransaction_ptr = std::shared_ptr<CommitTransaction>;

struct OES_COMMON_EXPORT CommitTransactionResponse final : AbstractMessage {
    std::array<uint8_t, 16> m_transactionUuid;
    bool m_successful;
    std::string m_failureReason;

    CommitTransactionResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        Transaction,(18), CommitTransactionResponse,(5)
    );
};
using CommitTransactionResponse_ptr = std::shared_ptr<CommitTransactionResponse>;

struct OES_COMMON_EXPORT RollbackTransaction final : AbstractMessage {
    std::array<uint8_t, 16> m_transactionUuid;

    RollbackTransaction();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        Transaction,(18), RollbackTransaction,(4)
    );
};
using RollbackTransaction_ptr = std::shared_ptr<RollbackTransaction>;

struct OES_COMMON_EXPORT RollbackTransactionResponse final : AbstractMessage {
    std::array<uint8_t, 16> m_transactionUuid;
    bool m_successful;
    std::string m_failureReason;

    RollbackTransactionResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        Transaction,(18), RollbackTransactionResponse,(6)
    );
};
using RollbackTransactionResponse_ptr = std::shared_ptr<RollbackTransactionResponse>;

struct OES_COMMON_EXPORT StartTransactionResponse final : AbstractMessage {
    std::array<uint8_t, 16> m_transactionUuid;
    bool m_successful;
    std::string m_failureReason;

    StartTransactionResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        Transaction,(18), StartTransactionResponse,(2)
    );
};
using StartTransactionResponse_ptr = std::shared_ptr<StartTransactionResponse>;

} //      oes::common::etp12::Protocol::Transaction

namespace oes::common::etp12::Protocol::ChannelSubscribe {

struct OES_COMMON_EXPORT GetChannelMetadata final : AbstractMessage {
    std::map<std::string, std::string> m_uris;

    GetChannelMetadata();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        ChannelSubscribe,(21), GetChannelMetadata,(1)
    );
};
using GetChannelMetadata_ptr = std::shared_ptr<GetChannelMetadata>;

struct OES_COMMON_EXPORT SubscribeChannelsResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    SubscribeChannelsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Sub, Response,
        ChannelSubscribe,(21), SubscribeChannelsResponse,(12)
    );
};
using SubscribeChannelsResponse_ptr = std::shared_ptr<SubscribeChannelsResponse>;

struct OES_COMMON_EXPORT SubscriptionsStopped final : AbstractMessage {
    std::string m_reason;
    std::map<std::string, int64_t> m_channelIds;

    SubscriptionsStopped();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, None, Response,
        ChannelSubscribe,(21), SubscriptionsStopped,(8)
    );
};
using SubscriptionsStopped_ptr = std::shared_ptr<SubscriptionsStopped>;

struct OES_COMMON_EXPORT UnsubscribeChannels final : AbstractMessage {
    std::map<std::string, int64_t> m_channelIds;

    UnsubscribeChannels();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Sub, Request,
        ChannelSubscribe,(21), UnsubscribeChannels,(7)
    );
};
using UnsubscribeChannels_ptr = std::shared_ptr<UnsubscribeChannels>;

struct OES_COMMON_EXPORT CancelGetRanges final : AbstractMessage {
    std::array<uint8_t, 16> m_requestUuid;

    CancelGetRanges();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelSubscribe,(21), CancelGetRanges,(11)
    );
};
using CancelGetRanges_ptr = std::shared_ptr<CancelGetRanges>;

struct OES_COMMON_EXPORT GetChangeAnnotations final : AbstractMessage {
    std::map<std::string, Datatypes::ChannelData::ChannelChangeRequestInfo> m_channels;
    bool m_latestOnly;

    GetChangeAnnotations();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        ChannelSubscribe,(21), GetChangeAnnotations,(14)
    );
};
using GetChangeAnnotations_ptr = std::shared_ptr<GetChangeAnnotations>;

struct OES_COMMON_EXPORT ChannelsTruncated final : AbstractMessage {
    std::vector<Datatypes::ChannelData::TruncateInfo> m_channels;
    int64_t m_changeTime;

    ChannelsTruncated();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, None, Notification,
        ChannelSubscribe,(21), ChannelsTruncated,(13)
    );
};
using ChannelsTruncated_ptr = std::shared_ptr<ChannelsTruncated>;

struct OES_COMMON_EXPORT SubscribeChannels final : AbstractMessage {
    std::map<std::string, Datatypes::ChannelData::ChannelSubscribeInfo> m_channels;

    SubscribeChannels();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Sub, Request,
        ChannelSubscribe,(21), SubscribeChannels,(3)
    );
};
using SubscribeChannels_ptr = std::shared_ptr<SubscribeChannels>;

struct OES_COMMON_EXPORT GetRanges final : AbstractMessage {
    std::array<uint8_t, 16> m_requestUuid;
    std::vector<Datatypes::ChannelData::ChannelRangeInfo> m_channelRanges;

    GetRanges();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        ChannelSubscribe,(21), GetRanges,(9)
    );
};
using GetRanges_ptr = std::shared_ptr<GetRanges>;

struct OES_COMMON_EXPORT GetChangeAnnotationsResponse final : AbstractMessage {
    std::map<std::string, Datatypes::Object::ChangeResponseInfo> m_changes;

    GetChangeAnnotationsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_changes; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        ChannelSubscribe,(21), GetChangeAnnotationsResponse,(15)
    );
};
using GetChangeAnnotationsResponse_ptr = std::shared_ptr<GetChangeAnnotationsResponse>;

struct OES_COMMON_EXPORT GetChannelMetadataResponse final : AbstractMessage {
    std::map<std::string, Datatypes::ChannelData::ChannelMetadataRecord> m_metadata;

    GetChannelMetadataResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_metadata; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        ChannelSubscribe,(21), GetChannelMetadataResponse,(2)
    );
};
using GetChannelMetadataResponse_ptr = std::shared_ptr<GetChannelMetadataResponse>;

struct OES_COMMON_EXPORT ChannelData final : AbstractMessage {
    std::vector<Datatypes::ChannelData::DataItem> m_data;

    ChannelData();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Notification,
        ChannelSubscribe,(21), ChannelData,(4)
    );
};
using ChannelData_ptr = std::shared_ptr<ChannelData>;

struct OES_COMMON_EXPORT GetRangesResponse final : AbstractMessage {
    std::vector<Datatypes::ChannelData::DataItem> m_data;

    GetRangesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_data; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        ChannelSubscribe,(21), GetRangesResponse,(10)
    );
};
using GetRangesResponse_ptr = std::shared_ptr<GetRangesResponse>;

struct OES_COMMON_EXPORT RangeReplaced final : AbstractMessage {
    int64_t m_changeTime;
    std::vector<int64_t> m_channelIds;
    Datatypes::Object::IndexInterval m_changedInterval;
    std::vector<Datatypes::ChannelData::DataItem> m_data;

    RangeReplaced();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, None, Notification,
        ChannelSubscribe,(21), RangeReplaced,(6)
    );
};
using RangeReplaced_ptr = std::shared_ptr<RangeReplaced>;

} //      oes::common::etp12::Protocol::ChannelSubscribe

namespace oes::common::etp12::Protocol::ChannelDataLoad {

struct OES_COMMON_EXPORT ChannelsClosed final : AbstractMessage {
    std::string m_reason;
    std::map<std::string, int64_t> m_id;

    ChannelsClosed();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, None, Response,
        ChannelDataLoad,(22), ChannelsClosed,(7)
    );
};
using ChannelsClosed_ptr = std::shared_ptr<ChannelsClosed>;

struct OES_COMMON_EXPORT CloseChannels final : AbstractMessage {
    std::map<std::string, int64_t> m_id;

    CloseChannels();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelDataLoad,(22), CloseChannels,(3)
    );
};
using CloseChannels_ptr = std::shared_ptr<CloseChannels>;

struct OES_COMMON_EXPORT OpenChannels final : AbstractMessage {
    std::map<std::string, std::string> m_uris;

    OpenChannels();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelDataLoad,(22), OpenChannels,(1)
    );
};
using OpenChannels_ptr = std::shared_ptr<OpenChannels>;

struct OES_COMMON_EXPORT ReplaceRangeResponse final : AbstractMessage {
    int64_t m_channelChangeTime;

    ReplaceRangeResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        ChannelDataLoad,(22), ReplaceRangeResponse,(8)
    );
};
using ReplaceRangeResponse_ptr = std::shared_ptr<ReplaceRangeResponse>;

struct OES_COMMON_EXPORT TruncateChannelsResponse final : AbstractMessage {
    std::map<std::string, int64_t> m_channelsTruncatedTime;

    TruncateChannelsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_channelsTruncatedTime; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        ChannelDataLoad,(22), TruncateChannelsResponse,(10)
    );
};
using TruncateChannelsResponse_ptr = std::shared_ptr<TruncateChannelsResponse>;

struct OES_COMMON_EXPORT TruncateChannels final : AbstractMessage {
    std::map<std::string, Datatypes::ChannelData::TruncateInfo> m_channels;

    TruncateChannels();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelDataLoad,(22), TruncateChannels,(9)
    );
};
using TruncateChannels_ptr = std::shared_ptr<TruncateChannels>;

struct OES_COMMON_EXPORT ChannelData final : AbstractMessage {
    std::vector<Datatypes::ChannelData::DataItem> m_data;

    ChannelData();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        ChannelDataLoad,(22), ChannelData,(4)
    );
};
using ChannelData_ptr = std::shared_ptr<ChannelData>;

struct OES_COMMON_EXPORT OpenChannelsResponse final : AbstractMessage {
    std::map<std::string, Datatypes::ChannelData::OpenChannelInfo> m_channels;

    OpenChannelsResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_channels; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        ChannelDataLoad,(22), OpenChannelsResponse,(2)
    );
};
using OpenChannelsResponse_ptr = std::shared_ptr<OpenChannelsResponse>;

struct OES_COMMON_EXPORT ReplaceRange final : AbstractMessage {
    Datatypes::Object::IndexInterval m_changedInterval;
    std::vector<int64_t> m_channelIds;
    std::vector<Datatypes::ChannelData::DataItem> m_data;

    ReplaceRange();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Request,
        ChannelDataLoad,(22), ReplaceRange,(6)
    );
};
using ReplaceRange_ptr = std::shared_ptr<ReplaceRange>;

} //      oes::common::etp12::Protocol::ChannelDataLoad

namespace oes::common::etp12::Protocol::Dataspace {

struct OES_COMMON_EXPORT DeleteDataspaces final : AbstractMessage {
    std::map<std::string, std::string> m_uris;

    DeleteDataspaces();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Del, Request,
        Dataspace,(24), DeleteDataspaces,(4)
    );
};
using DeleteDataspaces_ptr = std::shared_ptr<DeleteDataspaces>;

struct OES_COMMON_EXPORT DeleteDataspacesResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    DeleteDataspacesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Del, Response,
        Dataspace,(24), DeleteDataspacesResponse,(5)
    );
};
using DeleteDataspacesResponse_ptr = std::shared_ptr<DeleteDataspacesResponse>;

struct OES_COMMON_EXPORT GetDataspaces final : AbstractMessage {
    std::optional<int64_t> m_storeLastWriteFilter;

    GetDataspaces();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        Dataspace,(24), GetDataspaces,(1)
    );
};
using GetDataspaces_ptr = std::shared_ptr<GetDataspaces>;

struct OES_COMMON_EXPORT PutDataspacesResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    PutDataspacesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Put, Response,
        Dataspace,(24), PutDataspacesResponse,(6)
    );
};
using PutDataspacesResponse_ptr = std::shared_ptr<PutDataspacesResponse>;

struct OES_COMMON_EXPORT GetDataspacesResponse final : AbstractMessage {
    std::vector<Datatypes::Object::Dataspace> m_dataspaces;

    GetDataspacesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_dataspaces; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        Dataspace,(24), GetDataspacesResponse,(2)
    );
};
using GetDataspacesResponse_ptr = std::shared_ptr<GetDataspacesResponse>;

struct OES_COMMON_EXPORT PutDataspaces final : AbstractMessage {
    std::map<std::string, Datatypes::Object::Dataspace> m_dataspaces;

    PutDataspaces();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Put, Request,
        Dataspace,(24), PutDataspaces,(3)
    );
};
using PutDataspaces_ptr = std::shared_ptr<PutDataspaces>;

} //      oes::common::etp12::Protocol::Dataspace

namespace oes::common::etp12::Protocol::SupportedTypes {

struct OES_COMMON_EXPORT GetSupportedTypes final : AbstractMessage {
    std::string m_uri;
    Datatypes::Object::ContextScopeKind m_scope;
    bool m_returnEmptyTypes;
    bool m_countObjects;

    GetSupportedTypes();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        SupportedTypes,(25), GetSupportedTypes,(1)
    );
};
using GetSupportedTypes_ptr = std::shared_ptr<GetSupportedTypes>;

struct OES_COMMON_EXPORT GetSupportedTypesResponse final : AbstractMessage {
    std::vector<Datatypes::Object::SupportedType> m_supportedTypes;

    GetSupportedTypesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_supportedTypes; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        SupportedTypes,(25), GetSupportedTypesResponse,(2)
    );
};
using GetSupportedTypesResponse_ptr = std::shared_ptr<GetSupportedTypesResponse>;

} //      oes::common::etp12::Protocol::SupportedTypes

namespace oes::common::etp12::Protocol::CoreOSDU {

struct OES_COMMON_EXPORT ResumeSession final : AbstractMessage {
    std::string m_applicationName;
    std::string m_applicationVersion;
    std::array<uint8_t, 16> m_clientInstanceId;
    int64_t m_currentDateTime;
    std::array<uint8_t, 16> m_sessionId;

    ResumeSession();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        CoreOSDU,(2400), ResumeSession,(1)
    );
};
using ResumeSession_ptr = std::shared_ptr<ResumeSession>;

struct OES_COMMON_EXPORT ResumeSessionResponse final : AbstractMessage {
    std::string m_applicationName;
    std::string m_applicationVersion;
    std::array<uint8_t, 16> m_serverInstanceId;
    int64_t m_currentDateTime;
    std::array<uint8_t, 16> m_sessionId;
    int64_t m_earliestRetainedChangeTime;

    ResumeSessionResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        CoreOSDU,(2400), ResumeSessionResponse,(2)
    );
};
using ResumeSessionResponse_ptr = std::shared_ptr<ResumeSessionResponse>;

} //      oes::common::etp12::Protocol::CoreOSDU

namespace oes::common::etp12::Protocol::StoreOSDU {

struct OES_COMMON_EXPORT CopyDataObjectsByValue final : AbstractMessage {
    std::string m_uri;
    int32_t m_sourcesDepth;
    std::vector<std::string> m_dataObjectTypes;

    CopyDataObjectsByValue();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        StoreOSDU,(2404), CopyDataObjectsByValue,(1)
    );
};
using CopyDataObjectsByValue_ptr = std::shared_ptr<CopyDataObjectsByValue>;

struct OES_COMMON_EXPORT CopyDataObjectsByValueResponse final : AbstractMessage {
    std::vector<std::string> m_copiedDataObjects;

    CopyDataObjectsByValueResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_copiedDataObjects; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        StoreOSDU,(2404), CopyDataObjectsByValueResponse,(2)
    );
};
using CopyDataObjectsByValueResponse_ptr = std::shared_ptr<CopyDataObjectsByValueResponse>;

} //      oes::common::etp12::Protocol::StoreOSDU

namespace oes::common::etp12::Protocol::DataspaceOSDU {

struct OES_COMMON_EXPORT CopyDataspacesContent final : AbstractMessage {
    std::map<std::string, std::string> m_dataspaces;
    std::string m_targetDataspace;

    CopyDataspacesContent();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        DataspaceOSDU,(2424), CopyDataspacesContent,(3)
    );
};
using CopyDataspacesContent_ptr = std::shared_ptr<CopyDataspacesContent>;

struct OES_COMMON_EXPORT CopyDataspacesContentResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    CopyDataspacesContentResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        DataspaceOSDU,(2424), CopyDataspacesContentResponse,(4)
    );
};
using CopyDataspacesContentResponse_ptr = std::shared_ptr<CopyDataspacesContentResponse>;

struct OES_COMMON_EXPORT CopyToDataspace final : AbstractMessage {
    std::map<std::string, std::string> m_uris;
    std::string m_dataspaceUri;

    CopyToDataspace();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        DataspaceOSDU,(2424), CopyToDataspace,(7)
    );
};
using CopyToDataspace_ptr = std::shared_ptr<CopyToDataspace>;

struct OES_COMMON_EXPORT CopyToDataspaceResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    CopyToDataspaceResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_success; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        DataspaceOSDU,(2424), CopyToDataspaceResponse,(8)
    );
};
using CopyToDataspaceResponse_ptr = std::shared_ptr<CopyToDataspaceResponse>;

struct OES_COMMON_EXPORT GetDataspaceInfo final : AbstractMessage {
    std::map<std::string, std::string> m_uris;

    GetDataspaceInfo();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Get, Request,
        DataspaceOSDU,(2424), GetDataspaceInfo,(1)
    );
};
using GetDataspaceInfo_ptr = std::shared_ptr<GetDataspaceInfo>;

struct OES_COMMON_EXPORT LockDataspaces final : AbstractMessage {
    std::map<std::string, std::string> m_uris;
    bool m_lock;

    LockDataspaces();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        DataspaceOSDU,(2424), LockDataspaces,(5)
    );
};
using LockDataspaces_ptr = std::shared_ptr<LockDataspaces>;

struct OES_COMMON_EXPORT LockDataspacesResponse final : AbstractMessage {
    std::map<std::string, std::string> m_success;

    LockDataspacesResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Response,
        DataspaceOSDU,(2424), LockDataspacesResponse,(6)
    );
};
using LockDataspacesResponse_ptr = std::shared_ptr<LockDataspacesResponse>;

struct OES_COMMON_EXPORT CopyDataspaceFromRemote final : AbstractMessage {
    std::string m_action;
    std::string m_dataspacePath;
    Datatypes::Object::RemoteServer m_sourceServer;
    std::map<std::string, std::string> m_extraData;

    CopyDataspaceFromRemote();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;

    OES_COMMON_ETP12_MESSAGE(
        SinglePart, Misc, Request,
        DataspaceOSDU,(2424), CopyDataspaceFromRemote,(9)
    );
};
using CopyDataspaceFromRemote_ptr = std::shared_ptr<CopyDataspaceFromRemote>;

struct OES_COMMON_EXPORT CopyDataspaceFromRemoteResponse final : AbstractMessage {
    std::vector<Datatypes::Object::TableData> m_tableData;

    CopyDataspaceFromRemoteResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_tableData; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Misc, Response,
        DataspaceOSDU,(2424), CopyDataspaceFromRemoteResponse,(10)
    );
};
using CopyDataspaceFromRemoteResponse_ptr = std::shared_ptr<CopyDataspaceFromRemoteResponse>;

struct OES_COMMON_EXPORT GetDataspaceInfoResponse final : AbstractMessage {
    std::map<std::string, Datatypes::Object::Dataspace> m_dataspaces;

    GetDataspaceInfoResponse();

    static avro::ValidSchema schema();
    void encode(avro::Encoder&) const override;
    void decode(avro::Decoder&) override;
    auto& range() { return m_dataspaces; }

    OES_COMMON_ETP12_MESSAGE(
        MultiPart, Get, Response,
        DataspaceOSDU,(2424), GetDataspaceInfoResponse,(2)
    );
};
using GetDataspaceInfoResponse_ptr = std::shared_ptr<GetDataspaceInfoResponse>;

} //      oes::common::etp12::Protocol::DataspaceOSDU

namespace oes::common::etp12 {

constexpr const MsgInfo* MsgInfo::lookup(int proto, int type) {
    switch (proto) {

    case int(Datatypes::Protocol::eCore): // 0
        switch (type) {
        case Protocol::Core::RequestSession::info.type_.type_: return // 1
            &Protocol::Core::RequestSession::info;
        case Protocol::Core::OpenSession::info.type_.type_: return // 2
            &Protocol::Core::OpenSession::info;
        case Protocol::Core::CloseSession::info.type_.type_: return // 5
            &Protocol::Core::CloseSession::info;
        case Protocol::Core::Authorize::info.type_.type_: return // 6
            &Protocol::Core::Authorize::info;
        case Protocol::Core::AuthorizeResponse::info.type_.type_: return // 7
            &Protocol::Core::AuthorizeResponse::info;
        case Protocol::Core::Ping::info.type_.type_: return // 8
            &Protocol::Core::Ping::info;
        case Protocol::Core::Pong::info.type_.type_: return // 9
            &Protocol::Core::Pong::info;
        case Protocol::Core::ProtocolException::info.type_.type_: return // 1000
            &Protocol::Core::ProtocolException::info;
        case Protocol::Core::Acknowledge::info.type_.type_: return // 1001
            &Protocol::Core::Acknowledge::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelStreaming): // 1
        switch (type) {
        case Protocol::ChannelStreaming::ChannelMetadata::info.type_.type_: return // 1
            &Protocol::ChannelStreaming::ChannelMetadata::info;
        case Protocol::ChannelStreaming::ChannelData::info.type_.type_: return // 2
            &Protocol::ChannelStreaming::ChannelData::info;
        case Protocol::ChannelStreaming::StartStreaming::info.type_.type_: return // 3
            &Protocol::ChannelStreaming::StartStreaming::info;
        case Protocol::ChannelStreaming::StopStreaming::info.type_.type_: return // 4
            &Protocol::ChannelStreaming::StopStreaming::info;
        case Protocol::ChannelStreaming::TruncateChannels::info.type_.type_: return // 5
            &Protocol::ChannelStreaming::TruncateChannels::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelDataFrame): // 2
        switch (type) {
        case Protocol::ChannelDataFrame::GetFrameMetadata::info.type_.type_: return // 1
            &Protocol::ChannelDataFrame::GetFrameMetadata::info;
        case Protocol::ChannelDataFrame::GetFrameMetadataResponse::info.type_.type_: return // 2
            &Protocol::ChannelDataFrame::GetFrameMetadataResponse::info;
        case Protocol::ChannelDataFrame::GetFrame::info.type_.type_: return // 3
            &Protocol::ChannelDataFrame::GetFrame::info;
        case Protocol::ChannelDataFrame::GetFrameResponseHeader::info.type_.type_: return // 4
            &Protocol::ChannelDataFrame::GetFrameResponseHeader::info;
        case Protocol::ChannelDataFrame::CancelGetFrame::info.type_.type_: return // 5
            &Protocol::ChannelDataFrame::CancelGetFrame::info;
        case Protocol::ChannelDataFrame::GetFrameResponseRows::info.type_.type_: return // 6
            &Protocol::ChannelDataFrame::GetFrameResponseRows::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDiscovery): // 3
        switch (type) {
        case Protocol::Discovery::GetResources::info.type_.type_: return // 1
            &Protocol::Discovery::GetResources::info;
        case Protocol::Discovery::GetResourcesResponse::info.type_.type_: return // 4
            &Protocol::Discovery::GetResourcesResponse::info;
        case Protocol::Discovery::GetDeletedResources::info.type_.type_: return // 5
            &Protocol::Discovery::GetDeletedResources::info;
        case Protocol::Discovery::GetDeletedResourcesResponse::info.type_.type_: return // 6
            &Protocol::Discovery::GetDeletedResourcesResponse::info;
        case Protocol::Discovery::GetResourcesEdgesResponse::info.type_.type_: return // 7
            &Protocol::Discovery::GetResourcesEdgesResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStore): // 4
        switch (type) {
        case Protocol::Store::GetDataObjects::info.type_.type_: return // 1
            &Protocol::Store::GetDataObjects::info;
        case Protocol::Store::PutDataObjects::info.type_.type_: return // 2
            &Protocol::Store::PutDataObjects::info;
        case Protocol::Store::DeleteDataObjects::info.type_.type_: return // 3
            &Protocol::Store::DeleteDataObjects::info;
        case Protocol::Store::GetDataObjectsResponse::info.type_.type_: return // 4
            &Protocol::Store::GetDataObjectsResponse::info;
        case Protocol::Store::Chunk::info.type_.type_: return // 8
            &Protocol::Store::Chunk::info;
        case Protocol::Store::PutDataObjectsResponse::info.type_.type_: return // 9
            &Protocol::Store::PutDataObjectsResponse::info;
        case Protocol::Store::DeleteDataObjectsResponse::info.type_.type_: return // 10
            &Protocol::Store::DeleteDataObjectsResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreNotification): // 5
        switch (type) {
        case Protocol::StoreNotification::ObjectChanged::info.type_.type_: return // 2
            &Protocol::StoreNotification::ObjectChanged::info;
        case Protocol::StoreNotification::ObjectDeleted::info.type_.type_: return // 3
            &Protocol::StoreNotification::ObjectDeleted::info;
        case Protocol::StoreNotification::UnsubscribeNotifications::info.type_.type_: return // 4
            &Protocol::StoreNotification::UnsubscribeNotifications::info;
        case Protocol::StoreNotification::ObjectAccessRevoked::info.type_.type_: return // 5
            &Protocol::StoreNotification::ObjectAccessRevoked::info;
        case Protocol::StoreNotification::SubscribeNotifications::info.type_.type_: return // 6
            &Protocol::StoreNotification::SubscribeNotifications::info;
        case Protocol::StoreNotification::SubscriptionEnded::info.type_.type_: return // 7
            &Protocol::StoreNotification::SubscriptionEnded::info;
        case Protocol::StoreNotification::UnsolicitedStoreNotifications::info.type_.type_: return // 8
            &Protocol::StoreNotification::UnsolicitedStoreNotifications::info;
        case Protocol::StoreNotification::Chunk::info.type_.type_: return // 9
            &Protocol::StoreNotification::Chunk::info;
        case Protocol::StoreNotification::SubscribeNotificationsResponse::info.type_.type_: return // 10
            &Protocol::StoreNotification::SubscribeNotificationsResponse::info;
        case Protocol::StoreNotification::ObjectActiveStatusChanged::info.type_.type_: return // 11
            &Protocol::StoreNotification::ObjectActiveStatusChanged::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObject): // 6
        switch (type) {
        case Protocol::GrowingObject::DeleteParts::info.type_.type_: return // 1
            &Protocol::GrowingObject::DeleteParts::info;
        case Protocol::GrowingObject::GetParts::info.type_.type_: return // 3
            &Protocol::GrowingObject::GetParts::info;
        case Protocol::GrowingObject::GetPartsByRange::info.type_.type_: return // 4
            &Protocol::GrowingObject::GetPartsByRange::info;
        case Protocol::GrowingObject::PutParts::info.type_.type_: return // 5
            &Protocol::GrowingObject::PutParts::info;
        case Protocol::GrowingObject::GetPartsResponse::info.type_.type_: return // 6
            &Protocol::GrowingObject::GetPartsResponse::info;
        case Protocol::GrowingObject::ReplacePartsByRange::info.type_.type_: return // 7
            &Protocol::GrowingObject::ReplacePartsByRange::info;
        case Protocol::GrowingObject::GetPartsMetadata::info.type_.type_: return // 8
            &Protocol::GrowingObject::GetPartsMetadata::info;
        case Protocol::GrowingObject::GetPartsMetadataResponse::info.type_.type_: return // 9
            &Protocol::GrowingObject::GetPartsMetadataResponse::info;
        case Protocol::GrowingObject::GetPartsByRangeResponse::info.type_.type_: return // 10
            &Protocol::GrowingObject::GetPartsByRangeResponse::info;
        case Protocol::GrowingObject::DeletePartsResponse::info.type_.type_: return // 11
            &Protocol::GrowingObject::DeletePartsResponse::info;
        case Protocol::GrowingObject::PutPartsResponse::info.type_.type_: return // 13
            &Protocol::GrowingObject::PutPartsResponse::info;
        case Protocol::GrowingObject::GetGrowingDataObjectsHeader::info.type_.type_: return // 14
            &Protocol::GrowingObject::GetGrowingDataObjectsHeader::info;
        case Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse::info.type_.type_: return // 15
            &Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse::info;
        case Protocol::GrowingObject::PutGrowingDataObjectsHeader::info.type_.type_: return // 16
            &Protocol::GrowingObject::PutGrowingDataObjectsHeader::info;
        case Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse::info.type_.type_: return // 17
            &Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse::info;
        case Protocol::GrowingObject::ReplacePartsByRangeResponse::info.type_.type_: return // 18
            &Protocol::GrowingObject::ReplacePartsByRangeResponse::info;
        case Protocol::GrowingObject::GetChangeAnnotations::info.type_.type_: return // 19
            &Protocol::GrowingObject::GetChangeAnnotations::info;
        case Protocol::GrowingObject::GetChangeAnnotationsResponse::info.type_.type_: return // 20
            &Protocol::GrowingObject::GetChangeAnnotationsResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObjectNotification): // 7
        switch (type) {
        case Protocol::GrowingObjectNotification::PartsChanged::info.type_.type_: return // 2
            &Protocol::GrowingObjectNotification::PartsChanged::info;
        case Protocol::GrowingObjectNotification::PartsDeleted::info.type_.type_: return // 3
            &Protocol::GrowingObjectNotification::PartsDeleted::info;
        case Protocol::GrowingObjectNotification::UnsubscribePartNotification::info.type_.type_: return // 4
            &Protocol::GrowingObjectNotification::UnsubscribePartNotification::info;
        case Protocol::GrowingObjectNotification::PartsReplacedByRange::info.type_.type_: return // 6
            &Protocol::GrowingObjectNotification::PartsReplacedByRange::info;
        case Protocol::GrowingObjectNotification::SubscribePartNotifications::info.type_.type_: return // 7
            &Protocol::GrowingObjectNotification::SubscribePartNotifications::info;
        case Protocol::GrowingObjectNotification::PartSubscriptionEnded::info.type_.type_: return // 8
            &Protocol::GrowingObjectNotification::PartSubscriptionEnded::info;
        case Protocol::GrowingObjectNotification::UnsolicitedPartNotifications::info.type_.type_: return // 9
            &Protocol::GrowingObjectNotification::UnsolicitedPartNotifications::info;
        case Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse::info.type_.type_: return // 10
            &Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataArray): // 9
        switch (type) {
        case Protocol::DataArray::GetDataArraysResponse::info.type_.type_: return // 1
            &Protocol::DataArray::GetDataArraysResponse::info;
        case Protocol::DataArray::GetDataArrays::info.type_.type_: return // 2
            &Protocol::DataArray::GetDataArrays::info;
        case Protocol::DataArray::GetDataSubarrays::info.type_.type_: return // 3
            &Protocol::DataArray::GetDataSubarrays::info;
        case Protocol::DataArray::PutDataArrays::info.type_.type_: return // 4
            &Protocol::DataArray::PutDataArrays::info;
        case Protocol::DataArray::PutDataSubarrays::info.type_.type_: return // 5
            &Protocol::DataArray::PutDataSubarrays::info;
        case Protocol::DataArray::GetDataArrayMetadata::info.type_.type_: return // 6
            &Protocol::DataArray::GetDataArrayMetadata::info;
        case Protocol::DataArray::GetDataArrayMetadataResponse::info.type_.type_: return // 7
            &Protocol::DataArray::GetDataArrayMetadataResponse::info;
        case Protocol::DataArray::GetDataSubarraysResponse::info.type_.type_: return // 8
            &Protocol::DataArray::GetDataSubarraysResponse::info;
        case Protocol::DataArray::PutUninitializedDataArrays::info.type_.type_: return // 9
            &Protocol::DataArray::PutUninitializedDataArrays::info;
        case Protocol::DataArray::PutDataArraysResponse::info.type_.type_: return // 10
            &Protocol::DataArray::PutDataArraysResponse::info;
        case Protocol::DataArray::PutDataSubarraysResponse::info.type_.type_: return // 11
            &Protocol::DataArray::PutDataSubarraysResponse::info;
        case Protocol::DataArray::PutUninitializedDataArraysResponse::info.type_.type_: return // 12
            &Protocol::DataArray::PutUninitializedDataArraysResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDiscoveryQuery): // 13
        switch (type) {
        case Protocol::DiscoveryQuery::FindResources::info.type_.type_: return // 1
            &Protocol::DiscoveryQuery::FindResources::info;
        case Protocol::DiscoveryQuery::FindResourcesResponse::info.type_.type_: return // 2
            &Protocol::DiscoveryQuery::FindResourcesResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreQuery): // 14
        switch (type) {
        case Protocol::StoreQuery::FindDataObjects::info.type_.type_: return // 1
            &Protocol::StoreQuery::FindDataObjects::info;
        case Protocol::StoreQuery::FindDataObjectsResponse::info.type_.type_: return // 2
            &Protocol::StoreQuery::FindDataObjectsResponse::info;
        case Protocol::StoreQuery::Chunk::info.type_.type_: return // 3
            &Protocol::StoreQuery::Chunk::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eGrowingObjectQuery): // 16
        switch (type) {
        case Protocol::GrowingObjectQuery::FindParts::info.type_.type_: return // 1
            &Protocol::GrowingObjectQuery::FindParts::info;
        case Protocol::GrowingObjectQuery::FindPartsResponse::info.type_.type_: return // 2
            &Protocol::GrowingObjectQuery::FindPartsResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eTransaction): // 18
        switch (type) {
        case Protocol::Transaction::StartTransaction::info.type_.type_: return // 1
            &Protocol::Transaction::StartTransaction::info;
        case Protocol::Transaction::StartTransactionResponse::info.type_.type_: return // 2
            &Protocol::Transaction::StartTransactionResponse::info;
        case Protocol::Transaction::CommitTransaction::info.type_.type_: return // 3
            &Protocol::Transaction::CommitTransaction::info;
        case Protocol::Transaction::RollbackTransaction::info.type_.type_: return // 4
            &Protocol::Transaction::RollbackTransaction::info;
        case Protocol::Transaction::CommitTransactionResponse::info.type_.type_: return // 5
            &Protocol::Transaction::CommitTransactionResponse::info;
        case Protocol::Transaction::RollbackTransactionResponse::info.type_.type_: return // 6
            &Protocol::Transaction::RollbackTransactionResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelSubscribe): // 21
        switch (type) {
        case Protocol::ChannelSubscribe::GetChannelMetadata::info.type_.type_: return // 1
            &Protocol::ChannelSubscribe::GetChannelMetadata::info;
        case Protocol::ChannelSubscribe::GetChannelMetadataResponse::info.type_.type_: return // 2
            &Protocol::ChannelSubscribe::GetChannelMetadataResponse::info;
        case Protocol::ChannelSubscribe::SubscribeChannels::info.type_.type_: return // 3
            &Protocol::ChannelSubscribe::SubscribeChannels::info;
        case Protocol::ChannelSubscribe::ChannelData::info.type_.type_: return // 4
            &Protocol::ChannelSubscribe::ChannelData::info;
        case Protocol::ChannelSubscribe::RangeReplaced::info.type_.type_: return // 6
            &Protocol::ChannelSubscribe::RangeReplaced::info;
        case Protocol::ChannelSubscribe::UnsubscribeChannels::info.type_.type_: return // 7
            &Protocol::ChannelSubscribe::UnsubscribeChannels::info;
        case Protocol::ChannelSubscribe::SubscriptionsStopped::info.type_.type_: return // 8
            &Protocol::ChannelSubscribe::SubscriptionsStopped::info;
        case Protocol::ChannelSubscribe::GetRanges::info.type_.type_: return // 9
            &Protocol::ChannelSubscribe::GetRanges::info;
        case Protocol::ChannelSubscribe::GetRangesResponse::info.type_.type_: return // 10
            &Protocol::ChannelSubscribe::GetRangesResponse::info;
        case Protocol::ChannelSubscribe::CancelGetRanges::info.type_.type_: return // 11
            &Protocol::ChannelSubscribe::CancelGetRanges::info;
        case Protocol::ChannelSubscribe::SubscribeChannelsResponse::info.type_.type_: return // 12
            &Protocol::ChannelSubscribe::SubscribeChannelsResponse::info;
        case Protocol::ChannelSubscribe::ChannelsTruncated::info.type_.type_: return // 13
            &Protocol::ChannelSubscribe::ChannelsTruncated::info;
        case Protocol::ChannelSubscribe::GetChangeAnnotations::info.type_.type_: return // 14
            &Protocol::ChannelSubscribe::GetChangeAnnotations::info;
        case Protocol::ChannelSubscribe::GetChangeAnnotationsResponse::info.type_.type_: return // 15
            &Protocol::ChannelSubscribe::GetChangeAnnotationsResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eChannelDataLoad): // 22
        switch (type) {
        case Protocol::ChannelDataLoad::OpenChannels::info.type_.type_: return // 1
            &Protocol::ChannelDataLoad::OpenChannels::info;
        case Protocol::ChannelDataLoad::OpenChannelsResponse::info.type_.type_: return // 2
            &Protocol::ChannelDataLoad::OpenChannelsResponse::info;
        case Protocol::ChannelDataLoad::CloseChannels::info.type_.type_: return // 3
            &Protocol::ChannelDataLoad::CloseChannels::info;
        case Protocol::ChannelDataLoad::ChannelData::info.type_.type_: return // 4
            &Protocol::ChannelDataLoad::ChannelData::info;
        case Protocol::ChannelDataLoad::ReplaceRange::info.type_.type_: return // 6
            &Protocol::ChannelDataLoad::ReplaceRange::info;
        case Protocol::ChannelDataLoad::ChannelsClosed::info.type_.type_: return // 7
            &Protocol::ChannelDataLoad::ChannelsClosed::info;
        case Protocol::ChannelDataLoad::ReplaceRangeResponse::info.type_.type_: return // 8
            &Protocol::ChannelDataLoad::ReplaceRangeResponse::info;
        case Protocol::ChannelDataLoad::TruncateChannels::info.type_.type_: return // 9
            &Protocol::ChannelDataLoad::TruncateChannels::info;
        case Protocol::ChannelDataLoad::TruncateChannelsResponse::info.type_.type_: return // 10
            &Protocol::ChannelDataLoad::TruncateChannelsResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataspace): // 24
        switch (type) {
        case Protocol::Dataspace::GetDataspaces::info.type_.type_: return // 1
            &Protocol::Dataspace::GetDataspaces::info;
        case Protocol::Dataspace::GetDataspacesResponse::info.type_.type_: return // 2
            &Protocol::Dataspace::GetDataspacesResponse::info;
        case Protocol::Dataspace::PutDataspaces::info.type_.type_: return // 3
            &Protocol::Dataspace::PutDataspaces::info;
        case Protocol::Dataspace::DeleteDataspaces::info.type_.type_: return // 4
            &Protocol::Dataspace::DeleteDataspaces::info;
        case Protocol::Dataspace::DeleteDataspacesResponse::info.type_.type_: return // 5
            &Protocol::Dataspace::DeleteDataspacesResponse::info;
        case Protocol::Dataspace::PutDataspacesResponse::info.type_.type_: return // 6
            &Protocol::Dataspace::PutDataspacesResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eSupportedTypes): // 25
        switch (type) {
        case Protocol::SupportedTypes::GetSupportedTypes::info.type_.type_: return // 1
            &Protocol::SupportedTypes::GetSupportedTypes::info;
        case Protocol::SupportedTypes::GetSupportedTypesResponse::info.type_.type_: return // 2
            &Protocol::SupportedTypes::GetSupportedTypesResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eCoreOSDU): // 2400
        switch (type) {
        case Protocol::CoreOSDU::ResumeSession::info.type_.type_: return // 1
            &Protocol::CoreOSDU::ResumeSession::info;
        case Protocol::CoreOSDU::ResumeSessionResponse::info.type_.type_: return // 2
            &Protocol::CoreOSDU::ResumeSessionResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eStoreOSDU): // 2404
        switch (type) {
        case Protocol::StoreOSDU::CopyDataObjectsByValue::info.type_.type_: return // 1
            &Protocol::StoreOSDU::CopyDataObjectsByValue::info;
        case Protocol::StoreOSDU::CopyDataObjectsByValueResponse::info.type_.type_: return // 2
            &Protocol::StoreOSDU::CopyDataObjectsByValueResponse::info;
        default:
            break;
        }
        break;

    case int(Datatypes::Protocol::eDataspaceOSDU): // 2424
        switch (type) {
        case Protocol::DataspaceOSDU::GetDataspaceInfo::info.type_.type_: return // 1
            &Protocol::DataspaceOSDU::GetDataspaceInfo::info;
        case Protocol::DataspaceOSDU::GetDataspaceInfoResponse::info.type_.type_: return // 2
            &Protocol::DataspaceOSDU::GetDataspaceInfoResponse::info;
        case Protocol::DataspaceOSDU::CopyDataspacesContent::info.type_.type_: return // 3
            &Protocol::DataspaceOSDU::CopyDataspacesContent::info;
        case Protocol::DataspaceOSDU::CopyDataspacesContentResponse::info.type_.type_: return // 4
            &Protocol::DataspaceOSDU::CopyDataspacesContentResponse::info;
        case Protocol::DataspaceOSDU::LockDataspaces::info.type_.type_: return // 5
            &Protocol::DataspaceOSDU::LockDataspaces::info;
        case Protocol::DataspaceOSDU::LockDataspacesResponse::info.type_.type_: return // 6
            &Protocol::DataspaceOSDU::LockDataspacesResponse::info;
        case Protocol::DataspaceOSDU::CopyToDataspace::info.type_.type_: return // 7
            &Protocol::DataspaceOSDU::CopyToDataspace::info;
        case Protocol::DataspaceOSDU::CopyToDataspaceResponse::info.type_.type_: return // 8
            &Protocol::DataspaceOSDU::CopyToDataspaceResponse::info;
        case Protocol::DataspaceOSDU::CopyDataspaceFromRemote::info.type_.type_: return // 9
            &Protocol::DataspaceOSDU::CopyDataspaceFromRemote::info;
        case Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse::info.type_.type_: return // 10
            &Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse::info;
        default:
            break;
        }
        break;

    default:
        break;
    }
    return nullptr; // not found
}

} //      oes::common::etp12
