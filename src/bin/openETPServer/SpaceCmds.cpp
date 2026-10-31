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

#include <iomanip>
#include <filesystem>
#include <string_view>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "EmlAppContext.h"
#include "EtpClient12.h"

#include <oes/client/Client.hpp>
#include <oes/client/ImportEpc.h>
#include <oes/client/ExportEpc.h>
#include <oes/common/etp12/Protocols.h>

#include <oes/eml/etp12/Extensions.h>
#include <oes/eml/etp12/MsgUtils.h>

#include <oes/common/etp12/EmlUri.h>

#include <oes/eml/openkv/AdminDB.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Json.hpp>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/RangeUtils.hpp>
#include <oes/core/utils/ThousandSep.h>
#include <oes/core/utils/Otel.h>
#include <oes/core/utils/InputByteParser.h>

using namespace oes::apps::eml;
using oes::common::etp12::EmlUri;
namespace etp = oes::eml::etp12;
namespace common12 = oes::common::etp12;

namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace {

using oes::core::thou;
using oes::client::rpcFor;
const std::string dflt = "<Default>";

int64_t parseSince(const std::string& since, bool utc) {
    if (since.empty()) {
        return 0;
    }

    // Not a valid ISO date-time
    using namespace std::chrono;
    microseconds since_epoch(0); // UTC or Local, it depends
    if (oes::core::usecSinceEpochFromExtendedIsoDateTime(since, since_epoch)) {
        if (!utc) {
            // We assume the date we parsed is in local time, not UTC
            /*
            ** This requires date/tz.h or C++20
            time_point<system_clock, microseconds> tp_localtime(since_epoch);
            auto local_time = date::make_zoned(date::current_zone(), tp_localtime);
            return local_time.get_sys_time().time_since_epoch().count();
            */
            system_clock::time_point tp(since_epoch);
            tp = oes::core::localToUtc(tp);
            system_clock::duration ts = tp.time_since_epoch();
            since_epoch = duration_cast<microseconds>(ts);
        }
        return since_epoch.count();
    }

    std::istringstream iss(since);
    int64_t num;
    std::string uom;
    iss >> num >> uom;
    if (iss.fail() || !iss.eof()) {
        return -1;
    }

    microseconds offset(0);
    switch (uom.size()) {
    case  0: return -1;
    case  1:
        switch (uom[0]) {
        case 's': offset = seconds(num); break;
        case 'h': offset = hours(num); break;
        // days, weeks, months, years are C++20 only
        case 'd': offset = duration<int64_t, std::ratio<86400>>(num); break;
        case 'w': offset = duration<int64_t, std::ratio<604800>>(num); break;
        case 'm': offset = duration<int64_t, std::ratio<2629746>>(num); break;
        case 'y': offset = duration<int64_t, std::ratio<31556952>>(num); break;
        }
        break;
    case  2: return -1;
    case  3:
        if (uom == "min") {
            offset = minutes(num);
        }
        break;
    default: return -1;
    }

    if (offset.count() == 0) {
        return -1;
    }

    const auto now = system_clock::now(); // UTC
    const auto since_usec = time_point_cast<microseconds>(now - offset);
    return since_usec.time_since_epoch().count();
}

std::string printMicroSecDateTime(int64_t etp_usec, bool utc, bool iso, bool debug=true) {
    using namespace oes::core;
    using namespace std::chrono;
    microseconds usec(etp_usec); // UTC
    time_point<system_clock, microseconds> tp(usec);
    if (!utc) {
        tp = time_point_cast<microseconds>(utcToLocal(tp));
    }
    std::string dt_fmt = iso? isoDatetimeFormat(): ascDatetimeFormat();
    FracSecRes dt_res = debug && oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug
        ? FracSecRes::eMicroSecFloor: FracSecRes::eWholeSecFloor;
    std::string ts = formatDateTime(dt_fmt, tp, dt_res);
    if (utc) {
        ts += " UTC";
    }
    return ts;
}

std::pair<std::string, bool> getSpacePath(
    const EmlAppContext& ctx,
    bool required = false,
    bool default_ok = true,
    const std::string& key = "space-path"
) {
    if (!ctx.hasArg(key)) {
        if (required) {
            OES_ERROR("Missing --{} option", key);
            return { {}, false };
        }
    }

    std::string space_path = ctx.stringArg(key);
    if (space_path.empty()) {
        if (!default_ok && !ctx.hasArg("force")) {
            OES_ERROR("Cannot use {} unnamed dataspace", dflt);
            return { {}, false };
        }
        OES_WARNING("Using {} unnamed dataspace", dflt);
    }else{
        const size_t first_slash = space_path.find('/');
        if(first_slash == std::string::npos){
            space_path = space_path + "/default";
        }
    }
    return { std::move(space_path), true };
}

//============================================================================

// Convert special (Windows) chars that cannot appear in filenames
// Linux accepts arbitrary strings for filenames, but lets be consistent.
constexpr char win_special_char[] = "\\/:*?\"<>|";
std::string scrubFilename(const std::string& s) {
    if (s.find_first_of(win_special_char) == std::string::npos) {
        return s;
    }
    std::string copy(s);
    for (char& c : copy) {
        if (std::strchr(win_special_char, c)) {
            c = '#';
        }
    }
    return copy;
};

bool tryRemovefile(const std::string& filename, bool overwrite) {
    if (fs::exists(filename)) {
        if (!overwrite) {
            OES_ERROR("Output file exists (and --overwrite not specified)");
            return false;
        }
        if (!fs::remove(filename)) {
            OES_ERROR("Output file exists and cannot be removed");
            return false;
        }
    }
    return true;
}

// Unfortunately, fs::status(path).permissions() is not reliable on Windows.
// Especially for folder in C:\Program Files, with special UAC protections.
bool canWriteFileInFolder(const std::string& filename) {
    namespace fs = fs;
    fs::path path(filename);
    const size_t len = path.filename().string().size();

    // Warning: .parent_path() of relation path may be empty,
    // must convert to absolute path before getting its parent.
    fs::path folder = absolute(path).parent_path();
    if (!is_directory(folder)) {
        return false;
    }

    bool can_write = false;
    for (int i = 0; i < 7; ++i) {
        // to avoid running in path length limitations, especially
        // on Windows, don't use the full 32 chars of the Guid, but
        // as many of the input file name. But Use a minimum of 7 chars,
        // just in case, and account for those pre-existing (100 tries).
        std::string uid = oes::core::Guid::make().toStringNoDash();
        std::string stem = 'x' + uid.substr(0, std::max<size_t>(7, len));
        fs::path tmp_file = folder / stem;
        if (exists(tmp_file)) {
            continue;
        }
        try {
            std::ofstream ofs(tmp_file.string());
            if (ofs.is_open()) {
                ofs << '\n';
                ofs.close();
                can_write = true;
                try {
                    fs::remove(tmp_file);
                } catch (const std::exception&) {
                    // ignore, i.e. leave temp file behind...
                    // If we do not catch here, we don't break below
                }
                break;
            }
        } catch (const std::exception&) {
            // ignore? or break? We continue spinning if ignored,
            // but only 7 times, so lets give that a try.
        }
    }
    return can_write;
}

//============================================================================

int hdf5Level(const EmlAppContext& ctx, const std::string& opt_name) {
    // we provide a default
    OES_DEBUG_ASSERT(ctx.hasArg(opt_name));

    const int zlevel = ctx.args[opt_name].as<int>();
    if (0 <= zlevel && zlevel <= 9) {
        return zlevel;
    }
    OES_ERROR(
        "Invalid --{} level {}. Not in [0-9] range. Assuming 0."
        , opt_name, zlevel
    );
    return 0; // not compressed
}

int hdf5CompressionLevel(const EmlAppContext& ctx) {
    return hdf5Level(ctx, "hdf5-compression");
}

int hdf5LargeDataCompressionLevel(const EmlAppContext& ctx) {
    return hdf5Level(ctx, "hdf5-large-data-compression");
}

//============================================================================

bool space_new(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    auto [space_path, space_ok] = getSpacePath(ctx, true, false);
    if (!space_ok) {
        return false;
    }

    oes::eml::etp12::XDataMap xdata;
    std::string xdata_file = ctx.stringArg("xdata-file");
    if (!xdata_file.empty()) {
        if (!oes::eml::etp12::xdataFromJsonFile(xdata, xdata_file)) {
            OES_ERROR("Cannot read or parse JSON file: {}", xdata_file);
            return false;
        }
        OES_DEBUG("Added {} custom-data from {}"
            , xdata.size(), xdata_file);
    }

    std::string xdata_input = ctx.stringArg("xdata");
    if (!xdata_input.empty()) {
        size_t prev_size = xdata.size();
        if (!oes::eml::etp12::xdataFromJson(xdata, xdata_input)) {
            OES_ERROR("Cannot read or parse JSON : {}", xdata_input);
            return false;
        }
        OES_DEBUG("Added {} custom-data from cmd line"
            , (xdata.size() - prev_size) );
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace
    });
    if (!clt.connect()) {
        return false;
    }

    auto* proto = clt.space_proto();
    if (!proto) {
        return false;
    }

    auto rpc = rpcFor(proto, &etpc::proto12::IDataspace::PutDataspaces);

    etpc::data12::Object::Dataspace space;
    space.m_path = space_path;
    if (!xdata.empty()) {
        space.m_customData.swap(xdata);
        OES_DEBUG("Added {} custom-data"
            , space.m_customData.size());
    }

    // by default, the URI is derived from the path,
    // as per the discussion on the ETP forum.
    std::string uid = space_path;

    // But as an extension, we allow using a Guid-based URI instead.
    // And the client can now control the Guid that will be used.
    // Unless one is not explicitly provided, in which case we make it up.
    const bool use_guid = ctx.hasArg("space-guid");
    if (use_guid) {
        using oes::core::Guid;
        std::string guid_txt = ctx.stringArg("space-guid");
        Guid guid = guid_txt.empty()? Guid::make(): Guid::parse(guid_txt);
        if (!guid.isValid()) {
            OES_ERROR("Invalid UUID for --space-guid: {}", guid_txt);
            return false;
        }
        uid = guid.toXmlString();
    }
    space.m_uri = EmlUri::make_space(uid).to_string();

    rpc.req_.m_dataspaces.emplace("1", space);

    auto ok = rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!ok.first) {
        return false;
    }

    return true;
}

