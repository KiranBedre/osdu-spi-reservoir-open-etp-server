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

#include <oes/common/etp12/EmlUri.h>
#include <oes/common/etp12/Tracer.h>
#include <oes/common/AvroUtils.hpp>
#include <oes/client/Client.hpp>
#include <oes/eml/etp12/Extensions.h>

#include <oes/core/utils/Guid.h>

#include <oes/testing/eml/ConsoleUtils.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Stream.hh>
#include <avro/Decoder.hh>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <filesystem>

using namespace oes::testing;
using namespace oes::core;

using namespace oes::eml::etp12;
using namespace oes::testing::eml;

using namespace oes::common::etp12;

namespace {

//*****************************************************************************

BOOST_FIXTURE_TEST_SUITE(ClientServerTracingTests, OpenKVClientServerFixture)

BOOST_AUTO_TEST_CASE(test_tracing) {
    OES_TEST_CHECK_MANUAL_ENABLE();

    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );
    const std::string& traces_folder = oes::testing::resolveTestDataFile(
        "data/eml", "traces/", false // removes folder too
    );

    svr_conf_.enable_tracing_ = true;
    svr_conf_.trace_file_ = traces_folder;
    namespace fs = std::filesystem;
    fs::create_directory(traces_folder);

    startServer(); // with tracing, see above

    auto get_traces = [traces_folder]() {
        std::set<std::string> traces;
        for (const auto& p : fs::directory_iterator(traces_folder)) {
            traces.insert(p.path().string());
        }
        return traces;
    };
    std::set<std::string> traces = get_traces();
    BOOST_REQUIRE_EQUAL(traces.size(), 1);
    const std::string server_trace = *traces.begin();

    const auto& clt_conf = initClientConfig();

    const std::string space_path = "EPCs/OneWell";

    OESEmlRunner runner;
    runner.startAndWait({
        "space", "--new", "-s", space_path,
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf.server_url_,
        "--etp-trace", traces_folder + "/new-space.trace" // with explicit filename
    });
    traces = get_traces();
    BOOST_REQUIRE_EQUAL(traces.size(), 2);
    traces.erase(server_trace);
    BOOST_REQUIRE_EQUAL(traces.size(), 1);
    const std::string client_trace1 = *traces.begin();
    BOOST_CHECK(boost::ends_with(client_trace1, "new-space.trace"));

    runner.startAndWait({
        "space", "--import-epc", epc_input, "-s", space_path,
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf.server_url_,
        "--etp-trace", traces_folder // just a folder, filename inferred
    });
    traces = get_traces();
    BOOST_REQUIRE_EQUAL(traces.size(), 3);
    traces.erase(server_trace);
    traces.erase(client_trace1);
    BOOST_REQUIRE_EQUAL(traces.size(), 1);
    const std::string client_trace2 = *traces.begin();
    BOOST_CHECK(client_trace2.find("EmrEtp12-localhost-") != std::string::npos);
    BOOST_CHECK(boost::ends_with(client_trace2, ".trace"));

    runner.startAndWait({
        "space", "--info", "-s", space_path,
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf.server_url_,
        "--etp-trace", client_trace1 // with explicit *AND* existing file
    });
    traces = get_traces();
    BOOST_REQUIRE_EQUAL(traces.size(), 3);
    traces.erase(server_trace);
    traces.erase(client_trace1);
    traces.erase(client_trace2);
    BOOST_REQUIRE_EQUAL(traces.size(), 0);

    runner.startAndWait({
        "space", "--stats", "-s", space_path,
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf.server_url_,
        "--etp-trace", traces_folder // just a folder, filename inferred
    });
    traces = get_traces();
    BOOST_REQUIRE_EQUAL(traces.size(), 4);
    traces.erase(server_trace);
    traces.erase(client_trace1);
    traces.erase(client_trace2);
    BOOST_REQUIRE_EQUAL(traces.size(), 1);
    const std::string client_trace3 = *traces.begin();
    BOOST_CHECK(client_trace3.find("EmrEtp12-localhost-") != std::string::npos);
    BOOST_CHECK(boost::ends_with(client_trace3, ".trace"));

    stopServer();

    traces = get_traces();
    BOOST_REQUIRE_EQUAL(traces.size(), 4);

    //for (const std::string& t : traces) {
    //    runner.startAndWait({ "trace", "--validate", t });
    //}

