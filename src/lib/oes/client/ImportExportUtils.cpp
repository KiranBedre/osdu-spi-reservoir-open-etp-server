#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "ImportExportUtils.h"
#include "Client.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

#include <oes/common/etp12/EmlUri.h>
#include <oes/common/etp12/Messages.h>
#include <oes/common/etp12/Protocols.h>
#include <oes/core/utils/Logger.h>

using oes::common::etp12::EmlUri;

namespace oes::client {

std::string getDataspaceUriFromDataspaces(Client& clt, const std::string& path_or_uid, std::chrono::milliseconds timeout) {
    auto get_rpc = rpcFor(clt.space_proto(), &oes::client::proto12::IDataspace::GetDataspaces);
    std::ostringstream oss2;
    std::vector<oes::common::etp12::Datatypes::Object::Dataspace> spaces;
    if (!get_rpc.callForEachValue(
        oss2, timeout,
        [&](auto& ds) {
            spaces.emplace_back(ds);
        }
    )) {
        OES_ERROR("Error on getDataspaces: {}", oss2.str());
        return std::string();
    }

    const EmlUri space_uri = EmlUri::make_space(path_or_uid);
    const std::string space_uri_str = space_uri.to_string();
    auto found = std::find_if(
        spaces.begin(), spaces.end(), [path_or_uid, space_uri_str](const auto& space) {
            return space.m_path == path_or_uid || space.m_uri == space_uri_str;
        }
    );
    if (found == spaces.end()) {
        OES_ERROR("ETP Dataspace not found: {}", path_or_uid);
        return std::string();
    }
    return found->m_uri;
}

/*!
 * Get the ETP dataspace URI for a given path or UID using dataspaceOsdu protocol.
* \param clt ETP 1.2 client, already connected and authenticated with the server
* \param path_or_uid the path or UID of the dataspace
* \return the URI of the dataspace, or an empty string if not there
*/
std::string getDataspaceUriInfo(Client& clt, const std::string& path_or_uid, std::chrono::milliseconds timeout){
    auto get_rpc = rpcFor(clt.space_osdu_proto(), &oes::client::proto12::IDataspaceOSDU::GetDataspaceInfo);
    const EmlUri space_uri = EmlUri::make_space(path_or_uid);
    const std::string space_uri_str = space_uri.to_string();
    get_rpc.req_.m_uris.insert(std::pair<std::string, std::string>(path_or_uid, space_uri_str));
    
    std::ostringstream oss2;
    get_rpc.call(oss2, timeout);

    auto& spaces = get_rpc.res_.m_dataspaces;

    if (spaces.empty() || spaces.size() > 1) {
        OES_ERROR("Dataspace not found {}", path_or_uid);
        return std::string();
    }

    const auto& the_one = (*spaces.begin()).second;

    return the_one.m_uri;
}

/*!
 * Get the ETP dataspace URI for a given path or UID.
* \param clt ETP 1.2 client, already connected and authenticated with the server
* \param path_or_uid the path or UID of the dataspace
* \param timeout rpc timeout
* \return the URI of the dataspace, or an empty string if not found
*/
std::string getDataspaceUri(Client& clt, const std::string& path_or_uid, std::chrono::milliseconds timeout) {
    auto proto = clt.get_proto<proto12::IDataspaceOSDU>();
    const bool use_osdu_proto = proto != nullptr;
    return use_osdu_proto ? getDataspaceUriInfo(clt, path_or_uid, timeout) : getDataspaceUriFromDataspaces(clt, path_or_uid, timeout);
}

std::string normalizeHdfPath(std::string path) {
    const size_t first = path.find_first_not_of('/');
    path.erase(0, (first == std::string::npos)? path.size(): first);
    return path;
}

std::map<HdfPathKey, std::string> planHdfPaths(const std::vector<HdfPathKey>& keys) {
    std::vector<HdfPathKey> sorted;
    sorted.reserve(keys.size());
    for (const auto& [container, path] : keys) {
        sorted.emplace_back(normalizeHdfPath(path), container); // i.e. (path, container)
    }
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    std::set<std::string> datasets;
    std::set<std::string> groups;

    auto collides = [&datasets, &groups](const std::string& path) {
        if (datasets.count(path) || groups.count(path)) {
            return true;
        }
        for (size_t pos = path.find('/'); pos != std::string::npos; pos = path.find('/', pos + 1)) {
            if (datasets.count(path.substr(0, pos))) {
                return true;
            }
        }
        return false;
    };
    auto accept = [&datasets, &groups](const std::string& path) {
        datasets.insert(path);
        for (size_t pos = path.find('/'); pos != std::string::npos; pos = path.find('/', pos + 1)) {
            groups.insert(path.substr(0, pos));
        }
    };
    auto groupNameOf = [](const std::string& container) {
        std::string name = container.empty()? "_": container;
        for (char& c : name) {
            const bool ok = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.';
            if (!ok) {
                c = '_';
            }
        }
        return name;
    };

    std::map<HdfPathKey, std::string> planned; // (container, normalized path) -> output path
    for (const auto& [path, container] : sorted) {
        std::string out = path;
        if (collides(out)) {
            const std::string group = groupNameOf(container);
            out = group + '/' + path;
            for (int n = 2; collides(out); ++n) {
                out = fmt::format("{}_{}/{}", group, n, path);
            }
        }
        accept(out);
        planned.emplace(HdfPathKey{container, path}, std::move(out));
    }

    std::map<HdfPathKey, std::string> result;
    for (const auto& key : keys) {
        result.emplace(key, planned.at({key.first, normalizeHdfPath(key.second)}));
    }
    return result;
}

} // namespace oes::client