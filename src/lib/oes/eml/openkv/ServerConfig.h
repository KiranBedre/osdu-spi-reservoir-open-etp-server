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
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

#ifndef OES_EML_OPENKV_SERVERCONFIG_H
#define OES_EML_OPENKV_SERVERCONFIG_H

#include "../fwd.h"
#include "../export.h"
#include "../OpenETPServerConfig.h"

#include <functional>

namespace oes::eml::openkv {

//============================================================================

using OpenKVAuthNMgr = std::function<
    oes::common::auth::AuthenticatedUser(const oes::common::auth::AuthInfo& authenticationInfo, oes::common::auth::AuthDetails*)
>;

using OpenKVUserMgr = std::function<
    bool(const std::string& username, const std::string& password)
>;

using OpenKVSpaceMgr = std::function<
    Connection_ptr(const std::string& relative_dbfile, bool new_db)
>;

using OpenKVSpaceResolver = std::function<
    std::string(const std::string& relative_dbfile, bool new_db)
>;

using EntitlementMgr = std::function<
    oes::common::auth::Entitlement_ptr()
>;

//============================================================================

OES_EML_EXPORT
OpenKVAuthNMgr builtinAuthMgr(
    OpenKVUserMgr usr_mgr,
    std::string jwtSecret
);

//============================================================================

OES_EML_EXPORT
etp12::ServerConfig makeServerConfig(
    DbConnectionPool_ptr db_connection_pool,
    OpenKVAuthNMgr auth_mgr,
    std::string authn_delegation_url,
    OpenKVSpaceMgr space_mgr,
    OpenKVSpaceResolver space_resolver,
    EntitlementMgr entitlement_mgr,
    oes::eml::openETPServerConfig_ptr server_config_ptr
);

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_SERVERCONFIG_H