bool space_info(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    auto [space_path, space_ok] = getSpacePath(ctx);
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDataspaceOSDU,
    });
    if (!clt.connect()) {
        return false;
    }

    std::vector<oes::common::etp12::Datatypes::Object::Dataspace> spaces;
    auto* osdu_proto = clt.space_proto_osdu();
    if(osdu_proto){
        // Try to get the dataspace from the OSDU protocol using the path in URI
        const EmlUri space_uri = EmlUri::make_space(space_path);
        const std::string space_uri_str = space_uri.to_string();
        auto osdu_get_rpc = rpcFor(osdu_proto, &etpc::proto12::IDataspaceOSDU::GetDataspaceInfo);
        osdu_get_rpc.req_.m_uris.insert(std::pair<std::string, std::string>(space_path, space_uri_str));
        if (!osdu_get_rpc.callForEachValue(
            ctx.err(), clt.put_get_timeout_,
            [&](auto& ds) {
                if(ds.second.m_path == space_path){
                    spaces.emplace_back(ds.second);
                }
            }
        )) {
            throw ActionFailed();
        }
    }
    
    if(spaces.empty()){
        auto* proto = clt.space_proto();
        if (!proto) {
            return false;
        }

        auto get_rpc = rpcFor(proto, &etpc::proto12::IDataspace::GetDataspaces);

        if (!get_rpc.callForEachValue(
            ctx.err(), clt.put_get_timeout_,
            [&](auto& ds) {
                if(ds.m_path == space_path){
                    spaces.emplace_back(ds);
                }
            }
        )) {
            throw ActionFailed();
        }
    }

    if (spaces.size() != 1) {
        OES_ERROR("ETP Dataspace not found: {}", space_path);
        return false;
    }
    const auto& space = spaces[0];

    auto uri = EmlUri::parse(space.m_uri);
    if (uri.type_ != EmlUri::eDATA_SPACE) {
        OES_ERROR("Invalid URI: Not of dataspace type: {}", space.m_uri);
    }

    int64_t cetp_usec = space.m_storeCreated;
    std::string cts = printMicroSecDateTime( cetp_usec, ctx.hasArg("utc"), ctx.hasArg("iso") );

    int64_t metp_usec = space.m_storeLastWrite;
    std::string mts = printMicroSecDateTime( metp_usec, ctx.hasArg("utc"), ctx.hasArg("iso") );

    OES_WARNING(R"(
Dataspace {}:
           URI: {}
           Uid: {}
          Path: {}
       Created: {}
 Last-Modified: {}
)",
        (space_path.empty()? dflt: space_path),
        space.m_uri, uri.dataspace_, space.m_path, cts, mts
    );

    if (const size_t N = space.m_customData.size()) {
        OES_WARNING("{} custom data:", N);
        size_t max_key = 0;
        for (const auto& entry : space.m_customData) {
            const size_t ks = entry.first.size();
            if (ks <= 32u) {
                max_key = std::max<>(max_key, ks);
            }
        }
        const size_t tw = ctx.terminalWidth_;
        const size_t kw = std::max<size_t>(14, std::min<size_t>(24, max_key)); // key width
        const size_t vw = std::max<size_t>(tw, 48) - kw - 3; // value width
        using DV = etp::Datatypes::DataValue_item_e;
        for (const auto& entry : space.m_customData) {
            const auto& key = entry.first;
            std::string key_output;
            if (key.size() <= kw) {
                key_output = fmt::format("{:>{}}: ", key, kw);
            } else {
                key_output = fmt::format("{:>{}}:\n{:<{}}  ", key, kw, "", kw);
            }

            const auto& item = entry.second.m_item;
            std::string item_output;
            switch (auto idx = item.index()) {
            case static_cast<size_t>(DV::eNull):    item_output = fmt::format("<NULL>"); break;
            case static_cast<size_t>(DV::eBool):    item_output = fmt::format("{}", std::get<bool>(item)); break;
            case static_cast<size_t>(DV::eInt):     item_output = fmt::format("{}", std::get<int>(item)); break;
            case static_cast<size_t>(DV::eLong):    item_output = fmt::format("{}", std::get<int64_t>(item)); break;
            case static_cast<size_t>(DV::eFloat):   item_output = fmt::format("{}", std::get<float>(item)); break;
            case static_cast<size_t>(DV::eDouble):  item_output = fmt::format("{}", std::get<double>(item)); break;
            case static_cast<size_t>(DV::eString):  {
                const std::string& val = std::get<std::string>(item);
                const auto line = val.substr(0, vw);
                item_output = fmt::format("{}\n", line);
                if (val.size() > vw) {
                    item_output += fmt::format(
                        "{:<{}}  (and {} more bytes, truncated...)",
                        "", kw, (val.size() - vw)
                    );
                }
                break;          }
            case static_cast<size_t>(DV::eBytes):   {
                const auto& val = std::get<std::vector<uint8_t>>(item);
                item_output = fmt::format("<blob of {} bytes>", val.size());
                break;          }
            case static_cast<size_t>(DV::eArrayOfString): {
                const auto& ary = std::get<etp::Datatypes::ArrayOfString>(item);
                if (ary.m_values.empty()) {
                    item_output = "{}";
                } else {
                    item_output = "\n {\n";
                    for (const auto& val : ary.m_values) {
                        item_output += fmt::format("\t{},\n", val);
                    }
                    item_output += " }";
                }
                break;
            }
            default:
                item_output = fmt::format("TODO: Print array value (type #{})", idx);
            }
            OES_WARNING(key_output + item_output);
        }
    } else {
        OES_WARNING("\nNo custom data.");
    }

    return true;
}

