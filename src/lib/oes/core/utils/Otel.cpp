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

#include <variant>
#include <type_traits>
#include <memory>
#include <map>
#include <stdexcept>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/ext/http/client/curl/http_client_curl.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/samplers/always_off_factory.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/trace/propagation/http_trace_context.h>
#include <opentelemetry/context/propagation/global_propagator.h>
#include <curl/curl.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Otel.h>
#include <oes/core/utils/ContextServer.h>

#include <oes/core/utils/config.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>

namespace oes {
namespace core {

namespace otel_http_client = opentelemetry::ext::http::client;

using tracer_t = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>;
using span_t = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>;
using provider_t = std::shared_ptr<opentelemetry::trace::TracerProvider>;
using sspan_opt_t = std::shared_ptr<opentelemetry::trace::StartSpanOptions>;
using context_map_t = std::map<std::string, opentelemetry::trace::SpanContext>;

using init_list_otel_t = std::initializer_list<std::pair<std::string, opentelemetry::common::AttributeValue>>;

OtelConfig::OtelConfig(const oes::core::envvar_map_t& env_map)
    : otelEnabled_ (env_map.at("/telemetry/otelenabled")),
      otelServiceName_ (env_map.at("/telemetry/otelservicename")),
      otelExporterOtlpEndpoint_ (env_map.at("/telemetry/otelexporterotlpendpoint")),
      otelContextServerEnabled_ (env_map.at("/telemetry/otelcontextserverenabled")),
      otelContextServerPort_ (env_map.at("/telemetry/otelcontextserverport")) {}

const std::string& OtelConfig::otelEnabled() const noexcept {
    return otelEnabled_;
}
const std::string& OtelConfig::otelServiceName() const noexcept {
    return otelServiceName_;
}
const std::string& OtelConfig::otelExporterOtlpEndpoint() const noexcept {
    return otelExporterOtlpEndpoint_;
}
const std::string& OtelConfig::otelContextServerEnabled() const noexcept {
    return otelContextServerEnabled_;
}
const std::string& OtelConfig::otelContextServerPort() const noexcept {
    return otelContextServerPort_;
}

class OtelSpan::Impl_ {
public:
    Impl_() = delete;
    explicit Impl_(const std::string& span_name,
          const init_list_otel_t& attrs_list = {});
    Impl_(const std::string& span_name,
          const init_list_otel_t& attrs_list,
          const bool get_parent_context);
    Impl_(const std::string& span_name,
          const init_list_otel_t& attrs_list,
          std::string&& server_url);
    Impl_(const std::string& span_name,
          const init_list_otel_t& attrs_list,
          const Impl_& parent_impl);
    ~Impl_();
    
    span_t& operator->();

    void setAttr(const std::string& key, value_types_t value);
    void addEvent(std::string event);

    void setError(const std::string& message);
    bool sendContext(std::string&& server_url) const;
    void setDefaultService(const std::string& service_name);
    static bool registerContextResource(const std::string& key, const opentelemetry::trace::SpanContext& context);
    static void registerOtelConfig(OtelConfig_ptr config_ptr);

private:
    
    friend class OtelContextRequestHandler;
    friend class OtelSpan;
    
    static provider_t getProvider(std::string&& service_name);
    static void initTracer(const std::string& service_name = "");
    static void cleanupTracer();
    static opentelemetry::nostd::shared_ptr<trace_api::Tracer> getTracer();
    static std::vector<std::string> get_endpoints();

    static void configureTelemetry();
    static bool setOtelServiceName();
    static bool checkBackendConnections();
    static void enableOtel();
    static void disableOtel();
    static void setContextServerStatus(const bool status);
    static sspan_opt_t getStartSpanOptions(const bool get_parent_context);
    static sspan_opt_t getStartSpanOptions(const Impl_& impl);
    static bool checkOtelConfigStatus();

private:
    static bool TELEMETRY_ENABLED_;
    static std::map<std::string, provider_t> m_providers_map_;
    static std::vector<std::string> m_available_endpoints_;
    static bool m_telemetry_configured_;
    static std::string m_default_service_name_;

