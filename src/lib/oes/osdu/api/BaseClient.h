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

#ifndef OES_OSDU_API_BASECLIENT_H
#define OES_OSDU_API_BASECLIENT_H

#include <string>
#include <map>
#include <vector>
#include <memory>

#include <oes/core/http/HTTP.h>
#include <oes/osdu/export.h>

namespace oes {
    namespace osdu {
        namespace api {

            using HTTP_ptr = std::shared_ptr<oes::core::HTTP>;
            using HTTPFactory = std::function<HTTP_ptr()>;
           
            class OES_OSDU_EXPORT BaseClient
            {
            public:
                explicit BaseClient(const std::string& dataPartitionId = std::string(), HTTPFactory httpFactory = {});
                BaseClient(const BaseClient&) = delete;
                virtual ~BaseClient();

                BaseClient& operator=(BaseClient const&) = delete;

                virtual oes::core::HTTP::Response makeRequest(
                    oes::core::HTTP::Method method,
                    const std::string& bearerToken,
                    const std::string& url,
                    const std::string& queryParams = std::string(),
                    const oes::core::HTTP::Headers& additionalHeaders = std::vector<std::string>(),
                    const oes::core::HTTP::Fields& params = std::map<std::string, std::string>(),
                    const std::string* body = nullptr
               );

            private:
                struct Impl;
                Impl* _impl;

                const std::string _dataPartitionId;
            };
        } // namespace api
    } // namespace osdu
} // namespace oes

#endif //OES_OSDU_API_BASECLIENT_H
