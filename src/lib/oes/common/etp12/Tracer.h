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

#ifndef OES_COMMON_ETP12_TRACER_H
#define OES_COMMON_ETP12_TRACER_H

#include "../export.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

namespace oes::common::etp12 {

//============================================================================

struct OES_COMMON_EXPORT EtpTracerEventInfo {
    // This event's begin and end (relative) timestamps.
    // (needs info from EtpTraceHeader for absolute/system_clock timestamps)
    // beg_ == end_ means point-in-time event (i.e. has no duration).
    // \sa is_point_event() and set_point_event()
    std::chrono::high_resolution_clock::time_point beg_;
    std::chrono::high_resolution_clock::time_point end_;

    // Thread and session IDs
    unsigned tid_ = 0;
    unsigned sid_ = 0;

    // Phase when this event was recorded (bits 0-1).
    enum Phase : unsigned {
        eRECV, eSEND, ePROC, eMISC
    };

    // This event's payload (msg_) Encoding (bits 2-3).
    // For AVRO-based encodings, we assume ETP messages, using a specific ETP
    // version specified by the context, via the header (EtpTraceHeader)
    // of the file this event is part of.
    enum Encoding : unsigned {
        eAVRO_BINARY, eAVRO_JSON, eJSON, eBINARY
    };

    // This event's payload (msg_) Compression (bits 4-5).
    // Note that this is different from the compression of Avro messages,
    // where the ETP MessageHeader is always uncompressed, and the rest of
    // the message *may* be compressed, using a session-negotiated compressor.
    enum Compression : unsigned {
        eUNCOMPRESSED, eZLIB_DEFLATE, eZUNUSED1, eZUNUSED2
    };

    // This event's binary encoding version (varint-based, bits 6-7).
    // i.e. the undocumented format used by encode()/decode() below.
    // Not to be confused with this event's msg_ format (see Encoding).
    enum FormatVersion : unsigned {
        eVERSION0, eVUNUSED1, eVUNUSED2, eVUNUSED3
    };

    // When this trace is recorded on a PROXY ETP server,
    // it acts as both a client and a server. Thus we need
    // to know which role this event is from.
    enum ProxyRole : unsigned {
        ePROXY_NO_ROLE, ePROXY_AS_SERVER, ePROXY_AS_CLIENT, ePUNUSED1
    };

    // Flag encoding all 4 2-bit enums above.
    // Has room for future flags, provided FormatVersion is bumped.
    // Default 0-value implies eRECV, eAVRO_BINARY, eUNCOMPRESSED,
    // eVERSION0, and ePROXY_NO_ROLE.
    unsigned flags_ = 0;

    // message itself; MUST be non-empty.
    // (by value since processed async; moved to queue)
    std::string msg_;

public:
    size_t encoded_size(bool header_only = false) const;
    size_t encode(unsigned char*, bool header_only = false) const;
    size_t decode(const unsigned char*, bool header_only = false);

    static constexpr size_t min_header_size() { return size_t(6); }
    static constexpr size_t max_header_size() { return size_t(32); }

    bool is_point_event() const {
        return beg_ == end_;
    }
    void set_point_event(std::chrono::high_resolution_clock::time_point t) {
        beg_ = end_ = t;
    }

    Phase phase() const {
        return Phase((flags_ & 0x03u) >> 0);
    }
    Encoding encoding() const {
        return Encoding((flags_ & 0x0Cu) >> 2);
    }
    Compression compression() const {
        return Compression((flags_ & 0x30u) >> 4);
    }
    unsigned format_version() const {
        return FormatVersion((flags_ & 0xC0u) >> 6);
    }
    ProxyRole proxy_role() const {
        return ProxyRole((flags_ & 0x300u) >> 8);
    }

