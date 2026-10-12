// ============================================================================
// Copyright 2016-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "EmlAppContext.h"
#include "EtpClient12.h"

#include <oes/client/Client.hpp>
#include <oes/common/etp12/Messages.h>
#include <oes/common/etp12/Metadata.h>
#include <oes/common/AvroUtils.hpp>

#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Json.hpp>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThousandSep.h>

#include <oes/core/http/HTTP.h>

#include <oes/osdu/api/PartitionClient.h>



using namespace oes::apps::eml;
using namespace oes::core;

namespace po = boost::program_options;
namespace data12 = oes::common::etp12::Datatypes;

namespace {

//============================================================================

bool alt_etp12_server_ping(const EmlAppContext& ctx) {
    EtpClient12 clt(ctx);
    if (!clt.connect()) {
        return false;
    }

    auto* proto = clt.core_proto();
    if (!proto) {
        return false;
    }

    auto rpc = etpc::rpcFor(proto, &etpc::proto12::ICore::Ping);

    using namespace std::chrono;
    auto sys_now = time_point_cast<microseconds>(system_clock::now());
    rpc.req_.m_currentDateTime = sys_now.time_since_epoch().count();

    auto before = time_point_cast<microseconds>(high_resolution_clock::now());
    auto rc = rpc.call(ctx.err(), clt.put_get_timeout_);
    auto after = time_point_cast<microseconds>(high_resolution_clock::now());

    if (!rc.first || rc.second != std::future_status::ready) {
        OES_ERROR("Ping failed");
        return false;
    }

    int64_t diff = rpc.res_.m_currentDateTime - rpc.req_.m_currentDateTime;
    int64_t took = after.time_since_epoch().count() - before.time_since_epoch().count();
    int64_t skew = std::abs(diff - (std::max<int64_t>(2, took)/2));

    microseconds server_us(rpc.res_.m_currentDateTime);
    time_point<system_clock, microseconds> server_tp(server_us);

    auto client_ts = formatExtendedIsoDateTime(sys_now, FracSecRes::eMicroSecFloor);
    auto server_ts = formatExtendedIsoDateTime(server_tp, FracSecRes::eMicroSecFloor);

    OES_INFO("Client Ping Timestamp: {} UTC ({} us since EPOCH)"
        , client_ts, thou(rpc.req_.m_currentDateTime));
    OES_INFO("Server Pong Timestamp: {} UTC ({} us since EPOCH)"
        , server_ts, thou(rpc.res_.m_currentDateTime));
    if (diff < 0) {
        OES_INFO("   Client-Server Diff: -{}us", thou(-diff));
    } else {
        OES_INFO("   Client-Server Diff: {}us", thou(diff));
    }
    OES_INFO("  Ping/Pong Roundtrip: {}us", thou(took));
    OES_INFO("   Approx. Clock Skew: {}us", thou(skew));

    return true;
}

//============================================================================

bool alt_etp12_server_info(const EmlAppContext& ctx) {
    EtpClient12 clt(ctx);
    if (!clt.config()) {
        return false;
    }

    std::string url = ctx.stringArg("server-url");
    if (url.rfind("ws://", 0) == 0) { 
        url = url.replace(0,2, "http");
    } else if (url.rfind("wss://", 0) == 0) { 
        url = url.replace(0,3, "https");
    }
    
    if (url.empty()) {
        OES_WARNING("No --server-url specified. Using localhost:9002");
        url = "http://localhost:9002";
    }

    std::string req_url;
    oes::core::HTTP req;
    oes::core::HTTP::Response res;
    std::vector<std::string> headers{ "Accept: application/json" };

    if (!clt.cfg_.auth_hdr_.empty()) {
        headers.emplace_back("Authorization: " + clt.cfg_.auth_hdr_);
        OES_DEBUG("Authorization: {}", StringUtils::abbreviate(clt.cfg_.auth_hdr_));
    }

    req_url = url + "/.well-known/etp-server-capabilities?GetVersions=true";
    OES_DEBUG("HTTP GET {}", req_url);
    res = req.get(req_url, 0, headers);
    if (res.code != 200) {
        OES_ERROR("HTTP GET {} KO: {} {}", req_url, res.code, res.status);
        return false;
    }
    OES_DEBUG("BODY ({} bytes):", res.body.size());
    OES_INFO(res.body);
    if (!json_validate_syntax(res.body.c_str(), res.body.size())) {
        OES_ERROR("Invalid JSON: GET ...?GetVersions=true");
        return false;
    }
    std::vector<std::string> versions;
    if (!json_parse(res.body.c_str(), res.body.size(), versions, JSON_TYPE_STRING)) {
        OES_ERROR("Unexpected JSON: GET ...?GetVersions=true");
        return false;
    }

    switch (const size_t N = versions.size()) {
    case  0: OES_WARNING("Server advertises NO versions."); break;
    case  1: OES_WARNING("Server advertises 1 version:"); break;
    default: OES_WARNING("Server advertises {} versions:", N); break;
    }

    for (const std::string& v : versions) {
        OES_WARNING("- {}", v);
    }

    for (const std::string& v : versions) {
        res.body.clear();
        res.status.clear();
        OES_WARNING("\n=== {} ===", v);
        req_url = url + "/.well-known/etp-server-capabilities?GetVersion=" + v;
        OES_DEBUG("HTTP GET {}", req_url);
        res = req.get(req_url, 0, headers);
        if (res.code != 200) {
            OES_ERROR("HTTP GET {} KO: {} {}", req_url
                , res.code, res.status);
            continue;
        }
        OES_DEBUG("BODY ({} bytes):", res.body.size());
        OES_INFO(res.body);
        if (!json_validate_syntax(res.body.c_str(), res.body.size())) {
            OES_ERROR("Invalid JSON: GET ...?GetVersion={}", v);
            continue;
        }
        data12::ServerCapabilities caps;
        oes::common::my_buffer_ptr buf(new std::vector<uint8_t>(
            res.body.begin(), res.body.end()
        ));
        try {
            oes::common::decode_json(caps, buf);
        } catch (const std::exception& ex) {
            OES_ERROR("Avro JSON Parse: {}", ex.what());
            OES_ERROR("\tof GET ...?GetVersion={}", v);
            continue;
        } catch (...) {
            OES_ERROR("Avro JSON Parse: GET ...?GetVersion={}", v);
            continue;
        }

        OES_WARNING("{} ({})\n{}\n"
            , caps.m_applicationName
            , caps.m_applicationVersion
            , caps.m_contactInformation.m_organizationName
        );

        switch (const size_t N = caps.m_supportedProtocols.size()) {
        case  0: OES_WARNING("NO protocols. Ignored!"); continue;
        case  1: OES_WARNING("Just 1 protocol:"); break;
        default: OES_WARNING("{} protocols:", N); break;
        }
        for (const auto& proto : caps.m_supportedProtocols) {
            using namespace oes::common::etp12;
            auto found = protocols().find(proto.m_protocol);
            if (found == protocols().end()) {
                OES_WARNING("Unknown protocol ID: {}", proto.m_protocol);
                continue;
            }
            const ProtocolInfo& info = found->second;

            OES_WARNING("- {}", info.label());
        }

        const size_t nEnc = caps.m_supportedEncodings.size();
        const size_t nCmp = caps.m_supportedCompression.size();
        const size_t nFmt = caps.m_supportedFormats.size();

        if (nEnc + nCmp + nFmt) {
            OES_WARNING("");
        }

        if (nEnc) {
            std::string csv = boost::join(caps.m_supportedEncodings, ", ");
            OES_WARNING("{} supported encodings: {}", nEnc, csv);
        }
        if (nCmp) {
            std::string csv = boost::join(caps.m_supportedCompression, ", ");
            OES_WARNING("{} supported compression: {}", nCmp, csv);
        }
        if (nFmt) {
            std::string csv = boost::join(caps.m_supportedFormats, ", ");
            OES_WARNING("{} supported formats: {}", nFmt, csv);
        }

        if (const size_t N = caps.m_supportedDataObjects.size()) {
            OES_WARNING("\n{} supported objects: ", N);
            for (const auto& type : caps.m_supportedDataObjects) {
                auto& obj_caps = type.m_dataObjectCapabilities;

                // keys of obj_caps are supposed to be case-insensitive,
                // so we must normalize the keys first...
                std::map<std::string, const data12::DataValue*> imap;
                for (auto it = obj_caps.begin(); it != obj_caps.end(); ++it) {
                    imap.emplace(boost::to_lower_copy(it->first), &it->second);
                }

                std::vector<std::string> verb_caps;
                auto it = obj_caps.find("supportsget");
                if (it != obj_caps.end()) {
                    if (const bool* p_get = std::get_if<bool>(&it->second.m_item)) {
                        if (*p_get) {
                            verb_caps.emplace_back("get");
                        }
                    }
                }
                it = obj_caps.find("supportsput");
                if (it != obj_caps.end()) {
                    if (const bool* p_put = std::get_if<bool>(&it->second.m_item)) {
                        if (*p_put) {
                            verb_caps.emplace_back("put");
                        }
                    }
                }
                it = obj_caps.find("supportsdelete");
                if (it != obj_caps.end()) {
                    if (const bool* p_del = std::get_if<bool>(&it->second.m_item)) {
                        if (*p_del) {
                            verb_caps.emplace_back("delete");
                        }
                    }
                }
                // TODO(unknown): Show other caps, from DataObjectCapabilityKind
                std::string csv = boost::join(verb_caps, ", ");
                OES_WARNING("- {} ({})", type.m_qualifiedType, (!csv.empty()? csv: "no caps"));
            }
        }

        auto max_msg_found = caps.m_endpointCapabilities.find("MaxWebSocketMessagePayloadSize");
        // FIXME: Remove: Temporary work-around for PV's server
        if (max_msg_found == caps.m_endpointCapabilities.end()) {
            max_msg_found = caps.m_endpointCapabilities.find("maxWebSocketMessagePayloadSize");
        }
        if (max_msg_found != caps.m_endpointCapabilities.end()) {
            size_t N = 0;
            const data12::DataValue_item_v& val = max_msg_found->second.m_item;
            switch (val.index()) {
            case static_cast<size_t>(data12::DataValue_item_e::eDouble): N = static_cast<size_t>(std::get<double>(val)); break;
            case static_cast<size_t>(data12::DataValue_item_e::eFloat):  N = static_cast<size_t>(std::get<float>(val)); break;
            case static_cast<size_t>(data12::DataValue_item_e::eLong):   N = static_cast<size_t>(std::get<int64_t>(val)); break;
            case static_cast<size_t>(data12::DataValue_item_e::eInt):    N = static_cast<size_t>(std::get<int>(val)); break;
            default:
                OES_WARNING("\nWarning: Unknown type for MaxWebSocketMessagePayloadSize value");
            }
            if (N) {
                OES_WARNING("\nMaximum WebSocket Message Payload Size: {} bytes"
                    , thou(N));
            }
        } else {
            OES_WARNING("\nNO Maximum WebSocket Message Payload Size advertised!");
        }
    }

    return true;
}

//============================================================================

bool alt_osdu_platform_info(const EmlAppContext& ctx) {
    const std::string PARTITION_RESERVOIR_KEY = "reservoir-connection";

    EtpClient12 clt(ctx);
    if (!clt.config()) {
        return false;
    }

    std::string oauthToken;
    if (!clt.cfg_.auth_hdr_.empty()) {
        oauthToken = clt.cfg_.auth_hdr_;
        ctx.dbg() << "Authorization: " << StringUtils::abbreviate(clt.cfg_.auth_hdr_) << std::endl;
    } else {
        ctx.err() << "Error: OSDU principal_token not found." << std::endl;
        return false;
    }
    
    oes::osdu::api::PartitionClient partitionClient(ctx.appConfig());
    std::vector<std::string> partitions = partitionClient.getPartitionList(oauthToken);

    ctx.msg() << "List of available data partitions:" << std::endl;
    for(const auto& partitionName: partitions) {
        const auto partitionKeyVault = partitionClient.getPartitionInfo(oauthToken, partitionName);

        const auto partitionCell = partitionKeyVault.find(PARTITION_RESERVOIR_KEY);
        if (partitionCell == partitionKeyVault.end()) {
            ctx.msg() << partitionName << " (Reservoir DDMS not supported for this partition)" << std::endl;
        } else {
            ctx.msg() << partitionName << " (Ready to work)" << std::endl;
        }
    }

    return true;
}

//============================================================================

} //namespace

