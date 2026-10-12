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

#ifndef OES_COMMON_ETP12_METADATA_H
#define OES_COMMON_ETP12_METADATA_H

#include "../export.h"

#include <map>
#include <string>

namespace avro {
    class Encoder;
    class Decoder;
    class ValidSchema;
}

namespace oes::common::etp12 {

struct OES_COMMON_EXPORT ProtocolInfo {
    int protocolId_ = -1;

    std::string name_;

    std::string label() const;
};

struct OES_COMMON_EXPORT MessageInfo {
    int protocolId_ = -1;
    int messageTypeId_ = -1;
    bool multiPart_ = false;

    std::string name_;
    std::string ns_; // dot-separated

    std::string label() const;
    std::string qname(const char* sep = ".") const;
    avro::ValidSchema schema() const;
};

OES_COMMON_EXPORT
const std::map<int, const ProtocolInfo>& protocols();

OES_COMMON_EXPORT
const std::map<std::pair<int, int>, const MessageInfo>& messages();

OES_COMMON_EXPORT
const std::map<std::pair<int, int>, std::pair<int, int>>& apis();

OES_COMMON_EXPORT
avro::ValidSchema schemaOf(int protocolId, int messageTypeId);

OES_COMMON_EXPORT
bool recodeMessage(int protocolId, int messageTypeId, avro::Decoder&, avro::Encoder&);

OES_COMMON_EXPORT
std::string messageToJson(int protocolId, int messageTypeId, avro::Decoder&, bool pretty);

} // namespace oes::common::etp12

#endif
