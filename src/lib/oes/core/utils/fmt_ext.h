// ============================================================================
// Copyright (c) Microsoft.
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

#include <oes/client/Hdf5.h>
#include <oes/common/HdfAry.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <websocketpp/http/constants.hpp>
#include <fmt/base.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

// Extensions for the fmt library to properly format enums
namespace fmt {

using oes::common::hdf::HdfAryType;

namespace {

std::string H5T_class_tToString(const H5T_class_t t) {
    switch (t) {
    case H5T_NO_CLASS:
        return "H5T_NO_CLASS";
    case H5T_INTEGER:
        return "H5T_INTEGER";
    case H5T_FLOAT:
        return "H5T_FLOAT";
    case H5T_TIME:
        return "H5T_TIME";
    case H5T_STRING:
        return "H5T_STRING";
    case H5T_BITFIELD:
        return "H5T_BITFIELD";
    case H5T_OPAQUE:
        return "H5T_OPAQUE";
    case H5T_COMPOUND:
        return "H5T_COMPOUND";
    case H5T_REFERENCE:
        return "H5T_REFERENCE";
    case H5T_ENUM:
        return "H5T_ENUM";
    case H5T_VLEN:
        return "H5T_VLEN";
    case H5T_ARRAY:
        return "H5T_ARRAY";
    case H5T_NCLASSES:
        return "H5T_NCLASSES";
    default:
        // Returns the underlying integer as a string
        return std::to_string(t);
    }
}

std::string HdfAryTypeToString(const HdfAryType t) {
    switch (t) {
    case HdfAryType::eAryOfVoid:
        return "AryOfVoid";
    case HdfAryType::eAryOfInt:
        return "AryOfInt";
    case HdfAryType::eAryOfDouble:
        return "AryOfDouble";
    case HdfAryType::eAryOfFloat:
        return "AryOfFloat";
    case HdfAryType::eAryOfBytes:
        return "AryOfBytes";
    case HdfAryType::eAryOfBoolean:
        return "AryOfBoolean";
    case HdfAryType::eAryOfLong:
        return "AryOfLong";
    case HdfAryType::eAryOfString:
        return "AryOfString";
    default:
        // Returns the underlying integer as a string
        return std::to_string(t);
    }
}

} // namespace

template <> struct formatter<H5T_class_t>: formatter<fmt::string_view> {
    inline auto format(H5T_class_t t, format_context& ctx) const -> format_context::iterator {
        return formatter<string_view>::format(H5T_class_tToString(t), ctx);
    }
};

template <> struct formatter<oes::common::hdf::HdfAryType>: formatter<string_view> {
    inline auto format(HdfAryType t, format_context& ctx) const -> format_context::iterator {
        return formatter<string_view>::format(HdfAryTypeToString(t), ctx);
    }
};

template <> struct formatter<websocketpp::http::status_code::value>: formatter<string_view> {
    inline auto format(websocketpp::http::status_code::value v, format_context& ctx) const -> format_context::iterator {
        return formatter<string_view>::format(websocketpp::http::status_code::get_string(v), ctx);
    }
};

} // namespace fmt
