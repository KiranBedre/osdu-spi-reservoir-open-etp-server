#ifndef OES_IMPORT_EXPORT_UTILS_H
#define OES_IMPORT_EXPORT_UTILS_H

#include <string>
#include <chrono>

#include "export.h"
#include "Client.h"

namespace oes::client {

/*!
 * Get the ETP dataspace URI for a given path or UID.
* \param clt ETP 1.2 client, already connected and authenticated with the server
* \param path_or_uid the path or UID of the dataspace
* \param timeout rpc timeout
* \return the URI of the dataspace, or an empty string if not found
*/
OES_CLIENT_EXPORT
std::string getDataspaceUri(Client& clt, const std::string& path_or_uid, std::chrono::milliseconds timeout);

} // namespace oes::client

#endif