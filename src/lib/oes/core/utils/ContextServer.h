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

#include <memory>
#include <atomic>
#include <string>

#include <oes/core/utils/Otel.h>

namespace oes {
namespace core {

// OtelHttpServer - Simple server to receive the otel context from the client.
class OtelHttpServer {
public:
    OtelHttpServer(const std::string& server_name = "context_server", const int port = 8080);
    ~OtelHttpServer();

    void AddHandler(const std::string& path);

    void Start();
    void Stop();
private:
    class Impl_;
    std::unique_ptr<Impl_> impl_;
};

class OtelHttpClient {
public:
    OtelHttpClient() = delete;
    static bool sendContext(std::string&& context_server_url);
};

} // namespace otel
} // namespace oes