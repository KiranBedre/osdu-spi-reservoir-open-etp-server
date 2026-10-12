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

#ifndef OES_EML_ETP12_SERVER_H
#define OES_EML_ETP12_SERVER_H

#include <oes/eml/fwd.h>
#include <oes/eml/export.h>
#include <oes/eml/OpenETPServerConfig.h>
#include <oes/core/utils/Otel.h>

#include <oes/common/etp12/Protocols.h>
#include <oes/common/etp12/ErrorCodes.h>

#include <future>

namespace oes::postgresql{
    class DbConnectionPool;
};

namespace oes::eml::etp12 {

//============================================================================

namespace data12 = ::oes::common::etp12::Datatypes;
namespace proto12 = ::oes::common::etp12::Protocol;
namespace auth = ::oes::common::auth;
namespace Datatypes = ::oes::common::etp12::Datatypes;

enum {
    ETP_SERVER_DEFAULT_PORT = 9002
};

constexpr const char* APPLICATION_VERSION = "0.29.2";

class Impl;
using Impl_ptr  = std::shared_ptr<Impl>;
using Impl_wptr = std::weak_ptr  <Impl>;

class Session;
using Session_ptr  = std::shared_ptr<Session>;
using Session_wptr = std::weak_ptr  <Session>;
using Session_uptr = std::unique_ptr<Session>;

struct Handshake;
using RequestHeaderGetter = std::function<
    std::string(std::string)
>;
using SessionFactory = std::function<
    std::shared_ptr<Session>(Handshake, RequestHeaderGetter)
>;
using UserFactory = std::function<
    auth::AuthenticatedUser_ptr(Handshake)
>;
using EntitlementFactory = std::function<
    auth::Entitlement_ptr(Handshake, auth::AuthenticatedUser_ptr)
>;
using ProtocolFactory = std::function<
    proto12::IProtocol_ptr(const data12::SupportedProtocol&, std::weak_ptr<Session>)
>;

using data12::ErrorInfo;
using data12::ErrorCodes;
using proto12::EtpResponseStatus;
using proto12::EtpCompleteHandler;

//============================================================================

struct OES_EML_EXPORT Handshake {
    // Generic info from WebSocket
    bool        secure_ = false;
    std::string host_;
    uint16_t    port_ = 0;
    std::string resource_; // i.e. URI's path[?query][#fragment]
    std::string remote_endpoint_;
    std::string origin_;

    // ETP-specific info
    std::string client_id_;
    std::string session_id_;
    std::string etp_encoding_;
    std::string authorization_;
    std::string jwt_secret_;

    // OSDU-specific info
    std::string data_partition_id_;
    std::string authorization_mode_; // group or policy
};

//============================================================================

class OES_EML_EXPORT Session : public std::enable_shared_from_this<Session> {
public:
    Handshake handshake_;
    auth::AuthenticatedUser_ptr user_;
    auth::Entitlement_ptr entitlement_;
    bool authZ_enabled_ {false};
    std::string client_identification_;
    bool check_activities_ {false};

public:
    Session(Handshake handshake,
        auth::AuthenticatedUser_ptr user,
        auth::Entitlement_ptr entitlement
    );
    virtual ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    int id() const { return id_; }
    std::string uuid() const { return handshake_.session_id_; }
    const std::string& label() const { return label_; }

    size_t max_batch_size() const {
        return max_batch_size_;
    }
    void set_max_batch_size(size_t count) {
        max_batch_size_ = count; // For testing only
    }

    size_t negotiated_max_msg_size() const {
        return negotiated_max_msg_size_;
    }
    void set_negotiated_max_msg_size(size_t bytes) {
        negotiated_max_msg_size_ = bytes; // For testing only
    }

    size_t threads_count(){
        return threads_count_;
    };

    bool isOpen() const;
    bool isClosed() const;
    bool boundToServer() const;
    void close(std::string reason, bool from_ws);