    // Note that because the PID is saved as a varint, the header bytes
    // can vary slightly (+/- 2 bytes), thus we do an approximate check (.., i.e. 2 digits).
    // And apparently, even the messages bytes can vary, on some, between Linux and Windows,
    // perhaps around timestamps, not exactly sure...
    runner.startAndWait({ "trace", "--validate", server_trace });
    BOOST_CHECK(runner.outputLinesMatch({
        "SERVER ETP 1.2.0 \\(PID \\d+\\) .* UTC",
        "30 trace events; 48,... message bytes, ... header bytes.",
    }));

    // client_trace1 was used for two commands, so has two traces inside
    runner.startAndWait({ "trace", "--validate", client_trace1 });
    BOOST_CHECK(runner.outputLinesMatch({
        "CLIENT ETP 1.2.0 \\(PID \\d+\\) .* UTC",
        "5 trace events; ... message bytes, .. header bytes.",
        "CLIENT ETP 1.2.0 \\(PID \\d+\\) .* UTC",
        "5 trace events; ... message bytes, .. header bytes.",
        "", // varies around 1K, so either 1,... or ..., thus the \d+
        "TOTAL: 2 traces, 10 events; \\d+ message bytes, ... header bytes.",
    }));

    runner.startAndWait({ "trace", "--validate", client_trace2 }); // the EPC import
    BOOST_CHECK(runner.outputLinesMatch({
        "CLIENT ETP 1.2.0 \\(PID \\d+\\) .* UTC",
        "11 trace events; 46,... message bytes, ... header bytes.",
    }));

    runner.startAndWait({ "trace", "--validate", client_trace3 }); // space --stats
    BOOST_CHECK(runner.outputLinesMatch({
        "CLIENT ETP 1.2.0 \\(PID \\d+\\) .* UTC",
        "5 trace events; 6.. message bytes, .. header bytes.",
    }));

    // 30 != 5+5+11+5, because server-side has extra "marker" events
    // for "void-message" Core.CloseSession, 1 each for the 4 traces

    // Read all traces, and match CLT REQ to SVR REQ, and SVR RES to CLT RES,
    // verify the message bodies (past the MessageHeader's) are identical

    EtpTraceHeader header;
    EtpTraceReader12 reader = makeEtpTraceFileReader12(server_trace, header);
    BOOST_CHECK_EQUAL(header.etp_major_, 1);
    BOOST_CHECK_EQUAL(header.etp_minor_, 2);
    BOOST_CHECK_EQUAL(header.etp_patch_, 0);
    BOOST_CHECK_EQUAL(header.from_server_side(), true);
    EtpTraceReaderCtx reader_ctx;
    EtpTracerEventVec server_events;
    while (!reader_ctx.eof_ && !reader_ctx.io_errors_) {
        auto events = reader(reader_ctx);
        std::move(events.begin(), events.end(), std::back_inserter(server_events));
        BOOST_REQUIRE(reader_ctx.trace_header_);
        BOOST_CHECK_EQUAL(reader_ctx.trace_id_, 1);
        BOOST_CHECK(reader_ctx.trace_header_->from_server_side());
    }
    BOOST_CHECK(reader_ctx.eof_);
    BOOST_CHECK(!reader_ctx.io_errors_);
    BOOST_CHECK_EQUAL(server_events.size(), 30);

    struct Msg {
        std::string debug;
        oes::eml::etp12::Datatypes::MessageHeader header;
        std::string_view body;

        static Msg from(std::string& raw_msg) {
            static const ::avro::DecoderPtr& dec = ::avro::binaryDecoder();
            oes::common::StringAsAvroIStreamView buf(raw_msg);
            dec->init(buf);
            Msg msg;
            msg.header.decode(*dec);
            msg.debug = msgHeaderAsString(msg.header);
            dec->drain();
            const size_t header_bytes = buf.byteCount();
            msg.body = std::string_view(
                raw_msg.data() + header_bytes, raw_msg.size() - header_bytes
            );
            return msg;
        }
    };
    std::vector<Msg> svr_msgs;

    // FIXME: Responses come before the requests in server traces...
    //        That's because the server keeps the req event until
    //        after the response is sent (with its own event)
    BOOST_REQUIRE_EQUAL(server_events.size() % 2, 0);
    for (size_t i = 0; i < server_events.size(); i += 2) {
        auto& res = server_events[i];
        auto& req = server_events[i + 1];
        std::swap(res, req);
    }

    // All server events are monotonic in their start time
    constexpr auto eJSON        = EtpTracerEventInfo::Encoding::eJSON;
    constexpr auto eAVRO_BINARY = EtpTracerEventInfo::Encoding::eAVRO_BINARY;

    EtpTracerEventVec filtered_server_events; // w/o the CloseSession completion tokens