bool space_list(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    const bool iso = ctx.hasArg("iso");
    const bool utc = ctx.hasArg("utc");
    const std::string since = ctx.stringArg("since");
    const int64_t lastChangedFilter = parseSince(since, utc);
    if (lastChangedFilter < 0) {
        OES_ERROR(
            "Invalid --since value: {} \n\tUse an offset: 5min, 1d, 1w; or an ISO-8601 date-time (see --iso)"
            , since);
        return false;
    } else if (lastChangedFilter != 0) {
        using namespace std::chrono;
        microseconds usec(lastChangedFilter); // UTC
        system_clock::time_point ts(usec);
        OES_VERBOSE("Dataspaces since {} UTC", oes::core::formatIsoDateTime(ts));
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace
    });
    if (!clt.connect()) {
        return false; // error already issued
    }

    size_t total = 0;
    const std::string pattern = ctx.stringArg(ctx.action());
    auto spaces = clt.globSpacesSince(pattern, lastChangedFilter, &total);

    auto print = [&ctx, utc, iso](const etp::Datatypes::Object::Dataspace& space, size_t max_len) {
        const std::string& space_path = space.m_path.empty()? dflt: space.m_path;
        max_len = max_len? max_len: space_path.length();

        int64_t etp_usec = space.m_storeLastWrite;
        std::string ts = printMicroSecDateTime(etp_usec, utc, iso);

        size_t n = space.m_customData.size();

        bool ro = false;
        auto ro_found = space.m_customData.find("read-only");
        if (ro_found != space.m_customData.end()) {
            const auto& ro_entry = ro_found->second.m_item;
            if (const bool* p_ro = std::get_if<bool>(&ro_entry)) {
                ro = *p_ro;
                n-- ; // don't count read-only
            }
        }

        std::string size;
        auto size_found = space.m_customData.find("size");
        if (size_found != space.m_customData.end()) {
            const auto& size_entry = size_found->second.m_item;
            if (const std::string* p_string = std::get_if<std::string>(&size_entry)) {
                size = *p_string;
                n-- ; // don't count size
            }
        }

        std::string xdata = !n? "": fmt::format(" ({} custom data)", n);

        OES_WARNING(
            "- {:<{}} {} {}{} [{}]",
            space_path, max_len, (ro? "r-": "rw"), ts, xdata, size
        );
    };

    const bool flat = ctx.hasArg("flat");
    using SpacePtrs = std::vector<const etp::Datatypes::Object::Dataspace*>;
    using GroupedSpaceMap = std::map<std::string, SpacePtrs>;
    GroupedSpaceMap spaces_by_folders;
    SpacePtrs all_spaces; // kinda, already filtered in fact
    for (const auto& space : spaces) {
        all_spaces.emplace_back(&space);
        if (!flat) {
            const size_t slash = space.m_path.find_last_of('/');
            if (slash != std::string::npos) {
                std::string folder = space.m_path.substr(0, slash);
                spaces_by_folders[folder].emplace_back(&space);
            } else {
                spaces_by_folders[{}].emplace_back(&space);
            }
        }
    }

    auto print_spaces = [&print](
        std::ostream& os, const auto& spaces, const std::string& suffix
    ) {
        size_t max_len = 0;
        switch (auto N = spaces.size()) {
        case  0: os << "No dataspaces" << std::endl; break;
        case  1: os << "1 dataspace" << suffix << ":\n"; print(*spaces[0], 0); break;
        default: os << thou(N) << " dataspaces" << suffix << ":\n";
            for (const auto& p_space : spaces) {
                const auto& p = p_space->m_path;
                max_len = std::max<>(max_len, p.empty()? dflt.size(): p.size());
            }
            for (const auto& p_space : spaces) {
                print(*p_space, max_len);
            }
        }
    };

    if (flat) {
        print_spaces(ctx.out(), all_spaces, "");
    } else {
        const etpc::data12::Object::Dataspace* dflt_space = nullptr;
        for (const auto& entry : spaces_by_folders) {
            if (!entry.first.empty()) {
                print_spaces(ctx.out() << '\n', entry.second, ", in folder " + entry.first);
            } else {
                assert(entry.second.size() == 1);
                dflt_space = entry.second[0];
            }
        }

        if (dflt_space) {
            const auto& space = *dflt_space;
            OES_WARNING("\nDefault dataspace (no folder):");
            print(space, 0);
        }

        const size_t n = spaces.size();
        const size_t m = spaces_by_folders.size() - (dflt_space? 1: 0);
        OES_WARNING(
            "{} dataspace{}, in {} folder{}.",
            thou(n), (n > 1)? "s": "", thou(m), (m > 1)? "s": ""
        );
    }

    if (spaces.size() != total) {
        const size_t n = (total - spaces.size());
        OES_WARNING(
            "\n(Filtered-out {} dataspace{}, out of {})",
            thou(n), (n > 1)? "s": "", thou(total)
        );
    }

    return true;
}

void pushResource(
    oes::core::JSONWriter& json,
    const etp::Datatypes::Object::Resource& res
) {
    json.push_object();
    json
        .key("uri").string(res.m_uri)
        .key("name").string(res.m_name)
        .key("lastChanged").number(res.m_lastChanged)
        .key("lastChanged-ISO").string(
            printMicroSecDateTime(res.m_lastChanged, true, true, false)
        )
    ;

    if (res.m_storeLastWrite) {
        json
            .key("storeLastWrite").number(res.m_storeLastWrite)
            .key("lastChanged-ISO").string(
                printMicroSecDateTime(res.m_storeLastWrite, true, true, false)
            )
        ;
    }

    if (!res.m_alternateUris.empty()) {
        json
            .key("alternateUris")
            .value(res.m_alternateUris)
        ;
    }

    if (res.m_sourceCount) {
        json.key("sourceCount").number(*res.m_sourceCount);
    }

    if (res.m_targetCount) {
        json.key("targetCount").number(*res.m_targetCount);
    }

    if (!res.m_customData.empty()) {
        json.key("customData")
            .push_object();
        for (const auto& entry : res.m_customData) {
            etp::dataValueAsJson(json, false, entry.first, entry.second);
        }
        json.pop_object();
    }

    json.pop_object();
}

/*!
 * Get the ETP dataspace URI for a given path or UID.
* \param client ETP 1.2 client, already connected and authenticated with the server
* \param path_or_uid the path or UID of the dataspace
* \return the URI of the dataspace, or an empty string if not found
*/
std::string getDataspaceUri(EtpClient12& client, const std::string& path_or_uid){

    const EmlUri space_uri = EmlUri::make_space(path_or_uid);
    const std::string space_uri_str = space_uri.to_string();
    std::vector<oes::common::etp12::Datatypes::Object::Dataspace> spaces;
    auto* osdu_proto = client.space_proto_osdu();
    if(osdu_proto){
        // Try to get the dataspace from the OSDU protocol using the path in URI
        auto osdu_get_rpc = rpcFor(osdu_proto, &etpc::proto12::IDataspaceOSDU::GetDataspaceInfo);
        osdu_get_rpc.req_.m_uris.insert(std::pair<std::string, std::string>(path_or_uid, space_uri_str));
        std::ostringstream oss2;
        if (!osdu_get_rpc.callForEachValue(
            oss2, client.put_get_timeout_,
            [&](auto& ds) {
                if(ds.second.m_path == path_or_uid || ds.second.m_uri == space_uri_str){
                    spaces.emplace_back(ds.second);
                }
            }
        )) {
            throw ActionFailed();
        }
        if(spaces.size() == 1){
            return spaces[0].m_uri;
        }
    }
    
    auto* proto = client.space_proto();
    if (!proto) {
        return std::string();
    }

    auto get_rpc = rpcFor(proto, &etpc::proto12::IDataspace::GetDataspaces);

    std::ostringstream oss2;
    if (!get_rpc.callForEachValue(
        oss2, client.put_get_timeout_,
        [&](auto& ds) {
            if(ds.m_path == path_or_uid || ds.m_uri == space_uri_str){
                spaces.emplace_back(ds);
            }
        }
    )) {
        return std::string();
    }

    if(spaces.size() == 1){
        return spaces[0].m_uri;
    }

    OES_ERROR("EPC Import failed: ETP Dataspace not found: {}", path_or_uid);
    return std::string();
}

