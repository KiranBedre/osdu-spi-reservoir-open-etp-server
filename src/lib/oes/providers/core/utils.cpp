// ============================================================================
// Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
// Copyright © Microsoft Corporation
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

#include "utils.h"

#include <sstream>
#include <curl/curl.h>
#include <fmt/format.h>
#include <oes/core/utils/Logger.h>

namespace oes::providers::core::utils {

    int urlEncodeString(const std::string& unencoded_string, std::string& encoded_string) {
        CURL* curl = curl_easy_init();
        int ret = 0;
        if (curl != nullptr) {
            char* output = curl_easy_escape(curl, unencoded_string.c_str(), static_cast<int>(unencoded_string.length()));

            if (output != nullptr) {
                encoded_string.assign(output);
                curl_free(output);
            } else {
                OES_ERROR(fmt::format("libcurl failed to encode {0}\n", unencoded_string.c_str()));
                ret = 1;
            }
            curl_easy_cleanup(curl);
        } else {
            OES_ERROR("Failed to initialize libcurl\n");
            ret = 1;
        }
        return ret;
    }

    int mapParamsToQueryString(const std::map<std::string, std::string>& params, std::string& queryString) {
        auto iter = params.cbegin();
        auto iter_end = params.cend();
        if (params.empty() || iter == iter_end) {
            queryString.assign("");
            return 0;
        }
        std::stringstream buffer;
        const char* prepend = "";
        for (; iter != iter_end; ++iter) {
            if (iter->first.empty() || iter->second.empty()) {
                continue;
            }
            std::string url_encoded_param_name, url_encoded_param_value;
            int ret = urlEncodeString(iter->first, url_encoded_param_name);
            if (ret != 0) {
                OES_ERROR(fmt::format("Failed to encode URL Parameter Name {0}\n", iter->first));
                return ret;
            }

            ret = urlEncodeString(iter->second, url_encoded_param_value);
            if (ret != 0) {
                OES_ERROR(fmt::format("Failed to encode URL Parameter Value {0}\n", iter->second));
                return ret;
            }

            buffer << prepend << url_encoded_param_name << "=" << url_encoded_param_value;
            prepend = "&";
        }

        queryString.assign(buffer.str());
        return 0;
    }

}