    auto prev = server_events[0].beg_;
    for (auto& evt : server_events) {
        if (evt.encoding() == eAVRO_BINARY) {
            svr_msgs.emplace_back(Msg::from(evt.msg_));
            filtered_server_events.emplace_back(evt);
        } else {
            BOOST_CHECK_EQUAL(evt.encoding(), eJSON);
            BOOST_CHECK(evt.msg_.find("CloseSession") != std::string::npos);
        }
        BOOST_CHECK(prev <= evt.beg_);
        prev = evt.beg_;
    }

    EtpTracerEventVec client_events;

    traces.erase(server_trace);
    for (const std::string& clt_trace : traces) {
        reader = makeEtpTraceFileReader12(clt_trace, header);
        BOOST_CHECK_EQUAL(header.etp_major_, 1);
        BOOST_CHECK_EQUAL(header.etp_minor_, 2);
        BOOST_CHECK_EQUAL(header.etp_patch_, 0);
        BOOST_CHECK_EQUAL(header.from_client_side(), true);

        reader_ctx = EtpTraceReaderCtx();
        while (!reader_ctx.eof_ && !reader_ctx.io_errors_) {
            auto events = reader(reader_ctx);
            std::move(events.begin(), events.end(), std::back_inserter(client_events));
            BOOST_REQUIRE(reader_ctx.trace_header_);
            BOOST_CHECK(reader_ctx.trace_header_->from_client_side());
        }
        BOOST_CHECK(reader_ctx.eof_);
        BOOST_CHECK(!reader_ctx.io_errors_);
    }

    // server_events have extra events, for CloseSession "completion".
    // that's why we compared against filtered_server_events w/o those
    BOOST_REQUIRE_EQUAL(client_events.size(), filtered_server_events.size());

    // Since we re-use a trace file for two commands,
    // with another trace in between, the client traces cannot be properly ordered
    std::sort(
        client_events.begin(), client_events.end(),
        [](const EtpTracerEventInfo& lhs, const EtpTracerEventInfo& rhs) {
            return lhs.beg_ < rhs.beg_;
        }
    );

    std::vector<Msg> clt_msgs;
    for (auto& evt : client_events) {
        BOOST_REQUIRE_EQUAL(evt.encoding(), eAVRO_BINARY);
        clt_msgs.emplace_back(Msg::from(evt.msg_));
    }

    BOOST_REQUIRE_EQUAL(clt_msgs.size(), svr_msgs.size());

    constexpr auto eSEND = EtpTracerEventInfo::Phase::eSEND;
    constexpr auto eRECV = EtpTracerEventInfo::Phase::eRECV;

    BOOST_REQUIRE_EQUAL(client_events.size(), filtered_server_events.size());
    for (size_t i = 0; i < filtered_server_events.size();) {
        const EtpTracerEventInfo& clt_evt_req = client_events[i];
        const EtpTracerEventInfo& svr_evt_req = filtered_server_events[i];

        const Msg& clt_msg_req = clt_msgs[i];
        const Msg& svr_msg_req = svr_msgs[i];

        // Chronological order
        BOOST_CHECK_EQUAL(clt_evt_req.phase(), eSEND);
        BOOST_CHECK_EQUAL(svr_evt_req.phase(), eRECV);

        BOOST_CHECK(clt_msg_req.body == svr_msg_req.body);

        // Using debug as proxy for missing MessageHeader::op==
        BOOST_CHECK_EQUAL(clt_msg_req.debug, svr_msg_req.debug);

        // No response in CloseSession's case
        if (typeOf(clt_msg_req.header) == proto12::Core::CloseSession::info.type_) {
            ++i;
            continue;
        }

        const EtpTracerEventInfo& clt_evt_res = client_events[i + 1];
        const EtpTracerEventInfo& svr_evt_res = filtered_server_events[i + 1];

        BOOST_CHECK_EQUAL(svr_evt_res.phase(), eSEND);
        BOOST_CHECK_EQUAL(clt_evt_res.phase(), eRECV);

        const Msg& clt_msg_res = clt_msgs[i + 1];
        const Msg& svr_msg_res = svr_msgs[i + 1];

        // Using debug as proxy for missing MessageHeader::op==
        BOOST_CHECK_EQUAL(clt_msg_res.debug, svr_msg_res.debug);

        BOOST_CHECK(clt_msg_res.body == svr_msg_res.body);

        BOOST_CHECK_EQUAL(
            clt_msg_res.header.m_correlationId,
            svr_msg_req.header.m_messageId
        );

        i += 2;
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
