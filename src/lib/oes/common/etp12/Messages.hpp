// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_COMMON_ETP12_MESSAGES_HPP
#define OES_COMMON_ETP12_MESSAGES_HPP

#include "Messages.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Specific.hh>
#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <boost/mp11/algorithm.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <sstream>

// Additional Avro specializations for C++17 and later
namespace avro {

template <typename T1, typename T2> struct codec_traits<std::pair<T1, T2>> {
    static void encode(Encoder& e, const std::pair<T1, T2>& p) {
        avro::encode(e, p.first);
        avro::encode(e, p.second);
    }

    static void decode(Decoder& d, std::pair<T1, T2>& p) {
        avro::decode(d, p.first);
        avro::decode(d, p.second);
    }
};

template <typename T> struct codec_traits<std::optional<T>> {
    static void encode(Encoder& e, const std::optional<T>& opt) {
        if (opt) {
            e.encodeUnionIndex(1);
            avro::encode(e, *opt);
        } else {
            e.encodeUnionIndex(0);
            e.encodeNull();
        }
    }

    static void decode(Decoder& d, std::optional<T>& opt) {
        switch (d.decodeUnionIndex()) {
        case 0:
            d.decodeNull();
            return opt.reset();
        case 1:
            return avro::decode(d, opt.emplace());
        }
        throw avro::Exception("Invalid Union index");
    }
};

template <> struct codec_traits<std::monostate> {
    static void encode(Encoder& e, const std::monostate&) {
        e.encodeNull();
    }

    static void decode(Decoder& d, std::monostate&) {
        d.decodeNull();
    }
};

template <typename... Ts> struct codec_traits<std::variant<Ts...>> {
    static void encode(Encoder& e, const std::variant<Ts...>& var) {
        e.encodeUnionIndex(var.index());
        std::visit(
            [&e](const auto& alternative) {
                avro::encode(e, alternative);
            },
            var
        );
    }

    static void decode(Decoder& d, std::variant<Ts...>& var) {
        const size_t index = d.decodeUnionIndex();
        if (index >= sizeof...(Ts)) {
            throw avro::Exception("Invalid Union index");
        }
        boost::mp11::mp_with_index<sizeof...(Ts)>(
            index, [&](auto Idx) {
                constexpr size_t alternative_index = Idx;
                avro::decode(d, var.template emplace<alternative_index>());
            }
        );
    }
};

} // namespace avro

#include "codegen/Messages.i.hpp"

#endif
