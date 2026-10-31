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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <variant>
#include <condition_variable>

// work-around for TLS issue on RH6
#ifdef __linux__
#define BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING
#endif

#include <oes/core/utils/config_enter_3rdparty.h>
//#  include <boost/asio/signal_set.hpp>
#  include <avro/Stream.hh>
#  include <avro/Encoder.hh>
#  include <avro/Decoder.hh>
#  include <boost/asio/ssl/context.hpp>
#  include <fmt/format.h>
#  include <websocketpp/client.hpp>
#  include <websocketpp/config/asio_client.hpp>
#  include <websocketpp/config/asio_no_tls_client.hpp>
#  include <websocketpp/config/debug_asio_no_tls.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Client.hpp"

#include <oes/common/etp12/MsgFlags.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/common/etp12/EncoderSender.hpp>
#include <oes/common/etp12/Metadata.h>
#include <oes/common/etp12/Tracer.h>
#include <oes/common/auth/AuthN.h>
#include <oes/common/AvroUtils.hpp>
#include <oes/common/CompressUtils.hpp>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThreadUtils.hpp>
#include <oes/core/utils/ThousandSep.h>


// Transform a compiler macro definition into a string
#define _DEF_VAL(str) #str
#define DEFINE_TO_STRING(str) _DEF_VAL(str)

using oes::core::Guid;
using oes::core::thisThreadId;
using oes::core::thou;

using namespace oes::common::etp12;
using Datatypes::MessageHeader;
using Datatypes::ServerCapabilities;
using Protocol::EtpCompleteHandler;
using Protocol::EtpResponseHandler;
using Protocol::EtpResponseStatus;
using Protocol::EtpWrappedResponse;
using Protocol::WrappedMessageEncoder;
using Protocol::Core::ProtocolException;
using Protocol::Core::Acknowledge;

using duration_milli_t = std::chrono::duration<double, std::milli>;

/*!
 * \file oes/eml/etp12/Client.h
 * \brief Energistics ETP 1.2 client.
 *
 * Hides aways all WebSocket and Avro related concerns, exposing
 * a pure and typesafe C++ interface to ETP messages and protocols.
 */

namespace {
    constexpr char etpSubProtocol[] = "etp12.energistics.org";

using SSLClient = websocketpp::client<websocketpp::config::asio_tls_client>;
using NoSSLClient = websocketpp::client<websocketpp::config::asio_client>;
using ws_conn_hdl_t = websocketpp::connection_hdl;

std::atomic_int session_id__ = ATOMIC_VAR_INIT(0);

const char* msgOpcodeAsString(websocketpp::frame::opcode::value code) {
    switch (code) {
    case websocketpp::frame::opcode::CONTINUATION:  return "CONTINUATION";
    case websocketpp::frame::opcode::TEXT  :        return "TXT";
    case websocketpp::frame::opcode::BINARY:        return "BIN";
    case websocketpp::frame::opcode::CLOSE:         return "CLOSE";
    case websocketpp::frame::opcode::PING:          return "PING";
    case websocketpp::frame::opcode::PONG:          return "PONG";
    default:                                        return "???";
    }
}

using context_ptr = std::shared_ptr<boost::asio::ssl::context>;
static context_ptr on_tls_init()
{
    context_ptr ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::no_sslv3
            | boost::asio::ssl::context::single_dh_use
        );

    } catch (std::exception &e)
    {
        OES_ERROR("Error in context pointer: {}\n", e.what());
    }
    return ctx;
}

//============================================================================

struct OES_CLIENT_EXPORT EtpRawResponse {
    MessageHeader hdr_;
    ProtocolException ex_;

    bool is_acknowledge() const {
        return hdr_.m_messageType == Acknowledge::info.type_.type_;
    }
    bool is_exception() const {
        return hdr_.m_messageType == ProtocolException::info.type_.type_;
    }

    bool final_part() const {
        return has_FIN_bit(hdr_);
    }
    bool compressed() const {
        return has_ZIP_bit(hdr_);
    }

    /*!
     * Checks if message is a Store::GetDataObjectsResponse and if contains the m_blobId chunk indicator.
     * \param[in] header_bytes the header bytes to remove from raw_msg if needed to only decode body
     * \param[in] raw_msg raw message to be decoded to inspect the data objects for chunks
     * \param[out] chunk_container the GetDataObjectsResponse message that may contain chunks.
     * \returns true if chunk m_blobId found
     */
    bool is_chunk_container(const size_t& header_bytes, std::string& raw_msg, Protocol::Store::GetDataObjectsResponse& chunk_container) {

       const bool is_getdataobjs_res_msg = (hdr_.m_protocol == Protocol::Store::GetDataObjectsResponse::info.type_.proto_ &&
            hdr_.m_messageType == Protocol::Store::GetDataObjectsResponse::info.type_.type_);

        if (!is_getdataobjs_res_msg) {
            return false;
        }

        std::string body_msg;
        if(!compressed()){
            // substring this message, to decode only the body
            std::string_view body(raw_msg);
            body = body.substr(header_bytes);
            body_msg = body;
        } else {
            // raw_msg body has already been decompressed and substringed
            body_msg = raw_msg;
        }
        
        const ::avro::DecoderPtr& dec = ::avro::binaryDecoder();
        oes::common::StringAsAvroIStreamView buf(body_msg);
        dec->init(buf);

        chunk_container.decode(*dec);
        for (const auto& data_object : chunk_container.m_dataObjects) {
            const Datatypes::Object::DataObject& obj = data_object.second;
            if (obj.m_format != "xml") {
                continue;
            }
            if (obj.m_data.empty()) {
                if(obj.m_blobId){
                    return true;
                }
            }
        }
        chunk_container.m_dataObjects.clear();
        return false;
    }

    EtpResponseStatus move_to_status() {
        EtpResponseStatus rc;
        rc.final_part = final_part();
        rc.ex = std::move(ex_);
        // Server-side, we can have both rc.ok and rc.has_error() true.
        // But that's sent as a multi-part response, with an error and a message,
        // which we receive independently, so client side .ok is .has_error().
        rc.ok = !rc.has_error();
        return rc;
    }
};

using WrappedMessageHandler = std::variant<
    std::monostate,
    oes::client::proto12::WrappedMessageHandlerVoid,
    oes::client::proto12::WrappedMessageHandlerResp
>;

struct PendingRpc {
    MessageHeader hdr_;
    WrappedMessageHandler handler_;
    std::chrono::high_resolution_clock::time_point sent_;
    Protocol::Store::GetDataObjectsResponse chunk_container_;

    bool is_void_call() const {
        return std::holds_alternative<oes::client::proto12::WrappedMessageHandlerVoid>(handler_);
    }

    /*!
     * Accumulates the data contents of the \param chunk message and
     * reassembles the original chunk_container_
     */
    void acumulate_chunk(const Protocol::Store::Chunk& chunk) {
        if(chunk.m_data.size() == 0 ) {
            OES_ERROR("No chunk data decoded.");
            return;
        }
        if(chunk_container_.m_dataObjects.size() == 0) {
            OES_ERROR("No data objects found in chunk container.");
            return;
        }
        for (auto& data_object : chunk_container_.m_dataObjects) {
            Datatypes::Object::DataObject& obj = data_object.second;
            if (obj.m_format != "xml") {
                continue;
            }
            if(obj.m_blobId == chunk.m_blobId) {
                obj.m_data.insert(obj.m_data.end(), chunk.m_data.begin(), chunk.m_data.end());
                if(chunk.m_final) {
                    obj.m_blobId.reset();
                }
            }
        }
     }
};

inline
auto now() noexcept {
    return std::chrono::high_resolution_clock::now();
}

} // anonymous

//============================================================================
//============================================================================

namespace oes::client {

void setLoggerLevel(const LoggerLevel& level) {
    switch (level) {
        case LoggerLevel::Debug:
            oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::debug);
            break;
            case LoggerLevel::Info:
            oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::info);
            break;
            case LoggerLevel::Warning:
            oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::warning);
            break;
            case LoggerLevel::Error:
            oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::error);
            break;
        default:
            OES_INFO("Setting logger level to info.");
            oes::core::Logger::instance().setSeverityFilter(oes::core::Message::Severity::info);
            break;
    }
}

//============================================================================

ClientException::ClientException(const char* msg, bool bad_conn)
    : std::runtime_error(msg), bad_connection{bad_conn} {}

//============================================================================

class ClientImpl {
public:
    enum ConnStatus { eUninit, eConnecting, eOpen, eClosing, eFailed, eClosed };

    ClientImpl(const bool use_ssl);
    virtual ~ClientImpl();

    virtual void bindHandlers() = 0;
    virtual void bindTslHandler() = 0;
    virtual bool force_disconnection() = 0;

    virtual bool configure_and_connect(const ClientConfig& config, const std::string& url, data12::ErrorInfo* err) = 0;
    virtual void run() = 0;

    virtual void encode_and_send(
        IRequest& ireq, WrappedMessageEncoder req_enc,
        WrappedMessageHandler handler
    ) = 0;
    void log_and_trace_on_send(
        const MessageHeader& hdr,
        const EncodeAndSendRc& rc,
        EtpTracerEventInfo& evt
    );

