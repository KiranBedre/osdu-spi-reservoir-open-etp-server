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

#include <algorithm>
#include <atomic>
#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <stdexcept>
#include <unordered_map>
#include <queue>

// work-around for TLS issue on RH6
#ifdef __linux__
#define BOOST_ASIO_DISABLE_SMALL_BLOCK_RECYCLING
#endif

#include <oes/core/utils/config_enter_3rdparty.h>
//#  include <boost/asio/signal_set.hpp>
#  include <avro/Stream.hh>
#  include <avro/Encoder.hh>
#  include <avro/Decoder.hh>
#  include <boost/accumulators/accumulators.hpp>
#  include <boost/accumulators/statistics/stats.hpp>
#  include <boost/accumulators/statistics/count.hpp>
#  include <boost/accumulators/statistics/max.hpp>
#  include <boost/accumulators/statistics/mean.hpp>
#  include <boost/accumulators/statistics/median.hpp>
#  include <boost/accumulators/statistics/min.hpp>
#  include <boost/accumulators/statistics/sum.hpp>
#  include <boost/accumulators/statistics/extended_p_square.hpp>
#  include <boost/algorithm/string/case_conv.hpp>
#  include <boost/algorithm/string/classification.hpp>
#  include <boost/algorithm/string/join.hpp>
#  include <boost/algorithm/string/split.hpp>
#  include <boost/polymorphic_cast.hpp>
#  include <fmt/format.h>
#  include <fmt/ostream.h>
#  include <websocketpp/server.hpp>
#  include <websocketpp/config/asio_no_tls.hpp>
#  include <websocketpp/config/debug_asio_no_tls.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Server.h"
#include "MetricsConf.h"

#include <oes/common/etp12/ErrorCodes.h>
#include <oes/common/etp12/MsgFlags.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/common/etp12/EncoderSender.hpp>
#include <oes/common/etp12/Metadata.h>
#include <oes/common/etp12/Tracer.h>

#include <oes/common/auth/AuthN.h>
#include <oes/common/auth/Entitlement.h>
#include <oes/common/AvroUtils.h>
#include <oes/common/CompressUtils.hpp>
#include <oes/common/QueueUtils.hpp>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Guid.hpp>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsNames.h>
#include <oes/core/utils/String.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/ThreadUtils.hpp>
#include <oes/core/utils/ThousandSep.h>
#include <oes/core/http/HTTP.h>

#include <oes/postgresql/DbConnectionPool.h>

#include <oes/osdu/OsduConnectionProvider.h>

// Transform a compiler macro definition into a string
#define _DEF_VAL(str) #str
#define DEFINE_TO_STRING(str) _DEF_VAL(str)

using oes::core::Guid;
using oes::core::thisThreadId;
using oes::core::thou;

using duration_milli_t = std::chrono::duration<double, std::milli>;
using hires_time_point = std::chrono::high_resolution_clock::time_point;
static constexpr int cache_line_size = 64;  // bytes
static constexpr hires_time_point zero_htp;

/*!
 * \file oes/eml/etp12/Server.h
 * \brief Framework to implement Energistics ETP 1.2 servers.
 *
 * Hides aways all WebSocket and Avro related concerns, exposing
 * a pure and typesafe C++ interface to ETP messages and protocols.
 */

namespace {

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::common;
using namespace oes::common::etp12;
using Datatypes::MessageHeader;
using ProtoEnum = Datatypes::Protocol;
using Datatypes::ServerCapabilities;
using Protocol::EtpCompleteHandler;
using Protocol::EtpResponseHandler;
using Protocol::EtpResponseStatus;
using Protocol::EtpWrappedResponse;
using Protocol::IRequest;
using Protocol::Core::Acknowledge;
using Protocol::Core::ProtocolException;
using Protocol::Core::CloseSession;

//============================================================================

std::atomic_int session_id__ = ATOMIC_VAR_INIT(0);
struct ConnectionInfo {
    ConnectionInfo(
        std::shared_ptr<Session> session,
        std::string label
    ) : id_(++session_id__)
      , label_(std::move(label))
      , session_(std::move(session))
    {
    }

    ConnectionInfo(
        int id,
        std::string label
    ) : id_(id)
      , label_(std::move(label))
    {
    }

    const unsigned id_;
    const std::string label_;
    /*const*/ std::shared_ptr<Session> session_; // MSVC failure...
    // TODO: Time created, for 1h "reconnection" spec

    size_t calls_ = 0;
};
const ConnectionInfo no_info2(0, "Unknown");

struct LostConnectionSession {
    LostConnectionSession() = delete;
    LostConnectionSession& operator=(LostConnectionSession& other) = delete;

    explicit LostConnectionSession(
        std::string id,
        ConnectionInfo&& connection_info
    ) : id_(std::move(id))
      , conn_info_ (std::move(connection_info))
      , disconnect_time_(std::chrono::steady_clock::now()) {}

    bool isExpired(std::chrono::seconds& sec) const {
        auto now = std::chrono::steady_clock::now();
        return (now - disconnect_time_) > sec;
    }

    const std::string id_;
    ConnectionInfo conn_info_;
    std::chrono::steady_clock::time_point disconnect_time_;
};

/**
 * @brief Struct to support expired session cleanup jobs
 * 
 */
struct ExpiredSessionJob {
    std::string session_uuid;
    std::chrono::steady_clock::time_point when;
    std::function<void()> fn;

    bool operator<(const ExpiredSessionJob& other) const {
        return when < other.when;
    }
};
using JobQueue = std::priority_queue<ExpiredSessionJob, std::vector<ExpiredSessionJob>, std::less<>>;

using ws_server_t = websocketpp::server<websocketpp::config::asio>;
using ws_message_t = ws_server_t::message_ptr;
using ws_conn_hdl_t = websocketpp::connection_hdl;
using ws_conn_ptr_t = ws_server_t::connection_ptr;
using ws_conn_hdl_map_t = std::map<
    ws_conn_hdl_t, ConnectionInfo, std::owner_less<ws_conn_hdl_t>
>;
using lost_conn_session_map_t = std::map<
    std::string, LostConnectionSession
>;

//============================================================================

struct CoreProtoImpl final : public Protocol::ICore {
    std::weak_ptr<Session> session_;

    explicit CoreProtoImpl(std::weak_ptr<Session> session) : session_(session) {}

    void RequestSession(
        IRequest& ireq, RequestSession_req_t req,
        Protocol::EtpResponseHandler<RequestSession_res_t> process_cb
    ) override;

    void CloseSession(
        IRequest& ireq, CloseSession_req_t req,
        EtpCompleteHandler done_cb
    ) override;

    void Authorize(
        IRequest& ireq, Authorize_req_t req,
        EtpResponseHandler<Authorize_res_t> process_cb
    ) override;

    void Ping(
        IRequest& ireq, Ping_req_t req,
        EtpResponseHandler<Ping_res_t> process_cb
    ) override;
};

//============================================================================

struct CoreOSDUProtoImpl final: public Protocol::ICoreOSDU {
    std::weak_ptr<Session> session_;

    explicit CoreOSDUProtoImpl(std::weak_ptr<Session> session) : session_(session) {}

    void ResumeSession(
        IRequest& ireq, ResumeSession_req_t req,
        EtpResponseHandler<ResumeSession_res_t> process_cb
    ) override;
};

//============================================================================

inline std::ostream& operator<<(std::ostream& os, const ConnectionInfo& conn_info) {
    return os << fmt::format(
        "Session({}) on Thread({}), {}", conn_info.id_, thisThreadId(), conn_info.label_
    );
}

//============================================================================

void pingPong(
    int64_t& client_currentDateTime,
    int64_t& server_currentDateTime
) {
    using namespace std::chrono;
    using store_tstamp = time_point<system_clock, microseconds>;

    duration<int64_t> client_dur(client_currentDateTime);
    store_tstamp client_ts(client_dur);

    store_tstamp server_ts = time_point_cast<microseconds>(system_clock::now());
    server_currentDateTime = server_ts.time_since_epoch().count();

    duration<double> diff = client_ts - server_ts;

    OES_INFO(
        "SVR Client Ping Timestamp: {}us", thou(client_currentDateTime));
    OES_INFO(
        "SVR Server Pong Timestamp: {}us", thou(server_currentDateTime));
    //OES_INFO(oes::core::Format("SVR Client-Server skew: {}us", thou(diff.count())));
}

//============================================================================

void CoreProtoImpl::RequestSession(
    IRequest&, RequestSession_req_t req,
    EtpResponseHandler<RequestSession_res_t> process_cb
) {
    Session* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    EtpResponseStatus rc;
    RequestSession_res_t res;
    try {
        session->onStart(std::move(req), res);
        rc.ok = true;
    } catch (proto12::ProtocolException& ex) {
        rc.ex = std::move(ex);
    } catch (const std::exception& ex) {
        Datatypes::ErrorInfo info;
        info.m_code = data12::EINVALID_STATE;
        info.m_message = "Generic Exception: ";
        if (const char* msg = ex.what()) {
            info.m_message += msg;
        } else {
            info.m_message += "no message";
        }
        rc.ex.m_error = std::move(info);
    } catch (...) {
        Datatypes::ErrorInfo info;
        info.m_code = data12::EINVALID_STATE;
        info.m_message = "Unknown Exception!";
        rc.ex.m_error = std::move(info);
    }

    if(rc.ok){
        pingPong(req.m_currentDateTime, res.m_currentDateTime);
    }

    process_cb(rc, res);
}

void CoreProtoImpl::CloseSession(
    IRequest&, CloseSession_req_t req,
    EtpCompleteHandler done_cb
) {
    Session* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    EtpResponseStatus rc;
    if(session->isClosed()){
        Datatypes::ErrorInfo info;
        info.m_code = data12::EINVALID_STATE;
        info.m_message = "Session not opened.";
        rc.ex.m_error = std::move(info);
    } else {
        rc.ok = true;
        session->close(std::move(req.m_reason), false);
    }
    done_cb(rc);
}

void CoreProtoImpl::Authorize(
    IRequest&, Authorize_req_t req,
    EtpResponseHandler<Authorize_res_t> process_cb
) {
    Session* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    EtpResponseStatus rc;
    Authorize_res_t res;
    try {
        session->onAuthN(std::move(req.m_authorization));
        rc.ok = true;
        res.m_success = true;
    } catch (proto12::ProtocolException& ex) {
        rc.ex = std::move(ex);
    } catch (const std::exception& ex) {
        Datatypes::ErrorInfo info;
        info.m_code = data12::EINVALID_STATE; // what else?
        info.m_message = "Generic Exception: ";
        if (const char* msg = ex.what()) {
            info.m_message += msg;
        } else {
            info.m_message += "no message";
        }
        rc.ex.m_error = std::move(info);
    } catch (...) {
        Datatypes::ErrorInfo info;
        info.m_code = data12::EINVALID_STATE; // what else?
        info.m_message = "Unknown Exception!";
        rc.ex.m_error = std::move(info);
    }
    // TODO set the challenges field when token is empty. See ETP specs.
    process_cb(rc, res);
}

void CoreProtoImpl::Ping(
    IRequest&, Ping_req_t req,
    EtpResponseHandler<Ping_res_t> process_cb
) {
    EtpResponseStatus rc;
    rc.ok = true;
    Ping_res_t res;

    pingPong(req.m_currentDateTime, res.m_currentDateTime);

    process_cb(rc, res);

    try {
        oes::core::Metrics::instance().getCounter("ping_pong_counter").Increment();
    } catch (const oes::core::MetricNotFound& e) {
        OES_WARNING("Not registered metric. Reason: {}", e.what());
    }
}

//============================================================================

std::unique_ptr<EtpTracer12> setupFullEtp12Tracing(std::string trace_file) {
    EtpTraceHeader file_header = EtpTraceHeader::make();
    trace_file = EtpTracer12::trace_filename(trace_file, &file_header);
    OES_INFO("Tracing to file {}", trace_file);
    return std::unique_ptr<EtpTracer12>(new EtpTracer12(trace_file, file_header));
}

//============================================================================

namespace acc = boost::accumulators;

struct TimeStats {
    using TimeAcc = acc::accumulator_set<
        double, acc::stats<
            acc::tag::min,
            acc::tag::max,
            acc::tag::mean,
            acc::tag::median,
            acc::tag::sum,
            acc::tag::extended_p_square
        >
    >;

    static constexpr double probabilities[] { 0.90, 0.99 };

    TimeAcc accumulator_;

    TimeStats(
    ) : accumulator_(acc::extended_p_square_probabilities = probabilities)
    {
    }

    size_t count() const {
        return acc::count(accumulator_);
    }
    double sum() const {
        return acc::sum(accumulator_);
    }
    double min() const {
        return acc::min(accumulator_);
    }
    double max() const {
        return acc::max(accumulator_);
    }
    double mean() const {
        return acc::mean(accumulator_);
    }
    double median() const {
        // somehow median() returns 0 when having only two values
        return (count() == 2)? mean(): acc::median(accumulator_);
    }
    double p90() const {
        return acc::extended_p_square(accumulator_)[0];
    }
    double p99() const {
        return acc::extended_p_square(accumulator_)[1];
    }

    void add(double millis) {
        accumulator_(millis);
    }

    std::string to_string() const {
        return fmt::format(
            "(min: {:.1f}; med: {:.1f}; avg: {:.1f};"
            " p90: {:.1f}; p99: {:.1f}; max: {:.1f} milliseconds)",
            this->min(), this->median(), this->mean(),
            this->p90(), this->p99(),    this->max()
        );
    }
};

struct ProcTopN {
    using Record_t = std::pair<double, size_t>;
    using Detail_t = std::pair<double, std::string>;

    using Queue_t = std::priority_queue<
        Record_t, std::vector<Record_t>, std::greater<Record_t>
    >;

    size_t max_count_;

    Queue_t topN_;
    std::vector<Detail_t> details_;

    explicit ProcTopN(
        size_t topN = 5
    ) : max_count_(topN)
    {
        details_.reserve(topN);
    }

    // Use returned pointer to write to ->second some label that describes
    // that recorded event. That way, we only need to compute that label
    // for the subset of events that made it (perhaps temporarily) in the TopN
    Detail_t* add(double millis) {
        // until all slots are filled, unconditionally insert
        if (topN_.size() < max_count_) {
            topN_.emplace(millis, details_.size());
            Detail_t& back = details_.emplace_back(millis, "");

            assert(topN_.size() == details_.size()); // invariant

            return &back;
        }

        // insert only if new duration exceeds the current worse (smallest) one
        if (millis > topN_.top().first) {
            // Get back the details info for the slot being "evicted"
            size_t idx = topN_.top().second;
            assert(idx < details_.size());
            Detail_t& curr = details_[idx];
            assert(curr.first == topN_.top().first);

            // evict the slot, and update its duration
            topN_.pop();
            topN_.emplace(millis, idx);
            curr.first = millis;

            assert(topN_.size() == details_.size()); // invariant

            return &curr;
        }

        return nullptr;
    }

    std::vector<Detail_t> topN() const {
        std::vector<Detail_t> res(details_);
        std::sort(res.begin(), res.end(), std::greater<>());
        return res;
    }
};

struct SizeStats {
    using StatsAcc = acc::accumulator_set<
        size_t, acc::stats<
            acc::tag::min,
            acc::tag::max,
            acc::tag::mean,
            acc::tag::median,
            acc::tag::sum
        >
    >;

    StatsAcc accumulator_;

    size_t count() const {
        return acc::count(accumulator_);
    }
    size_t sum() const {
        return acc::sum(accumulator_);
    }
    size_t min() const {
        return acc::min(accumulator_);
    }
    size_t max() const {
        return acc::max(accumulator_);
    }
    size_t mean() const {
        return static_cast<size_t>(acc::mean(accumulator_));
    }
    size_t median() const {
        // somehow median() returns 0 when having only two values
        return (count() == 2)? mean(): static_cast<size_t>(acc::median(accumulator_));
    }

    void add(size_t msg_len) {
        accumulator_(msg_len);
    }

    std::string to_string() const {
        using oes::core::thou;
        return fmt::format(
            "(min: {}; med: {}; avg: {}; max: {} bytes)",
            thou(this->min()), thou(this->median()),
            thou(this->mean()), thou(this->max())
        );
    }
};

//============================================================================

std::string log_error(
    unsigned tid, int sid, bool partial,
    const MessageHeader& hdr, const ProtocolException& ex, char dir,
    const std::string& app_identification
) {
    assert(hdr.m_messageType == ex.info.type_.type_);

    std::string msg;

    const bool is_scalar = ex.m_error.has_value();
    const size_t array_err_count = ex.m_errors.size();
    const bool is_array = (array_err_count > 0);

    // Normally it's either ex.m_error or ex.m_errors, not both (or neither)
    assert(is_scalar != is_array);

    // FIXME: Always prints Core(0).ProtocolException(1000), when
    //        the protocol may be different
    const auto& label = ProtocolException::info.label_;

    // TODO: In case err.m_message (see both branches below) is
    //       newline-terminated, or multi-line, or just too long,
    //       print only the first N characters, on a single line.

    if (is_scalar) {
        // partial should associated to array errors only
        assert(!partial);

        const auto& err = *ex.m_error;

        msg = fmt::format(
            "[{}] SVR {}ERR Thread({}) Session({}) Msg({})_PMsg({}) {}scalar {}\n\tETP-{} {}",
            app_identification, dir, tid, sid, hdr.m_messageId, hdr.m_correlationId,
            partial? "partial ": "", label,
            err.m_code, err.m_message // pretty-print code?
        );
    }

    if (is_array) {
        // we show the first error only
        const auto& err = ex.m_errors.begin()->second;

        msg = fmt::format(
            "[{}] SVR {}ERR Thread({}) Session({}) Msg({})_PMsg({}) {}array[{}] {}\n\tETP-{} {}",
            app_identification, dir, tid, sid, hdr.m_messageId, hdr.m_correlationId,
            partial? "partial ": "", array_err_count, label,
            err.m_code, err.m_message // pretty-print code?
        );
    }

    assert(!msg.empty());
    return msg;
}

inline
auto now() noexcept {
    return std::chrono::high_resolution_clock::now();
}

//============================================================================

struct RepliesState {
    size_t res_count_ = 0;
    size_t res_bytes_ = 0;
    hires_time_point res_ttfr_; // Time-to-first response (i.e. latency)
    hires_time_point res_ttlr_; // Time-to-last response

