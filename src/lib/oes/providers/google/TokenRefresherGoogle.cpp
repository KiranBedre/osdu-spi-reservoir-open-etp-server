// ============================================================================
// Copyright 2023 Google LLC
// Copyright 2023 EPAM Systems
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

#include <chrono>
#include <thread>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <google/cloud/internal/oauth2_google_credentials.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "TokenRefresherGoogle.h"

#include <oes/core/utils/Logger.h>

#ifdef _WIN32
#pragma comment (lib, "ws2_32")
#endif

using namespace google::cloud::oauth2_internal::v2_4_0;

namespace
{
    constexpr uint32_t MAX_NUMBER_OF_TRY = 5;

    std::string _GetBearerToken(uint32_t tryNum)
    {
        OES_INFO("Using Google Token Refresher.");
        std::shared_ptr<Credentials> googleCredentials;

        auto creds = GoogleDefaultCredentials();
        if (creds.ok())
        {
            OES_INFO("Successfully got google credentials.");
            googleCredentials = *creds;
        }
        else
        {
            OES_ERROR(creds.status().message());
            throw std::invalid_argument("Malformed service account.");
        };

        auto header = googleCredentials->AuthorizationHeader();
        if (header.ok())
        {
            OES_INFO("Successfully got google auth token.");
            return header->second;
        }
        else
        {
            OES_ERROR("Can't get AuthorizationHeader.");
            OES_ERROR(header.status().message());

            if (tryNum >= MAX_NUMBER_OF_TRY) {
                return "";
            } else {
                OES_WARNING(fmt::format("The try number {} wasn't successful.", tryNum));
                std::this_thread::sleep_for(std::chrono::seconds(1 << tryNum));
                return _GetBearerToken(tryNum + 1);
            }
        }
    }
}

std::string oes::providers::google::TokenRefresherGoogle::GetBearerToken(
    const oes::providers::core::ProvidersConfig& /*app_config*/) {
    return _GetBearerToken(0);
}