    bool close_session(std::string reason, data12::ErrorInfo* err = nullptr, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));

    proto12::WrappedMessageSenderVoid void_sender() {
        return [this](
            IRequest& ireq, WrappedMessageEncoder msg_enc,
            proto12::WrappedMessageHandlerVoid decode_and_done_cb
        ) {
            WrappedMessageHandler handler;
            if (decode_and_done_cb) { // can be null
                handler = std::move(decode_and_done_cb);
            }
            encode_and_send(
                ireq, std::move(msg_enc),
                std::move(handler)
            );
        };
    }
    proto12::WrappedMessageSenderResp resp_sender() {
        return [this](
            IRequest& ireq, WrappedMessageEncoder msg_enc,
            proto12::WrappedMessageHandlerResp decode_and_process_cb
        ) {
            assert(decode_and_process_cb); // cannot be null
            encode_and_send(
                ireq, std::move(msg_enc),
                std::move(decode_and_process_cb)
            );
        };
    }

    /*!
     * \returns true if message header type and protocol is Store::Chunk
     */
    bool is_chunk(const MessageHeader& hdr) const {
        return (hdr.m_protocol == Protocol::Store::Chunk::info.type_.proto_ &&
            hdr.m_messageType == Protocol::Store::Chunk::info.type_.type_);
    }

public:
    const bool is_ssl_client = false;
    const Guid client_uid_ = Guid::make();

    const std::chrono::high_resolution_clock::time_point started_ = now();

  //int64_t last_inbound_msg_id_ = 0;
    MsgId last_outbound_msg_id_{MsgKind::eRequest};
    int64_t bytes_recv_ = 0;
    int64_t bytes_sent_ = 0;
    double proc_time_ = 0;

    std::chrono::high_resolution_clock::time_point last_res_recv_ = started_;
    std::chrono::high_resolution_clock::time_point last_req_sent_;

    websocketpp::connection_hdl ws_conn_hdl_;
    ConnStatus ws_conn_status_ = eUninit;

    oes::core::ThreadJoiner thread_joiner_;
    const unsigned id_ = ++session_id__;
    //! std::string session_id_;
    std::promise<std::errc> connected_;

    std::unique_ptr<EtpTracer12> tracer_;

    static constexpr int max_proto = 2425;
    proto12::IProtocol_uptr protos_[max_proto];

    std::mutex pending_rpcs_mt_;
    std::deque<PendingRpc> pending_rpcs_;

    size_t negotiated_max_msg_size_ = 64*1024; // For Core.RequestSession initial message

    const oes::common::ZCompressor* zcompressor_ = nullptr;

    bool acknowledge_ = false;

    int64_t chunk_correlation_id_ = 0;
};

ClientImpl::ClientImpl(const bool use_ssl)
    : is_ssl_client(use_ssl) {}

void ClientImpl::log_and_trace_on_send(
    const MessageHeader& hdr,
    const EncodeAndSendRc& rc,
    EtpTracerEventInfo& evt
) {
    bytes_sent_ += rc.bytes_sent_;
    last_req_sent_ = evt.end_ = now();

    EtpTracer12*const tracer = tracer_.get();
    if (!tracer && oes::core::Logger::instance().minSeverityFilter() > oes::core::Message::Severity::debug) {
        return;
    }

    const unsigned tid = thisThreadId();

    if (tracer) {
        tracer->async_trace(std::move(evt));
    }

    if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug) {
        if (rc.compressed_) {
            OES_DEBUG(
                "SVR DEFL ~{} ${} {} ({:.1f}ms) {} ({:.1f}x)\n",
                tid, id_, msgHeaderAsString(hdr), rc.deflate_millis_,
                thou(rc.body_bytes_), rc.body_zfactor()
            );
        }

        const duration_milli_t millis = evt.end_ - evt.beg_;

        OES_DEBUG(
            "CLT SEND {} {} bytes, to ${} from ~{} ({:.1f}ms later)\n",
            "BIN", rc.bytes_sent_, id_, tid, millis.count()
        );
    }
}

bool ClientImpl::close_session(std::string reason, data12::ErrorInfo* err, std::chrono::milliseconds timeout) try {
    // Open Session
    assert(protos_[0]);
    auto& core_proto = dynamic_cast<proto12::ICore&>(*protos_[0]);

    RequestHeader ireq;
    MessageHeader& hdr = ireq.head_;
    set_FIN_bit(hdr);
    hdr.m_protocol    = proto12::Core::CloseSession::info.type_.proto_;
    hdr.m_messageType = proto12::Core::CloseSession::info.type_.type_;

    EtpResponseStatus rc;
    proto12::Core::CloseSession req;
    req.m_reason = std::move(reason);

    std::promise<bool> done;
    EtpCompleteHandler done_cb;
    if (acknowledge_) {
        done_cb = [&rc, &done](EtpResponseStatus zrc) {
            rc = std::move(zrc);
            done.set_value(true);
        };
    }

    core_proto.CloseSession(
        ireq, req, done_cb
    );

    if (acknowledge_) {
        // We hardcode the timeout to 1/10th of 1s on CloseSession,
        // in case the server does not properly implement Acknowledge (or is too slow)
        std::future<bool> closed = done.get_future();
        std::future_status session_status = closed.wait_for(timeout);
        if (session_status == std::future_status::timeout) {
            if (err) {
                err->m_code = data12::ELIMIT_EXCEEDED;
                err->m_message = fmt::format(
                    "{} timed out", proto12::Core::CloseSession::info.label_
                );
            }
            return false;
        }

        closed.get(); // wait for response.
        if (err && rc.has_error()) {
            if (!rc.ex.m_error) {
                assert(!rc.ex.m_errors.empty());
                *err = rc.ex.m_errors.begin()->second; // Not normal...
            } else {
                assert(rc.ex.m_errors.empty());
                *err = *rc.ex.m_error;
            }
        }
    } else {
        // No response we await one, no done_cb, so return right away
        rc.ok = true;
    }

    return rc.ok;
} catch (const std::exception& ex) {
    if (err) {
        err->m_code = data12::EINVALID_STATE;
        err->m_message = ex.what();
    }
    OES_DEBUG(
        "CLT ERR on {}: {}\n", proto12::Core::CloseSession::info.label_, ex.what());
    return false;
} catch (...) {
    if (err) {
        err->m_code = data12::EINVALID_STATE;
        err->m_message = "Unknown error";
    }
    OES_DEBUG(
        "CLT Unknown ERR on {}\n", proto12::Core::CloseSession::info.label_);
    return false;
}

ClientImpl::~ClientImpl() {}

//============================================================================
template<typename T>
class WSClientAdapter final: public ClientImpl {
public:
    WSClientAdapter(bool is_ssl);
    ~WSClientAdapter() override;
    bool force_disconnection() override;

    bool configure_and_connect(const ClientConfig& config, const std::string& url, data12::ErrorInfo* err) override;
    void run() override;

    void bindTslHandler() override;
    void bindHandlers() override;

    void on_open(ws_conn_hdl_t hdl);
    void on_message(ws_conn_hdl_t hdl, typename T::message_ptr msg);
    void on_close(ws_conn_hdl_t hdl);
    void on_fail(ws_conn_hdl_t hdl);

    void encode_and_send(
        IRequest& ireq, WrappedMessageEncoder req_enc,
        WrappedMessageHandler handler
    ) override;

    T ws_client_;
};

template<typename T>
WSClientAdapter<T>::WSClientAdapter(bool use_ssl)
    : ClientImpl(use_ssl) {}

template<typename T>    
bool WSClientAdapter<T>::force_disconnection() {
    // Ensure the transport loop is interrupted even if close frame cannot be sent.
    ws_client_.stop_perpetual();

    bool attempted_close = false;
    if (ws_conn_status_ == eOpen) {
        websocketpp::lib::error_code ec;
        ws_client_.close(
            ws_conn_hdl_,
            websocketpp::close::status::going_away,
            "FORCE DISCONNECTION FROM CLIENT",
            ec
        );
        OES_UNUSED(ec);
        attempted_close = true;
        ws_conn_status_ = eClosing;
    }

    // Do not stop ASIO here: the close frame must be flushed so the server sees the disconnection now.
    // The hard-stop against stalled peers is done in the destructor.
    return attempted_close || ws_conn_status_ != eClosed;
}

template<typename T>    
bool WSClientAdapter<T>::configure_and_connect(const ClientConfig& config, const std::string& url, data12::ErrorInfo* err) {
    websocketpp::lib::error_code ec;
    typename T::connection_ptr conn = ws_client_.get_connection(url, ec);
    if (ec) {
        if (err) {
            // Can be invalid URI or not secure endpoint
            err->m_code = data12::EINVALID_URI;
            err->m_message = ec.message();
        }
        OES_ERROR("Failed connect to server ({})\n", ec.message());
        return false;
    }

    ws_conn_hdl_ = conn->get_handle();

    conn->add_subprotocol(etpSubProtocol, ec);
    OES_ASSERT(!ec, ;);

    if (!config.emulate_browser_client_) {
        conn->append_header("etp-encoding", config.avro_binary_? "binary": "json");

        if (!config.auth_hdr_.empty()) {
            conn->append_header("Authorization", config.auth_hdr_);
        }

        // OSDU data-partition support
        // It is required when connecting to a multi-partition (which is the default) OSDU server
        if (!config.data_partition_id_hdr_.empty()) {
            conn->append_header("data-partition-id", config.data_partition_id_hdr_);
        }

        // This header field allows the server to be more verbose about the session when reporting an error to the client.
        if (!config.client_id_.empty()) {
            conn->append_header("client-id", config.client_id_);
        }
    }

    // How big any single WebSocket message can be.
    // WebSocketPP automatically disconnects when exceeded!
    conn->set_max_message_size(config.max_mgs_size_);
    conn->set_max_http_body_size(config.max_mgs_size_);

    ws_conn_status_ = ClientImpl::eConnecting;
    ws_client_.connect(conn);
    last_req_sent_ = now();

    return true;
}