    duration_milli_t proc_ms_ = std::chrono::milliseconds(0);
    unsigned proc_tid_ = 0;

    void add(size_t res_bytes, hires_time_point sent_at) {
        res_bytes_ += res_bytes;
        ++res_count_;

        if (sent_at > zero_htp) {
            if (res_ttfr_ == zero_htp) {
                res_ttfr_ = res_ttlr_ = sent_at;
            } else {
                res_ttlr_ = sent_at;
            }
        }
    }
};

//============================================================================

struct PendingRpc;
using PendingRpc_ptr = std::shared_ptr<PendingRpc>;

//============================================================================

struct RecvState {
    int64_t last_inbound_msg_id_ = 0;   // Client request IDs must be even, and > 0
    hires_time_point last_req_recv_;

    TimeStats recv_time_ms_;
    TimeStats infl_time_ms_;
    SizeStats bytes_recv_;
    SizeStats body_usize_;
    size_t header_bytes_ = 0;
};

struct ProcState {
    TimeStats proc_time_ms_;
    ProcTopN proc_topN_ms_;

    void add(duration_milli_t millis) {
        proc_time_ms_.add(millis.count());
    }

    void maybeAddToTopN(PendingRpc& rpc, unsigned sid);
};

struct SendState {
    MsgId last_outbound_msg_id_{MsgKind::eResponse};
    hires_time_point last_res_sent_;

    TimeStats send_time_ms_;
    TimeStats defl_time_ms_;
    SizeStats bytes_sent_;
    SizeStats body_usize_;
    size_t header_bytes_ = 0;
    int64_t errors_sent_ = 0; // i.e. ProtocolException
};

//============================================================================

// A request which hasn't been fully processed and responded to, yet
struct PendingRpc final : Protocol::IRequest {
    std::string raw_msg_; // Warning: .clear()'d on fully decoded
    bool txt_msg_ = false;

    size_t req_bytes_ = 0;
    MessageHeader req_hdr_;
    ::avro::DecoderPtr decoder_;
    StringAsAvroIStreamView buf_;
    int expected_response_type_ = 0;

    const MsgInfo* msg_info_ = nullptr;
    AbstractMessage_uptr req_body_;

    hires_time_point sock_at_ = now(); // when we got the socket callback

    void init();
    void init_body(std::string& uncompressed_body);

    std::string decode_header();
    std::string decode_request();
    void mark_fully_decoded();

private:
    std::mutex replies_mt_;
    alignas(cache_line_size) RepliesState replies_; // must lock send_mt_ to access

public:
    // Reply: Either Acknowledge, ProtocolException, or an actual Response
    void add_reply(size_t res_bytes, hires_time_point sent_at);
    void set_proc_time(duration_milli_t proc, unsigned tid);
    RepliesState replies_state() /*const*/;

public: // Protocol::IRequest interface
    const MessageHeader& header() const override {
        return req_hdr_;
    }

    const Datatypes::MessageHeaderExtension* headerExt() const override {
        return nullptr; // TODO
    }

    AbstractMessage_uptr takeBody() override {
        assert(req_body_);
        return std::move(req_body_);
    }

    bool hasChunks() const override {
        return false; // TODO
    }
    std::vector<Protocol::Store::Chunk> takeChunks() override {
        return {}; // TODO
    };
    bool retainBodyNeedingChunks(AbstractMessage_uptr) override {
        return false; // TODO
    };
};

void PendingRpc::init() {
    assert(!raw_msg_.empty());
    req_bytes_ = raw_msg_.size();
    buf_.init(raw_msg_);

    if (!decoder_) {
        decoder_ = ::avro::binaryDecoder(); // TODO: JSON
    }
    decoder_->init(buf_);
}

void PendingRpc::init_body(std::string& uncompressed_body) {
    assert(!raw_msg_.empty());
    assert(!uncompressed_body.empty());
    assert(decoder_);
    assert(req_bytes_);

    // req_bytes_ remains unchanged
    // we lose the encoded bytes of the (already decoded) header, but that fine.
    raw_msg_.swap(uncompressed_body);
    raw_msg_.shrink_to_fit(); // FIXME: assign instead of swap+shrink_to_fit?
    buf_.init(raw_msg_);
    decoder_->init(buf_);
}

std::string PendingRpc::decode_header() try {
    req_hdr_.decode(*decoder_);
    msg_info_ = MsgInfo::lookup(req_hdr_.m_protocol, req_hdr_.m_messageType);
    return {};
} catch (const std::exception& ex) {
    return ex.what();
} catch (...) {
    return "Unknown error";
}

std::string PendingRpc::decode_request() try {
    assert(msg_info_);
    req_body_ = msg_info_->factory_();  // TODO: Reuse prev, if the same? How to clear it?
    req_body_->decode(*decoder_);       //       Same consecutive request type very common
    return {};
} catch (const std::exception& ex) {
    return ex.what();
} catch (...) {
    return "Unknown error";
}

void PendingRpc::mark_fully_decoded() {
    raw_msg_.clear(); // req_bytes_ retains raw_msg_'s size though
    buf_.reset(); // but we reuse decoder_
}

void PendingRpc::add_reply(size_t res_bytes, hires_time_point sent_at) {
    std::lock_guard lock(replies_mt_);
    replies_.add(res_bytes, sent_at);
}

void PendingRpc::set_proc_time(duration_milli_t proc, unsigned tid) {
    std::lock_guard lock(replies_mt_);
    replies_.proc_ms_ = proc;
    replies_.proc_tid_ = tid;
}

RepliesState PendingRpc::replies_state() /*const*/ {
    std::lock_guard lock(replies_mt_);
    return replies_;
}

//============================================================================

void ProcState::maybeAddToTopN(
    PendingRpc& rpc, unsigned sid
) {
    const auto replies = rpc.replies_state();

    if (auto* topN = proc_topN_ms_.add(replies.proc_ms_.count())) {
        const duration_milli_t ttfr =
            (replies.res_ttfr_ > zero_htp)?
                replies.res_ttfr_ - rpc.sock_at_:
                duration_milli_t();

        topN->second = fmt::format(
            "Thread({}) Session({}) {} ({} + {} x{}) @{:0.1f}ms",
            replies.proc_tid_, sid, msgHeaderAsString(rpc.req_hdr_),
            thou(rpc.req_bytes_), thou(replies.res_bytes_),
            replies.res_count_, ttfr.count()
        );
        if (replies.res_ttlr_ > replies.res_ttfr_) {
            duration_milli_t ttlr = replies.res_ttlr_ - rpc.sock_at_;
            topN->second += fmt::format(" - @{:0.1f}ms", ttlr.count());
        }
    }
}

//============================================================================

class PendingRpcs {
public:
    PendingRpcs() {}
    ~PendingRpcs() {}

    bool empty() const { return size() == 0; }
    size_t size() const { return pending_rpcs_.size(); }

    PendingRpc_ptr acquire_rpc();
    bool release_rpc(const PendingRpc_ptr& rpc);

    PendingRpc_ptr find_parent_rpc(int64_t correlation_id);

 private:
    std::deque<PendingRpc_ptr> pending_rpcs_;
};

// Warning: Must not be called concurrently, but serially.
PendingRpc_ptr PendingRpcs::acquire_rpc() {
    PendingRpc_ptr rpc(new PendingRpc);
    pending_rpcs_.push_back(rpc);
    return rpc;
}

bool PendingRpcs::release_rpc(const PendingRpc_ptr& rpc) {
    OES_ASSERT(rpc, return false;);
    
    if (rpc == pending_rpcs_.back()) {
        pending_rpcs_.pop_back();
    } else if (rpc == pending_rpcs_.front()) {
        pending_rpcs_.pop_front();
    }  else {
        auto itr = std::remove(pending_rpcs_.begin(), pending_rpcs_.end(), rpc);
        if (itr == pending_rpcs_.end()) {
            return false;
        }
        pending_rpcs_.erase(itr, pending_rpcs_.end());
    } 
    return true;
}

PendingRpc_ptr PendingRpcs::find_parent_rpc(int64_t correlation_id) {
    PendingRpc_ptr p_rpc;
    for (auto rpc_it = pending_rpcs_.begin(); rpc_it != pending_rpcs_.end(); ++rpc_it) {
        PendingRpc_ptr found_rpc_it = *rpc_it;
        if( !found_rpc_it ){
            continue;
        }
        if (correlation_id == found_rpc_it->req_hdr_.m_messageId) {
            p_rpc = found_rpc_it;
            break;
        }
    }
    return p_rpc;
}

//============================================================================

template <typename T, typename U>
bool same_owner(std::enable_shared_from_this<T>& t, const std::shared_ptr<U>& rhs) {
    auto lhs = t.weak_from_this();
    const bool owner_before_lhs = lhs.owner_before(rhs);
    const bool owner_before_rhs = rhs.owner_before(lhs);
    const bool same_owner = !owner_before_lhs && !owner_before_rhs;
    return same_owner;
}

} // anonymous


// Support formatting for custom types (can't be defined in the anonimous namespace)
template <> struct fmt::formatter<ConnectionInfo> : ostream_formatter {};

//============================================================================
//============================================================================

namespace oes::eml::etp12 {
// The built-in private state of a session
struct Session::State : std::enable_shared_from_this<Session::State> {
    State() {
        // No need to lock recv_mt_, since in Ctor
        recv_.last_req_recv_ = started_;
    }

    State& operator=(State& other);

    enum class Status { 
        Initial, 
        Open,
        AboutToClose,
        Closed 
    };
    Status status_ = Status::Initial;

    std::condition_variable rpc_cv_;

    const hires_time_point started_ = now();
    hires_time_point rpc_proc_at_;

    // mutexes and the respective state they protect, forced-aligned to 64-byte
    // boundaries, to ensure no https://en.wikipedia.org/wiki/False_sharing.
    std::mutex recv_mt_;
    std::mutex send_mt_;
    std::mutex proc_mt_;
    std::mutex rpcs_mt_;

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4324)  // '<...>': structure was padded due to alignment specifier
#endif
    alignas(cache_line_size) RecvState recv_;   // must lock recv_mt_ to access
    alignas(cache_line_size) SendState send_;   // must lock send_mt_ to access
    alignas(cache_line_size) ProcState proc_;   // must lock proc_mt_ to access
    alignas(cache_line_size) std::unique_ptr<PendingRpcs> rpcs_;
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif

    int sid_ = 0;
    bool is_avro_json_ = false;
    size_t max_msg_size_ = 64*1024; // Just in case not negotiated fast enough
    size_t max_msg_usize_ = 0;
    size_t max_batch_size_ = 0;
    const ZCompressor* zcompressor_ = nullptr;
    std::string client_identification_ = "";

    std::weak_ptr<Session> session_back_pointer_;

    std::weak_ptr<Session> get_session() const {
        return session_back_pointer_;
    }

    Impl_wptr wimpl_;
    std::weak_ptr<void> wconn_;
    std::unique_ptr<boost::asio::io_context::strand> recv_strand_;
    std::unique_ptr<boost::asio::io_context::strand> proc_strand_;

    void init(
        std::weak_ptr<Session> session, const Impl_ptr& impl,
        const ws_conn_hdl_t& hdl, boost::asio::io_service& asio_service,
        boost::asio::io_service& worker_service
    );
    void init_negotiated(const Session& session);

    PendingRpc_ptr acquire_rpc();
    void release_rpc(const PendingRpc_ptr& rpc, bool temporary_rpc = false);

    void send(
        const PendingRpc_ptr& p_rpc,
        MessageHeader& hdr, EtpWrappedResponse wres
    );
    void log_and_trace_post_send(
        ws_conn_ptr_t& conn,
        EtpTracer12* tracer,
        const PendingRpc_ptr& p_rpc,
        const MessageHeader& hdr,
        const EncodeAndSendRc& rc,
        EtpTracerEventInfo& evt
    );

    void send_error(
        const PendingRpc_ptr& p_rpc,
        MessageHeader& ex_hdr, const ProtocolException& ex,
        bool partial_success = false
    );
    void post_send_error(
        const PendingRpc_ptr& rpc,
        int32_t err_code, std::string err_msg
    );

    void send_acknowledge(const PendingRpc_ptr& p_rpc, MessageHeader& ack_hdr);
    void post_send_acknowledge(const PendingRpc_ptr& rpc);

    using EtpResponse = std::pair<EtpResponseStatus, EtpWrappedResponse>;
    using EtpResponse_ptr = std::shared_ptr<EtpResponse>;

    void handle_msg(const PendingRpc_ptr& p_rpc);
    void handle_req(const PendingRpc_ptr& p_rpc, proto12::IProtocol_ptr proto);
    void handle_res(const PendingRpc_ptr& p_rpc, const EtpResponse_ptr& p_res);

    /*!
     * \returns true if message header type and protocol is Store::Chunk
     */
    bool is_chunk_msg(const MessageHeader& hdr) const {
        return (hdr.m_protocol == Protocol::Store::Chunk::info.type_.proto_ &&
            hdr.m_messageType == Protocol::Store::Chunk::info.type_.type_);
    }

    /*!
     * \returns true if message header type and protocol is Store::PutDataObjectsResponse
     */
    bool is_put_obj_res_msg(const MessageHeader& hdr) const {
        return (hdr.m_protocol == Protocol::Store::PutDataObjectsResponse::info.type_.proto_ &&
            hdr.m_messageType == Protocol::Store::PutDataObjectsResponse::info.type_.type_);
    }

    /*!
     * \returns true if message header type and protocol is Store::PutDataObjects
     */
    bool is_put_obj_msg(const MessageHeader& hdr) const {
        return (hdr.m_protocol == Protocol::Store::PutDataObjects::info.type_.proto_ &&
            hdr.m_messageType == Protocol::Store::PutDataObjects::info.type_.type_);
    }
};

class Impl : public std::enable_shared_from_this<Impl> {
public:
    const Guid server_uid_ = Guid::make();
    oes::core::TracingMutex<std::mutex> config_mt_;
    const ServerConfig* config_ = nullptr;

    std::unique_ptr<EtpTracer12> tracer_;

    oes::core::TracingSharedMutex<std::shared_timed_mutex> ws_server_mt_;
    ws_server_t ws_server_;
    ws_conn_hdl_map_t ws_conn_info_map2_;
    oes::core::ThreadJoiner thread_joiner_;
    std::promise<int> server_port_promise_;
    
    lost_conn_session_map_t ws_lost_sessions_map_;
    std::mutex lost_sessions_mt_;

    std::unique_ptr<boost::asio::steady_timer> cleanup_timer_;
    JobQueue cleanup_lost_sessions_jobs_;
    std::mutex cleanup_timer_jobs_mt_;
    
    ws_conn_ptr_t get_conn(const ws_conn_hdl_t& hdl);
    ws_conn_ptr_t get_conn_no_lock(const ws_conn_hdl_t& hdl);

    const ConnectionInfo& get_conn_info(const ws_conn_hdl_t& hdl);
    const ConnectionInfo& get_conn_info_no_lock(const ws_conn_hdl_t& hdl);

    Impl();

    bool start(const ServerConfig& config, bool& running);

    // in the order they are called by WebSocket clients
    bool on_validate(ws_conn_hdl_t hdl);
    void on_open(ws_conn_hdl_t hdl);
    void on_message(ws_conn_hdl_t hdl, ws_message_t msg);
    void on_close(ws_conn_hdl_t hdl);
    void on_fail(ws_conn_hdl_t hdl);

    // to discover ETP server "capabilities"
    void on_http(ws_conn_hdl_t hdl);

    void adjustProtocolCapabilities(Datatypes::SupportedProtocol& proto, const Datatypes::SupportedProtocol& server_proto) const;
    proto12::IProtocol_ptr makeProtocol(ProtoEnum protoId, const Session::State& session) const;

    void schedule_cleanup_expired_sessions_job();

    boost::asio::io_service work_svc_;
    boost::asio::io_service::work work_;
};

Impl::Impl() : work_svc_(), work_(work_svc_) {
}

// assumes ws_server_mt_ is locked
ws_conn_ptr_t Impl::get_conn_no_lock(const ws_conn_hdl_t& hdl) {
    return ws_server_.get_con_from_hdl(hdl);
}

ws_conn_ptr_t Impl::get_conn(const ws_conn_hdl_t& hdl) {
    std::shared_lock sguard(ws_server_mt_); // read-only
    return get_conn_no_lock(hdl);
}

