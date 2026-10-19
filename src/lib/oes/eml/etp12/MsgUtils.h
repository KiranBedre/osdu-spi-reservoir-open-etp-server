// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_EML_ETP12_MSGUTILS_H
#define OES_EML_ETP12_MSGUTILS_H

#include <oes/eml/export.h>
#include <oes/core/utils/fwd.h>

#include <oes/common/etp12/Messages.h>

#include <map>
#include <optional>
#include <string>

namespace oes::eml::etp12 {

namespace Datatypes = ::oes::common::etp12::Datatypes;
using XDataMap = std::map<std::string, Datatypes::DataValue>;

struct XDataMapBuilder {
    XDataMap& xmap_;

    XDataMapBuilder(const XDataMapBuilder&) = delete;
    XDataMapBuilder(XDataMapBuilder&&) = delete;

    XDataMapBuilder& operator=(const XDataMapBuilder&) = delete;
    XDataMapBuilder& operator=(XDataMapBuilder&&) = delete;

#undef  XDATA_MAP_BUILDER_EMPLACE
#define XDATA_MAP_BUILDER_EMPLACE(type) \
    inline bool emplace(std::string k, type v) { \
        Datatypes::DataValue dv; \
        dv.m_item = std::move(v); \
        auto rc = xmap_.emplace(std::move(k), std::move(dv)); \
        return rc.second; \
    }

#undef  XDATA_MAP_BUILDER_EMPLACE_VEC
#define XDATA_MAP_BUILDER_EMPLACE_VEC(name, type) \
    inline bool emplace(std::string k, std::vector<type> v) { \
        Datatypes::DataValue dv; \
        dv.m_item.emplace<Datatypes::name>().m_values.swap(v); \
        auto rc = xmap_.emplace(std::move(k), std::move(dv)); \
        return rc.second; \
    }

    // Primitives
    XDATA_MAP_BUILDER_EMPLACE(bool)
    XDATA_MAP_BUILDER_EMPLACE(int32_t)
    XDATA_MAP_BUILDER_EMPLACE(int64_t)
    XDATA_MAP_BUILDER_EMPLACE(float)
    XDATA_MAP_BUILDER_EMPLACE(double)
    XDATA_MAP_BUILDER_EMPLACE(std::string)

    // Arrays (w/o nulls)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfBoolean, bool)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfInt, int32_t)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfLong, int64_t)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfFloat, float)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfDouble, double)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfString, std::string)

    XDATA_MAP_BUILDER_EMPLACE(std::vector<uint8_t>)

    // Arrays (w/o nulls)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfNullableBoolean, std::optional<bool>)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfNullableInt, std::optional<int32_t>)
    XDATA_MAP_BUILDER_EMPLACE_VEC(ArrayOfNullableLong, std::optional<int64_t>)

    // TODO: AnySparseArray

#undef  XDATA_MAP_BUILDER_EMPLACE
#undef  XDATA_MAP_BUILDER_EMPLACE_VEC
};

//! \name Serialize XDataMap
//@{
OES_EML_EXPORT
std::string xdataToJson(const XDataMap& xmap);

OES_EML_EXPORT
std::vector<uint8_t> xdataToAvroBinary(const XDataMap& xmap);

OES_EML_EXPORT
std::pair<std::string, std::vector<uint8_t>> xdataToJsonAndAvroBinary(
    const XDataMap& xmap, bool add_hints = true
);
//@}

//! \name Deserialize XDataMap
//@{
OES_EML_EXPORT
XDataMap xdataFromJson(const std::string& xdata_json);

OES_EML_EXPORT
size_t xdataFromAvroBinary(XDataMap& xmap, const std::vector<uint8_t>& xdata_avro);

OES_EML_EXPORT
bool xdataFromJson(XDataMap& xmap, const std::string& xdata_json);

OES_EML_EXPORT
bool xdataFromJsonAndAvroBinary(
    XDataMap& xmap,
    const std::string& xdata_json,
    const std::vector<uint8_t>& xdata_avro
);

OES_EML_EXPORT
bool xdataFromJsonFile(XDataMap& xmap, const std::string& filename);
//@}

OES_EML_EXPORT
bool dataValueAsJson(
    oes::core::JSONWriter& json, bool omit_unsupported,
    const std::string& k, const Datatypes::DataValue& dv
);

OES_EML_EXPORT
std::string xdataTypeHint(const Datatypes::DataValue& dv);

OES_EML_EXPORT
std::string arrayTypeHint(const Datatypes::AnyArray& ary);

OES_EML_EXPORT
bool xdataEmplace(XDataMap& xmap, std::string key, std::string str);

OES_EML_EXPORT
bool xdataEquals(const Datatypes::DataValue& lhs, const Datatypes::DataValue& rhs);

OES_EML_EXPORT
bool xdataLessThan(const Datatypes::DataValue& lhs, const Datatypes::DataValue& rhs);

} // namespace oes::eml::etp12

namespace oes::common::etp12::Datatypes {

inline
bool operator==(const DataValue& lhs, const DataValue& rhs) {
    return oes::eml::etp12::xdataEquals(lhs, rhs);
}

inline
bool operator<(const DataValue& lhs, const DataValue& rhs) {
    return oes::eml::etp12::xdataLessThan(lhs, rhs);
}

} // namespace Datatypes


#endif
