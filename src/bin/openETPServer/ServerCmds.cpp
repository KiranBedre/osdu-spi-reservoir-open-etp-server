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

#include <chrono>
#include <memory>
#include <iostream>
#include <ios>
#include <string>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string/predicate.hpp>
#include <websocketpp/uri.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "EmlAppContext.h"
#include "EtpClient12.h"

#include <oes/eml/openkv/AdminDB.h>
#include <oes/eml/openkv/ServerConfig.h>
#include <oes/postgresql/DbConnectionPool.h>

#include <oes/common/etp12/Messages.h>
#include <oes/common/auth/AuthN.h>
#include <oes/common/auth/DelegateClient.h>
#include <oes/common/auth/Entitlement.h>
#include <oes/common/etp12/Metadata.h>
#include <oes/common/AvroUtils.hpp>

#include <oes/eml/etp12/Server.h>
#include <oes/eml/etp12/MsgUtils.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/ThousandSep.h>
#include <oes/core/http/HTTP.h>

#include <oes/osdu/api/PartitionClient.h>
#include <oes/osdu/api/EntitlementClient.h>
#include <oes/osdu/OsduConnectionProvider.h>


using namespace oes::apps::eml;
using namespace oes::core;
using namespace oes::eml;

namespace po = boost::program_options;
namespace data12 = oes::common::etp12::Datatypes;
namespace auth = oes::common::auth;

namespace {
    oes::eml::openkv::DbConnectionPool_ptr getOsduRepos(const EmlAppContext& ctx)
    {
        auto dbConnectionPool = std::make_shared<oes::postgresql::DbConnectionPool>();
        for (auto const& partitionConnectionInfo: oes::osdu::OsduConnectionInfoProvider::getOsduConnectionInfo(ctx.appConfig()))
        {
            dbConnectionPool->registerDatabase(partitionConnectionInfo.first, partitionConnectionInfo.second, true);
        }
        return dbConnectionPool;
    }

    oes::eml::openkv::DbConnectionPool_ptr getStandaloneRepo(const EmlAppContext& ctx) {
        //TODO getStandaloneRepo
        return getOsduRepos(ctx);
    }

    std::string getMainRepo(const EmlAppContext& ctx)
    {
        std::string dir = ctx.stringArg("db-connection");

        if (dir.empty()) {
            auto connection_string = ctx.appConfig().standaloneConnectionStrings();
            if (connection_string.empty()) {
                OES_ERROR("Error: Missing required --db-connection or POSTGRESQL_CONN_STRING environment variable.");
            }
            
            return connection_string;
        }

        return dir;
    }

    bool initDatabase(const EmlAppContext& ctx, const openkv::OpenKVAdminDB_ptr& admin_db) {
        std::ostringstream oss;
        oss << "SELECT set_config('application_name','" << admin_db->app_id() << "', false);";

        const std::string app_pragmas = oss.str();
        admin_db->handle()->execute(app_pragmas.c_str());

        if(!admin_db->isSchemaCreated()){
            const bool ok = admin_db->createSchema();
            if (ok) {
                OES_INFO("Created main DB.");
            } else {
                OES_ERROR("Cannot create main DB.");
                // TODO We should really exit
                return false;
            }

            // Create the ETP 1.2 mandatory "default" (no name) Dataspace
            if (ok && !ctx.hasArg("no-default-space")) {
                // TODO(unknown): Create default space
            }
        } else {
            int current_v = 0;
            int latest_v = 0;
            admin_db->isSchemaUpToDate(true, &current_v, &latest_v);
            if(current_v != latest_v){
                OES_WARNING("Out-of-date main DB.");
                if(!admin_db->upgradeSchema(current_v, latest_v)){
                    OES_ERROR("Cannot upgrade main DB from version {} to {}.", current_v, latest_v);
                    // TODO We should really exit
                    return false;
                }else{
                    OES_INFO("Upgraded main DB from version {} to {}.", current_v, latest_v);
                }
            } else {
                //TODO: Remove Force update
                OES_INFO("Database already created.");
            }
        }

        return true;
    }

