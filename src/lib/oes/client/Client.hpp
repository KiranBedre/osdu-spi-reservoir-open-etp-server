// ============================================================================
// Copyright 2016-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CLIENT_HPP
#define OES_CLIENT_HPP

#include "Client.h"

#include <oes/common/etp12/MsgFlags.h>
#include <oes/common/QueueUtils.hpp>

#include <cassert>
#include <chrono>
#include <future>
#include <ostream>

namespace oes::client {

namespace etp = oes::common::etp12;
using oes::common::etp12::Datatypes::ErrorInfo;
using oes::common::etp12::Datatypes::MessageHeader;

using oes::common::etp12::Protocol::IRequest;
using oes::common::etp12::Protocol::EtpResponseStatus;
using oes::common::etp12::Protocol::EtpResponseHandler;
using oes::common::etp12::Protocol::EtpCompleteHandler;

//============================================================================

struct RequestHeader final : IRequest {
    MessageHeader head_;

    const MessageHeader& header() const override { return head_; }
    etp::AbstractMessage_uptr takeBody() override { return nullptr; }
};

//============================================================================

struct OES_CLIENT_EXPORT BaseRPC {
    using clock_t = std::chrono::high_resolution_clock;

    EtpResponseStatus rc_;

    double encode_sec_ = 0;
    double waited_sec_ = 0;

    void begin_multi_part() {
        assert(!correlation_id_);
        correlation_id_ = -1; // i.e. an invalid correlation ID
    }

    void sending_final_part() {
        assert(correlation_id_ == -1);
        correlation_id_ = -correlation_id_;
    }

    void reset_multi_part() {
        correlation_id_ = 0;
    }

protected:
    void reset() {
        checkpoint_ = clock_t::now();

        // reset rc, just in case
        rc_ = EtpResponseStatus();
    }

    RequestHeader encoding(etp::MsgType type, int msgFlags = 0) {
        reset();

        RequestHeader ireq;
        MessageHeader& req_hdr = ireq.head_; // ignored mostly
        req_hdr.m_protocol    = type.proto_;
        req_hdr.m_messageType = type.type_;
        req_hdr.m_messageFlags |= msgFlags;

        // TODO: Find a way to Change Protocols.h WrappedMessageSender* to return the messageId
        //       used by the client, to allow correlating multi-part messages.

        if (!correlation_id_) {
            req_hdr.m_messageFlags |= etp::MsgFlags::eFINALPART; // set
        } else {
            if (correlation_id_ == -1) {
                // first request message of a multi-part sequence
                // TODO: Record somehow that message actual messageID later.
                //       Either by changing all protocol APIs to return it,
                //       or those APIs to edit the hdr (in/out param), or
                //       some other way.
                req_hdr.m_messageFlags &= ~etp::MsgFlags::eFINALPART; // reset
                req_hdr.m_correlationId = 0;
            } else if (correlation_id_ < 2) {
                req_hdr.m_messageFlags |= etp::MsgFlags::eFINALPART; // set
                req_hdr.m_correlationId = -correlation_id_;
            } else {
                req_hdr.m_messageFlags &= ~etp::MsgFlags::eFINALPART; // reset
                req_hdr.m_correlationId = correlation_id_;
            }
        }

        return ireq;
    }

    void sent() {
        clock_t::time_point now = clock_t::now();
        std::chrono::duration<double> sec = now - checkpoint_;
        encode_sec_ += sec.count();
        checkpoint_ = now;
    }

    void received() {
        clock_t::time_point now = clock_t::now();
        std::chrono::duration<double> sec = now - checkpoint_;
        waited_sec_ += sec.count();
    }

private:
    clock_t::time_point checkpoint_;
    int64_t correlation_id_ = 0;
};

//============================================================================

template <typename PROTO, typename REQ, typename RES>
struct RetRPC : BaseRPC {
    using REQ_t = REQ;
    using RES_t = RES;
    using MSG_t = void (PROTO::*)(
        IRequest& ireq, REQ req,
        EtpResponseHandler<RES> process_cb
    );

