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

#include <sstream>
#include <fstream>
#include <iomanip>
#include <limits>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Json.h>

#include "MsgUtils.h"

#include <oes/common/etp12/Messages.h>

#include <oes/common/AvroBinary.h>
#include <oes/common/AvroUtils.h>


namespace {

using namespace oes::common::etp12::Datatypes;

using bytes = std::vector<uint8_t>;

template <typename TAry>
decltype(TAry::m_values)& ary_as(AnyArray& ary) {
    return std::get<TAry>(ary.m_item).m_values;
}

template <typename TAry>
const decltype(TAry::m_values)& ary_as(const AnyArray& ary) {
    return std::get<TAry>(ary.m_item).m_values;
}

template <typename TVal>
TVal& val_as(DataValue& val) {
    return std::get<TVal>(val.m_item);
}

template <typename TVal>
const TVal& val_as(const DataValue& val) {
    return std::get<TVal>(val.m_item);
}

template <typename TAry>
decltype(TAry::m_values)& ary_as(DataValue& val) {
    return std::get<TAry>(val.m_item).m_values;
}

template <typename TAry>
const decltype(TAry::m_values)& ary_as(const DataValue& val) {
    return std::get<TAry>(val.m_item).m_values;
}

} // namespace

namespace oes::eml::etp12 {

/*!
 * \brief Partial conversion of an ETP DataValue to JSON.
 *
 * Persists in JSON the subset of DataValue types which are scalar,
 * wrapping ambiguously-typed ones in an object with an explicit type.
 * All others are expected to be persisted in Avro-binary instead,
 * for full type-fidelity, but if \a omit_unsupported is false,
 * the key and type of those values will be persisted.
 *
 * \return true if the value was fully output; false otherwise, and
 *     either nothing was output if \a omit_unsupported is true, or
 *     just the key and type of the value was output if false.
 */
bool dataValueAsJson(
    oes::core::JSONWriter& json, bool omit_unsupported,
    const std::string& k, const DataValue& v
) {
    // TODO: Use std::visit instead
    switch (v.m_item.index()) {
    case size_t(DataValue_item_e::eNull):
        json.key(k).json_null();
        return true;

    case size_t(DataValue_item_e::eBool):
        json.key(k).json_bool(std::get<bool>(v.m_item));
        return true;

    case size_t(DataValue_item_e::eInt):
        json.key(k)
        .push_object()
            .key("T").string("int")
            .key("V").number(std::get<int>(v.m_item))
        .pop_object();
        return true;

    case size_t(DataValue_item_e::eLong):
        json.key(k)
        .push_object()
            .key("T").string("long")
            .key("V").number(std::get<int64_t>(v.m_item))
        .pop_object();
        return true;

    case size_t(DataValue_item_e::eFloat):
        json.key(k)
        .push_object()
            .key("T").string("float")
            .key("V")
        ;
        if (std::isfinite(std::get<float>(v.m_item))) {
            json.number(std::get<float>(v.m_item));
        } else {
            switch (std::fpclassify(std::get<float>(v.m_item))) {
            case FP_INFINITE:  json.string("Inf"); break;
            case FP_NAN:       json.string("NaN"); break;
            default:           json.json_null(); assert(false); break;
            }
        }
        json.pop_object();
        return true;

    case size_t(DataValue_item_e::eDouble):
        if (std::isfinite(std::get<double>(v.m_item))) {
            json.key(k).number(std::get<double>(v.m_item));
        } else {
            json.key(k)
            .push_object()
                .key("T").string("double")
                .key("V")
            ;
            switch (std::fpclassify(std::get<double>(v.m_item))) {
            case FP_INFINITE:  json.string("Inf"); break;
            case FP_NAN:       json.string("NaN"); break;
            default:           json.json_null(); assert(false); break;
            }
            json.pop_object();
        }
        return true;

    case size_t(DataValue_item_e::eString):
        json.key(k).string(std::get<std::string>(v.m_item));
        return true;

    case size_t(DataValue_item_e::eArrayOfBoolean):
        json.key(k).push_array();
        for( bool b : ary_as<ArrayOfBoolean>(v)) {
            json.json_bool(b);
        }
        json.pop_array();
        return true;

    case size_t(DataValue_item_e::eArrayOfDouble):
        json.key(k).push_array();
        for( double d : ary_as<ArrayOfDouble>(v)) {
            json.number(d);
        }
        json.pop_array();
        return true;

    case size_t(DataValue_item_e::eArrayOfString):
        json.key(k).push_array();
        for( std::string s : ary_as<ArrayOfString>(v)) {
            json.string(s);
        }
        json.pop_array();
        return true;

    case size_t(DataValue_item_e::eArrayOfNullableBoolean):
    case size_t(DataValue_item_e::eArrayOfInt):
    case size_t(DataValue_item_e::eArrayOfNullableInt):
    case size_t(DataValue_item_e::eArrayOfLong):
    case size_t(DataValue_item_e::eArrayOfNullableLong):
    case size_t(DataValue_item_e::eArrayOfFloat):
    case size_t(DataValue_item_e::eBytes):
    case size_t(DataValue_item_e::eAnySparseArray):
        if (!omit_unsupported) {
            json.key(k)
            .push_object()
                .key("T").string(xdataTypeHint(v))
            .pop_object();
        }
        break; // serialize those in binary, to preserve original type

    // no default on purpose
    }

    return false;
}

/*!
 * \brief Serializes \a xmap to JSON.
 *
 * \param xmap the map to serialize
 * \return the JSON of (possibly of subset of) the map
 *
 * \warning Only a subset of DataValue types are serialized to JSON.
 *     Prefer xdataToAvroBinary() or xdataToJsonAndAvroBinary() to
 *     fully serialize \a xmap
 */
std::string xdataToJson(const XDataMap& xmap) {
    if (xmap.empty()) {
        return {};
    }

    std::ostringstream oss;

    // Important! Precision defaults to 6, so a number like 123456.789
    // is larger enough that it gets rounded up to 123457. Increase it.
    // See https://en.cppreference.com/w/cpp/io/manip/setprecision max precision
    oss << std::setprecision(std::numeric_limits<double>::digits10 + 1);

    oes::core::JSONWriter json(oss);
    json.push_object();
    for (const auto& entry : xmap) {
        // Warning: Some pairs are not persisted
        dataValueAsJson(json, true, entry.first, entry.second);
    }
    json.pop_object();
    std::string json_txt = oss.str();
    return json_txt;
}

XDataMap xdataFromJson(const std::string& xdata_json) {
    XDataMap xmap;
    xdataFromJson(xmap, xdata_json);
    return xmap;
}

bool xdataFromJson(XDataMap& xmap, const std::string& xdata_json) {
    if (xdata_json.empty() || xdata_json == "{}") {
        return true;
    }

    using namespace oes::core;
    JSONReader json(xdata_json.data(), xdata_json.size());
    if (json.next() != JSON_OBJECT_BEGIN) {
        return false;
    }

    // we require keys in the T then = order
    auto nextT = [](JSONReader& json, DataValue& dv, bool& add_it) {
        if ((json.next() != JSON_OBJECT_KEY) ||
            !json.is_key("T") || (json.next() != JSON_STRING)
        ) {
            return false;
        }
        std::string T = json.get_string();

        if ((json.next() != JSON_OBJECT_KEY) || !json.is_key("V")) {
            return false;
        }
        switch (json.next()) {
        case JSON_NUMBER:
            if (T == "int") {
                dv.m_item = json.get_int();
            } else if (T == "long") {
                dv.m_item = json.get_int64_t();
            } else if (T == "float") {
                dv.m_item = json.get_float();
            } else {
                return false;
            }
            add_it = true;
            break;

        case JSON_NULL:
            add_it = false; // just a hint (or unexpected non-finite/NaN/Inf FP)
            break;

        case JSON_STRING:
            add_it = true;
            if (T == "float") {
                std::string V = json.get_string_or_null();
                if (V == "NaN") {
                    dv.m_item = NAN;
                } else if (V == "Inf") {
                    dv.m_item = INFINITY;
                } else {
                    assert(false);
                    return false;
                }
                break;
            } else if (T == "double") {
                std::string V = json.get_string_or_null();
                if (V == "NaN") {
                    dv.m_item = std::numeric_limits<double>::quiet_NaN();
                } else if (V == "Inf") {
                    dv.m_item = std::numeric_limits<double>::infinity();
                } else {
                    assert(false);
                    return false;
                }
                break;
            }
            [[fallthrough]];

        default:
            assert(false);
            return false;
        }
        return json.next() == JSON_OBJECT_END;
    };

    bool inserted = false;
    while (json.next() == JSON_OBJECT_KEY) {
        std::string k = json.get_key();
        DataValue dv;

        switch (json.next()) {
        case JSON_NULL:
            inserted = xmap.emplace(std::move(k), dv).second;
            break;

        case JSON_TRUE:
        case JSON_FALSE:
            dv.m_item = json.get_boolean();
            inserted = xmap.emplace(std::move(k), std::move(dv)).second;
            assert(inserted);
            break;

        case JSON_STRING:
            dv.m_item = json.get_string();
            inserted = xmap.emplace(std::move(k), std::move(dv)).second;
            assert(inserted);
            break;

        case JSON_NUMBER:
            dv.m_item = json.get_double();
            inserted = xmap.emplace(std::move(k), std::move(dv)).second;
            assert(inserted);
            break;

        case JSON_OBJECT_BEGIN: {
            bool add_it = false;
            if (!nextT(json, dv, add_it)) {
                return false;
            }
            if (add_it) {
                inserted = xmap.emplace(std::move(k), std::move(dv)).second;
                assert(inserted);
            }
            break;
        }

        case JSON_ARRAY_BEGIN: {
            ArrayOfString vec_string;
            ArrayOfDouble vec_double;
            ArrayOfBoolean vec_bool;
            auto event = json.next();
            while(  event != JSON_ARRAY_END ){
                switch (event) {
                    case JSON_STRING:
                        vec_string.m_values.push_back(json.get_string());
                        break;
                    case JSON_NUMBER:
                        vec_double.m_values.push_back(json.get_double());
                        break;
                    case JSON_TRUE:
                    case JSON_FALSE:
                        vec_bool.m_values.push_back(json.get_boolean());
                        break;
                    default:
                        break;

                }
                event = json.next();
            }

            if( !vec_string.m_values.empty() ) {
                dv.m_item = vec_string; 
            } else if( !vec_double.m_values.empty() ) { 
                dv.m_item = vec_double; 
            } else if( !vec_bool.m_values.empty() ) { 
                dv.m_item = vec_bool; 
            }
            inserted = xmap.emplace(std::move(k), std::move(dv)).second;
            assert(inserted);
            break;
        }

        default:
            return false; // no nested objects or complex arrays for now
        }

        OES_RELEASE_UNUSED(inserted);
    }
    return
        json.current() == JSON_OBJECT_END &&
        json.next() == JSON_END
    ;
}

bool xdataFromJsonFile(XDataMap& xmap, const std::string& filename) {
    std::ifstream ifs(filename.c_str(), std::ifstream::binary | std::ifstream::in);
    if (!ifs.good()) {
        return false;
    }
    ifs.seekg(0, std::ios_base::end);
    size_t size = static_cast<size_t>(ifs.tellg());
    ifs.seekg(0, std::ios_base::beg);

    if (size == 0) { // protect against too large a file?
        return true;
    }

    std::string json_text(size, '\0');
    ifs.read(json_text.data(), size);
    if (!ifs.good()) {
        return false;
    }

    return xdataFromJson(xmap, json_text);
}

/*!
 * \brief Serializes \a xmap to Avro binary.
 *
 * \param xmap the map to serialize
 * \return the Avro binary
 *
 * \note Unlike xdataToJson(), all map entries are serialized
 */
std::vector<uint8_t> xdataToAvroBinary(const XDataMap& xmap) {
    oes::common::AvroBinaryEncoder enc;
    enc.mapStart();
    if (!xmap.empty()) {
        enc.setItemCount(xmap.size());
        for (const auto& entry : xmap) {
            enc.startItem();
            enc.encodeString(entry.first);
            entry.second.encode(enc);
        }
    }
    enc.mapEnd();

    std::vector<uint8_t> avro_bin;
    enc.moveEncodingTo(avro_bin);
    return avro_bin;
}

/*!
 * \brief Serializes \a xmap to JSON \b and Avro binary.
 *
 * The same subset that xdataToJson() serializes in part of the
 * JSON string, and the rest goes into the binary \em blob.
 *
 * \param xmap the map to serialize
 * \param add_hints Whether to add Avro-binary entries in the JSON
 *     as a \em hint, for the human-friendly visibility in the DB.
 * \return the JSON and Avro binary. Either or both can be empty.
 */
std::pair<std::string, std::vector<uint8_t>> xdataToJsonAndAvroBinary(
    const XDataMap& xmap, bool add_hints
) {
    if (xmap.empty()) {
        return {};
    }

    std::vector<const XDataMap::value_type*> avro_entries;

    std::ostringstream oss;

    // Important! Precision defaults to 6, so a number like 123456.789
    // is large enough that it gets rounded up to 123457. Increase precision.
    // See https://en.cppreference.com/w/cpp/io/manip/setprecision max precision
    oss << std::setprecision(std::numeric_limits<double>::digits10 + 1);

    oes::core::JSONWriter json(oss);
    json.push_object();
    for (const auto& entry : xmap) {
        if (!dataValueAsJson(json, true, entry.first, entry.second)) {
            avro_entries.emplace_back(&entry);

            if (add_hints) {
                json.key(entry.first)
                .push_object()
                    .key("T").string(xdataTypeHint(entry.second))
                    .key("V").json_null()
                .pop_object();
            }
        }
    }
    json.pop_object();
    std::string json_txt = oss.str();

    std::vector<uint8_t> avro_bin;
    if (!avro_entries.empty()) {
        oes::common::AvroBinaryEncoder enc;
        enc.mapStart();
        enc.setItemCount(avro_entries.size());
        for (const auto& entry : avro_entries) {
            enc.startItem();
            enc.encodeString(entry->first);
            entry->second.encode(enc);
        }
        enc.mapEnd();
        enc.moveEncodingTo(avro_bin);
    }

    return { std::move(json_txt), std::move(avro_bin) };
}

/*!
 * \brief Fills \a xmap with entries serialized (in Avro binary format) in \a xdata_avro.
 * \return How many entries could not be inserted, because already present in the map.
 */
size_t xdataFromAvroBinary(XDataMap& xmap, const std::vector<uint8_t>& xdata_avro) {
    if (xdata_avro.empty()) {
        return 0;
    }

    oes::common::MemoryInputStream2 mem(xdata_avro.data(), xdata_avro.size());

    oes::common::AvroBinaryDecoder dec;
    dec.init(mem);

    //xmap.clear();

    size_t conflicts = 0;
    if (size_t n = dec.mapStart()) {
        for (size_t i = 0; i < n; ++i) {
            std::string k;
            dec.decodeString(k);

            DataValue v;
            v.decode(dec);

            if (!xmap.emplace(std::move(k), std::move(v)).second) {
                ++conflicts;
            }
        }
    }
    dec.mapNext(); // matching enc.mapEnd()
    return conflicts;
}

bool xdataFromJsonAndAvroBinary(
    XDataMap& xmap,
    const std::string& xdata_json,
    const std::vector<uint8_t>& xdata_avro
) {
    bool ok = true;

    // JSON part
    if (!xdata_json.empty() && xdata_json != "{}") {
        ok = etp12::xdataFromJson(xmap, xdata_json);
        assert(ok);
    }

    // Avro (binary) part
    if (ok && !xdata_avro.empty()) {
        const size_t conflicts = etp12::xdataFromAvroBinary(xmap, xdata_avro);
        OES_RELEASE_UNUSED(conflicts);
        assert(conflicts == 0);

        // conflicts are silently ignored (and version from JSON "wins"),
        // not setting ok to false
    }

    return ok;
}

std::string xdataTypeHint(const DataValue& dv) {
    switch (dv.m_item.index()) {
    case size_t(DataValue_item_e::eNull):
        return "null";

    case size_t(DataValue_item_e::eBool):
        return "bool";

    case size_t(DataValue_item_e::eInt):
        return "int";

    case size_t(DataValue_item_e::eLong):
        return "long";

    case size_t(DataValue_item_e::eFloat):
        return "float";

    case size_t(DataValue_item_e::eDouble):
        return "double";

    case size_t(DataValue_item_e::eString):
        return "string";

    case size_t(DataValue_item_e::eArrayOfBoolean):
        return fmt::format("[{}]bool", std::get<ArrayOfBoolean>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfNullableBoolean):
        return fmt::format("[{}]bool?", std::get<ArrayOfNullableBoolean>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfInt):
        return fmt::format("[{}]int", std::get<ArrayOfInt>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfNullableInt):
        return fmt::format("[{}]int?", std::get<ArrayOfNullableInt>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfLong):
        return fmt::format("[{}]long", std::get<ArrayOfLong>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfNullableLong):
        return fmt::format("[{}]long?", std::get<ArrayOfNullableLong>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfFloat):
        return fmt::format("[{}]float", std::get<ArrayOfFloat>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfDouble):
        return fmt::format("[{}]double", std::get<ArrayOfDouble>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eArrayOfString):
        return fmt::format("[{}]string", std::get<ArrayOfString>(dv.m_item).m_values.size());

    case size_t(DataValue_item_e::eBytes):
        return fmt::format("[{}]byte", std::get<std::vector<uint8_t>>(dv.m_item).size());

    case size_t(DataValue_item_e::eAnySparseArray):
        return fmt::format(
            "[{}][]{}",
            std::get<AnySparseArray>(dv.m_item).m_slices.size(),
            std::get<AnySparseArray>(dv.m_item).m_slices.empty()? "void": arrayTypeHint(
                std::get<AnySparseArray>(dv.m_item).m_slices[0].m_slice
            )
        );

    // no default on purpose
    }

    assert(false);
    return "void";
}

std::string arrayTypeHint(const AnyArray& ary) {
    switch (ary.m_item.index()) {
    case size_t(AnyArray_item_e::eArrayOfBoolean):
        return fmt::format("[{}]bool", ary_as<ArrayOfBoolean>(ary).size());

    case size_t(AnyArray_item_e::eArrayOfInt):
        return fmt::format("[{}]int", ary_as<ArrayOfInt>(ary).size());

    case size_t(AnyArray_item_e::eArrayOfLong):
        return fmt::format("[{}]long", ary_as<ArrayOfLong>(ary).size());

    case size_t(AnyArray_item_e::eArrayOfFloat):
        return fmt::format("[{}]float", ary_as<ArrayOfFloat>(ary).size());

    case size_t(AnyArray_item_e::eArrayOfDouble):
        return fmt::format("[{}]double", ary_as<ArrayOfDouble>(ary).size());

    case size_t(AnyArray_item_e::eArrayOfString):
        return fmt::format("[{}]string", ary_as<ArrayOfString>(ary).size());

    case size_t(AnyArray_item_e::eBytes):
        return fmt::format("[{}]byte", std::get<bytes>(ary.m_item).size());

    // no default on purpose
    }

    assert(false);
    return "void";
}

bool xdataEmplace(XDataMap& xmap, std::string key, std::string str) {
    DataValue val;
    val.m_item = std::move(str);
    return xmap.emplace(std::move(key), std::move(val)).second;
}

// TODO: Generate such functions
bool xdataEquals(const DataValue& lhs, const DataValue& rhs) {
    auto type = lhs.m_item.index();

    if (type != rhs.m_item.index()) {
        return false;
    }

    // NaN and Inf aware comparison of floating-points
    auto fpeq = [](auto lhs, auto rhs) {
        if (lhs == rhs) {
            return true; // note that Inf == Inf, unlike NaN != NaN
        }
        int lfpc = std::fpclassify(lhs);
        int rfpc = std::fpclassify(rhs);
        return lfpc == rfpc;
    };

    switch (type) {
    case size_t(DataValue_item_e::eNull):
        return true;

    case size_t(DataValue_item_e::eBool):
        return val_as<bool>(lhs) == val_as<bool>(rhs);

    case size_t(DataValue_item_e::eInt):
        return val_as<int>(lhs) == val_as<int>(rhs);

    case size_t(DataValue_item_e::eLong):
        return val_as<int64_t>(lhs) == val_as<int64_t>(rhs);

    case size_t(DataValue_item_e::eFloat):
        return fpeq(val_as<float>(lhs), val_as<float>(rhs));

    case size_t(DataValue_item_e::eDouble):
        return fpeq(val_as<double>(lhs), val_as<double>(rhs));

    case size_t(DataValue_item_e::eString):
        return val_as<std::string>(lhs) == val_as<std::string>(rhs);

    case size_t(DataValue_item_e::eArrayOfBoolean):
        return ary_as<ArrayOfBoolean>(lhs) == ary_as<ArrayOfBoolean>(rhs);

    case size_t(DataValue_item_e::eArrayOfNullableBoolean):
        return ary_as<ArrayOfNullableBoolean>(lhs) == ary_as<ArrayOfNullableBoolean>(rhs);

    case size_t(DataValue_item_e::eArrayOfInt):
        return ary_as<ArrayOfInt>(lhs) == ary_as<ArrayOfInt>(rhs);

    case size_t(DataValue_item_e::eArrayOfNullableInt):
        return ary_as<ArrayOfNullableInt>(lhs) == ary_as<ArrayOfNullableInt>(rhs);

    case size_t(DataValue_item_e::eArrayOfLong):
        return ary_as<ArrayOfLong>(lhs) == ary_as<ArrayOfLong>(rhs);

    case size_t(DataValue_item_e::eArrayOfNullableLong):
        return ary_as<ArrayOfNullableLong>(lhs) == ary_as<ArrayOfNullableLong>(rhs);

    case size_t(DataValue_item_e::eArrayOfFloat): // TODO: NaN and Inf aware comparison
        return ary_as<ArrayOfFloat>(lhs) == ary_as<ArrayOfFloat>(rhs);

    case size_t(DataValue_item_e::eArrayOfDouble): // TODO: NaN and Inf aware comparison
        return ary_as<ArrayOfDouble>(lhs) == ary_as<ArrayOfDouble>(rhs);

    case size_t(DataValue_item_e::eArrayOfString):
        return ary_as<ArrayOfString>(lhs) == ary_as<ArrayOfString>(rhs);

    case size_t(DataValue_item_e::eBytes):
        return std::get<bytes>(lhs.m_item) == std::get<bytes>(rhs.m_item);

    case size_t(DataValue_item_e::eAnySparseArray): break; // TODO


    // no default on purpose
    }

    assert(false);
    return false;
}

bool xdataLessThan(const DataValue& lhs, const DataValue& rhs) {
    auto ltype = lhs.m_item.index();
    auto rtype = rhs.m_item.index();

    if (ltype != rtype) {
        return ltype < rtype;
    }

    switch (ltype) {
    case size_t(DataValue_item_e::eNull):
        return false;

    case size_t(DataValue_item_e::eBool):
        return val_as<bool>(lhs) < val_as<bool>(rhs);

    case size_t(DataValue_item_e::eInt):
        return val_as<int>(lhs) < val_as<int>(rhs);

    case size_t(DataValue_item_e::eLong):
        return val_as<int64_t>(lhs) < val_as<int64_t>(rhs);

    case size_t(DataValue_item_e::eFloat):
        return val_as<float>(lhs) < val_as<float>(rhs);

    case size_t(DataValue_item_e::eDouble):
        return val_as<double>(lhs) < val_as<double>(rhs);

    case size_t(DataValue_item_e::eString):
        return val_as<std::string>(lhs) < val_as<std::string>(rhs);

    case size_t(DataValue_item_e::eArrayOfBoolean):
        return ary_as<ArrayOfBoolean>(lhs) < ary_as<ArrayOfBoolean>(rhs);

    case size_t(DataValue_item_e::eArrayOfNullableBoolean):
        return ary_as<ArrayOfNullableBoolean>(lhs) < ary_as<ArrayOfNullableBoolean>(rhs);

    case size_t(DataValue_item_e::eArrayOfInt):
        return ary_as<ArrayOfInt>(lhs) < ary_as<ArrayOfInt>(rhs);

    case size_t(DataValue_item_e::eArrayOfNullableInt):
        return ary_as<ArrayOfNullableInt>(lhs) < ary_as<ArrayOfNullableInt>(rhs);

    case size_t(DataValue_item_e::eArrayOfLong):
        return ary_as<ArrayOfLong>(lhs) < ary_as<ArrayOfLong>(rhs);

    case size_t(DataValue_item_e::eArrayOfNullableLong):
        return ary_as<ArrayOfNullableLong>(lhs) < ary_as<ArrayOfNullableLong>(rhs);

    case size_t(DataValue_item_e::eArrayOfFloat):
        return ary_as<ArrayOfFloat>(lhs) < ary_as<ArrayOfFloat>(rhs);

    case size_t(DataValue_item_e::eArrayOfDouble):
        return ary_as<ArrayOfDouble>(lhs) < ary_as<ArrayOfDouble>(rhs);

    case size_t(DataValue_item_e::eArrayOfString):
        return ary_as<ArrayOfString>(lhs) < ary_as<ArrayOfString>(rhs);

    case size_t(DataValue_item_e::eBytes):
        return std::get<bytes>(lhs.m_item) < std::get<bytes>(rhs.m_item);

    case size_t(DataValue_item_e::eAnySparseArray): break; // TODO

    // no default on purpose
    }

    assert(false);
    return false;
}

} // namespace oes::eml::etp12
