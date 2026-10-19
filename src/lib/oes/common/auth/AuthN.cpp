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

#include "AuthN.h"
#include "JWToken.h"

#include <oes/core/utils/Json.h>

#define BOOST_BEAST_HEADER_ONLY 1
#define BOOST_BEAST_USE_STD_STRING_VIEW 1

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/beast/core/detail/base64.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cassert>
#include <stdexcept>
#include <tuple>

namespace oes::common::auth {

AuthInfo::AuthInfo(const std::string& auth, const std::string& partitionId) {
    authentication_ = auth;
    partition_id_ = partitionId;
}

bool AuthenticatedUser::equals(const AuthenticatedUser& rhs) const {
    const AuthenticatedUser& lhs = *this;
    return std::tie(
        lhs.ok_, lhs.type_, lhs.name_, lhs.pswd_, lhs.role_, lhs.partition_id_,
        lhs.header_, lhs.ip_address_, lhs.since_, lhs.expires_at_
    ) == std::tie(
        rhs.ok_, rhs.type_, rhs.name_, rhs.pswd_, rhs.role_, rhs.partition_id_,
        rhs.header_, rhs.ip_address_, rhs.since_, rhs.expires_at_
    );
}

AuthenticatedUser AuthenticatedUser::withBasicAuth(
    std::string username, std::string password
) {
    AuthenticatedUser au;

    au.header_ = "Basic " + base64Encode(username + ":" + password);
    au.name_ = std::move(username);
    au.pswd_ = std::move(password);
    au.type_ = AuthType::eBasic;
    au.ok_ = true;

    return au;
}

AuthenticatedUser AuthenticatedUser::withJwtAuth(
    std::string username, std::string password,
    const std::string& jwtSecret
) {
    AuthenticatedUser au;

    JwtObject jwtObj;
    auto rc = jwtObj.bearerSignature(jwtSecret, username, password);
    if (rc.first != JwtOK) {
        assert(rc.first == JwtOK);
        assert(!au.ok_);
        return au;
    }

    au.header_ = rc.second;
    au.name_ = std::move(username);
    au.pswd_ = std::move(password);
    au.type_ = AuthType::eBearer;
    au.ok_ = true;

    return au;
}

AuthenticatedUser AuthenticatedUser::getDefaultOsduUser() {
    AuthenticatedUser au;
    au.type_ = AuthType::eOsduToken;
    au.name_ = "osdu_user";
    au.email_ = "osdu_user";
    au.pswd_ = "unknown";
    return au;
}

AuthenticatedUser AuthenticatedUser::decodeFromHeader(
    std::string authentication_header,
    const std::string& jwt_secret,
    AuthDetails* details
) {
    AuthenticatedUser au;
    au.header_ = std::move(authentication_header);

    const char* err = nullptr;
    const std::string& auth_hdr = au.header_;

    if (auth_hdr.empty()) {
        err = "No Authorization HTTP header (or URL query-string)";
    } else if (boost::starts_with(auth_hdr, "Basic")) {
        au.type_ = AuthType::eBasic;

        std::string auth = auth_hdr.substr(6);
        std::string token = base64Decode(auth);

        if (details) {
            details->basic_decoded_ = token;
        }

        size_t pos = token.find(':');
        if (pos < token.size()) {
            au.name_ = token.substr(0, pos);    // Note: can be empty
            au.pswd_ = token.substr(pos + 1);   // Note: can be empty too
            au.ok_ = true;
        } else {
            err = "Basic authentication expects username:password format";
        }
    } else if (boost::starts_with(auth_hdr, "Bearer")) {
        if (!jwt_secret.empty()) {
            au.type_ = AuthType::eBearer;

            std::string auth = auth_hdr.substr(7);
            size_t pos = auth.find('.');
            size_t rpos = auth.rfind('.');
            if (pos > 0 && pos < auth.size() && pos < rpos) {
                std::string header = auth.substr(0, pos);
                std::string payload = auth.substr(pos + 1, (rpos - pos - 1));

                if (details) {
                    // Note use of base64urlDecode(), not base64Decode().
                    // See https://tools.ietf.org/html/rfc7515#section-3.1
                    details->jwt_header_ = base64urlDecode(header);
                    details->jwt_payload_ = base64urlDecode(payload);
                }

                JwtObject jwtObj;
                JwtOutcome outcome = jwtObj.decode(auth, jwt_secret);
                if (outcome.first == JwtOK) {
                    // TODO: Deal with with ETP 1.2 required claims:
                    // Issuer (iss), Subject (sub), Issued at (iat), Expiration time (exp)
                    // and properly manage expirations
                    JwtOutcome usr_outcome = jwtObj.getPayloadStringClaim("username");
                    JwtOutcome pwd_outcome = jwtObj.getPayloadStringClaim("password");

                    if (usr_outcome.first == JwtOK &&
                        pwd_outcome.first == JwtOK
                    ) {
                        au.name_ = usr_outcome.second;
                        au.email_ = au.name_;
                        au.pswd_ = pwd_outcome.second;
                        au.ok_ = true;
                    } else {
                        err = "Required username and password claims missing from JWT";
                    }
                } else {
                    err = "Cannot decode JWT: Invalid format, signature, or secret";
                }
            } else {
                err = "Bearer authentication expects header.payload.signature format";
            }
        } else {
            err = "Bearer authentication disabled (empty secret)";
        }
    } else {
        err = "Missing Basic or Bearer prefix";
    }

    if (au.ok_) {
        assert(!err);
        au.since_ = std::chrono::system_clock::now();
    } else if (details) {
        assert(err);
        details->err_msg_ = err;
    }

    return au;
}

AuthenticatedUser AuthenticatedUser::decodeFromJson(
    const std::string& json_body,
    bool error_on_unknown_key,
    AuthDetails* details
) try {
    AuthenticatedUser au;

    std::string local_err;
    std::string& err_msg = details? details->err_msg_: local_err;

    if (json_body.empty() || json_body == "{}") {
        err_msg = "Empty JSON or Object";
        return au;
    }

    using namespace oes::core;
    JSONReader json(json_body.data(), json_body.size());
    if (json.next() != JSON_OBJECT_BEGIN) {
        err_msg = "Expected JSON Object";
        return au;
    }

    au.ok_ = true;
    auto get_string = [&err_msg, &au, &json](std::string& str, bool or_null) {
        if (json.next() == JSON_STRING) {
            str = json.get_string();
        } else if (or_null && json.current() == JSON_NULL) {
            str.clear();
        } else {
            // unexpected value
            err_msg = "Expected JSON String Value";
            au.ok_ = false;
        }
    };

    std::string user_email;
    std::string sub;
    while (au.ok_ && json.next() == JSON_OBJECT_KEY) {
        if (json.is_key("error")) {
            get_string(err_msg, false);
            au.ok_ = false;
            return au;
        }
        if (json.is_key("name")) {
            get_string(au.name_, false);
        } else if (json.is_key("username") && au.name_.empty()) {
            get_string(au.name_, false);
        }else if (json.is_key("unique_name")) {
            get_string(user_email, false);
        } else if (json.is_key("email") && user_email.empty()) {
            get_string(user_email, false);
        } else if (json.is_key("uuid")) {
            get_string(au.uuid_, false);
        } else if (json.is_key("auth-type")) {
            std::string auth_type;
            get_string(auth_type, true);
            if (auth_type.empty()) {
                au.type_ = AuthType::eUnknown;
            } else if (auth_type == "Basic") {
                au.type_ = AuthType::eBasic;
            } else if (auth_type == "Bearer") {
                au.type_ = AuthType::eBearer;
            }
        } else if (json.is_key("role")) {
            get_string(au.role_, false);
        } else if (json.is_key("partitionId")) {
            get_string(au.partition_id_, false);
        } else if (json.is_key("read-only")) {
            if (json.next() == JSON_TRUE) {
                au.read_only_ = true;
            } else if (json.current() == JSON_FALSE) {
                au.read_only_ = false;
            } else {
                err_msg = "Expected JSON Boolean Value";
                au.ok_ = false; // unexpected value
            }
        } else if (json.is_key("sub")) {
            get_string(sub, false);
        } else {
            if (error_on_unknown_key) {
                err_msg = "Unexpected JSON Key: " + json.get_key();
                au.ok_ = false; // unexpected key-value
            } else {
                json.skip_next();
            }
        }

        // Force user to be RW. Having it RO does not make any sense as we will also
        // manage authZ with a delegate.
        au.read_only_ = false;

        au.email_ = user_email;
        if (au.name_.empty()) {
            if (user_email.empty()) {
                au.name_ = sub;
                au.email_ = sub;
            } else {
                au.name_ = user_email;
            }
        }
        // TODO: Expired?
    }

    au.ok_ = au.ok_ &&
        json.current() == JSON_OBJECT_END &&
        json.next() == JSON_END
    ;

    // The only required key is 
    if (au.name_.empty()) {
        err_msg = "Required JSON Key 'username' missing";
        au.ok_ = false; // unexpected key-value
    }

    if (au.ok_) {
        au.since_ = std::chrono::system_clock::now();
    }

    return au;
} catch (const std::exception& ex) {
    if (details) {
        details->err_msg_ = ex.what();
    }
    return {};
}

std::string base64Encode(const std::string& binary_str) {
    using namespace boost::beast::detail;
    std::string encoded(base64::encoded_size(binary_str.size()), '\0');
    auto new_size = base64::encode(encoded.data(), binary_str.data(), binary_str.size());
    encoded.resize(new_size);
    return encoded;
}

std::string base64Decode(const std::string& base64_str) {
    using namespace boost::beast::detail;
    std::string decoded(base64::decoded_size(base64_str.size()), '\0');
    auto rc = base64::decode(decoded.data(), base64_str.data(), base64_str.size());
    decoded.resize(rc.first);
    return decoded;
}

// See https://tools.ietf.org/html/rfc7515#appendix-C
std::string base64urlEncode(const std::string& binary_str) {
    auto e = base64Encode(binary_str);
    while (e.back() == '=') {
        e.resize(e.size() - 1);
    }
    size_t pos = e.find_first_of("+/");
    while (pos != std::string::npos) {
        switch (char& c = e[pos]) {
        case '+': c = '-'; break;
        case '/': c = '_'; break;
        }
        pos = e.find_first_of("+/", pos + 1);
    }
    return e;
}

// See https://tools.ietf.org/html/rfc7515#appendix-C
std::string base64urlDecode(std::string base64url_str) {
    std::string& e = base64url_str;
    size_t pos = e.find_first_of("-_");
    while (pos != std::string::npos) {
        switch (char& c = e[pos]) {
        case '-': c = '+'; break;
        case '_': c = '/'; break;
        }
        pos = e.find_first_of("-_", pos + 1);
    }
    switch (e.size() % 4) {
    case 2: e.push_back('='); [[fallthrough]];
    case 3: e.push_back('='); break;
    }
    return base64Decode(e);
}

} // namespace oes::common::auth
