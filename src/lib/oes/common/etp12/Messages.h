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

#ifndef OES_COMMON_ETP12_MESSAGES_H
#define OES_COMMON_ETP12_MESSAGES_H

#include "../export.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace avro {
    class Encoder;
    class Decoder;
    class ValidSchema;
}

namespace oes::common::etp12 {

struct AbstractMessage;
using  AbstractMessage_uptr = std::unique_ptr<AbstractMessage>;
using  AbstractMessage_ptr  = std::shared_ptr<AbstractMessage>;
using  MessageFactory = AbstractMessage_uptr(*)();

template <typename T>
constexpr MessageFactory msgFactoryFor() {
    static_assert(sizeof(std::unique_ptr<T>) == sizeof(void*));
    return []() -> AbstractMessage_uptr { return std::make_unique<T>(); };
}

struct MsgType {
    const int proto_;
    const int type_;
};

constexpr
bool operator==(MsgType lhs, MsgType rhs) {
    return (lhs.proto_ == rhs.proto_) && (lhs.type_ == rhs.type_);
}

// Semantic of an ETP message
enum class MsgKind : uint8_t {
    eRequest, // client request
    eResponse, // server response
    eNotification, // response too, but "unsolicitated" (w/o a request)
    eAuxResponse, // optional response, in addition to a "main" response
    eChunk, // piece of a request or response, "externalized" in its own message
};

// Client request IDs must be even, starting at 2 (2, 4, 6, etc...)
// Server response IDs must be odd, starting at 1 (1, 3, 5, etc...)
// Yes, this leads to weird msg ID#2 replied to by msg ID#1...
class MsgId {
    std::atomic_int64_t value_;

public:
    MsgId(const MsgId&) = delete;
    MsgId& operator=(const MsgId&) = delete;

    MsgId(MsgId&&) = delete;
    MsgId& operator=(MsgId&&) = delete;

    explicit MsgId(MsgKind kind) : value_((kind == MsgKind::eRequest)? 0: -1) {}

    int64_t nextId() {
        int64_t id = value_.fetch_add(2, std::memory_order_relaxed);
        return id + 2;
    }

    int64_t currentId() {
        return value_.load();
    }
};

enum class ApiType : uint8_t {
    eNone,  // not a request (i.e. notification)
    eGet,   // (read-only)
    ePut,   // UPSERT, i.e. INSERT or UPDATE (requires write access)
    eDel,   // (requires write access)
    eSub,   // subscribe to notifications
    eMisc,  // e.g. OpenSession, Ping, etc...
};

// Message Cardinality
enum class MsgCard : uint8_t {
    eSinglePart,
    eMultiPart,
};

struct MsgInfo {
    constexpr MsgInfo(
        int proto_id, int type_id,
        MsgKind kind, MsgCard card, ApiType api,
        std::string_view name, std::string_view label, std::string_view ns,
        MessageFactory factory
    ) : type_{ proto_id, type_id }
      , kind_(kind)
      , card_(card)
      , api_(api)
      , name_(name)
      , label_(label)
      , dotns_(ns)
      , factory_(factory)
    {
    }

    const MsgType type_;
    const MsgKind kind_; // req, res, notif, ...
    const MsgCard card_; // cardinality
    const ApiType api_;

    const std::string_view name_;
    const std::string_view label_;
    const std::string_view dotns_; // dot-separated

    const MessageFactory factory_;

    constexpr bool isMultiPart() const {
        return card_ == MsgCard::eMultiPart;
    }

    static constexpr const MsgInfo* lookup(int proto, int type);
    static constexpr const MsgInfo* lookup(MsgType type) {
        return lookup(type.proto_, type.type_);
    }
};

constexpr
bool operator==(const MsgInfo& lhs, const MsgInfo& rhs) {
    return std::tie(
        lhs.type_, lhs.kind_, lhs.card_, lhs.api_, lhs.name_, lhs.label_, lhs.dotns_
    ) == std::tie(
        rhs.type_, rhs.kind_, rhs.card_, rhs.api_, rhs.name_, rhs.label_, rhs.dotns_
    );
}

constexpr
MsgType typeOf(const MsgInfo& info) {
    return info.type_;
}

struct MsgVariant;

struct AbstractMessage {
    virtual ~AbstractMessage() = default;

    virtual const MsgInfo& getInfo() const = 0;

    virtual void encode(avro::Encoder&) const = 0;
    virtual void decode(avro::Decoder&) = 0;
};

} // namespace oes::common::etp12

// Warning: this macro assumes types in oes::common::etp12 are "in-scope"
#define OES_COMMON_ETP12_MESSAGE(card, api, kind, proto, proto_id, type, type_id) \
    static constexpr MsgInfo info{                                              \
        /* .type_.proto_ = */ int(Datatypes::Protocol::e##proto),               \
        /* .type_.type_  = */ type_id,                                          \
        /* .kind_    = */ MsgKind::e##kind,                                     \
        /* .card_    = */ MsgCard::e##card,                                     \
        /* .api_     = */ ApiType::e##api,                                      \
        /* .name_    = */ #type,                                                \
        /* .label_   = */ #proto #proto_id "." #type  #type_id,                 \
        /* .dotns_   = */ "Energistics.Etp.v12.Protocols."  #proto "." #type,   \
        /* .factory_ = */ msgFactoryFor<type>()                                 \
    };                                                                          \
    const MsgInfo& getInfo() const override { return info; }                    \
    static_assert(int(Datatypes::Protocol::e##proto) == proto_id) // caller supplies the ;

#include "codegen/Messages.i.h"

#undef OES_COMMON_ETP12_MESSAGE

namespace oes::common::etp12 {

constexpr
MsgType typeOf(const Datatypes::MessageHeader& hdr) {
    return { hdr.m_protocol, hdr.m_messageType };
}

OES_COMMON_EXPORT
std::string msgHeaderAsString(
    const Datatypes::MessageHeader& msg_hdr,
    const MsgInfo* msg_info = nullptr,
    const bool full_flag_desc = false
);

} // namespace oes::common::etp12

#endif
