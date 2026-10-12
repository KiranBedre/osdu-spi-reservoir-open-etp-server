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

#ifndef OES_CLIENT_ETP12_IMPORTEPC_H
#define OES_CLIENT_ETP12_IMPORTEPC_H

#include "export.h"
#include "Client.h"
#include "ClientUtils.h"

#include <chrono>
#include <map>
#include <array>

namespace oes::client {

struct OES_CLIENT_EXPORT ImportEpcOptions: EpcOptions {
    bool reject_duplicates_ = false;

    size_t transaction_limit_bytes = 0;
    double transaction_limit_sec = -1;
    int reconnect_retries = 0;
};

OES_CLIENT_EXPORT
bool importEpc(
    const oes::client::ClientConfig& client_config,
    const std::string& etp_space,
    const std::string& epc_filename,
    const ImportEpcOptions& options = {}
);

/**
 * @brief Import data objects based on xml strings
 * 
 * @param client Etp client
 * @param space_uri_txt  space uri string
 * @param partIdStr_xmlStr_map map of string to be transformed into Objects. Key is the 
 * the xml object name with .xml extention 
 * @param options import epc options.
 * @return true if successed
 */
OES_CLIENT_EXPORT
bool importXmlStringObjects(
    Client& client,
    const std::string& space_uri_txt,
    std::map<std::string, std::string>&& partIdStr_xmlStr_map,
    const ImportEpcOptions& options = {}
);

} // namespace oes::client

#endif
