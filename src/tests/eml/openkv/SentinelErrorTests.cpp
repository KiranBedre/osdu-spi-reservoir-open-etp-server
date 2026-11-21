// ============================================================================
// Copyright (c) Microsoft Corporation.
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

#include <chrono>
#include <thread>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/common/auth/Entitlement.h>
#include <oes/osdu/api/EntitlementUtils.h>
#include <oes/testing/eml/ServerFixture.h>

#include <oes/common/etp12/EmlUri.h>
#include <oes/eml/openkv/SpaceOSDUProto.h>
#include <oes/eml/openkv/SpaceProto.h>

namespace {

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using namespace oes::common::etp12::Datatypes;
using namespace oes::common::etp12::Protocol;

const std::string sentinel = "SENTINEL_UPSTREAM_DETAIL";

class TestEntitlementConfig : public oes::osdu::api::EntitlementConfigBase {
public:
    TestEntitlementConfig() {
        authorization_mode_ = "osdu";
    }
};

class SentinelEntitlement : public oes::common::auth::Entitlement {
public:
    SentinelEntitlement()
        : Entitlement(oes::common::auth::EntitlementType::Policy) {
        config_.osdu_ = std::make_shared<TestEntitlementConfig>();
        error_.code_ = static_cast<short>(ErrorCodes::EINVALID_ARGUMENT);
        error_.status_ = sentinel;
    }

    const oes::osdu::api::EntitlementError& err() const override { return error_; }
    bool isValid() const override { return true; }
    std::string getMemberEmail() const override { return "test@example.com"; }

    bool hasServicePermissionCreate() override { return true; }
    bool hasServicePermissionRead() override { return true; }
    bool hasServicePermissionUpdate() override { return true; }
    bool hasServicePermissionDelete() override { return true; }

    bool canCreateDataspace(oes::osdu::api::EntitlementDataspace_ptr) override { return true; }
    bool canReadDataspace(oes::osdu::api::EntitlementDataspace_ptr) override { return true; }
    bool canUpdateDataspace(oes::osdu::api::EntitlementDataspace_ptr) override { return true; }
    bool canDeleteDataspace(oes::osdu::api::EntitlementDataspace_ptr) override { return true; }

    bool registerDataspace(oes::osdu::api::EntitlementDataspace_ptr) override {
        ++register_calls;
        return false;
    }

    bool unregisterDataspace(oes::osdu::api::EntitlementDataspace_ptr) override {
        ++unregister_calls;
        return false;
    }

    const oes::common::auth::EntitlementConfig& config() const override { return config_; }

    int register_calls = 0;
    int unregister_calls = 0;

private:
    oes::osdu::api::EntitlementError error_;
    oes::common::auth::EntitlementConfig config_;
};

class RejectingWebSocketEndpoint {
public:
    RejectingWebSocketEndpoint()
        : acceptor_(io_, tcp::endpoint(asio::ip::address_v4::loopback(), 0))
        , socket_(io_)
        , deadline_(io_)
        , response_(
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " + std::to_string(sentinel.size()) + "\r\n"
            "Connection: close\r\n\r\n" + sentinel) {
        port_ = acceptor_.local_endpoint().port();
        deadline_.expires_after(std::chrono::seconds(5));
        deadline_.async_wait([this](boost::system::error_code ec) {
            if (!ec) {
                fail("HTTP rejection deadline", asio::error::timed_out);
            } else if (ec != asio::error::operation_aborted) {
                fail("HTTP rejection timer", ec);
            }
        });
        acceptor_.async_accept(socket_, [this](boost::system::error_code ec) {
            if (ec) {
                return fail("accept", ec);
            }
            asio::async_read_until(socket_, request_, "\r\n\r\n",
                [this](boost::system::error_code read_ec, std::size_t) {
                    if (read_ec) {
                        return fail("read HTTP upgrade request", read_ec);
                    }
                    asio::async_write(socket_, asio::buffer(response_),
                        [this](boost::system::error_code write_ec, std::size_t) {
                            if (write_ec) {
                                return fail("write HTTP rejection", write_ec);
                            }
                            response_sent_ = true;
                            close();
                        });
                });
        });
        thread_ = std::thread([this] { io_.run(); });
    }

    ~RejectingWebSocketEndpoint() {
        stop();
    }

    void stop() {
        // All socket work is asynchronous; stopping the event loop also handles
        // early handler exits without waiting for an accept or HTTP handshake.
        io_.stop();
        wait();
    }

    void wait() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    unsigned short port() const {
        return port_;
    }

    bool responseSent() const {
        return response_sent_;
    }

    const std::string& error() const {
        return error_;
    }

private:
    void fail(const char* operation, boost::system::error_code ec) {
        if (error_.empty()) {
            error_ = std::string(operation) + ": " + ec.message() +
                " (" + ec.category().name() + ":" + std::to_string(ec.value()) + ")";
        }
        close();
    }