    provider_t m_provider_;
    tracer_t m_tracer_;
    sspan_opt_t m_options_;
    span_t m_span_;
    opentelemetry::trace::Scope m_scope_;

    static bool TELEMETRY_CONTEXT_SERVER_ON_;
    static std::unique_ptr<context_map_t> context_resources_;
    static std::unique_ptr<oes::core::OtelHttpServer> http_server_;
    static OtelConfig_ptr config_ptr_;
};

bool OtelSpan::Impl_::TELEMETRY_ENABLED_ = false;
std::map<std::string, provider_t> OtelSpan::Impl_::m_providers_map_ = {};
std::vector<std::string> OtelSpan::Impl_::m_available_endpoints_ = {};
bool OtelSpan::Impl_::m_telemetry_configured_ = false;
std::string OtelSpan::Impl_::m_default_service_name_ = "etp_otel_service";

OtelSpan::Impl_::Impl_(const std::string& span_name,
                       const init_list_otel_t& attrs_list)
    : m_provider_ {OtelSpan::Impl_::getProvider("")},
      m_tracer_ {m_provider_->GetTracer("default", OPENTELEMETRY_SDK_VERSION)},
      m_options_ {nullptr},
      m_span_ {m_tracer_->StartSpan(span_name, attrs_list)},
      m_scope_ {m_tracer_->WithActiveSpan(m_span_)} {}

OtelSpan::Impl_::Impl_(const std::string& span_name,
                       const init_list_otel_t& attrs_list,
                       const bool get_parent_context)
    : m_provider_ {OtelSpan::Impl_::getProvider("")},
      m_tracer_ {m_provider_->GetTracer("default", OPENTELEMETRY_SDK_VERSION)},
      m_options_{OtelSpan::Impl_::getStartSpanOptions(get_parent_context)},
      m_span_ {m_tracer_->StartSpan(span_name, attrs_list, *m_options_)},
      m_scope_ {m_tracer_->WithActiveSpan(m_span_)} {}

OtelSpan::Impl_::Impl_(const std::string& span_name,
                       const init_list_otel_t& attrs_list,
                       std::string&& server_url)
    : Impl_(span_name, attrs_list) {

    if (!OtelSpan::Impl_::TELEMETRY_ENABLED_ || !OtelSpan::Impl_::TELEMETRY_CONTEXT_SERVER_ON_) {
        return;
    }
    if (!this->sendContext(std::move(server_url))) {
        OES_ERROR("[Otel] Could not send span context to context server!");
    }
}

OtelSpan::Impl_::Impl_(const std::string& span_name,
          const init_list_otel_t& attrs_list,
          const Impl_& parent_impl)
          : m_provider_ {OtelSpan::Impl_::getProvider("")},
            m_tracer_ {m_provider_->GetTracer("default", OPENTELEMETRY_SDK_VERSION)},
            m_options_{OtelSpan::Impl_::getStartSpanOptions(parent_impl)},
            m_span_ {m_tracer_->StartSpan(span_name, attrs_list, *m_options_)},
            m_scope_ {m_tracer_->WithActiveSpan(m_span_)} {}

OtelSpan::Impl_::~Impl_() {
    m_span_->End();
}

span_t& OtelSpan::Impl_::operator->() {
    return m_span_;
}

struct VariantVisitor
{
    bool operator()(bool b) const { 
        return b;
    }
    size_t operator()(unsigned int i) const {
        return static_cast<size_t>(i);
    }
    int operator()(int i) const { 
        return i;
    }
    size_t operator()(size_t i) const { 
        return i;
    }
    double operator()(double d) const { 
        return d;
    }
    const char* operator()(const char* cstr) const { 
        return cstr;
    }
    std::string operator()(const std::string& s) const { 
        return s;
    }
};

void OtelSpan::Impl_::setAttr(const std::string& key, value_types_t value) {
    opentelemetry::common::AttributeValue value_tmp;

    static VariantVisitor vis{};
    static_cast<void>(vis);

    std::visit([&value_tmp](auto arg) -> void {
        value_tmp = vis(arg);
    }, value);
    this->m_span_->SetAttribute(key, value_tmp);
}

void OtelSpan::Impl_::addEvent(std::string event) {
    m_span_->AddEvent(event);
}

void OtelSpan::Impl_::setError(const std::string& message){
    m_span_->AddEvent(message, {{"log.level", "error"}});
    m_span_->SetStatus(opentelemetry::trace::StatusCode::kError);
}

bool OtelSpan::Impl_::sendContext(std::string&& server_url) const {
    std::string context_server_url;
    const auto first_colon_pos = server_url.find_first_of(':');
    const auto last_colon_pos = server_url.find_last_of(':');
    if ((first_colon_pos < last_colon_pos) && (last_colon_pos < server_url.length())) {
        context_server_url = "http" + server_url.substr(first_colon_pos, last_colon_pos - first_colon_pos + 1);
    } else {
        return false;
    }
    
    if(!OtelSpan::Impl_::checkOtelConfigStatus()) {
        return false;
    }
    std::string context_server_port = OtelSpan::Impl_::config_ptr_->otelContextServerPort();
    if (context_server_port.empty()) {
        context_server_port = "8080";
    }
    context_server_url += context_server_port + std::string("/contexturi");

    return OtelHttpClient::sendContext(std::move(context_server_url));
}

void OtelSpan::Impl_::setDefaultService(const std::string& service_name) {
    if (service_name.empty()) {
        OES_ERROR("[Otel] Service name must not be empty!");
        return;
    }

    OtelSpan::Impl_::m_default_service_name_ = service_name;
}

bool OtelSpan::Impl_::TELEMETRY_CONTEXT_SERVER_ON_ = false;
std::unique_ptr<context_map_t> OtelSpan::Impl_::context_resources_ = nullptr;
std::unique_ptr<oes::core::OtelHttpServer> OtelSpan::Impl_::http_server_ = nullptr;
OtelConfig_ptr OtelSpan::Impl_::config_ptr_ = nullptr;

provider_t OtelSpan::Impl_::getProvider(std::string&& service_name) {
    if (!OtelSpan::Impl_::m_telemetry_configured_) {
        OtelSpan::Impl_::configureTelemetry();
    }

    if (service_name.empty()) {
        service_name = OtelSpan::Impl_::m_default_service_name_;
        //OES_WARNING("[Otel] Given service_name is empty. Using default service name \"{}\"", OtelSpan::Impl_::m_default_service_name_);
    }
    
    if (OtelSpan::Impl_::m_providers_map_.find(service_name) == OtelSpan::Impl_::m_providers_map_.end()) {
        OtelSpan::Impl_::initTracer(service_name);
    }

    return OtelSpan::Impl_::m_providers_map_[service_name];
}

void OtelSpan::Impl_::initTracer(const std::string& service_name) {
    // Check if provider already exists.
    if (OtelSpan::Impl_::m_providers_map_.find(service_name) != OtelSpan::Impl_::m_providers_map_.end()){
        return;
    }

    opentelemetry::sdk::resource::ResourceAttributes attributes = {{"service.name", service_name}};
    auto resource = opentelemetry::sdk::resource::Resource::Create(attributes);

    std::vector<std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>> processors;
    for (const auto& url : OtelSpan::Impl_::m_available_endpoints_) {
        opentelemetry::exporter::otlp::OtlpHttpExporterOptions opts;

        opts.url = url + "/v1/traces";
        auto exporter  = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(opts);
        auto processor = opentelemetry::sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));

