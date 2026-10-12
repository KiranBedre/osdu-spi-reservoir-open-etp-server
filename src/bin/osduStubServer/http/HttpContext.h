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

#pragma once

#include <cstdint>  // Add this include for uint64_t
#include <string>
#include <map>
#include <unordered_map>


namespace oes::testing
{
    /**
     * \brief HTTP context.
     */
    struct HttpContext
    {
        struct HttpRequest
        {
            void parseUri(std::string_view url);

            std::string method;
            std::string path;
            std::unordered_map<std::string, std::string> queryArgs;
            std::string body;
            std::map<std::string, std::string> headers;
        };

        struct HttpResponse
        {
            uint16_t statusCode = 404;
            std::string body;
            std::map<std::string, std::string> headers;
        };

        HttpRequest request;
        HttpResponse response;
    };
}
