// ============================================================================
// Copyright 2013-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_UTILS_JSON_HPP
#define OES_CORE_UTILS_JSON_HPP

#include <oes/core/utils/Json.h>

#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/array.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes {
namespace core {

/****************************************************************************/

template <typename V>
std::string to_json_text(const V& value) {
    std::ostringstream oss;
    JSONWriter json(oss);
    // json_write() overload must be defined in the oes::core namespace,
    // or defined in the same namespace as T (to be found via ADL).
    json_write(json, value);
    return oss.str();
}

template <typename V>
void json_write_value_helper(JSONWriter& json, const V& value) {
    // json_write() overload must be defined in the oes::core namespace,
    // or defined in the same namespace as T (to be found via ADL).
    json_write(json, value);
}

/****************************************************************************/

template <typename V>
JSONWriter& JSONWriter::value(const V& value) {
    // name lookup somehow fails if I call json_write(*this, value); directly.
    oes::core::json_write_value_helper<V>(*this, value);
    return *this;
}

template <typename C>
JSONWriter& JSONWriter::size(const C& container) {
    return number(container.size());
}

template <typename C>
JSONWriter& JSONWriter::values(const C& container) {
    push_array();
    for (const auto& elem : container) {
        json_write(*this, elem);
    }
    pop_array();
    return *this;
}

/****************************************************************************/

template <typename V>
void json_write(JSONWriter& json, const std::map<std::string, V>& key_values) {
    json.push_object();
    for (const auto& entry : key_values) {
        json.key(entry.first);
        json_write(json, entry.second);
    }
    json.pop_object();
}

template <typename V>
void json_write(JSONWriter& json, const std::vector<V>& values) {
    json.values(values);
}

template <typename V>
inline void json_write(JSONWriter& json, const std::set<V>& values) {
    json.values(values);
}

template <typename V, std::size_t N>
void json_write(JSONWriter& json, const boost::array<V, N>& values) {
    json.values(values);
}

/*****************************************************************************/

template <class T>
void json_write_kv_unless_empty(
    JSONWriter& json, const std::string& key, const T& value
) {
    if (!json.options().omit_defaults_ || !value.empty()) {
        json.key(key).value(value);
    }
}

template <class T>
void json_write_kv_unless_equal(
    JSONWriter& json, const std::string& key, T value, T ndv = T()
) {
    if (!json.options().omit_defaults_ || value != ndv) {
        json.key(key).value(value);
    }
}

/****************************************************************************/

template <typename F>
bool JSONReader::parse_array(F lambda) {
    if (next() != JSON_ARRAY_BEGIN) {
        return false;
    }
    const size_t my_depth = depth();
    while (peek() != JSON_ARRAY_END) {
        if (!lambda()) {
            // whether array-end or real failure
            // determined by last return statement.
            break;
        }
        if (depth() != my_depth) {
            return false; // buggy lambda
        }
    }
    return next() == JSON_ARRAY_END && (depth() + 1) == my_depth;
}

template <typename F>
bool JSONReader::try_parse_array(F lambda) {
    try {
        return parse_array(lambda);
    } catch (const std::runtime_error& /*ex*/) {
        // TODO: log error
        return false;
    }
}

template <typename C>
bool json_parse_array(JSONReader& json, C& container) {
    using value_t = typename C::value_type;
    return json.try_parse_array([&]() {
        value_t val;
        if (!value_t::parse(json, val)) {
            return false;
        }
        container.emplace_back(std::move(val));
        return true;
    });
}

template <typename V>
bool JSONReader::try_parse(V& value) {
    try {
        return json_parse(*this, value);
    } catch (const std::runtime_error& /*ex*/) {
        // TODO: log error
        return false;
    }
}

template <typename V>
bool JSONReader::try_parse_member(V& value) {
    try {
        return V::parse(*this, value);
    } catch (const std::runtime_error& /*ex*/) {
        // TODO: log error
        return false;
    }
}

/****************************************************************************/

template <typename F>
bool json_parse_object(
    JSONReader& json, size_t min_keys, size_t max_keys, F&& lambda
) {
    if (json.next() != JSON_OBJECT_BEGIN) {
        return false;
    }
    if (json.peek() == JSON_OBJECT_END) {
        json.next();
        return min_keys == 0; // true when empty object OK; false otherwise.
    }

    const size_t inner_depth = json.depth();

    size_t i = 0;
    for (;  (json.next() == JSON_OBJECT_KEY) &&
            (json.depth() == inner_depth) &&
            (i < max_keys); ++i
    ) {
        if (!lambda()) {
            return false;
        }
        // catch buggy lambda. Not foul proof, could be consuming
        // several key-value pairs for example. Better than nothing.
        // Foolproof would require a json.savepoint() around reading
        // the value, which is not free, and also implies passing the
        // key to the lamdba explicitly.
        if (json.depth() != inner_depth) {
            return false;
        }
    }

    return
        (min_keys <= i && i <= max_keys) &&
        (json.current() == JSON_OBJECT_END) &&
        (json.depth() == (inner_depth - 1))
    ;
}

/****************************************************************************/

} // namespace core
} // namespace oes

#endif