template<typename T>
void WSClientAdapter<T>::run() {
    ws_client_.run();
}


template<typename T>
void WSClientAdapter<T>::encode_and_send(
    IRequest& ireq,
    WrappedMessageEncoder msg_enc,
    WrappedMessageHandler handler
) {
    if (ws_conn_status_ != eOpen) {
        throw ClientException("Connection not open", true);
    }

    const unsigned tid = thisThreadId();

    EtpTracerEventInfo evt; // those 3 fields used for logging too
    evt.beg_ = now();
    evt.tid_ = tid;
    evt.sid_ = id_;
    EtpTracer12*const tracer = tracer_.get();
    if (tracer) {
        evt.set_phase(EtpTracerEventInfo::eSEND);
        evt.set_encoding( // TODO: JSON
            /*text? EtpTracerEventInfo::eAVRO_JSON:*/ EtpTracerEventInfo::eAVRO_BINARY
        );
        // Partial (because header is never compressed) compression from ETP is separate
        // This is about the full-tracing event itself being compressed, and it is not.
        //evt.set_compression(EtpTracerEventInfo::eUNCOMPRESSED); i.e. the default
    }

    typename T::connection_ptr conn = ws_client_.get_con_from_hdl(ws_conn_hdl_);
    if (!conn) {
        throw ClientException("Cannot get WebSocket", true);
    }

    auto hdr = ireq.header(); // needs mutable header

    /*
    ** Both of these fields are supposed to have been set already,
    ** for multi-part messages. See BaseRPC::encoding in Client.hpp
    **
    hdr.m_correlationId = 0;
    hdr.m_messageFlags = 0; let clients set flags
    */

    if (is_chunk(hdr)) {
        // Chunk correlationId must be the messageId of the 
        // message that resulted in the need to create chunks
        if (chunk_correlation_id_ == 0) {
            chunk_correlation_id_ = last_outbound_msg_id_.currentId();
        }
        hdr.m_correlationId = chunk_correlation_id_;

        if (has_FIN_bit(hdr)) {
            chunk_correlation_id_ = 0; // reset
        }
    }

    if (acknowledge_) {
        set_ACK_bit(hdr);
    }

    // Need to lock `pending_rpcs_` before generate `hdr.m_messageId` for avoid wrong order of messages in multi-thread scenarios
    std::lock_guard lock(pending_rpcs_mt_);

    // TODO: Pass ireq and msg_id instead of hdr, so headerExt() can be encoded too
    const bool send_ok = doEncodeAndSend(
        conn, hdr, msg_enc, last_outbound_msg_id_,
        negotiated_max_msg_size_, std::min<size_t>(4*1204,negotiated_max_msg_size_-1), zcompressor_,
        [&](const MessageHeader& hdr) {
            if (handler.index()) { // i.e. has a handler (CloseSession w/o ACK has none)
                // Warning: Must update pending_rpcs_ *BEFORE* async-send on Connection,
                // (inside doEncodeAndSend()) to avoid race with reply arriving *BEFORE*
                // pending_rpcs_ was updated, leading to error not finding the handler.
                if (hdr.m_correlationId == 0) {
                    // But only the first/main request should have a handler. See also
                    // https://3.basecamp.com/3247751/buckets/838608/messages/4002407710
                    pending_rpcs_.emplace_back(PendingRpc{ hdr, std::move(handler), now(), {} });
                }
            }

            OES_DEBUG(
                "\nCLT REQ ~{} ${} {}\n",
                tid, id_, msgHeaderAsString(hdr)
            );

            return tracer? &evt.msg_: nullptr;
        },
        [this, &evt](const MessageHeader& hdr, const EncodeAndSendRc& rc) {
            log_and_trace_on_send(hdr, rc, evt);
        }
    );

    if (!send_ok) {
        OES_ERROR("Could not encode+send {}", msgHeaderAsString(hdr));
    }
}

template<typename T>
WSClientAdapter<T>::~WSClientAdapter() {
    if (ws_conn_status_ == eOpen) {
        close_session("EML done.");
        ws_conn_status_ = eClosing;
    }

    ws_client_.stop_perpetual();
    // Hard-stop, to avoid hanging on the thread join below when the peer is stalled.
    ws_client_.stop();

    size_t pending_rpcs_size_before = 0;
    {
        std::lock_guard<std::mutex> guard(pending_rpcs_mt_);
        pending_rpcs_size_before = pending_rpcs_.size();
        pending_rpcs_.clear();
    }
    if (pending_rpcs_size_before) {
        OES_ERROR("ClientImpl deleted with {} pending RPCs\n", pending_rpcs_size_before);
    }

    /** Still try to close after on_fail()?
     * Do not attempt to close after request has been made to server
    if (ws_conn_status_ == eClosing) {
        // See zaphoyd's utility_client.md (https://bit.ly/3t1yww8)
        websocketpp::lib::error_code ec;
        ws_client_.close(
            ws_conn_hdl_,
            websocketpp::close::status::going_away,
            "EML done.", ec
        );
        if (ec) {
            ws_client_.get_elog().write(
                websocketpp::log::elevel::fatal, fmt::format(
                    "CLT ~Impl(${} on ~{}) Error: {}: {}",
                    id_, thisThreadId(), ec.value(), ec.message()
                )
            );
        }
    }*/

    thread_joiner_.join_all(); // explicit, only for debugging
}

template<typename T>
void WSClientAdapter<T>::bindHandlers() {
    // Disable internal logging of WebSocketsPP library, except for the minimum level of logging
    ws_client_.clear_access_channels(websocketpp::log::alevel::all);
    if (core::Logger::instance().minSeverityFilter() > core::Message::Severity::min_severity)
    {
        ws_client_.clear_error_channels(websocketpp::log::elevel::all);
    }

    ws_client_.init_asio();
    ws_client_.start_perpetual();

    ws_client_.set_message_handler(
        [this](ws_conn_hdl_t hdl, typename T::message_ptr msg) {
            return on_message(std::move(hdl), std::move(msg));
        }
    );
    ws_client_.set_open_handshake_timeout(30000); // 30s
    ws_client_.set_open_handler(
        [this](ws_conn_hdl_t hdl) {
            return on_open(std::move(hdl));
        }
    );
    ws_client_.set_close_handler(
        [this](ws_conn_hdl_t hdl) {
            return on_close(std::move(hdl));
        }
    );
    ws_client_.set_fail_handler(
        [this](ws_conn_hdl_t hdl) {
            return on_fail(std::move(hdl));
        }
    );
}



template<>
void WSClientAdapter<SSLClient>::bindTslHandler() {
    ws_client_.set_tls_init_handler(websocketpp::lib::bind(&on_tls_init));
}

template<>
void WSClientAdapter<NoSSLClient>::bindTslHandler() {}

template<typename T>
void WSClientAdapter<T>::bindTslHandler() {
    throw std::runtime_error("Not implemented");
}

template<typename T>
void WSClientAdapter<T>::on_open(ws_conn_hdl_t hdl) {
    typename T::connection_ptr conn = ws_client_.get_con_from_hdl(hdl);

    bool secure                         = conn->is_secure();
    const std::string& host             = conn->get_host();
    uint16_t port                       = conn->get_port();
    const std::string& resource         = conn->get_resource();
    const std::string& remote_endpoint  = conn->get_remote_endpoint();

    OES_DEBUG(
        "CLT on_open({}://{}:{}{} from {})\n",
        (secure? "wss": "ws"), host, port, resource, remote_endpoint
    );

    OES_ASSERT(ws_conn_status_ == eConnecting, ;);
    ws_conn_status_ = eOpen;
    connected_.set_value(std::errc());
}