        processors.push_back(std::move(processor));
    }

    std::shared_ptr<opentelemetry::trace::TracerProvider> new_provider;

    if (OtelSpan::Impl_::TELEMETRY_ENABLED_) {
        new_provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processors),
                                                                                resource);
    }
    if (!new_provider || !OtelSpan::Impl_::TELEMETRY_ENABLED_) {
        auto always_off = opentelemetry::sdk::trace::AlwaysOffSamplerFactory::Create();
        new_provider = opentelemetry::sdk::trace::TracerProviderFactory::Create(std::move(processors),
                                                                                resource,
                                                                                std::move(always_off));
    }
    OtelSpan::Impl_::m_providers_map_[service_name] = new_provider;
    // Set the global trace 
    opentelemetry::trace::Provider::SetTracerProvider(new_provider);

    // Set global propagator
    opentelemetry::context::propagation::GlobalTextMapPropagator::SetGlobalPropagator(
        opentelemetry::nostd::shared_ptr<opentelemetry::context::propagation::TextMapPropagator>(
            new opentelemetry::trace::propagation::HttpTraceContext()
        )
    );
}

void OtelSpan::Impl_::cleanupTracer() {
    // We call ForceFlush to prevent to cancel running exportings, It's optional.
    for (auto& item : OtelSpan::Impl_::m_providers_map_) {
        if(item.second) {
            auto tracer_provider = dynamic_cast<opentelemetry::sdk::trace::TracerProvider*>(item.second.get());
            if(tracer_provider) {
                tracer_provider->ForceFlush();
            }
        }
    }
    OtelSpan::Impl_::m_providers_map_.clear();

    std::shared_ptr<opentelemetry::trace::TracerProvider> none;
    opentelemetry::trace::Provider::SetTracerProvider(none);

    // cleanup server
    if (OtelSpan::Impl_::http_server_) {
        OtelSpan::Impl_::setContextServerStatus(false); // will stop the server and cleanup OtelSpan::Impl_::http_server_
    }
}

