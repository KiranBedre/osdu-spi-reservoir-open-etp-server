// ============================================================================
// Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#pragma once

#include <string>
#include "BaseTokenClient.h"

namespace oes::providers::core
{
    class OES_PROVIDERS_EXPORT OAuthTokenClient final : public oes::providers::core::BaseTokenClient {
        public:
            OAuthTokenClient(
                const std::string& token_url,
                const std::string& client_id,
                const std::string& client_secret,
                const std::string& oauth_scope,
                const std::string& grant_type
            );
            std::string getAccessToken() const override;

        private:
            void generateAuthHeader(std::string& auth_header) const;
            int generateRequestBody(std::string& request_body) const;

            std::string token_url_;
            std::string client_id_;
            std::string client_secret_;
            std::string oauth_scope_;
            std::string grant_type_;
    };
}
