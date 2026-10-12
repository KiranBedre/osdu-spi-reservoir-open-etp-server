#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "ImportExportUtils.h"
#include "Client.hpp"

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

} // namespace oes::client