bool space_toc(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    const bool utc = ctx.hasArg("utc");
    const bool iso = ctx.hasArg("iso");
    const bool ovr = ctx.hasArg("overwrite");
    const std::string out = ctx.stringArg(ctx.action()); // filename

    std::ofstream fd;
    bool do_json = false;
    oes::core::JSONWriter json(fd);

    if (!out.empty()) {
        if (!tryRemovefile(out, ovr)) {
            return false;
        }

        fd.open(out, std::ios::out);
        if (!fd.good()) {
            OES_ERROR("Failed to open file: {}", out);
            return false;
        }

        fd << std::setprecision(std::numeric_limits<double>::digits10 + 1);
        do_json = true;
    }

    const std::string since = ctx.stringArg("since");
    const int64_t lastWrittenFilter = parseSince(since, utc);
    if (lastWrittenFilter < 0) {
        OES_ERROR(
            "Invalid --since value: {}\n\tUse an offset: 5min, 1d, 1w; or an ISO-8601 date-time (see --iso)"
            , since);
        return false;
    } else if (lastWrittenFilter != 0) {
        using namespace std::chrono;
        microseconds usec(lastWrittenFilter); // UTC
        system_clock::time_point ts(usec);
        OES_VERBOSE("Resources since {} UTC", oes::core::formatIsoDateTime(ts));
    }

    const auto [space_path, space_ok] = getSpacePath(ctx);
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDiscovery,
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDataspaceOSDU,
    });
    if (!clt.connect()) {
        return false; // error already issued
    }

    auto* proto = clt.disco_proto();
    if (!proto) {
        return false;
    }

    const std::string space_uri_txt = getDataspaceUri(clt, space_path);
    const EmlUri space_uri = EmlUri::parse(space_uri_txt);
    const std::string types_csv = ctx.stringArg("types");
    std::vector<std::string> types;
    if (!types_csv.empty()) {
        boost::split(types, types_csv, boost::algorithm::is_any_of("\t ,;|"));
        for (auto& type : types) {
            std::vector<std::string> parts;
            boost::split(parts, type, boost::algorithm::is_any_of("."));
            switch (parts.size()) {
            case  0: OES_ERROR("Invalid type: {}", type); return false;
            case  1: type = "resqml20." + type; break;
            case  2: break; // use as-is
            default: OES_ERROR("Invalid type: {}", type); return false;
            }
            OES_INFO("Filtering on type: {}", type);
        }
    } else {
        OES_INFO("No type-filtering: Getting all types.");
    }

    auto rpc = rpcFor(proto, &etpc::proto12::IDiscovery::GetResources);
    rpc.req_.m_context.m_uri = space_uri.to_string();
    rpc.req_.m_context.m_depth = 1; // required by PV's server... See iLab#3
    rpc.req_.m_context.m_dataObjectTypes = types;
    if (lastWrittenFilter) {
        rpc.req_.m_storeLastWriteFilter = lastWrittenFilter;
    }
    OES_INFO("Discovering {}...",rpc.req_.m_context.m_uri);

    if (do_json) {
        json.push_object();
        json
            .key("dataspace").string(space_uri.dataspace_)
            .key("resources").push_array();
    }

    size_t json_resources = 0;
    decltype(rpc.res_.m_resources) all_resources;

    // For now, in case of multi-part response,
    // we simply accumulate the multi-part responses,
    // instead of having a proper concurrent pipeline.
    // TODO(unknown): process "chunks" in separate thread,
    //       feeding each chunk via moodycamel's blocking-RWQueue,
    //       and using ProcTopN-like collection to discard chunks,
    //       avoiding large memory use (e.g. 460MB for KernRiver).
    if (!rpc.callForEachValue(
        ctx.err(), clt.put_get_timeout_,
        [&](auto& res) {
            if (do_json) {
                ++json_resources;
                pushResource(json, res);
            } else {
                all_resources.emplace_back(std::move(res));
            }
        }
    )) {
        return false;
    }

    if (do_json) {
        json
            .pop_array()
            .pop_object();
        fd.close();
    }

    auto& rscs = all_resources;
    const size_t res_count = std::max<>(rscs.size(), json_resources);
    if (res_count == 0) {
        if (types.empty() && !lastWrittenFilter) {
            OES_WARNING("Empty dataspace.");
        } else {
            OES_WARNING("No resources matching filter(s).");
        }
        return true;
    }

    if (do_json) {
        OES_WARNING("Wrote {} resource{} to JSON file {}",
            res_count, (res_count == 1)? "": "s", out
        );
        return true;
    } else {
        OES_WARNING("Got {} resource{}.",
            res_count, (res_count == 1)? "": "s"
        );
    }

    std::sort(rscs.begin(), rscs.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.m_uri < rhs.m_uri;
    });

    const size_t topN = std::min<>(rscs.size(), size_t(ctx.args["top"].as<int>()));
    const size_t skipN = std::max<>(0, ctx.args["skip"].as<int>()); // 0 by default

    // Gather the range of dates for all resources
    size_t lmod_count = 0;
    size_t slwr_count = 0;
    size_t lmod0_count = 0;
    size_t slwr0_count = 0;
    int64_t lmod_min = std::numeric_limits<int64_t>::max();
    int64_t lmod_max = 0;
    int64_t slwr_min = std::numeric_limits<int64_t>::max();
    int64_t slwr_max = 0;

    for (const auto& rsc : rscs) {
        if (0 == rsc.m_lastChanged) {
            ++lmod0_count;
        } else if (!lmod_count) {
            ++lmod_count;
            lmod_min = lmod_max = rsc.m_lastChanged;
        } else {
            ++lmod_count;
            lmod_min = std::min<>(lmod_min, rsc.m_lastChanged);
            lmod_max = std::max<>(lmod_max, rsc.m_lastChanged);
        }

        if (0 == rsc.m_storeLastWrite) {
            ++slwr0_count;
        } else if (!slwr_count) {
            ++slwr_count;
            slwr_min = slwr_max = rsc.m_storeLastWrite;
        } else {
            ++slwr_count;
            slwr_min = std::min<>(slwr_min, rsc.m_storeLastWrite);
            slwr_max = std::max<>(slwr_max, rsc.m_storeLastWrite);
        }
    }

    for (size_t i = 0; i < topN; ++i) {
        const auto& rsc = rscs[skipN+i];
        EmlUri uri = EmlUri::parse(rsc.m_uri);
        OES_ASSERT(uri.type_ == EmlUri::eOBJ_INSTANCE, continue;);
        std::string lmod = printMicroSecDateTime(rsc.m_lastChanged, utc, iso);
        std::string slwr = printMicroSecDateTime(rsc.m_storeLastWrite, utc, iso);
        OES_WARNING(R"(
               URI: {}
    Alternate-URIs: {})",
            rsc.m_uri, rsc.m_alternateUris.size()
        );
        OES_WARNING(R"(
              Name: {}
              Type: {}
              Guid: {}
     Last-Modified: {}
  Store-Last-Write: {}
       Custom-Data: {} entr{}
)",
            rsc.m_name, uri.to_type(), uri.guid_.toXmlString(),
            lmod, slwr, rsc.m_customData.size(),
            (rsc.m_customData.size() == 1)? "y": "ies"
        );
    }

    int width = static_cast<int>(std::log10(rscs.size()));
    width += (width / 3); // for commas
    width += 1;
    const std::string spacer(width + 30, ' ');

    if (topN+skipN < rscs.size()) {
        OES_WARNING("({:>{}} remaining. Increase --top or use --skip to see more)",
            thou(rscs.size() - topN -skipN), width
        );
    }

    if (lmod0_count) {
        OES_WARNING("({:>{}} resource{} no Last-Modified timestamp)",
            thou(lmod0_count), width, (lmod0_count == 1)? " has": "s have"
        );
    }
    if (slwr0_count) {
        OES_WARNING("({:>{}} resource{} no Store-Last-Write timestamp)",
            thou(slwr0_count), width, (slwr0_count == 1)? " has": "s have"
        );
    }

    if (lmod_min == lmod_max) {
        OES_WARNING("({:>{}} resource{} Last-Modified on {})",
            thou(lmod_count), width, (lmod_count == 1)? "": "s",
            printMicroSecDateTime(lmod_min, utc, iso)
        );
    } else if (lmod_count) {
        OES_WARNING("({:>{}} resource{} Last-Modified between {}\n{}and {})",
            thou(lmod_count), width, (lmod_count == 1)? "": "s",
            printMicroSecDateTime(lmod_min, utc, iso), spacer,
            printMicroSecDateTime(lmod_max, utc, iso)
        );
    }

    if (slwr_min == slwr_max) {
        OES_WARNING("({:>{}} resource{} Store-Last-Write on {})",
            thou(slwr_count), width, (slwr_count == 1)? "": "s",
            printMicroSecDateTime(slwr_min, utc, iso)
        );
    } else if (slwr_count) {
        OES_WARNING("({:>{}} resource{} Store-Last-Write between {}\n{}and {})",
            thou(slwr_count), width, (slwr_count == 1)? "": "s",
            printMicroSecDateTime(slwr_min, utc, iso), spacer,
            printMicroSecDateTime(slwr_max, utc, iso)
        );
    }

    return true;
}

bool space_stats_via_discovery(
    const EmlAppContext& ctx, EtpClient12& clt,
    etpc::proto12::IDiscovery& proto, const std::string& space_path
) {
    const std::string space_uri_txt = getDataspaceUri(clt, space_path);
    const EmlUri space_uri = EmlUri::parse(space_uri_txt);

    auto rpc = rpcFor(&proto, &etpc::proto12::IDiscovery::GetResources);

    rpc.req_.m_context.m_uri = space_uri.to_string();
    rpc.req_.m_context.m_depth = 1; // required by PV's server... See iLab#3
    rpc.req_.m_context.m_dataObjectTypes.clear(); // i.e. no type filtering
    rpc.req_.m_storeLastWriteFilter.reset(); // i.e. no time filtering
    OES_INFO("Discovering {}...", rpc.req_.m_context.m_uri);

    decltype(rpc.res_.m_resources) rscs;
    if (!rpc.callForEachValue(
        ctx.err(), clt.put_get_timeout_,
        [&rscs](auto& res) {
            rscs.emplace_back(std::move(res));
        }
    )) {
        return false;
    }

    if (rscs.empty()) {
        OES_WARNING("No ETP types found");
        return true;
    }

    std::map<std::string, int32_t> types;
    for (const auto& rsc : rscs) {
        EmlUri uri = EmlUri::parse(rsc.m_uri);
        ++types[uri.to_type()];
    }

    for (const auto& entry : types) {
        const std::string& name = entry.first;
        int32_t count = entry.second;
        OES_WARNING("{:>18} {}", thou(count), name);
    }
    OES_WARNING("{:>18} {}", thou(rscs.size()), "TOTAL");
    return true;
}

struct AryTypeInfo {
    common12::AryType type_;
    int elem_size_;
    std::string_view label_;
};

auto typeOf(common12::AryType type) -> const AryTypeInfo* {
    using namespace std::string_view_literals;
    static constexpr AryTypeInfo ary_types[] {
        { common12::AryType::eArrayOfBoolean, 1, "Boolean"sv },
        { common12::AryType::eArrayOfInt    , 4, "Int"sv },
        { common12::AryType::eArrayOfLong   , 8, "Long"sv },
        { common12::AryType::eArrayOfFloat  , 4, "Float"sv },
        { common12::AryType::eArrayOfDouble , 8, "Double"sv },
        { common12::AryType::eArrayOfString , 0, "String"sv }, // variable size
        { common12::AryType::eBytes         , 1, "Byte"sv },
    };
    // Do linear search, in case the enum evolves behind our back
    for (const AryTypeInfo& ary_type : ary_types) {
        if (type == ary_type.type_) {
            return &ary_type;
        }
    }
    return nullptr;
}

