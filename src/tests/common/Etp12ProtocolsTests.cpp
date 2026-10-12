// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/common/etp12/Protocols.h>
#include <oes/common/etp12/Metadata.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <set>

using namespace oes::common::etp12;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

namespace {

struct RequestHeader final : IRequest {
    MessageHeader head_;

    const MessageHeader& header() const override { return head_; }
    AbstractMessage_uptr takeBody() override { return nullptr; }
};

struct MockDiscoSyncImpl : IDiscoveryQuery {
    void FindResources(
        IRequest& ireq, FindResources_req_t req,
        EtpResponseHandler<FindResources_res_t> done_cb
    ) override {
        BOOST_REQUIRE(typeOf(ireq) == req.info.type_);

        DiscoveryQuery::FindResourcesResponse res;
        Object::Resource oneRes;
        oneRes.m_name = "Dummy";
        oneRes.m_uri = req.m_context.m_uri;
        res.m_resources.push_back(oneRes);

        EtpResponseStatus rc;
        rc.ok = true;
        done_cb(rc, res);
    }
};

BOOST_AUTO_TEST_SUITE(Etp12ProtocolsTests)

BOOST_AUTO_TEST_CASE(test_builds) {
    RequestHeader ireq;
    MessageHeader& req_hdr = ireq.head_;
    DiscoveryQuery::FindResources req;
    req.m_context.m_uri = "eml://myDS/myPRJ/resqml20/MyType(Guid)";

    req_hdr.m_protocol = IDiscoveryQuery::protocolId;
    req_hdr.m_messageType = req.info.type_.type_;
    req_hdr.m_correlationId = 42;
    req_hdr.m_messageId = 1001;
    req_hdr.m_messageFlags = 0x10; // eAcknowledgeRequested

    MockDiscoSyncImpl sync_impl;
    IDiscoveryQuery& iproto = sync_impl;

    DiscoveryQuery::FindResourcesResponse res;

    iproto.FindResources(
        ireq, req,
        [ &res](auto rc, auto response) {
            BOOST_REQUIRE(rc.ok);
            BOOST_REQUIRE(!rc.ex.m_error);
            res = std::move(response);
        }
    );

    BOOST_REQUIRE_EQUAL(size_t(1),res.m_resources.size());
    BOOST_CHECK_EQUAL(res.m_resources[0].m_name, "Dummy");
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml://myDS/myPRJ/resqml20/MyType(Guid)");
}

BOOST_AUTO_TEST_CASE(test_makeClientImpls) {
    using oes::common::etp12::Datatypes::Protocol;
    const std::set<Protocol> expected_nulls{
        Protocol::eDEPRECATED_8,
        Protocol::eRESERVED_10,
        Protocol::eRESERVED_11,
        Protocol::eRESERVED_12,
        Protocol::eRESERVED_15,
        Protocol::eRESERVED_17,
        Protocol::eRESERVED_19,
        Protocol::eRESERVED_20,
        Protocol::eRESERVED_23,
    };
    for (const auto& entry : oes::common::etp12::protocols()) {
        Protocol proto_id = static_cast<Protocol>(entry.first);
        IProtocol_uptr proto = IProtocol::makeClientImpl(proto_id, nullptr, nullptr);
        auto found = expected_nulls.find(proto_id);
        if (found != expected_nulls.end()) {
            BOOST_CHECK(proto == nullptr);
        } else {
            BOOST_REQUIRE(proto != nullptr);
            BOOST_CHECK_EQUAL(int(proto->id_), int(proto_id));
            BOOST_CHECK_EQUAL(proto->name_, entry.second.name_);
            BOOST_CHECK_EQUAL(proto->name_id_, entry.second.label());
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