// assumes ws_server_mt_ is locked
const ConnectionInfo& Impl::get_conn_info_no_lock(const ws_conn_hdl_t& hdl) {
    auto found = ws_conn_info_map2_.find(hdl);
    if (found != ws_conn_info_map2_.end()) {
        return found->second;
    }
    return no_info2;
}

const ConnectionInfo& Impl::get_conn_info(const ws_conn_hdl_t& hdl) {
    std::shared_lock sguard(ws_server_mt_); // read-only
    return get_conn_info_no_lock(hdl);
}

bool Impl::start(const ServerConfig& config, bool& running) {
    OES_ASSERT(0 <= config.port_ && config.port_ < (1<<16), return false;);

    setUpPrometheus();

    try {
        if (config.enable_tracing_) try {
            tracer_ = setupFullEtp12Tracing(config.trace_file_);
            if (tracer_) {
                tracer_->start();
            }
        } catch (const std::exception& ex) {
            OES_ERROR("Cannot open trace file: {}", ex.what());
            return false;
        }

        std::unique_lock xguard(ws_server_mt_); // read-write

        // Configure internal logging of WebSocketsPP library
        if (core::Logger::instance().minSeverityFilter() <= core::Message::Severity::debug)
        {
            ws_server_.set_access_channels(websocketpp::log::alevel::none);
            ws_server_.set_access_channels( // impl does |= then "clear" above
                websocketpp::log::alevel::access_core //| // i.e. Connect, Disconnect, Fail, and HTTP
                //websocketpp::log::alevel::frame_header |
                //websocketpp::log::alevel::frame_payload |
                //websocketpp::log::alevel::debug_handshake |
                //websocketpp::log::alevel::debug_close
            );
        }
        else
        {
            ws_server_.set_access_channels(websocketpp::log::alevel::none);
            ws_server_.set_error_channels(websocketpp::log::alevel::none);
        }

        ws_server_.init_asio();

        const auto mainThreadId = thisThreadId();
        OES_INFO( "Main Thread {}.", mainThreadId );


        /*
        ** Attempt at properly "stopping" ETP server on CTRL-C. Does not work :(
        boost::asio::signal_set signals(ws_server_.get_io_service(), SIGINT, SIGTERM);
        signals.async_wait(
            [](const boost::system::error_code& ec, int sig) {
                OES_INFO(
                    "ETP Server Stopped via signal ({}): {}"
                    , sig, ec.message()
                );
                exit(1);
            }
        );
        */

        // HTTP-related
        ws_server_.set_http_handler(
            [this](ws_conn_hdl_t hdl) {
                on_http(std::move(hdl));
            }
        );

        // WebSocket-related
        ws_server_.set_message_handler(
            [this](ws_conn_hdl_t hdl, ws_message_t msg) {
                on_message(std::move(hdl), std::move(msg));
            }
        );
        ws_server_.set_validate_handler(
            [this](ws_conn_hdl_t hdl) {
                return on_validate(std::move(hdl));
            }
        );
        ws_server_.set_open_handshake_timeout(30000); // 30s
        ws_server_.set_open_handler(
            [this](ws_conn_hdl_t hdl) {
                return on_open(std::move(hdl));
            }
        );
        ws_server_.set_close_handler(
            [this](ws_conn_hdl_t hdl) {
                return on_close(std::move(hdl));
            }
        );
        // when killing the client, via `taskkill /IM oes_dxx.exe /F`
        // on Windows for example, on_close() is called, not on_fail().
        ws_server_.set_fail_handler(
            [this](ws_conn_hdl_t hdl) {
                return on_fail(std::move(hdl));
            }
        );

        ws_server_.set_reuse_addr(config.reuseAddressSocket_);

        boost::asio::ip::tcp tcp_v = config.useIpV4_?
            boost::asio::ip::tcp::v4():
            boost::asio::ip::tcp::v6()
        ;

        int port = config.port_;
        ws_server_.listen(tcp_v, static_cast<uint16_t>(port));

        if (port == 0) {
            websocketpp::lib::asio::error_code ec;
            websocketpp::lib::asio::ip::tcp::endpoint svr_endpoint = ws_server_.get_local_endpoint(ec);
            if (ec) {
                OES_ERROR("Cannot determine server port: {}", ec.message());
                return false;
            }
            port = svr_endpoint.port();
        }

        OES_INFO(
            "ETP Server Listening on {}port {}.\n",
            (config.port_? "": "(random) "), port
        );

        // Start the server accept loop
        ws_server_.start_accept();

        xguard.unlock();

        // Start the ASIO io_service run loop
        const unsigned int thread_count = (config.totalThreadCount_ == 0)?
            std::thread::hardware_concurrency():
            config.totalThreadCount_
        ;
        if (thread_count > 1) {
            
            bool skip_check {config.enable_any_number_of_threads_};

            const unsigned int actual_worker_threads = skip_check ? thread_count : std::min<>(
                std::thread::hardware_concurrency(), thread_count
            );
            if (thread_count != actual_worker_threads) {
                OES_WARNING(
                    "Using {} threads; not {} as requested",
                    actual_worker_threads, thread_count
                );
            }
            OES_VERBOSE(
                "SVR Starting {} additional worker thread(s)",
                (actual_worker_threads - 1) // minus "this" thread (ws_server_.run() below)
            );
            if (skip_check) {
                OES_WARNING("SVR Using any number of threads. Using {} threads.", actual_worker_threads);
            }
            boost::asio::io_service& work_svc = actual_worker_threads>1 ? 
                work_svc_ : ws_server_.get_io_service();
            for (unsigned int i = 1; i < actual_worker_threads; ++i) {
                thread_joiner_.threads_.emplace_back([&work_svc]() {
                    work_svc.run(); // start a ASIO work-thread
                });
            }
        }

        // We fulfill the port-promise just before starting, which means
        // the client could wake-up before we are really "ready". Not too
        // sure how to postpone that properly, post .run() below...
        server_port_promise_.set_value(port);

        // This call blocks, so worker threads need to be started before.
        running = true;
        const size_t handler_execd_count = ws_server_.run();
        running = false;

        OES_VERBOSE(
            "SVR stopped. Served {} calls",
            handler_execd_count
        );
        return true;
    } catch (const std::exception& ex) {
        OES_ERROR("Unable to start server: {}", ex.what());
    } catch (websocketpp::lib::error_code& ec) {
        OES_ERROR("Unable to start server: {}", ec.message());
    } catch (...) {
        OES_ERROR("Unable to start server: Unknown exception");
    }
    running = false;
    return false;
}

bool Impl::on_validate(ws_conn_hdl_t hdl) {
    try {
        oes::core::Metrics::instance().getCounter("any_message_counter").Increment();
    } catch (const oes::core::MetricNotFound& e) {
        OES_WARNING("Not registered metric. Reason: {}", e.what());
    }

    const hires_time_point start = now();

    ws_conn_ptr_t conn;
    try {
        conn = get_conn(hdl);
    } catch (const std::exception& ex) {
        OES_ERROR(
            "SVR KO: CONNECTION REFUSED Thread({}) Reason: {}",
            thisThreadId(), ex.what()
        );
        return false;
    }

    Handshake shaker;
    shaker.secure_          = conn->is_secure();
    shaker.host_            = conn->get_host();
    shaker.port_            = conn->get_port();
    shaker.resource_        = conn->get_resource();
    shaker.remote_endpoint_ = conn->get_remote_endpoint();
    shaker.origin_          = conn->get_origin();

    if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::verbose) {
        OES_VERBOSE("SVR ===========");
        OES_VERBOSE(
            "SVR on_validate({}://{}:{}{} from {})",
            (shaker.secure_? "wss": "ws"), shaker.host_, shaker.port_,
            shaker.resource_, shaker.remote_endpoint_
        );

        const std::vector<std::string>& subprotos = conn->get_requested_subprotocols();
        switch (const size_t n = subprotos.size()) {
        case  0:
            OES_VERBOSE("SVR\tNo WebSocket sub-protocols requested");
            break;
        case  1:
            OES_VERBOSE(
                "SVR\t1 WebSocket sub-protocol requested: {}", subprotos[0]
            );
            break;
        default:
            OES_VERBOSE(
                "SVR\t{} WebSocket sub-protocol{} requested: {}",
                n, (n == 1? "": "s"), boost::join(subprotos, ", ")
            );
            break;
        }
    }

    websocketpp::lib::error_code ec;
    conn->select_subprotocol("etp12.energistics.org", ec);
    if (ec) {
        conn->set_status(
            websocketpp::http::status_code::precondition_failed,
            "Connection Refused: Client not requesting etp12.energistics.org subprotocol."
        );
        return false;
    }

    // How big any single WebSocket message can be.
    // WebSocketPP automatically disconnects the client when exceeded!
    conn->set_max_message_size(config_->max_mgs_size_);
    conn->set_max_http_body_size(config_->max_mgs_size_);

    shaker.client_id_         = conn->get_request_header("client-id");
    shaker.session_id_        = Guid::make().toXmlString();
    shaker.etp_encoding_      = conn->get_request_header("etp-encoding");
    shaker.authorization_     = conn->get_request_header("Authorization");
    shaker.data_partition_id_ = conn->get_request_header("data-partition-id");

    // Browser clients cannot set HTTP headers,
    // so check the URL's query-string for parameters too
    const auto uri = *conn->get_uri();
    std::string query = uri.get_query();
    if (!query.empty()) {
        std::ostringstream oss;
        EmlUri::percentDecode(oss, query);
        query = oss.str();

        std::vector<std::string> params;
        namespace ba = boost::algorithm;
        ba::split(params, query, ba::is_any_of("&"), ba::token_compress_off);
        for (const std::string& kv : params) {
            size_t eq = kv.find('=');
            if (eq == std::string::npos) {
                continue; // probably not normal.
            }

            std::string key = kv.substr(0, eq);
            std::string val = kv.substr(eq + 1);
            boost::algorithm::to_lower(key);

            if (key == "client-id") {
                if (shaker.client_id_.empty()) {
                    shaker.client_id_ = val;
                }
            } else if (key == "etp-encoding") {
                if (shaker.etp_encoding_.empty()) {
                    shaker.etp_encoding_ = val;
                }
            } else if (key == "authorization") {
                if (shaker.authorization_.empty()) {
                    shaker.authorization_ = val;
                }
            } else if (key == "data-partition-id") {
                if (shaker.data_partition_id_.empty()) {
                    shaker.data_partition_id_ = val;
                }
            }
        }
    }
    
    if (!shaker.client_id_.empty()) {
        OES_DEBUG("SVR\tclient-id: {}", shaker.client_id_);
    }
    if (!shaker.etp_encoding_.empty()) {
        OES_DEBUG("SVR\tetp-encoding: {}", shaker.etp_encoding_);
    }
    if (!shaker.authorization_.empty()) {
        OES_DEBUG("SVR\tAuthorization: {}", oes::core::StringUtils::abbreviate(shaker.authorization_));
    }

    RequestHeaderGetter getter = [&conn](std::string header) {
        return conn->get_request_header(header);
    };

    std::shared_ptr<Session> session;
    {
        // We "serialize" on purpose session-creation, for safety.
        // Factory-functors from the config thus need not be thread-safe
        std::unique_lock factory_lock(config_mt_);
        try {
            session = config_->session_factory_(
                std::move(shaker), std::move(getter)
            );
        } catch (const std::exception& e) {
            OES_ERROR(e.what());
            // ToDo: Refactor the session factory for able to distinguish business and system errors
            // Currently exception may thrown due not existing partition (business case) or due system related reason like PostgreSQL (should not occur during normal work)
            conn->set_status(websocketpp::http::status_code::bad_request);
            return false;
        }
    }

    if (!session) {
        const hires_time_point end = now();
        const duration_milli_t millis = end - start;

        OES_ERROR(
            "SVR KO: CONNECTION REFUSED in {:.1f}ms; Thread({}), from {}",
            millis.count(), thisThreadId(), conn->get_remote_endpoint()
        );

        conn->set_status(
            websocketpp::http::status_code::unauthorized,
            "Connection Refused: Cannot Create ETP Session."
        );
        return false;
    }

    const std::string label = session->label();

    std::unique_lock xguard(ws_server_mt_); // read-write
    auto new_entry = ws_conn_info_map2_.emplace(
        hdl, ConnectionInfo(std::move(session), label)
    );
    OES_ASSERT(new_entry.second, ;);
    if (new_entry.second) {
        auto new_session = new_entry.first->second.session_;
        assert(new_session);
        new_session->id_ = new_entry.first->second.id_;
        const unsigned int thread_count = (config_->totalThreadCount_ == 0)?
            std::thread::hardware_concurrency():
            config_->totalThreadCount_
        ;
        const unsigned int actual_worker_threads = std::min<>(
            std::thread::hardware_concurrency(), thread_count
        );
        boost::asio::io_service& work_svc = actual_worker_threads>1 ? 
            work_svc_ : ws_server_.get_io_service();
        new_session->state_->init(
            new_session, shared_from_this(), hdl, ws_server_.get_io_service(), work_svc
        );
    }
    xguard.unlock();

    const hires_time_point end = now();
    const duration_milli_t millis = end - start;

    OES_INFO(
        "SVR OK: CONNECTION ACCEPTED in {:.1f}ms; Thread({}) Session({}), from {}",
        millis.count(), thisThreadId(), new_entry.first->second.id_, label
    );
    return true;
}

void Impl::on_open(ws_conn_hdl_t hdl) {
    const ConnectionInfo& conn_info = get_conn_info(hdl);
    OES_DEBUG("SVR on_open({})\n", conn_info);
}

void Impl::on_fail(ws_conn_hdl_t hdl) {
    try {
        oes::core::Metrics::instance().getCounter("any_error_counter").Increment();
    } catch (const oes::core::MetricNotFound& e) {
        OES_WARNING("Not registered metric. Reason: {}", e.what());
    }

    const ConnectionInfo& conn_info = get_conn_info(hdl);
    OES_DEBUG("SVR on_fail({})\n", conn_info);
}

const char* msgOpcodeAsString(websocketpp::frame::opcode::value code) {
    switch (code) {
    case websocketpp::frame::opcode::CONTINUATION:  return "CONTINUATION";
    case websocketpp::frame::opcode::TEXT:          return "TXT";
    case websocketpp::frame::opcode::BINARY:        return "BIN";
    case websocketpp::frame::opcode::CLOSE:         return "CLOSE";
    case websocketpp::frame::opcode::PING:          return "PING";
    case websocketpp::frame::opcode::PONG:          return "PONG";
    default:                                        return "???";
    }
}

// Entry-point to the server, a raw message was received from a client.
// This method looks up the WebSocket "session" of the connection,
// does (optional) logging and tracing of that raw message, then handles
// control over to session-specific handling code.
void Impl::on_message(ws_conn_hdl_t hdl, ws_message_t msg) {
    const auto sock_at = now();

    const auto msg_opcode = msg->get_opcode();
    std::string& raw_msg = msg->get_raw_payload();
    const size_t bytes = raw_msg.size();

    const ConnectionInfo& conn_info2 = get_conn_info(hdl);
    OES_ASSERT(conn_info2.id_, return;);

    std::shared_ptr<Session> session = conn_info2.session_;
    OES_ASSERT(session.get(), return;);
    Session::State& state = *session->state_;

    // FYI: 1) Update stats;  2) Tracing;  3) Logging.

    duration_milli_t millis_later;
    {
        std::lock_guard lock(state.recv_mt_);
        millis_later = sock_at - state.recv_.last_req_recv_;
        state.recv_.last_req_recv_ = sock_at;
        state.recv_.bytes_recv_.add(bytes);
    }

    EtpTracer12*const tracer = tracer_.get();
    if (tracer) {
        EtpTracerEventInfo trace_evt;
        trace_evt.set_point_event(sock_at);
        trace_evt.tid_ = thisThreadId();
        trace_evt.sid_ = conn_info2.id_;
        trace_evt.set_phase(EtpTracerEventInfo::eRECV);
        trace_evt.set_encoding(
            (msg_opcode == websocketpp::frame::opcode::TEXT)?
                EtpTracerEventInfo::eAVRO_JSON:
                EtpTracerEventInfo::eAVRO_BINARY
        );
        trace_evt.msg_ = raw_msg; // FIXME: Avoid copy
        tracer->async_trace(std::move(trace_evt));
    }

    if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug) {
        const char* msg_type = msgOpcodeAsString(msg_opcode);
        OES_DEBUG(
            "\n"
            "SVR SOCK {} UTC ({:.1f}ms later)\n"
            "SVR RECV Thread({}) Session({}) {} {} bytes, from {}",
            oes::core::formatExtendedIsoDateTime(
                std::chrono::system_clock::now(),
                oes::core::FracSecRes::eMicroSecFloor
            ),
            millis_later.count(),
            thisThreadId(), conn_info2.id_,
            msg_type, bytes, conn_info2.label_
        );
    }

    PendingRpc_ptr p_rpc = state.acquire_rpc();
    OES_ASSERT(p_rpc, return;);

    p_rpc->sock_at_ = sock_at;
    p_rpc->raw_msg_ = std::move(raw_msg);
    p_rpc->txt_msg_ = (msg_opcode == websocketpp::frame::opcode::TEXT);

    state.recv_strand_->post(
        [state = session->state_, p_rpc]() mutable {
            assert(same_owner(*state, p_rpc));
            state->handle_msg(p_rpc);
        }
    );
}