    bool isDatabaseReady(const EmlAppContext& ctx, const openkv::OpenKVAdminDB_ptr& admin_db, const std::string& dbName) {
        const bool created = initDatabase(ctx, admin_db);
        if (created) {
            if (!admin_db->isSchemaUpToDate(false)) {
                OES_WARNING("Out-of-date main DB: {}.", dbName);
            }
        } else {
            OES_ERROR("Empty main DB: {}.", dbName);
            return false;
        }

        return true;
    }

    std::string databaseMode(const EmlAppContext& ctx) {
        std::string dbm = ctx.appConfig().partitionMode();
        
        if (dbm != "single" and dbm != "multiple") {
            throw std::runtime_error("Database mode unknown. Possible modes: (single, multiple). Was given " + dbm);
        }

        return dbm;
    }
    
    std::string connectionMode(const EmlAppContext& ctx) {
        auto connectivity_str = ctx.appConfig().connectivityMode();

        if (connectivity_str.empty()) {
            connectivity_str = "osdu";
        }

        if (connectivity_str != "standalone" and connectivity_str != "osdu") {
            throw std::runtime_error("Connection mode unknown. Possible modes: (standalone, osdu). Was given " + connectivity_str);
        }
        
        return connectivity_str;
    }

    bool initConnectionPool(const EmlAppContext& ctx, oes::eml::openkv::DbConnectionPool_ptr& dbConnectionPool){
        std::string dbm; // single or multiple
        std::string connectivity_mode; // standalone or osdu

        try {
            dbm = databaseMode(ctx); // single or multiple
            connectivity_mode = connectionMode(ctx); // standalone or osdu
        } catch(const std::exception& ex) {
            OES_ERROR("openETPServer: {}", ex.what());
            return false;
        } catch(...) {
            OES_ERROR("Database mode unknown.");
            return false;
        }

        if (dbm == "single"){
            const std::string mainRepoConnStr = getMainRepo(ctx);
            if (mainRepoConnStr.empty()) {
                OES_ERROR("Single Database mode cannot connect.");
                return false;
            }
            
            if (!dbConnectionPool->registerDatabase("single", mainRepoConnStr, true)) {
                throw std::runtime_error("Could not register 'single' database.");
            }

        } else if (dbm == "multiple") {
            if (connectivity_mode == "standalone")
                dbConnectionPool = getStandaloneRepo(ctx);
            else if ( connectivity_mode == "osdu" )
                dbConnectionPool = getOsduRepos(ctx);
        }

        return true;
    }

    //============================================================================