template<typename T>
void WSClientAdapter<T>::on_message(ws_conn_hdl_t hdl, typename T::message_ptr msg) {
    const auto msg_opcode = msg->get_opcode();
    std::string& raw_msg = msg->get_raw_payload();

    EtpTracer12*const tracer = tracer_.get();
    EtpTracerEventInfo trace_evt;
    trace_evt.beg_ = now();
    trace_evt.tid_ = thisThreadId();
    trace_evt.sid_ = id_;
    if (tracer) {
        trace_evt.set_phase(EtpTracerEventInfo::eRECV);
        trace_evt.set_encoding( // TODO: JSON
            /*text? EtpTracerEventInfo::eAVRO_JSON:*/ EtpTracerEventInfo::eAVRO_BINARY
        );
        trace_evt.set_compression( // TODO: Compression
            /*compressed?*/ EtpTracerEventInfo::eUNCOMPRESSED//: EtpTracerEventInfo::eZLIB_DEFLATE
        );

        trace_evt.msg_ = raw_msg; // FIXME: Avoid copy
        tracer->async_trace(trace_evt);
    }

    typename T::connection_ptr conn = ws_client_.get_con_from_hdl(hdl);

    OES_ASSERT(raw_msg.size() > 0, ;);

    const size_t bytes = raw_msg.size();
    const char* msg_type = msgOpcodeAsString(msg_opcode);
    if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug) {
        const duration_milli_t
            millis_later = trace_evt.beg_ - last_res_recv_,
            millis_idle  = trace_evt.beg_ - last_req_sent_
        ;

        OES_DEBUG(
            "CLT SOCK {} UTC ({:.1f}ms later; {:.1f}ms idle)\n",
            oes::core::formatExtendedIsoDateTime(
                std::chrono::system_clock::now(),
                oes::core::FracSecRes::eMicroSecFloor
            ),
            millis_later.count(), millis_idle.count()
        );

        OES_DEBUG(
            "CLT RECV {} {} bytes, from ${} on ~{}, {}\n",
            msg_type, bytes, id_, thisThreadId(), conn->get_remote_endpoint()
        );
    }
    last_res_recv_ = trace_evt.beg_;
    bytes_recv_ += bytes;

    const bool is_binary = (msg_opcode == websocketpp::frame::opcode::BINARY);
    if (!is_binary) {
        OES_ERROR("Unexpected WebSocket {} message. Silently ignored", msg_type);
        return;
    }

    EtpRawResponse raw_res;
    MessageHeader& msg_hdr = raw_res.hdr_;
    oes::common::StringAsAvroIStreamView buf(raw_msg);
    // TODO: JSON (when is JSON negotiated again?)
    const ::avro::DecoderPtr& dec = ::avro::binaryDecoder();
    dec->init(buf);
    msg_hdr.decode(*dec);

    dec->drain();
    const size_t header_bytes = buf.byteCount();

    OES_DEBUG(
        "CLT RES ~{} ${} {} -{}\n",
        trace_evt.tid_, id_, msgHeaderAsString(msg_hdr), header_bytes
    );

    // Special messages
    const bool is_ack = raw_res.is_acknowledge();
    const bool is_err = raw_res.is_exception();
    const bool is_final = raw_res.final_part();
    const bool is_compressed = raw_res.compressed();

    // Uncompress the body, if needed
    if (is_compressed) {
        // Core.RequestSession (or its responses, see 5.2.2 #7)
        // should not be compressed. But then, zcompressor_ will not
        // have been initialized, so test below should cover that case.
        if (!zcompressor_) {
            OES_ERROR(
                "Unexpected response from {}: Compression not negotiated\n"
                , conn->get_remote_endpoint()
            );
            return; // FIXME: How to inform/resume client!!!
        }

        const auto znow = now();

        // Substring this message, to decompress only the body
        std::string_view body(raw_msg);
        body = body.substr(header_bytes);

        // Decompress the message's body
        std::string ubody;
        auto [zok, ztail] = zcompressor_->inflate2(body, ubody);

        const duration_milli_t zdur = now() - znow;

        if (!zok) {
            OES_ERROR(
                "Unexpected response from {}: Could not decompress body\n"
                , conn->get_remote_endpoint()
            );
            return; // FIXME: How to inform/resume client!!!
        }

        OES_DEBUG(
            "CLT INFL ~{} ${} {} ({:.1f}ms) {} ({:.1f}x) +{}\n",
            trace_evt.tid_, id_, msgHeaderAsString(msg_hdr), zdur.count(),
            thou(ubody.size()), (1. * ubody.size() / body.size()), ztail
        );

        // Install this uncompressed body, for later decoding
        raw_msg.swap(ubody);
        raw_msg.shrink_to_fit(); // FIXME: assign instead of swap+shrink_to_fit?
        buf.init(raw_msg);
        dec->init(buf);
    }

    // TODO: Acknowledge. Yes, the server can also request an ACK from the client.
    //       Probably more useful on Notifications, which we don't implement yet.

    PendingRpc* req = nullptr;
    PendingRpc removed_rpc; // needed for lifetime
    size_t pending_rpcs_size_before = 0;
    const char* removed_from = "???";

    std::unique_lock guard(pending_rpcs_mt_);
    pending_rpcs_size_before = pending_rpcs_.size();

    size_t found_count = 0;
    std::deque<PendingRpc>::iterator found_rpc_it;
    for (auto rpc_it = pending_rpcs_.begin(); rpc_it != pending_rpcs_.end(); ++rpc_it)
    {
        if (msg_hdr.m_correlationId == rpc_it->hdr_.m_messageId) {
            found_rpc_it = rpc_it;
            ++found_count;
        }
    }
    if (found_count == 0) {
        // FIXME: Could it be an is_ack for a split-message?
        // Request splitting is rare, only 3 messages do it. See
        // https://3.basecamp.com/3247751/buckets/838608/messages/4002407710
        // Check again when Chunk'ing on Store.PutDataObjects is implemented
        OES_ERROR(
            "Unexpected response from {}: No matching request\n"
            , conn->get_remote_endpoint()
        );
        return; // FIXME: How to inform/resume client!!!
    }
    if (found_count != 1) {
        OES_ERROR(
            "Duplicate response from {}? {} matching pending RPCs\n"
            , conn->get_remote_endpoint(), found_count
        );
        return; // FIXME: How to inform/resume client!!!
    }

    // Cleanup pending_rpcs_
    if (is_final || (found_rpc_it->is_void_call() && is_ack)) {
        removed_rpc = std::move(*found_rpc_it);

        if (&*found_rpc_it == &pending_rpcs_.back()) {
            pending_rpcs_.pop_back();
            removed_from = "back";
        }
        else if (found_rpc_it == pending_rpcs_.begin()) {
            pending_rpcs_.pop_front();
            removed_from = "front";
        }
        else {
            pending_rpcs_.erase(found_rpc_it);
            removed_from = "middle";
        }

        req = &removed_rpc;
        guard.unlock();
    }
    else {
        // Need to keep lock of `removed_rpc` deque because RPC still in the list (can be relocated)
        req = &(*found_rpc_it);
    }

    assert(req);

    auto* void_handler = std::get_if<proto12::WrappedMessageHandlerVoid>(&req->handler_);

    if (is_err) {
        raw_res.ex_.decode(*dec);
        OES_DEBUG(
            "CLT RES ProtocolException\n{}\n", oes::common::to_json(raw_res.ex_));
    } else if (is_ack) {
        const duration_milli_t millis = trace_evt.beg_ - req->sent_;
        OES_DEBUG(
            "CLT ACK of {} ({:.1f}ms later)\n",
            msgHeaderAsString(req->hdr_), millis.count()
        );
        if (!void_handler) {
            return;
        } else {
            // probably in "response" to Core.CloseSession
            return;
        }
    } else {
        OES_ASSERT(msg_hdr.m_protocol == req->hdr_.m_protocol, );
    }

    Protocol::Store::GetDataObjectsResponse chunk_container;
    if(raw_res.is_chunk_container(header_bytes, raw_msg, chunk_container)){
        OES_ASSERT(chunk_container.m_dataObjects.size() > 0, return;);
        req->chunk_container_ = chunk_container;
        OES_DEBUG(
            "CLT RES ~{} ${} {} contains CHUNKS", 
            trace_evt.tid_, id_, msgHeaderAsString(msg_hdr)
        );
        return;
    }

    std::unique_ptr<avro::InputStream> acumulated_chunk_in;
    oes::common::my_buffer_ptr acumulated_chunk_buf;
    if (is_chunk(msg_hdr)) {
        Protocol::Store::Chunk chunk;
        chunk.decode(*dec);
        req->acumulate_chunk(chunk);
        if(!chunk.m_final){
            return;
        }
        if(req->chunk_container_.m_dataObjects.size() > 0){
            acumulated_chunk_buf = oes::common::encode_binary(req->chunk_container_);
            acumulated_chunk_in = std::make_unique<oes::common::MemoryInputStream2>(acumulated_chunk_buf->data(), acumulated_chunk_buf->size());
            dec->init(*acumulated_chunk_in);
            req->chunk_container_.m_dataObjects.clear();
        }
    }

    if (auto* resp_handler = std::get_if<proto12::WrappedMessageHandlerResp>(&req->handler_)) {
        (*resp_handler)(raw_res.move_to_status(), *dec);
    } else if (void_handler) {
        (*void_handler)(raw_res.move_to_status());
    } else {
        const std::string err_msg = fmt::format(
            "No handler (popped {} of RPC[{}]): ~{} ${} {}",
            removed_from, pending_rpcs_size_before,
            thisThreadId(), id_, msgHeaderAsString(msg_hdr)
        );
        /*
        ** I suspect linked to timeouts, but could be a data-RACE too...
        ** Was indeed a Data-Race: See CL#1320547
[2020-Dec-15 20:25:32] Error: No handler: ~3 $9 #24239(24240) DataArray(9).PutDataSubarraysResponse(11) F
[2020-Dec-15 20:25:42] Error: IDataArray::PutDataSubarrays: Error: DataArray(9).PutDataSubarrays(5) timed-out
[2020-Dec-15 20:25:42] Error: Cannot read or send HDF5 dataset: RESQML/f24e7261-265d-43b2-978c-2e672fffe20a/values_patch0
.../Etp12ImportEpcTests.cpp(334): fatal error: in "Etp12ImportEpcTests/test_import_large_epc_max_msg_size": critical check import_ok has failed
        */
        //throw std::logic_error(err_msg);
        OES_ERROR("{}\n", err_msg);
    }

    if (is_final) {
        const duration_milli_t millis = now() - req->sent_;
        OES_DEBUG(
            "CLT DONE with {} ({:.1f}ms total)\n",
            msgHeaderAsString(req->hdr_), millis.count()
        );
    }
}

