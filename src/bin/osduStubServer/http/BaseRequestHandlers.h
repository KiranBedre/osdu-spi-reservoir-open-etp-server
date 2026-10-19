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
#include <string>
#include <thread>
#include <functional>
#include <optional>
#include <regex>
#include <unordered_map>

#include "HttpContext.h"

namespace oes::testing
{
    /**
     * \brief Interface of HTTP request handler.
     * \details Follow Middleware concept that allow to build pipeline to handle requests and responses.
     * Each component:
     *  - Chooses whether to pass the request to the next component in the pipeline.
     *  - Can perform work before and after the next component in the pipeline.
     */
    class IHttpRequestHandler
    {
    public:
        IHttpRequestHandler() = default;
        IHttpRequestHandler(const IHttpRequestHandler&) = default;
        IHttpRequestHandler(IHttpRequestHandler&&) = default;
        virtual ~IHttpRequestHandler() = default;

        IHttpRequestHandler& operator=(const IHttpRequestHandler&) = default;
        IHttpRequestHandler& operator=(IHttpRequestHandler&&) = default;

        /**
         * \brief Processes HTTP request.
         * \details `nextFn()` should be invoked when request does not match requirements of current handler.
         */
        virtual void processRequest(HttpContext& httpContext, std::function<void()> nextFn) = 0;
    };
    using SharedRequestHandler = std::shared_ptr<IHttpRequestHandler>;


    /**
     * \brief Implementation of IHttpRequestHandler for easy mock HTTP requests.
     * \details Allows to set multiple required conditions such as URL path, query arg, header, etc.
     * Will return a user specified response when all are matched.
     */
    class RequestHandlerMock : public IHttpRequestHandler
    {
    public:
        /// HTTP method should be equal to passed value (case insensitive).
        RequestHandlerMock& withHttpMethod(std::string httpMethod);

        /// URL path should match passed value (case insensitive).
        RequestHandlerMock& withUrlPath(std::string urlPath);

        /*!
         * \brief URL path should match passed regex.
         * \details Common regex samples:
         *  - Path to resource "/resource":                         R"(^\/resource))"
         *  - Path to resource with any ID like "/resource/10":     R"(^\/resource\/+([0-9]+))"
         */
        RequestHandlerMock& withUrlPath(std::regex urlRegex);

        /// HTTP query should contain argument with value (optional), case sensitive.
        RequestHandlerMock& withQueryArg(std::string arg, std::optional<std::string> value = {});

        /// HTTP request should contain header with value (optional), case sensitive.
        RequestHandlerMock& withHeader(std::string headerKey, std::optional<std::string> value = {});

        /// HTTP body should be equal passed value.
        RequestHandlerMock& withBody(std::string body);

        /// Custom request handler, should return `true` when handled.
        RequestHandlerMock& onRequest(std::function<bool(HttpContext&)> fn);

        /// Will return passed value and status code when all conditions are passed.
        RequestHandlerMock& willReturn(std::string responseStr, uint16_t httpStatusCode = 200);

        /// Returns number of successfully handled requests (when all conditions are passed).
        [[nodiscard]] size_t getMatchedRequestsCount() const noexcept {
            return matchedRequestsCount_;
        }

        /// Handles HTTP request depending on the selected conditions.
        void processRequest(HttpContext& httpContext, std::function<void()> nextFn) override;

    private:
        std::optional<std::string> httpMethod_;
        std::optional<std::string> requireUrlPath_;
        std::optional<std::regex> requireUrlRegex_;
        std::optional<std::unordered_map<std::string, std::optional<std::string>>> requireQueryArgs_;
        std::optional<std::unordered_map<std::string, std::optional<std::string>>> requireHeaders_;
        std::optional<std::string> requireBody_;
        std::function<bool(HttpContext&)> customFn_;
        std::optional<std::string> responseString_;
        std::optional<uint16_t> responseStatusCode_;
        size_t matchedRequestsCount_ = 0;
    };


    /**
     * \brief Base class for REST request handlers.
     */
    class BaseRestRequestHandler : public IHttpRequestHandler
    {
    public:
        explicit BaseRestRequestHandler(std::string_view resourceName, std::optional<std::string_view> basePath = std::nullopt);

        virtual bool getEntitiesList(HttpContext& httpContext) = 0;
        virtual bool getEntityById(HttpContext& httpContext, const std::string& id) = 0;
        virtual bool createEntity(HttpContext& httpContext) = 0;
        virtual bool updateEntity(HttpContext& httpContext) = 0;
        virtual bool deleteEntity(HttpContext& httpContext, const std::string& id) = 0;

        /**
         * \brief Handles HTTP request depending on the selected conditions.
         */
        void processRequest(HttpContext& httpContext, std::function<void()> nextFn) override;

    private:
        const std::string resourceName_;
        const std::string resPath_;
        std::regex resPathRegex_;
    };


    /**
     * \brief Prints request/response information
     */
    class DebugPrintRequestHandler : public IHttpRequestHandler
    {
    public:
        /**
         * \brief Handles HTTP request depending on the selected conditions.
         */
        void processRequest(HttpContext& httpContext, std::function<void()> nextFn) override;
    };
}