    PROTO* proto_;
    MSG_t msg_;

    REQ req_;
    RES res_;

    using RcRes = std::pair<EtpResponseStatus, RES>;
    oes::common::SpscQueue<RcRes> res_queue_;

    RetRPC(
        PROTO* proto, MSG_t msg,
        size_t max_queue_depth = 10
    ) : proto_(proto)
      , msg_(msg)
      , res_queue_(max_queue_depth, std::chrono::seconds(3))
    {
        assert(proto);
    }

    void async_send(REQ req, std::chrono::milliseconds timeout) {
        RequestHeader ireq = encoding(REQ::info.type_);

        // reset res, just in case
        res_ = RES();

        res_queue_.timeout_ = timeout;

        (proto_->*msg_)(
            ireq, std::move(req),
            [this](EtpResponseStatus rc, RES res) {
                /*const bool timed_out =*/ res_queue_.enqueue(
                    RcRes(std::move(rc), std::move(res))
                );
                // FIXME: What to do on timeout?
            }
        );

        sent();
    }

    std::pair<bool, std::future_status> recv(
        std::ostream& err,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(-1)
    ) {
        RcRes rc_res;
        bool deq_ok = timeout.count() > 0 ?
            res_queue_.dequeue(rc_res, timeout) :
            res_queue_.dequeue(rc_res);
        if (!deq_ok) {
            err << REQ::info.label_ << " timed-out" << std::endl;
            return { false, std::future_status::timeout };
        }

        received();

        rc_ = std::move(rc_res.first);
        res_ = std::move(rc_res.second);

        if (rc_.ok) {
            return { true, std::future_status::ready };
        }

        err << REQ::info.label_ << " failed: ";
        const ErrorInfo* err_info =
            !rc_.ex.m_error? nullptr: &*rc_.ex.m_error
        ;
        if (!err_info && !rc_.ex.m_errors.empty()) {
            err_info = &(rc_.ex.m_errors.begin()->second);
        }
        if (!err_info) {
            err << "Unknown ETP error" << std::endl;
        } else {
            err << "ETP#" << err_info->m_code << ": " << err_info->m_message << std::endl;
        }
        return { false, std::future_status::ready };
    }

    std::pair<bool, std::future_status> call(
        std::ostream& err, std::chrono::milliseconds timeout
    ) {
        try {
            async_send(req_, timeout); // copies req_ by default.
        } catch (const ClientException& e) {
            if (e.bad_connection) {
                return std::make_pair(false, std::future_status::ready);
            }

            throw std::runtime_error(e.what());
        }
        return recv(err);
    }

    std::pair<bool, std::future_status> call_consuming_req(
        std::ostream& err, std::chrono::milliseconds timeout
    ) {
        async_send(std::move(req_), timeout); // moves req_, so "consumed"
        req_ = REQ(); // in case moved-from state is invalid
        return recv(err);
    }

    // TODO: enable_if ONLY when RES is multipart
    std::pair<bool, std::future_status> recv_more(std::ostream& err) {
        // Similar to async_send(), w/o the sending part
        assert(rc_.final_part == false);

        reset();

        // reset res, just in case
        res_ = RES();

        return recv(err);
    }

    template <typename F>
    bool callForEachRange(
        std::ostream& err, std::chrono::milliseconds timeout, F&& func
    ) {
        bool all_ok{true};
        for (
            auto ok = call(err, timeout);
            ;    ok = recv_more(err)
        ) {
            if (!ok.first) {
                if (!rc_.final_part) {
                    all_ok = false;
                    continue;
                }
                return false;
            }

            auto& col = res_.range();
            if (col.empty()) {
                assert(rc_.final_part);
                break;
            }

            func(col); // throw on error

            if (rc_.final_part) {
                break;
            }
        }

        return all_ok;
    }