void Impl::on_close(ws_conn_hdl_t hdl) {
    if (tracer_) {
        tracer_->async_flush();
    }

    std::unique_lock xguard(ws_server_mt_); // FIXME: needs read-write?
    ws_conn_ptr_t conn = get_conn_no_lock(hdl);

    // Not using get_conn_info_no_lock() because of move below
    auto found = ws_conn_info_map2_.find(hdl); // lock is above
    OES_ASSERT(found != ws_conn_info_map2_.end(), return;);

    // we are about to erase() this entry, so transfer ownership
    // to local var, we'll remain sole owners of the session.
    ConnectionInfo conn_info2 = std::move(found->second);
    const auto session = conn_info2.session_;
    OES_ASSERT(session->state_, ;);

    if(session->state_->status_ != Session::State::Status::Closed) {
       session->state_->status_ = Session::State::Status::AboutToClose;
    }

    OES_DEBUG(
        "\n"
        "SVR on_close({})\n"
        "SVR\tLOCK config: {}\n"
        "SVR\t     server: {}\n"
        "SVR\t     shared: {}\n",
        conn_info2,
        config_mt_.to_string(),
        ws_server_mt_.to_string(),
        ws_server_mt_.shared_to_string()
    );

    if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::verbose) {
        Session::State& state = *session->state_;
        std::scoped_lock lock(state.recv_mt_, state.send_mt_, state.proc_mt_);
        const size_t bytes_recv = state.recv_.bytes_recv_.count();
        if (!bytes_recv) {
            const hires_time_point end = now();
            const std::chrono::duration<double> sec = end - state.started_;

            OES_INFO(
                "[{}] SVR CONNECTION CLOSED: Session {}"
                "SVR    No ETP messages received ({:.3f}s idle)\n",
                conn_info2.session_->client_identification_, conn_info2, sec.count()
            );
        } else {
            const hires_time_point end = now();
            const std::chrono::duration<double> sec = end - state.started_;

            const auto& recv_time_ms = state.recv_.recv_time_ms_;
            const auto& send_time_ms = state.send_.send_time_ms_;
            const auto& proc_time_ms = state.proc_.proc_time_ms_;
            const auto& proc_topN_ms = state.proc_.proc_topN_ms_;
            const double proc_time_sec = proc_time_ms.sum() / 1000.0;
            const double recv_time_sec = recv_time_ms.sum() / 1000.0;
            const double send_time_sec = send_time_ms.sum() / 1000.0;

            using oes::core::thou;
            OES_INFO(
                "[{19}] SVR CONNECTION CLOSED: Session {0}\n"
                "[{19}] SVR\tRECV {1} ({2} bytes, {17:.3f}s ({18:.1f}%)),\n"
                "[{19}] SVR\t     {10};\n"
                "[{19}] SVR\t     {16};\n"
                "[{19}] SVR\tSEND {3} ({4} bytes, {14:.3f}s ({15:.1f}%), {9} errors),\n"
                "[{19}] SVR\t     {11};\n"
                "[{19}] SVR\t     {13};\n"
                "[{19}] SVR\tPROC {5:.3f}s ({7:.1f}%) of {6:.3f}s ({8:.3f}s idle)\n"
                "[{19}] SVR\t     {12}.\n",
                conn_info2,
                thou(state.recv_.bytes_recv_.count()), thou(state.recv_.bytes_recv_.sum()),
                thou(state.send_.bytes_sent_.count()), thou(state.send_.bytes_sent_.sum()),
                proc_time_sec, sec.count(),
                (proc_time_sec * 100.0 / sec.count()),
                (sec.count() - proc_time_sec),
                thou(state.send_.errors_sent_),
                state.recv_.bytes_recv_.to_string(),
                state.send_.bytes_sent_.to_string(),
                proc_time_ms.to_string(),
                send_time_ms.to_string(), send_time_sec,
                (send_time_sec * 100.0 / sec.count()),
                recv_time_ms.to_string(), recv_time_sec,
                (recv_time_sec * 100.0 / sec.count()),
                conn_info2.session_->client_identification_
            );

            const bool did_inflate = (state.recv_.infl_time_ms_.count() > 0);
            const bool did_deflate = (state.send_.defl_time_ms_.count() > 0);

            if (did_inflate) {
                const double infl_time_sec = state.recv_.infl_time_ms_.sum() / 1000.0;
                const size_t head_bytes = state.recv_.header_bytes_;
                const size_t body_bytes = state.recv_.body_usize_.sum();
                const size_t recv_bytes = state.recv_.bytes_recv_.sum();

                OES_VERBOSE(
                    "SVR\tINFL {} {} ({} bytes ({:.1f}x), {:.3f}s ({:.1f}%; {:.1f}% of RECV)),\n"
                    "SVR\t     {};\n"
                    "SVR\t     {}.",
                    thou(state.recv_.infl_time_ms_.count()),
                    state.zcompressor_->name_, thou(body_bytes),
                    (1. * body_bytes / (recv_bytes - head_bytes)),
                    infl_time_sec, (infl_time_sec * 100.0 / sec.count()),
                    (infl_time_sec * 100.0 / recv_time_sec),
                    state.recv_.body_usize_.to_string(),
                    state.recv_.infl_time_ms_.to_string()
                );
            }

            if (did_deflate) {
                const double defl_time_sec = state.send_.defl_time_ms_.sum() / 1000.0;
                const size_t head_bytes = state.send_.header_bytes_;
                const size_t body_bytes = state.send_.body_usize_.sum();
                const size_t sent_bytes = state.send_.bytes_sent_.sum();

                OES_VERBOSE(
                    "SVR\tDEFL {} {} ({} bytes ({:.1f}x), {:.3f}s ({:.1f}%; {:.1f}% of SEND)),\n"
                    "SVR\t     {};\n"
                    "SVR\t     {}.",
                    thou(state.send_.defl_time_ms_.count()),
                    state.zcompressor_->name_, thou(body_bytes),
                    (1. * body_bytes / (sent_bytes - head_bytes)),
                    defl_time_sec, (defl_time_sec * 100.0 / sec.count()),
                    (defl_time_sec * 100.0 / send_time_sec),
                    state.send_.body_usize_.to_string(),
                    state.send_.defl_time_ms_.to_string()
                );
            }

            if (did_inflate || did_deflate) {
                OES_VERBOSE("");
            }

            OES_INFO(
                "[{}] SVR\tTOP-{} requests by PROC time:\n",
                conn_info2.session_->client_identification_, proc_topN_ms.topN_.size()
            );
            const int digits = 1 +static_cast<int>(std::log10(proc_time_ms.max()));
            for (const auto& entry : proc_topN_ms.topN()) {
                OES_INFO(
                    "[{}] SVR\t{:{}.1f}ms {}\n",
                    conn_info2.session_->client_identification_, entry.first, digits + 2, entry.second
                );
            }
        }
    }
    ws_conn_info_map2_.erase(found);

    if (!session->isClosed()) {
        // WebSocket closed w/o expected ETP Core.CloseSession message
        namespace cs = websocketpp::close::status;
        const cs::value remote_cc = conn->get_remote_close_code();
        std::string reason = conn->get_remote_close_reason();
        const std::error_code ec = conn->get_ec();

        // Put this log in verbose mode, it does not really bring anything new, the session->close()
        // call below already reports the session being closed forcefully if the client disconnected
        // before closing its session.
        OES_VERBOSE(
            "[{}] SVR on_close Thread({}) Session({}) close_code={}({}) reason=\"{}\" lib_error=\"{}\"",
            conn_info2.session_->client_identification_,
            thisThreadId(), conn_info2.id_,
            remote_cc, cs::get_string(remote_cc),
            reason,
            ec ? ec.message() : ""
        );

        session->close(std::move(reason), true);
        
        // In case of session survivability 
        if (config_->session_keep_alive_duration_.count() > 0) {
            auto session_uuid = session->handshake_.session_id_;
            // remove dashes from the uuid string
            session_uuid.erase(std::remove_if(session_uuid.begin(), session_uuid.end(), [](unsigned char c){
                return c == '-';
            }), session_uuid.end());
            
            // Save the session
            ws_lost_sessions_map_.emplace(std::make_pair(session_uuid, LostConnectionSession(session_uuid, std::move(conn_info2))));

            if (!cleanup_timer_) {
                cleanup_timer_ = std::make_unique<boost::asio::steady_timer>(ws_server_.get_io_service());
            }

            {
                ExpiredSessionJob job;
                job.when = std::chrono::steady_clock::now() + config_->session_keep_alive_duration_;
                job.session_uuid = session_uuid;
                OES_DEBUG("Schedule clean on for uuid {}", session_uuid);
                job.fn = [this, s_uuid = session_uuid](){
                    std::lock_guard lock(lost_sessions_mt_);
                    // OES_INFO("Cleanup service for session {}", s_uuid);
                    auto found = ws_lost_sessions_map_.find(s_uuid);
                    if (found != ws_lost_sessions_map_.end()) {
                        ws_lost_sessions_map_.erase(found);
                        OES_DEBUG("Expired session {} deleted", s_uuid);
                    }
                };

                std::lock_guard lock(cleanup_timer_jobs_mt_);                
                cleanup_lost_sessions_jobs_.push(job);
            }

            schedule_cleanup_expired_sessions_job();

        }
    } else {
        // Already closed by an ETP Core.CloseSession message
        conn_info2.session_ = nullptr; // delete the session (explicitly)
    }


    // show memory usage after we got rid of the session
    if (oes::core::Logger::instance().minSeverityFilter()<=oes::core::Message::Severity::verbose) {
        using oes::core::thou;
        auto mem_used = oes::core::System::getMemoryUsage();
        OES_VERBOSE(
            "SVR MEMORY: {} resident, {} virtual, {} peak\n",
            thou(mem_used.resident_bytes),
            thou(mem_used.virtual_bytes),
            thou(mem_used.peak_bytes)
        );
    }
}

void Impl::on_http(ws_conn_hdl_t hdl) {
    // both get_conn_info() and ws_server_.get_con_from_hdl (i.e. get_conn())
    // must be protected under the same shared/ro lock.
    std::shared_lock sguard(ws_server_mt_); // read-only
    const ConnectionInfo& conn_info = get_conn_info_no_lock(hdl);
    ws_conn_ptr_t conn = get_conn_no_lock(hdl);
    sguard.unlock();

    OES_VERBOSE(
        "SVR HTTP {} UTC",
        oes::core::formatExtendedIsoDateTime(
            std::chrono::system_clock::now(),
            oes::core::FracSecRes::eMicroSecFloor
        )
    );

    OES_VERBOSE("SVR on_http({})", conn_info);

    conn->defer_http_response();

    if (conn->get_request().get_method() != "GET") {
        conn->set_body("ETP server accepting only HTTP GET, in addition to WebSocket requests");
        conn->set_status(websocketpp::http::status_code::method_not_allowed); // 405
        conn->send_http_response();
        return;
    }

    std::string uri = conn->get_request().get_uri();

    std::string query;
    std::size_t qm_found = uri.find('?');
    if (qm_found != std::string::npos) {
        query = uri.substr(qm_found + 1);
        uri.resize(qm_found);
    }

    // Adding health check (Based on storage svc https://community.opengroup.org/osdu/platform/system/storage/-/blob/master/storage-core/src/main/java/org/opengroup/osdu/storage/api/HealthCheckApi.java#L9)
    // Strictly needed by Azure AppGW to allow traffic to the openetp
    if (uri == "/health" ||
        uri == "/health/liveness" ||
        uri == "/health/liveness_check" ||
        uri == "/health/readiness" ||
        uri == "/health/readiness_check") {

        conn->set_body("Ready for " + uri);
        conn->set_status(websocketpp::http::status_code::ok); // 200
        conn->send_http_response();
        return;
    }

    if (uri == "/health/startup") {
        if (config_->db_connection_pool_->checkConnectionsStatus()) {
            conn->set_body("Ready for " + uri);
            conn->set_status(websocketpp::http::status_code::ok); // 200
            conn->send_http_response();
            try {
                oes::core::Metrics::instance().getCounter("health_ep_counter").Increment();
            } catch (const oes::core::MetricNotFound& e) {
                OES_WARNING("Not registered metric. Reason: {}", e.what());
            }
            return;
        } else {
            conn->set_body("Not Ready for " + uri);
            conn->set_status(websocketpp::http::status_code::service_unavailable); // 503
            conn->send_http_response();
            return;
        }
    }

    if (uri == "/metrics") {
        conn->set_body(oes::core::Metrics::instance().getSerializedRegistry());
        conn->set_status(websocketpp::http::status_code::ok); // 200
        conn->send_http_response();
        try {
            oes::core::Metrics::instance().getCounter("metrics_ep_counter").Increment();
        } catch (const oes::core::MetricNotFound& e) {
            OES_WARNING("Not registered metric. Reason: {}", e.what());
        }
        return;
    }

    if( uri == "/info"){
        const std::string appName = "OSDU Reservoir DDMS ETP-1.2 Server";
        const std::string appVers = config_->server_capabilities_.m_applicationVersion;

        #if defined GIT_COMMIT_HASH && defined GIT_COMMIT_TIMESTAMP
        // Use last git commit hash and date as version number
        const std::string commitId = DEFINE_TO_STRING(GIT_COMMIT_HASH);
        const std::string commitTime = DEFINE_TO_STRING(GIT_COMMIT_TIMESTAMP);
        const std::string buildTime = DEFINE_TO_STRING(BUILD_TIME);
        #else
        const std::string commitId = "unknown";
        const std::string commitTime = "unknown";
        const std::string buildTime = "unknown";
        #endif

        conn->set_body(R"json({
    "groupId": "org.opengroup.osdu",
    "artifactId": ")json" + appName + R"json(",
    "version": ")json" + appVers + R"json(",
    "commitId": ")json" + commitId + R"json(",
    "commitTime": ")json" + commitTime + R"json(",
    "buildTime": ")json" + buildTime + R"json("
})json");
        conn->set_status(websocketpp::http::status_code::ok); // 200
        conn->send_http_response();
        return;
    }

    if (uri == "/refresh-partitions") {
        // If AuthN is delegated, allow this critical call only if this is a service request.
        if(!this->config_->authn_delegate_url_.empty() ) {
            const auto http_client_ptr = std::make_shared<oes::core::HTTP>();
            const auto auth_hdr = conn->get_request_header("Authorization");
            oes::core::HTTP::Headers auth_headers;
            auth_headers.push_back("Authorization: " + auth_hdr);
            auto res = http_client_ptr->get(this->config_->authn_delegate_url_ + "/service", 0, auth_headers );
            if( res.code != 200 ) {
                conn->set_body("Forbidden.");
                conn->set_status(websocketpp::http::status_code::forbidden); // 403
                conn->send_http_response();
                return;
            }
        }

        const auto& server_config = *this->config_->server_config_;

        // Currently we get osduRepos when in mode standalone too
        if (server_config.partitionMode() != "multiple") { // && server_config.connectivityMode() != "osdu"
            conn->set_body("Server not running in multiple partition mode.");
            conn->set_status(websocketpp::http::status_code::bad_request); // 400
            conn->send_http_response();
            return;
        } 
        
        auto partitionInfo = oes::osdu::OsduConnectionInfoProvider::getOsduConnectionInfo(server_config);
        auto status = this->config_->db_connection_pool_->refreshDatabases(partitionInfo);

        if (status) {
            conn->set_body(R"json({
    "newPartitions": true,
})json");
        } else {
            conn->set_body(R"json({
    "newPartitions": false,
})json");
        }
        conn->set_status(websocketpp::http::status_code::ok); // 200
        conn->send_http_response();
        return;
    }

    // For now, we support a single route, per the ETP spec
    if (uri != "/.well-known/etp-server-capabilities") {
        conn->set_body("Unknown resource: " + uri);
        conn->set_status(websocketpp::http::status_code::not_found); // 404
        conn->send_http_response();
        return;
    }

    // Which version(s) of the URI protocol does this server support. Just 1.2 for now.
    if (query == "GetVersions=true") {
        conn->set_body(R"json(["etp12.energistics.org"])json");
        conn->set_status(websocketpp::http::status_code::ok); // 200
        conn->send_http_response();
        return;
    }

    // check if query starts with "GetVersion=etp12.energistics.org"
    if (query.rfind("GetVersion=etp12.energistics.org", 0) != 0) {
        conn->set_status(websocketpp::http::status_code::not_implemented); // 501
        conn->send_http_response();
        return;
    }

    my_buffer_ptr response_buffer = encode_json(config_->server_capabilities_, true);
    std::string response_str( // FIXME: Can avoid copy as string?
        reinterpret_cast<const char*>(response_buffer->data()), response_buffer->size()
    );
    conn->set_body(response_str);
    conn->set_status(websocketpp::http::status_code::ok); // 200
    conn->send_http_response();
}

void Impl::adjustProtocolCapabilities(Datatypes::SupportedProtocol& proto, const Datatypes::SupportedProtocol& server_proto) const
{
    // Copy matching server proto capabilities.
    proto.m_protocolCapabilities = server_proto.m_protocolCapabilities;
}

