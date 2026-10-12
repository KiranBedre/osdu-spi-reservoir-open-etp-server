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

#ifndef OES_CLIENT_H
#define OES_CLIENT_H

#include "export.h"

#include <oes/common/etp12/Protocols.h>
#include <oes/common/etp12/ErrorCodes.h>

#include <chrono>
#include <stdexcept>

namespace oes::client {

//============================================================================

namespace data12 = oes::common::etp12::Datatypes;
namespace proto12 = oes::common::etp12::Protocol;

class ClientImpl;
using ClientImpl_ptr  = std::shared_ptr<ClientImpl>;
using ClientImpl_wptr = std::weak_ptr  <ClientImpl>;

//============================================================================

enum class LoggerLevel {
    Debug,
    Info,
    Warning,
    Error
};

OES_CLIENT_EXPORT
void setLoggerLevel(const LoggerLevel& level);

//============================================================================

class OES_CLIENT_EXPORT ClientException: public std::runtime_error {
public:
    const bool bad_connection;
    explicit ClientException(const char* msg, bool bad_conn);
    ~ClientException() throw() override {}
};

//============================================================================

struct OES_CLIENT_EXPORT ClientConfig {
    std::string server_url_;            // e.g. ws://localhost:9002
    std::string auth_hdr_;              // Authentication HTTP header (i.e. Basic or Bearer JWT)
    std::string data_partition_id_hdr_; // OSDU header for data-partition support
    std::string client_id_;             // Typically a Guid

    bool avro_binary_ = true;   // Avro JSON is deprecated in ETP 1.2

    // Whether to request the server to acknowledge reception of all client request.
    bool acknowledge_ = false;

    // Whether client requests will be compressed or not.
    // And which compressors are supported.
    bool compress_ = false;
    std::vector<std::string> zcompressors_{ "gzip" }; // TODO: lz4, zstd

    // When true, uses server URL query-string parameters for
    // auth_hdr_, data_partition_id_hdr_, client_id_, and avro_binary_ rather than HTTP headers.
    bool emulate_browser_client_ = false;

    // The maximum size in bytes this client prefers messages to be.
    // The server value may be smaller, and the less of the two will be used.
    // \sa Client::negotiated_max_msg_size()
    size_t max_mgs_size_ = size_t(2e9); // defaults to 2GB (not 2GiB)

    // Which protocols the client will need during its session.
    // The server may not support all of those.
    std::vector<data12::Protocol> requested_protocols_{
        data12::Protocol::eCore,
        data12::Protocol::eDataspace,
        data12::Protocol::eDiscovery,
        data12::Protocol::eSupportedTypes,
        data12::Protocol::eStore,
        data12::Protocol::eStoreOSDU,
        data12::Protocol::eDataArray,
        data12::Protocol::eTransaction,
        data12::Protocol::eDataspaceOSDU,
    };

    /*
    ** TODO
    bool use_tls_;
    std::string tls_certificate_;
    */

    bool enable_tracing_ = false;
    std::string trace_file_; // or folder (named about PID and date/time then)

    void withBasicAuth(
        const std::string& username, const std::string& password
    );
    bool withJwtAuth(
        const std::string& username, const std::string& password,
        const std::string& jwtSecret // used to crypto-sign the resulting token
    );
    void withJwtAuth(const std::string& jwtToken);
};

//============================================================================

class OES_CLIENT_EXPORT Client {
public:
    explicit Client(ClientConfig config);
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    bool connect(
        std::chrono::milliseconds timeout, data12::ErrorInfo* p_info = nullptr
    );
    bool reconnect(std::chrono::milliseconds timeout, data12::ErrorInfo* p_info = nullptr);

    bool isConnOpen() const;

    bool close(std::string& failure, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
    ClientConfig getConfig() const;

    std::vector<data12::Protocol> requested_protocols() const;
    std::vector<data12::Protocol> negotiated_protocols() const;

    size_t negotiated_max_msg_size() const {
        return negotiated_max_msg_size_;
    }

    proto12::IProtocol* getProtocol(data12::Protocol proto_id);

    template <typename TProto>
    TProto* get_proto() {
        auto proto_id = static_cast<data12::Protocol>(TProto::protocolId);
        proto12::IProtocol* iproto = getProtocol(proto_id);
        return dynamic_cast<TProto*>(iproto);
    }

    auto core_proto() { return get_proto<proto12::ICore>(); }

    auto store_proto() { return get_proto<proto12::IStore>(); }
    auto array_proto() { return get_proto<proto12::IDataArray>(); }
    auto disco_proto() { return get_proto<proto12::IDiscovery>(); }
    auto types_proto() { return get_proto<proto12::ISupportedTypes>(); }
    auto space_proto() { return get_proto<proto12::IDataspace>(); }
    auto trans_proto() { return get_proto<proto12::ITransaction>(); }

    auto store_osdu_proto() { return get_proto<proto12::IStoreOSDU>(); }
    auto space_osdu_proto() { return get_proto<proto12::IDataspaceOSDU>(); }


    bool force_disconnection();
private:
    ClientConfig config_;
    ClientImpl_ptr impl_;
    size_t negotiated_max_msg_size_ = 0;

    std::array<uint8_t, 16> session_uuid_;
    bool can_reconnect_ = false;
    std::vector<data12::Protocol> supported_proto_ids_;

    void reset_impl();
};

//============================================================================

class OES_CLIENT_EXPORT ScopedTransaction {
public:
    ScopedTransaction(
        oes::client::Client& client,
        std::chrono::milliseconds timeout
    );
    ~ScopedTransaction();

    ScopedTransaction(ScopedTransaction&&) = default;
    ScopedTransaction& operator=(ScopedTransaction&&) = default;

    ScopedTransaction(const ScopedTransaction&) = delete;
    ScopedTransaction& operator=(const ScopedTransaction&) = delete;

    void replace_proto(oes::client::Client& client);
    bool supported() const { return tx_proto_; }
    bool started() const { return started_; }

    bool start(
        std::string etp_space, std::string message, bool read_only,
        std::string* failure_reason = nullptr,
        const unsigned short retries = 1000,
        std::chrono::milliseconds retry_interval = std::chrono::milliseconds(400),
        double factor = 1.5,
        std::chrono::milliseconds max_interval = std::chrono::milliseconds(2000)
    );
    bool commit(std::string* failure_reason = nullptr);
    bool rollback(std::string* failure_reason = nullptr);

private:
    template <typename TRpc>
    bool call(TRpc& rpc, std::string* failure_reason);

private:
    proto12::ITransaction* tx_proto_;
    std::chrono::milliseconds timeout_;
    std::array<uint8_t, 16> tx_uid_;
    bool started_ = false;
};

//============================================================================

} // namespace oes::client

#endif