    void set_phase(Phase phase) {
        flags_ &= ~0x03u;
        flags_ |= (phase << 0);
    }
    void set_encoding(Encoding encoding) {
        flags_ &= ~0x0Cu;
        flags_ |= (encoding << 2);
    }
    void set_compression(Compression compression) {
        flags_ &= ~0x30u;
        flags_ |= (compression << 4);
    }
    void set_format_version(FormatVersion version) {
        flags_ &= ~0xC0u;
        flags_ |= (version << 6);
    }
    void set_proxy_role(ProxyRole role) {
        flags_ &= ~0x300u;
        flags_ |= (role << 8);
    }
};

using EtpTracerEventVec = std::vector<EtpTracerEventInfo>;

//============================================================================

struct OES_COMMON_EXPORT EtpTraceHeader {
    int format_version_ = 1;

    // matters for Avro decoding of ETP messages
    int etp_major_ = 1;
    int etp_minor_ = 2;
    int etp_patch_ = 0;

    // Process ID of client or server
    int pid_ = 0;

    // Officially UTC-since-Unix-EPOC since C++20
    std::chrono::system_clock::time_point started_at_;

    // Map UTC EPOC-time above to hi-res clock used to stamp events
    std::chrono::high_resolution_clock::time_point started_at_hrc_;

    // Who recorded the trace
    enum Tier : unsigned {
        eCLIENT = 1, eSERVER = 2, ePROXY = 3
    };

    unsigned flags_ = eSERVER;

    static EtpTraceHeader make(Tier tier = eSERVER);

public:
    size_t encoded_size() const;
    size_t encode(unsigned char*) const;
    size_t decode(const unsigned char*);

    Tier tier() const {
        return Tier((flags_ & 0x03u) >> 0);
    }

    void set_tier(Tier tier) {
        flags_ &= ~0x03u;
        flags_ |= (tier << 0);
    }

    std::string as_file_name(const std::string* p_host = nullptr) const;
    static std::string as_file_name(
        const std::string& prefix, int pid,
        std::chrono::system_clock::time_point utc_tp
    );

    bool from_client_side() const {
        return tier() == eCLIENT;
    }
    bool from_server_side() const {
        return tier() == eSERVER;
    }
};

//============================================================================

class OES_COMMON_EXPORT EtpTracer12 {
public:
    EtpTracer12(std::string filename, const EtpTraceHeader& file_header);
    ~EtpTracer12();

    void start();
    void async_trace(EtpTracerEventInfo info);
    void async_flush();
    void stop();

    size_t io_error_count();

    std::string filename() const {
        return filename_;
    }

    static std::string trace_filename(
        std::string trace_file_or_folder,
        const EtpTraceHeader* p_header = nullptr,
        const std::string* p_host = nullptr
    );

private:
    void wait_until(int);
    void process_events();

    static bool write_events(FILE*, EtpTracerEventVec&, size_t& written);

private:
    std::mutex mt_;
    std::condition_variable cv_;

    // shared-state, protected by mt_ above
    EtpTracerEventVec event_queue_;
    enum {
        eSTOPPED, eRUNNING, eSHOULD_STOP, eSHOULD_FLUSH
    } backend_status_ = eSTOPPED;

    // backend-thread-specific state
    const std::string filename_;
    void* ofs_ = nullptr; // FILE*
    std::thread backend_;
    size_t io_errors_ = 0;
    std::chrono::milliseconds timeout_;
    size_t written_since_last_flush_ = 0;
};

//============================================================================

struct OES_COMMON_EXPORT EtpTraceReaderCtx {
    bool eof_ = false;
    size_t io_errors_ = 0;

    unsigned trace_id_ = 0; // 1-based for valid traces
    const EtpTraceHeader* trace_header_ = nullptr;
};

using EtpTraceReader12 = std::function<
    EtpTracerEventVec(EtpTraceReaderCtx& ctx)
>;

OES_COMMON_EXPORT
EtpTraceReader12 makeEtpTraceFileReader12(
    const std::string& filename,
    EtpTraceHeader& first_trace_header,
    size_t max_events_per_call = 100,
    size_t io_buffer_size = 4*1024*1204 // 4MB
);

OES_COMMON_EXPORT
bool validateEtpTraceFile(
    const std::string& filename,
    EtpTraceHeader& file_header,
    std::ostream& out, std::ostream& err,
    size_t io_buffer_size = 4 * 1024 * 1204 // 4MB
);

//============================================================================

} // namespace oes::common::etp12

#endif