template<typename T>
void WSClientAdapter<T>::on_close(ws_conn_hdl_t hdl) {
    typename T::connection_ptr conn = ws_client_.get_con_from_hdl(hdl);
    if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug) {
        std::string lib_err;
        std::error_code ec = conn->get_ec();
        if (ec) {
            lib_err = fmt::format(
                "    Library error: {:04}: {}\n",
                ec.value(), ec.message()
            );
        }

        namespace cs = websocketpp::close::status;
        cs::value remote_cc = conn->get_remote_close_code();
        cs::value local_cc  = conn->get_local_close_code();

        const std::string& remote_ep = conn->get_remote_endpoint();
        const std::string& remote_cr = conn->get_remote_close_reason();
        const std::string& local_cr  = conn->get_local_close_reason();

        OES_DEBUG(
            "\nCLT on_close(${} on ~{}, :{} to {})\n"
            "    Remote reason: {:04}: {}; {}\n"
            "     Local reason: {:04}: {}; {}\n"
            "{}",
            id_, thisThreadId(), conn->get_port(), remote_ep,
            remote_cc, cs::get_string(remote_cc),  remote_cr,
            local_cc,  cs::get_string(local_cc),   local_cr,
            lib_err
        );
    }

    ws_conn_status_ = eClosed;

    /*!
    ** Cannot do that, overwise throws std::promise_already_satisfied
    ** Plus connected_ is only used during connect() anyway
    connected_.set_value(false);
    */
}

std::shared_ptr<ClientImpl> makeClientImpl(bool use_ssl) {
    OES_INFO("Using SSL Connection mode {}", use_ssl ? "ON" : "OFF");
    if (use_ssl) {
        auto new_client = std::make_shared<WSClientAdapter<SSLClient>>(true);
        return new_client;
    } else {
        return std::make_shared<WSClientAdapter<NoSSLClient>>(false);
    }
}

template<typename T>
void WSClientAdapter<T>::on_fail(ws_conn_hdl_t hdl) {
    typename T::connection_ptr conn = ws_client_.get_con_from_hdl(hdl);

    const auto error_reason = conn->get_ec();
    OES_DEBUG(
        "\nCLT on_fail(${} on ~{}, :{} to {})\n"
        "    Error reason: {}\n",
        id_, thisThreadId(), conn->get_port(), conn->get_remote_endpoint(), error_reason.message()
    );

    std::errc result = std::errc::host_unreachable;

    // Clarify error code when failed by invalid HTTP status
    if (error_reason.value() == websocketpp::processor::error::invalid_http_status)
    {
        const auto& response = conn->get_response();
        const auto response_code = response.get_status_code();

        if (response_code == websocketpp::http::status_code::bad_request) {
            result = std::errc::bad_message;
        } else if (response_code == websocketpp::http::status_code::unauthorized) {
            result = std::errc::permission_denied;
        } else if (response_code == websocketpp::http::status_code::not_found) {
            result = std::errc::no_such_file_or_directory;
        } else if (response_code == websocketpp::http::status_code::precondition_failed) {
            result = std::errc::wrong_protocol_type;
        }
        else {
            result = std::errc::connection_refused;
        }

        const auto response_code_msg = response.get_status_msg();
        OES_DEBUG("HTTP status code: {} {}\n", response_code, response_code_msg);

        // Logging the HTTP body (if received)
        if (response.ready())
        {
            const auto& response_body = response.get_body();
            OES_DEBUG("Response body:: {}\n", response_body);
        }
    }

    const bool was_connecting = (ws_conn_status_ == eConnecting);
    ws_conn_status_ = eFailed;

    if (was_connecting) {
        // i.e. update connected_ only on initial connection,
        // not for subsequent errors, since promise already fulfilled
        // to true in on_open() above, and cannot be changed.
        connected_.set_value(result);
    }
}

//============================================================================

void ClientConfig::withBasicAuth(
    const std::string& username, const std::string& password
) {
    auto user = oes::common::auth::AuthenticatedUser::withBasicAuth(username, password);
    OES_ASSERT(user.ok_, ;);
    auth_hdr_ = user.header_;
}

bool ClientConfig::withJwtAuth(
    const std::string& username, const std::string& password,
    const std::string& jwtSecret
) {
    auto user = oes::common::auth::AuthenticatedUser::withJwtAuth(username, password, jwtSecret);
    auth_hdr_ = user.header_;
    return user.ok_;
}

void ClientConfig::withJwtAuth(const std::string& jwtToken) {
    auth_hdr_ = "Bearer " + jwtToken;
}

//============================================================================

Client::Client(
    ClientConfig config
) : config_(config)
{
    const bool use_ssl = config.server_url_.rfind("wss:", 0) == 0;
    impl_ = makeClientImpl(use_ssl);

    impl_->acknowledge_ = config.acknowledge_;

    if (config.enable_tracing_) {
        std::string host;
        const std::string* p_host = nullptr;
        websocketpp::uri uri(config.server_url_);
        if (uri.get_valid()) {
            host = uri.get_host();
            p_host = &host;
        }

        EtpTraceHeader file_header = EtpTraceHeader::make();
        file_header.set_tier(EtpTraceHeader::Tier::eCLIENT);
        std::string trace_file = EtpTracer12::trace_filename(
            config.trace_file_, &file_header, p_host
        );
        impl_->tracer_.reset(new EtpTracer12(trace_file, file_header));
        OES_VERBOSE("Tracing to file {}\n", trace_file);
    }

    // dedup protocols, just in case
    auto& protos = config_.requested_protocols_;
    std::sort(protos.begin(), protos.end());
    protos.erase(std::unique(protos.begin(), protos.end()), protos.end());

    impl_->bindTslHandler();
    impl_->bindHandlers();

    auto void_sender = impl_->void_sender();
    auto resp_sender = impl_->resp_sender();

    // Always instantiate Core(0) protocol, it is special.
    impl_->protos_[0] = proto12::IProtocol::makeClientImpl(
        data12::Protocol::eCore, void_sender, resp_sender
    );
    supported_proto_ids_.push_back(data12::Protocol::eCore);

    // For the other requested protocols, we wait for the OpenSession
    // response to our RequestSession, to call makeClientImpl().
}

void Client::reset_impl() {
    const bool use_ssl = impl_->is_ssl_client;
    auto tracer = std::move(impl_->tracer_);

    impl_.reset();
    impl_ = makeClientImpl(use_ssl);
    impl_->acknowledge_ = config_.acknowledge_;

    if (tracer) {
        impl_->tracer_ = std::move(tracer);
    }

    impl_->bindTslHandler();
    impl_->bindHandlers();

    auto void_sender = impl_->void_sender();
    auto resp_sender = impl_->resp_sender();

    for (auto proto_id : supported_proto_ids_) {
        impl_->protos_[static_cast<int>(proto_id)] = proto12::IProtocol::makeClientImpl(
            proto_id, void_sender, resp_sender
        );
    }
}

bool Client::force_disconnection() {
    return impl_->force_disconnection();
}

