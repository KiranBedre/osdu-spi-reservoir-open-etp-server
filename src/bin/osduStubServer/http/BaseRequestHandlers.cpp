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

#include <iostream>
#include <regex>
#include <utility>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "BaseRequestHandlers.h"
#include <oes/core/utils/String.h>


namespace oes::testing
{
    namespace
    {
        std::string escapePathForRegex(const std::string& path)
        {
            std::string escapedResPath = path;
            for (size_t pos = escapedResPath.find('/'); pos != std::string::npos; pos = escapedResPath.find('/', pos))
            {
                escapedResPath.insert(pos, "\\");
                pos += 2;
            }
            return escapedResPath;
        }
    }

    RequestHandlerMock& RequestHandlerMock::withHttpMethod(std::string httpMethod)
    {
        httpMethod_ = std::move(httpMethod);
        return *this;
    }

    RequestHandlerMock& RequestHandlerMock::withUrlPath(std::string urlPath)
    {
        requireUrlPath_ = std::move(urlPath);
        if (requireUrlPath_->empty() || requireUrlPath_->front() != '/') {
            requireUrlPath_->insert(requireUrlPath_->begin(), '/');
        }
        return *this;
    }

    RequestHandlerMock& RequestHandlerMock::withUrlPath(std::regex urlRegex)
    {
        requireUrlRegex_ = std::move(urlRegex);
        return *this;
    }

    RequestHandlerMock& RequestHandlerMock::withQueryArg(std::string arg, std::optional<std::string> value)
    {
        if (!requireQueryArgs_) {
            requireQueryArgs_.emplace();
        }
        requireQueryArgs_.value().emplace(std::move(arg), std::move(value));
        return *this;
    }

    RequestHandlerMock& RequestHandlerMock::withHeader(std::string headerKey, std::optional<std::string> value)
    {
        if (!requireHeaders_) {
            requireHeaders_.emplace();
        }
        requireHeaders_.value().emplace(std::move(headerKey), std::move(value));
        return *this;
    }

    RequestHandlerMock& RequestHandlerMock::withBody(std::string body)
    {
        requireBody_ = std::move(body);
        return *this;
    }

    RequestHandlerMock& RequestHandlerMock::onRequest(std::function<bool(HttpContext&)> fn)
    {
        customFn_ = std::move(fn);
        return *this;
    }

    RequestHandlerMock& RequestHandlerMock::willReturn(std::string responseStr, uint16_t httpStatusCode)
    {
        responseString_ = std::move(responseStr);
        responseStatusCode_ = httpStatusCode;
        return *this;
    }

    void RequestHandlerMock::processRequest(HttpContext& httpContext, std::function<void()> nextFn)
    {
        bool processed = true;

        // HTTP method condition
        if (httpMethod_)
        {
            if (oes::core::StringUtils::caseInsensitiveCompare(httpContext.request.method, httpMethod_.value()) != 0)
            {
                processed = false;
            }
        }

        // URL path condition
        if (processed && requireUrlPath_)
        {
            if (oes::core::StringUtils::caseInsensitiveCompare(httpContext.request.path, requireUrlPath_.value()) != 0)
            {
                processed = false;
            }
        }

        // URL path regex condition
        if (processed && requireUrlRegex_)
        {
            std::cmatch what;
            if (!regex_match(httpContext.request.path.c_str(), what, requireUrlRegex_.value()))
            {
                processed = false;
            }
        }

        // HTTP query condition
        if (requireQueryArgs_)
        {
            for (const auto& queryArg : requireQueryArgs_.value())
            {
                const auto it = httpContext.request.queryArgs.find(queryArg.first);
                if (it != httpContext.request.queryArgs.cend())
                {
                    if (queryArg.second && queryArg.second.value() != it->second)
                    {
                        processed = false;
                        break;
                    }
                }
                else
                {
                    processed = false;
                    break;
                }
            }
        }

        // HTTP header condition
        if (requireHeaders_)
        {
            for (const auto& header : requireHeaders_.value())
            {
                const auto it = httpContext.request.headers.find(header.first);
                if (it != httpContext.request.headers.cend())
                {
                    if (header.second && header.second.value() != it->second)
                    {
                        processed = false;
                        break;
                    }
                }
                else
                {
                    processed = false;
                    break;
                }
            }
        }

        // HTTP body condition
        if (requireBody_)
        {
            if (httpContext.request.body != requireBody_.value())
            {
                processed = false;
            }
        }

        // Custom handler
        if (processed && customFn_)
        {
            processed = customFn_(httpContext);
        }

        // Set response when all conditions were matched
        if (processed)
        {
            if (responseString_.has_value()) {
                httpContext.response.body = responseString_.value();
            }
            if (responseStatusCode_) {
                httpContext.response.statusCode = responseStatusCode_.value();
            }

            ++matchedRequestsCount_;
        }
        else
        {
            nextFn();
        }
    }