opentelemetry::nostd::shared_ptr<trace_api::Tracer> OtelSpan::Impl_::getTracer() {
    auto provider = trace_api::Provider::GetTracerProvider();
    return provider->GetTracer("default", OPENTELEMETRY_SDK_VERSION);
}

std::vector<std::string> OtelSpan::Impl_::get_endpoints() {
    if (!OtelSpan::Impl_::checkOtelConfigStatus()) {
        return {};
    }

    auto endpoints_str = OtelSpan::Impl_::config_ptr_->otelExporterOtlpEndpoint();
    if (endpoints_str.empty()){
        return {};
    }

    std::vector<std::string> endpoints;
    std::string tmp_str;
    std::stringstream ss_endpoints {std::string(endpoints_str)};

    while(std::getline(ss_endpoints, tmp_str, ' ')) {
        if (!tmp_str.empty()) {
            endpoints.push_back(tmp_str);
        }
    }

    return endpoints;
}

void OtelSpan::Impl_::configureTelemetry() {
    OtelSpan::Impl_::m_telemetry_configured_ = true;

    // Check OTEL_ENABLED env var
    if(!OtelSpan::Impl_::checkOtelConfigStatus()) {
        OtelSpan::Impl_::disableOtel();
        return;
    }
    
    std::string enable_otel = OtelSpan::Impl_::config_ptr_->otelEnabled();
    if (enable_otel == "false" || enable_otel.empty()) {
        OtelSpan::Impl_::disableOtel();
        return;
    } else if (enable_otel != "true") {
        OES_ERROR("[Otel] OTEL_ENABLED envvar must be true or false");
        OtelSpan::Impl_::disableOtel();
        return;
    }

    if (OtelSpan::Impl_::setOtelServiceName() && OtelSpan::Impl_::checkBackendConnections()) {
        OtelSpan::Impl_::enableOtel();
        
        auto context_server_enabled = OtelSpan::Impl_::config_ptr_->otelContextServerEnabled();
        if (context_server_enabled == "true") {
            OtelSpan::Impl_::setContextServerStatus(true);
        }

        if (OtelSpan::Impl_::TELEMETRY_CONTEXT_SERVER_ON_) {
            auto context_server_port = OtelSpan::Impl_::config_ptr_->otelContextServerPort();
            if (context_server_port.empty()) {
                context_server_port = "8080";
            }
            int port{};
            try {
                port = std::stoi(context_server_port);
            } catch (std::invalid_argument& e) {
                OES_DEBUG("[Otel] {}", e.what());
                OES_WARNING("[Otel] Could not use port {}. Using 8080 for context server.", context_server_port);
                port = 8080;
            }

            OtelSpan::Impl_::context_resources_ = std::make_unique<context_map_t>();
            OtelSpan::Impl_::http_server_ = std::make_unique<oes::core::OtelHttpServer>("test-context-server", port);
            OtelSpan::Impl_::http_server_->AddHandler("/contexturi");
            OtelSpan::Impl_::http_server_->Start();
        }
    } else {
        OtelSpan::Impl_::disableOtel();
        return;
    }

    // Register cleanup
    auto reg_status = std::atexit(OtelSpan::Impl_::cleanupTracer);
    if (reg_status != 0) {
        OES_ERROR("[Otel] Could not register cleanup function.");
    }
}

