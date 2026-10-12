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

#include <oes/common/etp12/Messages.h>
#include <oes/common/etp12/Protocols.h>
#include <oes/common/EtpSchemas.h>
#include <oes/common/AvroUtils.hpp>

#include <oes/core/utils/Guid.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <avro/Decoder.hh>
#include <avro/Encoder.hh>
#include <avro/Specific.hh>
#include <avro/Stream.hh>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <memory>
#include <stdexcept>

namespace oes {
namespace common {
namespace etp12 {
namespace Datatypes {

// TODO: Generate op== for all ETP 1.2 types

bool operator==(const MessageHeader& lhs, const MessageHeader& rhs) {
    const auto lhs_hdr = msgHeaderAsString(lhs);
    const auto rhs_hdr = msgHeaderAsString(rhs);
    return lhs_hdr == rhs_hdr;
}
std::ostream& operator<<(std::ostream& os, const MessageHeader& hdr) {
    return os << msgHeaderAsString(hdr);
}

}}}} // oes::comon::etp12::Datatypes

namespace {

using namespace oes::common;
using namespace oes::common::etp12;
using namespace oes::common::etp12::Datatypes;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Protocol::Core;

SupportedDataObject sdo(std::string qtype) {
    SupportedDataObject type;
    type.m_qualifiedType = std::move(qtype);
    type.m_dataObjectCapabilities.clear(); // none
    return type;
}

template <typename T>
MessageHeader headerFor(
    int64_t message_id,
    int64_t correlation_id = 0,
    int32_t flags = MsgFlags::eFINALPART
) {
    MessageHeader hdr;
    hdr.m_protocol      = T::info.type_.proto_;
    hdr.m_messageType   = T::info.type_.type_;
    hdr.m_correlationId = correlation_id;
    hdr.m_messageId     = message_id;
    hdr.m_messageFlags  = flags;
    return hdr;
}

BOOST_AUTO_TEST_SUITE(Etp12MessagesTests)

BOOST_AUTO_TEST_CASE(test_msgHeaderAsString) {
    int64_t req_id = 0;
    int64_t res_id = -1;

    // Below we simulate a real session
    {
        MessageHeader req = headerFor<Core::RequestSession>(req_id += 2);
        MessageHeader res = headerFor<Core::OpenSession>(res_id += 2, req.m_messageId);
        BOOST_CHECK_EQUAL("Msg(2)_PMsg(0) Core(0).RequestSession(1) F", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(1)_PMsg(2) Core(0).OpenSession(2) F", msgHeaderAsString(res));
    }
    {
        MessageHeader req = headerFor<Dataspace::GetDataspaces>(req_id += 2);
        MessageHeader res = headerFor<Dataspace::GetDataspacesResponse>(res_id += 2, req.m_messageId);
        BOOST_CHECK_EQUAL("Msg(4)_PMsg(0) Dataspace(24).GetDataspaces(1) F", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(3)_PMsg(4) Dataspace(24).GetDataspacesResponse(2) F", msgHeaderAsString(res));
    }
    {
        MessageHeader req = headerFor<Transaction::StartTransaction>(req_id += 2);
        MessageHeader res = headerFor<Transaction::StartTransactionResponse>(res_id += 2, req.m_messageId);
        BOOST_CHECK_EQUAL("Msg(6)_PMsg(0) Transaction(18).StartTransaction(1) F", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(5)_PMsg(6) Transaction(18).StartTransactionResponse(2) F", msgHeaderAsString(res));
    }
    {
        MessageHeader req = headerFor<Store::PutDataObjects>(req_id += 2, 0, MsgFlags::eHEADER_EXTENSION);
        MessageHeader ck1 = headerFor<Store::Chunk>(req_id += 2, req.m_messageId, 0);
        MessageHeader ck2 = headerFor<Store::Chunk>(req_id += 2, req.m_messageId, MsgFlags::eFINALPART);
        MessageHeader res = headerFor<Store::PutDataObjectsResponse>(res_id += 2, req.m_messageId);
        BOOST_CHECK_EQUAL("Msg(8)_PMsg(0) Store(4).PutDataObjects(2) X", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(10)_PMsg(8) Store(4).Chunk(8) -", msgHeaderAsString(ck1));
        BOOST_CHECK_EQUAL("Msg(12)_PMsg(8) Store(4).Chunk(8) F", msgHeaderAsString(ck2));
        BOOST_CHECK_EQUAL("Msg(7)_PMsg(8) Store(4).PutDataObjectsResponse(9) F", msgHeaderAsString(res));
    }
    {
        MessageHeader req = headerFor<DataArray::PutDataArrays>(
            req_id += 2, 0, MsgFlags::eFINALPART | MsgFlags::eCOMPRESSED_BODY
        );
        MessageHeader res = headerFor<DataArray::PutDataArraysResponse>(res_id += 2, req.m_messageId);
        BOOST_CHECK_EQUAL("Msg(14)_PMsg(0) DataArray(9).PutDataArrays(4) FZ", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(9)_PMsg(14) DataArray(9).PutDataArraysResponse(10) F", msgHeaderAsString(res));
    }
    {
        MessageHeader req = headerFor<DataArray::PutDataArrays>(req_id += 2);
        MessageHeader err = headerFor<Core::ProtocolException>(res_id += 2, req.m_messageId);
        err.m_protocol = req.m_protocol; // errors are cross-protocol
        BOOST_CHECK_EQUAL("Msg(16)_PMsg(0) DataArray(9).PutDataArrays(4) F", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(11)_PMsg(16) DataArray(9).ProtocolException(1000) F", msgHeaderAsString(err));
    } //                           ^^^^^^^^^^^^ Not Core(0)
    {
        MessageHeader req = headerFor<Transaction::CommitTransaction>(req_id += 2);
        MessageHeader res = headerFor<Transaction::CommitTransactionResponse>(res_id += 2, req.m_messageId);
        BOOST_CHECK_EQUAL("Msg(18)_PMsg(0) Transaction(18).CommitTransaction(3) F", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(13)_PMsg(18) Transaction(18).CommitTransactionResponse(5) F", msgHeaderAsString(res));
    }
    {
        MessageHeader req = headerFor<Core::CloseSession>(
            req_id += 2, 0, MsgFlags::eFINALPART | MsgFlags::eACKNOWLEDGE_RECEIPT | (1<<15)
        );
        MessageHeader ack = headerFor<Core::Acknowledge>(res_id += 2, req.m_messageId);
        BOOST_CHECK_EQUAL("Msg(20)_PMsg(0) Core(0).CloseSession(5) FA?", msgHeaderAsString(req));
        BOOST_CHECK_EQUAL("Msg(15)_PMsg(20) Core(0).Acknowledge(1001) F", msgHeaderAsString(ack));
    }

    {
        MessageHeader bad_proto = headerFor<Core::CloseSession>(
            req_id += 2, 0, MsgFlags::eFINALPART | MsgFlags::eACKNOWLEDGE_RECEIPT | (1<<15)
        );
        bad_proto.m_protocol = 53;
        BOOST_CHECK_EQUAL("Msg(22)_PMsg(0) " "???" "(53)." "???" "(5) FA?", msgHeaderAsString(bad_proto));
    } //                            ^^^^^ avoid ??( trigraph warning on GCC
    {
        MessageHeader bad_mtype = headerFor<Core::CloseSession>(
            req_id += 2, 44,
            MsgFlags::eFINALPART | MsgFlags::eCOMPRESSED_BODY | // all valid flags
            MsgFlags::eHEADER_EXTENSION| MsgFlags::eACKNOWLEDGE_RECEIPT |
            (1<<15) | (1<<27) // a couple invalid flags
        );
        bad_mtype.m_messageType = 42; //              v Single ?, despite 2 invalid flags
        BOOST_CHECK_EQUAL("Msg(24)_PMsg(44) " "???" "(0)." "???" "(42) FZAX?", msgHeaderAsString(bad_mtype));
    } //                           ^^^ No proto name, even though we could show it I guess
}

struct SizeT {
    size_t value_ = 0;

    void encode(avro::Encoder& e) const {
        avro::encode(e, int64_t(value_));
    }
    void decode(avro::Decoder& d) {
        int64_t value;
        avro::decode(d, value);
        value_ = value;
    }
};

// "test" that demonstrates when an encoded size_t changes "byte size"
// Useful to know, if wanting to change the encoded size a-posteriori
// with an "overwrite", since size differences means shifting all bytes.
// For our purposes of MultiPart message splitting, [0, 64) and [64, 8192)
// are the ranges of most interest to us, and perhaps the next one,
// as having more than 1M parts would lead to excessive message sizes.
BOOST_AUTO_TEST_CASE(test_size_encoded_bytes_thresholds) {
    const struct {
        size_t value; size_t expected_bytes;
    } test_cases[] {
        {     0, 1 },
        {     1, 1 }, {     2, 1 }, {     3, 1 },
        {     3, 1 }, {     4, 1 }, {     5, 1 },
        {     7, 1 }, {     8, 1 }, {     9, 1 },
        {    15, 1 }, {    16, 1 }, {    17, 1 },
        {    31, 1 }, {    32, 1 }, {    33, 1 },
        {    63, 1 }, {    64, 2 }, {    65, 2 }, // 1 to 2 bytes @64 (2^6)
        {   127, 2 }, {   128, 2 }, {   129, 2 },
        {   255, 2 }, {   256, 2 }, {   257, 2 },
        {   511, 2 }, {   512, 2 }, {   513, 2 },
        {  1023, 2 }, {  1024, 2 }, {  1025, 2 },
        {  2047, 2 }, {  2048, 2 }, {  2049, 2 },
        {  4095, 2 }, {  4096, 2 }, {  4097, 2 },
        {  8191, 2 }, {  8192, 3 }, {  8193, 3 }, // 2 to 3 bytes @8'192 (2^13)
        { 16483, 3 }, { 16484, 3 }, { 16485, 3 },
        { 32967, 3 }, { 32968, 3 }, { 32969, 3 },
        { 65535, 3 }, { 65536, 3 }, { 65537, 3 },

        { 1048575, 3 }, { 1048576, 4 }, { 1048577, 4 }, // 3 to 4 bytes @1'048'576 (2^20)

        // Takes up an additional byte every 7 bits
        { size_t(std::pow(2, 27)) - 1, 4 },
        { size_t(std::pow(2, 27)) + 0, 5 },
        { size_t(std::pow(2, 27)) + 1, 5 },

        { size_t(std::pow(2, 34)) - 1, 5 },
        { size_t(std::pow(2, 34)) + 0, 6 },
        { size_t(std::pow(2, 34)) + 1, 6 },

        // Warning: double-to-size_t conversion gets lossy after 53 bits more or less
    };
    for (const auto& tc : test_cases) {
        SizeT in{ tc.value };
        my_buffer_ptr buf = encode_binary(in);
        BOOST_CHECK_MESSAGE(
            buf->size() == tc.expected_bytes,
            "Value = " << tc.value
            << "; WANT " << tc.expected_bytes
            << " bytes, GOT " << buf->size()
        );
        SizeT out{ 0 };
        decode_binary(out, buf);
        BOOST_CHECK_EQUAL(in.value_, out.value_);
    }
}

BOOST_AUTO_TEST_CASE(test_record_simple_MessageHeader_binary) {
    MessageHeader hdr;

    // check zero-value (i.e. default-Ctor)
    BOOST_CHECK_EQUAL(0, hdr.m_protocol);
    BOOST_CHECK_EQUAL(0, hdr.m_messageType);
    BOOST_CHECK_EQUAL(0, hdr.m_correlationId);
    BOOST_CHECK_EQUAL(0, hdr.m_messageId);
    BOOST_CHECK_EQUAL(0, hdr.m_messageFlags);

    my_buffer_ptr buf = encode_binary(hdr);
    BOOST_CHECK_EQUAL(buf->size(), size_t(5)); // 5x zero = 5 bytes
    MessageHeader hdr_out;
    decode_binary(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);

    hdr.m_protocol       = 3; // Store
    hdr.m_messageType    = 2; // PutObject
    hdr.m_correlationId  = 256; // just over 1 byte
    hdr.m_messageId      = (1<<16); // just over 2 bytes
    hdr.m_messageFlags   = MsgFlags::eCOMPRESSED_BODY | MsgFlags::eACKNOWLEDGE_RECEIPT;

    buf = encode_binary(hdr);
    BOOST_CHECK_EQUAL(buf->size(), size_t(8));
    decode_binary(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);

    constexpr int32_t max32 = std::numeric_limits<int32_t>::max();
    constexpr int64_t max64 = std::numeric_limits<int64_t>::max();
    hdr.m_protocol       = max32;
    hdr.m_messageType    = max32;
    hdr.m_correlationId  = max64;
    hdr.m_messageId      = max64;
    hdr.m_messageFlags   = max32;

    buf = encode_binary(hdr);
    BOOST_CHECK_EQUAL(buf->size(), size_t(35));
    decode_binary(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);

    constexpr int32_t min32 = std::numeric_limits<int32_t>::min();
    constexpr int64_t min64 = std::numeric_limits<int64_t>::min();
    hdr.m_protocol       = min32;
    hdr.m_messageType    = min32;
    hdr.m_correlationId  = min64;
    hdr.m_messageId      = min64;
    hdr.m_messageFlags   = min32;

    buf = encode_binary(hdr);
    BOOST_CHECK_EQUAL(buf->size(), size_t(35));
    decode_binary(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);
}

// TODO: Move to EtpSchemasTests.cpp
BOOST_AUTO_TEST_CASE(test_getValidSchema) {
    std::string etp12 = getSpecFile(1, 2);
    BOOST_CHECK(!etp12.empty());

    avro::ValidSchema schema = getValidSchema(1, 2);
    BOOST_CHECK(schema.root().get());

    std::ostringstream oss;
    schema.root()->printBasicInfo(oss);
    std::string root_info = oss.str();
    BOOST_CHECK(!root_info.empty());

    BOOST_CHECK_EQUAL(avro::AVRO_UNION, schema.root()->type());
    BOOST_CHECK_EQUAL(size_t(228), schema.root()->leaves());

    avro::NodePtr msg_hdr_node;
    for (int i = 0; i < int(schema.root()->leaves()); ++i) {
        const avro::NodePtr& node = schema.root()->leafAt(i);
        if (node->hasName() &&
            (node->name().simpleName() == "MessageHeader") &&
            (node->name().ns() == "Energistics.Etp.v12.Datatypes")
        ) {
            BOOST_CHECK(!msg_hdr_node);
            msg_hdr_node = node;
        }
    }
    BOOST_REQUIRE(msg_hdr_node);

    avro::ValidSchema msg_hdr_schema1(msg_hdr_node);
    avro::ValidSchema msg_hdr_schema2 = getValidSchemaOfType12(
        "Energistics.Etp.v12.Datatypes.MessageHeader"
    );
    avro::ValidSchema msg_hdr_schema3 = MessageHeader::schema();

    // #2 and #3 share the same underlying Avro node, but not #1
    // since all calls to getValidSchema() reparses the etp-X.Y.avpr
    // and instantiates new nodes, while #2 et #3 share the same cache.
    BOOST_REQUIRE(msg_hdr_schema1.root() != msg_hdr_schema2.root());
    BOOST_REQUIRE_EQUAL(msg_hdr_schema2.root(), msg_hdr_schema3.root());
    BOOST_REQUIRE(msg_hdr_schema1.root() != msg_hdr_schema3.root());
}

BOOST_AUTO_TEST_CASE(test_record_simple_MessageHeader_json) {
    MessageHeader hdr;
    my_buffer_ptr buf = encode_json(hdr, false);
    BOOST_CHECK_EQUAL(buf->size(), size_t(79));
    MessageHeader hdr_out;
    decode_json(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);
    std::string buf_str(buf->begin(), buf->end());
    std::string expected_str =
        "{"
        "\"protocol\":0,"
        "\"messageType\":0,"
        "\"correlationId\":0,"
        "\"messageId\":0,"
        "\"messageFlags\":0"
        "}";
    BOOST_CHECK_EQUAL(buf_str, expected_str);

    buf = encode_json(hdr, true); // 2-char indent apparently
    BOOST_CHECK_EQUAL(buf->size(), size_t(100)); // 21 whitespace chars extra
    buf_str.assign(buf->begin(), buf->end());
    expected_str =
R"json({
  "protocol": 0,
  "messageType": 0,
  "correlationId": 0,
  "messageId": 0,
  "messageFlags": 0
})json"
    ;
    BOOST_CHECK_EQUAL(buf_str,expected_str);
    decode_json(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);

    hdr.m_protocol = 3; // Store
    hdr.m_messageType = 2; // PutObject
    hdr.m_correlationId = 256; // just over 1 byte
    hdr.m_messageId = (1 << 16); // just over 2 bytes
    hdr.m_messageFlags = MsgFlags::eCOMPRESSED_BODY | MsgFlags::eACKNOWLEDGE_RECEIPT;

    buf = encode_json(hdr, false);
    BOOST_CHECK_EQUAL(buf->size(), size_t(86));
    decode_json(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);
    buf_str.assign(buf->begin(), buf->end());
    expected_str =
        "{"
        "\"protocol\":3,"
        "\"messageType\":2,"
        "\"correlationId\":256,"
        "\"messageId\":65536,"
        "\"messageFlags\":24" // not very user friendly
        "}";
    BOOST_CHECK_EQUAL(buf_str, expected_str);

    buf = encode_json(hdr, true); // 2-char indent apparently
    BOOST_CHECK_EQUAL(buf->size(), size_t(107)); // 21 whitespace chars extra
    buf_str.assign(buf->begin(), buf->end());
    expected_str =
R"json({
  "protocol": 3,
  "messageType": 2,
  "correlationId": 256,
  "messageId": 65536,
  "messageFlags": 24
})json"
    ;
    BOOST_CHECK_EQUAL(buf_str,expected_str);
    decode_json(hdr_out, buf);
    BOOST_CHECK_EQUAL(hdr, hdr_out);
}

bool OpenSession_equals(const OpenSession& lhs, const OpenSession& rhs) {
    return
        lhs.m_applicationName               == rhs.m_applicationName     &&
        lhs.m_applicationVersion            == rhs.m_applicationVersion  &&
        lhs.m_serverInstanceId              == rhs.m_serverInstanceId &&
        lhs.m_supportedProtocols.size()     == rhs.m_supportedProtocols.size() && // TODO: full equals
        lhs.m_supportedDataObjects.size()   == rhs.m_supportedDataObjects.size()   &&
        lhs.m_supportedCompression          == rhs.m_supportedCompression &&
        lhs.m_supportedFormats.size()       == rhs.m_supportedFormats.size()
    ;
}

// record OpenSession depends on
//     record SupportedProtocol which depends on
//         record Version,
//         record DataValue which depends on
//             record ArrayOfDouble
//      record SupportedDataObject
BOOST_AUTO_TEST_CASE(test_record_composite_OpenSession_binary) {
    OpenSession ose;
    my_buffer_ptr buf = encode_binary(ose);
    BOOST_CHECK_EQUAL(buf->size(), size_t(41)); // 10 empty (top-level) fields, 41 bytes!
    OpenSession ose_out;
    decode_binary(ose_out, buf);
    BOOST_CHECK(OpenSession_equals(ose, ose_out));

    ose.m_applicationName = "App";
    ose.m_applicationVersion = "1.0";
    ose.m_supportedProtocols.resize(2);
    ose.m_supportedProtocols[0].m_protocol = int(Datatypes::Protocol::eCore);
    ose.m_supportedProtocols[0].m_protocolVersion.m_major = 1;
    ose.m_supportedProtocols[0].m_protocolVersion.m_minor= 2;
    ose.m_supportedProtocols[1].m_protocol = int(Datatypes::Protocol::eTransaction);
    ose.m_supportedProtocols[1].m_protocolVersion.m_major = 1;
    ose.m_supportedProtocols[1].m_protocolVersion.m_minor = 2;
    ose.m_supportedDataObjects.assign({ sdo("foo"), sdo("bar"), sdo("etc...") });
    ose.m_supportedCompression = "gzip";
    ose.m_supportedFormats.assign({ "xml" });

    buf = encode_binary(ose);
    BOOST_CHECK_EQUAL(buf->size(), size_t(90));
    decode_binary(ose_out, buf);
    BOOST_CHECK(OpenSession_equals(ose, ose_out));
}

BOOST_AUTO_TEST_CASE(test_record_composite_OpenSession_json) {
    OpenSession ose;
    my_buffer_ptr buf = encode_json(ose, false);
    BOOST_CHECK_EQUAL(buf->size(), size_t(450));
    OpenSession ose_out;
    decode_json(ose_out, buf);
    BOOST_CHECK(OpenSession_equals(ose, ose_out));
    std::string buf_str(buf->begin(), buf->end());
    std::string expected_str =
        "{"
        "\"applicationName\":\"\","
        "\"applicationVersion\":\"\","
        "\"serverInstanceId\":\"\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\","
        "\"supportedProtocols\":[],"
        "\"supportedDataObjects\":[],"
        "\"supportedCompression\":\"\","
        "\"supportedFormats\":[],"
        "\"currentDateTime\":0,"
        "\"earliestRetainedChangeTime\":0,"
        "\"sessionId\":\"\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\","
        "\"endpointCapabilities\":{}"
        "}";
    BOOST_CHECK_EQUAL(buf_str, expected_str);

    std::array<uint8_t, 16> serverInstanceId;
    assignUid(serverInstanceId, oes::core::Guid("540e4ce2-d2f7-4726-b0cc-1fc28d5a7ca5"));
    ose.m_applicationName = "App";
    ose.m_applicationVersion = "1.0";
    ose.m_serverInstanceId = serverInstanceId;
    ose.m_supportedProtocols.resize(2);
    ose.m_supportedProtocols[0].m_protocol = int(Datatypes::Protocol::eCore);
    ose.m_supportedProtocols[0].m_protocolVersion.m_major = 1;
    ose.m_supportedProtocols[0].m_protocolVersion.m_minor = 2;
    ose.m_supportedProtocols[0].m_role = "consumer";
    ose.m_supportedProtocols[1].m_protocol = int(Datatypes::Protocol::eTransaction);
    ose.m_supportedProtocols[1].m_protocolVersion.m_major = 1;
    ose.m_supportedProtocols[1].m_protocolVersion.m_minor = 2;
    ose.m_supportedDataObjects.assign({ sdo("foo"), sdo("bar"), sdo("etc...") });
    ose.m_supportedCompression = "gzip";
    ose.m_supportedFormats.assign({ "xml" });

    buf = encode_json(ose, false);
    BOOST_CHECK_EQUAL(buf->size(), size_t(829));
    decode_json(ose_out, buf);
    BOOST_CHECK(OpenSession_equals(ose, ose_out));

    buf_str.assign(buf->begin(), buf->end());
    expected_str =
        "{"
        "\"applicationName\":\"App\","
        "\"applicationVersion\":\"1.0\","
        "\"serverInstanceId\":\"T\\u000eL\\u00e2\\u00d2\\u00f7G&\\u00b0\\u00cc\\u001f\\u00c2\\u008dZ|\\u00a5\","
        "\"supportedProtocols\":[{"
            "\"protocol\":0,"
            "\"protocolVersion\":{"
                "\"major\":1,"
                "\"minor\":2,"
                "\"revision\":0,"
                "\"patch\":0"
            "},"
            "\"role\":\"consumer\","
            "\"protocolCapabilities\":{}"
        "},{"
            "\"protocol\":18,"
            "\"protocolVersion\":{"
                "\"major\":1,"
                "\"minor\":2,"
                "\"revision\":0,"
                "\"patch\":0"
            "},"
            "\"role\":\"\","
            "\"protocolCapabilities\":{}"
        "}],"
        "\"supportedDataObjects\":["
            "{\"qualifiedType\":\"foo\",\"dataObjectCapabilities\":{}},"
            "{\"qualifiedType\":\"bar\",\"dataObjectCapabilities\":{}},"
            "{\"qualifiedType\":\"etc...\",\"dataObjectCapabilities\":{}}"
        "],"
        "\"supportedCompression\":\"gzip\","
        "\"supportedFormats\":[\"xml\"],"
        "\"currentDateTime\":0,"
        "\"earliestRetainedChangeTime\":0,"
        "\"sessionId\":\"\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\\u0000\","
        "\"endpointCapabilities\":{}"
        "}";
    BOOST_CHECK_EQUAL(buf_str, expected_str);

    buf = encode_json(ose, true);
    BOOST_CHECK_EQUAL(buf->size(), size_t(1232));
    decode_json(ose_out, buf);
    BOOST_CHECK(OpenSession_equals(ose, ose_out));

    buf_str.assign(buf->begin(), buf->end());
    expected_str =
R"json({
  "applicationName": "App",
  "applicationVersion": "1.0",
  "serverInstanceId": "T\u000eL\u00e2\u00d2\u00f7G&\u00b0\u00cc\u001f\u00c2\u008dZ|\u00a5",
  "supportedProtocols": [
    {
      "protocol": 0,
      "protocolVersion": {
        "major": 1,
        "minor": 2,
        "revision": 0,
        "patch": 0
      },
      "role": "consumer",
      "protocolCapabilities": {
        
      }
    },
    {
      "protocol": 18,
      "protocolVersion": {
        "major": 1,
        "minor": 2,
        "revision": 0,
        "patch": 0
      },
      "role": "",
      "protocolCapabilities": {
        
      }
    }
  ],
  "supportedDataObjects": [
    {
      "qualifiedType": "foo",
      "dataObjectCapabilities": {
        
      }
    },
    {
      "qualifiedType": "bar",
      "dataObjectCapabilities": {
        
      }
    },
    {
      "qualifiedType": "etc...",
      "dataObjectCapabilities": {
        
      }
    }
  ],
  "supportedCompression": "gzip",
  "supportedFormats": [
    "xml"
  ],
  "currentDateTime": 0,
  "earliestRetainedChangeTime": 0,
  "sessionId": "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000",
  "endpointCapabilities": {
    
  }
})json";
    BOOST_CHECK_EQUAL(buf_str, expected_str);
}

BOOST_AUTO_TEST_CASE(test_union_AnyArray_binary) {
    AnyArray ary;
    AnyArray ary_out;

    std::vector<int> int_vec{ 1, 2, 0, -1, 3 };
    ArrayOfInt int_ary;
    int_ary.m_values = int_vec;
    ary.m_item = int_ary;
    BOOST_CHECK_EQUAL(ary.m_item.index(), size_t(AnyArray_item_e::eArrayOfInt));

    my_buffer_ptr buf = encode_binary(ary);
    BOOST_CHECK_EQUAL(buf->size(), size_t(8)); // for 5 small ints!
    decode_binary(ary_out, buf);
    BOOST_CHECK_EQUAL(ary.m_item.index(), ary_out.m_item.index());
    BOOST_CHECK(int_vec == std::get<ArrayOfInt>(ary_out.m_item).m_values);
}

BOOST_AUTO_TEST_CASE(test_union_AnyArray_json) {
    AnyArray ary;
    AnyArray ary_out;
    std::string buf_str;

    std::vector<int> int_vec{ 1, 2, 0, -1, 3 };
    ArrayOfInt int_ary;
    int_ary.m_values = int_vec;
    ary.m_item = int_ary;
    BOOST_CHECK_EQUAL(ary.m_item.index(), size_t(AnyArray_item_e::eArrayOfInt));

    my_buffer_ptr buf = encode_json(ary, false);
    BOOST_CHECK_EQUAL(buf->size(), size_t(77));
    decode_json(ary_out, buf);
    BOOST_CHECK_EQUAL(ary.m_item.index(), ary_out.m_item.index());
    BOOST_CHECK(int_vec == std::get<ArrayOfInt>(ary_out.m_item).m_values);

    buf_str.assign(buf->begin(), buf->end());
    const std::string expected_str = "{"
        "\"item\":{\"Energistics.Etp.v12.Datatypes.ArrayOfInt\":{\"values\":[1,2,0,-1,3]}}"
        "}";
    BOOST_CHECK_EQUAL(buf_str, expected_str);
}

BOOST_AUTO_TEST_CASE(test_record_with_uuid_binary) {
    using namespace etp12::Protocol::Transaction;

    CommitTransaction rec;

    const decltype(rec.m_transactionUuid) null_uuid{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    // check zero-value (i.e. default-Ctor)
    BOOST_CHECK(rec.m_transactionUuid == null_uuid);

    my_buffer_ptr buf = encode_binary(rec);
    BOOST_CHECK_EQUAL(buf->size(), size_t(16)); // zeros not compressed?
    CommitTransaction rec_out;
    decode_binary(rec_out, buf);
    BOOST_CHECK(rec.m_transactionUuid == rec_out.m_transactionUuid);

    const decltype(rec.m_transactionUuid) arbitrary_uuid{
        255, 1, 45, 0, 0, 0, 55, 0, 0, 0, 7, 0, 0, 0, 0, 99
    };
    rec.m_transactionUuid = arbitrary_uuid;

    buf = encode_binary(rec);
    BOOST_CHECK_EQUAL(buf->size(), size_t(16));
    decode_binary(rec_out, buf);
    BOOST_CHECK(rec.m_transactionUuid == rec_out.m_transactionUuid);

    // Shows how to get a OES Guid from the boost::array
    using oes::core::Guid;
    Guid null_guid;
    Guid arbi_guid;
    Guid rec1_guid;
    Guid rec2_guid;
    assignUid(null_guid, null_uuid);
    assignUid(arbi_guid, arbitrary_uuid);
    assignUid(rec1_guid, rec.m_transactionUuid);
    assignUid(rec2_guid, rec_out.m_transactionUuid);

    BOOST_CHECK(!null_guid.isValid());
    BOOST_CHECK(arbi_guid.isValid());
    BOOST_CHECK_EQUAL(arbi_guid, rec1_guid);
    BOOST_CHECK_EQUAL(arbi_guid, rec2_guid);

    const Guid other_guid = Guid::make();
    BOOST_CHECK(other_guid != arbi_guid);
    std::memcpy(rec.m_transactionUuid.data(), other_guid.begin(), Guid::static_size());
    Guid tx_uuid;
    assignUid(tx_uuid, rec.m_transactionUuid);
    BOOST_CHECK_EQUAL(other_guid, tx_uuid);

    // wrapper for memcpy above
    assignUid(rec_out.m_transactionUuid, other_guid);

    Guid yet_another_guid;
    assignUid(yet_another_guid, rec_out.m_transactionUuid);
    BOOST_CHECK_EQUAL(other_guid, yet_another_guid);
}

BOOST_AUTO_TEST_CASE(test_record_with_uuid_json) {
    using namespace etp12::Protocol::Transaction;

    CommitTransaction rec;

    my_buffer_ptr buf = encode_json(rec, false);
    BOOST_CHECK_EQUAL(buf->size(), size_t(118)); // zeros not compressed?
    CommitTransaction rec_out;
    decode_json(rec_out, buf);
    BOOST_CHECK(rec.m_transactionUuid == rec_out.m_transactionUuid);
    std::string buf_str(buf->begin(), buf->end());
    std::string expected_str =
        R"json({"transactionUuid":"\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000"})json";
    BOOST_CHECK_EQUAL(buf_str, expected_str);

    using oes::core::Guid;
    Guid guid = Guid::parse("632a84c2-e821-11e8-9f32-f2801f1b9fd1");
    assignUid(rec.m_transactionUuid, guid);

    buf = encode_json(rec, false);
    BOOST_CHECK_EQUAL(buf->size(), size_t(98));
    decode_json(rec_out, buf);
    BOOST_CHECK(rec.m_transactionUuid == rec_out.m_transactionUuid);

    buf_str.assign(buf->begin(), buf->end());
    expected_str =
        R"json({"transactionUuid":"c*\u0084\u00c2\u00e8!\u0011\u00e8\u009f2\u00f2\u0080\u001f\u001b\u009f\u00d1"})json";
    BOOST_CHECK_EQUAL(buf_str, expected_str);
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

struct MyMsgWithStdArray {
    std::array<uint8_t, 16> m_requestUuid;

    void encode(avro::Encoder& e) const {
        avro::encode(e, m_requestUuid);
    }
    void decode(avro::Decoder& d) {
        avro::decode(d, m_requestUuid);
    }
};

struct MyMsgWithBoostArray {
    boost::array<uint8_t, 16> m_requestUuid;

    void encode(avro::Encoder& e) const {
        avro::encode(e, m_requestUuid);
    }
    void decode(avro::Decoder& d) {
        avro::decode(d, m_requestUuid);
    }
};

struct MyMsgWithArrays {
    MyMsgWithStdArray std_array_;
    MyMsgWithBoostArray boost_array_;

    void encode(avro::Encoder& e) const {
        avro::encode(e, std_array_);
        avro::encode(e, boost_array_);
    }
    void decode(avro::Decoder& d) {
        avro::decode(d, std_array_);
        avro::decode(d, boost_array_);
    }
};

} // namespace

namespace avro {

template <> struct codec_traits<MyMsgWithStdArray> {
    static void encode(Encoder& e, const MyMsgWithStdArray& r) { r.encode(e); }
    static void decode(Decoder& d, MyMsgWithStdArray& r) { r.decode(d); }
};
template <> struct codec_traits<MyMsgWithBoostArray> {
    static void encode(Encoder& e, const MyMsgWithBoostArray& r) { r.encode(e); }
    static void decode(Decoder& d, MyMsgWithBoostArray& r) { r.decode(d); }
};
template <> struct codec_traits<MyMsgWithArrays> {
    static void encode(Encoder& e, const MyMsgWithArrays& r) { r.encode(e); }
    static void decode(Decoder& d, MyMsgWithArrays& r) { r.decode(d); }
};

} // namespace avro

namespace {

//============================================================================

BOOST_AUTO_TEST_SUITE(AvroUtilsTests)

BOOST_AUTO_TEST_CASE(test_arrays) {
    using oes::core::Guid;
    const Guid uid1 = Guid::make();
    const Guid uid2 = Guid::make();

    MyMsgWithArrays msg;
    assignUid(msg.std_array_.m_requestUuid, uid1);
    assignUid(msg.boost_array_.m_requestUuid, uid2);

    my_buffer_ptr buf = encode_binary(msg);
    BOOST_CHECK_EQUAL(buf->size(), size_t(32)); // 2x 16-bytes Guids

    MyMsgWithArrays msg_out;
    decode_binary(msg_out, buf);

    Guid uid1_out;
    Guid uid2_out;

    assignUid(uid1_out, msg_out.std_array_.m_requestUuid);
    assignUid(uid2_out, msg_out.boost_array_.m_requestUuid);

    BOOST_CHECK_EQUAL(uid1, uid1_out);
    BOOST_CHECK_EQUAL(uid2, uid2_out);
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