proto12::IProtocol_ptr Impl::makeProtocol(ProtoEnum protoId, const Session::State& state) const
{
    proto12::IProtocol_ptr proto;

    const std::weak_ptr<Session> session = state.get_session();

    // Without established session, only `Core` protocol is allowed
    if (protoId == ProtoEnum::eCore) {
        proto = std::make_shared<CoreProtoImpl>(session);
    } else if (protoId == ProtoEnum::eCoreOSDU) {
        proto = std::make_shared<CoreOSDUProtoImpl>(session);
    } else if (session.lock().get()) {
        const std::shared_ptr<Session> sessionPtr = session.lock();
        const auto supportedProtocolIt = std::find_if(sessionPtr->adjustedProtocols_.cbegin(), sessionPtr->adjustedProtocols_.cend(), [protoId](auto& pr) {
            return pr.m_protocol == static_cast<int>(protoId);
        });
        if (supportedProtocolIt != sessionPtr->adjustedProtocols_.cend()) {
            proto = config_->protocol_factory_(*supportedProtocolIt, session);
        }
    } else {
        OES_ERROR(
            "[{}] SVR PROC Thread({}) Session({}): Session has been forcefully terminated.",
            state.client_identification_, thisThreadId(), state.sid_
        );
    }
    return proto;
}

/**
 * @brief Schedule the next clean up
 * 
 */
void Impl::schedule_cleanup_expired_sessions_job() {
    std::lock_guard lock(cleanup_timer_jobs_mt_);

    if (cleanup_lost_sessions_jobs_.empty()) {
        return;
    }

    const auto next_clean_up = cleanup_lost_sessions_jobs_.top().when;
    const auto do_schedule = cleanup_lost_sessions_jobs_.size() == 1 // if only one then we MUST schedule
        || cleanup_timer_->expiry() <= std::chrono::steady_clock::now() // previous already executed
        || next_clean_up < cleanup_timer_->expiry(); // the next one is earlier than the one to be executed
    
    if (do_schedule) {
        cleanup_timer_->cancel();

        cleanup_timer_->expires_at(next_clean_up);
        cleanup_timer_->async_wait(
            [this](const boost::system::error_code& ec) {
                if (ec == boost::asio::error::operation_aborted) {
                    return;
                } else {
                    const auto now = std::chrono::steady_clock::now();
    
                    std::lock_guard lock(cleanup_timer_jobs_mt_);
                    while (!cleanup_lost_sessions_jobs_.empty() && cleanup_lost_sessions_jobs_.top().when <= now) {
                        auto job = std::move(cleanup_lost_sessions_jobs_.top());
                        cleanup_lost_sessions_jobs_.pop();
                        job.fn();
                    }
                }
                
                OES_DEBUG("Schedule next clean up for uuid {}", cleanup_lost_sessions_jobs_.top().session_uuid);
                schedule_cleanup_expired_sessions_job();
            }
        );
    }
}

//============================================================================
//============================================================================

/*!
 * \class Server oes/eml/etp12/.h
 *
 * \brief The ETP server.
 *
 * Encapsulates interaction with the underlying WebSocket implementation.
 * Instantiates ETP sessions via the provided config's SessionFactory,
 * negotiating per-session-specific available ETP protocols.
 */

Server::Server(
    ServerConfig config
) : config_(std::move(config))
  , impl_(std::make_shared<Impl>())
  , running_(false)
{
    OES_ASSERT(
        config_.session_factory_ && config_.protocol_factory_
        && config_.user_factory_ && config_.entitlement_factory_,
        throw std::runtime_error("Missing Session and/or Protocol factories");
    );

    if (config_.flavor_ != ServerConfig::WebSocketPP) {
        throw std::runtime_error("Boost.Beast ETP server not supported yet");
    }

    // TODO: move elsewhere
    //registerSupportedProtocol(std::make_shared<CoreProtoImpl>());

    // Could be out-of-sync from what config.protocol_factory_ does...
    const std::string appName = "OSDU Reservoir DDMS ETP-1.2 Server";

    #if defined GIT_COMMIT_HASH && defined GIT_COMMIT_TIMESTAMP
    // Use last git commit hash and date as version number
    const std::string commitId = DEFINE_TO_STRING(GIT_COMMIT_HASH);
    const std::string buildTime = DEFINE_TO_STRING(GIT_COMMIT_TIMESTAMP);
    const std::string appVers = commitId + " (" + buildTime + ")";
    #else
    const std::string appVers = "v1";
    #endif

    ServerCapabilities& cap = config_.server_capabilities_;

    // Set (or augment) information about the server
    if (cap.m_applicationName.empty()) {
        cap.m_applicationName = appName;
    } else {
        (cap.m_applicationName += " - ") += appName;
    }

    if (cap.m_applicationVersion.empty()) {
        cap.m_applicationVersion = appVers;
    } else {
        (cap.m_applicationVersion += " - ") += appVers;
    }

    if (cap.m_contactInformation.m_organizationName.empty()) {
        cap.m_contactInformation.m_organizationName = "OSDU-Energistics";
    }
    if (cap.m_contactInformation.m_contactEmail.empty()) {
        cap.m_contactInformation.m_contactEmail = "laurent.deny@aspentech.com";
    }

    using namespace Datatypes;
    Version v12;
    v12.m_major = 1;
    v12.m_minor = 2;
    v12.m_revision = 0;
    v12.m_patch = 0;

    bool hasCoreProto = false;
    for (const auto& item : cap.m_supportedProtocols) {
        if (item.m_protocol == static_cast<int>(ProtoEnum::eCore)) {
            hasCoreProto = true;
            break;
        }
    }
    OES_ASSERT(!hasCoreProto, ;);
    if (!hasCoreProto) {
        cap.m_supportedProtocols.emplace_back();
        cap.m_supportedProtocols.back().m_protocol = static_cast<int>(ProtoEnum::eCore);
        cap.m_supportedProtocols.back().m_protocolVersion = v12;
        cap.m_supportedProtocols.back().m_role = "server";
        cap.m_supportedProtocols.back().m_protocolCapabilities = {}; // TODO
    }

    std::sort(
        cap.m_supportedProtocols.begin(), cap.m_supportedProtocols.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.m_protocol < rhs.m_protocol;
        }
    );

    // These three OTOH are implemented by this server, so set here
    cap.m_supportedEncodings = { "binary" }; // TODO: add (Avro) "json" encoding
    cap.m_supportedFormats = { "xml" };      // TODO: add "json" format
    cap.m_supportedCompression = { "gzip" }; // TODO: add lz4, zstd compression

    // For now, we hardcode this here, if there's nothing in the config
    if (cap.m_supportedDataObjects.empty()) {
        for (const auto& ml : { "eml20", "resqml20", "resqml22", "eml23", "witsml21", "prodml22", "prodml23" }) {
            auto& it = cap.m_supportedDataObjects.emplace_back();
            it.m_qualifiedType = fmt::format("{}.*", ml);
            Datatypes::DataValue true_val;
            true_val.m_item = true;
            it.m_dataObjectCapabilities = {
                { "SupportsGet", true_val },
                { "SupportsPut", true_val },
                { "SupportsDelete", true_val }
            };
        }
    }

    // Advertise how big a WebSocket message this server supports
    // Any message from the client larger than this will terminate
    // the WebSocket connection, directly at the WebSocketPP level.
    {
        auto& val = cap.m_endpointCapabilities["MaxWebSocketMessagePayloadSize"];
        val.m_item = int64_t(config_.max_mgs_size_);
    }

    // We also want to limit how much memory uncompression will take (non-standard)
    // It's nice to have highly compressible data, but because we must pre-allocate
    // data prior to decompression (as of now), too large a value is not good.
    {
        auto& val = cap.m_endpointCapabilities["MaxMessagePayloadUncompressedSize"];
        val.m_item = int64_t(config_.max_mgs_usize_);
    }

    // Advertise if the server is handling Multiple data partition.
    // value could be "single' or "multiple"
    {
        auto& val = cap.m_endpointCapabilities["DataPartitionMode"];
        val.m_item = config_.dataPartitionMode_;
    }

    // advertise if the server is handling OSDU data connectivity
    // value could be "standalone" or "osdu"
    {
        auto& val = cap.m_endpointCapabilities["DataConnectivityMode"];
        val.m_item = config_.dataConnectivityMode_;
    }

    impl_->config_ = &config_;
}

ServerCapabilities Server::getServerInfo() const {
    return config_.server_capabilities_;
}

bool Server::start() {
    return impl_->start(config_, running_);
}

// Not thread-safe
bool Server::isRunning() const {
    return running_;
}

void Server::stopListening() {
    // See https://github.com/zaphoyd/websocketpp/issues/803
    // and https://docs.websocketpp.org/faq.html
    return impl_->ws_server_.stop_listening();
}

std::future<int> Server::getRandomServerPort() {
    return impl_->server_port_promise_.get_future();
}

//============================================================================
//============================================================================

/*!
 * \class Session oes/eml/etp12/.h
 *
 * \brief Event used to follow an OracleProxy process.
 */

Session::Session(
    Handshake handshake,
    auth::AuthenticatedUser_ptr user,
    auth::Entitlement_ptr entitlement
) : handshake_(std::move(handshake))
  , user_(std::move(user))
  , entitlement_(std::move(entitlement))
  , state_(std::make_shared<State>())
  , id_(0)
  , otel_parent_span_(oes::core::OtelSpan("Server.Session", {}, true))
{
    std::string user_name = "ANON";
    if (user_) {
        if (!user_->name_.empty()) {
            user_name = user_->name_;
        }
    }
    label_ = fmt::format("{}@{}", user_name, handshake_.remote_endpoint_);
}

Session::~Session() {
}

/*!
 * \brief Get the current user name.
 *
 * If the user is not authenticated, the user name is "<Unknown>".
 * \return The user name.
*/
const std::string& Session::userName() const {
    static std::string unknown = "<Unknown>";
    return user_ ? user_->name_ : unknown;
};

/*!
 * \brief Get the current user mail.
 *
 * If the user is not authenticated, the user mail is "<Unknown>".
 * \return The user mail.
*/
const std::string& Session::userMail() const {
    static std::string unkown = "<Unknown>";
    return user_ ? user_->email_ : unkown;
}

void Session::onStart(
    proto12::Core::RequestSession req,
    proto12::Core::OpenSession& res
) {
    OES_ASSERT(state_->status_ == State::Status::Initial, throw std::runtime_error("Session already open"););

    Impl_ptr impl = state_->wimpl_.lock();
    OES_ASSERT(impl, return;);

    threads_count_ = impl->thread_joiner_.threads_.size();

    // Require a valid entitlement when authZ is enabled; user_ alone is not enough
    // (under --authN none it is a non-null placeholder even without a token).
    if( !user_ || (authZ_enabled_ && (!entitlement_ || !entitlement_->isValid())) ) {
        // If authorization is provided on connection, give a chance to authorize
        onAuthN(handshake_.authorization_);
        if( !user_ || (authZ_enabled_ && (!entitlement_ || !entitlement_->isValid())) ) {
            proto12::ProtocolException ex;
            Datatypes::ErrorInfo info;
            info.m_code = data12::EAUTHORIZATION_REQUIRED;
            info.m_message = "Authorization has not been provided.";
            ex.m_error = std::move(info);
            throw ex;
        }
    }


    OES_VERBOSE(
        "SVR Application:  {}\n"
        "SVR App-Version:  {}",
        req.m_applicationName,
        req.m_applicationVersion
    );
    Guid client_uid;
    assignUid(client_uid, req.m_clientInstanceId);
    OES_DEBUG(
        "SVR Client-UUID:  {}\n"
        "SVR Server-UUID:  {}\n"
        "SVR Session-UUID: {}",
        client_uid.toXmlString(),
        impl->server_uid_.toXmlString(),
        handshake_.session_id_
    );

    res.m_applicationName = impl->config_->server_capabilities_.m_applicationName;
    res.m_applicationVersion = impl->config_->server_capabilities_.m_applicationVersion;

    // Blindly accept all requested dataobject types starting with resqml20. or eml20.
    // TODO : check that the datatype following the namespace is indeed supported
    // For example "eml20.MyOwnCustomDatatype" should not be supported
    for (auto& req_dataobject : req.m_supportedDataObjects) {
        if (req_dataobject.m_qualifiedType.find("resqml20.") != std::string::npos ||
            req_dataobject.m_qualifiedType.find("eml20.") != std::string::npos) {
            auto& it = res.m_supportedDataObjects.emplace_back();
            it.m_qualifiedType = req_dataobject.m_qualifiedType;
            it.m_dataObjectCapabilities = req_dataobject.m_dataObjectCapabilities;
        }
    }

    if (res.m_supportedDataObjects.empty()) {
        proto12::ProtocolException ex;
        Datatypes::ErrorInfo info;
        info.m_code = data12::ENOSUPPORTEDDATAOBJECTTYPES;
        info.m_message = "None of the requested dataobject types are supported.";
        ex.m_error = std::move(info);
        OES_DEBUG(info.m_message);
        throw ex;
    }

    res.m_supportedFormats = { "xml" }; // TODO: xml+lz4, xml+deflate[+dict], etc...

    oes::common::assignUid(res.m_serverInstanceId, impl->server_uid_);
    oes::common::assignUid(res.m_sessionId, Guid::parse(handshake_.session_id_));

    client_identification_ = req.m_applicationName;
    if (!handshake_.client_id_.empty()) {
        client_identification_ += " @ " + handshake_.client_id_;
    } else if (user_) {
        client_identification_ += " @ (" + user_->email_ + ")@" + handshake_.data_partition_id_;
    }

   //res.m_currentDateTime set by caller, via pingPong()

    const auto& supportedProtocols = impl->config_->server_capabilities_.m_supportedProtocols ;
    for (auto& req_proto : req.m_requestedProtocols)
    {
        // Check if protocol is supported
        const auto proto_id = req_proto.m_protocol;
        const auto server_proto = std::find_if(
            supportedProtocols.cbegin(), supportedProtocols.cend(),
            [proto_id] (const oes::common::etp12::Datatypes::SupportedProtocol& p) { return p.m_protocol == proto_id ;}
        ) ;
        if (server_proto == supportedProtocols.cend()) {
            OES_VERBOSE("SVR Unknown protocol ID (ignored): {}", proto_id);
            continue; // unknown or unsupported protocol
        }

        // Validate role, eCore proto is the only one with "server" role, all other protocols use "store" as role
        const auto& role = req_proto.m_role;
        if (proto_id == static_cast<int32_t>(ProtoEnum::eCore))
        {
            if (role != "server") {
                OES_ERROR("SVR Unexpected role for Core(0) protocol: {}", role);
                continue;
            }
        }
        else
        {
            if (role != "store") {
                OES_ERROR("SVR Unexpected protocol role (ignored): {}, {}", proto_id, role);
                continue;
            }
        }

        // Check for duplicates in the requested protocols
        const auto existProtoIt = std::find_if(res.m_supportedProtocols.cbegin(), res.m_supportedProtocols.cend(), [proto_id](auto& proto) {
            return proto.m_protocol == static_cast<int>(proto_id);
        });
        if (existProtoIt != res.m_supportedProtocols.cend()) {
            // No support for multiple roles.
            // Either a "client" or a "server", but not both.
            OES_VERBOSE(
                "SVR Duplicate requested protocol (ignored): {}",
                proto_id
            );
            continue;
        }

        // Check version compatibility here.
        const data12::Version& v = req_proto.m_protocolVersion;
        const data12::Version& server_v = (*server_proto).m_protocolVersion ;
        if (v.m_major != server_v.m_major ||
            v.m_minor != server_v.m_minor ||
            (v.m_patch < 0 && v.m_patch > server_v.m_patch) ||
            (v.m_revision < 0 && v.m_revision > server_v.m_revision)) {
            OES_ERROR(
                "[{}] SVR Unexpected protocol version (ignored): {}, v{}.{}.{}.{}",
                client_identification_, proto_id, v.m_major, v.m_minor, v.m_patch, v.m_revision
            );
            continue;
        }
            
        // Adjust protocol capabilities.
        impl->adjustProtocolCapabilities(req_proto, *server_proto);

        res.m_supportedProtocols.emplace_back(std::move(req_proto));
        OES_DEBUG(
            "SVR Supports requested protocol: {}, {}, v{}.{}.{}.{}",
            proto_id, role, v.m_major, v.m_minor, v.m_patch, v.m_revision
        );
    }

    if (res.m_supportedProtocols.empty()) {
        proto12::ProtocolException ex;
        Datatypes::ErrorInfo info;
        info.m_code = data12::ENOSUPPORTEDPROTOCOLS;
        info.m_message = "None of the requested protocols are supported.";
        ex.m_error = std::move(info);
        throw ex;
    }

    adjustedProtocols_ = res.m_supportedProtocols;

    // Cap#1 MaxWebSocketMessagePayloadSize
    // Negotiate the "max-message-size" value for this session,
    // based on the minimum of the client and server maxima.
    size_t client_max_msg_size = 0;
    size_t server_max_msg_size = impl->config_->max_mgs_size_;
    constexpr char max_msg_size_key[] ="MaxWebSocketMessagePayloadSize";
    auto max_msg_found = req.m_endpointCapabilities.find(max_msg_size_key);
    if (max_msg_found != req.m_endpointCapabilities.end()) {
        const data12::DataValue_item_v& val = max_msg_found->second.m_item;
        switch (val.index()) {
        case size_t(data12::DataValue_item_e::eLong):   client_max_msg_size = size_t(std::get<int64_t>(val)); break;
        case size_t(data12::DataValue_item_e::eInt):    client_max_msg_size = size_t(std::get<int>(val)); break;
        default:
            OES_ERROR(
                "[{}] Unexpected {} value type (not integer)",
                client_identification_,
                max_msg_size_key
            );
        }
        if (size_t N = client_max_msg_size) {
            OES_DEBUG(
                "SVR Client  {} = {}", max_msg_size_key, thou(N)
            );
        }
    } else {
        OES_VERBOSE(
            "SVR No Client-advertised {}", max_msg_size_key
        );
    }

    OES_DEBUG(
        "SVR Server  {} = {}", max_msg_size_key, thou(server_max_msg_size)
    );

    size_t session_max_msg_size = server_max_msg_size;
    if (client_max_msg_size) {
        session_max_msg_size = std::min<>(client_max_msg_size, server_max_msg_size);
    }
    if (session_max_msg_size) {
        auto& val = res.m_endpointCapabilities[max_msg_size_key];
        val.m_item = int64_t(session_max_msg_size);
        negotiated_max_msg_size_ = session_max_msg_size;

        OES_VERBOSE(
            "SVR Session {} = {}", max_msg_size_key, thou(session_max_msg_size)
        );
    }

    // Cap#2 MaxBatchSize
    // Warning: Not ETP standard
    // Max-Batch-Size, i.e. when responding,
    // do not return more that this many items per message.
    max_batch_size_ = impl->config_->max_batch_size_;
    check_activities_ = impl->config_->check_activities_;
    pg_bulk_size_ = impl->config_->pg_bulk_size_;
    OES_VERBOSE(
        "SVR Session MaxBatchSize = {}", thou(max_batch_size_)
    );

    // Negotiate compression
    const auto& clt_zcomp = req.m_supportedCompression;
    const auto& svr_zcomp = impl->config_->server_capabilities_.m_supportedCompression;

    res.m_supportedCompression.clear();
    if (clt_zcomp.empty() || svr_zcomp.empty()) {
        // No compression possible
    } else {
        // Both endpoints support compression. Find the common ones.
        // Note that they are ordered, so the first common one is the one.
        for (const auto& zcomp : clt_zcomp) {
            auto zcomp_found = std::find(svr_zcomp.begin(), svr_zcomp.end(), zcomp);
            if (zcomp_found != svr_zcomp.end()) {
                res.m_supportedCompression = zcomp;
                negotiated_compression_ = zcomp;
                break; // no need for more, first match is it
            }
        }
    }

    // Inits state_->zcompressor_, so must be before verbose message below
    state_->init_negotiated(*this);

    if (!res.m_supportedCompression.empty()) {
        assert(state_->zcompressor_);
        OES_VERBOSE(
            "SVR Session Compression = {} ({})",
            res.m_supportedCompression, state_->zcompressor_->name_
        );
    }

    state_->status_ = State::Status::Open;

    OES_INFO("SVR Thread({}) Session({}) from [{}] OPEN",  
        thisThreadId(), id_, client_identification_
    );
}

