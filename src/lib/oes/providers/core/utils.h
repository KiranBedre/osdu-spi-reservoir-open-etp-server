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

#pragma once

#include <map>
#include <string>

namespace oes::providers::core::utils {

    /**
     * @brief URL encode a string using libcurl
     * @param unencoded_string The string to encode
     * @param encoded_string Output parameter for the encoded string
     * @return 0 on success, non-zero on failure
     */
    int urlEncodeString(const std::string& unencoded_string, std::string& encoded_string);

    /**
     * @brief Convert a map of parameters to a URL query string
     * @param params Map of parameter names to values
     * @param queryString Output parameter for the query string
     * @return 0 on success, non-zero on failure
     */
    int mapParamsToQueryString(const std::map<std::string, std::string>& params, std::string& queryString);

}
