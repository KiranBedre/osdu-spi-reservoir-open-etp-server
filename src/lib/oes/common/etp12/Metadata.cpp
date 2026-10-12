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

#include "Metadata.h"

#include "Messages.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Specific.hh>
#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <sstream>

namespace {

template <typename TMsg>
bool recode(avro::Decoder& dec, avro::Encoder& enc) {
    try {
        TMsg msg;
        msg.decode(dec);
        msg.encode(enc);
        return true;
    } catch (...) {
    }
    return false;
}

} // namespace

namespace oes::common::etp12 {

std::string ProtocolInfo::label() const {
    std::ostringstream oss;
    oss << name_ << '(' << protocolId_ << ')';
    return oss.str();
}

std::string MessageInfo::label() const {
    auto proto_found = protocols().find(protocolId_);
    std::string proto_name;
    if (proto_found != protocols().end()) {
        proto_name = proto_found->second.name_;
    }
    std::ostringstream oss;
    oss << proto_name << '(' << protocolId_ << ')'
        << '.' << name_ << '(' << messageTypeId_ << ')';
    return oss.str();
}

std::string MessageInfo::qname(const char* sep) const {
    std::ostringstream oss;
    const bool nsdot = ns_.find('.') != std::string::npos;
    const bool isdot = sep && sep[0] == '.' && sep[1] == '\0';
    if (!nsdot || isdot) {
        oss << ns_ << sep << name_;
    } else {
        oss << ns_ << sep << name_; // TODO
    }
    return oss.str();
}

avro::ValidSchema MessageInfo::schema() const {
    return schemaOf(protocolId_, messageTypeId_);
}

std::string messageToJson(int protocolId, int messageTypeId, avro::Decoder& dec, bool pretty) {
    ::avro::ValidSchema schema = schemaOf(protocolId, messageTypeId);
    if (!schema.root().get()) {
        return std::string();
    }

    auto enc = pretty? ::avro::jsonPrettyEncoder(schema): ::avro::jsonEncoder(schema);
    auto out = ::avro::memoryOutputStream();
    enc->init(*out);

    if (!recodeMessage(protocolId, messageTypeId, dec, *enc)) {
        return std::string();
    }

    enc->flush(); // calls ::avro::OutputStream::backup(), for minimum message

    auto buf  = ::avro::snapshot(*out);
    return std::string(buf->begin(), buf->end());
}

} // namespace oes::common::etp12

#include "codegen/Metadata.i.ipp"
