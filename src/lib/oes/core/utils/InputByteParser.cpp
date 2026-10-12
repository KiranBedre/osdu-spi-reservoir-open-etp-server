// ============================================================================
// Copyright 2025 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <sstream>


#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "InputByteParser.h"

namespace oes::core {

size_t parseByteSize(const std::string& spec, double limit_bytes) {
    std::stringstream ss(spec);

    double val = 0;
    ss >> val;
    if (!ss) {
        throw std::runtime_error("Expected leading number in " + spec);
    }

    if (val < 0 || (limit_bytes > 0 && val > limit_bytes)) {
        throw std::runtime_error(fmt::format("Unexpected size: {}", val));
    }

    std::string uom;
    ss >> uom;

    if (uom.empty()) {
        // No unit, assume bytes directly
        return static_cast<size_t>(val);
    }

    if (uom.back() != 'B') {
        throw std::runtime_error(fmt::format("Invalid uom: {}: Must end with B, for Byte", uom));
    }

    const bool power_of_2 = (uom.size() == 3 && uom[1] == 'i');

    if (!(uom.size() == 1 || uom.size() == 2 || power_of_2)) {
        throw std::runtime_error(fmt::format("Invalid uom: {}. Use KB, MB, GB; KiB, MiB, GiB", uom));
    }

    const double factor = power_of_2 ? 1024: 1000;

    switch (uom[0]) {
    case 'T': val *= factor; [[fallthrough]];
    case 'G': val *= factor; [[fallthrough]];
    case 'M': val *= factor; [[fallthrough]];
    case 'K': val *= factor; [[fallthrough]];
    case 'B': break;
    default:
        throw std::runtime_error(fmt::format("Invalid uom: {}. Use KB, MB, GB; KiB, MiB, GiB", uom));
    }

    if ( limit_bytes > 0 && val > limit_bytes) {
        throw std::runtime_error(fmt::format("Size too large (>{}B): {}", limit_bytes, val));
    }

    return static_cast<size_t>(val);
}

} // oes::core