bool space_stats(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    const auto [space_path, space_ok] = getSpacePath(ctx);
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDiscovery,
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDataspaceOSDU,
        etpc::data12::Protocol::eSupportedTypes
    });
    if (!clt.connect()) {
        return false; // error already issued
    }

    // If SupportedTypes proto not supported, do it via Discovery instead.
    // Can be tested via `--shun-protocols types` for example.
    auto* proto = clt.types_proto();
    if (!proto) {
        auto* proto2 = clt.disco_proto();
        if (proto2) {
            OES_WARNING("No SupportedTypes(25) support: "
                "Using slower Discovery(3) instead");
            return space_stats_via_discovery(ctx, clt, *proto2, space_path);
        }
        OES_ERROR("Server supports neither SupportedTypes(25) nor Discovery(3) protocols");
        return false;
    }

    const std::string space_uri_txt = getDataspaceUri(clt, space_path);
    const EmlUri uri = EmlUri::parse(space_uri_txt);

    auto rpc = rpcFor(proto, &etpc::proto12::ISupportedTypes::GetSupportedTypes);

    rpc.req_.m_uri = uri.to_string(); // init request
    rpc.req_.m_scope = etpc::data12::Object::ContextScopeKind::eSelf; // ignored
    rpc.req_.m_countObjects = true;
    rpc.req_.m_returnEmptyTypes = false;

    auto ok = rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!ok.first) {
        return false;
    }

    auto& types = rpc.res_.m_supportedTypes;
    if (types.empty()) {
        OES_WARNING("No ETP types found");
        return true;
    }

    // sort, for deterministic order
    std::sort(types.begin(), types.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.m_dataObjectType < rhs.m_dataObjectType;
    });

    int64_t total = 0;
    for (const auto& entry : types) {
        const std::string& name = entry.m_dataObjectType;
        int32_t count = !entry.m_objectCount? 0: *entry.m_objectCount;
        if (count > 0) {
            total += count;
        }
        OES_WARNING("{:>18} {}", thou(count), name);
    }
    OES_WARNING("{:>18} {}", thou(total), "TOTAL");
    return true;
}

bool space_array_stats(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    const auto [space_path, space_ok] = getSpacePath(ctx);
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataArray
    });
    if (!clt.connect()) {
        return false; // error already issued
    }

    auto* proto = clt.array_proto();
    if (!proto) {
        return false;
    }

    auto rpc = rpcFor(proto, &etpc::proto12::IDataArray::GetDataArrayMetadata);
    rpc.req_ = etp::DataArrayExt::to_list_arrays_req(space_path);

    auto ok = rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!ok.first) {
        return false;
    }

    auto arrays = etp::DataArrayExt::move_to_info_vec(rpc.res_.m_arrayMetadata);
    if (arrays.empty()) {
        OES_WARNING("No ETP arrays found");
        return true;
    }

    const size_t topN = std::max<>(1, ctx.args["top"].as<int>()); // 10 by default
    std::multimap<size_t, const common12::AryInfo*> topMap;

    std::vector<size_t> bytes_histogram_;
    bytes_histogram_.reserve(32);

    size_t errors = 0;
    std::vector<size_t> countByRank(8);
    std::map<etp::Datatypes::AnyArrayType, size_t> countByType;
    std::map<std::string, size_t> countByObjType;
    std::map<std::string, size_t> bytesByObjType;
    for (const auto& entry : arrays) {
        EmlUri uri = EmlUri::parse(entry.ident_.m_uri);
        if (uri.type_ != EmlUri::eOBJ_INSTANCE) {
            ++errors;
            continue;
        }
        const size_t rank = entry.meta_.m_dimensions.size();
        if (!(0 < rank && rank < countByRank.size())) {
            ++errors;
            continue;
        }

        etp::Datatypes::AnyArrayType type = entry.meta_.m_transportArrayType;
        if (!(common12::AryType::eArrayOfBoolean <= type && type <= common12::AryType::eBytes)) {
            ++errors;
            continue;
        }

        const AryTypeInfo* tinfo = typeOf(type);
        const int elem_bytes = tinfo? tinfo->elem_size_ : -1;
        if (elem_bytes < 0) {
            ++errors;
            continue;
        }
        size_t bytes = elem_bytes;
        if (bytes >= 1) {
            for (int64_t dim : entry.meta_.m_dimensions) {
                bytes *= dim;
            }
        }

        ++countByRank[rank];
        ++countByType[type];
        ++countByObjType[uri.obj_type_];

        if (bytes == 0) {
            // e.g. 2D point array with 0x3 dimensions.
            // Not sure that's normal though...
            if (bytes_histogram_.empty()) {
                bytes_histogram_.emplace_back(1);
            } else {
                ++bytes_histogram_[0];
            }
            continue;
        }

        bytesByObjType[uri.obj_type_] += bytes;

        if (topMap.size() < topN) {
            topMap.emplace(bytes, &entry);
        } else if (topMap.begin()->first < bytes) {
            topMap.erase(topMap.begin());
            topMap.emplace(bytes, &entry);
        }

        const size_t bucket = 1 + static_cast<size_t>(std::log2(bytes));
        if (bucket >= bytes_histogram_.size()) {
            bytes_histogram_.resize(bucket + 1);
        }
        ++bytes_histogram_[bucket];
    }

    OES_WARNING("{:>18} by rank >>>", "<<< Arrays");
    for (size_t rank = 0; rank < countByRank.size(); ++rank) {
        if (size_t n = countByRank[rank]) {
            OES_WARNING("{:>18} {}D", thou(n), rank);
        }
    }

    OES_WARNING("\n{:>18} by element type >>>", "<<< Arrays");
    for (const auto& entry : countByType) {
        if (const AryTypeInfo* tinfo = typeOf(entry.first)) {
            if (size_t n = entry.second) {
                OES_WARNING("{:>18} {}", thou(n), tinfo->label_);
            }
        }
    }

    OES_WARNING("\n{:>18} by bytes-range >>>", "<<< Arrays");
    if (!bytes_histogram_.empty() && bytes_histogram_[0]) {
        OES_WARNING("{:>18} empty", thou(bytes_histogram_[0]));
    }
    for (size_t i = 1; i < bytes_histogram_.size(); ++i) {
        const size_t width = thou(static_cast<size_t>(pow(2, bytes_histogram_.size() - 1))).size();
        const size_t count = bytes_histogram_[i];
        OES_WARNING(
            "{:>18} ({:>{}} - {:>{}}] bytes", thou(count),
            thou(size_t(pow(2, i - 1))), width,
            thou(size_t(pow(2, i))), width
        );
    }

    int width = static_cast<int>(std::log10(arrays.size()));
    width += (width / 3); // for commas
    width += 1;

    size_t total_bytes = 0;
    OES_WARNING("\n{:>18} by object type >>>", "<<< Arrays");
    for (const auto& count_entry : countByObjType) {
        const std::string& obj_type = count_entry.first;
        const size_t count = count_entry.second;
        const size_t bytes = bytesByObjType[obj_type];
        OES_WARNING("{:>18} {:>{}}x {}", thou(bytes), thou(count), width, obj_type);
        total_bytes += bytes;
    }
    OES_WARNING("{:>18} {:>{}}x TOTAL", thou(total_bytes), thou(arrays.size()), width);

    OES_WARNING("\n{:>18} bytes - Top {} >>>", "<<< Arrays", topN);
    for (const auto& entry : oes::core::reverse_of(topMap)) {
        const size_t bytes = entry.first;
        const common12::AryInfo& ary = *entry.second;
        EmlUri uri = EmlUri::parse(ary.ident_.m_uri);
        const AryTypeInfo* tinfo = typeOf(ary.meta_.m_transportArrayType);
        const auto& elem_type = tinfo? tinfo->label_: "???";
        const size_t rank = ary.meta_.m_dimensions.size();
        std::string dims = thou(ary.meta_.m_dimensions[0]);
        for (size_t i = 1; i < rank; ++i) {
            dims += " x ";
            dims += thou(ary.meta_.m_dimensions[i]);
        }

        OES_WARNING(
            "{:>18} {}({})\n{:>18} {}\n{:>18} {}D - {} - {}",
            thou(bytes), uri.obj_type_, uri.guid_.toXmlString(),
            "", ary.ident_.m_pathInResource,
            "", rank, dims, elem_type
        );
    }

    return true;
}

