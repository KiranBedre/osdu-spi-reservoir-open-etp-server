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

#include "../core/BearerTokenProvider.h"
#include "../core/OAuthTokenClient.h"
#include "BearerTokenProviderCImpl.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::providers::core;

class CImplProvidersConfig : public ProvidersConfig {
public:
    CImplProvidersConfig(const ProvidersConfig& provider) 
      : ProvidersConfig(provider.envMap()) {
        service_account_info_ = value("/cloudproviders/cimpl/cimplserviceaccountinformation");
        if (service_account_info_.empty()) {
            // Fallback to environment variables
            token_uri_ = value("/cloudproviders/cimpl/cimpltokenuri");
            client_id_ = value("/cloudproviders/cimpl/cimplclientid");
            client_secret_ = value("/cloudproviders/cimpl/cimplclientsecret");
            grant_type_ = valueOr("/cloudproviders/cimpl/cimplgranttype", "client_credentials");
        } else {
            // Parse the service account information as JSON
            nlohmann::json sa_json = nlohmann::json::parse(service_account_info_);
            if (!sa_json.empty()) {
                token_uri_ = sa_json.value("token_uri", "");
                client_id_ = sa_json.value("client_id", "");
                client_secret_ = sa_json.value("client_secret", "");
                grant_type_ = sa_json.value("grant_type", "");
            }
        }
    }

    std::string getTokenURI() const {
        return token_uri_;
    }
    std::string getClientId() const {
        return client_id_;
    }
    std::string getClientSecret() const {
        return client_secret_;
    }
    std::string getGrantType() const {
        return grant_type_;
    }
protected:
    std::string service_account_info_; // Service account information
    std::string token_uri_;
    std::string client_id_;
    std::string client_secret_;
    std::string grant_type_;
};

namespace oes::providers::cimpl {

std::string BearerTokenProviderCImpl::GetBearerToken(const ProvidersConfig& app_config) {
    CImplProvidersConfig cimpl_config(app_config);
    std::string oidc_endpoint = cimpl_config.getTokenURI();
    std::string client_id = cimpl_config.getClientId();
    std::string client_secret = cimpl_config.getClientSecret();
    std::string grant_type = cimpl_config.getGrantType();

    oes::providers::core::OAuthTokenClient client(oidc_endpoint, client_id, client_secret,"openid", grant_type);
    return client.getAccessToken();
}

}