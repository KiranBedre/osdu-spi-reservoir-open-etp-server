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

#include <set>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <fmt/format.h>
#include <boost/regex.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "EtpClient12.h"
#include <oes/client/Client.hpp>
#include <oes/common/etp12/Metadata.h>

#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/InputByteParser.h>


namespace oes::apps::eml {

using oes::client::rpcFor;
using oes::common::etp12::Datatypes::ErrorInfo;

//============================================================================

EtpClient12::EtpClient12(
    const EmlAppContext& ctx
) : ctx_(ctx)
{
}

bool EtpClient12::config() {
    const std::string& timeouts_spec = ctx_.stringArg("timeout");
    auto timeouts = oes::core::parseCsvSimpleDurationPair(timeouts_spec);
    const std::chrono::milliseconds zero_ms(0);
    if (timeouts.first < zero_ms || timeouts.second < zero_ms) {
        OES_ERROR("Invalid --timeout. Unknown time unit (accepts s, ms, and min only).");
        return false;
    }
    if (timeouts.first == zero_ms || timeouts.second == zero_ms) {
        OES_ERROR("Invalid --timeout. Cannot be empty or zero.");
        return false;
    }
    connect_timeout_ = timeouts.first;
    put_get_timeout_ = timeouts.second;


    if (!ctx_.hasArg("server-url")) {
        OES_ERROR("Missing --server-url option");
        return false;
    }

    if (ctx_.hasArg("etp-trace")) {
        cfg_.enable_tracing_ = true;
        cfg_.trace_file_ = ctx_.stringArg("etp-trace");
    }

    std::string space_path  = ctx_.stringArg("space-path");

    const std::string& mms_spec = ctx_.stringArg("max-msg-size");
    if (!mms_spec.empty()) { try {
        cfg_.max_mgs_size_ = oes::core::parseByteSize(mms_spec, 2147483647); // max 2 Gib
    } catch (const std::runtime_error& ex) {
        OES_ERROR("Invalid --max-msg-size option: {}", ex.what());
        return false;
    }
    }

    auto warnWhenSet = [this](std::vector<std::string> args) {
        for (const auto& arg : args) {
            if (ctx_.hasArg(arg)) {
                OES_WARNING("Option -- {} ignored", arg);
            }
        }
    };
    auto warnWhenEmpty = [this](std::vector<std::string> args) {
        for (const auto& arg : args) {
            if (ctx_.stringArg(arg).empty()) {
                OES_WARNING("No -- {} option", arg);
            }
        }
    };

    std::string auth = ctx_.stringArg("auth");

    if (auth == "auto") {
        if (ctx_.hasArg("jwt-token") || ctx_.hasArg("jwt-secret")) {
            auth = "bearer";
        } else if (ctx_.hasArg("username") || ctx_.hasArg("password")) {
            auth = "basic";
        } else {
            auth = "none";
        }
    }

    if (auth == "none") {
        warnWhenSet({ "username", "password", "jwt-secret", "jwt-token" });
        if (ctx_.stringArg("auth") != "none") { // i.e. not --auth=auto
            OES_INFO("No authentication, since no --username or --password");
        }
        cfg_.auth_hdr_.clear();
    } else if (auth == "basic") {
        warnWhenSet({ "jwt-secret", "jwt-token" });
        warnWhenEmpty({ "username", "password" });

        std::string username = ctx_.stringArg("username");
        std::string password = ctx_.stringArg("password");
        cfg_.withBasicAuth(username, password);
    } else if (auth == "bearer") {
        if (ctx_.hasArg("jwt-token")) {
            warnWhenSet({ "username", "password", "jwt-secret" });

            cfg_.withJwtAuth(ctx_.stringArg("jwt-token"));
        } else if (ctx_.hasArg("jwt-secret")) {
            warnWhenSet({ "jwt-token" });
            warnWhenEmpty({ "username", "password" });

            std::string username = ctx_.stringArg("username");
            std::string password = ctx_.stringArg("password");
            cfg_.withJwtAuth(username, password, ctx_.stringArg("jwt-secret"));
        } else {
            OES_ERROR("Bearer authentication requires --jwt-secret or --jwt-token");
            return false;
        }
    } else {
        assert(auth != "auto");
        OES_ERROR("Invalid --auth option: "
            "Use 'auto', 'none', 'basic, or 'bearer'");
        return false;
    }

    if (ctx_.hasArg("data-partition-id")) {
        cfg_.data_partition_id_hdr_ = ctx_.stringArg("data-partition-id");
    }

    if (ctx_.hasArg("no-http-headers")) {
        cfg_.emulate_browser_client_ = true;
    }

    if (ctx_.hasArg("acknowledge")) {
        cfg_.acknowledge_ = true;
    }

    cfg_.compress_ = ctx_.hasArg("compress");
    if (cfg_.compress_) {
        std::string zcomp_csv = ctx_.stringArg("compress");
        std::vector<std::string> zcomp_args;
        boost::split(zcomp_args, zcomp_csv, boost::algorithm::is_any_of(",;|"));
        // Check compressor names perhaps
        cfg_.zcompressors_ = zcomp_args;
    }

    if (ctx_.hasArg("shun-protocols")) {
        // Split the arg's value
        std::string shun_csv = ctx_.stringArg("shun-protocols");
        std::vector<std::string> shun_args;
        boost::split(shun_args, shun_csv, boost::algorithm::is_any_of(",;|"));

        // Build the set of valid values to be shunned.
        // Accept Store (the name), Store(4) (the label), 4 (just the ID).
        const auto& proto_map = oes::common::etp12::protocols();
        using ProtoEnum = oes::common::etp12::Datatypes::Protocol;
        std::map<std::string, oes::common::etp12::Datatypes::Protocol> accepted;
        for (const auto& entry : proto_map) {
            const oes::common::etp12::ProtocolInfo& pinfo = entry.second;
            const auto proto_id = static_cast<ProtoEnum>(entry.first);
            if (proto_id != ProtoEnum::eCore) {
                accepted.emplace(boost::to_lower_copy(pinfo.name_), proto_id);
                accepted.emplace(boost::to_lower_copy(pinfo.label()), proto_id);
                accepted.emplace(std::to_string(static_cast<int>(proto_id)), proto_id);
            }
        }
        // Add my own "shorter" aliases too
        accepted.emplace("types", ProtoEnum::eSupportedTypes);
        accepted.emplace("disco", ProtoEnum::eDiscovery);
        accepted.emplace("trans", ProtoEnum::eTransaction);
        accepted.emplace("space", ProtoEnum::eDataspace);
        accepted.emplace("array", ProtoEnum::eDataArray);

        std::set<ProtoEnum> shunned;
        for (const std::string& arg : shun_args) {
            auto shun_arg = boost::to_lower_copy(arg);
            boost::trim(shun_arg);
            auto found = accepted.find(shun_arg);
            if (found != accepted.end()) {
                ProtoEnum proto_id = found->second;
                if (proto_id == ProtoEnum::eCore) {
                    OES_ERROR("Cannot shun Core(0) protocol");
                    return false;
                }
                if (shunned.emplace(proto_id).second) {
                    auto proto_found = proto_map.find(static_cast<int>(proto_id));
                    assert(proto_found != proto_map.end());
                    OES_DEBUG("Shun-Protocol {}", proto_found->second.label());
                }
            } else {
                OES_ERROR("Cannot shun unknown protocol: {}", arg);
                return false;
            }
        }

        if (shunned.empty()) {
            OES_ERROR("Invalid --shun-protocols option: empty");
            return false;
        }

        std::vector<ProtoEnum> requested;
        for (auto proto : cfg_.requested_protocols_) {
            if (shunned.find(proto) == shunned.end()) {
                requested.emplace_back(proto);
            } else {
                auto proto_found = proto_map.find(static_cast<int>(proto));
                assert(proto_found != proto_map.end());
                OES_WARNING("Shunning {} protocol", proto_found->second.label());
            }
        }
        cfg_.requested_protocols_.swap(requested);

        if (cfg_.requested_protocols_.empty()) {
            OES_WARNING("All requested protocols were shunned");
            return false;
        }
    }

    cfg_.server_url_ = ctx_.stringArg("server-url");

    return true;
}

bool EtpClient12::connect() {
    if (!config()) {
        return false;
    }

    clt_.reset(new etpc::Client(cfg_));
    ErrorInfo err;

    if (!clt_->connect(connect_timeout_, &err)) {
        OES_ERROR("Failed connect to server: {} # {}: {}", cfg_.server_url_
            , err.m_code, err.m_message);
        return false;
    }
    return true;
}

std::vector<Dataspace> EtpClient12::globSpaces(
    const std::string& pattern, size_t* total
) {
    return globSpacesSince(pattern, 0, total);
}

/**
 * @brief Get the list of dataspaces matching the given pattern.
 * @param pattern The pattern to match. (Note that it is a regex pattern, not a glob pattern.)
 * @param lastChangedFilter If non-zero, only return dataspaces that have
 *       changed since this timestamp.
 * @param total If non-null, the total number of dataspaces before the
 *      pattern is applied.
 * @return The list of dataspaces matching the pattern.
*/
std::vector<Dataspace> EtpClient12::globSpacesSince(
    const std::string& pattern, int64_t lastChangedFilter, size_t* total
) {
    auto* proto = space_proto();
    if (!proto) {
        throw ActionFailed();
    }

    auto rpc = rpcFor(proto, &etpc::proto12::IDataspace::GetDataspaces);

    if (lastChangedFilter) {
        rpc.req_.m_storeLastWriteFilter = lastChangedFilter;
    }


    std::vector<oes::common::etp12::Datatypes::Object::Dataspace> spaces;
    if (!rpc.callForEachValue(
        ctx_.err(), put_get_timeout_,
        [&](auto& ds) {
            if(!pattern.empty()) {
                if( boost::regex_match(ds.m_path, boost::regex(pattern) ) ){
                    spaces.emplace_back(ds);
                }
            }else{
                spaces.emplace_back(ds);
            }
        }
    )) {
        throw ActionFailed();
    }

    if (total) {
        *total = spaces.size();
    }

    return spaces;
}

//============================================================================

options_description connect_options(unsigned terminalWidth, EtpDefaults defaults) {

    const std::string& dT = defaults.timeout_;
    const std::string& dZ = defaults.zcompressors_;
    const std::string& dM = defaults.max_msg_size_;
    const int          dB = defaults.max_batch_size_;
    const int          dQ = 500; // Deprecated. Left for backward compatibility 

    auto zarg = defaults.compress_? txtArg(dZ): txtArgOpt(dZ);
    auto zcmt = "Set Compress-Body flag on all ETP requests\n"
                "Specify compressor(s) (Default: " + dZ + ")";

    options_description opts("Connect Options", terminalWidth);
    opts.add_options()
        ("server-url,S",    txtArg(),       "ETP 1.2 server URL")
        ("etp-trace",       txtArgOpt(),    "Trace ETP messages to file\n"
                                            "Filename inferred if empty or directory")

        ("timeout",             txtArg(dT), "ETP timeouts for CONNECT and GET/PUT requests")
        ("max-msg-size,M",      txtArg(dM), "Maximum WebSocket message size(s) in bytes")
        ("max-batch-size,B",    intArg(dB), "Maximum parts per multi-part message")

        // Advertise how many concurrent (in-flight) messages this server
        // is willing to accept per-session/connection. This is to handle
        // "back-pressure", and limit per-session memory usage.
        ("msg-queue-depth,Q",   intArg(dQ),   "Deprecated: Maximum concurrent\n"
                                              "(in-flight) messages")

        ("acknowledge",                     "Set Acknowledge flag on all ETP requests")

        // On client-side, compression is opt-in, while
        // on server-side, compression is always active, BUT depends on client
        ("compress,Z", zarg, zcmt.c_str())

        // Allows client-side to simulate a server not implementing a protocol
        // the server does in fact support, by not requesting it. For testing only.
        // E.g. `space --stats` normally uses SupportedTypes. Shunning SupportedTypes
        // allows testing its (slower) fallback using Discovery protocol instead.
        ("shun-protocols",  txtArg(),   "Force client to ignore those protocols")
     // ("shun-extensions", txtArg(),   "Force client to ignore those extensions") // TODO
     // ("hide-capabilities", txtArg(), "Force endpoint to hide its capabilities") // TODO
    ;
    return opts;
}

//============================================================================

options_description authenticate_options(unsigned terminalWidth) {
    options_description opts("Authentication Options", terminalWidth);
    opts.add_options()
        // Auth-related options
        ("auth,A", txtArg("auto"), "Authentication method: 'auto, 'none', 'basic', 'bearer'.")
        ("username,u",   txtArg(), "User's name to authenticate with.")
        ("password,p",   txtArg(), "User's password to authenticate with.")
        ("jwt-token,T",  txtArg(), "Explicit JWT token. Used as-is.")
        ("jwt-secret,k", txtArg(), "JWT Signing Key. Uses Basic Authentication if omitted.")
        ("no-http-headers",        "Use URL for AuthN. Emulates Browser-based clients.")
    ;
    return opts;
}

//============================================================================

options_description osdu_partition_options(unsigned terminalWidth) {
    options_description opts("OSDU Partition Options", terminalWidth);
    opts.add_options()
        ("data-partition-id,P", txtArg(), "OSDU data partition name.")
    ;
    return opts;
}

//============================================================================

} // namespace oes::apps::eml
