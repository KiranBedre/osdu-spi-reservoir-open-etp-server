#ifndef OES_IMPORT_EXPORT_UTILS_H
#define OES_IMPORT_EXPORT_UTILS_H

#include <string>
#include <chrono>
#include <map>
#include <utility>
#include <vector>

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

//! An array in an export: (container, dataset path). The container is the EML 2.0
//! HdfProxy uuid, or the EML 2.3 external part URI.
using HdfPathKey = std::pair<std::string, std::string>;

//! \return \a path without its leading '/' characters.
OES_CLIENT_EXPORT
std::string normalizeHdfPath(std::string path);

/*!
 * Assigns each array a dataset path in a single HDF5 file.
 *
 * Arrays from different containers may share a path, or overlap (a dataset at
 * a/b and another at a/b/c), which cannot coexist in one HDF5 file. Among those,
 * the smallest (path, container) keeps its path, the others are relocated under
 * a top group named after their container: "<container>/<path>", or
 * "<container>_2/<path>", etc... if that is still taken. Deterministic, regardless
 * of the order of \a keys.
 * \return the normalized output path of each key.
 */
OES_CLIENT_EXPORT
std::map<HdfPathKey, std::string> planHdfPaths(const std::vector<HdfPathKey>& keys);

} // namespace oes::client

#endif