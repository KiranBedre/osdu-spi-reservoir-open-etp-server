// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
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

#include "HttpContext.h"

#include <oes/core/utils/String.h>


namespace oes::testing
{
    namespace 
    {
        std::unordered_map<std::string, std::string> parseQuery(std::string_view rawHttpQuery)
        {
            std::string decodedQuery;
            const size_t len = rawHttpQuery.size();
            for (size_t i = 0; i < len; i++)
            {
                if (rawHttpQuery[i] == '%')
                {
                    // Convert character from HEX
                    ++i;
                    if (i + 2 < len)
                    {
                        static auto hexFn = [](char ch) -> char { return static_cast<char>((ch <= '9') ? ch - '0' : (ch & 0x7) + 9); };
                        decodedQuery += static_cast<char>((hexFn(rawHttpQuery[i]) << 4) + hexFn(rawHttpQuery[i + 1]));
                    }
                    ++i;
                }
                else
                {
                    if (rawHttpQuery[i] == '+') {
                        decodedQuery += ' ';
                    }
                    else {
                        decodedQuery += rawHttpQuery[i];
                    }
                }
            }

            // Split query arguments
            std::vector<std::string> queryArgs;
            core::StringUtils::split(queryArgs, decodedQuery, "&");

            std::unordered_map<std::string, std::string> queryArgsMap;
            queryArgsMap.reserve(queryArgs.size());
            for (const auto& arg : queryArgs)
            {
                const auto pos = arg.find('=');
                if (pos == std::string::npos) {
                    queryArgsMap.emplace(arg, "");
                }
                else {
                    queryArgsMap.emplace(arg.substr(0, pos), arg.substr(pos + 1));
                }
            }
            return queryArgsMap;
        }
    }

    void HttpContext::HttpRequest::parseUri(std::string_view url)
    {
        // Parse path
        auto queryPos = url.find('?');
        path = url.substr(0, queryPos);

        // Parse HTTP query
        if (queryPos != std::string::npos)
        {
            ++queryPos;
            queryArgs = parseQuery(std::string_view(url.data() + queryPos, url.size() - queryPos));
        }
    }
}