    void close() {
        boost::system::error_code ignored;
        deadline_.cancel(ignored);
        acceptor_.close(ignored);
        socket_.close(ignored);
    }

    asio::io_context io_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
    asio::steady_timer deadline_;
    asio::streambuf request_{8192};
    std::string response_;
    unsigned short port_ = 0;
    bool response_sent_ = false;
    std::string error_;
    std::thread thread_;
};

void checkGenericError(
    const oes::common::etp12::Protocol::EtpResponseStatus& response,
    const std::string& expected_message
) {
    BOOST_REQUIRE(response.ex.m_error);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_code, ErrorCodes::EINVALID_ARGUMENT);
    BOOST_CHECK_EQUAL(response.ex.m_error->m_message, expected_message);
    BOOST_CHECK(response.ex.m_error->m_message.find(sentinel) == std::string::npos);
}

BOOST_FIXTURE_TEST_SUITE(SentinelErrorTests, oes::testing::eml::OpenKVServerFixture)

BOOST_AUTO_TEST_CASE(put_dataspace_registration_failure_does_not_disclose_detail) {
    auto entitlement = std::make_shared<SentinelEntitlement>();
    session_->entitlement_ = entitlement;

    auto put_rpc = makeRpc(&oes::eml::openkv::OpenKVDataspace::PutDataspaces);
    const std::string path = "Sentinel/Register";
    auto& space = put_rpc.req_.m_dataspaces[path];
    space.m_uri = oes::common::etp12::EmlUri::make_space(path).to_string();
    space.m_path = path;

    BOOST_CHECK(!put_rpc.call());
    BOOST_CHECK_EQUAL(entitlement->register_calls, 1);
    BOOST_CHECK(put_rpc.res_.m_success.empty());
    BOOST_REQUIRE_EQUAL(put_rpc.rc_.ex.m_errors.size(), 1);
    const auto& error = put_rpc.rc_.ex.m_errors.begin()->second;
    BOOST_CHECK_EQUAL(error.m_code, ErrorCodes::EREQUEST_DENIED);
    BOOST_CHECK_EQUAL(error.m_message, "Could not register dataspace. Check authorization and ACL configuration.");
    BOOST_CHECK(error.m_message.find(sentinel) == std::string::npos);
}

BOOST_AUTO_TEST_CASE(delete_dataspace_unregistration_failure_does_not_disclose_detail) {
    const auto space = putSpace("Sentinel/Unregister");
    auto entitlement = std::make_shared<SentinelEntitlement>();
    session_->entitlement_ = entitlement;

    auto delete_rpc = makeRpc(&oes::eml::openkv::OpenKVDataspace::DeleteDataspaces);
    delete_rpc.req_.m_uris.emplace(space.m_path, space.m_uri);

    BOOST_CHECK(!delete_rpc.call());
    BOOST_CHECK_EQUAL(entitlement->unregister_calls, 1);
    BOOST_REQUIRE_EQUAL(delete_rpc.rc_.ex.m_errors.size(), 1);
    const auto& error = delete_rpc.rc_.ex.m_errors.begin()->second;
    BOOST_CHECK_EQUAL(error.m_code, ErrorCodes::EINVALID_ARGUMENT);
    BOOST_CHECK_EQUAL(error.m_message, "Could not unregister dataspace");
    BOOST_CHECK(error.m_message.find(sentinel) == std::string::npos);
    BOOST_CHECK(delete_rpc.res_.m_success.empty());
}

BOOST_AUTO_TEST_CASE(remote_copy_connection_failure_does_not_disclose_http_body) {
    const auto target = putSpace("Sentinel/RemoteCopyTarget");
    RejectingWebSocketEndpoint endpoint;

    auto copy_rpc = makeRpc(&oes::eml::openkv::OpenKVDataspaceOSDU::CopyDataspaceFromRemote);
    copy_rpc.req_.m_action = "trigger";
    copy_rpc.req_.m_dataspacePath = target.m_path;
    copy_rpc.req_.m_sourceServer.m_sourceServer =
        "ws://127.0.0.1:" + std::to_string(endpoint.port()) + "/";
    copy_rpc.req_.m_sourceServer.m_sourceDataspacePath = "Sentinel/RemoteSource";
    copy_rpc.req_.m_extraData.emplace("timeout_ms", "3000");

    BOOST_CHECK(!copy_rpc.call());
    endpoint.wait();
    BOOST_CHECK_MESSAGE(endpoint.error().empty(), endpoint.error());
    BOOST_CHECK_MESSAGE(endpoint.responseSent(), "Endpoint did not send the sentinel HTTP rejection");
    checkGenericError(copy_rpc.rc_, "Failed to connect to source server");
    BOOST_CHECK(copy_rpc.res_.m_tableData.empty());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