bool Client::connect(std::chrono::milliseconds timeout, data12::ErrorInfo* err) {
    impl_->last_res_recv_ = now();
    OES_DEBUG(
        "CLT CONN {} UTC Timeout = {}ms\n",
        oes::core::formatExtendedIsoDateTime(
            std::chrono::system_clock::now(),
            oes::core::FracSecRes::eMicroSecFloor
        ),
        timeout.count()
    );

    if (auto* tracer = impl_->tracer_.get()) {
        tracer->start();
    }

    std::string url = config_.server_url_;
    assert(!url.empty());

    if (config_.emulate_browser_client_) {
        if (url.back() != '/') {
            url += '/';
        }

        url += fmt::format(
            "?etp-encoding={}", config_.avro_binary_? "binary": "json"
        );

        if (!config_.auth_hdr_.empty()) {
            url += fmt::format(
                "&Authorization={}", config_.auth_hdr_
            );
        }

        // OSDU data-partition support
        if (!config_.data_partition_id_hdr_.empty()) {
            url += fmt::format(
                "&data-partition-id={}", config_.data_partition_id_hdr_
            );
        }

        if (!config_.client_id_.empty()) {
            url += fmt::format(
                "&client-id={}", config_.client_id_
            );
        }

        std::ostringstream oss;
        EmlUri::percentEncode(oss, url);
        url = oss.str();
    }

    OES_DEBUG(
        "CLT connect(${} on ~{}, {}) Connecting\n",
        impl_->id_, thisThreadId(), url
    );

    websocketpp::lib::error_code ec;
    impl_->configure_and_connect(config_, url, err);

    // start the background thread running the ASIO loop
    impl_->thread_joiner_.threads_.emplace_back([impl = this->impl_.get()]() {
        impl->run(); // Does not return, until Impl's Dtor
    });

    // Wait for WebSocketPP to call us back on either on_open() or on_fail()
    // FIXME: If timeout is too small (i.e. 500ms in Etp12ServerTests.cpp),
    //        get can get "promise already fulfilled" exception...
    std::future<std::errc> connected = impl_->connected_.get_future();
    std::future_status connection_status = connected.wait_for(timeout);
    if (connection_status == std::future_status::timeout) {
        impl_.reset();
        if (err) {
            err->m_code = data12::ETIMED_OUT;
            err->m_message = "WebSocket connection timed-out";
        }
        return false;
    }

    const std::errc errCode = connected.get(); // wait for the response
    if (errCode != std::errc()) {
        const duration_milli_t millis = now() - impl_->last_res_recv_;
        OES_DEBUG(
            "CLT connect(${} on ~{}, {}) Failed ({:.1f}ms)\n",
            impl_->id_, thisThreadId(), url, millis.count()
        );
        if (err)
        {
            switch (errCode)
            {
            // Server is down or wrong URL
            case std::errc::host_unreachable:
                err->m_code = data12::ENOT_FOUND;
                err->m_message = "The server is unreachable";
                break;

            // Server returns 400 bad request (wrong partition, etc)
            case std::errc::bad_message:
                err->m_code = data12::EINVALID_ARGUMENT;
                err->m_message = "Bad request";
                break;

            // Server returns 401 unauthorized request
            case std::errc::permission_denied:
                err->m_code = data12::EAUTHORIZATION_REQUIRED;
                err->m_message = "Unauthorized request";
                break;

            // Server returns 404 page not found (wrong URL)
            case std::errc::no_such_file_or_directory:
                err->m_code = data12::ENOT_FOUND;
                err->m_message = "The requested URL was not found";
                break;

            // Connection was refused due unsupported sub-protocol
            case std::errc::wrong_protocol_type:
                err->m_code = data12::ENOT_FOUND;
                err->m_message = fmt::format("Connection refused, requested sub-protocol '{}' is not supported", etpSubProtocol);
                break;

            default:
                // All other error connection reasons
                err->m_code = data12::ELIMIT_EXCEEDED;
                err->m_message = "WebSocket connection refused";
                break;
            }
        }
        impl_.reset();
        return false;
    }

    OES_ASSERT(impl_->ws_conn_status_ == ClientImpl::eOpen, ;);
    const duration_milli_t millis = now() - impl_->last_res_recv_;
    OES_INFO(
        "CLT connect(${} on ~{}, {}) OK ({:.1f}ms)\n",
        impl_->id_, thisThreadId(), url, millis.count()
    );


    assert(impl_->protos_[0]);
    auto& core_proto = dynamic_cast<proto12::ICore&>(*impl_->protos_[0]);

    // Authorize
    RequestHeader ireq_auth;
    MessageHeader& hdr_auth = ireq_auth.head_;
    set_FIN_bit(hdr_auth);
    hdr_auth.m_protocol    = proto12::Core::Authorize::info.type_.proto_;
    hdr_auth.m_messageType = proto12::Core::Authorize::info.type_.type_;

    if (!config_.auth_hdr_.empty()) {
        proto12::Core::Authorize auth;
        auth.m_authorization = config_.auth_hdr_;

        EtpResponseStatus rc_auth;
        proto12::Core::AuthorizeResponse res_auth;
        std::promise<bool> done_auth;
        core_proto.Authorize(
            ireq_auth, auth,
            [&rc_auth, &res_auth, &done_auth](EtpResponseStatus zrc,  proto12::Core::AuthorizeResponse zres) {
                rc_auth = std::move(zrc);
                res_auth = std::move(zres);
                done_auth.set_value(true);
            }
        );

        std::future<bool> authorized = done_auth.get_future();
        std::future_status authorize_status = authorized.wait_for(timeout);
        if (authorize_status == std::future_status::timeout) {
            if (err) {
                err->m_code = data12::ELIMIT_EXCEEDED;
                err->m_message = fmt::format("{} timed-out", proto12::Core::Authorize::info.label_);
            }
            return false;
        }

        authorized.get(); // wait for response
        if (err && rc_auth.has_error()) {
            if (!rc_auth.ex.m_error) {
                assert(!rc_auth.ex.m_errors.empty());
                *err = rc_auth.ex.m_errors.begin()->second; // Not normal...
            } else {
                assert(rc_auth.ex.m_errors.empty());
                *err = *rc_auth.ex.m_error;
            }
        }
    }


    // Open Session
    RequestHeader ireq;
    MessageHeader& hdr = ireq.head_;
    set_FIN_bit(hdr);
    hdr.m_protocol    = proto12::Core::RequestSession::info.type_.proto_;
    hdr.m_messageType = proto12::Core::RequestSession::info.type_.type_;

    // The ETP version we support. 1.2.0 ATM
    Datatypes::Version v12;
    v12.m_major = 1;
    v12.m_minor = 2;

    EtpResponseStatus rc;
    proto12::Core::RequestSession req;
    req.m_applicationName = fmt::format(
        "EML - Emerson ETP-{}.{} Client", v12.m_major, v12.m_minor
    );

    #if defined GIT_COMMIT_HASH && defined GIT_COMMIT_TIMESTAMP
    // Use last git commit hash and date as version number
    const std::string commitId = DEFINE_TO_STRING(GIT_COMMIT_HASH);
    const std::string buildTime = DEFINE_TO_STRING(GIT_COMMIT_TIMESTAMP);
    const std::string appVers = commitId + " (" + buildTime + ")";
    #else
    const std::string appVers = "v1";
    #endif

    req.m_applicationVersion = "1.0 - " + appVers;
    oes::common::assignUid(req.m_clientInstanceId, impl_->client_uid_);
    req.m_supportedFormats = { "xml" };

    // Requested protocols a client-supplied arg
    auto& req_protos = config_.requested_protocols_;
    if (!req_protos.empty()) {
        if (req_protos[0] != data12::Protocol::eCore) {
            req_protos.insert(req_protos.begin(), data12::Protocol::eCore);
        }
    } else {
        req_protos.emplace_back(data12::Protocol::eCore);
    }
    assert(req_protos[0] == data12::Protocol::eCore);

    // Subset of protocols we've actually tested against (alphabetical):
    // eCore, eDataArray, eDataspace, eDiscovery, eStore, eSupportedTypes, eTransaction

    // eCore proto is the only one with "server" role, all other protocols
    // use "store" as role. We made sure it's always 1st, so role starts as
    // "server", and is changed to "store" after the first one (eCore).
    const char* role = "server";
    for (Datatypes::Protocol proto_id : req_protos) {
        data12::SupportedProtocol& proto = req.m_requestedProtocols.emplace_back();
        proto.m_protocol = int32_t(proto_id);
        proto.m_role = role;
        proto.m_protocolVersion = v12;
        if (proto_id == Datatypes::Protocol::eStoreOSDU ||
            proto_id == Datatypes::Protocol::eDataspaceOSDU) {
            proto.m_protocolVersion.m_revision = 1;
        }

        role = "store"; // see above why
    }

    if (!config_.compress_ || config_.zcompressors_.empty()) {
        req.m_supportedCompression.clear();
    } else {
        req.m_supportedCompression = config_.zcompressors_;
    }

    auto& max_mgs_size = req.m_endpointCapabilities["MaxWebSocketMessagePayloadSize"];
    max_mgs_size.m_item = static_cast<int64_t>(config_.max_mgs_size_);

    req.m_supportedDataObjects.emplace_back().m_qualifiedType = "eml20.*";
    req.m_supportedDataObjects.emplace_back().m_qualifiedType = "resqml20.*";

    {
        using namespace std::chrono;
        using store_tstamp = time_point<system_clock, microseconds>;

        store_tstamp client_ts = time_point_cast<microseconds>(system_clock::now());
        req.m_currentDateTime = client_ts.time_since_epoch().count();
    }

    proto12::Core::OpenSession res; // TODO: Keep it around, fill negotiated_protocols, etc...
    std::promise<bool> done;
    core_proto.RequestSession(
        ireq, req,
        [&rc, &res, &done](EtpResponseStatus zrc,  proto12::Core::OpenSession zres) {
            rc = std::move(zrc);
            res = std::move(zres);
            done.set_value(true);
        }
    );

    std::future<bool> opened = done.get_future();
    std::future_status session_status = opened.wait_for(timeout);
    if (session_status == std::future_status::timeout) {
        if (err) {
            err->m_code = data12::ELIMIT_EXCEEDED;
            err->m_message = fmt::format("{} timed-out", proto12::Core::OpenSession::info.label_);
        }
        return false;
    }

    opened.get(); // wait for response
    if (err && rc.has_error()) {
        if (!rc.ex.m_error) {
            assert(!rc.ex.m_errors.empty());
            *err = rc.ex.m_errors.begin()->second; // Not normal...
        } else {
            assert(rc.ex.m_errors.empty());
            *err = *rc.ex.m_error;
        }
    }

    if (!rc.ok) {
        return rc.ok;
    }

    auto uid_to_str = [](std::array<uint8_t, 16>& arr) -> std::string {
        Guid guid;
        oes::common::assignUid(guid, arr);

        return guid.toStringNoDash();
    };

    session_uuid_ = res.m_sessionId;
    OES_DEBUG("Connection session_uuid = {}", uid_to_str(session_uuid_));

    // Check the server-returned protocols,
    // which should be a subset of the ones we requested
    auto& protos = res.m_supportedProtocols;
    const size_t count_before = protos.size();
    std::sort(
        protos.begin(), protos.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.m_protocol < rhs.m_protocol;
        }
    );
    protos.erase(
        std::unique(
            protos.begin(), protos.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.m_protocol == rhs.m_protocol;
            }
        ),
        protos.end()
    );
    if (count_before != protos.size()) {
        OES_WARNING("Duplicated Server SupportedProtocols IDs\n");
    }

    auto void_sender = impl_->void_sender();
    auto resp_sender = impl_->resp_sender();

    for (const Datatypes::SupportedProtocol& sproto : protos) {
        auto proto_id = Datatypes::Protocol(sproto.m_protocol);
        OES_ASSERT(sproto.m_protocol < ClientImpl::max_proto, return false;);

        if (proto_id == data12::Protocol::eCore) {
            continue; // makeClientImpl() already called in Ctor
        }

        // Did we request that proto?
        auto found = std::lower_bound(req_protos.begin(), req_protos.end(), proto_id);
        if (found == req_protos.end() || *found != proto_id) {
            OES_WARNING("Server returned unrequested protocol: {}", static_cast<int>(proto_id));
            continue;
        }

        proto12::IProtocol_uptr proto = proto12::IProtocol::makeClientImpl(
            proto_id, void_sender, resp_sender
        );
        if (proto) {
            auto proto_id_int = static_cast<int>(proto->id_);
            OES_ASSERT(0 < proto_id_int && proto_id_int < impl_->max_proto, continue;);
            impl_->protos_[proto_id_int] = std::move(proto);

            supported_proto_ids_.push_back(proto_id);
        }

        if (proto_id == data12::Protocol::eCoreOSDU) {
            can_reconnect_ = true;
        }
    }

    if (!res.m_supportedCompression.empty()) {
        if (res.m_supportedCompression == "gzip") {
            impl_->zcompressor_ = &oes::common::GZipCompressor;
        } else {
            assert(false); // should not happen
        }

        // TODO: LZ4F, ZSTD
    }

    // Cap#1 MaxWebSocketMessagePayloadSize
    constexpr char max_msg_size_key[] = "MaxWebSocketMessagePayloadSize";
    size_t client_max_msg_size = config_.max_mgs_size_;
    OES_VERBOSE(
        "CLT Client  {} = {}\n", max_msg_size_key, thou(client_max_msg_size));

    size_t session_max_msg_size = client_max_msg_size;
    auto max_msg_found = res.m_endpointCapabilities.find(max_msg_size_key);
    // FIXME: Remove: Temporary work-around for PV's server
    if (max_msg_found == res.m_endpointCapabilities.end()) {
        std::string alt_key = max_msg_size_key; alt_key[0] = 'm';
        max_msg_found = res.m_endpointCapabilities.find(alt_key);
    }
    if (max_msg_found != res.m_endpointCapabilities.end()) {
        const data12::DataValue_item_v& val = max_msg_found->second.m_item;
        switch (val.index()) {
        case size_t(data12::DataValue_item_e::eLong):   session_max_msg_size = size_t(std::get<int64_t>(val)); break;
        case size_t(data12::DataValue_item_e::eInt):    session_max_msg_size = size_t(std::get<int>(val)); break;
        default:
            OES_WARNING("Unexpected {} value type\n", max_msg_size_key);
        }
        if (size_t N = session_max_msg_size) {
            OES_VERBOSE("CLT Session {} = {}\n", max_msg_size_key, thou(N));
            if (N > client_max_msg_size) {
                OES_WARNING("CLT Session {} exceeds Client one!\n", max_msg_size_key);
            }
            negotiated_max_msg_size_ = impl_->negotiated_max_msg_size_ = N;
        }
    } else {
        OES_VERBOSE("No Session-advertised {}\n", max_msg_size_key);
        negotiated_max_msg_size_ = impl_->negotiated_max_msg_size_ = client_max_msg_size;
    }

    // Cap#2 MaxBatchSize
    // TODO: server-side only for now

    return rc.ok;
}

