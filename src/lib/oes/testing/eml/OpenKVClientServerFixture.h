#pragma once
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

#include "export.h"

#include <oes/client/Client.h>

#include <oes/eml/fwd.h>
#include <oes/eml/etp12/Server.h>
#include <oes/eml/OpenETPServerConfig.h>

#include <oes/core/utils/ThreadUtils.h>
#include <oes/testing/core/TestConfig.h>

#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace oes::testing::eml {

using Client_uptr = std::unique_ptr<oes::client::Client>;

class OES_TESTING_EML_EXPORT OpenKVClientServerFixture {
public:
    OpenKVClientServerFixture();
    OpenKVClientServerFixture(const OpenKVClientServerFixture&) = delete;
    OpenKVClientServerFixture& operator=(const OpenKVClientServerFixture&) = delete;

    virtual ~OpenKVClientServerFixture();

    // What you can edit to control the server behavior:
    // - openkv_folder_ and/or admin_db_name_ for where backend stores DBs
    // - svr_conf_ for the server's configuration, i.e. max_mgs_size_
    virtual void initServer();
    bool directImport(std::string epc, std::string space_path);
    virtual void clearImports();
    virtual void clearAdmin();
    void startServer(uint32_t totalThreadCount = 1, int maxConnections = 10);
    void stopServer();

    oes::client::ClientConfig& initClientConfig();
    Client_uptr connectClient();

private:
    void shutdownServer();

protected:
    oes::eml::openkv::OpenKVAdminDB_ptr admin_db_;
    oes::eml::openkv::DbConnectionPool_ptr connection_pool_;

    std::string openkv_folder_;
    std::string admin_db_name_ = "EmrEtp12OpenKV.db";
    bool create_default_space_ = false;
    bool init_done_ = false;

    int svr_port_ = 0;
    std::string jwtSecret_;
    oes::eml::etp12::ServerConfig svr_conf_;
    oes::eml::etp12::Server* svr_ = nullptr;
    std::promise<bool> svr_rc_;
    oes::core::ThreadJoiner svr_joiner_;

    oes::client::ClientConfig clt_conf_;

    std::string current_space_;
    std::vector<std::string> created_spaces_;

    oes::eml::openETPServerConfig_ptr test_config_;
};

//============================================================================

using RpcCallRc = std::pair<bool, std::future_status>;

OES_TESTING_EML_EXPORT
bool      call_succeeded(const RpcCallRc& ok, const std::ostringstream& oss);

OES_TESTING_EML_EXPORT
std::string call_err_msg(const RpcCallRc& ok, const std::ostringstream& oss);

//============================================================================

} // namespace oes::testing::eml
