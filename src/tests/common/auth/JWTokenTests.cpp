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

#include <oes/common/auth/JWToken.h>

#include <oes/core/utils/Guid.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::common::auth;

namespace {

BOOST_AUTO_TEST_SUITE(JWTokenTests)

BOOST_AUTO_TEST_CASE(test_jwt_roundtrips) {
    using oes::core::Guid;

    std::string secret = Guid::make_deterministic("foobar").toXmlString();
    std::string username = "Albert Einstein";
    std::string password =  Guid::make_deterministic("Mushroom Boom").toStringNoDash();

    JwtObject jwtObjEncoder;
    JwtOutcome outcome = jwtObjEncoder.encode(secret, username, password);
    JwtOutcome outcome2 = jwtObjEncoder.bearerSignature(secret, username, password);
    BOOST_REQUIRE_EQUAL(JwtOK, outcome.first);
    BOOST_REQUIRE_EQUAL(JwtOK, outcome.err());
    BOOST_REQUIRE(outcome2);

    std::string expected_token =
        "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9" // part1
        "."
        "eyJwYXNzd29yZCI6ImVjYWRiMTgyNGM3MTUxM2M4ZWVhMjEwMGI2MTUyNzg1IiwidXNlcm5hbWUiOiJBbGJlcnQgRWluc3RlaW4ifQ" // part2
        "."
        "oS2rOTDk18F_7T47DeXkaOLlRndfJ0h4VvTH2qt-U98" // part3
    ;
    std::string token = outcome.second;
    std::string bearer_token = outcome2.second;
    BOOST_CHECK_EQUAL("Bearer " + token, bearer_token);
    BOOST_CHECK_EQUAL(token, expected_token);

    JwtObject jwtObjDecoder;
    outcome = jwtObjDecoder.decode(token, secret);
    BOOST_REQUIRE_EQUAL(JwtOK, outcome.first);

    outcome = jwtObjDecoder.getPayloadStringClaim("username");
    BOOST_REQUIRE_EQUAL(JwtOK, outcome.first);
    BOOST_REQUIRE_EQUAL(username, outcome.second);

    outcome = jwtObjDecoder.getPayloadStringClaim("password");
    BOOST_REQUIRE_EQUAL(JwtOK, outcome.first);
    BOOST_REQUIRE_EQUAL(password, outcome.second);

    std::string out_username;
    std::string out_password;
    JwtObject jwtObjDecoder2;
    JwtOutcome outcome3 = jwtObjDecoder2.decode(
        token, secret, out_username, out_password
    );
    BOOST_REQUIRE(outcome3);
    BOOST_REQUIRE(outcome3.result().empty());
    BOOST_REQUIRE_EQUAL(username, out_username);
    BOOST_REQUIRE_EQUAL(password, out_password);
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
