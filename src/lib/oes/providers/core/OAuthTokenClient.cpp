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

#include "OAuthTokenClient.h"
#include "Utils.h"

#include <string>
#include <sstream>
#include <boost/beast/core/detail/base64.hpp>

#include <oes/core/http/HTTP.h>
#include <fmt/format.h>
#include <oes/core/utils/Logger.h>

#include <nlohmann/json.hpp>

namespace oes::providers::core {

    OAuthTokenClient::OAuthTokenClient(
        const std::string& token_url, 
        const std::string& client_id,
        const std::string& client_secret,
        const std::string& oauth_scope,
        const std::string& grant_type
    ) :
        token_url_(token_url),
        client_id_(client_id),
        client_secret_(client_secret),
        grant_type_(grant_type),
        oauth_scope_(oauth_scope) {
    }

    std::string OAuthTokenClient::getAccessToken() const {
        // Get the Authentication Header
        std::string auth_header;
        this->generateAuthHeader(auth_header);

        // Headers for OAUTH request
        std::vector<std::string> headersVector{
                auth_header,
                "Content-Type: application/x-www-form-urlencoded"
        };

        // Parameters for OAUTH request
        std::string request_body;
        int result = generateRequestBody(request_body);
        if (result != 0) return ""; // Error already logged within generateRequestBody

        // Send the POST request, and process based on response
        oes::core::HTTP request;
        oes::core::HTTP::Options options;
        oes::core::HTTP::Response response = request.post(token_url_, 0, headersVector, request_body, options);


        if (response.code == 200) {
            OES_INFO(fmt::format("Successfully received client auth token\n"));
            const auto json_response = nlohmann::json::parse(response.body.c_str());
            return json_response["access_token"];
        } else {
            OES_ERROR(fmt::format("Could not acquire the access token! Status code: {0}\n", response.code));
            OES_ERROR(fmt::format(response.body + "\n"));
            return "";
        }
    }

    void OAuthTokenClient::generateAuthHeader(std::string& auth_header) const {
        std::stringstream outbuffer;
        // The auth client base format to be base64 encoded
        outbuffer <<  client_id_.c_str() << ":" << client_secret_.c_str();
        const std::string auth(outbuffer.str());
        outbuffer.str(std::string());   // Clear the outbuffer

        // Convert to the base64 encoded
        std::string base64_encoded_auth(boost::beast::detail::base64::encoded_size(auth.length()), '\0');

        boost::beast::detail::base64::encode(base64_encoded_auth.data(), auth.c_str(), auth.length());

        // Get the full Authorization header, including the name and delimiter
        outbuffer << "Authorization: Basic " << base64_encoded_auth;

        auth_header.assign(outbuffer.str());
    }

    int OAuthTokenClient::generateRequestBody(std::string &request_body) const {
        std::map<std::string, std::string> parameters_map{
            {"client_id", client_id_},
            {"client_secret", client_secret_},
            {"grant_type", grant_type_},
            {"scope", oauth_scope_}
        };

        int error_result = mapParamsToQueryString(parameters_map, request_body);
        if (error_result) {
            OES_ERROR(fmt::format("Failed to encode the client credentials parameters.\n"));
            return error_result;
        }
        return 0;
    }
}