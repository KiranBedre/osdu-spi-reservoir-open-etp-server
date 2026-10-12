// Copyright 2024 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <string>
#include <memory>
#include <functional>

#include "AuthN.h"
#include <oes/core/http/HTTP.h>

#ifndef DELEGATE_CLIENT_H
#define DELEGATE_CLIENT_H

namespace oes::common::auth {

using HTTP_ptr = std::shared_ptr<oes::core::HTTP>;
using HTTPFactory = std::function<HTTP_ptr()>;


struct OES_COMMON_EXPORT DelegateClientConfig {
    HTTPFactory HTTP_factory_;
    std::string DelegateUrl_;
};

class OES_COMMON_EXPORT DelegateClient {
public:
    explicit DelegateClient(const DelegateClientConfig& config);
    AuthenticatedUser getUser(const AuthInfo& auth_info, AuthDetails* p_details, bool error_on_missing_partition) const;

    static bool testDelegateUrl_get(const std::string& del_url, HTTP_ptr http_client, const short expected_code);
    static bool testDelegateUrl_post(const std::string& del_url, HTTP_ptr http_client, const short expected_code);
private:
    std::string DelegateUrl_;
    HTTP_ptr httpReq_;
};

} // namespace oes::common::auth

#endif // DELEGATE_CLIENT_H