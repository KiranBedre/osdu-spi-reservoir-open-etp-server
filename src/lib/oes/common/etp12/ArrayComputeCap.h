// ============================================================================
// Copyright 2023-2026 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <string>
#include <variant>

namespace oes {
namespace common {
namespace etp12 {

// Endpoint-capability key advertised by a compute-capable (array-compute) build
// in its ServerCapabilities and per-session endpoint capabilities. An
// un-updated store server (e.g. an "OSDU M27" build without the array-compute
// handlers) does NOT advertise this key, so a client must not dispatch the
// custom compute requests (STATS marker) to it
// -- those would be silently mis-handled as a plain GetDataArrayMetadata. The
// client guards every compute route on this advertisement and, when it is
// absent, refuses to route compute to that server.
inline constexpr char SUPPORTED_ARRAY_EXTENSIONS_CAP[] = "supportedArrayExtensions";

// Returns true if an endpoint-capability map advertises the array-compute
// extensions. When present and the value is a string, the comma-separated
// extension list is copied into *out (when non-null). Templated on the map type
// so it can be reused against both ServerCapabilities and OpenSession response
// capabilities without pulling in the heavy generated message headers here.
template <typename CapabilityMap>
inline bool server_advertises_array_extensions(const CapabilityMap& caps, std::string* out = nullptr) {
    auto it = caps.find(SUPPORTED_ARRAY_EXTENSIONS_CAP);
    if (it == caps.end()) {
        return false;
    }
    if (out != nullptr) {
        if (const auto* s = std::get_if<std::string>(&it->second.m_item)) {
            *out = *s;
        }
    }
    return true;
}

} // namespace etp12
} // namespace common
} // namespace oes
