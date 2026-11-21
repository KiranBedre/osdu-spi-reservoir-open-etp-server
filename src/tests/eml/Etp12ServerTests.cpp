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

#include <oes/eml/etp12/Server.hpp>
#include <oes/eml/openkv/ServerConfig.h>
#include <oes/core/http/HTTP.h>
#include <oes/common/auth/AuthN.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/core/TestMain.h>

#include <thread>
#include <future>
#include <atomic>

using namespace oes::eml::etp12;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

namespace {

BOOST_FIXTURE_TEST_SUITE(Etp12ServerTests, OpenKVClientServerFixture)

BOOST_AUTO_TEST_CASE(test_unexpected_exception_is_sanitized) {
    EtpResponseStatus response;
    EtpCompleteHandler responseHandler = [&response](EtpResponseStatus result) {
        response = std::move(result);
    };

    try {
        throw std::runtime_error("sensitive backend details");
    } catch (...) {
        handleException(responseHandler);
    }

    BOOST_REQUIRE(response.ex.m_error);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_code, EINVALID_STATE);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_message, "Server error");
    BOOST_CHECK_EQUAL(response.ex.m_error->m_message.find("sensitive backend details"), std::string::npos);

    EtpResponseHandler<Core::OpenSession> typedResponseHandler = [&response](EtpResponseStatus result, Core::OpenSession) {
        response = std::move(result);
    };

    try {
        throw std::runtime_error("different sensitive details");
    } catch (...) {
        handleException(typedResponseHandler);
    }

    BOOST_REQUIRE(response.ex.m_error);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_code, EINVALID_STATE);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_message, "Server error");
    BOOST_CHECK_EQUAL(response.ex.m_error->m_message.find("different sensitive details"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_unknown_exception_is_sanitized) {
    EtpResponseStatus response;
    EtpCompleteHandler responseHandler = [&response](EtpResponseStatus result) {
        response = std::move(result);
    };

    try {
        throw 42;
    } catch (...) {
        handleException(responseHandler);
    }

    BOOST_REQUIRE(response.ex.m_error);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_code, EINVALID_STATE);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_message, "Server Error: Unknown Exception.");

    EtpResponseHandler<Core::OpenSession> typedResponseHandler = [&response](EtpResponseStatus result, Core::OpenSession) {
        response = std::move(result);
    };

    try {
        throw 42;
    } catch (...) {
        handleException(typedResponseHandler);
    }

    BOOST_REQUIRE(response.ex.m_error);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_code, EINVALID_STATE);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_message, "Server Error: Unknown Exception.");
}

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

class ServiceAuthorizationStub {
public:
    explicit ServiceAuthorizationStub(websocketpp::http::status_code::value status)
        : status_(status) {
        server_.clear_access_channels(websocketpp::log::alevel::all);
        server_.clear_error_channels(websocketpp::log::elevel::all);
        server_.init_asio();
        server_.set_http_handler([this](websocketpp::connection_hdl hdl) {
            const auto conn = server_.get_con_from_hdl(hdl);
            ++requests_;
            const bool valid_request = conn->get_resource() == "/service"
                && conn->get_request_header("Authorization") == "Bearer service-token";
            conn->set_status(valid_request ? status_ : websocketpp::http::status_code::forbidden);
            conn->set_body("sentinel internal authorization details");
        });
        server_.listen(websocketpp::lib::asio::ip::tcp::endpoint(
            websocketpp::lib::asio::ip::address_v4::loopback(), 0));
        websocketpp::lib::asio::error_code ec;
        const auto endpoint = server_.get_local_endpoint(ec);
        BOOST_REQUIRE_MESSAGE(!ec, ec.message());
        url_ = fmt::format("http://127.0.0.1:{}", endpoint.port());
        server_.start_accept();
        thread_ = std::thread([this]() { server_.run(); });
    }

    ~ServiceAuthorizationStub() {
        websocketpp::lib::asio::error_code ec;
        server_.stop_listening(ec);
        server_.stop();
        thread_.join();
    }

    std::string url_;
    std::atomic<unsigned int> requests_ { 0 };

private:
    websocketpp::server<websocketpp::config::asio> server_;
    websocketpp::http::status_code::value status_;
    std::thread thread_;
};

struct RefreshPartitionsFixture : OpenKVClientServerFixture {
    std::string delegate_url_;
    bool no_auth_ = false;

    void initServer() override {
        // HTTP authorization must be decided before any database access.
        init_done_ = true;
    }

    void configureServer() override {
        if (no_auth_) {
            auto no_auth_config = oes::eml::openkv::makeServerConfig(
                connection_pool_,
                [](const oes::common::auth::AuthInfo&, oes::common::auth::AuthDetails*) {
                    oes::common::auth::AuthenticatedUser user;
                    user.ok_ = true;
                    user.read_only_ = false;
                    return user;
                },
                "", nullptr, {}, nullptr, test_config_);
            svr_conf_.session_factory_ = std::move(no_auth_config.session_factory_);
            svr_conf_.user_factory_ = std::move(no_auth_config.user_factory_);
        }
        svr_conf_.authn_delegate_url_ = delegate_url_;
        svr_conf_.stopOnLastDisconnection_ = false;
        svr_conf_.useIpV4_ = true;
    }