bool Client::reconnect(std::chrono::milliseconds timeout, data12::ErrorInfo* err) {
    if (!can_reconnect_) {
        OES_ERROR("Can not reconnect to the server. Primary session not start or unsupported ResumeSession protocol");
        return false;
    }

    reset_impl();
    impl_->last_res_recv_ = now();


    OES_DEBUG(
        "CLT RECONN {} UTC Timeout = {}ms\n",
        oes::core::formatExtendedIsoDateTime(
            std::chrono::system_clock::now(),
            oes::core::FracSecRes::eMicroSecFloor
        ),
        timeout.count()
    );

    std::string url = config_.server_url_;
    assert(!url.empty());

    if (config_.emulate_browser_client_) {
        if (url.back() != '/') {
            url += '/';
        }

        url += fmt::format(
            "?etp-encoding={}", config_.avro_binary_? "binary": "json"
        );

        if (!config_.auth_hdr_.empty()) {
            url += fmt::format(
                "&Authorization={}", config_.auth_hdr_
            );
        }

        // OSDU data-partition support
        if (!config_.data_partition_id_hdr_.empty()) {
            url += fmt::format(
                "&data-partition-id={}", config_.data_partition_id_hdr_
            );
        }

        if (!config_.client_id_.empty()) {
            url += fmt::format(
                "&client-id={}", config_.client_id_
            );
        }

        std::ostringstream oss;
        EmlUri::percentEncode(oss, url);
        url = oss.str();
    }

    OES_DEBUG(
        "CLT connect(${} on ~{}, {}) Connecting\n",
        impl_->id_, thisThreadId(), url
    );

    // connect to server
    // TODO refactor
    impl_->configure_and_connect(config_, url, err);
    impl_->thread_joiner_.threads_.emplace_back([impl = this->impl_.get()](){
        impl->run();
    });

    std::future<std::errc> connected = impl_->connected_.get_future();
    std::future_status connection_status = connected.wait_for(timeout);
    if (connection_status == std::future_status::timeout) {
        impl_.reset();
        if (err) {
            err->m_code = data12::ETIMED_OUT;
            err->m_message = "WebSocket reconnection timed-out";
        }

        return false;
    }

    const std::errc errCode = connected.get(); // wait for the response
    if (errCode != std::errc()) {
        const duration_milli_t millis = now() - impl_->last_res_recv_;
        OES_DEBUG(
            "CLT connect(${} on ~{}, {}) Failed ({:.1f}ms)\n",
            impl_->id_, thisThreadId(), url, millis.count()
        );
        if (err)
        {
            switch (errCode)
            {
            // Server is down or wrong URL
            case std::errc::host_unreachable:
                err->m_code = data12::ENOT_FOUND;
                err->m_message = "The server is unreachable";
                break;

            // Server returns 400 bad request (wrong partition, etc)
            case std::errc::bad_message:
                err->m_code = data12::EINVALID_ARGUMENT;
                err->m_message = "Bad request";
                break;

            // Server returns 401 unauthorized request
            case std::errc::permission_denied:
                err->m_code = data12::EAUTHORIZATION_REQUIRED;
                err->m_message = "Unauthorized request";
                break;

            // Server returns 404 page not found (wrong URL)
            case std::errc::no_such_file_or_directory:
                err->m_code = data12::ENOT_FOUND;
                err->m_message = "The requested URL was not found";
                break;

            // Connection was refused due unsupported sub-protocol
            case std::errc::wrong_protocol_type:
                err->m_code = data12::ENOT_FOUND;
                err->m_message = fmt::format("Connection refused, requested sub-protocol '{}' is not supported", etpSubProtocol);
                break;

            default:
                // All other error connection reasons
                err->m_code = data12::ELIMIT_EXCEEDED;
                err->m_message = "WebSocket connection refused";
                break;
            }
        }
        impl_.reset();
        return false;
    }

    OES_ASSERT(impl_->ws_conn_status_ == ClientImpl::eOpen, ;);
    const duration_milli_t millis = now() - impl_->last_res_recv_;
    OES_INFO(
        "CLT connect(${} on ~{}, {}) OK ({:.1f}ms)\n",
        impl_->id_, thisThreadId(), url, millis.count()
    );    

    /// .... 
    assert(impl_->protos_[0]);
    auto& core_proto = dynamic_cast<proto12::ICore&>(*impl_->protos_[0]);

    // Authorize
    RequestHeader ireq_auth;
    MessageHeader& hdr_auth = ireq_auth.head_;
    set_FIN_bit(hdr_auth);
    hdr_auth.m_protocol    = proto12::Core::Authorize::info.type_.proto_;
    hdr_auth.m_messageType = proto12::Core::Authorize::info.type_.type_;

    if (!config_.auth_hdr_.empty()) {
        proto12::Core::Authorize auth;
        auth.m_authorization = config_.auth_hdr_;

        EtpResponseStatus rc_auth;
        proto12::Core::AuthorizeResponse res_auth;
        std::promise<bool> done_auth;
        core_proto.Authorize(
            ireq_auth, auth,
            [&rc_auth, &res_auth, &done_auth](EtpResponseStatus zrc,  proto12::Core::AuthorizeResponse zres) {
                rc_auth = std::move(zrc);
                res_auth = std::move(zres);
                done_auth.set_value(true);
            }
        );

        std::future<bool> authorized = done_auth.get_future();
        std::future_status authorize_status = authorized.wait_for(timeout);
        if (authorize_status == std::future_status::timeout) {
            if (err) {
                err->m_code = data12::ELIMIT_EXCEEDED;
                err->m_message = fmt::format("{} timed-out", proto12::Core::Authorize::info.label_);
            }
            return false;
        }

        authorized.get(); // wait for response
        if (err && rc_auth.has_error()) {
            if (!rc_auth.ex.m_error) {
                assert(!rc_auth.ex.m_errors.empty());
                *err = rc_auth.ex.m_errors.begin()->second; // Not normal...
            } else {
                assert(rc_auth.ex.m_errors.empty());
                *err = *rc_auth.ex.m_error;
            }
        }
    }

    // Resume Session
    RequestHeader ireq;
    MessageHeader& hdr = ireq.head_;
    set_FIN_bit(hdr);
    hdr.m_protocol = proto12::CoreOSDU::ResumeSession::info.type_.proto_;
    hdr.m_messageType = proto12::CoreOSDU::ResumeSession::info.type_.type_;

    // The ETP version we support. 1.2.0 ATM
    Datatypes::Version v12;
    v12.m_major = 1;
    v12.m_minor = 2;

    EtpResponseStatus rc;
    proto12::CoreOSDU::ResumeSession req;

    assert(!session_uuid_.empty());
    req.m_sessionId = session_uuid_;
    // oes::common::assignUid(req.m_sessionId, Guid::parse(impl_->session_id_)); 

    req.m_applicationName = fmt::format(
        "EML - Emerson ETP-{}.{} Client", v12.m_major, v12.m_minor
    );

    #if defined GIT_COMMIT_HASH && defined GIT_COMMIT_TIMESTAMP
    // Use last git commit hash and date as version number
    const std::string commitId = DEFINE_TO_STRING(GIT_COMMIT_HASH);
    const std::string buildTime = DEFINE_TO_STRING(GIT_COMMIT_TIMESTAMP);
    const std::string appVers = commitId + " (" + buildTime + ")";
    #else
    const std::string appVers = "v1";
    #endif

    req.m_applicationVersion = "1.0 - " + appVers;
    oes::common::assignUid(req.m_clientInstanceId, impl_->client_uid_);
    
    {
        using namespace std::chrono;
        using store_tstamp = time_point<system_clock, microseconds>;

        store_tstamp client_ts = time_point_cast<microseconds>(system_clock::now());
        req.m_currentDateTime = client_ts.time_since_epoch().count();
    }

    proto12::CoreOSDU::ResumeSessionResponse res;
    std::promise<bool> done;

    const auto core_osdu_proto_id = static_cast<int>(data12::Protocol::eCoreOSDU);
    assert(impl_->protos_[core_osdu_proto_id] != nullptr);

    auto& core_osdu_proto = dynamic_cast<proto12::ICoreOSDU&>(*impl_->protos_[core_osdu_proto_id]);
    core_osdu_proto.ResumeSession(
        ireq, req,
        [&rc, &res, &done](EtpResponseStatus zrc, proto12::CoreOSDU::ResumeSessionResponse zres) {
            rc = std::move(zrc);
            res = std::move(zres);
            done.set_value(true);
        }
    );

    std::future<bool> session_recovered = done.get_future();
    std::future_status session_status = session_recovered.wait_for(timeout);
    if (session_status == std::future_status::timeout) {
        if (err) {
            err->m_code = data12::ELIMIT_EXCEEDED;
            err->m_message = fmt::format("{} timed-out", proto12::CoreOSDU::ResumeSessionResponse::info.label_);
        }

        return false;
    }

    session_recovered.get();
    if (err && rc.has_error()) {
        if (!rc.ex.m_error) {
            assert(!rc.ex.m_errors.empty());
            *err = rc.ex.m_errors.begin()->second;
        } else {
            assert(rc.ex.m_errors.empty());
            *err = *rc.ex.m_error;
        }
    }

    if (!rc.ok) {
        return rc.ok;
    }

    session_uuid_ = res.m_sessionId;
    return rc.ok;
} 

