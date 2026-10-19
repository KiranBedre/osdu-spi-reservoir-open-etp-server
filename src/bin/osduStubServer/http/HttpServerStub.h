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
#include <vector>
#include <memory>

#include "BaseRequestHandlers.h"


namespace oes::testing
{
    /**
     * Simple single-threaded HTTP server for make endpoints stubs.
     */
    class HttpServerStub
    {
    public:
        explicit HttpServerStub(uint16_t port = 9080);
        ~HttpServerStub();

        HttpServerStub(const HttpServerStub&) = delete;
        HttpServerStub(HttpServerStub&&) = delete;
        HttpServerStub& operator=(const HttpServerStub&) = delete;
        HttpServerStub& operator=(HttpServerStub&&) = delete;

        bool start();
        [[nodiscard]] bool isListening() const;
        [[nodiscard]] uint16_t getPort() const noexcept { return port_; }
        void stop();

        /**
         * \brief Emplaces a request handler (pass type of handler and necessary arguments for its constructor).
         * \param args Arguments to forward to the constructor of the handler.
         * \return reference to the request handler.
         */
        template <typename T, typename ...TArgs>
        T& emplaceRequestHandler(TArgs&&... args)
        {
            // cppcheck-suppress [danglingTempReference,redundantAssignment]
            auto& handlerRef = requestHandlers_.emplace_back(std::make_shared<T>(std::forward<TArgs>(args)...));
            // cppcheck-suppress danglingTempReference
            return static_cast<T&>(*handlerRef.get());
        }

        /**
         * \brief Adds a request handler.
         * \param requestHandler Shared pointer to request handler.
         * \return reference to the request handler.
         */
        template <typename T>
        T& addRequestHandler(std::shared_ptr<T> requestHandler)
        {
            auto& handlerRef = *requestHandler;
            requestHandlers_.push_back(std::move(requestHandler));
            return handlerRef;
        }

        /**
         * \brief Creates and adds new RequestHandlerMock.
         * \return reference to created RequestHandlerMock.
         */
        RequestHandlerMock& addRequestHandlerMock()
        {
            return addRequestHandler(std::make_shared<RequestHandlerMock>());
        }

    private:
        class ServerImpl;

        const uint16_t port_ = 0;
        std::unique_ptr<ServerImpl> server_;
        std::vector<SharedRequestHandler> requestHandlers_;
    };
}