    oes::core::HTTP::Response refresh(const std::string& authorization) {
        oes::core::HTTP http;
        oes::core::HTTP::Headers headers;
        if (!authorization.empty()) {
            headers.push_back("Authorization: " + authorization);
        }
        oes::core::HTTP::Options options;
        options.timeout = 5;
        return http.get(fmt::format("http://127.0.0.1:{}/refresh-partitions", svr_port_),
                        0, headers, options);
    }
};

BOOST_FIXTURE_TEST_SUITE(RefreshPartitionsAuthorizationTests, RefreshPartitionsFixture)

BOOST_AUTO_TEST_CASE(builtin_auth_cannot_refresh_partitions) {
    startServer();
    const auto response = refresh("Basic Zm9vOmJhcg==");
    BOOST_CHECK_EQUAL(response.code, 403);
    BOOST_CHECK_EQUAL(response.body, "Forbidden.");
}

BOOST_AUTO_TEST_CASE(disabled_auth_cannot_refresh_partitions) {
    no_auth_ = true;
    startServer();
    const auto anonymous_response = refresh("");
    BOOST_CHECK_EQUAL(anonymous_response.code, 403);
    BOOST_CHECK_EQUAL(anonymous_response.body, "Forbidden.");
    const auto spoofed_response = refresh("Bearer service-token");
    BOOST_CHECK_EQUAL(spoofed_response.code, 403);
    BOOST_CHECK_EQUAL(spoofed_response.body, "Forbidden.");
}

BOOST_AUTO_TEST_CASE(missing_credentials_never_reach_delegate) {
    ServiceAuthorizationStub delegate(websocketpp::http::status_code::ok);
    delegate_url_ = delegate.url_;
    startServer();
    const auto response = refresh("");
    BOOST_CHECK_EQUAL(response.code, 403);
    BOOST_CHECK_EQUAL(response.body, "Forbidden.");
    BOOST_CHECK_EQUAL(delegate.requests_.load(), 0);
}

BOOST_AUTO_TEST_CASE(delegate_denial_and_errors_fail_closed) {
    for (const auto status : {websocketpp::http::status_code::forbidden,
                              websocketpp::http::status_code::service_unavailable}) {
        ServiceAuthorizationStub delegate(status);
        delegate_url_ = delegate.url_;
        startServer();
        const auto response = refresh("Bearer service-token");
        BOOST_CHECK_EQUAL(response.code, 403);
        BOOST_CHECK_EQUAL(response.body, "Forbidden.");
        BOOST_CHECK_EQUAL(response.body.find("sentinel"), std::string::npos);
        BOOST_CHECK_EQUAL(delegate.requests_.load(), 1);
        stopServer();
        svr_rc_ = std::promise<bool>();
    }
}

BOOST_AUTO_TEST_CASE(valid_service_credentials_pass_authorization) {
    ServiceAuthorizationStub delegate(websocketpp::http::status_code::ok);
    delegate_url_ = delegate.url_;
    startServer();
    const auto response = refresh("Bearer service-token");
    // Single-partition mode rejects refresh after the service authorization succeeds.
    BOOST_CHECK_EQUAL(response.code, 400);
    BOOST_CHECK_EQUAL(response.body, "Server not running in multiple partition mode.");
    BOOST_CHECK_EQUAL(delegate.requests_.load(), 1);
}

BOOST_AUTO_TEST_CASE(invalid_credentials_cannot_refresh_partitions) {
    ServiceAuthorizationStub delegate(websocketpp::http::status_code::ok);
    delegate_url_ = delegate.url_;
    startServer();
    const auto response = refresh("Bearer invalid-token");
    BOOST_CHECK_EQUAL(response.code, 403);
    BOOST_CHECK_EQUAL(response.body, "Forbidden.");
    BOOST_CHECK_EQUAL(delegate.requests_.load(), 1);
}

BOOST_AUTO_TEST_CASE(unavailable_delegate_cannot_refresh_partitions) {
    websocketpp::lib::asio::io_service io;
    websocketpp::lib::asio::ip::tcp::socket reserved_port(io);
    reserved_port.open(websocketpp::lib::asio::ip::tcp::v4());
    reserved_port.bind(websocketpp::lib::asio::ip::tcp::endpoint(
        websocketpp::lib::asio::ip::address_v4::loopback(), 0));
    delegate_url_ = fmt::format("http://127.0.0.1:{}", reserved_port.local_endpoint().port());
    startServer();
    const auto response = refresh("Bearer service-token");
    BOOST_CHECK_EQUAL(response.code, 403);
    BOOST_CHECK_EQUAL(response.body, "Forbidden.");
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
