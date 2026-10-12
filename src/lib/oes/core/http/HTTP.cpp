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

#include <oes/core/http/HTTP.h>

#include <stdlib.h>
#include <sstream>
#include <set>
#include <thread>
#include <cmath>
#include <utility>
#include <string>
#include <cstring>

#include <curl/curl.h>

namespace oes {
    namespace core {

        static std::set s_httpErrorCodes = { 408, 429, 500, 502, 503, 504 };

        struct HTTPInitializer {
            HTTPInitializer() {
                curl_global_init(CURL_GLOBAL_DEFAULT);
            }

            ~HTTPInitializer() {
                curl_global_cleanup();
            }
        };

        struct HTTP::Impl {
            static std::string ca_bundle;
            Impl() {
                static bool first_init (true);
                if (first_init) {
                    const char* s_ca_bundle = getenv("PG_HTTPS_CA_BUNDLE");
                    if (s_ca_bundle) {
                        // Define a buffer with a fixed size of 8 KB to avoid overflow
                        char buffer[8192];
                        // Use strncpy to copy the value to the buffer
                        std::strncpy(buffer, s_ca_bundle, sizeof(buffer) - 1);
                        // Ensure the buffer is null-terminated
                        buffer[sizeof(buffer) - 1] = '\0';
                        HTTP::Impl::ca_bundle = std::string(buffer);
                    }
                    first_init = false;
                }
            }
            

            struct Request {
                Request();
                ~Request();

                bool run(Method method, const std::string& url, const size_t port,
                    const Headers& inHeader, std::istream& inBody, long& outCode,
                    std::ostream& outHeader, std::ostream& outBody, const Options& opt);

                static size_t readCallback(void *target, size_t size, size_t nmemb, void *userData);
                static size_t writeCallback(void *source, size_t size, size_t nmemb, void *userData);

                size_t streamlen(std::istream& inData);

                CURL* _curl = nullptr;
            };

            Response response(bool result, long code, std::stringstream& header, std::stringstream& body);

            Request _request;
            #ifdef ASYNC
            Request _asyncRequest;
            DispatchQueue _dispatchQueue;
            #endif
        };

        std::string HTTP::Impl::ca_bundle = "";

        HTTP::Impl::Request::Request() {
            /* Create a new curl handle */
            _curl = curl_easy_init();
        }

        HTTP::Impl::Request::~Request() {
            if (_curl) {
                /* always cleanup */
                curl_easy_cleanup(_curl);
            }
        }

        bool HTTP::Impl::Request::run(Method method, const std::string& url, const size_t port,
            const Headers& inHeader, std::istream& inBody, long& outCode,
            std::ostream& outHeader, std::ostream& outBody, const Options& opt) {

            if (!_curl) {
                outCode = CURLE_FAILED_INIT;
                return false;
            }

            unsigned int retry_count = 0;

            do {
                if (retry_count > 0) {
                    auto seconds = std::min(600, static_cast<int>(std::pow(2, retry_count - 1)));
                    std::this_thread::sleep_for(std::chrono::seconds(seconds));
                }

                /* First set the URL that is about to receive our command. */
                curl_easy_setopt(_curl, CURLOPT_URL, url.c_str());

                /* Set the PORT. */
                if (port) {
                    curl_easy_setopt(_curl, CURLOPT_PORT, port);
                }

                /* Get the custom headers */
                struct curl_slist* header = NULL;
                if (inHeader.size() > 0) {
                    for (const auto& field : inHeader) {
                        header = curl_slist_append(header, field.c_str());
                    }
                }

                /* Set "Expect:" to disable "Expect: 100-continue" */
                header = curl_slist_append(header, "Expect:");

                /* Set the custom headers */
                curl_easy_setopt(_curl, CURLOPT_HTTPHEADER, header);

                if (method == Method::eGET) {
                    /* Now specify we want to GET data */
                    curl_easy_setopt(_curl, CURLOPT_HTTPGET, 1L);
                } else if (method == Method::ePOST) {
                    /* Now specify we want to POST data */
                    curl_easy_setopt(_curl, CURLOPT_POST, 1L);

                    /* We want to use our own read function */
                    curl_easy_setopt(_curl, CURLOPT_READFUNCTION, readCallback);

                    /* Pointer to pass to our read function */
                    curl_easy_setopt(_curl, CURLOPT_READDATA, (void*)&inBody);

                    /* Set the expected POST size. */
                    curl_easy_setopt(_curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)streamlen(inBody));
                } else if (method == Method::ePUT) {
                    /* Now specify we want to PUT data */
                    curl_easy_setopt(_curl, CURLOPT_UPLOAD, 1L);

                    /* We want to use our own read function */
                    curl_easy_setopt(_curl, CURLOPT_READFUNCTION, readCallback);

                    /* Pointer to pass to our read function */
                    curl_easy_setopt(_curl, CURLOPT_READDATA, (void*)&inBody);

                    /* Set the expected PUT size. */
                    curl_easy_setopt(_curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)streamlen(inBody));
                } else if (method == Method::eDELETE) {
                    /* Now specify we want to DELETE data */
                    curl_easy_setopt(_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
                } else if (method == Method::eHEAD) {
                    /* Now specify we want to HEAD data */
                    curl_easy_setopt(_curl, CURLOPT_NOBODY, 1L);
                }

                /* Send all data to this function  */
                curl_easy_setopt(_curl, CURLOPT_WRITEFUNCTION, writeCallback);

                /* We want the headers be written to this file handle */
                curl_easy_setopt(_curl, CURLOPT_HEADERDATA, (void*)&outHeader);

                /* We want the body be written to this stream instead of stdout */
                curl_easy_setopt(_curl, CURLOPT_WRITEDATA, (void*)&outBody);

                if (!ca_bundle.empty()) {
                    /* Set path to ca bundle */
                    curl_easy_setopt(_curl, CURLOPT_CAINFO, ca_bundle.c_str());
                } else {
                    /* Disable peer verification */
                    if (opt.disableVERIFYPEER) {
                        curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYPEER, 0L);
                    }
                    if (opt.disableVERIFYHOST) {
                        curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYHOST, 0L);
                    }
                    if (opt.disableVERIFYSTATUS) {
                        curl_easy_setopt(_curl, CURLOPT_SSL_VERIFYSTATUS, 0L);
                    }
                }

