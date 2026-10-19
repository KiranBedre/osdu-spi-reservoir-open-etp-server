// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2022 EPAM Systems
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

#include "Utils.h"

#include <string>
#include <map>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <azure/core/diagnostics/logger.hpp>
#include <curl/curl.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>

namespace oes::providers::azure {

std::string urlEncode(const char* msg) {
    CURL* c = curl_easy_init();
    char* encoded = curl_easy_escape(c, msg, 0);
    std::string retval = encoded;
    curl_free(encoded);
    curl_easy_cleanup(c);
    return retval;
};

std::string paramsToString(const std::map<std::string, std::string>& params) {
    if(params.empty())
    { return ""; }

    auto pb = params.cbegin();
    auto pe = params.cend();
    std::string data = urlEncode(&pb->first[0]) + "=" + urlEncode(&pb->second[0]);
    ++pb;
    if(pb == pe)
        { return data; }

    for(; pb!= pe; ++ pb)
        { data += "&" + urlEncode(&pb->first[0]) + "=" + urlEncode(&pb->second[0]); }
    return data;
};

std::string getEnv(const std::string& name) {
    char *tmp = nullptr;
    tmp = std::getenv(name.c_str());
    if (tmp != nullptr)
    {
        return tmp;
    }
    return "";
};

void setupAzureLogging() {
    static bool loggingSetup;
    static const std::string tag = "[Azure::Core::Diagnostics::Logger]";

    // One-time setup to connect the Azure logger to the OES logger.
    if (!loggingSetup) {
        OES_DEBUG("Setting up Azure logging...");

        using namespace Azure::Core::Diagnostics;
        Logger::SetLevel(Logger::Level::Verbose);
        Logger::SetListener([&](auto lvl, auto msg){ 
            switch (lvl) {
            case Logger::Level::Error:
                OES_ERROR("{} {}", tag, msg);
                break;
            case Logger::Level::Warning:
                OES_WARNING("{} {}", tag, msg);
                break;
            case Logger::Level::Informational:
                OES_INFO("{} {}", tag, msg);
                break;
            case Logger::Level::Verbose:
                OES_VERBOSE("{} {}", tag, msg);
                break;
            }
        });

        loggingSetup = true;
    }
}

} // namespace oes::providers::azure