bool OtelSpan::Impl_::setOtelServiceName() {
    if (!OtelSpan::Impl_::checkOtelConfigStatus()) {
        return false;
    }
    std::string service_name = OtelSpan::Impl_::config_ptr_->otelServiceName();
    if (service_name.empty()) {
        OES_INFO("[Otel] Environment variable OTEL_SERVICE_NAME is not set or is empty.");
        return false;
    }
    
    OtelSpan::Impl_::m_default_service_name_ = service_name;
    return true;
}

bool OtelSpan::Impl_::checkBackendConnections() {
    auto endpoints {get_endpoints()};
    if (endpoints.empty()){
        return false;
    }

    for (const auto& req_uri : endpoints) {
        const size_t last_colon_position { req_uri.find_last_of(":") };
        if (last_colon_position >= req_uri.length()) {
            continue;
        }
        std::string req_url { req_uri.substr(0, last_colon_position) };
        const short port { static_cast<short>(std::stoi(req_uri.substr(last_colon_position + 1))) };

        auto curl { curl_easy_init() };

        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_URL, req_url.c_str());
        curl_easy_setopt(curl, CURLOPT_PORT, port);

        const CURLcode outCode { curl_easy_perform(curl) };
        curl_easy_reset(curl);
        curl_easy_cleanup(curl);

        if (outCode == CURLE_OK) {
            OtelSpan::Impl_::m_available_endpoints_.push_back(req_uri);
        }
    }

    return (!OtelSpan::Impl_::m_available_endpoints_.empty());
}

void OtelSpan::Impl_::enableOtel() {
    OtelSpan::Impl_::TELEMETRY_ENABLED_ = true;
    OtelSpan::m_ignore_spans_ = false;
// #ifdef OES_DEBUG_BUILD /// Failing e2e test
//     OES_INFO("Telemetry is enabled.\n"));
// #endif
}

void OtelSpan::Impl_::disableOtel() {
    OtelSpan::Impl_::TELEMETRY_ENABLED_ = false;
    OtelSpan::m_ignore_spans_ = true;
// #ifdef OES_DEBUG_BUILD // Failing e2e test
//     OES_INFO("Telemetry is disabled.\n"));
// #endif
}

bool OtelSpan::Impl_::registerContextResource(const std::string& key, const opentelemetry::trace::SpanContext& context) {
    OtelSpan::Impl_::context_resources_->insert(std::make_pair(key, context));
    return true;
}

void OtelSpan::Impl_::registerOtelConfig(OtelConfig_ptr config_ptr) {
    if (!config_ptr) {
        OES_ERROR("[Otel] Could not register otel configuration.");
        return;
    }
    OtelSpan::Impl_::config_ptr_ = config_ptr;
}

void OtelSpan::Impl_::setContextServerStatus(const bool status) {
    OtelSpan::Impl_::TELEMETRY_CONTEXT_SERVER_ON_ = status;

    if (!status && OtelSpan::Impl_::http_server_) {
        OtelSpan::Impl_::http_server_.reset(nullptr);
    }
}

