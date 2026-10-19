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

#include <filesystem>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/eml/OpenKVClientServerFixture.h>

#include <oes/common/etp12/Protocols.h>
#include <oes/client/ImportEpc.h>

#include <oes/eml/openkv/AdminDB.h>
#include <oes/eml/openkv/ServerConfig.h>
#include <oes/eml/openkv/Session.h>

#include <oes/client/Client.hpp>

#include <oes/postgresql/DbConnectionPool.h>

#include <oes/testing/core/TestMain.h>


using namespace oes::eml;
using oes::client::rpcFor;
namespace etp = oes::eml::etp12;
namespace fs = std::filesystem;

namespace oes::testing::eml {

OpenKVClientServerFixture::OpenKVClientServerFixture()
    : connection_pool_(std::make_shared<postgresql::DbConnectionPool>(100)),
      test_config_(oes::testing::TestConfig::make())
{ }

OpenKVClientServerFixture::~OpenKVClientServerFixture() {
    stopServer();
    OpenKVClientServerFixture::clearImports();
    OpenKVClientServerFixture::clearAdmin();
}

void OpenKVClientServerFixture::initServer() {
    BOOST_REQUIRE(!init_done_);
    std::string connection_str = test_config_->standaloneConnectionStrings();
    BOOST_CHECK(!connection_str.empty());

    const bool isRegistered = connection_pool_->registerDatabase("single", connection_str, true);
    BOOST_CHECK_MESSAGE(isRegistered, "Error: Single Database mode cannot connect");

    auto dbConnection = connection_pool_->tryAcquireConnection("single");
    admin_db_ = std::make_shared<openkv::OpenKVAdminDB>(dbConnection);

    bool schema_ok = false;
    if (!admin_db_->isSchemaCreated()) {
        schema_ok = admin_db_->createSchema();
    } else {
        schema_ok = true;
    }
    BOOST_REQUIRE(schema_ok);

    if (create_default_space_) {
        openkv::Space space;
        space.uid_.clear(); // No uid
        space.path_.clear(); // No name either
        space.dbfile_ = "EmrEtp12DefaultSpace";

        const bool space_ok = admin_db_->addSpaces({ &space, &space + 1 });
        BOOST_REQUIRE(space_ok);
    }

    init_done_ = true;
}

bool OpenKVClientServerFixture::directImport(
    std::string epc, std::string space_path
) {
    if (!init_done_) {
        initServer();
    }

    const fs::path epc_path(epc);
    const fs::file_status epc_stat = fs::status(epc_path);
    BOOST_REQUIRE(fs::is_regular_file(epc_stat));

    
    const std::string db_path = space_path.empty() ?
        "EmrEtp12DefaultSpace" : space_path;

    {
    std::string path, returned_db_path, folder, name, err_msg;
    const bool path_ok = openkv::Space::validatePath(
        space_path,
        path, returned_db_path, folder, name, err_msg // outputs
    );
    BOOST_REQUIRE(path_ok);
    }

    auto client = connectClient();
    const std::chrono::milliseconds timeout(100'000); // for debugging

    auto* proto = client->space_proto();
    if (!proto) {
        return false;
    }

    auto rpc = rpcFor(proto, &etp::proto12::IDataspace::PutDataspaces);

    etp::data12::Object::Dataspace space;
    space.m_path = db_path;
    std::string uid = db_path;
    space.m_uri = oes::common::etp12::EmlUri::make_space(uid).to_string();
    rpc.req_.m_dataspaces.emplace(uid, space);

    std::ostringstream oss;
    auto ok = rpc.call(oss, timeout);
    if (!ok.first) {
        return false;
    }
    current_space_ = db_path;
    oes::client::ImportEpcOptions options;
    options.timeout_ = timeout;
    const bool import_ok = oes::client::importEpc(client->getConfig(), db_path, epc, options);
    // Saving newly created space
    if (import_ok) {
        // Ok to save everytime because a second import of the same project will fail
        created_spaces_.push_back(current_space_);
    }
    return import_ok;
}

void OpenKVClientServerFixture::clearImports(){
    if( !current_space_.empty() ){
        oes::eml::openkv::Space space = admin_db_->findSpace(current_space_);
        if(space.isValid()){
            const oes::postgresql::relational::PhysicalName phys(space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
            admin_db_->handle()->execute(fmt::format("DROP SCHEMA IF EXISTS {} CASCADE", phys.quotedName()));
        }
    }
    for(const std::string& sp : created_spaces_) {
        oes::eml::openkv::Space space = admin_db_->findSpace(sp);
        if(space.isValid()){
            const oes::postgresql::relational::PhysicalName phys(space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
            admin_db_->handle()->execute(fmt::format("DROP SCHEMA IF EXISTS {} CASCADE", phys.quotedName()));
        }
    }
}

void OpenKVClientServerFixture::clearAdmin(){
    if( admin_db_ ){
        admin_db_->dropAdmin();
    }
}

void OpenKVClientServerFixture::startServer(uint32_t totalThreadCount, int maxConnections) {
    if (!init_done_) {
        initServer();
    }

    fs::path folder(openkv_folder_);

    openkv::OpenKVUserMgr usr_mgr = [](const std::string&, const std::string&) {
        return true; // accept any creds
    };
    openkv::OpenKVSpaceResolver space_resolver = [folder](const std::string& rel_dbfile, bool) {
        auto db = (folder / rel_dbfile);
        if (db.parent_path() != folder) {
            fs::create_directory(db.parent_path()); // OK if already exists
        }
        const std::string abs_dbfile = db.string();
        return abs_dbfile;
    };

    // Allow changing the JWT secret
    if (jwtSecret_.empty()) {
        jwtSecret_ = "victoria";
    }

    {
        // We get the factories and capabilities from the functions.
        // Other configs are either hardcoded (below), or client-code configurable
        auto auth_mgr = openkv::builtinAuthMgr(usr_mgr, jwtSecret_);
        svr_conf_ = openkv::makeServerConfig(
            connection_pool_, std::move(auth_mgr), "", nullptr, space_resolver, nullptr, test_config_
        );
    }

    const etp12::ServerConfig default_conf;

    svr_conf_.port_ = 0; // i.e. use any port
    svr_conf_.totalThreadCount_ = totalThreadCount;
    svr_conf_.reuseAddressSocket_ = true;
    svr_conf_.stopOnLastDisconnection_ = true;
    svr_conf_.useIpV4_ = false; // avoid 1s WebSocket connection delay on Windows
    svr_conf_.maxNumberConnection_ = maxConnections;
    svr_conf_.session_keep_alive_duration_ = std::chrono::seconds(5);

    // Use a smaller default than "normal", for tests
    // The 2GB default is mostly for compat with DX ETP
    if (default_conf.max_mgs_size_ == svr_conf_.max_mgs_size_) {
        // TODO: Use much smaller values when Multi-Part auto-split implemented
        svr_conf_.max_mgs_size_ = 30'000'000; // 30 MB
    }

    // TODO: Decompression shouldn't allocate all that memory upfront!
    if (default_conf.max_mgs_usize_ == svr_conf_.max_mgs_usize_) {
        // TODO: Use much smaller values when Multi-Part auto-split implemented
        svr_conf_.max_mgs_usize_ = 64'000'000; // 64 MB
    }

    svr_ = new etp12::Server(svr_conf_);
    etp::Datatypes::ServerCapabilities svr_info = svr_->getServerInfo();
    BOOST_REQUIRE_EQUAL(svr_info.m_applicationName.find("ReservoirDDMS"), 0);

    std::future<int> svr_port_fut = svr_->getRandomServerPort();

    svr_joiner_.threads_.emplace_back([this]() {
        try {
            const bool ok = svr_->start();
            svr_rc_.set_value(ok);
        } catch (...) {
            // exception "transported" across threads below
            svr_rc_.set_exception(std::current_exception());
        }
    });

    // blocks until the server is about to start .run()'ing
    svr_port_ = svr_port_fut.get();
    BOOST_REQUIRE_NE(svr_port_, 0);

    auto& conf = initClientConfig();
    BOOST_REQUIRE_EQUAL(&conf, &clt_conf_);
}

void OpenKVClientServerFixture::stopServer() {
    if (svr_) {
        shutdownServer();
        delete svr_;
        svr_ = nullptr;
    }
}

oes::client::ClientConfig& OpenKVClientServerFixture::initClientConfig() {
    const oes::client::ClientConfig default_conf;

    // Use a smaller default than "normal", for tests
    // The 2GB default is mostly for compat with DX ETP
    if (default_conf.max_mgs_size_ == clt_conf_.max_mgs_size_) {
        clt_conf_.max_mgs_size_ = 32*1024*1024; // 32 MiB
    }

    // Default to JWT Auth, unless Auth already selected
    if (clt_conf_.auth_hdr_.empty()) {
        clt_conf_.withJwtAuth("foo", "bar", jwtSecret_);
    }

    // Always override this, not for client-code to control
    clt_conf_.server_url_ = fmt::format("ws://localhost:{}", svr_port_);

    return clt_conf_;
}

Client_uptr OpenKVClientServerFixture::connectClient() {
    BOOST_REQUIRE(svr_ && svr_port_); // i.e. server started

    Client_uptr clt(new oes::client::Client(clt_conf_));
    const bool clt_ok = clt->connect(std::chrono::milliseconds(5000));
    BOOST_REQUIRE(clt_ok);
    return clt;
}

void OpenKVClientServerFixture::shutdownServer() {
    BOOST_REQUIRE(svr_);

    // shutdown the server
    svr_->stopListening();

    // And wait for it to exit
    const bool svr_ok = svr_rc_.get_future().get(); // might throw
    BOOST_CHECK(svr_ok);

    // Wait for the server-thread to exit (explicitly)
    {
        oes::core::ThreadJoiner local_joiner;
        local_joiner.threads_.swap(svr_joiner_.threads_);
        BOOST_CHECK(svr_joiner_.threads_.empty());
    } // join / wait on the server's thread here
}

//============================================================================

bool call_succeeded(const RpcCallRc& ok, const std::ostringstream& oss) {
    if (!ok.first) {
        return false;
    }
    std::string err_msg = oss.str();
    if (!err_msg.empty()) {
        return false;
    }
    return true;
}

std::string call_err_msg(const RpcCallRc& ok, const std::ostringstream& oss) {
    BOOST_CHECK(!ok.first);

    std::string err_msg;
    switch (ok.second) {
    case std::future_status::ready:    err_msg = "ready"; break;
    case std::future_status::timeout:  err_msg = "timeout"; break;
    case std::future_status::deferred: err_msg = "deferred"; break;
    }

    err_msg += ": ";
    err_msg += oss.str();
    return err_msg;
}

//============================================================================

} // namespace oes::testing::eml
