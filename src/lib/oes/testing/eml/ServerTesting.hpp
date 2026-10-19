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

#ifndef OES_TESTING_EML_SERVERTESTING_HPP
#define OES_TESTING_EML_SERVERTESTING_HPP

#include <oes/common/etp12/Protocols.h>

#include <oes/eml/fwd.h>
#include <oes/eml/openkv/Repo.h>
#include <oes/testing/core/TestConfig.h>

#include <string>
#include <utility>

namespace oes::testing::eml {

using namespace oes::eml::etp12;
using namespace oes::eml;
namespace Protocol = oes::common::etp12::Protocol;
namespace Datatypes = oes::common::etp12::Datatypes;


//============================================================================

using Datatypes::MessageHeader;

using Protocol::IRequest;
using Protocol::EtpResponseStatus;
using Protocol::EtpResponseHandler;
using Protocol::EtpCompleteHandler;

//============================================================================

template <typename T>
MessageHeader headerOf() {
    MessageHeader req_hdr;
    req_hdr.m_protocol    = T::info.type_.proto_;
    req_hdr.m_messageType = T::info.type_.type_;
    return req_hdr;
}

//============================================================================

struct UserDataType {
    std::string label_;

    template <typename REQ>
    void init() {
        label_ = REQ::info.label_;
    }
};

//============================================================================

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

template <typename PROTO, typename REQ, typename RES, typename UDT>
struct TestRetRPC {
    using MSG = void (PROTO::*)(
        IRequest& ireq, REQ req,
        EtpResponseHandler<RES> process_cb
    );

    UDT udt_;
    RequestHeader ireq_;
    EtpResponseStatus rc_;
    MSG msg_;
    REQ req_;
    RES res_;

    std::weak_ptr<openkv::OpenKVSession> session_;
    openkv::DbConnectionPool_ptr dbConnectionPool_;

    TestRetRPC(openkv::DbConnectionPool_ptr dbConnectionPool, std::weak_ptr<openkv::OpenKVSession> session, MSG msg, UDT udt)
        : udt_(udt)
        , ireq_(requestOf<REQ>())
        , msg_(msg)
        , session_(session)
        , dbConnectionPool_(std::move(dbConnectionPool))
    {
        udt_.template init<REQ>();
    }

    bool call() {
        // The lifetime of `Proto` must be `per call` for immediately release used DB connection
        auto app_config = oes::testing::TestConfig::make();
        PROTO proto(std::make_shared<openkv::OpenKVRepo>(session_, dbConnectionPool_, app_config), session_);
        return call(proto);
    }

    bool call(PROTO& proto) { // cppcheck-suppress constParameter
        // reset rc and res, just in case
        rc_ = EtpResponseStatus();
        res_ = RES();

        (proto.*msg_)(
            ireq_, req_,
            [this](EtpResponseStatus rc, RES res) {
                this->rc_  = std::move(rc);
                this->res_ = std::move(res);
            }
        );

        return rc_.ok;
    }
};

template <typename PROTO, typename REQ, typename UDT>
struct TestVoidRPC {
    using MSG = void (PROTO::*)(
        IRequest& ireq, REQ req,
        EtpCompleteHandler done_cb
    );

    UDT udt_;
    RequestHeader ireq_;
    EtpResponseStatus rc_;
    MSG msg_;
    REQ req_;

    openkv::OpenKVSession& session_;
    openkv::DbConnectionPool_ptr dbConnectionPool_;

    TestVoidRPC(openkv::DbConnectionPool_ptr dbConnectionPool, openkv::OpenKVSession& session, MSG msg, UDT udt)
        : udt_(udt)
        , ireq_(requestOf<REQ>())
        , msg_(msg)
        , session_(session)
        , dbConnectionPool_(std::move(dbConnectionPool))
    {
        udt_.template init<REQ>();
    }

    bool call() {
        // reset rc, just in case
        rc_ = EtpResponseStatus();

        // The lifetime of `Proto` must be `per call` for immediately release used DB connection
        auto app_config = oes::testing::TestConfig::make();
        PROTO proto(std::make_shared<openkv::OpenKVRepo>(session_, dbConnectionPool_), session_, app_config);
        (proto.*msg_)(
            ireq_, req_,
            [this](EtpResponseStatus rc) {
                this->rc_  = std::move(rc);
            }
        );

        return rc_.ok;
    }
};

//============================================================================

} // namespace oes::testing::eml

#endif
