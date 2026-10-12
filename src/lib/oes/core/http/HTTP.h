// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_HTTP_HTTP_H
#define OES_CORE_HTTP_HTTP_H

/*! \file HTTP.h
*   \brief Describes a \c HTTP class
*/

#include <oes/core/http/export.h>

#include <string>
#include <vector>
#include <map>
#include <future>

namespace oes {
    namespace core {

        /*! \class HTTP
        * \brief Simple HTTP client implementation
        * \ingroup oes_core_http
        * \details
        * HTTP class provides HTTP sync and async methods:
        * GET, POST, PUT, DELETE and HEAD
        *
        * \code
        * HTTP http;
        * HTTP::Response response;
        * HTTP::AsyncResponse async_response;
        *
        * response = http.get("https://example.com/", 0, { "Accept: text/html" });
        *
        * async_response = http.async_post("https://example.com/", 0, { "Content-Type: application/json" }, R"({ "JSON": "struct"})");
        * response = async_response.get();
        * \endcode
        *
        * Note: By default HTTPS certificate verification is disabled.
        * To enable HTTPS certificate verification, set PG_HTTPS_CA_BUNDLE to the CA bundle file.
        * The latest CA bundle file can be found here https://curl.haxx.se/docs/caextract.html
        */
        class OES_CORE_HTTP_EXPORT HTTP
        {
        public:
            enum class Method {
                eGET,
                ePOST,
                ePUT,
                eDELETE,
                eHEAD
            };

            using Headers = std::vector<std::string>;

            using Fields = std::map<std::string, std::string>;

            /*! \struct Response
            * \brief HTTP response
            * 
            * Note: If the http method was successful, code/status contains
            * the server's response code/status.
            * Otherwise, in case of internal failure, the code is -1 
            * and the status contains an internal error message.
            *
            * \ingroup oes_core_http
            */
            struct Response {
                Response() {}
                short code = 0;
                std::string status;
                Fields fields;
                std::string body;
            };

            using AsyncResponse = std::future<Response>;

            /*! \struct Options
            * \brief HTTP request options
            *
            * \ingroup oes_core_http
            */
            struct Options {
                Options() {}
                /* request timeout, 0 by default */
                long timeout = 60;
                /* true to disable CURLOPT_SSL_VERIFYPEER when there is no certificate bundle, true by default*/
                bool disableVERIFYPEER = true;
                /* true to disable CURLOPT_SSL_VERIFYHOST when there is no certificate bundle, false by default*/
                bool disableVERIFYHOST = false;
                /* true to disable CURLOPT_SSL_VERIFYSTATUS when there is no certificate bundle, false by default*/
                bool disableVERIFYSTATUS = false;
                /* number of retries on request error, exponential delay starting at 1 second, 0 by default */
                unsigned int retry = 0;
            };

            HTTP();
            virtual ~HTTP();

            HTTP(const HTTP&) = delete;
            HTTP& operator=(const HTTP&) = delete;

            /*! \brief HTTP GET method
            * 
            * \param url HTTP server URL
            * \param port HTTP server port
            * \param headers HTTP header fields
            * \param opt HTTP request options
            *
            * \retval Response Server's response.
            *
            * \ingroup oes_core_http
            */
            virtual Response get(const std::string& url, const size_t port, const Headers& headers, const Options& opt = Options());

            /*! \brief HTTP POST method
            *
            * \param url HTTP server URL
            * \param port HTTP server port
            * \param headers HTTP header fields
            * \param body HTTP body
            * \param opt HTTP request options
            *
            * \retval Response Server's response.
            *
            * \ingroup oes_core_http
            */
            virtual Response post(const std::string& url, const size_t port, const Headers& headers, const std::string& body, const Options& opt = Options());

            /*! \brief HTTP PUT method
            *
            * \param url HTTP server URL
            * \param port HTTP server port
            * \param headers HTTP header fields
            * \param body HTTP body
            * \param opt HTTP request options
            *
            * \retval Response Server's response.
            *
            * \ingroup oes_core_http
            */
            virtual Response put(const std::string& url, const size_t port, const Headers& headers, const std::string& body, const Options& opt = Options());

            /*! \brief HTTP DELETE method
            *
            * \param url HTTP server URL
            * \param port HTTP server port
            * \param headers HTTP header fields
            * \param opt HTTP request options
            *
            * \retval Response Server's response.
            *
            * \ingroup oes_core_http
            */
            virtual Response del(const std::string& url, const size_t port, const Headers& headers, const Options& opt = Options());

            /*! \brief HTTP HEAD method
            *
            * \param url HTTP server URL
            * \param port HTTP server port
            * \param opt HTTP request options
            *
            * \retval Response Server's response.
            *
            * \ingroup oes_core_http
            */
            virtual Response head(const std::string& url, const size_t port, const Options& opt = Options());

        private:
            struct Impl;
            Impl* _impl;
        };

    } // namespace core
} // namespace oes

#endif /* OES_CORE_HTTP_HTTP_H */
