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

#ifndef OES_COMMON_ETP12_PROTOCOLS_H
#define OES_COMMON_ETP12_PROTOCOLS_H

#include "Messages.h"
#include "MsgFlags.h"

#include <functional>
#include <memory>

namespace oes::common::etp12::Protocol {

//============================================================================

using Datatypes::MessageHeader;
using Datatypes::MessageHeaderExtension;
using Datatypes::Uuid;
using Protocol::Store::Chunk;
using Core::ProtocolException;

struct OES_COMMON_EXPORT EtpResponseStatus {
    bool ok = false;
    bool final_part = true;
    ProtocolException ex;


    std::vector<std::shared_ptr<AbstractMessage>> extra_messages;
    std::vector<uint8_t> chunks;
    Uuid chunk_uuid;

    bool has_error() const {
        return ex.m_error.has_value() || !ex.m_errors.empty();
    }
};

//============================================================================

struct OES_COMMON_EXPORT IRequest {
    virtual const MessageHeader& header() const = 0;
    virtual const MessageHeaderExtension* headerExt() const { return nullptr; }
    virtual AbstractMessage_uptr takeBody() = 0;

    virtual bool hasChunks() const { return false; }
    virtual std::vector<Chunk> takeChunks() { return {}; }; // takes ownnership / move
    virtual bool retainBodyNeedingChunks(AbstractMessage_uptr) { return false; };

    bool has_FIN_bit() const {
        return ::oes::common::etp12::has_FIN_bit(header());
    }

protected:
    virtual ~IRequest() = default;
};

inline
MsgType typeOf(const IRequest& ireq) {
    return ::oes::common::etp12::typeOf(ireq.header());
}

//============================================================================

template <typename T>
using EtpResponseHandler = std::function<void(EtpResponseStatus rc, T res)>;
using EtpCompleteHandler = std::function<void(EtpResponseStatus rc)>;
using EtpWrappedResponse = std::function<void(avro::Encoder&)>;
using EtpWrappedResponseHandler = std::function<void(EtpResponseStatus rc, EtpWrappedResponse wres)>;

using WrappedMessageEncoder = std::function<void (avro::Encoder&)>; // i.e. EtpWrappedResponse
using WrappedMessageHandlerVoid = std::function<void(EtpResponseStatus rc)>; // i.e. EtpCompleteHandler
using WrappedMessageHandlerResp = std::function<void(EtpResponseStatus rc, ::avro::Decoder& dec)>;

using WrappedMessageSenderVoid = std::function<
    void (IRequest& ireq, WrappedMessageEncoder msg_enc, WrappedMessageHandlerVoid decode_and_done_cb)
>;
using WrappedMessageSenderResp = std::function<
    void (IRequest& ireq, WrappedMessageEncoder msg_enc, WrappedMessageHandlerResp decode_and_process_db)
>;

//============================================================================

struct IProtocol;
using  IProtocol_ptr  = std::shared_ptr<IProtocol>;
using  IProtocol_wptr = std::weak_ptr  <IProtocol>;
using  IProtocol_uptr = std::unique_ptr<IProtocol>;

struct OES_COMMON_EXPORT IProtocol {
    const Datatypes::Protocol id_;
    const std::string name_;
    const std::string name_id_;

public:
    virtual ~IProtocol() = default;

    virtual const char* messageTypeName(int messageType) const = 0;
    virtual int responseMessageType(int requestMessageType) const = 0;

    virtual void dispatchRequest(
        IRequest& req, EtpWrappedResponseHandler res_handler
    ) = 0;

    static IProtocol_uptr makeClientImpl(
        Datatypes::Protocol proto_id,
        WrappedMessageSenderVoid void_sender,
        WrappedMessageSenderResp resp_sender
    );

protected:
    IProtocol(Datatypes::Protocol id, const char* name);

private:
    IProtocol& operator=(const IProtocol&) = delete;
};

//============================================================================

} // namespace oes::common::etp12::Protocol

#include "codegen/Protocols.i.h"

#endif
