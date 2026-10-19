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
#ifndef OES_COMMON_AUTH_AUTHN_H
#define OES_COMMON_AUTH_AUTHN_H

#include "../export.h"

#include <chrono>
#include <string>

namespace oes::common::auth {

enum class AuthType : unsigned char {
    eUnknown,
    eBasic,
    eBearer,
    eOsduToken
};

// Struct to pass Auth information for authentication process 
struct OES_COMMON_EXPORT AuthInfo {
    std::string authentication_;
    std::string partition_id_;

    AuthInfo(const std::string& auth, const std::string& partitionId);
    AuthInfo() = default;
};

// for debugging / troubleshooting
struct OES_COMMON_EXPORT AuthDetails {
    std::string err_msg_;

    std::string basic_decoded_; // empty unless type_ is AuthType::eBasic

    std::string jwt_header_;    // empty unless type_ is AuthType::eBearer
    std::string jwt_payload_;   // for access to all claims

    std::string client_id_;
};

struct OES_COMMON_EXPORT AuthenticatedUser {
    bool ok_ = false;

    bool read_only_ = true;
    bool bypass_authZ_ = false;

    AuthType type_ = AuthType::eUnknown;

    std::string uuid_;
    std::string email_;
    std::string name_;
    std::string pswd_;  // possibly encrypted or empty (e.g. not recorded)
    std::string role_;
    std::string partition_id_; // Might be in the token.
    std::string header_;        // input Authentication HTTP header (or query-string)
    std::string ip_address_;    // of remote endpoint being authenticated

    std::chrono::system_clock::time_point since_;
    std::chrono::system_clock::time_point expires_at_;

    bool equals(const AuthenticatedUser& rhs) const;

    static AuthenticatedUser withBasicAuth(
        std::string username, std::string password
    );
    static AuthenticatedUser withJwtAuth(
        std::string username, std::string password,
        const std::string& jwtSecret // used to crypto-sign the resulting token
    );

    static AuthenticatedUser decodeFromHeader(
        std::string authentication_header,
        const std::string& jwt_secret = {},
        AuthDetails* details = nullptr
    );

    static AuthenticatedUser decodeFromJson(
        const std::string& json_body,
        bool error_on_unknown_key = true,
        AuthDetails* details = nullptr
    );

    static AuthenticatedUser getDefaultOsduUser();
};

inline bool operator==(const AuthenticatedUser& lhs, const AuthenticatedUser& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const AuthenticatedUser& lhs, const AuthenticatedUser& rhs) {
    return !lhs.equals(rhs);
}

OES_COMMON_EXPORT
std::string base64Decode(const std::string& base64_str);

OES_COMMON_EXPORT
std::string base64Encode(const std::string& binary_str);

OES_COMMON_EXPORT
std::string base64urlDecode(std::string base64url_str);

OES_COMMON_EXPORT
std::string base64urlEncode(const std::string& binary_str);

} // namespace oes::common::auth

#endif // OES_COMMON_AUTH_AUTHN_H