void Session::onResume(proto12::CoreOSDU::ResumeSession req,
                       proto12::CoreOSDU::ResumeSessionResponse& res) {
    OES_ASSERT(state_->status_ == State::Status::Initial, throw std::runtime_error("Session already open"););

    auto state_impl = state_->wimpl_.lock();
    if(!state_impl) {
        throw std::runtime_error("Invalid state");
    }

    if (!user_ || (entitlement_ && !(entitlement_->isValid())) ) {
        onAuthN(handshake_.authorization_);
        if (!user_ || (entitlement_ && !(entitlement_->isValid()))) {
            proto12::ProtocolException ex;
            Datatypes::ErrorInfo info;
            info.m_code = data12::EAUTHORIZATION_REQUIRED;
            info.m_message = "Authorization has not been provided.";
            ex.m_error = std::move(info);
            throw ex;
        }
    }

    OES_VERBOSE(
        "SVR Application:  {}\n"
        "SVR App-Version:  {}",
        req.m_applicationName,
        req.m_applicationVersion
    );
    Guid client_uid;
    assignUid(client_uid, req.m_clientInstanceId);
    OES_DEBUG(
        "SVR Client-UUID:  {}\n"
        "SVR Server-UUID:  {}\n"
        "SVR Session-UUID: {}",
        client_uid.toXmlString(),
        state_impl->server_uid_.toXmlString(),
        handshake_.session_id_
    );
    
    res.m_applicationName = state_impl->config_->server_capabilities_.m_applicationName;
    res.m_applicationVersion = state_impl->config_->server_capabilities_.m_applicationVersion;
    
    Guid guid;
    oes::common::assignUid(guid, req.m_sessionId);
    const std::string uuid_str = guid.toStringNoDash();
    OES_WARNING("UUid to recover: {}", uuid_str);

    // migrate the state...
    EtpResponseStatus rc;
    auto found = state_impl->ws_lost_sessions_map_.find(uuid_str);
    if (found == state_impl->ws_lost_sessions_map_.end()) {
        proto12::ProtocolException ex;
        Datatypes::ErrorInfo info;
        info.m_code = data12::ENOT_FOUND;
        info.m_message = "Session not found or expired";

        ex.m_error = std::move(info);
        throw ex;
    }

    auto node = state_impl->ws_lost_sessions_map_.extract(uuid_str);

    std::lock_guard lock(state_impl->lost_sessions_mt_);
    // auto& lost_session = found->second.conn_info_.session_;
    auto& lost_session = node.mapped().conn_info_.session_;

    resumeFromExistingSession(lost_session);
    oes::common::assignUid(res.m_sessionId, Guid::parse(handshake_.session_id_));
}

void Session::resumeFromExistingSession(std::shared_ptr<Session> other) {
    // Transfer core session properties
    id_ = other->id_;
    label_ = other->label_;
    client_identification_ = other->client_identification_;
    
    // Transfer negotiated properties
    negotiated_max_msg_size_ = other->negotiated_max_msg_size_;
    negotiated_compression_ = other->negotiated_compression_;
    max_batch_size_ = other->max_batch_size_;
    check_activities_ = other->check_activities_;
    pg_bulk_size_ = other->pg_bulk_size_;
    threads_count_ = other->threads_count_;
    
    db_connection_pool_ = std::move(other->db_connection_pool_);
    adjustedProtocols_ = std::move(other->adjustedProtocols_);
    
    OES_INFO("SVR Thread({}) Session({}) RESUMED from existing session", 
        thisThreadId(), id_
    );
}

/*!
 * \brief Equivalent to Core(0).Authorize(6).
 *
 * \param token the security token to authorize with
 */
void Session::onAuthN(std::string token) {
   
    // TODO
    // if( token.empty() ) {
    //     proto12::ProtocolException ex;
    //     Datatypes::ErrorInfo info;
    //     info.m_code = data12::EAUTHORIZATION_REQUIRED;
    //     info.m_message = "Authorize token not provided";
    //     ex.m_error = std::move(info);
    //     throw ex;
    // }

    handshake_.authorization_ = token;

    Impl_ptr impl = state_->wimpl_.lock();
    OES_ASSERT(impl, return;);

    // encapsulate the user_ initialization in a try/catch block
    // so that we can send an error message back to the client
    try {
        user_ = impl->config_->user_factory_(handshake_);
        // Some older clients do not provide the partition ID, but it may be enclosed in the token.
        // At this stage the token has been validated, so we can rely on it.
        if( handshake_.data_partition_id_.empty() && ! user_->partition_id_.empty() ) {
            OES_INFO("SVR Thread({}) Session({}) Session::onAuthN: got missing partition Id {} from user.", thisThreadId(), id_,  user_->partition_id_);
            handshake_.data_partition_id_ = user_->partition_id_;
        }
        entitlement_ = impl->config_->entitlement_factory_(handshake_, user_);
    } catch (const std::exception& e) {
        proto12::ProtocolException ex;
        Datatypes::ErrorInfo info;
        info.m_code = data12::EAUTHORIZATION_REQUIRED;
        info.m_message = e.what();
        ex.m_error = std::move(info);
        throw ex;
    }
    
    std::string user_name = "ANON";
    if (user_) {
        if (!user_->name_.empty()) {
            user_name = user_->name_;
        }
    }

    label_ = fmt::format("{}@{}", user_name, handshake_.remote_endpoint_);
}

/*!
 * \brief Equivalent to Core(0).CloseSession(5).
 *
 * \param reason the reason for closing this session. May be empty.
 * \param from_ws true when the WebSocket is closed at the transport level.
 *     false otherwise, when receiving Core(0).CloseSession(5) ETP message.
 *
 * \sa close()
 */
void Session::onClose(std::string reason, bool from_ws) {
    OES_ASSERT(state_->status_ != State::Status::Closed, throw std::runtime_error("Session not open"););

    OES_UNUSED(reason);
    OES_UNUSED(from_ws);
}

Session::State_ptr Session::getState() {
    return state_;
}

/*!
 * \brief Whether this session is open.
 * \return true between onStart() and close(); false otherwise.
 */
bool Session::isOpen() const {
    return state_->status_ == State::Status::Open;
}

/*!
 * \brief Whether this session is closed.
 * \return true after close(); false otherwise.
 */
bool Session::isClosed() const {
    return state_->status_ == State::Status::Closed;
}

/*!
 * \brief Whether this session is bound to an Server instance.
 *
 * \return true if bound to a server, therefore heap-allocated;
 *     false if not bound, thus either heap-or-stack-allocated.
 *     Stack-allocation is mostly for unit-testing purposes only.
 */
bool Session::boundToServer() const {
    OES_ASSERT(state_->status_ == State::Status::Open, throw std::runtime_error("Session not open"););

    return state_->wimpl_.lock() != nullptr;
}

/*!
 * \brief Explicitly ask this session to close / terminate.
 *
 * Will first call onClose(), to allow possibly derived-specific parts of
 * this session to free resources early. A \em closed session cannot be \em re-started.
 *
 * \param reason the possibly non-empty reason why the session was closed.
 *     When \a from_ws is false, this reason is coming from the ETP message,
 *     thus is it not limited in length; while when true, it is coming from
 *     the WebSocket protocol, and thus limited to 125 bytes (assumed to be UTF-8)
 * \param from_ws Whether the close \a reason is from the ETP CloseSession message
 *     (when true), or from the WebSocket protocol.
 * 
 */
void Session::close(std::string reason, bool from_ws) {
    OES_ASSERT(state_->status_ != State::Status::Closed, throw std::runtime_error("Session not open"););

    if (!from_ws) {
        // Wait for any pending messages that may still be processing in other threads
        const std::chrono::milliseconds timeout(5000);
        std::unique_lock<std::mutex> lock(state_->rpcs_mt_);
        if(!state_->rpc_cv_.wait_for(lock, timeout, [this] { 
            return (state_->rpcs_->size() < 2); 
        })) {
            OES_ERROR(
                "[{}] SVR Thread({}) Session({}) CLOSE TIMEOUT: Waiting for pending messages to be processed", 
                client_identification_, thisThreadId(), id_
            );
        }
        lock.unlock();
    }

    onClose(reason, from_ws);

    /*
    ** Following ETP specification chapter 5.2.1.2, the side receiving closeConnection message
    ** must gracefully close the connection
    */
    if (!from_ws) {
        // Ask WebSocketPP to gracefully close the connection
        Impl_ptr impl = state_->wimpl_.lock();
        OES_ASSERT(impl, return;);

        try {
            ws_conn_ptr_t conn = impl->get_conn(state_->wconn_);
            const auto state  = conn->get_state();
            // Connection can already be closing because of client shutting down
            if( state == websocketpp::session::state::open ){
                // note: under lock in on_close(), so no need to lock impl->ws_server_mt_
                // (since close() calls get_con_from_hdl())
                websocketpp::lib::error_code ec;
                impl->ws_server_.close(
                    state_->wconn_, websocketpp::close::status::going_away, reason, ec
                );
                if (ec) {
                    OES_ERROR(
                        "[{}] Unable to close WebSocket connection: {}",
                        client_identification_,
                        ec.message()
                    );
                }
            }
        } catch (const std::exception& ex) {
            OES_ERROR("[{}] SVR Thread({}) Session({}) CLOSE FAILED. Reason: {}", 
                client_identification_, thisThreadId(), id_, ex.what()
            );
        }
    }

    // Cannot yet delete this session's state, because
    // CoreProtoImpl::CloseSession calls the "done" handler
    // which does tracing and "stuff" that depend on state_.
    // Also used in Impl::on_close() to show session stats.
    //state_.reset();

    state_->session_back_pointer_.reset();
    state_->status_ = State::Status::Closed;

    OES_INFO("SVR Thread({}) Session({}) from [{}] CLOSED {}{}",
        thisThreadId(), id_, client_identification_, from_ws ? "FORCEFULLY" : "NORMALLY", hasTransaction()? "(WITH ROLLBACK)" : ""
    );
}

// ============================================================================

// Session::State& Session::State::operator=(Session::State& other) {
//     sid_ = other.sid_;
//     is_avro_json_ = other.is_avro_json_;

//     max_msg_size_ = other.max_msg_size_;
//     max_msg_usize_ = other.max_msg_usize_;
//     max_batch_size_ = other.max_batch_size_;

//     zcompressor_ = other.zcompressor_;
//     other.zcompressor_ = nullptr;

//     // session_back_pointer_;
//     // wimpl_;
//     // wconn_;

//     //? recv_strand_
//     //? proc_strand_



// }

void Session::State::init(
    std::weak_ptr<Session> wsession, const Impl_ptr& impl,
    const ws_conn_hdl_t& hdl, boost::asio::io_service& asio_service, boost::asio::io_service& worker_service
) {
    session_back_pointer_ = wsession; // "back-pointer" to owner-session
    wimpl_ = impl;
    wconn_ = hdl;

    const std::shared_ptr<Session> session = wsession.lock();
    // Copy some values we might need when the session itself is gone,
    // because tasks buffered on strands may outlive the session itself,
    // but keep the session's State alive via a shared_ptr
    // Copies "static" values, "negotiated / dynamic" ones copied later.
    sid_ = session->id_;
    is_avro_json_ = (session->handshake_.etp_encoding_ == "json");

    max_msg_usize_ = impl->config_->max_mgs_usize_;

    // The various stands we need to dispatch tasks on
    recv_strand_.reset(new boost::asio::io_context::strand(asio_service));
    proc_strand_.reset(new boost::asio::io_context::strand(worker_service));

    // No negotiation occurred yet => use server config
    rpcs_.reset(new PendingRpcs());
}

// Copies "negotiated / dynamic" values.
void Session::State::init_negotiated(const Session& session) {
    max_msg_size_ = session.negotiated_max_msg_size_;
    max_batch_size_ = session.max_batch_size_;
    client_identification_ = session.client_identification_;

    const std::string& zcomp = session.negotiated_compression_;
    if (zcomp.empty()) {
        // nothing to do
    } else if (zcomp == "gzip") {
        zcompressor_ = &GZipCompressor;
    } else {
        assert(false); // should not happen
    }
}

PendingRpc_ptr Session::State::acquire_rpc() {
    std::lock_guard lock(rpcs_mt_);
    rpc_proc_at_ = now();
    if (PendingRpc_ptr rpc = rpcs_->acquire_rpc()) {
        rpc_cv_.notify_all();
        return PendingRpc_ptr(shared_from_this(), rpc.get()); // aliasing Ctor
    }
    return nullptr;
}

void Session::State::release_rpc(const PendingRpc_ptr& rpc, bool temporary_rpc) {
    assert(proc_strand_->running_in_this_thread() || recv_strand_->running_in_this_thread());

    {
        std::lock_guard lk(proc_mt_);
        proc_.maybeAddToTopN(*rpc, sid_);
    }

    std::lock_guard lock(rpcs_mt_);

    try {
        const auto done_at = now();
        const unsigned tid = thisThreadId();
        const duration_milli_t millis = done_at - rpc_proc_at_;

        rpc->set_proc_time(millis, tid);

        {
            std::lock_guard lk(proc_mt_);
            proc_.add(millis);
        }

        Impl_ptr impl = wimpl_.lock();
        OES_ASSERT_MSG(impl, "Server alive", return;);
        
        if (EtpTracer12*const tracer = impl->tracer_.get()) {
            EtpTracerEventInfo evt;
            evt.beg_ = rpc_proc_at_;
            evt.end_ = done_at;
            evt.tid_ = tid;
            evt.sid_ = sid_;
            evt.set_phase(EtpTracerEventInfo::ePROC);
            evt.set_encoding(EtpTracerEventInfo::eJSON);
            evt.msg_ = fmt::format(
                "[{},\"PROC {}\"]",
                rpc->req_hdr_.m_messageId, msgHeaderAsString(rpc->req_hdr_)
            );
            tracer->async_trace(std::move(evt));
        }

        // Put this log line in verbose mode, as it effectively doubles the number of log lines for
        // each request termination.
        const auto replies = rpc->replies_state();
        OES_VERBOSE(
            "SVR PROC Thread({}) Session({}) {} ({:.1f}ms, req {} B, res {} B)",
            tid, sid_, msgHeaderAsString(rpc->req_hdr_), millis.count(),
            oes::core::thou(rpc->req_bytes_), oes::core::thou(replies.res_bytes_)
        );

        // Request size is one received frame, recorded here per RPC. Response size is
        // recorded per sent frame in log_and_trace_post_send() so multipart replies are
        // not conflated into one oversized sample.
        try {
            oes::core::Metrics::instance().getHistogram(oes::core::metrics::ETP_REQUEST_BYTES)
                .Observe(static_cast<double>(rpc->req_bytes_));
        } catch (const oes::core::MetricNotFound&) {
            // Metrics not registered (e.g. in tests); ignore.
        }
    }  catch (const std::exception& ex) {
        OES_ERROR("Failed to send trace event (on release RPC): {}", ex.what());
    }  catch (...) {
        OES_ERROR("Failed to send trace event (on release RPC): Unknown exception");
    }

    if (rpcs_->release_rpc(rpc)) {
        rpc_cv_.notify_all();
        if (!temporary_rpc) {
            assert(same_owner(*this, rpc));
        }
    } else {
        // Temporary rpc (with its own shared_ptr ref-counter), because we
        // could not acquire_rpc(). See Impl::on_message()'s if (!p_rpc)
        assert(!same_owner(*this, rpc));
    }
}

