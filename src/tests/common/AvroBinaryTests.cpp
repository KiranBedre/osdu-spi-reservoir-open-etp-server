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

#include <oes/common/AvroBinary.h>
#include <oes/common/AvroUtils.hpp>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Specific.hh>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <limits>

using namespace oes::common;

namespace {

//============================================================================

const int ints[] {
    std::numeric_limits<int>::min(),
    -782349,
    -1,
    0,
    1,
    782349,
    std::numeric_limits<int>::max(),
};

const int64_t longs[] {
    std::numeric_limits<int64_t>::min(),
    std::numeric_limits<int>::min(),
    -782349,
    -1,
    0,
    1,
    782349,
    std::numeric_limits<int>::max(),
    std::numeric_limits<int64_t>::max(),
};

const float floats[] {
    std::numeric_limits<float>::min(),
    -782349.1237f,
    -1.1f,
    0.0f,
    1.2f,
    782349.1237f,
    std::numeric_limits<float>::lowest(),
    std::numeric_limits<float>::epsilon(),
    std::numeric_limits<float>::round_error(),
    std::numeric_limits<float>::max(),
};

const double doubles[] {
    std::numeric_limits<double>::min(),
    -782349.1237,
    -1.1,
    0,
    1.2,
    782349.1237,
    std::numeric_limits<double>::lowest(),
    std::numeric_limits<double>::epsilon(),
    std::numeric_limits<double>::round_error(),
    std::numeric_limits<double>::max(),
};

const std::string strings[] {
    "",
    "a",
    "Z",
    "foo bar",
    "foo'bar \"baz\"",
    std::string(4936, '?'),
};

const size_t enums[] {
    0,
    1,
    2,
};

const size_t union_indices[] {
    0,
    1,
    2,
    7,
};

const uint8_t bytes[] { 0x00, 0x01, 0x99, 0xFF, 0x03, 0xFE, 0x5B };

//============================================================================

BOOST_AUTO_TEST_SUITE(AvroBinaryTests)

BOOST_AUTO_TEST_CASE(test_encode_decode_primitives) {
    const size_t chunk_size = 4*1024; // What Avro uses
#ifdef OES_DEBUG_BUILD
    const size_t max_size =  25*1024*1024; //  25 MiB faster for debug
#else
    const size_t max_size = 100*1024*1024; // 100 MiB large enough for benches
#endif

    auto official_enc = ::avro::binaryEncoder();
    AvroBinaryEncoder custom_enc;
    ::avro::Encoder* encoders[] {
        official_enc.get(),
        &custom_enc,
    };
    std::vector<std::string> encoded;

    for (auto p_enc : encoders) {
        ::avro::Encoder& enc = *p_enc;

        std::string str;
        StringAsAvroOStreamView aos(str, chunk_size, max_size);

        enc.init(aos);

        enc.encodeNull();

        enc.encodeBool(true);
        enc.encodeBool(false);

        for (int i : ints) {
            enc.encodeInt(i);
        }
        for (int64_t l : longs) {
            enc.encodeLong(l);
        }
        for (float f : floats) {
            enc.encodeFloat(f);
        }
        for (double d : doubles) {
            enc.encodeDouble(d);
        }
        for (const std::string& s : strings) {
            enc.encodeString(s);
        }
        for (size_t e : enums) {
            enc.encodeEnum(e);
        }
        for (size_t e : union_indices) {
            enc.encodeUnionIndex(e);
        }

        enc.encodeBytes(bytes, sizeof(bytes));

        enc.encodeFixed(bytes + 1, sizeof(bytes) - 2);

        enc.arrayStart();
        enc.arrayEnd();

        enc.mapStart();
        enc.mapEnd();

        // Cannot be zero
        BOOST_CHECK_THROW(enc.setItemCount(0), std::runtime_error);

        enc.setItemCount(1);
        enc.startItem();

        enc.setItemCount(2);
        enc.startItem();
        enc.startItem();

        enc.flush(); // calls ::avro::OutputStream::backup(), for minimum message

        encoded.emplace_back(std::move(str));
    }

    BOOST_REQUIRE_EQUAL(encoded.size(), 2);
    BOOST_CHECK_EQUAL(encoded[0].size(), encoded[1].size());
    BOOST_CHECK_EQUAL(encoded[0].size(), 5'169);
    BOOST_CHECK_EQUAL(encoded[0], encoded[1]);

    // Now decoding
    auto official_dec = ::avro::binaryDecoder();
    AvroBinaryDecoder custom_dec;
    ::avro::Decoder& odec = *official_dec;
    ::avro::Decoder& cdec = custom_dec;

    StringAsAvroIStreamView ois(encoded[0]);
    StringAsAvroIStreamView cis(encoded[1]);

    odec.init(ois);
    cdec.init(cis);

    // null
    odec.decodeNull();
    cdec.decodeNull();

    // bool
    bool obool, cbool;
    obool = odec.decodeBool();
    cbool = cdec.decodeBool();
    BOOST_CHECK_EQUAL(obool, cbool);
    BOOST_CHECK_EQUAL(obool, true);

    obool = odec.decodeBool();
    cbool = cdec.decodeBool();
    BOOST_CHECK_EQUAL(obool, cbool);
    BOOST_CHECK_EQUAL(obool, false);

    // int
    for (auto val : ints) {
        auto oval = odec.decodeInt();
        auto cval = cdec.decodeInt();
        BOOST_CHECK_EQUAL(oval, cval);
        BOOST_CHECK_EQUAL(oval, val);
    }

    // long
    for (auto val : longs) {
        auto oval = odec.decodeLong();
        auto cval = cdec.decodeLong();
        BOOST_CHECK_EQUAL(oval, cval);
        BOOST_CHECK_EQUAL(oval, val);
    }

    // float
    for (auto val : floats) {
        auto oval = odec.decodeFloat();
        auto cval = cdec.decodeFloat();
        BOOST_CHECK_EQUAL(oval, cval);
        BOOST_CHECK_EQUAL(oval, val);
    }

    // double
    for (auto val : doubles) {
        auto oval = odec.decodeDouble();
        auto cval = cdec.decodeDouble();
        BOOST_CHECK_EQUAL(oval, cval);
        BOOST_CHECK_EQUAL(oval, val);
    }

    // string
    for (const auto& val : strings) {
        auto oval = odec.decodeString();
        auto cval = cdec.decodeString();
        BOOST_CHECK_EQUAL(oval, cval);
        BOOST_CHECK_EQUAL(oval, val);
    }

    // enum
    for (auto val : enums) {
        auto oval = odec.decodeEnum();
        auto cval = cdec.decodeEnum();
        BOOST_CHECK_EQUAL(oval, cval);
        BOOST_CHECK_EQUAL(oval, val);
    }

    // union index
    for (auto val : union_indices) {
        auto oval = odec.decodeUnionIndex();
        auto cval = cdec.decodeUnionIndex();
        BOOST_CHECK_EQUAL(oval, cval);
        BOOST_CHECK_EQUAL(oval, val);
    }
}

BOOST_AUTO_TEST_CASE(test_encode_decode_array) {
    const size_t chunk_size = 4*1024; // What Avro uses
#ifdef OES_DEBUG_BUILD
    const size_t max_size =  25*1024*1024; //  25 MiB faster for debug
#else
    const size_t max_size = 100*1024*1024; // 100 MiB large enough for benches
#endif

    auto official_enc = ::avro::binaryEncoder();
    AvroBinaryEncoder custom_enc;
    ::avro::Encoder* encoders[] {
        official_enc.get(),
        &custom_enc,
    };
    std::vector<std::string> encoded;

    const std::vector<bool> bool_vec{ true, false, true };
    const std::vector<int> int_vec(ints, ints + std::size(ints));
    const std::vector<int64_t> long_vec(longs, longs + std::size(longs));
    const std::vector<float> float_vec(floats, floats + std::size(floats));
    const std::vector<double> double_vec(doubles, doubles + std::size(doubles));
    const std::vector<std::string> string_vec(strings, strings + std::size(strings));

    for (auto p_enc : encoders) {
        ::avro::Encoder& enc = *p_enc;

        std::string str;
        StringAsAvroOStreamView aos(str, chunk_size, max_size);

        enc.init(aos);

        ::avro::encode(enc, bool_vec);
        ::avro::encode(enc, int_vec);
        ::avro::encode(enc, long_vec);
        ::avro::encode(enc, float_vec);
        ::avro::encode(enc, double_vec);
        ::avro::encode(enc, string_vec);

        enc.flush(); // calls ::avro::OutputStream::backup(), for minimum message

        encoded.emplace_back(std::move(str));
    }

    BOOST_REQUIRE_EQUAL(encoded.size(), 2);
    BOOST_CHECK_EQUAL(encoded[0].size(), encoded[1].size());
    BOOST_CHECK_EQUAL(encoded[0].size(), 5'158);
    BOOST_CHECK_EQUAL(encoded[0], encoded[1]);

    // Now decoding
    auto official_dec = ::avro::binaryDecoder();
    AvroBinaryDecoder custom_dec;
    ::avro::Decoder& odec = *official_dec;
    ::avro::Decoder& cdec = custom_dec;

    StringAsAvroIStreamView ois(encoded[0]);
    StringAsAvroIStreamView cis(encoded[1]);

    odec.init(ois);
    cdec.init(cis);

    auto check_decode = [&odec, &cdec](auto vec) -> int {
        decltype(vec) ovec, cvec;
        ::avro::decode(odec, ovec);
        ::avro::decode(cdec, cvec);
        if (ovec != cvec) {
            return 1;
        }
        if (ovec != vec) {
            return 2;
        }
        return 0;
    };

    BOOST_CHECK_EQUAL(0, check_decode(bool_vec));
    BOOST_CHECK_EQUAL(0, check_decode(int_vec));
    BOOST_CHECK_EQUAL(0, check_decode(long_vec));
    BOOST_CHECK_EQUAL(0, check_decode(float_vec));
    BOOST_CHECK_EQUAL(0, check_decode(double_vec));
    BOOST_CHECK_EQUAL(0, check_decode(string_vec));
}

BOOST_AUTO_TEST_CASE(test_encode_decode_map) {
    const size_t chunk_size = 4*1024; // What Avro uses
#ifdef OES_DEBUG_BUILD
    const size_t max_size =  25*1024*1024; //  25 MiB faster for debug
#else
    const size_t max_size = 100*1024*1024; // 100 MiB large enough for benches
#endif

    auto official_enc = ::avro::binaryEncoder();
    AvroBinaryEncoder custom_enc;
    ::avro::Encoder* encoders[] {
        official_enc.get(),
        &custom_enc,
    };
    std::vector<std::string> encoded;

    std::map<std::string, bool> bool_map{ { "true", true }, { "false", false } };
    std::map<std::string, int> int_map;
    std::map<std::string, int64_t> long_map;
    std::map<std::string, float> float_map;
    std::map<std::string, double> double_map;
    std::map<std::string, std::string> string_map;
    for (auto val : ints) {
        int_map.emplace(std::to_string(val), val);
    }
    for (auto val : longs) {
        long_map.emplace(std::to_string(val), val);
    }
    for (auto val : floats) {
        float_map.emplace(std::to_string(val), val);
    }
    for (auto val : doubles) {
        double_map.emplace(std::to_string(val), val);
    }
    for (auto val : strings) {
        string_map.emplace(val, val);
    }

    for (auto p_enc : encoders) {
        ::avro::Encoder& enc = *p_enc;

        std::string str;
        StringAsAvroOStreamView aos(str, chunk_size, max_size);

        enc.init(aos);

        ::avro::encode(enc, bool_map);
        ::avro::encode(enc, int_map);
        ::avro::encode(enc, long_map);
        ::avro::encode(enc, float_map);
        ::avro::encode(enc, double_map);
        ::avro::encode(enc, string_map);

        enc.flush(); // calls ::avro::OutputStream::backup(), for minimum message

        encoded.emplace_back(std::move(str));
    }

    BOOST_REQUIRE_EQUAL(encoded.size(), 2);
    BOOST_CHECK_EQUAL(encoded[0].size(), encoded[1].size());
    BOOST_CHECK_EQUAL(encoded[0].size(), 11'104);
    BOOST_CHECK_EQUAL(encoded[0], encoded[1]);

    // Now decoding
    auto official_dec = ::avro::binaryDecoder();
    AvroBinaryDecoder custom_dec;
    ::avro::Decoder& odec = *official_dec;
    ::avro::Decoder& cdec = custom_dec;

    StringAsAvroIStreamView ois(encoded[0]);
    StringAsAvroIStreamView cis(encoded[1]);

    odec.init(ois);
    cdec.init(cis);

    auto check_decode = [&odec, &cdec](auto map) -> int {
        decltype(map) omap, cmap;
        ::avro::decode(odec, omap);
        ::avro::decode(cdec, cmap);
        if (omap != cmap) {
            return 1;
        }
        if (omap != map) {
            return 2;
        }
        return 0;
    };

    BOOST_CHECK_EQUAL(0, check_decode(bool_map));
    BOOST_CHECK_EQUAL(0, check_decode(int_map));
    BOOST_CHECK_EQUAL(0, check_decode(long_map));
    BOOST_CHECK_EQUAL(0, check_decode(float_map));
    BOOST_CHECK_EQUAL(0, check_decode(double_map));
    BOOST_CHECK_EQUAL(0, check_decode(string_map));
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