bool space_delete(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    const auto [space_path, space_ok] = getSpacePath(ctx, true, false);
    if (!space_ok) {
        OES_ERROR("Invalid dataspace path: {}", space_path);
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDataspaceOSDU
    });
    if (!clt.connect()) {
        OES_ERROR("Failed to connect to server: {}", ctx.stringArg("server-url"));
        return false;
    }

    auto* proto = clt.space_proto();
    if (!proto) {
        OES_ERROR("ETP Dataspace protocol not supported");
        return false;
    }

    // DeleteDS needs uids, while users/humans know about paths.
    // So must look for dataspaces to find the uids.
    std::string space_uri = getDataspaceUri(clt, space_path);
    if (space_uri.empty()) {
        OES_ERROR("ETP Dataspace not found: {}", space_path);
        return false;
    }

    auto del_rpc = rpcFor(proto, &etpc::proto12::IDataspace::DeleteDataspaces);
    del_rpc.req_.m_uris.emplace(space_path, space_uri);

    auto del_ok = del_rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!del_ok.first) {
        OES_ERROR("Failed to delete ETP dataspace: {}", space_path);
        if (del_ok.second == std::future_status::timeout) {
            OES_ERROR("Error: Operation timed out");
        } else if (del_ok.second == std::future_status::deferred) {
            OES_ERROR("Error: Operation deferred");
        } else {
            OES_ERROR("Unknown error");
        }
        return false;
    }
    return true;
}

bool space_delete_all(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace
    });
    if (!clt.connect()) {
        return false;
    }

    size_t total = 0;
    auto spaces = clt.globSpaces(ctx.stringArg(ctx.action()), &total);
    if (spaces.empty()) {
        OES_WARNING("Nothing to delete");
        return true;
    }

    auto* proto = clt.space_proto();
    assert(proto); // globSpaces() throws on no DS proto

    auto del_rpc = rpcFor(proto, &etpc::proto12::IDataspace::DeleteDataspaces);
    for (const auto& space : spaces) {
        del_rpc.req_.m_uris.emplace(space.m_uri, space.m_uri);
    }

    if (del_rpc.req_.m_uris.empty()) {
        OES_WARNING("Nothing to delete");
        return true;
    }

    auto del_ok = del_rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!del_ok.first) {
        return false;
    }

    const size_t n = spaces.size();
    OES_WARNING(
        "Deleted {} ETP dataspace{}, out of {}",
        thou(n), (n > 1)? "s": "", thou(total)
    );
    return true;
}

/**
 * @brief Locks a dataspace.
 * 
 * @param ctx command line context
 * @return success
*/
bool space_lock(const EmlAppContext& ctx) {
    const auto [space_path, space_ok] = getSpacePath(ctx, true, false);
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDataspaceOSDU
    });
    if (!clt.connect()) {
        return false;
    }

    auto* proto = clt.space_proto_osdu();
    if (!proto) {
        return false;
    }

    const std::string space_uri_txt = getDataspaceUri(clt, space_path);
    const EmlUri uri = EmlUri::parse(space_uri_txt);

    std::map<std::string, std::string> uris;
    uris.emplace(space_path,uri.to_string());
    auto lock_rpc = rpcFor(proto, &etpc::proto12::IDataspaceOSDU::LockDataspaces);
    lock_rpc.req_.m_uris = uris;
    lock_rpc.req_.m_lock = true;

    auto lock_ok = lock_rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!lock_ok.first) {
        return false;
    }

    return true;
}

/**
 * @brief Unlocks a dataspace.
 * 
 * @param ctx command line context
 * @return success
*/
bool space_unlock(const EmlAppContext& ctx) {
    const auto [space_path, space_ok] = getSpacePath(ctx, true, false);
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDataspaceOSDU
    });
    if (!clt.connect()) {
        return false;
    }

    auto* proto = clt.space_proto_osdu();
    if (!proto) {
        return false;
    }

    const std::string space_uri_txt = getDataspaceUri(clt, space_path);
    const EmlUri uri = EmlUri::parse(space_uri_txt);

    std::map<std::string, std::string> uris;
    uris.emplace(space_path,uri.to_string());
    auto lock_rpc = rpcFor(proto, &etpc::proto12::IDataspaceOSDU::LockDataspaces);
    lock_rpc.req_.m_uris = uris;
    lock_rpc.req_.m_lock = false;

    auto lock_ok = lock_rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!lock_ok.first) {
        return false;
    }

    return true;
}

//============================================================================
unsigned short num_scoped_transaction_retries(const EmlAppContext& ctx) {
    int n_transaction_retries { -1 };
    std::string tr_str;
    if (ctx.hasArg("transaction-retries")) {
        n_transaction_retries = ctx.args["transaction-retries"].as<int>();
    }
    try {
        if (n_transaction_retries < 0 ) {
            tr_str = ctx.appConfig().transactionRetries();
            n_transaction_retries = std::stoi(tr_str);
        }
        if (n_transaction_retries < 0 ) {
            throw std::invalid_argument("Invalid arg");
        }
    } catch (std::invalid_argument& /*e*/) {
        OES_DEBUG("Invalid argument \"transaction-retries\". Value should be greater than 0.");
        n_transaction_retries = 0;
        OES_DEBUG("Scoped Transaction will not retry start.");
    }

    return static_cast<unsigned short>(n_transaction_retries);
}

//============================================================================

bool space_import_epc(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));
    // Note: Prolog mostly the same as in openkv_import_epc()
    const std::string epc = ctx.stringArg(ctx.action());
    if (epc.empty()) {
        OES_ERROR("Missing required argument to --{}", ctx.action());
        return false;
    }

    fs::path epc_path(epc);
    fs::file_status epc_stat = fs::status(epc_path);
    if (!fs::is_regular_file(epc_stat)) {
        OES_ERROR("EPC file not found: {}", epc);
        return false;
    }

    // space-path should be of the form folder/name, where
    // folder shouldn't contain any back-slashes, and at most a single slash,
    // If name is omitted, it comes from the EPC filename.
    // If --space-path completely omitted, uses default dataspace.
    auto [space_path, space_ok] = getSpacePath(ctx);
    if (!space_ok) {
        return false;
    }

    // Probably close to what ProtocolCapabilityKind::eMaxDataItemCount should be
    // (max_batch_size == 0) disabled by-count batching (remains by-bytes batching)
    int max_batch_size = ctx.args["max-batch-size"].as<int>();
    if (!(0 <= max_batch_size && max_batch_size < 10*1000)) {
        OES_ERROR("Invalid --max-batch-size: Not in [0 - 10,000] range");
        return false;
    }

    // reject duplicates
    // In true case the open-etp-server will not import already existing objects AND
    // Will fail if any conflict of existing one:
    // Example: a xml that is already in db has guid_x and date_x
    //          if we try to import one with guid_x and date_y then the upload is failed!
    const bool reject_duplicates = ctx.hasArg("reject-duplicates");

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eStore,
        etpc::data12::Protocol::eDataArray,
        etpc::data12::Protocol::eTransaction,
        etpc::data12::Protocol::eDiscovery,
        etpc::data12::Protocol::eDataspaceOSDU
    });
    if (!clt.connect()) {
        return false;
    }

    oes::client::ImportEpcOptions options;

    options.timeout_ = clt.put_get_timeout_;
    options.max_batch_size_ = static_cast<size_t>(max_batch_size);
    options.reject_duplicates_ = reject_duplicates;
    options.out_  = &ctx.out();
    options.msg_  = &ctx.msg();
    options.warn_ = &ctx.out();
    options.err_  = &ctx.err();
    options.n_transaction_retries_ = num_scoped_transaction_retries(ctx);
    options.reconnect_retries = ctx.args["reconnect-retries"].as<int>();

    if (ctx.hasArg("transaction-limit-time")) {
        std::string time_spec = ctx.stringArg("transaction-limit-time");
        if (time_spec != "inf") {
            auto time_s = std::chrono::duration_cast<std::chrono::seconds>(oes::core::parseSimpleDuration(time_spec));
            options.transaction_limit_sec = static_cast<double>(time_s.count());
            if (options.transaction_limit_sec > -1 && options.transaction_limit_sec < 1) {
                OES_ERROR("transaction-limit-time should be at least 1 second.");
                return false;
            }
        }
    }
    if (ctx.hasArg("transaction-limit-size")) {
        std::string lim_size = ctx.stringArg("transaction-limit-size");
        if (!lim_size.empty() && lim_size != "inf") {
            options.transaction_limit_bytes = oes::core::parseByteSize(lim_size);
            if (options.transaction_limit_bytes < 1'000'000 /*1 MB*/) {
                OES_ERROR("transaction-limit-size less than 1 mb");
                return false;
            }
        }
    }

    auto result = oes::client::importEpc(
        clt.cfg_, space_path, epc,
        options
    );

    if(result) {
        OES_INFO("Final progress: 100%");
    }else{
        OES_INFO("Failure");
    }

    return result;
}

//============================================================================

bool space_import_space(const EmlAppContext& ctx) {
    // Note: Prolog mostly the same as in openkv_import_epc()
    const std::string origin = ctx.stringArg(ctx.action());
    if ( origin.empty() ) {
        OES_ERROR("Missing required argument to --{}", ctx.action());
        return false;
    }

    const auto [space_path, space_ok] = getSpacePath(ctx, true, false);
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDataspaceOSDU
    });
    if (!clt.connect()) {
        return false;
    }

    auto* proto = clt.space_proto_osdu();
    if (!proto) {
        return false;
    }

    const std::string origin_uri_txt = getDataspaceUri(clt, origin);
    const EmlUri uri = EmlUri::parse(origin_uri_txt);

    std::map<std::string, std::string> uris;
    uris.emplace(space_path,uri.to_string());
    auto dc_rpc = rpcFor(proto, &etpc::proto12::IDataspaceOSDU::CopyDataspacesContent);
    dc_rpc.req_.m_targetDataspace = getDataspaceUri(clt, space_path);
    dc_rpc.req_.m_dataspaces = uris;

    dc_rpc.call(ctx.err(), clt.put_get_timeout_);
    return true;
}