void Session::State::send( // send here
    const PendingRpc_ptr& p_rpc,
    MessageHeader& hdr, EtpWrappedResponse wres
) {
    assert(proc_strand_->running_in_this_thread() || recv_strand_->running_in_this_thread());

    const auto send_at = now();

    Impl_ptr impl = wimpl_.lock();
    OES_ASSERT(impl, return;);

    ws_conn_ptr_t conn;
    try {
        conn = impl->get_conn(wconn_);
    } catch (const std::exception& e) {
        OES_ERROR("SVR send failed Thread({}) Session({}) Reason: {}", 
            thisThreadId(), sid_, e.what()
        );
        return;
    }

    EtpTracerEventInfo evt;
    EtpTracer12*const tracer = impl->tracer_.get();

    bool send_ok = false;
    std::string err_msg;

    try {
        send_ok = doEncodeAndSend(
            conn, hdr, wres, send_.last_outbound_msg_id_,
            max_msg_size_, std::min<size_t>(4*1204,max_msg_size_-1), zcompressor_,
            [&tracer, &evt](const MessageHeader&) {
                return tracer? &evt.msg_: nullptr;
            },
            [&](const MessageHeader& hdr, const EncodeAndSendRc& rc) {
                log_and_trace_post_send(conn, tracer, p_rpc, hdr, rc, evt);
            }
        );
    } catch (const std::exception& ex) {
        // E.g. Message could not be split (typically)
        err_msg = ex.what();
    } catch (...) {
        err_msg = "Unknown error";
    }

    if (!send_ok) {
        // Check if err_msg starts with "Oversize #"
        // (which is the only error we can recover from)
        if( err_msg.compare(0, 10, "Oversize #") == 0 ){
            throw std::runtime_error(err_msg);
        }

        // Message could not be split?
        ProtocolException ex;
        auto& err = ex.m_error.emplace();
        err.m_code = Datatypes::EINVALID_STATE;
        err.m_message = fmt::format(
            "Could not encode+send {} (-M {}): {}",
            msgHeaderAsString(hdr), max_msg_size_, err_msg
        );
        hdr.m_messageType = ProtocolException::info.type_.type_;
        set_FIN_bit(hdr); // Might have been removed if split
        unset_ZIP_bit(hdr); // Just in case
        return send_error(p_rpc, hdr, ex);
    }

    {
        const auto sent_at = now(); // well, queued to be sent by transport, really
        const duration_milli_t millis = sent_at - send_at;

        std::lock_guard lk(send_mt_);
        send_.send_time_ms_.add(millis.count());
    }

    if (has_FIN_bit(hdr)) {
        release_rpc(p_rpc);
    }
}

void Session::State::log_and_trace_post_send(
    ws_conn_ptr_t& conn,
    EtpTracer12*const tracer,
    const PendingRpc_ptr& p_rpc,
    const MessageHeader& hdr,
    const EncodeAndSendRc& rc,
    EtpTracerEventInfo& evt
) {
    // FYI: 1) Update stats;  2) Tracing;  3) Logging.

    const auto sent_at = now(); // well, queued to be sent by transport, really

    {
        std::lock_guard lk(send_mt_);
        send_.bytes_sent_.add(rc.bytes_sent_);
        send_.body_usize_.add(rc.body_bytes_);
        send_.header_bytes_ += rc.header_bytes_;
        send_.last_res_sent_ = sent_at;

        if (rc.deflate_millis_ > 0) {
            send_.defl_time_ms_.add(rc.deflate_millis_);
        }
    }

    // We want to record the fact we send a message (and thus some bytes)
    // to acknowledge a request's reception, but not the time ACK was sent,
    // as that would skew the "real" time-to-first-response, i.e. the latency.
    const bool is_ack = (hdr.m_messageType == Acknowledge::info.type_.type_);
    p_rpc->add_reply(rc.bytes_sent_, is_ack? zero_htp: sent_at);

    // Record each sent response frame's on-the-wire size (per message, not per RPC), so a
    // multipart reply produces one sample per frame instead of a single summed sample.
    if (!is_ack) {
        try {
            oes::core::Metrics::instance().getHistogram(oes::core::metrics::ETP_RESPONSE_BYTES)
                .Observe(static_cast<double>(rc.bytes_sent_));
        } catch (const oes::core::MetricNotFound&) {
            // Metrics not registered (e.g. in tests); ignore.
        }
    }

    if (!tracer && oes::core::Logger::instance().minSeverityFilter() > oes::core::Message::Severity::debug) {
        return;
    }

    const unsigned tid = thisThreadId();

    if (tracer) {
        evt.set_point_event(sent_at);
        evt.tid_ = tid;
        evt.sid_ = sid_;
        evt.set_phase(EtpTracerEventInfo::eSEND);
        evt.set_encoding(
            p_rpc->txt_msg_?
                EtpTracerEventInfo::eAVRO_JSON:
                EtpTracerEventInfo::eAVRO_BINARY
        );
        // Partial (because header is never compressed) compression from ETP is separate
        // This is about the full-tracing event itself being compressed, and it is not.
        //evt.set_compression(EtpTracerEventInfo::eUNCOMPRESSED); i.e. the default
        tracer->async_trace(std::move(evt));
    }

    if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug) {
        if (rc.compressed_) {
            OES_DEBUG(
                "SVR DEFL Thread({}) Session({}) {} ({:.1f}ms) {} ({:.1f}x)",
                tid, sid_, msgHeaderAsString(hdr), rc.deflate_millis_,
                thou(rc.body_bytes_), rc.body_zfactor()
            );
        }

        // Warning: Call below takes the write-lock on the socket
        const size_t bufd_size = conn->get_buffered_amount();
        std::string buffered;
        if (bufd_size == 0) {
            // async-send on connection already "consumed" the message
            // by actually sending it on the wire. Common case.
        } else if (bufd_size == rc.bytes_sent_) {
            // async-send on connection is not "backed up", but did already "consume"
            // the message we just queued-up for sending. Most common case.
            buffered = ", BUF 1x";
        } else if (bufd_size > rc.bytes_sent_) {
            // connection has at least 2 messages still waiting to the sent,
            // the one we just queued-up, and another one.
            // TODO: Apply Back-Pressure if too large
            buffered = fmt::format(", BUF +{} bytes", bufd_size - rc.bytes_sent_);
        } else {
            // not sure when and if this would happen
            buffered = fmt::format(", BUF {} bytes", bufd_size);
        }

        const duration_milli_t millis = sent_at - p_rpc->sock_at_;

        std::string parts;
        if (rc.was_split()) {
            parts = fmt::format(
                " {} of {} parts",
                rc.actual_part_count_, rc.total_part_count_
            );
        } else {
            switch (size_t N = rc.total_part_count_) {
            case  0: break;
            case  1: parts = " 1 part"; break;
            default: parts = fmt::format(" {} parts", N); break;
            }
        }

        OES_DEBUG(
            "SVR SEND Thread({}) Session({}) {} {} bytes ({:.1f}ms later{}){}",
            tid, sid_, p_rpc->txt_msg_? "TXT": "BIN",
            rc.bytes_sent_, millis.count(), buffered, parts
        );
    }
}

void Session::State::send_error(
    const PendingRpc_ptr& p_rpc,
    MessageHeader& ex_hdr, const ProtocolException& ex,
    bool partial_success
) {
    auto enc = [&ex](::avro::Encoder& encoder) {
        ex.encode(encoder);
    };
    send(p_rpc, ex_hdr, enc);
    {
        std::lock_guard lk(send_mt_);
        ++send_.errors_sent_;
    }

    std::string msg = log_error(
        thisThreadId(), sid_,
        partial_success, ex_hdr, ex, '>',
        client_identification_
    );
    OES_ERROR(msg);
}

void Session::State::post_send_error(
    const PendingRpc_ptr& p_rpc,
    int32_t err_code, std::string err_msg
) {
    const PendingRpc& rpc = *p_rpc;

    MessageHeader ex_hdr;
    ex_hdr.m_protocol = rpc.req_hdr_.m_protocol; // ProtoEx can be part of any protocol
    ex_hdr.m_messageType = ProtocolException::info.type_.type_;
    ex_hdr.m_correlationId = rpc.req_hdr_.m_messageId;
    set_FIN_bit(ex_hdr);

    Datatypes::ErrorInfo info;
    info.m_code = err_code;
    info.m_message = std::move(err_msg);

    // Using shared_ptr (i.e. heap) exception, because ASIO post() copies things.
    auto p_ex = std::make_shared<ProtocolException>();
    p_ex->m_error = std::move(info);

    send_error(p_rpc, ex_hdr, *p_ex);
}

void Session::State::send_acknowledge(
    const PendingRpc_ptr& p_rpc, MessageHeader& ack_hdr
) {
    Acknowledge ack; // empty message...

    auto enc = [&ack](::avro::Encoder& encoder) {
        ack.encode(encoder);
    };
    send(p_rpc, ack_hdr, enc);

    OES_INFO(
        "[{}] SVR >ACK Thread({}) Session({}) {}", // or show p_rpc->req_hdr_?
        client_identification_,
        thisThreadId(), sid_, msgHeaderAsString(ack_hdr, nullptr, true)
    );
}

void Session::State::post_send_acknowledge(
    const PendingRpc_ptr& p_rpc
) {
    MessageHeader ack_hdr;
    ack_hdr.m_protocol = p_rpc->req_hdr_.m_protocol; // ProtoEx can be part of any protocol
    ack_hdr.m_messageType = Acknowledge::info.type_.type_;
    ack_hdr.m_correlationId = p_rpc->req_hdr_.m_messageId;
    ack_hdr.m_messageFlags = 0; // i.e. no flags at all

    send_acknowledge(p_rpc, ack_hdr);
}

/*!
 * \brief Handles a new WebSocket message.
 *
 * This is the primary \em input to the ETP server, and this implementation
 * decodes and dispatches the message according to the ETP 1.2 specification.
 *
 * Note that this particular method only decodes the ETP header, performing
 * a first series of validations on that header, then it delegates the rest
 * of the processing (decompression, deserialization, dispatching/handling,
 * etc...) to a separate async (internal) method, \a handle_req().
 *
 * This means that this method will return quickly, BEFORE the request
 * was fully handled (asynchronously).
 */
void Session::State::handle_msg(const PendingRpc_ptr& p_rpc) {
    assert(recv_strand_->running_in_this_thread());

    if(status_ == Status::AboutToClose){
        return post_send_error(
            p_rpc, Datatypes::EINVALID_STATE,
            "Unable to handle websocket message. Session is about to close."
        );
    } else if(status_ == Status::Closed) {
        OES_ERROR("Unable to handle websocket message. Session is closed.");
        return;
    }

    const auto recv_at = now();

    PendingRpc& rpc = *p_rpc;

    rpc.init();

    OES_ASSERT(rpc.txt_msg_ == is_avro_json_, ;);

    // First, we decode the message header, always uncompressed
    const std::string head_err = rpc.decode_header();
    if (!head_err.empty()) {
        return post_send_error(
            p_rpc, Datatypes::EINVALID_MESSAGE,
            fmt::format("Cannot Avro-decode message header: {}", head_err)
        );
    }

    // How many bytes did it take to decode the header?
    rpc.decoder_->drain();
    const size_t header_bytes = rpc.buf_.byteCount();

    MessageHeader& msg_hdr = rpc.req_hdr_;
    
    // We are at the close signal, ignore any subsequent msg
    if(msg_hdr.m_protocol == static_cast<int>(ProtoEnum::eCore) && msg_hdr.m_messageType == CloseSession::info.type_.type_) {
        status_ = Status::AboutToClose;
    }

    if (client_identification_.empty()) {
        OES_INFO(
            "SVR <REQ Thread({}) Session({}) {} -{} ({} B)",
            thisThreadId(), sid_, msgHeaderAsString(msg_hdr, nullptr, true), header_bytes,
            oes::core::thou(rpc.req_bytes_)
        );
    } else {
        OES_INFO(
            "[{}] SVR <REQ Thread({}) Session({}) {} -{} ({} B)",
            client_identification_, thisThreadId(), sid_, msgHeaderAsString(msg_hdr, nullptr, true), header_bytes,
            oes::core::thou(rpc.req_bytes_)
        );
    }

    // Send this *RIGHT AWAY* after the MessageHeader is successfully decoded,
    // even before the many possible errors that can occur before the "real"
    // processing begins. That's how the ETP guys want it...
    // See https://3.basecamp.com/3247751/buckets/838608/messages/3630364731
    if (has_ACK_bit(msg_hdr)) {
        post_send_acknowledge(p_rpc);
        // no return, processing continues
    }

    if (!rpc.msg_info_) {
        return post_send_error(
            p_rpc, Datatypes::EINVALID_MESSAGETYPE,
            "Unknown message type"
        );
    }

    // TOOD: Check session was already opened (REQUEST_DENIED),
    //       if this request is not Core.RequestSession itself.
    // TODO: Check session token has not expired (EEXPIRED_TOKEN)

    // Next, we verify client incoming messages have even IDs
    if (msg_hdr.m_messageId & 0x1) {
        return post_send_error(
            p_rpc, Datatypes::EINVALID_ARGUMENT,
            "Invalid Message-ID: Not Even"
        );
    }

    // And that those IDs are monotonically increasing
    {
        std::lock_guard lock(recv_mt_);
        if (msg_hdr.m_messageId <= recv_.last_inbound_msg_id_) {
            return post_send_error(
                p_rpc, Datatypes::EINVALID_ARGUMENT,
                "Invalid Message-ID: Not monotonically increasing"
            );
        }
        recv_.last_inbound_msg_id_ = msg_hdr.m_messageId;
        recv_.header_bytes_ += header_bytes;
    }

    // Uncompress the body, if needed
    if (has_ZIP_bit(msg_hdr)) {
        // Core.RequestSession (or its responses, see 5.2.2 #7)
        // should not be compressed. But then, zcompressor_ will not
        // have been initialized, so test below should cover that case.
        if (!zcompressor_) {
            return post_send_error(
                p_rpc, Datatypes::ECOMPRESSION_NOTSUPPORTED,
                "Compression not negotiated"
            );
        }

        const auto znow = now();

        // Substring this message, to decompress only the body
        std::string_view body(rpc.raw_msg_);
        body = body.substr(header_bytes);

        // Decompress the message's body
        std::string ubody;
        auto [zok, ztail] = zcompressor_->inflate2(body, ubody);

        const duration_milli_t zdur = now() - znow;

        {
            std::unique_lock lock(recv_mt_);
            recv_.infl_time_ms_.add(zdur.count());
            recv_.body_usize_.add(ubody.size());
        }

        if (!zok) {
            return post_send_error(
                p_rpc, Datatypes::EINVALID_MESSAGE,
                "Could not decompress body"
            );
        }

        OES_DEBUG(
            "SVR INFL Thread({}) Session({}) {} ({:.1f}ms) {} ({:.1f}x) +{}",
            thisThreadId(), sid_, msgHeaderAsString(msg_hdr), zdur.count(),
            thou(ubody.size()), (1. * ubody.size() / body.size()), ztail
        );

        // Install this uncompressed body, for later decoding
        rpc.init_body(ubody);
    }

    // And there is no extended header between header and body (still a TODO)
    if (has_EXT_bit(msg_hdr)) {
        return post_send_error(
            p_rpc, Datatypes::ENOTSUPPORTED,
            "MessageHeaderExtension not supported yet"
        );
    }

    // ProtocolException is special, in that it can be sent on any protocol
    // The fact the client sends us a ProtocolException means either:
    // 1) we are buggy and sent the client an invalid message
    // 2) the client itself is buggy.
    // either way, there's not much we can do...
    if (msg_hdr.m_messageType == ProtocolException::info.type_.type_) {
        ProtocolException ex;
        try {
            ex.decode(*rpc.decoder_);
        } catch (...) {
            EtpResponseStatus rc = errorStatus(
                Datatypes::EINVALID_MESSAGE,
                "Cannot Avro-decode Client-sent ProtocolException"
            );
            ex = std::move(rc.ex);
        }

        std::string msg = log_error(
            thisThreadId(), sid_, false,
            msg_hdr, ex, '<',
            client_identification_);
        OES_DEBUG(msg);
        OES_DEBUG("SVR <ERR Invalid Client-sent ProtocolException ignored");
        return;
    }

    // Instantiate protocol
    Impl_ptr impl = wimpl_.lock();
    OES_ASSERT_MSG(impl, "Server alive", return;);

    if(!get_session().lock().get()){
        OES_ERROR(
            "[{}] SVR PROC Thread({}) Session({}): Session has been forcefully terminated.",
            client_identification_, thisThreadId(), sid_
        );
        return;
    }

    auto proto = impl->makeProtocol(static_cast<ProtoEnum>(msg_hdr.m_protocol), *this);
    if (!proto) {
        return post_send_error(
            p_rpc, Datatypes::EUNSUPPORTED_PROTOCOL, fmt::format(
                "Unsupported protocol: {}", msg_hdr.m_protocol
            )
        );
    }

    // TODO: Use rpc.msg_info_ instead, to avoid another lookup
    //       Note also we also want to know whether the message is void or not
    rpc.expected_response_type_ = proto->responseMessageType(msg_hdr.m_messageType);
    const int resMsgTypeId = rpc.expected_response_type_;
    if (resMsgTypeId < 0) {
        return post_send_error(
            p_rpc, Datatypes::EINVALID_MESSAGETYPE, fmt::format(
                "Unexpected message: {}.{}", proto->name_id_, msg_hdr.m_messageType
            )
        );
    }

    // Enforce FIN-bit presence for "single-part" request
    if (!rpc.msg_info_->isMultiPart() && !has_FIN_bit(msg_hdr)) {
        return post_send_error(
            p_rpc, Datatypes::EINVALID_ARGUMENT,
            "Non-Multi-Part request lacks FIN bit"
        );
    }

    const std::string body_err = rpc.decode_request();
    if (!body_err.empty()) {
        return post_send_error(
            p_rpc, Datatypes::EINVALID_MESSAGE,
            fmt::format("Cannot Avro-decode request: {}", body_err)
        );
    }

    assert(proc_strand_);
    proc_strand_->post(
        [weakState = weak_from_this(), weakRpc = std::weak_ptr<PendingRpc>(p_rpc), proto = std::move(proto)]() mutable {
            auto state = weakState.lock();
            auto rpc = weakRpc.lock();
            if (state && rpc) {
                auto session_ptr = state->get_session().lock();
                std::unique_ptr<oes::core::OtelSpan> span_ptr {nullptr};
                if (session_ptr) {
                    span_ptr.reset(new oes::core::OtelSpan(std::string("Session.State.Post"),
                                                           {{"thread_id", thisThreadId()}},
                                                           session_ptr->otel_parent_span_));
                } else {
                    span_ptr.reset(new oes::core::OtelSpan(std::string("Session.State.Post"),
                                                           {{"thread_id", thisThreadId()}}));
                }
                assert(same_owner(*state, rpc));

                MessageHeader& msg_hdr = rpc->req_hdr_;
                bool release_chunk_msg_rpc = false;
                if (state->is_chunk_msg(msg_hdr) && !has_FIN_bit(msg_hdr)){
                    release_chunk_msg_rpc = true;
                }

                state->handle_req(rpc, std::move(proto));

                if (release_chunk_msg_rpc && rpc) {
                    state->release_rpc(rpc);
                }
            }
        }
    );

    { // TODO: Clocks happy-case only for now. Use RAII to record in all cases?
        const auto recvd_at = now();
        const duration_milli_t millis = recvd_at - recv_at;

        std::unique_lock lock(recv_mt_);
        recv_.recv_time_ms_.add(millis.count());

        try {
            oes::core::Metrics::instance().getSummary("ws_message_latencies").Observe(millis.count());
        } catch (const oes::core::MetricNotFound& e) {
            OES_WARNING("Not registered metric. Reason: {}", e.what());
        }
    }
}

