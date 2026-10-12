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

namespace avro {

template <>
struct codec_traits<oes::common::etp12::Datatypes::AnyArrayType> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::AnyArrayType v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::AnyArrayType& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::AnyArrayType>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::AnyArrayType::eArrayOfBoolean) ||
            v > int(oes::common::etp12::Datatypes::AnyArrayType::eBytes)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::AnyArrayType"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::AnyLogicalArrayType> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::AnyLogicalArrayType v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::AnyLogicalArrayType& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::AnyLogicalArrayType>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::AnyLogicalArrayType::eArrayOfBoolean) ||
            v > int(oes::common::etp12::Datatypes::AnyLogicalArrayType::eArrayOfCustom)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::AnyLogicalArrayType"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataObjectCapabilityKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::DataObjectCapabilityKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::DataObjectCapabilityKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::DataObjectCapabilityKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::DataObjectCapabilityKind::eActiveTimeoutPeriod) ||
            v > int(oes::common::etp12::Datatypes::DataObjectCapabilityKind::eMaxSecondaryIndexCount)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::DataObjectCapabilityKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::EndpointCapabilityKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::EndpointCapabilityKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::EndpointCapabilityKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::EndpointCapabilityKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::EndpointCapabilityKind::eActiveTimeoutPeriod) ||
            v > int(oes::common::etp12::Datatypes::EndpointCapabilityKind::eSupportsMessageHeaderExtensions)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::EndpointCapabilityKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Protocol> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::Protocol v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::Protocol& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::Protocol>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::Protocol::eCore) ||
            v > int(oes::common::etp12::Datatypes::Protocol::eDataspaceOSDU)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::Protocol"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ProtocolCapabilityKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::ProtocolCapabilityKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::ProtocolCapabilityKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::ProtocolCapabilityKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::ProtocolCapabilityKind::eFrameChangeDetectionPeriod) ||
            v > int(oes::common::etp12::Datatypes::ProtocolCapabilityKind::eTransactionTimeoutPeriod)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::ProtocolCapabilityKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::ChannelDataKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::ChannelData::ChannelDataKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::ChannelData::ChannelDataKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::ChannelData::ChannelDataKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::ChannelData::ChannelDataKind::eDateTime) ||
            v > int(oes::common::etp12::Datatypes::ChannelData::ChannelDataKind::eTypeBytes)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::ChannelData::ChannelDataKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::ChannelIndexKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::ChannelData::ChannelIndexKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::ChannelData::ChannelIndexKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::ChannelData::ChannelIndexKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::ChannelData::ChannelIndexKind::eDateTime) ||
            v > int(oes::common::etp12::Datatypes::ChannelData::ChannelIndexKind::eScalar)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::ChannelData::ChannelIndexKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::IndexDirection> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::ChannelData::IndexDirection v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::ChannelData::IndexDirection& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::ChannelData::IndexDirection>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::ChannelData::IndexDirection::eIncreasing) ||
            v > int(oes::common::etp12::Datatypes::ChannelData::IndexDirection::eUnordered)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::ChannelData::IndexDirection"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::PassDirection> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::ChannelData::PassDirection v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::ChannelData::PassDirection& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::ChannelData::PassDirection>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::ChannelData::PassDirection::eUp) ||
            v > int(oes::common::etp12::Datatypes::ChannelData::PassDirection::eDown)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::ChannelData::PassDirection"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ActiveStatusKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::Object::ActiveStatusKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::Object::ActiveStatusKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::Object::ActiveStatusKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::Object::ActiveStatusKind::eActive) ||
            v > int(oes::common::etp12::Datatypes::Object::ActiveStatusKind::eInactive)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::Object::ActiveStatusKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ContextScopeKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::Object::ContextScopeKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::Object::ContextScopeKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::Object::ContextScopeKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::Object::ContextScopeKind::eSelf) ||
            v > int(oes::common::etp12::Datatypes::Object::ContextScopeKind::eTargetsOrSelf)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::Object::ContextScopeKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ObjectChangeKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::Object::ObjectChangeKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::Object::ObjectChangeKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::Object::ObjectChangeKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::Object::ObjectChangeKind::eInsert) ||
            v > int(oes::common::etp12::Datatypes::Object::ObjectChangeKind::eUnjoinedSubscription)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::Object::ObjectChangeKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::RelationshipKind> {
    static void encode(Encoder& e, oes::common::etp12::Datatypes::Object::RelationshipKind v) {
        ensure_in_range(int(v), "encoded");
        e.encodeEnum(int(v));
    }
    static void decode(Decoder& d, oes::common::etp12::Datatypes::Object::RelationshipKind& v) {
        const int index = static_cast<int>(d.decodeEnum());
        ensure_in_range(index, "decoded");
        v = static_cast<oes::common::etp12::Datatypes::Object::RelationshipKind>(index);
    }
    static void ensure_in_range(int v, const char* what) {
        if (v < int(oes::common::etp12::Datatypes::Object::RelationshipKind::ePrimary) ||
            v > int(oes::common::etp12::Datatypes::Object::RelationshipKind::eBoth)
        ) {
            std::ostringstream oss;
            oss << "Error: Enum value " << v << " is out of bound for "
                "oes::common::etp12::Datatypes::Object::RelationshipKind"
                " and cannot be " << what;
            throw avro::Exception(oss.str());
        }
    }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfBoolean> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfBoolean;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfBytes> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfBytes;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfDouble> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfDouble;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfFloat> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfFloat;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfInt> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfInt;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfLong> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfLong;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfNullableBoolean> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfNullableBoolean;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfNullableInt> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfNullableInt;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfNullableLong> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfNullableLong;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ArrayOfString> {
    using Record_t = oes::common::etp12::Datatypes::ArrayOfString;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Contact> {
    using Record_t = oes::common::etp12::Datatypes::Contact;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ErrorInfo> {
    using Record_t = oes::common::etp12::Datatypes::ErrorInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::MessageHeader> {
    using Record_t = oes::common::etp12::Datatypes::MessageHeader;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Version> {
    using Record_t = oes::common::etp12::Datatypes::Version;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::ChannelChangeRequestInfo> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::ChannelChangeRequestInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier> {
    using Record_t = oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ObjectPart> {
    using Record_t = oes::common::etp12::Datatypes::Object::ObjectPart;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::PutResponse> {
    using Record_t = oes::common::etp12::Datatypes::Object::PutResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::RemoteServer> {
    using Record_t = oes::common::etp12::Datatypes::Object::RemoteServer;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::TableData> {
    using Record_t = oes::common::etp12::Datatypes::Object::TableData;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::AnyArray> {
    using Record_t = oes::common::etp12::Datatypes::AnyArray;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::AttributeMetadataRecord> {
    using Record_t = oes::common::etp12::Datatypes::AttributeMetadataRecord;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::PassIndexedDepth> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::PassIndexedDepth;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataArrayTypes::GetDataSubarraysType> {
    using Record_t = oes::common::etp12::Datatypes::DataArrayTypes::GetDataSubarraysType;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ContextInfo> {
    using Record_t = oes::common::etp12::Datatypes::Object::ContextInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::SupportedType> {
    using Record_t = oes::common::etp12::Datatypes::Object::SupportedType;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::AnySubarray> {
    using Record_t = oes::common::etp12::Datatypes::AnySubarray;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::IndexValue> {
    using Record_t = oes::common::etp12::Datatypes::IndexValue;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataArrayTypes::DataArray> {
    using Record_t = oes::common::etp12::Datatypes::DataArrayTypes::DataArray;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataArrayTypes::PutDataSubarraysType> {
    using Record_t = oes::common::etp12::Datatypes::DataArrayTypes::PutDataSubarraysType;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::SubscriptionInfo> {
    using Record_t = oes::common::etp12::Datatypes::Object::SubscriptionInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::AnySparseArray> {
    using Record_t = oes::common::etp12::Datatypes::AnySparseArray;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::ChannelSubscribeInfo> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::ChannelSubscribeInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::TruncateInfo> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::TruncateInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::IndexInterval> {
    using Record_t = oes::common::etp12::Datatypes::Object::IndexInterval;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataValue> {
    using Record_t = oes::common::etp12::Datatypes::DataValue;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::ChannelRangeInfo> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::ChannelRangeInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::IndexMetadataRecord> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::IndexMetadataRecord;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ChangeAnnotation> {
    using Record_t = oes::common::etp12::Datatypes::Object::ChangeAnnotation;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataAttribute> {
    using Record_t = oes::common::etp12::Datatypes::DataAttribute;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::MessageHeaderExtension> {
    using Record_t = oes::common::etp12::Datatypes::MessageHeaderExtension;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::SupportedDataObject> {
    using Record_t = oes::common::etp12::Datatypes::SupportedDataObject;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::SupportedProtocol> {
    using Record_t = oes::common::etp12::Datatypes::SupportedProtocol;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::ChannelMetadataRecord> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::ChannelMetadataRecord;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::FrameChannelMetadataRecord> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::FrameChannelMetadataRecord;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataArrayTypes::DataArrayMetadata> {
    using Record_t = oes::common::etp12::Datatypes::DataArrayTypes::DataArrayMetadata;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataArrayTypes::PutDataArraysType> {
    using Record_t = oes::common::etp12::Datatypes::DataArrayTypes::PutDataArraysType;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ChangeResponseInfo> {
    using Record_t = oes::common::etp12::Datatypes::Object::ChangeResponseInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::Dataspace> {
    using Record_t = oes::common::etp12::Datatypes::Object::Dataspace;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::DeletedResource> {
    using Record_t = oes::common::etp12::Datatypes::Object::DeletedResource;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::Edge> {
    using Record_t = oes::common::etp12::Datatypes::Object::Edge;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::PartsMetadataInfo> {
    using Record_t = oes::common::etp12::Datatypes::Object::PartsMetadataInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::Resource> {
    using Record_t = oes::common::etp12::Datatypes::Object::Resource;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ServerCapabilities> {
    using Record_t = oes::common::etp12::Datatypes::ServerCapabilities;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::DataItem> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::DataItem;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::FramePoint> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::FramePoint;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::OpenChannelInfo> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::OpenChannelInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::DataArrayTypes::PutUninitializedDataArrayType> {
    using Record_t = oes::common::etp12::Datatypes::DataArrayTypes::PutUninitializedDataArrayType;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::DataObject> {
    using Record_t = oes::common::etp12::Datatypes::Object::DataObject;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::ChannelData::FrameRow> {
    using Record_t = oes::common::etp12::Datatypes::ChannelData::FrameRow;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Datatypes::Object::ObjectChange> {
    using Record_t = oes::common::etp12::Datatypes::Object::ObjectChange;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::Acknowledge> {
    using Record_t = oes::common::etp12::Protocol::Core::Acknowledge;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::Authorize> {
    using Record_t = oes::common::etp12::Protocol::Core::Authorize;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::AuthorizeResponse> {
    using Record_t = oes::common::etp12::Protocol::Core::AuthorizeResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::CloseSession> {
    using Record_t = oes::common::etp12::Protocol::Core::CloseSession;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::Ping> {
    using Record_t = oes::common::etp12::Protocol::Core::Ping;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::Pong> {
    using Record_t = oes::common::etp12::Protocol::Core::Pong;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::ProtocolException> {
    using Record_t = oes::common::etp12::Protocol::Core::ProtocolException;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::OpenSession> {
    using Record_t = oes::common::etp12::Protocol::Core::OpenSession;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Core::RequestSession> {
    using Record_t = oes::common::etp12::Protocol::Core::RequestSession;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelStreaming::StartStreaming> {
    using Record_t = oes::common::etp12::Protocol::ChannelStreaming::StartStreaming;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelStreaming::StopStreaming> {
    using Record_t = oes::common::etp12::Protocol::ChannelStreaming::StopStreaming;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelStreaming::TruncateChannels> {
    using Record_t = oes::common::etp12::Protocol::ChannelStreaming::TruncateChannels;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelStreaming::ChannelMetadata> {
    using Record_t = oes::common::etp12::Protocol::ChannelStreaming::ChannelMetadata;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelStreaming::ChannelData> {
    using Record_t = oes::common::etp12::Protocol::ChannelStreaming::ChannelData;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataFrame::GetFrameMetadata> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataFrame::GetFrameMetadata;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataFrame::CancelGetFrame> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataFrame::CancelGetFrame;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataFrame::GetFrame> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataFrame::GetFrame;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataFrame::GetFrameResponseHeader> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataFrame::GetFrameResponseHeader;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataFrame::GetFrameMetadataResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataFrame::GetFrameMetadataResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataFrame::GetFrameResponseRows> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataFrame::GetFrameResponseRows;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Discovery::GetDeletedResources> {
    using Record_t = oes::common::etp12::Protocol::Discovery::GetDeletedResources;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Discovery::GetResources> {
    using Record_t = oes::common::etp12::Protocol::Discovery::GetResources;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Discovery::GetDeletedResourcesResponse> {
    using Record_t = oes::common::etp12::Protocol::Discovery::GetDeletedResourcesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Discovery::GetResourcesEdgesResponse> {
    using Record_t = oes::common::etp12::Protocol::Discovery::GetResourcesEdgesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Discovery::GetResourcesResponse> {
    using Record_t = oes::common::etp12::Protocol::Discovery::GetResourcesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Store::DeleteDataObjects> {
    using Record_t = oes::common::etp12::Protocol::Store::DeleteDataObjects;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Store::GetDataObjects> {
    using Record_t = oes::common::etp12::Protocol::Store::GetDataObjects;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Store::Chunk> {
    using Record_t = oes::common::etp12::Protocol::Store::Chunk;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Store::DeleteDataObjectsResponse> {
    using Record_t = oes::common::etp12::Protocol::Store::DeleteDataObjectsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Store::PutDataObjectsResponse> {
    using Record_t = oes::common::etp12::Protocol::Store::PutDataObjectsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Store::GetDataObjectsResponse> {
    using Record_t = oes::common::etp12::Protocol::Store::GetDataObjectsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Store::PutDataObjects> {
    using Record_t = oes::common::etp12::Protocol::Store::PutDataObjects;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::SubscribeNotificationsResponse> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::SubscribeNotificationsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::Chunk> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::Chunk;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::ObjectAccessRevoked> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::ObjectAccessRevoked;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::ObjectDeleted> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::ObjectDeleted;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::SubscriptionEnded> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::SubscriptionEnded;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::UnsubscribeNotifications> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::UnsubscribeNotifications;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::SubscribeNotifications> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::SubscribeNotifications;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::UnsolicitedStoreNotifications> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::UnsolicitedStoreNotifications;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::ObjectActiveStatusChanged> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::ObjectActiveStatusChanged;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreNotification::ObjectChanged> {
    using Record_t = oes::common::etp12::Protocol::StoreNotification::ObjectChanged;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::DeleteParts> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::DeleteParts;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::DeletePartsResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::DeletePartsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetChangeAnnotations> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetChangeAnnotations;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetGrowingDataObjectsHeader> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetGrowingDataObjectsHeader;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetParts> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetParts;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetPartsMetadata> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetPartsMetadata;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::PutGrowingDataObjectsHeaderResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::PutPartsResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::PutPartsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::ReplacePartsByRangeResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::ReplacePartsByRangeResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetPartsByRangeResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetPartsByRangeResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetPartsResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetPartsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::PutParts> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::PutParts;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetPartsByRange> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetPartsByRange;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::ReplacePartsByRange> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::ReplacePartsByRange;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetChangeAnnotationsResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetChangeAnnotationsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetPartsMetadataResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetPartsMetadataResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::GetGrowingDataObjectsHeaderResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObject::PutGrowingDataObjectsHeader> {
    using Record_t = oes::common::etp12::Protocol::GrowingObject::PutGrowingDataObjectsHeader;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::SubscribePartNotificationsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::PartSubscriptionEnded> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::PartSubscriptionEnded;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::PartsChanged> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::PartsChanged;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::PartsDeleted> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::PartsDeleted;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::UnsubscribePartNotification> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::UnsubscribePartNotification;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::SubscribePartNotifications> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::SubscribePartNotifications;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::UnsolicitedPartNotifications> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::UnsolicitedPartNotifications;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectNotification::PartsReplacedByRange> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectNotification::PartsReplacedByRange;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::PutDataArraysResponse> {
    using Record_t = oes::common::etp12::Protocol::DataArray::PutDataArraysResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::PutDataSubarraysResponse> {
    using Record_t = oes::common::etp12::Protocol::DataArray::PutDataSubarraysResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::PutUninitializedDataArraysResponse> {
    using Record_t = oes::common::etp12::Protocol::DataArray::PutUninitializedDataArraysResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::GetDataArrayMetadata> {
    using Record_t = oes::common::etp12::Protocol::DataArray::GetDataArrayMetadata;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::GetDataArrays> {
    using Record_t = oes::common::etp12::Protocol::DataArray::GetDataArrays;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::GetDataSubarrays> {
    using Record_t = oes::common::etp12::Protocol::DataArray::GetDataSubarrays;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::GetDataArraysResponse> {
    using Record_t = oes::common::etp12::Protocol::DataArray::GetDataArraysResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::GetDataSubarraysResponse> {
    using Record_t = oes::common::etp12::Protocol::DataArray::GetDataSubarraysResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::PutDataSubarrays> {
    using Record_t = oes::common::etp12::Protocol::DataArray::PutDataSubarrays;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::GetDataArrayMetadataResponse> {
    using Record_t = oes::common::etp12::Protocol::DataArray::GetDataArrayMetadataResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::PutDataArrays> {
    using Record_t = oes::common::etp12::Protocol::DataArray::PutDataArrays;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataArray::PutUninitializedDataArrays> {
    using Record_t = oes::common::etp12::Protocol::DataArray::PutUninitializedDataArrays;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DiscoveryQuery::FindResources> {
    using Record_t = oes::common::etp12::Protocol::DiscoveryQuery::FindResources;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DiscoveryQuery::FindResourcesResponse> {
    using Record_t = oes::common::etp12::Protocol::DiscoveryQuery::FindResourcesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreQuery::Chunk> {
    using Record_t = oes::common::etp12::Protocol::StoreQuery::Chunk;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreQuery::FindDataObjects> {
    using Record_t = oes::common::etp12::Protocol::StoreQuery::FindDataObjects;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreQuery::FindDataObjectsResponse> {
    using Record_t = oes::common::etp12::Protocol::StoreQuery::FindDataObjectsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectQuery::FindParts> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectQuery::FindParts;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::GrowingObjectQuery::FindPartsResponse> {
    using Record_t = oes::common::etp12::Protocol::GrowingObjectQuery::FindPartsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Transaction::StartTransaction> {
    using Record_t = oes::common::etp12::Protocol::Transaction::StartTransaction;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Transaction::CommitTransaction> {
    using Record_t = oes::common::etp12::Protocol::Transaction::CommitTransaction;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Transaction::CommitTransactionResponse> {
    using Record_t = oes::common::etp12::Protocol::Transaction::CommitTransactionResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Transaction::RollbackTransaction> {
    using Record_t = oes::common::etp12::Protocol::Transaction::RollbackTransaction;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Transaction::RollbackTransactionResponse> {
    using Record_t = oes::common::etp12::Protocol::Transaction::RollbackTransactionResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Transaction::StartTransactionResponse> {
    using Record_t = oes::common::etp12::Protocol::Transaction::StartTransactionResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::GetChannelMetadata> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::GetChannelMetadata;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::SubscribeChannelsResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::SubscribeChannelsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::SubscriptionsStopped> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::SubscriptionsStopped;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::UnsubscribeChannels> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::UnsubscribeChannels;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::CancelGetRanges> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::CancelGetRanges;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::GetChangeAnnotations> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::GetChangeAnnotations;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::ChannelsTruncated> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::ChannelsTruncated;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::SubscribeChannels> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::SubscribeChannels;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::GetRanges> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::GetRanges;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::GetChangeAnnotationsResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::GetChangeAnnotationsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::GetChannelMetadataResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::GetChannelMetadataResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::ChannelData> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::ChannelData;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::GetRangesResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::GetRangesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelSubscribe::RangeReplaced> {
    using Record_t = oes::common::etp12::Protocol::ChannelSubscribe::RangeReplaced;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::ChannelsClosed> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::ChannelsClosed;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::CloseChannels> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::CloseChannels;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::OpenChannels> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::OpenChannels;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::ReplaceRangeResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::ReplaceRangeResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::TruncateChannelsResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::TruncateChannelsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::TruncateChannels> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::TruncateChannels;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::ChannelData> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::ChannelData;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::OpenChannelsResponse> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::OpenChannelsResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::ChannelDataLoad::ReplaceRange> {
    using Record_t = oes::common::etp12::Protocol::ChannelDataLoad::ReplaceRange;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Dataspace::DeleteDataspaces> {
    using Record_t = oes::common::etp12::Protocol::Dataspace::DeleteDataspaces;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Dataspace::DeleteDataspacesResponse> {
    using Record_t = oes::common::etp12::Protocol::Dataspace::DeleteDataspacesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Dataspace::GetDataspaces> {
    using Record_t = oes::common::etp12::Protocol::Dataspace::GetDataspaces;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Dataspace::PutDataspacesResponse> {
    using Record_t = oes::common::etp12::Protocol::Dataspace::PutDataspacesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Dataspace::GetDataspacesResponse> {
    using Record_t = oes::common::etp12::Protocol::Dataspace::GetDataspacesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::Dataspace::PutDataspaces> {
    using Record_t = oes::common::etp12::Protocol::Dataspace::PutDataspaces;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::SupportedTypes::GetSupportedTypes> {
    using Record_t = oes::common::etp12::Protocol::SupportedTypes::GetSupportedTypes;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::SupportedTypes::GetSupportedTypesResponse> {
    using Record_t = oes::common::etp12::Protocol::SupportedTypes::GetSupportedTypesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::CoreOSDU::ResumeSession> {
    using Record_t = oes::common::etp12::Protocol::CoreOSDU::ResumeSession;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::CoreOSDU::ResumeSessionResponse> {
    using Record_t = oes::common::etp12::Protocol::CoreOSDU::ResumeSessionResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreOSDU::CopyDataObjectsByValue> {
    using Record_t = oes::common::etp12::Protocol::StoreOSDU::CopyDataObjectsByValue;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::StoreOSDU::CopyDataObjectsByValueResponse> {
    using Record_t = oes::common::etp12::Protocol::StoreOSDU::CopyDataObjectsByValueResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspacesContent> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspacesContent;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspacesContentResponse> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspacesContentResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::CopyToDataspace> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::CopyToDataspace;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::CopyToDataspaceResponse> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::CopyToDataspaceResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::GetDataspaceInfo> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::GetDataspaceInfo;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::LockDataspaces> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::LockDataspaces;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::LockDataspacesResponse> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::LockDataspacesResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspaceFromRemote> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspaceFromRemote;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::CopyDataspaceFromRemoteResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

template <>
struct codec_traits<oes::common::etp12::Protocol::DataspaceOSDU::GetDataspaceInfoResponse> {
    using Record_t = oes::common::etp12::Protocol::DataspaceOSDU::GetDataspaceInfoResponse;
    static void encode(Encoder& e, const Record_t& r) { r.encode(e); }
    static void decode(Decoder& d,       Record_t& r) { r.decode(d); }
};

} //      avro