    virtual void onStart(proto12::Core::RequestSession req,
                         proto12::Core::OpenSession& res);
    virtual void onResume(proto12::CoreOSDU::ResumeSession req,
                          proto12::CoreOSDU::ResumeSessionResponse& res);
    virtual void onAuthN(std::string token);
    virtual void onClose(std::string reason, bool from_ws);

    virtual const bool hasTransaction() const {
        return false;
    }

    const std::string& userName() const;
    const std::string& userMail() const;

    struct State;
    using State_ptr = std::shared_ptr<State>;
    using State_wptr = std::weak_ptr<State>;
    State_ptr getState();

private:
    friend Impl;

    State_ptr state_;

    int id_;
    std::string label_;
    size_t max_batch_size_ = 1'000'000;
    size_t negotiated_max_msg_size_ = 2'000'000'000;
    size_t threads_count_ = 1;
    std::string negotiated_compression_;
    std::shared_ptr<oes::postgresql::DbConnectionPool> db_connection_pool_;
    std::vector<Datatypes::SupportedProtocol> adjustedProtocols_;
    oes::core::OtelSpan otel_parent_span_;

protected:
    virtual void resumeFromExistingSession(std::shared_ptr<Session> other);
};

//============================================================================

struct OES_EML_EXPORT ServerConfig {
    int port_ = ETP_SERVER_DEFAULT_PORT;

    // Will stop listening on last disconnection, shutting down the server
    bool stopOnLastDisconnection_ = false;

    // Warning: If true this will allow other program to listen
    bool reuseAddressSocket_ = false;

    bool useIpV4_ = false;

    int maxNumberConnection_ = -1;

    // single or multiple data partition mode
    std::string dataPartitionMode_;
    // standalone or osdu data connectivity mode
    std::string dataConnectivityMode_;

    unsigned int totalThreadCount_ = 1;
    bool enable_any_number_of_threads_ = false;

    std::chrono::seconds session_keep_alive_duration_ = std::chrono::seconds(2*60);

    size_t max_mgs_size_  = 2'000'000'000;  // Defaults to 2GB (not 2GiB)
    size_t max_mgs_usize_ = 2'000'000'000;  // WARNING: Will allocate that much!
    size_t max_batch_size_ = 1'000'000;     // Warning: NOT ETP standard

    bool enable_tracing_ = false;
    std::string trace_file_;

    bool check_activities_ = false;

    enum Flavor { WebSocketPP, BoostBeast };
    Flavor flavor_ = WebSocketPP;

    SessionFactory session_factory_;
    UserFactory user_factory_;
    std::string authn_delegate_url_;
    EntitlementFactory entitlement_factory_;
    ProtocolFactory protocol_factory_;
    data12::ServerCapabilities server_capabilities_;
    std::vector<Datatypes::Protocol> supportedProtocols_;
    std::shared_ptr<oes::postgresql::DbConnectionPool> db_connection_pool_;
    oes::eml::openETPServerConfig_ptr server_config_;
};

//============================================================================

class OES_EML_EXPORT Server {
public:
    explicit Server(ServerConfig config);
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool start();
    bool isRunning() const;

    // APIs for testing mostly
    void stopListening();
    std::future<int> getRandomServerPort();

    data12::ServerCapabilities getServerInfo() const;

private:
    ServerConfig config_;
    Impl_ptr impl_;
    bool running_;
};

//============================================================================

struct OES_EML_EXPORT Todo : std::runtime_error {
    Todo();
};

OES_EML_EXPORT
EtpResponseStatus errorStatus(ErrorCodes errc, std::string msg);

OES_EML_EXPORT
bool addErrorStatus(EtpResponseStatus& rc, const std::string& key, ErrorInfo err);

OES_EML_EXPORT
bool addErrorStatus(
    EtpResponseStatus& rc, const std::string& key, ErrorCodes errc, std::string msg
);

OES_EML_EXPORT
void handleException(EtpCompleteHandler& res_handler);

//============================================================================

} // namespace oes::eml::etp12

#endif