    template <typename F>
    bool callForEachValue(
        std::ostream& err, std::chrono::milliseconds timeout, F&& func
    ) {
        return callForEachRange(
            err, timeout, [&func](auto& col) {
                for (auto& val : col) {
                    func(val); // throw on error
                }
            }
        );
    }
};

//============================================================================

template <typename PROTO, typename REQ>
struct VoidRPC : BaseRPC {
    using REQ_t = REQ;
    using MSG_t = void (PROTO::*)(
        IRequest& ireq, REQ req,
        EtpCompleteHandler done_cb
    );

    std::promise<bool> ready_;

    PROTO* proto_;
    MSG_t msg_;

    REQ req_;

    VoidRPC(
        PROTO* proto, MSG_t msg
    ) : proto_(proto)
      , msg_(msg)
    {
        assert(proto);
    }

    void async_send(REQ req) {
        RequestHeader ireq = encoding(
            REQ::info.type_,
            etp::MsgFlags::eACKNOWLEDGE_RECEIPT
        );

        (proto_->*msg_)(
            ireq, std::move(req_),
            [this](EtpResponseStatus rc) {
                this->rc_ = std::move(rc);
                ready_.set_value(true);
            }
        );

        sent();
    }

    std::pair<bool, std::future_status> recv(
        std::ostream& err, std::chrono::milliseconds timeout
    ) {
        // FIXME: Waiting on a void call is problematic in ETP. We set the
        // ACKNOWLEDGE flag, but it's often sent (if at all...) before the
        // message is processed server-side, i.e. when just "received".
        std::future_status status = ready_.get_future().wait_for(timeout);

        received();

        switch (status) {
        case std::future_status::ready:
            if (rc_.ok) {
                return { true, status };
            } else {
                err << "Error: " << REQ::info.label_ << " failed: ";
                const ErrorInfo* err_info =
                    !rc_.ex.m_error? nullptr: &*rc_.ex.m_error
                ;
                if (!err_info && !rc_.ex.m_errors.empty()) {
                    err_info = &(rc_.ex.m_errors.begin()->second);
                }
                if (!err_info) {
                    err << "Unknown ETP error" << std::endl;
                } else {
                    err << "ETP#" << err_info->m_code << ": " << err_info->m_message << std::endl;
                }
                return { false, status };
            }

        case std::future_status::timeout:
            err << "Error: " << REQ::info.label_ << " timed-out" << std::endl;
            return { false, status };

        case std::future_status::deferred:
            err << "Error: " << REQ::info.label_ << " deferred" << std::endl;
            return { false, status };
        }

        throw std::logic_error("Unreachable");
    }

    std::pair<bool, std::future_status> call(
        std::ostream& err, std::chrono::milliseconds timeout
    ) {
        async_send(req_); // copies req_ by default.
        return recv(err, timeout);
    }

    std::pair<bool, std::future_status> call_consuming_req(
        std::ostream& err, std::chrono::milliseconds timeout
    ) {
        async_send(std::move(req_)); // moves req_, so "consumed"
        req_ = REQ(); // in case moved-from state is invalid
        return recv(err, timeout);
    }
};

//============================================================================

template <typename PROTO, typename REQ, typename RES>
auto rpcFor(
    PROTO* proto,
    void (PROTO::*msg)(
        IRequest& ireq, REQ req,
        EtpResponseHandler<RES> process_cb
    )
) {
    return RetRPC<PROTO, REQ, RES>(proto, msg);
}

template <typename PROTO, typename REQ>
auto rpcFor(
    PROTO* proto,
    void (PROTO::*msg)(
        IRequest& ireq, REQ req,
        EtpCompleteHandler done_cb
    )
) {
    return VoidRPC<PROTO, REQ>(proto, msg);
}

//============================================================================

} // namespace oes::client

#endif
