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

#include <oes/testing/eml/OpenKVClientServerFixture.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/core/TestMain.h>

#include <thread>
#include <future>

using namespace oes::eml::etp12;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

namespace {

BOOST_FIXTURE_TEST_SUITE(Etp12ServerTests, OpenKVClientServerFixture)

BOOST_AUTO_TEST_CASE(test_max_msg_size_negotiation) {
    svr_conf_.max_mgs_size_ = 30*1000*1000; // 30 MB

    startServer();

    // Connect to server, using BASIC Auth client
    {
        clt_conf_.max_mgs_size_ = 50*1024*1024; // 50 MiB
        clt_conf_.withBasicAuth("foo", "bar");

        auto clt = connectClient();

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);

        // server's value is smaller, so the negotiated one
        BOOST_CHECK_EQUAL(clt->negotiated_max_msg_size(), svr_conf_.max_mgs_size_);
    }

    // Connect to server, using JWT Auth client
    {
        clt_conf_.max_mgs_size_ = 15*1024*1024; // 15 MiB
        clt_conf_.withJwtAuth("foo", "bar", "victoria");

        auto clt = connectClient();

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);

        // client's value is smaller, so the negotiated one
        BOOST_CHECK_EQUAL(clt->negotiated_max_msg_size(), clt_conf_.max_mgs_size_);
    }
}

BOOST_AUTO_TEST_CASE(test_auth_via_query_string) {
    startServer();

    // Connect to server, using BASIC Auth client
    {
        clt_conf_.emulate_browser_client_ = true;
        clt_conf_.withBasicAuth("foo", "bar");

        auto clt = connectClient();

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);
    }

    // Connect to server, using JWT Auth client
    {
        clt_conf_.emulate_browser_client_ = true;
        clt_conf_.withJwtAuth("foo", "bar", "victoria");

        auto clt = connectClient();

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);
    }
}

BOOST_AUTO_TEST_CASE(test_session_survivability) {
    startServer();

    // Try resume session before cleanup service
    {
        clt_conf_.requested_protocols_ = {
            Datatypes::Protocol::eCoreOSDU
        };
        
        auto clt = connectClient();
        auto proto =  clt->getProtocol(Datatypes::Protocol::eCoreOSDU);
        BOOST_CHECK(proto);

        auto ok = clt->force_disconnection();
        BOOST_CHECK(ok);

        std::this_thread::sleep_for(std::chrono::seconds(3));
        ok = clt->reconnect(std::chrono::milliseconds(5000));
        BOOST_CHECK(ok);
    }
    
    // Try resume session after cleanup service
    {
        clt_conf_.requested_protocols_ = {
            Datatypes::Protocol::eCoreOSDU
        };
        
        auto clt = connectClient();
        auto proto =  clt->getProtocol(Datatypes::Protocol::eCoreOSDU);
        BOOST_CHECK(proto);

        auto ok = clt->force_disconnection();
        BOOST_CHECK(ok);

        std::this_thread::sleep_for(std::chrono::seconds(7));
        ok = clt->reconnect(std::chrono::milliseconds(5000));
        BOOST_CHECK_EQUAL(ok, false);
    }

    // Try resume session multiple times before cleanup service
    {
        clt_conf_.requested_protocols_ = {
            Datatypes::Protocol::eCoreOSDU
        };
        
        auto clt = connectClient();
        auto proto =  clt->getProtocol(Datatypes::Protocol::eCoreOSDU);
        BOOST_CHECK(proto);

        auto ok = clt->force_disconnection();
        BOOST_CHECK(ok);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        ok = clt->reconnect(std::chrono::milliseconds(5000));
        BOOST_CHECK(ok);

        // second time
        ok = clt->force_disconnection();
        BOOST_CHECK(ok);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        ok = clt->reconnect(std::chrono::milliseconds(5000));
        BOOST_CHECK(ok);

        // third time
        ok = clt->force_disconnection();
        BOOST_CHECK(ok);

        std::this_thread::sleep_for(std::chrono::seconds(1));
        ok = clt->reconnect(std::chrono::milliseconds(5000));
        BOOST_CHECK(ok);
    }

    // Test the session clean jobs concurrently
    {
        clt_conf_.requested_protocols_ = {
            Datatypes::Protocol::eCoreOSDU
        };
        
        std::vector<Client_uptr> clients;
        for (auto i = 0; i < 6; i++) {
            clients.push_back(std::move(connectClient()));

            auto proto = clients.back()->getProtocol(Datatypes::Protocol::eCoreOSDU);
            BOOST_CHECK(proto);
        }

        bool ok;
        const auto timeout = std::chrono::milliseconds(5000);
        for (auto& clt : clients) {
            ok = clt->force_disconnection();
            BOOST_CHECK(ok);
        }

        std::this_thread::sleep_for(std::chrono::seconds(2)); // ~3s left to expire

        for (auto i = 0; i < 2; i++) {
            ok = clients.at(i)->reconnect(timeout);
            BOOST_CHECK_EQUAL(ok, true);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1)); // ~2s left to expire

        ok = clients.at(2)->reconnect(timeout);
        BOOST_CHECK_EQUAL(ok, true);

        std::this_thread::sleep_for(std::chrono::seconds(4)); // Expired!

        for (auto i = 3; i < 5; i++) {
            ok = clients.at(i)->reconnect(timeout);
            BOOST_CHECK_EQUAL(ok, false);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1)); // Expired!

        ok = clients.at(5)->reconnect(timeout);
        BOOST_CHECK_EQUAL(ok, false);
    }

}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
