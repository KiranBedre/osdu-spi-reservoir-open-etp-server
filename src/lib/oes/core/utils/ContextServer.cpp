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

#include <atomic>
#include <string>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/trace/span_context.h>
#include <opentelemetry/trace/context.h>
#include <opentelemetry/context/propagation/text_map_propagator.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <opentelemetry/context/runtime_context.h>
#include <opentelemetry/ext/http/server/http_server.h>
#include <opentelemetry/ext/http/client/http_client_factory.h>
#include <opentelemetry/ext/http/common/url_parser.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/ContextServer.h>
#include <oes/core/utils/Otel.h>

namespace http_server_ns = HTTP_SERVER_NS;

namespace oes {
namespace core {

namespace otel_http_client = opentelemetry::ext::http::client;

template<typename T>
class HttpTextMapCarrier: public opentelemetry::context::propagation::TextMapCarrier {
public:
    explicit HttpTextMapCarrier(T &headers);
    HttpTextMapCarrier() = default;

    virtual opentelemetry::nostd::string_view Get(
        opentelemetry::nostd::string_view key) const noexcept override;

    virtual void Set(opentelemetry::nostd::string_view key,
                    opentelemetry::nostd::string_view value) noexcept override;

    T headers_;

    friend class OtelHttpClient;
};

template<typename T>
HttpTextMapCarrier<T>::HttpTextMapCarrier(T &headers)
    : headers_(headers) {}

template<typename T>
opentelemetry::nostd::string_view HttpTextMapCarrier<T>::Get(opentelemetry::nostd::string_view key) const noexcept {
    std::string key_to_compare = key.data();
    // Header's first letter seems to be  automatically capitaliazed by our test http-server, so
    // compare accordingly.
    if (key == opentelemetry::trace::propagation::kTraceParent) {
        key_to_compare = "Traceparent";
    }
    else if (key == opentelemetry::trace::propagation::kTraceState) {
        key_to_compare = "Tracestate";
    }
    
    auto it = headers_.find(key_to_compare);
    if (it != headers_.end())
    {
        return it->second;
    }
    return "";
}

template<typename T>
void HttpTextMapCarrier<T>::Set(opentelemetry::nostd::string_view key,
                                       opentelemetry::nostd::string_view value) noexcept {
    headers_.emplace(std::pair<std::string, std::string>(std::string(key), std::string(value)));
}

template class HttpTextMapCarrier<otel_http_client::Headers>;

class OtelContextRequestHandler : public http_server_ns::HttpRequestCallback {
public:
    virtual int onHttpRequest(http_server_ns::HttpRequest const &request,
                                http_server_ns::HttpResponse &response) override;
};

int OtelContextRequestHandler::onHttpRequest(http_server_ns::HttpRequest const &request,
                                             http_server_ns::HttpResponse &response) {
    // extract context from http header
    std::map<std::string, std::string> &request_headers =
        const_cast<std::map<std::string, std::string> &>(request.headers);
    const HttpTextMapCarrier<std::map<std::string, std::string>> carrier(request_headers);
    auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
    auto current_ctx = opentelemetry::context::RuntimeContext::GetCurrent();
    auto new_context = prop->Extract(carrier, current_ctx);

    auto span_context = opentelemetry::trace::GetSpan(new_context)->GetContext();
    oes::core::OtelSpan::registerContextResource("conn_context", &span_context);
    
    if (request.uri == "/contexturi") {
        response.headers[http_server_ns::CONTENT_TYPE] = http_server_ns::CONTENT_TYPE_TEXT;
        return 200;
    }
    return 404;
}

class OtelHttpServer::Impl_ : public http_server_ns::HttpRequestCallback {
protected:
    http_server_ns::HttpServer server_;
    std::string server_url_;
    int port_;
    std::atomic<bool> is_running_{false};
    std::unique_ptr<OtelContextRequestHandler> handler_;

public:
    Impl_(const std::string& server_name, const int port);

    void AddHandler(const std::string& path);

    void Start();

    void Stop();

    ~Impl_();
};

OtelHttpServer::Impl_::Impl_(const std::string& server_name, const int port)
    : port_{port},
      handler_{std::make_unique<OtelContextRequestHandler>()} {
    server_.setServerName(server_name);
    server_.setKeepalive(false);
}

void OtelHttpServer::Impl_::AddHandler(const std::string& path) {
    server_.addHandler(path, *handler_);
}

void OtelHttpServer::Impl_::Start() {
    if (!is_running_.exchange(true))
    {
        server_.addListeningPort(port_);
        server_.start();
    }
}

void OtelHttpServer::Impl_::Stop() {
    if (is_running_.exchange(false))
    {
      server_.stop();
    }
}

OtelHttpServer::Impl_::~Impl_() {
    Stop();
}

OtelHttpServer::OtelHttpServer(const std::string& server_name, const int port)
    : impl_{new OtelHttpServer::Impl_(server_name, port)} {}

void OtelHttpServer::AddHandler(const std::string& path) {
    this->impl_->AddHandler(path);
}

void OtelHttpServer::Start() {
    this->impl_->Start();
}

void OtelHttpServer::Stop() {
    this->impl_->Stop();
}

OtelHttpServer::~OtelHttpServer() {
    this->impl_->Stop();
}

bool OtelHttpClient::sendContext(std::string&& context_server_url) {
    auto http_client = otel_http_client::HttpClientFactory::CreateSync();
    HttpTextMapCarrier<otel_http_client::Headers> carrier;
    auto prop = opentelemetry::context::propagation::GlobalTextMapPropagator::GetGlobalPropagator();
    auto current_context = opentelemetry::context::RuntimeContext::GetCurrent();
    prop->Inject(carrier, current_context);

    // SSL is not required for this call
    otel_http_client::Result result = http_client->GetNoSsl(context_server_url,
                                                            carrier.headers_);
    auto status_code = result.GetResponse().GetStatusCode();
    
    return (status_code == 200);
}

} // namespace otel
} // namespace oes
