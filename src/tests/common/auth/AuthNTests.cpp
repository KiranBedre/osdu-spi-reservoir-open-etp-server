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

#include <oes/common/auth/AuthN.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::common::auth;

namespace {

BOOST_AUTO_TEST_SUITE(AuthNTests)

BOOST_AUTO_TEST_CASE(test_empty) {
    auto output = AuthenticatedUser::decodeFromHeader("");
    BOOST_CHECK(!output.ok_);
}

BOOST_AUTO_TEST_CASE(test_unknown_prefix) {
    auto output = AuthenticatedUser::decodeFromHeader("Complex 0g==");
    BOOST_CHECK(!output.ok_);
}

BOOST_AUTO_TEST_CASE(test_basic_empty) {
    auto input = AuthenticatedUser::withBasicAuth("", "");
    auto output = AuthenticatedUser::decodeFromHeader(input.header_);
    BOOST_CHECK_EQUAL(output.name_, "");
    BOOST_CHECK_EQUAL(output.pswd_, "");
    output.since_ = output.expires_at_; // reset .since_ for == below
    BOOST_CHECK(input == output);
}

BOOST_AUTO_TEST_CASE(test_basic_foo_bar) {
    auto input = AuthenticatedUser::withBasicAuth("foo", "bar");
    auto output = AuthenticatedUser::decodeFromHeader(input.header_);
    BOOST_CHECK_EQUAL(output.name_, "foo");
    BOOST_CHECK_EQUAL(output.pswd_, "bar");
    output.since_ = output.expires_at_; // reset .since_ for == below
    BOOST_CHECK(input == output);
}

BOOST_AUTO_TEST_CASE(test_bearer_empty) {
    // Note: secret cannot be empty. AlgorithmErrc::KeyNotFoundErr otherwise.
    const std::string secret("victoria");
    auto input = AuthenticatedUser::withJwtAuth("", "", secret);
    auto output = AuthenticatedUser::decodeFromHeader(input.header_, secret);
    BOOST_CHECK_EQUAL(output.name_, "");
    BOOST_CHECK_EQUAL(output.pswd_, "");
    output.since_ = output.expires_at_; // reset .since_ for == below
    BOOST_CHECK(input == output);
}

BOOST_AUTO_TEST_CASE(test_bearer_foo_bar) {
    // Note: secret cannot be empty. AlgorithmErrc::KeyNotFoundErr otherwise.
    const std::string secret("victoria");
    auto input = AuthenticatedUser::withJwtAuth("foo", "bar", secret);
    auto output = AuthenticatedUser::decodeFromHeader(input.header_, secret);
    BOOST_CHECK_EQUAL(output.name_, "foo");
    BOOST_CHECK_EQUAL(output.pswd_, "bar");
    output.since_ = output.expires_at_; // reset .since_ for == below
    BOOST_CHECK(input == output);
}

BOOST_AUTO_TEST_CASE(test_json_empty) {
    auto output = AuthenticatedUser::decodeFromJson("");
    BOOST_CHECK(!output.ok_);

    output = AuthenticatedUser::decodeFromJson("{}");
    BOOST_CHECK(!output.ok_);
}

BOOST_AUTO_TEST_CASE(test_json_error) {
    AuthDetails details;
    auto output = AuthenticatedUser::decodeFromJson(R"json(

{ "error": "Custom error message" }

    )json", true, &details);
    BOOST_CHECK(!output.ok_);
    BOOST_CHECK_EQUAL(details.err_msg_, "Custom error message");
}

BOOST_AUTO_TEST_CASE(test_json_invalid) {
    AuthDetails details;
    auto output = AuthenticatedUser::decodeFromJson(R"json(

{ "username": "bob, "foo": "BAR" }

    )json", true, &details);
    BOOST_CHECK(!output.ok_);
    BOOST_CHECK_EQUAL(details.err_msg_, "Invalid object: missing comma separator");
}

BOOST_AUTO_TEST_CASE(test_json_missing_user) {
    AuthDetails details;
    auto output = AuthenticatedUser::decodeFromJson(R"json(

{ "role": "Root" }

    )json", true, &details);
    BOOST_CHECK(!output.ok_);
    BOOST_CHECK_EQUAL(details.err_msg_, "Required JSON Key 'username' missing");
}

BOOST_AUTO_TEST_CASE(test_json_unknown_key) {
    AuthDetails details;
    auto output = AuthenticatedUser::decodeFromJson(R"json(

{ "username": "bob", "foo": "BAR" }

    )json", true, &details);
    BOOST_CHECK(!output.ok_);
    BOOST_CHECK_EQUAL(output.name_, "bob"); // still has this
    BOOST_CHECK_EQUAL(details.err_msg_, "Unexpected JSON Key: foo");

    // retry in more forgiving mode
    details.err_msg_.clear();
    output = AuthenticatedUser::decodeFromJson(R"json(

{ "username": "bob", "foo": "BAR" }

    )json", false, &details);
    BOOST_CHECK(output.ok_);
    BOOST_CHECK_EQUAL(output.name_, "bob");
    BOOST_CHECK(details.err_msg_.empty());
}

BOOST_AUTO_TEST_CASE(test_json_just_user) {
    auto output = AuthenticatedUser::decodeFromJson(R"json(

{ "username": "bob" }

    )json");
    BOOST_CHECK(output.ok_);
    BOOST_CHECK_EQUAL(output.name_, "bob");
}

BOOST_AUTO_TEST_CASE(test_json_full) {
    auto output = AuthenticatedUser::decodeFromJson(R"json(

{ "username": "bob", "role": "User", "read-only": false, "auth-type": "Bearer" }

    )json");
    BOOST_CHECK(output.ok_);
    BOOST_CHECK_EQUAL(output.name_, "bob");
    BOOST_CHECK_EQUAL(output.role_, "User");
    BOOST_CHECK_EQUAL(output.read_only_, false);
    BOOST_CHECK(output.type_ == AuthType::eBearer);
}

BOOST_AUTO_TEST_CASE(test_encode_decode_base64) {
    struct {
        const char* decoded;
        const char* encoded;
    } test_cases[] {
        // Verify proper base64 tail-encoding, from https://en.wikipedia.org/wiki/Base64
        { "any carnal pleasure.",   "YW55IGNhcm5hbCBwbGVhc3VyZS4=" },
        { "any carnal pleasure",    "YW55IGNhcm5hbCBwbGVhc3VyZQ==" },
        { "any carnal pleasur",     "YW55IGNhcm5hbCBwbGVhc3Vy" },
        { "any carnal pleasu",      "YW55IGNhcm5hbCBwbGVhc3U=" },
        { "any carnal pleas",       "YW55IGNhcm5hbCBwbGVhcw==" },

        // Emulate Basic auth
        { ":", "Og==" },
        { "foo:bar", "Zm9vOmJhcg==" },

        // Emulate Bearer (JWT) auth, but not really, since has == tail
        { R"({"alg":"HS256","typ":"JWT"})", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9" },
        { R"({"password":"P","username":"U"})", "eyJwYXNzd29yZCI6IlAiLCJ1c2VybmFtZSI6IlUifQ==" },
    };
    for (const auto& tc : test_cases) {
        std::string encoded = base64Encode(tc.decoded);
        std::string decoded = base64Decode(tc.encoded);
        BOOST_CHECK_EQUAL(std::string(tc.decoded), decoded);
        BOOST_CHECK_EQUAL(std::string(tc.encoded), encoded);
    }
}

BOOST_AUTO_TEST_CASE(test_encode_decode_base64url) {
    // From https://tools.ietf.org/html/rfc7515#appendix-A.1.1
    unsigned char HMAC_SHA256_signature[] {
        116,  24, 223, 180, 151, 153, 224,  37,
         79, 250,  96, 125, 216, 173, 187, 186,
         22, 212,  37,  77, 105, 214, 191, 240,
         91,  88,   5,  88,  83, 132, 141, 121,
          0 // null terminator
    };

    struct {
        const char* decoded;
        const char* encoded;
    } test_cases[] {
        // Emulate Bearer (JWT) auth. Note second one lacks == tail
        // See https://tools.ietf.org/html/rfc7515#appendix-C
        { R"({"alg":"HS256","typ":"JWT"})", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9" },
        { R"({"password":"P","username":"U"})", "eyJwYXNzd29yZCI6IlAiLCJ1c2VybmFtZSI6IlUifQ" },

        // A weirder example, to exercise mapping from +/ to -_
        { (const char*)(HMAC_SHA256_signature), "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk" },
    };
    for (const auto& tc : test_cases) {
        std::string encoded = base64urlEncode(tc.decoded);
        std::string decoded = base64urlDecode(tc.encoded);
        BOOST_CHECK_EQUAL(std::string(tc.decoded), decoded);
        BOOST_CHECK_EQUAL(std::string(tc.encoded), encoded);
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
