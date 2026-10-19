// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/testing/eml/ServerFixture.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/TestConfig.h>

#include <oes/common/auth/AuthN.h>
#include <oes/common/auth/JWToken.h>

#include <oes/eml/openkv/ServerConfig.h>
#include <oes/eml/openkv/DiscoProto.h>

namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
namespace auth = oes::common::auth;

//============================================================================

BOOST_FIXTURE_TEST_SUITE(ServerConfigTests, oes::testing::eml::OpenKVServerFixture)

BOOST_AUTO_TEST_CASE(test_server_config_factories) {
    std::string username = "foo";
    std::string password = "bar";
    OpenKVUserMgr usr_mgr =
        [usr=username, pwd=password]
        (const std::string& username, const std::string& password) {
            return (username == usr) && (password == pwd);
        }
    ;

    OpenKVSpaceResolver space_resolver =
        [db = eml_db_filename_]
        (const std::string& relative_dbfile, bool) -> std::string {
            OES_UNUSED(relative_dbfile);
            return db;
        }
    ;

    // TODO: Will need admin_db (1st arg below) for auth eventually
    std::string jwtSecret = "blah blah blah";
    auto auth_mgr = openkv::builtinAuthMgr(usr_mgr, jwtSecret);
    auto test_config = oes::testing::TestConfig::make();
    ServerConfig config = openkv::makeServerConfig(
        connection_pool_, std::move(auth_mgr), "", nullptr, space_resolver, nullptr, test_config
    );

    Handshake shaker;
    auth::JwtObject jwt;
    auth::JwtOutcome out = jwt.bearerSignature(
        jwtSecret, username, password
    );
    BOOST_REQUIRE(out.ok());
    shaker.authorization_ = out.result();
    shaker.jwt_secret_ = jwtSecret;

    // test the session factory, i.e. authentication
    std::shared_ptr<Session> session;
    session = config.session_factory_(shaker, nullptr);
    BOOST_REQUIRE(session);

    auto alt_openkv_session = std::dynamic_pointer_cast<OpenKVSession>(session);
    BOOST_REQUIRE(alt_openkv_session);

    auth::AuthenticatedUser_ptr user = config.user_factory_(shaker);
    BOOST_REQUIRE(user);
    BOOST_CHECK_EQUAL(username, user->name_);

    // Not Direct-instantiating the Protocol implementation,
    // to test the config's protocol_factory_ too.
    data12::SupportedProtocol req_proto;
    req_proto.m_protocol = int(data12::Protocol::eDiscovery);
    req_proto.m_role = "store";
    data12::DataValue value;
    value.m_item = true;
    proto12::IProtocol_ptr iproto = config.protocol_factory_(
        req_proto, alt_openkv_session
    );
    BOOST_REQUIRE(iproto);

    BOOST_CHECK_EQUAL(10, config.supportedProtocols_.size());

    auto* p_disco = dynamic_cast<OpenKVDiscovery*>(iproto.get());
    BOOST_REQUIRE(p_disco);

    OpenKVDiscovery& disco = *p_disco;

    // Warning: get_rpc.proto_ is a separate new instance from disco above
    // That's why we .call(disco) below, and not .call(), to really test the
    // disco-protocol instance the factory above created.
    auto get_rpc = makeRpc(&OpenKVDiscovery::GetResources);

    proto12::Discovery::GetResources& body = get_rpc.req_;
    const EmlUri root_uri = EmlUri::make_space();
    body.m_context.m_uri = root_uri.to_string();
    body.m_context.m_depth = 1; // i.e. Dataspaces only

    BOOST_REQUIRE(!get_rpc.call(disco)); // see above why not just .call()
    BOOST_REQUIRE(!get_rpc.rc_.ok); // since no admin_db_ in config, normal
}

BOOST_AUTO_TEST_SUITE_END() // ServerConfigTests

//============================================================================

} // End anonymous namespace
