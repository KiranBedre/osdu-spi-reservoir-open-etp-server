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

#ifndef OES_EML_OPENKV_STOREOSDUPROTO_H
#define OES_EML_OPENKV_STOREOSDUPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>

namespace oes::eml::openkv {

using oes::common::etp12::Protocol::IRequest;
using oes::common::etp12::Protocol::EtpResponseHandler;
using oes::common::etp12::Protocol::EtpResponseStatus;

template <typename T> class LocalTransactionWriteRaii;

//============================================================================

struct OES_EML_EXPORT OpenKVStoreOSDU : BaseProto, oes::common::etp12::Protocol::IStoreOSDU
{
    OpenKVStoreOSDU(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    virtual void CopyDataObjectsByValue( // messageTypeId 1
        IRequest& ireq, CopyDataObjectsByValue_req_t req,
        EtpResponseHandler<CopyDataObjectsByValue_res_t> process_cb // messageTypeId 2
    ) final;

    EtpResponseStatus copyDataArray(const oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier& dataArrraySource,
        const oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier& dataArrrayTarget);

    // All below is copied (and tweaked a bit) from ServerTesting.hpp
    template <typename T>
    MessageHeader headerOf() {
        MessageHeader req_hdr;
        req_hdr.m_protocol    = T::info.type_.proto_;
        req_hdr.m_messageType = T::info.type_.type_;
        return req_hdr;
    }//============================================================================

    struct RequestHeader final : IRequest {
        MessageHeader head_;

        const MessageHeader& header() const override { return head_; }
        oes::common::etp12::AbstractMessage_uptr takeBody() override { return nullptr; }
    };

    template <typename T>
    RequestHeader requestOf() {
        RequestHeader ireq;
        ireq.head_ = headerOf<T>();
        return ireq;
    }

    //============================================================================

    template <typename PROTO, typename REQ, typename RES>
    std::pair<EtpResponseStatus, RES> rpcCall(
        void (PROTO::* msg)(IRequest& ireq, REQ req,EtpResponseHandler<RES> process_cb),
        const REQ& request) {
        EtpResponseStatus status;
        RES response;
        RequestHeader ireq = requestOf<REQ>();

        PROTO proto(repo(), session_);
        (proto.*msg)(
            ireq, request,
            [&status, &response](EtpResponseStatus rc, RES res) {
                status  = std::move(rc);
                response = std::move(res);
            }
        );

        return std::make_pair(status, response);
    }

protected:
    friend LocalTransactionWriteRaii<OpenKVStoreOSDU>;
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_STOREOSDUPROTO_H
