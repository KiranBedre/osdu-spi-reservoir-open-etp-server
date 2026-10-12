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
#ifndef OES_COMMON_AUTH_JWTOKEN_H
#define OES_COMMON_AUTH_JWTOKEN_H

#include "../export.h"

#include <string>
#include <functional>

namespace oes::common::auth {

using JwtDecryptAuthFunc = std::function<
    bool(const std::string& jwt, std::string& username, std::string& password)
>;

enum JwtErrorClass {
    JwtOpenSslErrorClass       = (1<<10),
    JwtDecodeErrorClass        = (1<<11),
    JwtVerificationErrorClass  = (1<<12)
};

enum JwtError {
    JwtOK = 0,
    JwtUnknownError = 1,

    JwtMemAllocError            =  1 + JwtOpenSslErrorClass,
    JwtSigningError             =  2 + JwtOpenSslErrorClass,

    JwtOtherDecodeError         =  0 + JwtDecodeErrorClass,
    JwtSignatureFormatError     =  1 + JwtDecodeErrorClass,
    JwtKeyNotPresentError       =  2 + JwtDecodeErrorClass,

    JwtOtherVerificationError   =  0 + JwtVerificationErrorClass,
    JwtInvalidAlgorithmError    =  1 + JwtVerificationErrorClass,
    JwtTokenExpiredError        =  2 + JwtVerificationErrorClass,
    JwtInvalidIssuerError       =  3 + JwtVerificationErrorClass,
    JwtInvalidAudienceError     =  4 + JwtVerificationErrorClass,
    JwtInvalidSubjectError      =  5 + JwtVerificationErrorClass,
    JwtInvalidIATError          =  6 + JwtVerificationErrorClass,
    JwtInvalidJTIError          =  7 + JwtVerificationErrorClass,
    JwtImmatureSignatureError   =  8 + JwtVerificationErrorClass,
    JwtInvalidSignatureError    =  9 + JwtVerificationErrorClass,
    JwtTypeConversionError      = 10 + JwtVerificationErrorClass
};

using JwtOutcome_base_t = std::pair<JwtError, std::string>;

struct OES_COMMON_EXPORT JwtOutcome : JwtOutcome_base_t {
    using JwtOutcome_base_t::pair; // inherit pair's constructors

    bool ok() const { return err() == JwtOK; }
    explicit operator bool() const { return ok(); }

    JwtError err() const { return first; }

    std::string& result() { return second; }
    const std::string& result() const { return second; }
};

class OES_COMMON_EXPORT JwtObject {
public:
    JwtObject();
    ~JwtObject();

    JwtObject(const JwtObject&) = delete;
    JwtObject& operator=(const JwtObject&) = delete;

    JwtOutcome encode(
        const std::string& jwt_secret,
        const std::string& username, const std::string& password,
        const std::string& algo = "hs256"
    );
    JwtOutcome decode(
        const std::string& token, const std::string& jwt_secret,
        std::string& username, std::string& password,
        const std::string& algo = "hs256", bool jwt_verify = true
    );

    JwtOutcome decode(
        const std::string& token, const std::string& jwt_secret,
        const std::string& algo = "hs256", bool jwt_verify = true
    );

    JwtOutcome bearerSignature(
        const std::string& jwt_secret,
        const std::string& username, const std::string& password
    );

    JwtOutcome getPayloadStringClaim(const std::string& claim_key) const;

private:
    struct Impl;
    Impl*const impl_;
};

} // namespace oes::common::auth

#endif // OES_COMMON_AUTH_JWTOKEN_H
