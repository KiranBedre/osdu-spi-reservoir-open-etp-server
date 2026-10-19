// ============================================================================
// Copyright 2022-2023 Aspen Technology, Inc. All rights reserved.
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

#include <oes/common/etp12/Metadata.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Specific.hh>
#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

BOOST_AUTO_TEST_SUITE(Etp12MessagesTests)

BOOST_AUTO_TEST_CASE(ProtocolInfo_Label_Test)
{
    oes::common::etp12::ProtocolInfo protocolInfo;
    protocolInfo.protocolId_ = 1;
    protocolInfo.name_ = "Protocol 1";

    std::string expectedLabel = "Protocol 1(1)";
    std::string actualLabel = protocolInfo.label();


    BOOST_CHECK_EQUAL(actualLabel, expectedLabel);
}

BOOST_AUTO_TEST_CASE(MessageInfo_Label_Test)
{
    oes::common::etp12::MessageInfo messageInfo;
    messageInfo.protocolId_ = 1;
    messageInfo.messageTypeId_ = 2;
    messageInfo.name_ = "Message 2";
    messageInfo.ns_ = "oes.eml.etp12";

    std::string expectedLabel = "ChannelStreaming(1).Message 2(2)";
    std::string actualLabel = messageInfo.label();

    BOOST_CHECK_EQUAL(actualLabel, expectedLabel);
}

BOOST_AUTO_TEST_CASE(MessageInfo_QName_Test)
{
    oes::common::etp12::MessageInfo messageInfo;
    messageInfo.protocolId_ = 1;
    messageInfo.messageTypeId_ = 2;
    messageInfo.name_ = "Message 2";
    messageInfo.ns_ = "oes.eml.etp12";

    std::string expectedQName = "oes.eml.etp12.Message 2";
    std::string actualQName = messageInfo.qname();

    BOOST_CHECK_EQUAL(actualQName, expectedQName);
}

BOOST_AUTO_TEST_CASE(SchemaOf_Test)
{
    int protocolId = 1;
    int messageTypeId = 2;

    avro::ValidSchema schema = oes::common::etp12::schemaOf(protocolId, messageTypeId);

    // Perform assertions on the schema, e.g., check if it is valid or has expected fields
    // ...
    // For demonstration purposes, we'll just check if the schema is not null
    BOOST_CHECK(schema.root());
}

/*
BOOST_AUTO_TEST_CASE(RecodeMessage_Test)
{
    int protocolId = 1;
    int messageTypeId = 2;

    // Create an avro::Decoder and avro::Encoder with test data
    const ::avro::DecoderPtr decoder = ::avro::binaryDecoder();
    const avro::EncoderPtr encoder = ::avro::binaryEncoder();

    bool result = oes::eml::etp12::recodeMessage(protocolId, messageTypeId, *decoder, *encoder);

    // Perform assertions on the result, e.g., check if the recoding was successful
    BOOST_CHECK(result);
}

BOOST_AUTO_TEST_CASE(MessageToJson_Test)
{
    int protocolId = 1;
    int messageTypeId = 2;

    // Create an avro::Decoder with test data
    const ::avro::DecoderPtr decoder = ::avro::binaryDecoder();

    bool pretty = true;
    std::string json = oes::eml::etp12::messageToJson(protocolId, messageTypeId, *decoder, pretty);

    // Perform assertions on the generated JSON, e.g., check if it has expected fields or structure
    // ...
    // For demonstration purposes, we'll just check if the JSON is not empty
    BOOST_CHECK(!json.empty());
}
*/

BOOST_AUTO_TEST_SUITE_END()
