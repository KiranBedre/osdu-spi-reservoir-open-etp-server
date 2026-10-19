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

#include <variant>
#include <type_traits>
#include <memory>
#include <map>
#include <string>

#include <oes/core/utils/export.h>
#include <oes/core/utils/ContextServer.h>
#include <oes/core/utils/AppConfigReader.h>

namespace oes {
namespace core {

using value_types_t = std::variant<bool,
                                   unsigned int,
                                   int,
                                   std::size_t,
                                   double,
                                   const char*,
                                   //std::string_view,
                                   std::string>;

using init_list_t = std::initializer_list<std::pair<std::string, value_types_t>>;

class OES_CORE_UTILS_EXPORT OtelConfig {
public:
    OtelConfig() = delete;
    explicit OtelConfig(const envvar_map_t& env_map);
    virtual ~OtelConfig() = default;

    const std::string& otelEnabled() const noexcept;
    const std::string& otelServiceName() const noexcept;
    const std::string& otelExporterOtlpEndpoint() const noexcept;
    const std::string& otelContextServerEnabled() const noexcept;
    const std::string& otelContextServerPort() const noexcept;
private:
    std::string otelEnabled_;
    std::string otelServiceName_;
    std::string otelExporterOtlpEndpoint_;
    std::string otelContextServerEnabled_;
    std::string otelContextServerPort_;
};

using OtelConfig_ptr = std::shared_ptr<OtelConfig>;

class OES_CORE_UTILS_EXPORT OtelSpan {
public:
    OtelSpan() = delete;
    explicit OtelSpan(const std::string& span_name,
             init_list_t attrs_list = {});
    OtelSpan(const std::string& span_name,
             init_list_t attrs_list,
             const bool get_parent_context);
    OtelSpan(const std::string& span_name,
             init_list_t attrs_list,
             std::string&& server_url);
    OtelSpan(const std::string& span_name,
             init_list_t attrs_list,
             const OtelSpan& parent_span);
    ~OtelSpan();
    
    void setAttr(const std::string& key, value_types_t value);
    template<typename T>
    void setAttrs(std::initializer_list<std::pair<std::string, T>> attrs_list);
    
    // operator<< to set events!
    friend OES_CORE_UTILS_EXPORT OtelSpan& operator<<(OtelSpan& span, const std::string& event);
    void setError(const std::string& message);
    bool sendContext(std::string&& server_url) const;
    static bool registerContextResource(const std::string& key, void * context);
    static void registerOtelConfig(OtelConfig_ptr config_ptr);
    
private:
    friend class OtelContextRequestHandler;
    
private:
    class Impl_;
    std::unique_ptr<Impl_> impl_;
    static bool m_ignore_spans_;
};

} // namespace otel
} // namespace oes