bool Client::isConnOpen() const {
    if (!impl_) {
        OES_INFO("Client is not connected.");
        return false;
    }

    return impl_->ws_conn_status_ == ClientImpl::eOpen;
}

bool Client::close(std::string& failure, std::chrono::milliseconds timeout) {
    if (!impl_) {
        OES_INFO("Client is not connected.");
        return true;
    }

    data12::ErrorInfo err;
    auto status = impl_->close_session("EML done. Closed by the client.", &err, timeout);
    failure = err.m_message;
    // CloseSession sent: the destructor must not send another one (the server would reject it).
    impl_->ws_conn_status_ = ClientImpl::eClosing;
    return status;
}

ClientConfig Client::getConfig() const {
    return config_;
}

std::vector<data12::Protocol> Client::requested_protocols() const {
    return config_.requested_protocols_;
}

std::vector<data12::Protocol> Client::negotiated_protocols() const {
    OES_ASSERT(impl_, return {};); // not connected

    std::vector<data12::Protocol> negotiated;
    negotiated.reserve(config_.requested_protocols_.size());
    for (auto proto_id : config_.requested_protocols_) {
        OES_ASSERT(int(proto_id) < ClientImpl::max_proto, continue;);
        auto* proto = impl_->protos_[int(proto_id)].get();
        if (proto) {
            negotiated.emplace_back(proto_id);
        }
    }

    return negotiated;
}

proto12::IProtocol* Client::getProtocol(data12::Protocol proto_id) {
    OES_ASSERT(impl_, return nullptr;); // not connected

    const auto& protos = config_.requested_protocols_;
    auto found = std::lower_bound(protos.begin(), protos.end(), proto_id);
    if (found == protos.end() || *found != proto_id) {
        return nullptr;
    }
    OES_ASSERT(int(proto_id) < ClientImpl::max_proto, return nullptr;);
    return impl_->protos_[int(proto_id)].get();
}

//============================================================================
//============================================================================

ScopedTransaction::ScopedTransaction(
    oes::client::Client& client,
    std::chrono::milliseconds timeout
) : tx_proto_(client.trans_proto())
  , timeout_(timeout)
{
}

void ScopedTransaction::replace_proto(oes::client::Client& client) {
    tx_proto_ = client.trans_proto();
}

ScopedTransaction::~ScopedTransaction() {
    if (tx_proto_ && started_) {
        rollback();
    }
}

template <typename TRpc>
bool ScopedTransaction::call(TRpc& rpc, std::string* failure_reason) {
    std::ostringstream oss;
    auto ok = rpc.call(oss, timeout_);
    if (!ok.first || !oss.str().empty()) {
        if (failure_reason) {
            *failure_reason = rpc.res_.m_failureReason;

            if (failure_reason->empty()) {
                *failure_reason = oss.str();
            }
        }
        return false;
    }

    return rpc.res_.m_successful;
}

bool ScopedTransaction::start(
    std::string space_uri,
    std::string message,
    bool read_only,
    std::string* failure_reason,
    const unsigned short retries,
    std::chrono::milliseconds retry_interval,
    double factor,
    std::chrono::milliseconds max_interval
) {
    if (!tx_proto_) {
        return true; // silent no-op
    }
    if (started_) {
        return false;
    }

    auto rpc = rpcFor(tx_proto_, &proto12::ITransaction::StartTransaction);

    rpc.req_.m_readOnly = read_only;
    rpc.req_.m_message = std::move(message);
    rpc.req_.m_dataspaceUris.clear();
    rpc.req_.m_dataspaceUris.emplace_back(std::move(space_uri));

    if (factor < 1) {
        factor = 1;
    }

    if (retry_interval.count() < 100 ) {
        retry_interval = std::chrono::milliseconds(100);
    } 

    if (max_interval < retry_interval ) {
        retry_interval = retry_interval*2;
    } 

    int attempts { 1 };
    // Variables to avoid casts during arithmetic operations with factor
    size_t retry_interval_ms = retry_interval.count();
    const size_t max_interval_ms = max_interval.count();
    while (attempts <= retries + 1) {
        if (call(rpc, failure_reason)) {
            std::copy(
                rpc.res_.m_transactionUuid.begin(), rpc.res_.m_transactionUuid.end(),
                tx_uid_.begin()
            );
            return started_ = true;
        }
        OES_WARNING("Could not start Scoped Transaction. Attempt number {} of {}", attempts, retries + 1);
        if (static_cast<size_t>(retry_interval_ms * factor) > max_interval_ms) {
            factor = 1;
            retry_interval_ms = max_interval_ms;
        }
        ++attempts;
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
        retry_interval_ms = static_cast<size_t>(retry_interval_ms*factor);
    }

    return started_ = false;
}

bool ScopedTransaction::commit(std::string* failure_reason) {
    if (!tx_proto_) {
        return true; // silent no-op
    }
    if (!started_) {
        return false;
    }

    auto rpc = rpcFor(tx_proto_, &proto12::ITransaction::CommitTransaction);
    std::copy(tx_uid_.begin(), tx_uid_.end(), rpc.req_.m_transactionUuid.begin());

    const bool ok = call(rpc, failure_reason);

    if (ok && rpc.res_.m_successful) {
        started_ = false;
    } else if (failure_reason) {
        // the TX is still considered "alive", so one can still call rollback()
        *failure_reason = std::move(rpc.res_.m_failureReason);
    }

    return ok && rpc.res_.m_successful;
}

bool ScopedTransaction::rollback(std::string* failure_reason) {
    if (!tx_proto_) {
        return true; // silent no-op
    }
    if (!started_) {
        return false;
    }

    auto rpc = rpcFor(tx_proto_, &proto12::ITransaction::RollbackTransaction);
    std::copy(tx_uid_.begin(), tx_uid_.end(), rpc.req_.m_transactionUuid.begin());
    started_ = false; // in all cases, the TX is dead

    const bool ok = call(rpc, failure_reason);

    if (!ok && failure_reason) {
        *failure_reason = std::move(rpc.res_.m_failureReason);
    }

    return ok;
}

//============================================================================

} // namespace oes::client