namespace oes {
    namespace apps {
        namespace eml {

        /****************************************************************************/

        int probe_main(EmlAppContext& ctx, const UnparsedArgs& unparsed_args) {
            po::options_description cmd_desc(
                "\nUsage: openETPServer [global-options] " + ctx.command + " --info -S URL\n"
                "Usage: openETPServer [global-options] " + ctx.command + " --ping -S URL [auth-options]\n"
                "\nActions", ctx.terminalWidth_
            );
            cmd_desc.add_options()
                ("help,h", "Display this information")

                ("info", "Get ETP 1.2 server capabilities")
                ("ping", "Connect and ping ETP 1.2 server")
                ("osdu", "Get OSDU data platform support info")
            ;

            cmd_desc.add(connect_options(ctx.terminalWidth_));
            cmd_desc.add(authenticate_options(ctx.terminalWidth_));
            cmd_desc.add(osdu_partition_options(ctx.terminalWidth_));

            if (int rc = ctx.reparse(cmd_desc, po::positional_options_description(), unparsed_args)) {
                return (rc == -1)? 0: rc;
            }

            EmlAppNamedAction actions[] {
                { "info", &alt_etp12_server_info },
                { "ping", &alt_etp12_server_ping },
                {"osdu", &alt_osdu_platform_info },
            };
            return ctx.dispatchActions(actions);
        }

        } // namespace eml
    } // namespace apps
} // namespace oes