//============================================================================

bool space_export_epc(const EmlAppContext& ctx) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("Client.") + __func__,
                    {},
                    ctx.stringArg("server-url"));

    const auto [space_path, space_ok] = getSpacePath(ctx);
    if (!space_ok) {
        return false;
    }

    std::string epc = ctx.stringArg(ctx.action());
    const bool epc_was_empty = epc.empty();
    if (epc_was_empty) {
        // Infer the filename from the space_path
        EmlUri uri = EmlUri::make_space(space_path);
        std::string folder = uri.dataspaceFolder();
        if (folder.empty()) {
            epc = fmt::format("{}.epc", scrubFilename(uri.dataspace_));
        } else {
            epc = fmt::format("{}@{}.epc", scrubFilename(uri.dataspaceName()), scrubFilename(folder));
        }
    }
    fs::path epc_path(epc);
    if (!epc_path.has_extension()) {
        epc_path.replace_extension(".epc");
    }
    if (epc_was_empty) {
        epc_path = epc_path.filename(); // strip leading folder
    }
    epc = epc_path.string();

    // check file doesn't exist or can be overwritten, before too much I/O
    const bool overwrite = ctx.hasArg("overwrite");
    if (!tryRemovefile(epc, overwrite)) {
        return false;
    }

    // Try to create the directory tree to export the file to.
    const fs::path dest_dir(absolute(epc_path).parent_path());
    try {
        // Can throw std::bad_alloc or filesystem::filesystem_error
        fs::create_directories(dest_dir);
    } catch(const std::exception&) {
        OES_ERROR("Cannot create folder folder {}", dest_dir.string());
        if (!ctx.hasArg("force")) { // allow bypassing that check, in case it's buggy...
            return false; // bail out
        }
    }

    // Check the folder is writable too, in case user opened DOS Console
    // in UAC-protected C:\Program Files subfolder, and forgot to CD elsewhere.
    // Both HDF5 and ZIP fail horribly in that case, with tons of errors.
    const bool can_write = canWriteFileInFolder(epc);
    if (!can_write) {
        namespace fs = fs;
        OES_ERROR("Cannot write to folder {}", dest_dir.string());
        if (!ctx.hasArg("force")) { // allow bypassing that check, in case it's buggy...
            return false; // bail out
        }
    }

    fs::path hdf_path(epc_path);
    hdf_path.replace_extension(".h5");
    const std::string hdf = hdf_path.string();

    // Probably close to what ProtocolCapabilityKind::eMaxDataItemCount should be
    // (max_batch_size == 0) disabled by-count batching (remains by-bytes batching)
    const int max_batch_size = ctx.args["max-batch-size"].as<int>();
    if (!(0 <= max_batch_size && max_batch_size <= 10*1000)) {
        OES_ERROR("Invalid --max-batch-size: Not in [0 - 10,000] range");
        return false;
    }

    // Peak RAM will be proportional to (constant + (max_queue_depth * max_msg_size)),
    // which by default would be (constant + (10 * 1MB)). Given that the producer side
    // of the queue (DataArray.Get messages) are much slower than the consumer side
    // (HDF5 write), the queue is in fact unlikely to get full, and the consumer side
    // will be "stalled" most of the time, waiting for new entries to be enqueued.
    const int max_queue_depth = ctx.args["array-queue-depth"].as<int>();
    if (!(1 <= max_queue_depth && max_queue_depth <= 1000)) {
        OES_ERROR("Invalid --array-queue-depth: Not in [1 - 1,000] range");
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eTransaction,
        etpc::data12::Protocol::eDiscovery,
        etpc::data12::Protocol::eStore,
        etpc::data12::Protocol::eDataArray,
        etpc::data12::Protocol::eDataspaceOSDU
    });
    if (!clt.connect()) {
        return false; // error already issued
    }

    oes::client::ExportEpcOptions options;

    options.timeout_ = clt.put_get_timeout_;
    options.max_batch_size_ = static_cast<size_t>(max_batch_size);
    options.overwrite_ = overwrite;

    options.max_queue_depth_ = max_queue_depth;
    options.hdf5_compression_ = hdf5CompressionLevel(ctx);
    options.hdf5_large_data_compression_ = hdf5LargeDataCompressionLevel(ctx);

    options.out_  = &ctx.out();
    options.msg_  = &ctx.msg();
    options.warn_ = &ctx.out();
    options.err_  = &ctx.err();

    options.n_transaction_retries_ = num_scoped_transaction_retries(ctx);

    const bool multi_threaded = ctx.hasArg("jobs");
    if (!multi_threaded) {
        OES_DEBUG("Using single-threaded, load-all-then-write EPC export.");
        auto result = exportEpc(*clt.clt_, space_path, epc, hdf, options);
        if (options.out_) {
            OES_INFO("Final progress: 100%");
        }
        return result;
    }

    // single-threaded when -j is not explicitly specified
    int64_t jobs = ctx.args["jobs"].as<int64_t>();
    const auto max_jobs = std::thread::hardware_concurrency();
    if (jobs && !(3 <= jobs && jobs <= max_jobs)) {
        OES_ERROR("Invalid --jobs: Not in [3 - {}] range", max_jobs);
        return false;
    }
    options.thread_count_ = static_cast<unsigned int>(jobs);
    OES_DEBUG("Using multi-Threaded, pipelined load-one-and-write EPC export.");
    auto result = exportEpcMt(*clt.clt_, space_path, epc, hdf, options);
    if (options.out_) {
        OES_INFO("Final progress: 100%");
    }
    return result;
}

//============================================================================
bool space_copy_from_remote(const EmlAppContext& ctx) {
    std::string source_space_path, target_space_path;
    bool space_ok = false;

    std::tie(target_space_path, space_ok) = getSpacePath(ctx, true);
    if (!space_ok) {
        return false;
    }
    std::tie(source_space_path, space_ok) = getSpacePath(ctx, true, true, "source-space");
    if (!space_ok) {
        return false;
    }

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspaceOSDU
    });
    if (!clt.connect()) {
        return false;
    }

    auto* spc_osdu_proto = clt.space_proto_osdu();
    if (!spc_osdu_proto) {
        OES_ERROR("Could not aquire osdu proto");
        return false;
    }

    auto cpy_remote_rpc = rpcFor(spc_osdu_proto, &etpc::proto12::IDataspaceOSDU::CopyDataspaceFromRemote);

    cpy_remote_rpc.req_.m_action = "trigger";
    cpy_remote_rpc.req_.m_dataspacePath = target_space_path;

    cpy_remote_rpc.req_.m_sourceServer.m_sourceDataspacePath = source_space_path;
    cpy_remote_rpc.req_.m_sourceServer.m_sourceJwt = ctx.args["source-token"].as<std::string>();
    cpy_remote_rpc.req_.m_sourceServer.m_sourcePartition = ctx.args["source-partition"].as<std::string>();
    cpy_remote_rpc.req_.m_sourceServer.m_sourceServer = ctx.args["source-server"].as<std::string>();
    cpy_remote_rpc.req_.m_extraData.emplace("timeout_ms", std::to_string(clt.put_get_timeout_.count()));

    if (cpy_remote_rpc.req_.m_sourceServer.m_sourceServer.empty()) {
        OES_ERROR("Source server empty");
        return false;
    }
    
    if (cpy_remote_rpc.req_.m_sourceServer.m_sourcePartition.empty()) {
        OES_WARNING("Source partition empty, using \"single\" partition");
        cpy_remote_rpc.req_.m_sourceServer.m_sourcePartition = "single";
    }

    std::string curr_table;
    ushort progress{10}; // assume 9 tables (10% each)
    std::stringstream ss;
    if (!cpy_remote_rpc.callForEachValue(
        ss, clt.put_get_timeout_,
        [&](auto& entry){
            curr_table = entry.m_tableName;
            if (curr_table == "admin") { // admin may not be transfered if it does not have any xdata
                progress = 0; // 10 tables with admin
            }

            progress += 10;
            OES_INFO("Copy dataspace (table {}) progress: {}%", curr_table, progress);
    })) {
        OES_ERROR("Error on dataspace copy");
        return false;
    }

    OES_INFO("Copy dataspace succeeded");
    return true;
}

//============================================================================