    bool alt_etp12_openkv_start(const EmlAppContext& ctx)
    {
        auto dbConnectionPool = std::make_shared<oes::postgresql::DbConnectionPool>();
        if (!initConnectionPool(ctx, dbConnectionPool)) {
            return false;
        }

        for (auto const& connectionElement: dbConnectionPool->getRegisteredDatabases())
        {
            auto dbConnection = dbConnectionPool->tryAcquireConnection(connectionElement.first);
            openkv::OpenKVAdminDB_ptr adminDb = std::make_shared<openkv::OpenKVAdminDB>(dbConnection);

            if (!isDatabaseReady(ctx, adminDb, connectionElement.first)) {
                OES_ERROR("Cannot start server: Repo database '{}' is not initialized.", connectionElement.first);
            }
        }

        std::string jwtSecret = ctx.stringArg("jwt-secret");

        std::string authN = ctx.stringArg("authN");
        if (ctx.hasArg("auth")) {
            if (ctx.hasArg("authN") && !ctx.args["authN"].defaulted()) {
                OES_ERROR("--auth replaced by --authN. Please use the latter.");
                return false;
            } else {
                OES_WARNING("--auth deprecated: Please use --authN instead.");
                authN = ctx.stringArg("auth");
            }
        }

        if (authN != "none" && authN != "builtin" && !boost::starts_with(authN, "delegate=")) {
            OES_ERROR("Invalid --authN: Use 'none', 'builtin', or 'delegate=URL'");
            return false;
        }

        if (authN == "delegate=none") {
            authN = "builtin";
        }

        std::string authZ = ctx.stringArg("authZ");
        if (authZ != "none" && authZ != "builtin" && !boost::starts_with(authZ, "delegate=") && !boost::starts_with(authZ, "policyDelegate=")) {
            OES_ERROR("Invalid --authZ: Use 'none', 'builtin', or 'delegate=URL'");
            return false;
        }

        if (authZ == "policyDelegate=none") { // Temporary
            OES_WARNING("Changing authZ from {} to none", authZ);
            authZ = "none";
        }

        if (authN != "builtin" && !jwtSecret.empty()) {
            OES_WARNING("Option --jwt-secret ignored; requires --authN=builtin.");
        }

        // AuthN, i.e. Authentication
        openkv::OpenKVAuthNMgr authN_mgr;
        std::string authN_delegation_url = "" ;
        if (authN == "none") {
            OES_WARNING("Authentication disabled.");

            authN_mgr = [&ctx](
                const auth::AuthInfo& auth_info, auth::AuthDetails* p_details
            ) {
                const auto auth_hdr = auth_info.authentication_; 
                auth::AuthenticatedUser au;
                std::string connectivity_mode = connectionMode(ctx);

                if (connectivity_mode == "standalone") {
                    // try to get the current user, for AuthZ later.
                    if (!auth_hdr.empty()) {
                        OES_WARNING("Authentication header ignored: --authN none");

                        // We try decoding the header with a dummy JWT secret, that will fail
                        // of course, but give us the jwt_payload, to reparse that as JSON, and
                        // we look for the "username" key, i.e. the same "claim" as our JWT processing.
                        auth::AuthDetails local_details;
                        auth::AuthDetails& details = p_details? *p_details: local_details;
                        au = auth::AuthenticatedUser::decodeFromHeader(
                            auth_hdr, "jwt-secret", &details
                        );
                        auth::AuthType prev_type = au.type_;

                        if (!au.ok_ && !details.jwt_payload_.empty()) {
                            au = auth::AuthenticatedUser::decodeFromJson(
                                details.jwt_payload_, false, &details
                            );
                            au.type_ = prev_type; // otherwise eUnknown overrides eBearer
                        }
                    }
                    if (au.name_.empty()) {
                        au.name_ = "nobody"; // failed to get username from auth_hdr
                        au.email_ = "nobody";
                    }
                }
                else if (connectivity_mode == "osdu") {
                    au = auth::AuthenticatedUser::getDefaultOsduUser();
                } else {
                    au.name_ = "nobody"; // failed to get username from auth_hdr
                    au.email_ = "nobody";
                }

                au.since_ = std::chrono::system_clock::now();
                au.read_only_ = false; // No authN, leave it to authZ (if any)
                au.header_ = auth_hdr;
                au.ok_ = true;
                return au;
            };
        } else if (authN == "builtin") {
            if (jwtSecret.empty()) {
                OES_WARNING("No --jwt-secret: Bearer authentication disabled (Basic only).");
            }

            // TODO(unknown): Use Repo's users table, instead of hard-coded names below
            std::string username = "foo";
            std::string password = "bar";
            openkv::OpenKVUserMgr user_mgr =
                [usr = username, pwd = password]
                (const std::string& username, const std::string& password) {
                    return (username == usr) && (password == pwd);
                }
            ;
            authN_mgr = openkv::builtinAuthMgr(user_mgr, jwtSecret);
        } else if (boost::starts_with(authN, "delegate=")) { // authN is delegate=URL
            auto delegate_url = authN.substr( authN.find_first_of('=') + 1 );
            const auto url_length = delegate_url.length();
            if( url_length >= 2 &&
                ((delegate_url[0] == '\'' && delegate_url[url_length - 1] == '\'') ||
                 (delegate_url[0] == '"' && delegate_url[url_length - 1] == '"'))
            ) {
                delegate_url = delegate_url.substr(1, url_length-1);
            }
            
            auth::HTTPFactory http_factory = []{
                return std::make_shared<oes::core::HTTP>();
            };

            // test: the server is able to reach the given Delegate endpoint - throws if fail
            auth::DelegateClient::testDelegateUrl_get(delegate_url, http_factory(), 401);

            OES_INFO("Authentication delegated to {}", delegate_url);
            authN_delegation_url = delegate_url ;
            auto dbm = databaseMode(ctx);
            try {
                dbm = databaseMode(ctx); // single or multiple
            } catch(const std::exception& ex) {
                OES_WARNING("openETPServer: {}", ex.what());
                OES_WARNING("Database mode unknown, assuming multiple.");
                dbm = "multiple" ;
            } catch(...) {
                OES_WARNING("Database mode unknown, assuming multiple.");
                dbm = "multiple" ;
            }

            authN_mgr = [dbm, delegate_url, http_factory](const auth::AuthInfo& auth_info, auth::AuthDetails* p_details) {
                auth::DelegateClientConfig del_config;
                del_config.DelegateUrl_ = delegate_url;
                del_config.HTTP_factory_ = http_factory;
                
                auto error_on_missing_partition = dbm == "multiple" ;
                auth::DelegateClient del_client(del_config);
                return del_client.getUser(auth_info, p_details, error_on_missing_partition);
            };
        }

        // AuthZ, i.e. Authorization
        openkv::EntitlementMgr entitlement_mgr;
        if (authZ == "none") {
            if (!ctx.args["authZ"].defaulted()) {
                OES_WARNING("Authorization disabled.");
            }
        } else if (authZ == "builtin") {
            OES_WARNING("No built-in authorization yet.");
        } else if (boost::starts_with(authZ, "delegate=")
                   || boost::starts_with(authZ, "policyDelegate=")) {
            const auto authZ_mode = boost::starts_with(authZ, "delegate=") ? 
                oes::common::auth::EntitlementType::Groups :
                oes::common::auth::EntitlementType::Policy;

            if (authZ_mode == oes::common::auth::EntitlementType::Groups && ctx.appConfig().authorizationMode() == "simple") {
                OES_ERROR("Cannot start server with authZ delegated to group based authorization and in simple authorization mode.");
                return false;
            }

            std::string delegateUrl = authZ.substr( authZ.find_first_of('=') + 1 );
            const size_t length = delegateUrl.length();
            // Remove first and last quote if any
            if( length>=2 &&
                ((delegateUrl[0]=='\'' && delegateUrl[length-1]=='\'') ||
                 (delegateUrl[0]=='"' && delegateUrl[length-1]=='"'))
            ) {
                delegateUrl = delegateUrl.substr(1, length-1);
            }

            auto conn_mode = connectionMode(ctx);
            auto ent_config = oes::osdu::api::EntitlementConfigBase::make(delegateUrl, ctx.appConfig(), conn_mode);
            
            auth::HTTPFactory http_factory = []{
                return std::make_shared<oes::core::HTTP>();
            };

            // Test endpoint if policy based authorization
            if (authZ_mode == oes::common::auth::EntitlementType::Policy) {
                auth::DelegateClient::testDelegateUrl_post(delegateUrl + ent_config->evaluatePolicyPath(), http_factory(), 401);
                OES_INFO("Authorization delegated to {}", delegateUrl);
                OES_INFO(" => Eval url: {}", delegateUrl + ent_config->evaluatePolicyPath());
            }

            entitlement_mgr = [ent_config, http_factory, authZ_mode, conn_mode, &ctx] {
                auth::EntitlementConfig entitlement_config;
                entitlement_config.HTTP_factory_ = http_factory;
                entitlement_config.osdu_ = ent_config;
                entitlement_config.osdu_->setConnectivityMode(conn_mode);
                
                return auth::entitlement_factory(authZ_mode, entitlement_config);
            };
            
        } else {
            OES_ERROR("Invalid --authZ: Use 'none', 'builtin', 'delegate=URL', or 'policyDelegate=URL'");
            return false;
        }

        openkv::OpenKVSpaceResolver space_resolver = []
            (const std::string& relative_dbfile, bool new_db) -> std::string {
                // See also how new_space.dbfile_ is assigned in this file,
                // to be relative to main-folder *and* using "unix path"
                OES_UNUSED(new_db);
                return relative_dbfile;
            }
        ;

        oes::eml::etp12::ServerConfig config = openkv::makeServerConfig(
            dbConnectionPool, std::move(authN_mgr), authN_delegation_url, nullptr, std::move(space_resolver),
            std::move(entitlement_mgr), ctx.appConfig_ptr()
        );

        config.port_ = ctx.args["port"].as<int>();
        config.reuseAddressSocket_ = ctx.args["reuseAddressSocket"].as<bool>();
        config.stopOnLastDisconnection_ = ctx.args["stopOnLastDisconnection"].as<bool>();
        config.useIpV4_ = ctx.args["ipv4"].as<bool>();
        config.maxNumberConnection_ = ctx.args["maxConnections"].as<int>();
        config.dataPartitionMode_ = databaseMode(ctx);
        config.dataConnectivityMode_ = connectionMode(ctx);
        config.check_activities_ = ctx.args["check-activities"].as<bool>();

        const std::string& mms_spec = ctx.stringArg("max-msg-size");
        if (!mms_spec.empty()) { 
            try {
                auto [zsize, usize] = parseByteSizes(mms_spec);
                config.max_mgs_size_ = zsize;
                config.max_mgs_usize_ = usize; // Warning: Not ETP standard
            } catch (const std::runtime_error& ex) {
                OES_ERROR("Invalid --max-msg-size option: {}", ex.what());
                return false;
            }
        }

        // Warning: NOT ETP standard. Used to exist, but was eventually removed, in favor
        // of purely max_mgs_size-driven splitting of messages into multi-part responses.
        const int max_batch_size = ctx.args["max-batch-size"].as<int>();
        if (!(0 <= max_batch_size && max_batch_size <= 10'000) && (max_batch_size != 1'000'000)) {
            OES_ERROR("Invalid --max-batch-size: Not in [0 - 10,000] range");
            return false;
        }
        config.max_batch_size_ = max_batch_size;

        if (ctx.hasArg("jobs")) {
            // single-threaded when -j is not explicitly specified
            int64_t jobs = ctx.args["jobs"].as<int64_t>();
            OES_ASSERT(jobs >= 0, jobs = 1;);
            config.totalThreadCount_ = static_cast<unsigned int>(jobs);
        }

        if (ctx.hasArg("etp-trace")) {
            config.enable_tracing_ = true;
            config.trace_file_ = ctx.stringArg("etp-trace");
        }

        oes::eml::etp12::Server server(config);
        data12::ServerCapabilities svr_info = server.getServerInfo();
        OES_INFO("{} {}", svr_info.m_applicationName, svr_info.m_applicationVersion);
        return server.start();
    }

    //============================================================================

    bool alt_etp12_openkv_init_admin_db(const EmlAppContext& ctx) {
        auto dbConnectionPool = std::make_shared<oes::postgresql::DbConnectionPool>();
        if (!initConnectionPool(ctx, dbConnectionPool)) {

            return false;
        }

        for (auto const& connectionElement: dbConnectionPool->getRegisteredDatabases()) {
            auto dbConnection = dbConnectionPool->tryAcquireConnection(connectionElement.first);
            openkv::OpenKVAdminDB_ptr adminDb = std::make_shared<openkv::OpenKVAdminDB>(dbConnection);
            initDatabase(ctx, adminDb);
        }
        return true;
    }

    //============================================================================

    bool alt_etp12_openkv_check_admin_db(const EmlAppContext& ctx) {
        auto dbConnectionPool = std::make_shared<oes::postgresql::DbConnectionPool>();
        if (!initConnectionPool(ctx, dbConnectionPool)) {
            return false;
        }

        for (auto const& connectionElement: dbConnectionPool->getRegisteredDatabases())
        {
            auto dbConnection = dbConnectionPool->tryAcquireConnection(connectionElement.first);
            auto admin_db = std::make_shared<openkv::OpenKVAdminDB>(dbConnection);
            if (isDatabaseReady(ctx, admin_db, connectionElement.first)) {
                OES_INFO( "OK. " + connectionElement.first +  " DB schema up to date.");
            } else {
                OES_ERROR( "Cannot start server: Repo database "
                          + connectionElement.first + " not initialized.");
            }
        }

        return true;
    }

//============================================================================

} //namespace

namespace oes::apps::eml {

/****************************************************************************/

int server_main(EmlAppContext& ctx, const UnparsedArgs& unparsed_args) {

    po::options_description cmd_desc(
        "\nUsage: openETPServer [global-options] " + ctx.command + " --start -d conn_string [options]\n"
        "\nActions", ctx.terminalWidth_
    );
    cmd_desc.add_options()
        ("help,h", "Display this information")
        ("init", "Initialize main DB")
        ("check", "Check main DB")
        ("start", "Start ETP 1.2 server")
    ;

    po::options_description opt_desc("Options", ctx.terminalWidth_);
    opt_desc.add_options()
        ("port,P",
            po::value<int>()->default_value(9002),
            "Specify port to listen to")
        ("reuseAddressSocket",
            po::value<bool>()->default_value(true),
            "Reuse socket address. Allows reusing socket port\n"
            "shortly after previous server instance stopped.")
        ("stopOnLastDisconnection",
            po::value<bool>()->default_value(true),
            "Stop when last client disconnect")
        ("ipv4",
            po::value<bool>()->default_value(false),
            "Use ipv4 protocol instead of ipv6")
        ("maxConnections",
            po::value<int>()->default_value(-1),
            "Specify maximum number of connections")

        ("db-connection,d", txtArg(), 
            "PostgreSQL connection string (If not defined, will use POSTGRESQL_CONN_STRING environment variable")
        ("space-path,s",    txtArg(), "ETP dataspace name (folder or folder/project)")
        ("space-guid",   txtArgOpt(), "ETP dataspace guid (uses path if omitted)")

        ("xdata-file", txtArg(), "New Space meta-data as JSON file (on EPC Direct-import)")
        ("xdata", txtArg(), "New Space meta-data as JSON (on EPC Direct-import)")
        ("vacuum",               "Vacuum (i.e. optimize) DB, post EPC Direct-import")

        ("no-default-space", "Omit ETP 1.2 mandatory 'default' dataspace on --init")

        ("overwrite", "Overwrite existing EPC on Direct-import")

        ("check-activities", 
            po::value<bool>()->default_value(true),
            "Check if new data has corresponding activity")

        // Auth-related options
        ("auth",    txtArg(),           "Deprecated: Legacy name of authN")
        ("authN,N", txtArg("builtin"),  "Authentication: 'none', 'builtin', or 'delegate=URL'")
        ("authZ",   txtArg("none"),     "Authorization:  'none', 'builtin', 'delegate=URL', or 'policyDelegate=URL'")
        ("jwt-secret,k", txtArg(),      "JWT Signing Key (--authN=builtin only).")

        ("jobs,j",
            greedy_value<int64_t>()->implicit_value(0),
            "How many worker-threads:\n"
            "* not specified: Defaults to single-threaded;\n"
            "* -j: Use as many threads as hardware cores;\n"
            "* -jN: Use exactly N threads")

        ("force", "Continue instead of exit, on some warnings/errors")
    ;

    EtpDefaults defaults;
    defaults.max_msg_size_ = "16MB, 128MB";
    defaults.max_batch_size_ = 1'000'000; // I.e. disable it for now, by default
    defaults.compress_ = true;

    cmd_desc.add(opt_desc);
    cmd_desc.add(connect_options(ctx.terminalWidth_, defaults));

    if (int rc = ctx.reparse(cmd_desc, po::positional_options_description(), unparsed_args)) {
        return (rc == -1)? 0: rc;
    }

    EmlAppNamedAction actions[] {
        { "init",        &alt_etp12_openkv_init_admin_db },
        { "check",       &alt_etp12_openkv_check_admin_db },
        { "start",       &alt_etp12_openkv_start },
    };
    return ctx.dispatchActions(actions);
}

} // namespace oes::apps::eml
