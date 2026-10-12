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

#include "JWToken.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <jwt/jwt.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <algorithm>
#include <iterator>

namespace {

using namespace oes::common::auth;

JwtOutcome jwtTryCatch(std::function<JwtOutcome()> f) {
    try {
        return f();
    } catch (const jwt::MemoryAllocationException&ex) { return { JwtMemAllocError, ex.what() };
    } catch (const jwt::SigningError             &ex) { return { JwtSigningError, ex.what() };

    } catch (const jwt::SignatureFormatError     &ex) { return { JwtSignatureFormatError, ex.what() };
    } catch (const jwt::KeyNotPresentError       &ex) { return { JwtKeyNotPresentError, ex.what() };
    } catch (const jwt::DecodeError              &ex) { return { JwtOtherDecodeError, ex.what() };

    } catch (const jwt::InvalidAlgorithmError    &ex) { return { JwtInvalidAlgorithmError, ex.what() };
    } catch (const jwt::TokenExpiredError        &ex) { return { JwtTokenExpiredError, ex.what() };
    } catch (const jwt::InvalidIssuerError       &ex) { return { JwtInvalidIssuerError, ex.what() };
    } catch (const jwt::InvalidAudienceError     &ex) { return { JwtInvalidAudienceError, ex.what() };
    } catch (const jwt::InvalidSubjectError      &ex) { return { JwtInvalidSubjectError, ex.what() };
    } catch (const jwt::InvalidIATError          &ex) { return { JwtInvalidIATError, ex.what() };
    } catch (const jwt::InvalidJTIError          &ex) { return { JwtInvalidJTIError, ex.what() };
    } catch (const jwt::ImmatureSignatureError   &ex) { return { JwtImmatureSignatureError, ex.what() };
    } catch (const jwt::InvalidSignatureError    &ex) { return { JwtInvalidSignatureError, ex.what() };
    } catch (const jwt::TypeConversionError      &ex) { return { JwtTypeConversionError, ex.what() };
    } catch (const jwt::VerificationError        &ex) { return { JwtOtherVerificationError, ex.what() };

    } catch (const std::exception                &ex) { return { JwtUnknownError, ex.what() };
    } catch (...)                                     { return { JwtUnknownError, "Unknown Error" };
    }
}

} // namespace

namespace oes::common::auth {

struct JwtObject::Impl {
    jwt::jwt_object  obj_;
    jwt::jwt_payload pay_;
};

JwtObject::JwtObject(
) : impl_(new JwtObject::Impl())
{
}

JwtObject::~JwtObject() {
    delete impl_;
}

JwtOutcome JwtObject::encode(
    const std::string& jwt_secret,
    const std::string& username, const std::string& password,
    const std::string& algo
) {
    return jwtTryCatch([&]() -> JwtOutcome {
        using namespace jwt::params;
        impl_->obj_ = jwt::jwt_object{
            algorithm(algo), payload({
                { "username", username },
                { "password", password }
            }), secret(jwt_secret)
        };
        return { JwtOK, impl_->obj_.signature() };
    });
}

JwtOutcome JwtObject::decode(
    const std::string& token, const std::string& jwt_secret,
    const std::string& algo, bool jwt_verify
) {
    return jwtTryCatch([&]() -> JwtOutcome {
        using namespace jwt::params;
        impl_->obj_ = jwt::decode(
            token, algorithms({ algo }), secret(jwt_secret), verify(jwt_verify)
        );
        impl_->pay_ = impl_->obj_.payload();
        return { JwtOK, "" };
   });
}

JwtOutcome JwtObject::decode(
    const std::string& token, const std::string& jwt_secret,
    std::string& username, std::string& password,
    const std::string& algo, bool jwt_verify
) {
    JwtOutcome outcome = decode(token, jwt_secret, algo, jwt_verify);
    if (outcome) {
        outcome = getPayloadStringClaim("username");
        if (outcome) {
            username = outcome.result();
            outcome.result().clear();

            outcome = getPayloadStringClaim("password");
            if (outcome) {
                password = outcome.result();
                outcome.result().clear();
            }
        }
    }
    return outcome;
}

JwtOutcome JwtObject::bearerSignature(
    const std::string& jwt_secret,
    const std::string& username, const std::string& password
) {
    JwtOutcome outcome = encode(jwt_secret, username, password);
    if (outcome) {
        outcome.second = "Bearer " + outcome.second;
    }
    return outcome;
}

JwtOutcome JwtObject::getPayloadStringClaim(const std::string& claim_key) const {
    return jwtTryCatch([&]() -> JwtOutcome {
        std::string value = impl_->pay_.get_claim_value<std::string>(claim_key);
        return { JwtOK, std::move(value) };
    });
}

} // namespace oes::common::auth
