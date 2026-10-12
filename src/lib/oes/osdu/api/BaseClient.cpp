// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2022 EPAM Systems
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
#include <oes/osdu/api/BaseClient.h>
#include <oes/osdu/api/OsduClientException.h>

#include <oes/core/utils/Logger.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <curl/curl.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cassert>
#include <chrono>

namespace oes {
    namespace osdu {
        namespace api {

            namespace {
                // Helper function to convert HTTP Method enum to string for logging
                const char* methodToString(oes::core::HTTP::Method method) {
                    switch(method) {
                        case oes::core::HTTP::Method::eGET:    return "GET";
                        case oes::core::HTTP::Method::ePOST:   return "POST";
                        case oes::core::HTTP::Method::ePUT:    return "PUT";
                        case oes::core::HTTP::Method::eDELETE: return "DELETE";
                        case oes::core::HTTP::Method::eHEAD:   return "HEAD";
                        default:
                            OES_WARNING("HTTP", "Unknown HTTP method enum value: {}", static_cast<int>(method));
                            return "UNKNOWN";
                    }
                }

                // Helper function to extract base URL without query parameters (to avoid logging PII)
                std::string sanitizeUrl(const std::string& url) {
                    // Remove query parameters to avoid logging potential PII
                    size_t queryPos = url.find('?');
                    if (queryPos != std::string::npos) {
                        return url.substr(0, queryPos) + "?<query-params-redacted>";
                    }
                    return url;
                }
            }

            struct BaseClient::Impl {
                explicit Impl(HTTPFactory httpFactory) {
                    if (httpFactory) {
                        _httpReq = httpFactory();
                    }
                    if (!_httpReq) {
                        _httpReq = std::make_shared<oes::core::HTTP>();
                    }
                }

                static std::string paramsToString(const oes::core::HTTP::Fields& params);

                oes::core::HTTP::Response sendRequest(
                    oes::core::HTTP::Method method,
                    const std::string& url,
                    const oes::core::HTTP::Headers& headers,
                    const std::string& bodyParams
                );

                oes::core::HTTP::Response sendRequestWithBearerToken(
                    oes::core::HTTP::Method method,
                    const std::string& url,
                    oes::core::HTTP::Headers& headers,
                    const std::string& bodyParams,
                    const std::string& bearerToken
                );

                HTTP_ptr _httpReq;
            };

            std::string BaseClient::Impl::paramsToString(const oes::core::HTTP::Fields& params)
            {
                if(params.empty()) return "";
                oes::core::HTTP::Fields::const_iterator pb = params.cbegin(), pe = params.cend();
                std::string data= pb->first+ "="+ pb->second;
                ++ pb; if(pb == pe) return data;
                for(; pb!= pe; ++ pb)
                    data+= "&"+ pb-> first+ "="+ pb-> second;
                return data;
            }

            oes::core::HTTP::Response BaseClient::Impl::sendRequest(
                oes::core::HTTP::Method method,
                const std::string& url,
                const oes::core::HTTP::Headers& headers,
                const std::string& bodyParams
            )
            {
                assert(_httpReq);

                // Log request (PII-safe: sanitize URL to remove query params)
                const char* methodStr = methodToString(method);
                std::string sanitizedUrl = sanitizeUrl(url);
                OES_INFO("HTTP Request: {} {} [body_size={}]",
                         methodStr, sanitizedUrl, bodyParams.size());

                auto startTime = std::chrono::steady_clock::now();

                int defaultPort(0);
                oes::core::HTTP::Response res;

                try {
                    switch(method) {
                        case oes::core::HTTP::Method::eGET:
                            res = _httpReq->get(url, defaultPort, headers);
                            break;
                        case oes::core::HTTP::Method::ePOST:
                            res = _httpReq->post(url, defaultPort, headers, bodyParams);
                            break;
                        case oes::core::HTTP::Method::ePUT:
                            res = _httpReq->put(url, defaultPort, headers, bodyParams);
                            break;
                        case oes::core::HTTP::Method::eDELETE:
                            res = _httpReq->del(url, defaultPort, headers);
                            break;
                        default:
                            throw OsduClientException("Unknown request type");
                    }
                } catch (const std::exception& ex) {
                    auto endTime = std::chrono::steady_clock::now();
                    auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                    OES_ERROR("HTTP Request Failed: {} {} [duration={}ms, error={}]",
                              methodStr, sanitizedUrl, durationMs, ex.what());
                    throw;
                }

                auto endTime = std::chrono::steady_clock::now();
                auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

                // Log response (PII-safe: only status code, not response body which may contain PII)
                if (res.code >= 200 && res.code < 300) {
                    OES_INFO("HTTP Response: {} {} - {} [duration={}ms, body_size={}]",
                             methodStr, sanitizedUrl, res.code, durationMs, res.body.size());
                } else {
                    OES_ERROR("HTTP Response: {} {} - {} {} [duration={}ms]",
                              methodStr, sanitizedUrl, res.code, res.status, durationMs);
                }

                return res;
            }

            oes::core::HTTP::Response BaseClient::Impl::sendRequestWithBearerToken(
                oes::core::HTTP::Method method,
                const std::string& url,
                oes::core::HTTP::Headers& headers,
                const std::string& bodyParams,
                const std::string& bearerToken
            )
            {
                std::string token = bearerToken;
                if (!bearerToken.empty() && bearerToken.find("Bearer ") == std::string::npos) {
                    token = "Bearer " + bearerToken;
                }
                headers.push_back("Authorization: " + token);

                return sendRequest(method, url, headers, bodyParams);
            }

            BaseClient::BaseClient(const std::string& dataPartitionId, HTTPFactory httpFactory) :
                _impl(new Impl(httpFactory)), _dataPartitionId(dataPartitionId) {}

            BaseClient::~BaseClient() {
                delete _impl;
            }

            oes::core::HTTP::Response BaseClient::makeRequest(
                oes::core::HTTP::Method method,
                const std::string& bearerToken,
                const std::string& url,
                const std::string& queryParams,
                const oes::core::HTTP::Headers& additionalHeaders,
                const oes::core::HTTP::Fields& params,
                const std::string* body
            )
            {
                oes::core::HTTP::Headers headers {
                    "content-type: application/json",
                };

                if (!_dataPartitionId.empty()) {
                    headers.push_back("data-partition-id: " + _dataPartitionId);
                }

                for(const std::string& header : additionalHeaders) {
                    headers.push_back(header);
                }

                std::string bodyParams;
                if (!body) {
                    bodyParams = Impl::paramsToString(params);
                }

                if (!bearerToken.empty()) {
                    return _impl->sendRequestWithBearerToken(
                            method, url + queryParams, headers, body ? *body : bodyParams, bearerToken);
                } else {
                    throw OsduClientException("There is no strategy to get Bearer token.");
                }
            }
        }
    }
}