void Session::State::handle_res(
    const PendingRpc_ptr& p_rpc, const EtpResponse_ptr& p_res
) {
    assert(proc_strand_->running_in_this_thread());

    PendingRpc& rpc = *p_rpc;
    auto& [rc, wres] = *p_res;

    // FIXME: Should be done earlier
    rpc.mark_fully_decoded();

    MessageHeader& msg_hdr = rpc.req_hdr_;
    const hires_time_point& tstamp = rpc.sock_at_;

    Impl_ptr impl = wimpl_.lock();
    OES_ASSERT_MSG(impl, "Server alive", return;);

    const int resMsgTypeId = rpc.expected_response_type_;
    const bool no_res_msg = (resMsgTypeId == 0);
    if (rc.ok && no_res_msg) {
        assert(!rc.has_error());

        // No response to send (Acknowledge is always *before* processing starts).
        const auto done_at = now();

        const duration_milli_t millis = done_at - tstamp;
        OES_DEBUG(
            "SVR DONE Thread({}) Session({}) {} ({:.1f}ms later)",
            thisThreadId(), sid_, msgHeaderAsString(msg_hdr), millis.count()
        );

        // Empty empty trace event makes no sense,
        // there's no way to know what it is, or correlates to.
        // FIXME: Allow correlating traces events together, and have "point-events"
        //        to "terminate" an earlier event.
        if (EtpTracer12*const tracer = impl->tracer_.get()) {
            EtpTracerEventInfo evt;
            evt.set_point_event(done_at);
            evt.tid_ = thisThreadId();
            evt.sid_ = sid_;
            evt.set_phase(EtpTracerEventInfo::eMISC);
            evt.set_encoding(EtpTracerEventInfo::eJSON);
            evt.msg_ = fmt::format(
                "[{},\"DONE {}\"]",
                msg_hdr.m_correlationId, msgHeaderAsString(msg_hdr)
            );
            tracer->async_trace(std::move(evt));
        }

        return release_rpc(p_rpc);
    }

    if (status_ == Status::Closed){
        OES_INFO(
            "SVR >RES Thread({}) Session({}) is closed. {}",
            thisThreadId(), sid_, msgHeaderAsString(msg_hdr)
        );
        return release_rpc(p_rpc);
    }

    try {
        ws_conn_ptr_t conn = impl->get_conn(wconn_);
    } catch (const std::exception& e) {
        OES_ERROR("SVR >RES Thread({}) Session({}) failed. Reason: {}", 
            thisThreadId(), sid_, e.what()
        );
        return;
    }

    MessageHeader res_hdr;
    res_hdr.m_protocol = msg_hdr.m_protocol;
    res_hdr.m_messageType =
        !rc.ok?         ProtocolException::info.type_.type_:
        !no_res_msg?    resMsgTypeId:
        -1 // i.e. unknown message
    ;
    assert(res_hdr.m_messageType >= 0);
   
    if(is_put_obj_res_msg(res_hdr) && is_chunk_msg(msg_hdr)){
        // This is a PutDataObjectsResponse from Chunk message
        // Set the correlation ID the same as Chunk since it is
        // the same as the original PutDataObjects request message Id 
        res_hdr.m_correlationId = msg_hdr.m_correlationId;
        // Find the parent PutDataObjects msg
        PendingRpc_ptr found_rpc = rpcs_->find_parent_rpc(res_hdr.m_correlationId);
        if (found_rpc.get() != nullptr) {
            if(is_put_obj_msg(found_rpc->req_hdr_)){
                release_rpc(found_rpc, true /*temporary_rpc*/);
            }
        }
    } else {
        res_hdr.m_correlationId = msg_hdr.m_messageId;
    }

    if (rc.has_error() && rc.ok) {
        // i.e. partial success
        // We send both an error, and a response

        assert(res_hdr.m_messageType != ProtocolException::info.type_.type_);
        const int32_t msg_type = res_hdr.m_messageType; // remember response type
        res_hdr.m_messageType = ProtocolException::info.type_.type_;

        assert(!has_FIN_bit(res_hdr));
        send_error(p_rpc, res_hdr, rc.ex, /*partial_success*/true);

        // restore response's type
        res_hdr.m_messageType = msg_type;
    }

    if(rc.extra_messages.size() > 0) {
        // Send extra messages within the same protocol but different message types
        for(const auto& extra : rc.extra_messages) {
            MessageHeader extra_hdr;
            extra_hdr.m_protocol = extra->getInfo().type_.proto_;
            extra_hdr.m_messageType = extra->getInfo().type_.type_;
            if(extra_hdr.m_protocol!=msg_hdr.m_protocol) {
                OES_ERROR(
                    "[{}] SVR PROC Thread({}) Session({}): Session has been forcefully terminated. Extra message protocol mismatch for message protocol {} type {} associated with message protocol {} type {}.",
                    client_identification_, thisThreadId(), sid_,
                    extra_hdr.m_protocol, extra_hdr.m_messageType, msg_hdr.m_protocol, msg_hdr.m_messageType
                );
                continue;
            }
            extra_hdr.m_correlationId = msg_hdr.m_messageId;
            unset_FIN_bit(extra_hdr);
            auto enc = [&extra](::avro::Encoder& encoder) {
                extra->encode(encoder);
            };
            send(p_rpc, extra_hdr, enc);

            OES_INFO(
                "[{}] SVR >RES Thread({}) Session({}) {}",
                client_identification_, thisThreadId(), sid_, msgHeaderAsString(extra_hdr, nullptr, true)
            );
        }
    }

    if (rc.final_part) {
        set_FIN_bit(res_hdr);
    }

    if (!rc.ok) {
        send_error(p_rpc, res_hdr, rc.ex);
    } else if (wres) {
        // encode "type-erased" response
        try{
            if(rc.chunks.size()>0){
                // We have chunks to send, so the last message will be a chunk
                unset_FIN_bit(res_hdr);
            }
            send(p_rpc, res_hdr, wres);
            // Note: Must be AFTER send(), to have res_hdr.messageId correctly set
            OES_INFO(
                "[{}] SVR >RES Thread({}) Session({}) {}",
                client_identification_,
                thisThreadId(), sid_, msgHeaderAsString(res_hdr, nullptr, true)
            );
        }catch(const std::exception& ex){
            // convert ex.what() to string
            std::string err_msg = ex.what();
            if( err_msg.compare(0, 10, "Oversize #") == 0 ){
                rc.ok = false;
                size_t key_index = err_msg.find("<key>");
                if(key_index>0){
                    key_index += 5;
                    std::string key = err_msg.substr(key_index, err_msg.find("</key>",key_index)-key_index);
                    ErrorInfo info;
                    info.m_code = ErrorCodes::EMAXSIZE_EXCEEDED;
                    info.m_message = "Maximum size exceeded";
                    // Try to repackage request with chunks
                    rc.ex.m_errors.emplace(key,info);
                }
                res_hdr.m_messageType = ProtocolException::info.type_.type_;
                send_error(p_rpc, res_hdr, rc.ex);
            }
        }
       

        // Send chunks if any
        if(rc.chunks.size()>0){
            const size_t chunkTotalSize = rc.chunks.size();

            size_t nbChunks = std::lround(std::ceil(1.0 * chunkTotalSize / (max_msg_size_-100)));
            size_t chunkSize = std::lround(std::ceil(1.0 * chunkTotalSize / nbChunks));
            for(size_t offset=0; offset<chunkTotalSize; offset+=chunkSize){
                nbChunks--;
                size_t size = std::min<>(chunkSize, chunkTotalSize - offset);

                EtpResponseHandler<oes::common::etp12::Protocol::Store::Chunk> chunk_cb;
                oes::common::etp12::Protocol::Store::Chunk resChunk;
                resChunk.m_blobId = rc.chunk_uuid;
                resChunk.m_data = std::move(std::vector<uint8_t>(rc.chunks.begin()+offset, rc.chunks.begin()+offset+size));
                resChunk.m_final = nbChunks==0;

                MessageHeader chunk_hdr;
                chunk_hdr.m_protocol = resChunk.getInfo().type_.proto_;
                chunk_hdr.m_messageType = resChunk.getInfo().type_.type_;
                // Chunk correlationId must be the messageId of the 
                // message that resulted in the need to create chunks
                chunk_hdr.m_correlationId = msg_hdr.m_messageId;                
                if(resChunk.m_final && rc.final_part) {
                    set_FIN_bit(chunk_hdr);
                }
                auto chunkEnc = [&resChunk](::avro::Encoder& encoder) {
                    resChunk.encode(encoder);
                };
                send(p_rpc, chunk_hdr, chunkEnc);
                OES_INFO(
                    "[{}] SVR >RES Thread({}) Session({}) {} {}",
                    client_identification_, thisThreadId(), sid_,
                    msgHeaderAsString(chunk_hdr, nullptr, true), resChunk.m_final? "LAST_CHUNK": ""
                );
            }
        }

    } else {
        // no wres, means void-call / no-response
        assert(false);
    }
}

void Session::State::handle_req(
    const PendingRpc_ptr& p_rpc, proto12::IProtocol_ptr proto
) {
    assert(proc_strand_->running_in_this_thread());

    if( status_ == Status::AboutToClose || status_ == Status::Closed) {
        const auto& msg_hdr = p_rpc->req_hdr_;
        if (msg_hdr.m_protocol != static_cast<int>(ProtoEnum::eCore) && msg_hdr.m_messageType == CloseSession::info.type_.type_) {
            OES_ERROR(
                "SVR <REQ Thread({}) Session({}) is in terminated state {}",
                thisThreadId(), sid_, msgHeaderAsString(p_rpc->req_hdr_)
            );
            return;
        }
    }

    try {
        proto->dispatchRequest(
            *p_rpc,
            [state = shared_from_this(), p_rpc](
                EtpResponseStatus rc, EtpWrappedResponse wres
            ) -> void {
                // This lambda is called from the proc_strand_, and we want to
                // return processing right away, without having to wait for responses
                // to be sent (serialized, perhaps split, compressed, etc...)
                assert(same_owner(*state, p_rpc));

                // Because ASIO copies things around when queueing to strands,
                // and we want to avoid copies here (wres wraps by-value responses),
                // we explicitly move things to a shared_ptr, which is cheap to copy.
                auto p_res = std::make_shared<EtpResponse>(std::move(rc), std::move(wres));

                state->handle_res(p_rpc, p_res);
            }
        );
    } catch (const std::exception& ex) {
        post_send_error(
            p_rpc, Datatypes::EINVALID_STATE,
            fmt::format("Request processing failed: {}", ex.what())
        );
        // fall-thru
    } catch (...) {
        post_send_error(
            p_rpc, Datatypes::EINVALID_STATE,
            "Request processing failed: Unknown error"
        );
        // fall-thru
    }
}

//============================================================================

Todo::Todo(
) : std::runtime_error("TODO")
{
}

EtpResponseStatus errorStatus(ErrorCodes errc, std::string msg) {
    ErrorInfo erri;
    erri.m_code = errc;
    erri.m_message = std::move(msg);

    EtpResponseStatus rc;
    rc.ok = false;
    rc.ex.m_error = std::move(erri);
    return rc; // move above is no-op, since const& arg for now
}

bool addErrorStatus(
    EtpResponseStatus& rc, const std::string& key, ErrorCodes errc, std::string msg
) {
    OES_DEBUG_ASSERT(!rc.ex.m_error); // don't mix scalar and map errors

    auto rc_it = rc.ex.m_errors.emplace(key, ErrorInfo{});
    if (rc_it.second) {
        rc_it.first->second.m_code = errc;
        rc_it.first->second.m_message = std::move(msg);
        rc.ok = false;
        return true;
    }
    // We already have an error for this key... Normal???
    OES_DEBUG_ASSERT(false);
    return false;
}

bool addErrorStatus(EtpResponseStatus& rc, const std::string& key, ErrorInfo err) {
    OES_DEBUG_ASSERT(!rc.ex.m_error); // don't mix scalar and map errors

    if (rc.ex.m_errors.emplace(key, std::move(err)).second) {
        rc.ok = false;
        return true;
    }
    // We already have an error for this key... Normal???
    OES_DEBUG_ASSERT(false);
    return false;
}

void handleException(const EtpCompleteHandler& res_handler) {
    std::string msg;
    ErrorCodes errc = data12::EINVALID_STATE;

    try {
        std::rethrow_exception(std::current_exception());
    } catch (const Todo&) {
        msg = "Server Error: TODO";
        errc = data12::ENOTSUPPORTED;
    } catch (const std::exception& ex) {
        msg = "Server Error: Unexpected Exception: ";
        msg += ex.what();

        // trim trailing newlines
        while (msg.back() == '\n') {
            msg.resize(msg.size() - 1);
        }
    } catch (...) {
        msg = "Server Error: Unknown Exception.";
    }

    res_handler(errorStatus(errc, std::move(msg)));
}

//============================================================================

} // namespace oes::eml::etp12

namespace {

void CoreOSDUProtoImpl::ResumeSession(
    IRequest&, ResumeSession_req_t req,
    EtpResponseHandler<ResumeSession_res_t> process_cb
) {
    Session* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    auto state_impl = session->getState()->wimpl_.lock();
    if (!state_impl) {
        OES_ERROR("SRV state corrupted");
        return;
    }

    EtpResponseStatus rc;
    ResumeSession_res_t res;
    try {
        session->onResume(std::move(req), res);
        rc.ok = true;
    } catch (proto12::ProtocolException& ex) {
        rc.ex = std::move(ex);
    } catch (const std::exception& ex) {
        Datatypes::ErrorInfo info;
        info.m_code = data12::EINVALID_STATE;
        info.m_message = "Generic Exception: ";
        if (const char* msg = ex.what()) {
            info.m_message += msg;
        } else {
            info.m_message += "no message";
        }
        rc.ex.m_error = std::move(info);
    } catch (...) {
        Datatypes::ErrorInfo info;
        info.m_code = data12::EINVALID_STATE;
        info.m_message = "Unknown Exception!";
        rc.ex.m_error = std::move(info);
    }

    if (rc.ok) {
        pingPong(req.m_currentDateTime, res.m_currentDateTime);
    }

    process_cb(rc, res);
}


}
