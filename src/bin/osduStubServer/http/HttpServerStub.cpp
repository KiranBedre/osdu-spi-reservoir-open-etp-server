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

#include "HttpServerStub.h"

#include <iostream>
#include <utility>
#include <thread>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>


namespace oes::testing
{
    /**
     * Private implementation of HttpServer::ServerImpl.
     */
    class HttpServerStub::ServerImpl
    {
    public:
        typedef websocketpp::server<websocketpp::config::asio> AsioWebServer;

        explicit ServerImpl(std::vector<SharedRequestHandler>& requestHandlersRef)
            : requestHandlers_(requestHandlersRef)
        {
            init();
        }

        void init()
        {
            server_.set_reuse_addr(true);

            // Set logging settings
            server_.set_error_channels(websocketpp::log::elevel::fatal);    // set 'all' for get more details
            server_.set_access_channels(websocketpp::log::alevel::none);    // set 'all' for view requests details

            // Initialize Asio
            server_.init_asio();

            // Set HTTP handler
            server_.set_http_handler([this](auto&& hdl) {
                onHttpRequest(std::forward<decltype(hdl)>(hdl));
            });
        }

        bool start(uint16_t port)
        {
            websocketpp::lib::error_code ec;
            server_.listen(port, ec);
            if (!ec)
            {
                // Queues a connection accept operation
                server_.start_accept(ec);
                if (!ec)
                {
                    // Start the Asio io_service run loop on thread
                    workThread_ = std::thread(&AsioWebServer::run, &server_);
                    return true;
                }
            }

            std::cerr << "Can't start HttpServerStub, error code: " << ec.value() << " (" << ec.message() << ")" << std::endl;
            return false;
        }

        [[nodiscard]] bool isListening() const
        {
            return server_.is_listening();
        }

        void stop()
        {
            if (workThread_.joinable())
            {
                // Stopping the Websocket listener
                if (isListening())
                {
                    websocketpp::lib::error_code ec;
                    server_.stop_listening(ec);
                    if (ec) {
                        std::cerr << "Failed to stop listening, error code: " << ec.value() << " (" << ec.message() << ")" << std::endl;
                    }
                }

                // Stop server
                server_.stop_perpetual();
                server_.stop();
                workThread_.join();
            }
        }

        void onHttpRequest(websocketpp::connection_hdl hdl)
        {
            const AsioWebServer::connection_ptr conn = server_.get_con_from_hdl(std::move(hdl));
            HttpContext httpContext = makeHttpContext(conn);

            // Process handlers
            try
            {
                invokeNextHandler(requestHandlers_.get().cbegin(), requestHandlers_.get().cend(), httpContext);
            }
            catch (const std::exception& ex)
            {
                std::cerr << "HttpServerStub. Unhandled exception: " << ex.what() << std::endl;
                httpContext.response.statusCode = 500;
            }
            catch (...)
            {
                std::cerr << "HttpServerStub. Unhandled unknown exception" << std::endl;
                httpContext.response.statusCode = 500;
            }

            // Send response
            conn->set_status(static_cast<websocketpp::http::status_code::value>(httpContext.response.statusCode));
            conn->set_body(httpContext.response.body);

            // Set headers
            for (const auto& header : httpContext.response.headers)
            {
                conn->append_header(header.first, header.second);
            }
        }

        static HttpContext makeHttpContext(const AsioWebServer::connection_ptr& connection)
        {
            HttpContext httpContext;
            httpContext.request.method = connection->get_request().get_method();
            httpContext.request.body = connection->get_request().get_body();
            httpContext.request.headers.insert(connection->get_request().get_headers().cbegin(), connection->get_request().get_headers().cend());
            httpContext.request.parseUri(connection->get_request().get_uri());
            return httpContext;
        }

        static void invokeNextHandler(std::vector<SharedRequestHandler>::const_iterator nextIt,
            std::vector<SharedRequestHandler>::const_iterator endIt, HttpContext& httpContext)
        {
            if (nextIt != endIt)
            {
                auto& handler = *nextIt;
                handler->processRequest(httpContext, [&nextIt, &endIt, &httpContext]
                {
                    ++nextIt;
                    invokeNextHandler(nextIt, endIt, httpContext);
                });
            }
        }

    private:
        AsioWebServer server_;
        std::thread workThread_;
        std::reference_wrapper<std::vector<SharedRequestHandler>> requestHandlers_;
    };

    // ============================================================================

    HttpServerStub::HttpServerStub(uint16_t port)
        : port_(port)
        , server_(std::make_unique<ServerImpl>(requestHandlers_))
    { }

    HttpServerStub::~HttpServerStub()
    {
        stop();
    }

    bool HttpServerStub::start()
    {
        return server_->start(port_);
    }

    bool HttpServerStub::isListening() const
    {
        return server_->isListening();
    }

    void HttpServerStub::stop()
    {
        server_->stop();
    }
}