    // ============================================================================

    BaseRestRequestHandler::BaseRestRequestHandler(std::string_view resourceName, std::optional<std::string_view> basePath)
        : resourceName_(resourceName)
    {
        std::string resPath = basePath ? std::string(basePath.value()) : "";
        if (resPath.empty() || resPath.front() != '/') {
            resPath.insert(0, "/");
        }
        if (!resPath.empty() && resPath.back() != '/') {
            resPath += '/';
        }
        resPath += resourceName_;

        const std::string pathRegex = fmt::format(R"(^{}(?:\/+([:a-zA-Z0-9_-]+))?)", escapePathForRegex(resPath));
        resPathRegex_ = std::regex(pathRegex);
    }

    void BaseRestRequestHandler::processRequest(HttpContext& httpContext, std::function<void()> nextFn)
    {
        bool processed = false;

        std::cmatch what;
        if (regex_match(httpContext.request.path.c_str(), what, resPathRegex_))
        {
            // Extract resource ID (optional)
            std::optional<std::string> id;
            if (what[1].matched) {
                id = what[1];
            }

            if (core::StringUtils::caseInsensitiveCompare(httpContext.request.method, "GET") == 0)
            {
                if (id) {
                    processed = getEntityById(httpContext, id.value());
                }
                else {
                    processed = getEntitiesList(httpContext);
                }
            }
            else if (core::StringUtils::caseInsensitiveCompare(httpContext.request.method, "POST") == 0)
            {
                // OSDU logical delete: POST /{resource}/{id}:delete
                static const std::string delete_suffix(":delete");
                if (id && id->size() > delete_suffix.size() &&
                    id->compare(id->size() - delete_suffix.size(), delete_suffix.size(), delete_suffix) == 0)
                {
                    processed = deleteEntity(httpContext, id->substr(0, id->size() - delete_suffix.size()));
                }
                else {
                    processed = createEntity(httpContext);
                }
            }
            else if (core::StringUtils::caseInsensitiveCompare(httpContext.request.method, "PUT") == 0)
            {
                processed = updateEntity(httpContext);
            }
            else if (id && core::StringUtils::caseInsensitiveCompare(httpContext.request.method, "DELETE") == 0)
            {
                processed = deleteEntity(httpContext, id.value());
            }
        }

        if (!processed)
        {
            nextFn();
        }
    }

    // ============================================================================

    void DebugPrintRequestHandler::processRequest(HttpContext& httpContext, std::function<void()> nextFn)
    {
        // Print request
        std::cout << std::endl << "--------------------------------------------------------------------------------" << std::endl;
        std::cout << "[" << httpContext.request.method << "] " << httpContext.request.path << std::endl;
        if (!httpContext.request.queryArgs.empty())
        {
            std::cout << "Query args: " << std::endl;
            for (const auto& queryArg : httpContext.request.queryArgs)
            {
                std::cout << "    " << queryArg.first << ": " << queryArg.second << std::endl;
            }
        }

        std::cout << "Headers: " << std::endl;
        for (const auto& header : httpContext.request.headers)
        {
            std::cout << "    " << header.first << ": " << header.second << std::endl;
        }
        if (!httpContext.request.body.empty())
        {
            std::cout << "Body: " << std::endl << httpContext.request.body << std::endl;
        }

        // Process requests
        nextFn();

        // Print response
        std::cout << "Response code: " << httpContext.response.statusCode << std::endl;
        if (!httpContext.response.body.empty())
        {
            std::cout << "Response body: " << httpContext.response.body << std::endl;
        }
    }
}
