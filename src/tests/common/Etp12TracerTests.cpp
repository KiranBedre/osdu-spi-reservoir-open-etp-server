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

#include <oes/common/etp12/Tracer.h>
#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/Guid.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <sstream>
#include <fstream>
#include <limits>
#include <set>
#include <map>

using namespace oes::common::etp12;

namespace {

BOOST_AUTO_TEST_SUITE(EtpTracer12Tests)

BOOST_AUTO_TEST_CASE(test_event_flags) {
    const EtpTracerEventInfo::Phase phases[] = {
        EtpTracerEventInfo::eRECV,
        EtpTracerEventInfo::eSEND,
        EtpTracerEventInfo::ePROC,
        EtpTracerEventInfo::eMISC
    };
    const EtpTracerEventInfo::Encoding encodings[] = {
        EtpTracerEventInfo::eAVRO_BINARY,
        EtpTracerEventInfo::eAVRO_JSON,
        EtpTracerEventInfo::eJSON,
        EtpTracerEventInfo::eBINARY
    };
    const EtpTracerEventInfo::Compression compressions[] = {
        EtpTracerEventInfo::eUNCOMPRESSED,
        EtpTracerEventInfo::eZLIB_DEFLATE,
        EtpTracerEventInfo::eZUNUSED1,
        EtpTracerEventInfo::eZUNUSED2
    };
    const EtpTracerEventInfo::FormatVersion versions[] = {
        EtpTracerEventInfo::eVERSION0,
        EtpTracerEventInfo::eVUNUSED1,
        EtpTracerEventInfo::eVUNUSED2,
        EtpTracerEventInfo::eVUNUSED3
    };
    const EtpTracerEventInfo::ProxyRole roles[] = {
        EtpTracerEventInfo::ePROXY_NO_ROLE,
        EtpTracerEventInfo::ePROXY_AS_CLIENT,
        EtpTracerEventInfo::ePROXY_AS_SERVER,
        EtpTracerEventInfo::ePUNUSED1
    };

    std::set<unsigned> all_flags;
    for (auto p : phases)
    for (auto e : encodings)
    for (auto c : compressions)
    for (auto v : versions)
    for (auto r : roles) {
        EtpTracerEventInfo evt;
        evt.set_phase(p);
        evt.set_encoding(e);
        evt.set_compression(c);
        evt.set_format_version(v);
        evt.set_proxy_role(r);

        all_flags.emplace(evt.flags_);

        BOOST_CHECK_EQUAL(p, evt.phase());
        BOOST_CHECK_EQUAL(e, evt.encoding());
        BOOST_CHECK_EQUAL(c, evt.compression());
        BOOST_CHECK_EQUAL(v, evt.format_version());
        BOOST_CHECK_EQUAL(r, evt.proxy_role());
    }
    BOOST_CHECK_EQUAL(all_flags.size(), size_t(4*4*4*4*4));
}

BOOST_AUTO_TEST_CASE(test_event_encoding_empty) {
    EtpTracerEventInfo evt;

    const size_t header = evt.encoded_size(true);
    const size_t whole = evt.encoded_size(false);
    BOOST_CHECK_EQUAL(header, size_t(6)); // MIN header size overhead
    BOOST_CHECK_EQUAL(whole, size_t(6));

    const size_t min_header = EtpTracerEventInfo::min_header_size();
    BOOST_CHECK_EQUAL(header, min_header);

    const size_t max_header = EtpTracerEventInfo::max_header_size();
    BOOST_CHECK(header <= max_header);

    std::vector<unsigned char> buf(whole);
    const size_t len = evt.encode(&buf[0]);
    BOOST_CHECK_EQUAL(len, buf.size());

    EtpTracerEventInfo evt2;
    const size_t len2 = evt2.decode(&buf[0]);
    BOOST_CHECK_EQUAL(len2, buf.size());

    BOOST_CHECK(evt.beg_ == evt2.beg_);
    BOOST_CHECK(evt.end_ == evt2.end_);
    BOOST_CHECK_EQUAL(evt.tid_, evt2.tid_);
    BOOST_CHECK_EQUAL(evt.sid_, evt2.sid_);
    BOOST_CHECK_EQUAL(evt.flags_, evt2.flags_);
    BOOST_CHECK_EQUAL(evt.msg_.size(), evt2.msg_.size());
}

BOOST_AUTO_TEST_CASE(test_event_encoding_small) {
    EtpTracerEventInfo evt;
    /*
    ** To avoid clock differences, possibly leading to different
    ** number of bytes, use fixed date: 2019-30-07 11:50'ish
    ** Actually happened to me, Windows desktop vs Arachne...
    evt.beg_ = std::chrono::high_resolution_clock::now();
    */
    evt.beg_ += std::chrono::nanoseconds(UINT64_C(10893307660225968));
    evt.end_ = evt.beg_ + std::chrono::microseconds(507);
    evt.tid_ = 2;
    evt.sid_ = 3;
    evt.set_phase(EtpTracerEventInfo::eSEND);
    evt.msg_ = "foo bar baz";

    const size_t header = evt.encoded_size(true);
    const size_t whole = evt.encoded_size(false);
    BOOST_CHECK_EQUAL(header, size_t(15));
    BOOST_CHECK_EQUAL(whole, size_t(26));

    const size_t max_header = EtpTracerEventInfo::max_header_size();
    BOOST_CHECK(header <= max_header);

    std::vector<unsigned char> buf(whole);
    const size_t len = evt.encode(&buf[0]);
    BOOST_CHECK_EQUAL(len, buf.size());

    EtpTracerEventInfo evt2;
    const size_t len2 = evt2.decode(&buf[0]);
    BOOST_CHECK_EQUAL(len2, buf.size());

    BOOST_CHECK(evt.beg_ == evt2.beg_);
    BOOST_CHECK(evt.end_ == evt2.end_);
    BOOST_CHECK_EQUAL(evt.tid_, evt2.tid_);
    BOOST_CHECK_EQUAL(evt.sid_, evt2.sid_);
    BOOST_CHECK_EQUAL(evt.flags_, evt2.flags_);
    BOOST_CHECK_EQUAL(evt.msg_.size(), evt2.msg_.size());
    BOOST_CHECK_EQUAL(evt.msg_, evt2.msg_);
}

BOOST_AUTO_TEST_CASE(test_event_encoding_big) {
    EtpTracerEventInfo evt;
    evt.beg_ = decltype(evt.beg_)::max();
    evt.end_ = decltype(evt.end_)::max();
    evt.tid_ = std::numeric_limits<decltype(evt.tid_)>::max();
    evt.sid_ = std::numeric_limits<decltype(evt.sid_)>::max();

    evt.set_phase(EtpTracerEventInfo::eMISC);
    evt.set_encoding(EtpTracerEventInfo::eBINARY);
    evt.set_compression(EtpTracerEventInfo::eZUNUSED2);

    // should be 4 bytes, since > 67823
    // see https://sqlite.org/src4/doc/trunk/www/varint.wiki
    evt.msg_.assign(70*1000, 'X');

    const size_t header = evt.encoded_size(true);
    const size_t whole = evt.encoded_size(false);
    BOOST_CHECK_EQUAL(header, size_t(24)); // MAX header size overhead
    BOOST_CHECK_EQUAL(whole, size_t(70024));

    const size_t max_header = EtpTracerEventInfo::max_header_size();
    BOOST_CHECK(header <= max_header);

    std::vector<unsigned char> buf(whole);
    const size_t len = evt.encode(&buf[0]);
    BOOST_CHECK_EQUAL(len, buf.size());

    EtpTracerEventInfo evt2;
    const size_t len2 = evt2.decode(&buf[0]);
    BOOST_CHECK_EQUAL(len2, buf.size());

    BOOST_CHECK(evt.beg_ == evt2.beg_);
    BOOST_CHECK(evt.end_ == evt2.end_);
    BOOST_CHECK_EQUAL(evt.tid_, evt2.tid_);
    BOOST_CHECK_EQUAL(evt.sid_, evt2.sid_);
    BOOST_CHECK_EQUAL(evt.flags_, evt2.flags_);
    BOOST_CHECK_EQUAL(evt.msg_.size(), evt2.msg_.size());
    BOOST_CHECK_EQUAL(evt.msg_, evt2.msg_);
}

BOOST_AUTO_TEST_CASE(test_trace_file_ctor) {
    EtpTraceHeader hdr1;
    EtpTraceHeader hdr2;
    EtpTraceHeader hdr3 = EtpTraceHeader::make();

    // check pid
    BOOST_CHECK_EQUAL(hdr1.pid_, 0u);
    BOOST_CHECK_EQUAL(hdr2.pid_, 0u);
    BOOST_CHECK_NE(hdr3.pid_, 0u); // make() inits the PID

    // check timestamps
    const std::chrono::system_clock::time_point zero; // i.e. EPOC
    const std::chrono::high_resolution_clock::time_point zero_hires;

    const auto now = std::chrono::system_clock::now();
    const auto now_hires = std::chrono::high_resolution_clock::now();

    BOOST_CHECK(hdr1.started_at_ == zero);
    BOOST_CHECK(hdr1.started_at_hrc_ == zero_hires);

    BOOST_CHECK(hdr2.started_at_ == zero);
    BOOST_CHECK(hdr2.started_at_hrc_ == zero_hires);

    BOOST_CHECK(hdr3.started_at_ != zero);
    BOOST_CHECK(hdr3.started_at_hrc_ != zero_hires);

    BOOST_CHECK(hdr3.started_at_ <= now);
    BOOST_CHECK(hdr3.started_at_hrc_ < now_hires);

    std::string hdr1_str = hdr1.as_file_name();
    std::string hdr2_str = hdr2.as_file_name();
    std::string hdr3_str = hdr3.as_file_name();
    BOOST_CHECK_EQUAL(hdr1_str, hdr2_str);
    BOOST_CHECK_NE(hdr1_str, hdr3_str);
}

BOOST_AUTO_TEST_CASE(test_simple_tracing) {
    const size_t event_count = 100;

    std::string filename = oes::testing::resolveTestDataFile(
        "data/eml", "Etp12Tracer.trace", false
    );
    EtpTraceHeader file_header;
    const std::chrono::nanoseconds deterministic_now(UINT64_C(10893307660225968));
    auto kmillis(std::chrono::duration_cast<std::chrono::milliseconds>(deterministic_now));
    std::chrono::system_clock::time_point kstarted_at(kmillis); // Warning: senseless system-time.
    auto knanos(std::chrono::duration_cast<std::chrono::nanoseconds>(deterministic_now));
    std::chrono::high_resolution_clock::time_point kstarted_at_hrc(knanos);
    file_header.started_at_ = kstarted_at;
    file_header.started_at_hrc_ = kstarted_at_hrc;
    file_header.pid_ = 20908;

    BOOST_CHECK(file_header.from_server_side()); // by default
    BOOST_CHECK(!file_header.from_client_side());
    file_header.set_tier(EtpTraceHeader::eCLIENT);
    BOOST_CHECK(!file_header.from_server_side());
    BOOST_CHECK(file_header.from_client_side());
    file_header.set_tier(EtpTraceHeader::ePROXY);
    BOOST_CHECK(!file_header.from_server_side());
    BOOST_CHECK(!file_header.from_client_side());
    file_header.set_tier(EtpTraceHeader::eSERVER); // back to default
    BOOST_CHECK(file_header.from_server_side());
    BOOST_CHECK(!file_header.from_client_side());

    EtpTracerEventVec events(event_count);
    for (auto& evt : events) {
        ptrdiff_t i = std::distance(&events[0], &evt);
        evt.beg_ += deterministic_now;
        evt.end_ = evt.beg_ + std::chrono::nanoseconds(i * 57);
        evt.set_phase(EtpTracerEventInfo::Phase(i & 0x3));
        evt.msg_.assign(i, 'A' + (i % 26));
        if (evt.msg_.empty()) {
            // Because tracer asserts msg_ must be non-empty.
            // Could have been .assign(i+1, ...) above too, it's just more bytes.
            evt.msg_ = "1st";
        }
    }

    {
        EtpTracer12 tracer(filename, file_header);
        tracer.start();

        for (auto& evt : events) {
            tracer.async_trace(evt); // used later, so don't move
        }

        tracer.async_flush();

        for (auto& evt : events) {
            ++evt.sid_; // different "concurrent" session for example
            tracer.async_trace(std::move(evt));
        }

        tracer.async_flush();

        tracer.stop();

        BOOST_CHECK_EQUAL(size_t(0), tracer.io_error_count());
    }

    // the trace file size should be deterministic (across platforms!)
    std::ifstream ifs(filename, std::ios::ate | std::ios::binary);
    const std::ifstream::pos_type file_size = ifs.tellg();
    BOOST_CHECK_EQUAL(std::ifstream::pos_type(13735), file_size);

    std::vector<char> file_content(file_size);
    ifs.seekg(0, std::ios::beg);
    ifs.read(&file_content[0], file_size);
    BOOST_CHECK(ifs.good() && (ifs.tellg() == file_size));

    std::string hash_str = oes::core::Guid::make_deterministic(
        std::string((char*)file_content.data(), size_t(file_content.size()))
        ).toStringNoDash();
    BOOST_CHECK_EQUAL(hash_str, "88a4de61935d55d780cbacc8d6fc1a98");

    const size_t fsize = (file_size);
    const std::vector<size_t> io_buffer_sizes{
        fsize / 2, fsize - 1, fsize, fsize + 1,
        2*fsize - 1, 2*fsize, 2*fsize + 1,
        4 * 1024 * 1024 // the default
    };
    const std::vector<size_t> chunk_sizes{
        size_t(1), size_t(2), size_t(7),
        event_count / 10, event_count - 1, event_count, event_count + 1,
        2 * event_count + 1, 2 * event_count, 3 * event_count
    };

    for (const size_t io_buffer_size : io_buffer_sizes) {
        for (const size_t chunk_size : chunk_sizes) {
            const size_t expected_calls = (2 * event_count) / chunk_size;
            const bool partial_last_chunk = (chunk_size * expected_calls) != (2 * event_count);

            EtpTraceHeader read_file_header;
            EtpTraceReader12 reader = makeEtpTraceFileReader12(
                filename, read_file_header, chunk_size, io_buffer_size
            );
            BOOST_CHECK(file_header.started_at_ == read_file_header.started_at_);
            BOOST_CHECK_EQUAL(file_header.pid_, read_file_header.pid_);
            BOOST_CHECK_EQUAL(file_header.etp_major_, read_file_header.etp_major_);
            BOOST_CHECK_EQUAL(file_header.etp_minor_, read_file_header.etp_minor_);
            BOOST_CHECK_EQUAL(file_header.etp_patch_, read_file_header.etp_patch_);

            EtpTraceReaderCtx ctx;
            EtpTracerEventVec read_events;
            for (size_t i = 0; i < expected_calls; ++i) {
                const bool last_full_chunk = ((i + 1) == expected_calls);
                EtpTracerEventVec chunk;
                if (last_full_chunk) {
                    chunk = reader(ctx);
                } else {
                    chunk = reader(ctx);
                }
                BOOST_CHECK_EQUAL(chunk_size, chunk.size());

                if (last_full_chunk) {
                    BOOST_CHECK_EQUAL(ctx.eof_, !partial_last_chunk);
                } else {
                    BOOST_CHECK_EQUAL(ctx.eof_, false);
                }

                BOOST_CHECK_EQUAL(ctx.io_errors_, size_t(0));

                std::move(chunk.begin(), chunk.end(), std::back_inserter(read_events));
            }

            if (partial_last_chunk) {
                EtpTracerEventVec chunk = reader(ctx);
                BOOST_CHECK_EQUAL(chunk.empty(), false);
                BOOST_CHECK(chunk.size() < chunk_size);
                BOOST_CHECK_EQUAL(chunk.size() + (chunk_size * expected_calls), 2 * event_count);
                BOOST_CHECK_EQUAL(ctx.eof_, true);
                BOOST_CHECK_EQUAL(ctx.io_errors_, size_t(0));

                std::move(chunk.begin(), chunk.end(), std::back_inserter(read_events));
            } else {
                EtpTracerEventVec chunk = reader(ctx);
                BOOST_CHECK_EQUAL(chunk.empty(), true);
                BOOST_CHECK_EQUAL(ctx.eof_, true);
                BOOST_CHECK_EQUAL(ctx.io_errors_, size_t(0));
            }

            BOOST_CHECK_EQUAL(2 * event_count, read_events.size());
        }

        // Not chunk_size-dependent tests below

        std::ostringstream out;
        std::ostringstream err;
        const bool file_is_valid = validateEtpTraceFile(
            filename, file_header, out, err, io_buffer_size
        );
        BOOST_CHECK_EQUAL(file_is_valid, true);
        BOOST_CHECK_EQUAL(err.str(), std::string());
        /*
        ** Example output:
        CLIENT ETP 1.2.0 (PID 60828) 2021-03-25T14:56:20.275 UTC
        8 trace events; 659 message bytes, 128 header bytes.
        */
        BOOST_CHECK(0 == out.str().find("SERVER ETP 1.2.0"));
        BOOST_CHECK(std::string::npos != out.str().find(
            "\n200 trace events; 9906 message bytes, 2794 header bytes."
        ));
    }

    // Trace again, to the same file.
    {
        file_header.set_tier(EtpTraceHeader::Tier::eCLIENT);
        EtpTracer12 tracer(filename, file_header);
        tracer.start();

        EtpTracerEventInfo evt;
        evt.beg_ += deterministic_now;
        evt.end_ = evt.beg_ + std::chrono::nanoseconds(507);
        evt.set_encoding(EtpTracerEventInfo::eJSON);
        evt.set_phase(EtpTracerEventInfo::eMISC);
        evt.msg_ = R"json({"name"="foo", "instant":true, "value": 1})json";
        evt.sid_ = 7;

        tracer.async_trace(evt);
        tracer.async_flush();
        tracer.stop();
    }

    // Test "concatenated" traces can be parsed OK too
    std::ostringstream out;
    std::ostringstream err;
    const bool file_is_valid = validateEtpTraceFile(
        filename, file_header, out, err
    );
    BOOST_CHECK_EQUAL(file_is_valid, true);
    BOOST_CHECK_EQUAL(err.str(), std::string());
    /*
    ** Example output:
    SERVER ETP 1.2.0 (PID ...) ...
    200 trace events; 9900 message bytes, 2794 header bytes.
    CLIENT ETP 1.2.0 (PID ...) ...
    1 trace events; 42 message bytes, 14 header bytes.

    TOTAL: 2 traces, 201 events; 9942 message bytes, 2808 header bytes.
    */
    std::string out_str = out.str();
    BOOST_CHECK(0 == out_str.find("SERVER ETP 1.2.0"));
    BOOST_CHECK(0 != out_str.rfind("CLIENT ETP 1.2.0"));
    BOOST_CHECK(std::string::npos != out_str.rfind("CLIENT ETP 1.2.0"));
    BOOST_CHECK(out_str.find("ETP 1.2.0") != out_str.rfind("ETP 1.2.0"));

    BOOST_CHECK(std::string::npos != out.str().find(
        "\n200 trace events; 9906 message bytes, 2794 header bytes."
    ));
    BOOST_CHECK(std::string::npos != out.str().find(
        "\n1 trace events; 42 message bytes, 14 header bytes."
    ));
    BOOST_CHECK(std::string::npos != out.str().find(
        "\nTOTAL: 2 traces, 201 events; 9948 message bytes, 2808 header bytes."
    ));

    EtpTraceHeader file_header2;
    EtpTraceReader12 reader = makeEtpTraceFileReader12(filename, file_header2);

    EtpTraceReaderCtx reader_ctx;
    std::map<unsigned, std::pair<size_t, size_t>> evt_count_per_trace;
    while (!reader_ctx.eof_ && !reader_ctx.io_errors_) {
        EtpTracerEventVec read_events = reader(reader_ctx);
        auto& per_trace = evt_count_per_trace[reader_ctx.trace_id_];
        per_trace.first += read_events.size();

        for (const auto& evt : read_events) {
            per_trace.second += evt.msg_.size();
        }
    }
    BOOST_REQUIRE_EQUAL(evt_count_per_trace.size(), size_t(2));

    BOOST_CHECK_EQUAL(evt_count_per_trace[1].first, size_t(200));
    BOOST_CHECK_EQUAL(evt_count_per_trace[1].second, size_t(9906));

    BOOST_CHECK_EQUAL(evt_count_per_trace[2].first, size_t(1));
    BOOST_CHECK_EQUAL(evt_count_per_trace[2].second, size_t(42));
}

// TODO: Test with a large message that exceeds the remaining buffer size,
//       and also that's larger to even a full buffer (of 4MB by default).
//       Bugs fixed in both parsers on 9/4/19, but needs a test...

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