                if (opt.timeout > 0) {
                    curl_easy_setopt(_curl, CURLOPT_TIMEOUT, opt.timeout);
                }

                /* Get verbose debug output please */
                // curl_easy_setopt(_curl, CURLOPT_VERBOSE, 1L);

                /* Perform the request, res will get the return code */
                CURLcode res = curl_easy_perform(_curl);
                /* Check for errors */
                if (res != CURLE_OK) {
                    outCode = res;

                    /* Always reset */
                    curl_easy_reset(_curl);

                    if (header) {
                        /* Free the custom headers */
                        curl_slist_free_all(header);
                    }
                    
                    return false;
                }

                curl_easy_getinfo(_curl, CURLINFO_RESPONSE_CODE, &outCode);

                /* Always reset */
                curl_easy_reset(_curl);

                if (header) {
                    /* Free the custom headers */
                    curl_slist_free_all(header);
                }

                retry_count++;

            } while (retry_count < opt.retry && s_httpErrorCodes.find(outCode) != s_httpErrorCodes.end());
            
            return true;
        }

        size_t HTTP::Impl::Request::readCallback(
            void *target, size_t size, size_t nmemb, void *userData) {
            auto req_body = (std::istream*)userData;

            try {
                req_body->read((char*)target, size * nmemb);
            }
            catch (...) {
                return 0;
            }

            return req_body->gcount();
        }

        size_t HTTP::Impl::Request::writeCallback(
            void *source, size_t size, size_t nmemb, void *userData) {
            auto res_body = (std::ostream*)userData;

            try {
                res_body->write((const char*)source, size * nmemb);
            }
            catch (...) {
                return 0;
            }

            return size * nmemb;
        }

        size_t HTTP::Impl::Request::streamlen(std::istream& stream) {
            stream.seekg(0, std::ios_base::end);
            size_t length = stream.tellg();
            stream.seekg(0);

            return length;
        }

        HTTP::Response HTTP::Impl::response(bool result,
            long code, std::stringstream& header, std::stringstream& body) {
            Response res;
            
            if (result) {
                res.code = (short)code;
                std::getline(header, res.status, '\r');
                header.seekg(1, std::ios_base::cur);    // skip '\n'

                for (std::string field; std::getline(header, field, '\r'); ) {
                    if (!field.empty()) {
                        auto key_end = field.find_first_of(": ");
                        res.fields.emplace(
                            field.substr(0, key_end),
                            field.substr(key_end + 2, field.length() - key_end));
                    }
                    header.seekg(1, std::ios_base::cur);    // skip '\n'
                }

                res.body = body.str();
            }
            else {
                res.code = -1;
                res.status = curl_easy_strerror((CURLcode)code);
            }

            return res;
        }

        HTTP::HTTP() : _impl(new Impl()) {
            static HTTPInitializer init;
        }

        HTTP::~HTTP() {
            delete _impl;
        }

        HTTP::Response HTTP::get(const std::string& url, const size_t port,
            const Headers& header, const Options& opt) {
            std::stringstream req_body, res_header, res_body;
            long res_code;
            bool res = _impl->_request.run(HTTP::Method::eGET, url, port,
                header, req_body, res_code, res_header, res_body, opt);

            return _impl->response(res, res_code, res_header, res_body);
        }

        HTTP::Response HTTP::post(const std::string& url, const size_t port,
            const Headers& header, const std::string& body, const Options& opt) {
            std::stringstream req_body(body), res_header, res_body;
            long res_code;
            bool res = _impl->_request.run(
                    HTTP::Method::ePOST, url, port, header, req_body, res_code, res_header, res_body, opt);

            return _impl->response(res, res_code, res_header, res_body);
        }

        HTTP::Response HTTP::put(const std::string& url, const size_t port,
            const Headers& header, const std::string& body, const Options& opt) {
            std::stringstream req_body(body), res_header, res_body;
            long res_code;
            bool res = _impl->_request.run(
                    HTTP::Method::ePUT, url, port, header, req_body, res_code, res_header, res_body, opt);

            return _impl->response(res, res_code, res_header, res_body);
        }


        HTTP::Response HTTP::del(const std::string& url, const size_t port, const Headers& header, const Options& opt) {
            std::stringstream req_body, res_header, res_body;
            long res_code;
            bool res = _impl->_request.run(
                    HTTP::Method::eDELETE, url, port, header, req_body, res_code, res_header, res_body, opt);

            return _impl->response(res, res_code, res_header, res_body);
        }

        HTTP::Response HTTP::head(const std::string& url, const size_t port, const Options& opt) {
            std::stringstream req_body, res_header, res_body;
            long res_code;
            bool res = _impl->_request.run(
                    HTTP::Method::eHEAD, url, port, {}, req_body, res_code, res_header, res_body, opt);

            return _impl->response(res, res_code, res_header, res_body);
        }

    } // namespace core
} // namespace oes