bool space_copy_ref(const EmlAppContext& ctx) {
    auto [dest_space_path, space_ok] = getSpacePath(ctx);
    if (!space_ok) {
        return false;
    }

    std::string orig_space_path = ctx.args["source-space"].as<std::string>();

    EtpClient12 clt(ctx);
    clt.cfg_.requested_protocols_.assign({
        etpc::data12::Protocol::eDataspace,
        etpc::data12::Protocol::eDiscovery,
        etpc::data12::Protocol::eDataspaceOSDU,
        etpc::data12::Protocol::eTransaction
    });
    if (!clt.connect()) {
        return false;
    }

    auto* get_proto = clt.space_proto();
    auto* disc_proto = clt.disco_proto();
    auto* spc_osdu_proto = clt.space_proto_osdu();

    if (!get_proto || !disc_proto || !spc_osdu_proto) {
        return false;
    }
    auto get_rpc = rpcFor(get_proto, &etpc::proto12::IDataspace::GetDataspaces);

    oes::common::etp12::Datatypes::Object::Dataspace orig_space;
    oes::common::etp12::Datatypes::Object::Dataspace dest_space;

    if (!get_rpc.callForEachValue(
        ctx.err(), clt.put_get_timeout_,
        [&](auto& ds) {
            if (ds.m_path == orig_space_path) {
                orig_space = ds;
            } else if (ds.m_path == dest_space_path) {
                dest_space = ds;
            }
        }
    )) {
        throw ActionFailed();
    }

    if (orig_space.m_uri.empty() || dest_space.m_uri.empty()) {
        OES_ERROR("Could not find origin and/or dest dataspace");
        return false;
    }

    const auto retries = num_scoped_transaction_retries(ctx);
    oes::client::ScopedTransaction tx_source(*clt.clt_, clt.put_get_timeout_);
    oes::client::ScopedTransaction tx_target(*clt.clt_, clt.put_get_timeout_);
    if (!tx_source.start(orig_space.m_uri, "Source copy", false, nullptr, retries)){
        OES_WARNING("Could not start copy dataspace content: source dataspace");
    }
    if (!tx_target.start(dest_space.m_uri, "Destiny copy", false, nullptr, retries)){
        OES_WARNING("Could not start copy dataspace content: destiny dataspace");
    }

    auto disc_rpc = rpcFor(disc_proto, &etpc::proto12::IDiscovery::GetResources);
    disc_rpc.req_.m_context.m_uri = orig_space.m_uri;
    disc_rpc.req_.m_context.m_depth = 1;
    std::vector<oes::common::etp12::Datatypes::Object::Resource> resources;
    if (!disc_rpc.callForEachValue(
        ctx.err(), clt.put_get_timeout_,
        [&](auto& res) {
            resources.emplace_back(std::move(res));
        }
    )) {
        OES_ERROR("Error calling disc rpc");
    }

    auto spc_osdu_rpc = rpcFor(spc_osdu_proto, &etpc::proto12::IDataspaceOSDU::CopyToDataspace);
    spc_osdu_rpc.req_.m_dataspaceUri = dest_space.m_uri;
    for (const auto& res : resources) {
        spc_osdu_rpc.req_.m_uris.emplace(res.m_uri, res.m_uri);
    }

    auto res = spc_osdu_rpc.call(ctx.err(), clt.put_get_timeout_);
    if (!res.first) {
        OES_ERROR("Could not copy to dest dataspace");
        return false;
    }

    tx_source.commit();
    tx_target.commit();
    
    return true;
}

//============================================================================
bool space_copy(const EmlAppContext& ctx) {
    const bool by_ref = ctx.hasArg("by-ref");
    const bool from_remote = ctx.hasArg("from-remote");

    if (!by_ref && !from_remote) {
        OES_ERROR("None args \"by-ref\" and \"from-remote\" passed, please use one");
        return false;
    } else if (!by_ref && !from_remote) {
        OES_ERROR("Both args \"by-ref\" and \"from-remote\" passed, please use only one");
        return false;
    }

    return by_ref ? space_copy_ref(ctx) : space_copy_from_remote(ctx);
}

} //namespace

namespace oes::apps::eml {

/****************************************************************************/

int space_main(EmlAppContext& ctx, const UnparsedArgs& unparsed_args) {
    po::options_description cmd_desc(
        "\nUsage: openETPServer [global-options] " + ctx.command + " --action [args]\n"
        "\nActions", ctx.terminalWidth_
    );
    cmd_desc.add_options()
        ("help,h", "Display this information")

        ("list,l", txtArgOpt(),     "List ETP dataspaces (aka Projects)")
        ("info",                    "Show info / details on ETP dataspace")
        ("toc",    txtArgOpt(),     "Get Table-of-Contents on ETP dataspace\n"
                                    "Prints Top-N (no argument); or writes JSON file")
        ("stats",                   "Get Statistics on ETP dataspace")
        ("array-stats",             "Get Statistics on ETP dataspace bulk data arrays")
        ("new",                     "Create new ETP dataspace")
        ("delete",                  "Delete ETP dataspace")
        ("delete-all", txtArg(),    "Delete All matching ETP dataspaces")
        ("lock",                    "Lock ETP dataspace (set read-only)")
        ("unlock",                  "Unlock ETP dataspace (set read-write)")
        ("import-space",txtArg(),   "Import other ETP dataspace contents into this one")
        ("import-epc", txtArg(),    "Import (PUT) EPC file into ETP server")
        ("export-epc", txtArgOpt(), "Export (GET) EPC file from ETP server")
        ("copy",                    "Copy dataspace by reference OR from remote server. Must provide --by-ref XOR --from-remote")
    ;

    po::options_description opt_desc("Options", ctx.terminalWidth_);
    opt_desc.add_options()
        ("space-path,s",    txtArg(), "ETP dataspace path (folder/project)")
        ("space-guid",   txtArgOpt(), "ETP dataspace guid (uses path if omitted)")
        ("top",             intArg(10), "How many entries to list")
        ("skip",            intArg(0), "Skip how many entries in the list")
        ("utc", "Show timestamps in UTC, instead of local time")
        ("iso", "Show timestamps in ISO-8601 format")
        ("flat", "Do not group ETP dataspaces by folder")
        ("since", txtArg(), "Filter by Last-Store-Write timestamp.\n"
                            "(ISO date-time; or duration: 1d, 3m, 1y, etc...)")
        ("types", txtArg(), "Filter TOC by object types (CSV).\n")
        ("xdata-file", txtArg(), "New Space meta-data as JSON file")
        ("xdata", txtArg(), "New Space meta-data as JSON")
        ("overwrite", "Overwrite existing EPC on export")
        ("reject-duplicates", "The import is reject if any importing resource is already in the dataspace")
        ("force", "Continue instead of exit, on some warnings/errors")

        ("jobs,j",
            greedy_value<int64_t>()->implicit_value(0),
            "How many worker-threads:\n"
            "* not specified: Defaults to single-threaded;\n"
            "* -j: Use as many threads as hardware cores;\n"
            "* -jN: Use exactly N threads")

        ("array-queue-depth", intArg(10),   "Max (sub)arrays accumulated in-memory\n"
                                            "(Peak RAM proportional to -M x this-option)")

        ("hdf5-compression,z",  intArg(3),          "Compression level of most datasets:\n"
                                                    "0 none; 1 fast but bigger; 9 best but slower")
        ("hdf5-large-data-compression", intArg(3),  "Compression level of large datasets (3D or more):\n"
                                                    "0 none; 1 fast but bigger; 9 best but slower")
        ("transaction-retries", intArg(-1), "Number of retries to try to start a scoped transaction.\n"
                                          "Default is 0 - only one try, no retries")
        ("transaction-limit-time", txtArg("inf"), "Minimum amount of time to trigger transaction commit and start a new one when uploading arrays.")
        ("transaction-limit-size", txtArg("inf"), "Minimum size to trigger transaction commit and start a new one when uploading arrays.")
        ("reconnect-retries", intArg(-1), "Maximum number of reconnection retries while uploading arrays")
        
        ("by-ref", "Copy dataspace by reference")
        ("from-remote", "Copy dataspace from remote server (--source-server) to the target server (--server-url)")

        ("source-partition", txtArg(""), "Original data partition.")
        ("source-space", txtArg(""), "Original dataspace to be copied from.")
        ("source-token", txtArg(""), "Source remote server token")
        ("source-server", txtArg(""), "Source remote server")
    ;

    cmd_desc.add(opt_desc);
    cmd_desc.add(connect_options(ctx.terminalWidth_));
    cmd_desc.add(authenticate_options(ctx.terminalWidth_));
    cmd_desc.add(osdu_partition_options(ctx.terminalWidth_));

    if (int rc = ctx.reparse(cmd_desc, unparsed_args)) {
        return (rc == -1)? 0: rc;
    }

    EmlAppNamedAction actions[] {
        { "new",          &space_new },
        { "info",         &space_info },
        { "list",         &space_list },
        { "toc",          &space_toc },
        { "stats",        &space_stats },
        { "array-stats",  &space_array_stats },
        { "delete",       &space_delete },
        { "delete-all",   &space_delete_all },
        { "lock",         &space_lock },
        { "unlock",       &space_unlock },
        { "import-epc",   &space_import_epc },
        { "import-space", &space_import_space },
        { "export-epc",   &space_export_epc },
        { "copy",         &space_copy }
    };
    return ctx.dispatchActions(actions);
}

/****************************************************************************/

} // namespace oes::apps::eml