sspan_opt_t OtelSpan::Impl_::getStartSpanOptions(const bool get_parent_context) {
    
    if (!get_parent_context) { // || !OtelSpan::TELEMETRY_CONTEXT_SERVER_ON_) { // or server is off
        return nullptr;
    }
    
    sspan_opt_t options { std::make_shared<opentelemetry::trace::StartSpanOptions>() };
    if (OtelSpan::Impl_::context_resources_) {
        // consume the resource
        if (OtelSpan::Impl_::context_resources_->find("conn_context") != OtelSpan::Impl_::context_resources_->end()) {
            options->parent = OtelSpan::Impl_::context_resources_->at("conn_context");
            OtelSpan::Impl_::context_resources_->erase("conn_context");
        }
    }
    return options;
}

sspan_opt_t OtelSpan::Impl_::getStartSpanOptions(const OtelSpan::Impl_& impl) {
    
    auto context = impl.m_span_->GetContext();
    
    sspan_opt_t options { std::make_shared<opentelemetry::trace::StartSpanOptions>() };
    options->parent = context;

    return options;
}

bool OtelSpan::Impl_::checkOtelConfigStatus() {
    if (!OtelSpan::Impl_::config_ptr_) {
        OES_ERROR("[Otel] Otel configuration is not initialized. OtelSpan::registerOtelConfig must be called before the first call.");
        return false;
    }
    return true;
}

// OtelSpan
bool OtelSpan::m_ignore_spans_ = false;

OtelSpan::OtelSpan(const std::string& span_name,
                   init_list_t attrs_list)
    : impl_ { OtelSpan::m_ignore_spans_ ? nullptr : std::make_unique<OtelSpan::Impl_>(span_name, init_list_otel_t())} {
        if (impl_) {
            this->setAttrs(attrs_list);
        }
    }

OtelSpan::OtelSpan(const std::string& span_name,
                   init_list_t attrs_list,
                   const bool get_parent_context)
    : impl_ { OtelSpan::m_ignore_spans_ ? nullptr : std::make_unique<OtelSpan::Impl_>(span_name, init_list_otel_t(), get_parent_context)} {
        if (impl_) {
            this->setAttrs(attrs_list);
        }
    }

OtelSpan::OtelSpan(const std::string& span_name,
                   init_list_t attrs_list,
                   std::string&& server_url)
    : impl_ { OtelSpan::m_ignore_spans_ ? nullptr : std::make_unique<OtelSpan::Impl_>(span_name, init_list_otel_t(), std::move(server_url))} {
        if (impl_) {
            this->setAttrs(attrs_list);
        }
    }

OtelSpan::OtelSpan(const std::string& span_name,
                   init_list_t attrs_list,
                   const OtelSpan& parent_span)
    : impl_ { OtelSpan::m_ignore_spans_ ? nullptr : std::make_unique<OtelSpan::Impl_>(span_name, init_list_otel_t(), *parent_span.impl_)} {
        if (impl_) {
            this->setAttrs(attrs_list);
        }
    }

OtelSpan::~OtelSpan() = default;

void OtelSpan::setAttr(const std::string& key, value_types_t value) {
    if (impl_) {
        impl_->setAttr(key, value);
    }
}

template<typename T>
void OtelSpan::setAttrs(std::initializer_list<std::pair<std::string, T>> attrs_list){
    if (!impl_) {
        return;
    }
    for (auto& item : attrs_list) {
        //@impl_->setAttr(item.first, item.second);
        this->setAttr(item.first, item.second);
    }
}

OtelSpan& operator<<(OtelSpan& span, const std::string& event) {
    if (span.impl_) {
        span.impl_->addEvent(event);
    }
    return span;
}

void OtelSpan::setError(const std::string& message){
    if (impl_) {
        impl_->setError(message);
    }
}

bool OtelSpan::registerContextResource(const std::string& key, void *context) {
    return OtelSpan::Impl_::registerContextResource(key, *static_cast<opentelemetry::trace::SpanContext*>(context));
}

void OtelSpan::registerOtelConfig(OtelConfig_ptr config_ptr) {
    OtelSpan::Impl_::registerOtelConfig(config_ptr);
}

} // namespace otel
} // namespace oes