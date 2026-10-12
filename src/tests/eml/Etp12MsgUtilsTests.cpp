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

#include <oes/eml/etp12/MsgUtils.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cmath>
#include <optional>
#include <limits>

namespace {

using namespace oes::eml::etp12;
using oes::eml::etp12::Datatypes::DataValue;

BOOST_AUTO_TEST_SUITE(Etp12MsgUtilsTests)

BOOST_AUTO_TEST_CASE(test_XDataMap_empty) {
    XDataMap xmap;
    std::string json = xdataToJson(xmap);
    BOOST_CHECK_EQUAL("", json);

    XDataMap out;
    bool rc = xdataFromJson(out, json);
    BOOST_CHECK(rc);
    BOOST_CHECK(out.empty());
    BOOST_CHECK(xmap == out);

    rc = xdataFromJson(out, "{}");
    BOOST_CHECK(rc);
    BOOST_CHECK(out.empty());
    BOOST_CHECK(xmap == out);
}

BOOST_AUTO_TEST_CASE(test_XDataMap_primitives_json) {
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};

    xmap.emplace("nil", DataValue());

    xmb.emplace("b1", true);
    xmb.emplace("b2", false);

    // Note: int, long, or float, are still serialized to JSON
    // but wrapped in an object that explicitly state their types.

    xmb.emplace("i1", -1);
    xmb.emplace("i2", 0);
    xmb.emplace("i3", +1);
    xmb.emplace("i4", (1<<31));

    xmb.emplace("l1", int64_t(-1));
    xmb.emplace("l2", int64_t(0));
    xmb.emplace("l3", int64_t(+1));
    xmb.emplace("l4", int64_t(1)<<63);

    xmb.emplace("f1", -1.0f);
    xmb.emplace("f2", 0.0f);
    xmb.emplace("f3", +1.0f);
    xmb.emplace("f4", 123456.789f);
    xmb.emplace("fInf", INFINITY);
    xmb.emplace("fNaN", NAN);

    xmb.emplace("d1", -1.0);
    xmb.emplace("d2", 0.0);
    xmb.emplace("d3", +1.0);
    xmb.emplace("d4", 123456.789);
    xmb.emplace("dInf", std::numeric_limits<double>::infinity());
    xmb.emplace("dNaN", std::numeric_limits<double>::quiet_NaN());

    xmb.emplace("s1", "");
    xmb.emplace("s2", "a");
    xmb.emplace("s3", "AB");
    xmb.emplace("s4", "The quick brown fox jumps over the lazy dog");

    std::string json = xdataToJson(xmap);

    XDataMap out;
    bool rc = xdataFromJson(out, json);
    BOOST_CHECK(rc);
    BOOST_CHECK(xmap == out);
}

BOOST_AUTO_TEST_CASE(test_XDataMap_primitives_avro) {
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};

    xmap.emplace("nil", DataValue());

    xmb.emplace("b1", true);
    xmb.emplace("b2", false);

    xmb.emplace("i1", -1);
    xmb.emplace("i2", 0);
    xmb.emplace("i3", +1);
    xmb.emplace("i4", (1<<31));

    xmb.emplace("l1", int64_t(-1));
    xmb.emplace("l2", int64_t(0));
    xmb.emplace("l3", int64_t(+1));
    xmb.emplace("l4", int64_t(1)<<63);

    xmb.emplace("f1", -1.0f);
    xmb.emplace("f2", 0.0f);
    xmb.emplace("f3", +1.0f);
    xmb.emplace("f4", 123456.789f);

    xmb.emplace("d1", -1.0);
    xmb.emplace("d2", 0.0);
    xmb.emplace("d3", +1.0);
    xmb.emplace("d4", 123456.789);

    xmb.emplace("s1", "");
    xmb.emplace("s2", "a");
    xmb.emplace("s3", "AB");
    xmb.emplace("s4", "The quick brown fox jumps over the lazy dog");

    std::vector<uint8_t> avro = xdataToAvroBinary(xmap);

    XDataMap out;
    xdataFromAvroBinary(out, avro);
    BOOST_CHECK(xmap == out);
}

template <typename T>
using vec = std::vector<T>;

// TODO FIXME Arrays are not fully supported for now.
// Partial implementation of Arrays in xdataFromJson
// only boolean, double and string arrays are supported for now
BOOST_AUTO_TEST_CASE(test_XDataMap_vectors_json) {
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};

    xmb.emplace("B", vec<uint8_t>{ 0x00, 0xFF, 0x12, 0x90, 0xCA, 0xFE, 0xBA, 0xBE });
    xmb.emplace("b", vec<bool>{ true, false });
    xmb.emplace("i", vec<int>{ -1, 0, +1, (1<<31) });
    xmb.emplace("l", vec<int64_t>{ -1, 0, +1, (int64_t(1)<<63) });
    xmb.emplace("f", vec<float>{ -1.0f, 0.0f, +1.0f, 123456.789f });
    xmb.emplace("d", vec<double>{ -1.0, 0.0, +1.0, 123456.789 });
    xmb.emplace("s", vec<std::string>{
        "", "a", "AB", "The quick brown fox jumps over the lazy dog" }
    );

    std::string json = xdataToJson(xmap);

    XDataMap out;
    bool rc = xdataFromJson(out, json);
    BOOST_CHECK(rc);
    BOOST_CHECK(out.count("B") == 0); // Binary arrays not yet implemented
    BOOST_CHECK(out.count("b") == 1);
    BOOST_CHECK(out.count("i") == 0); // 32bits Integer arrays not yet implemented
    BOOST_CHECK(out.count("l") == 0); // 64bits Integer arrays not yet implemented
    BOOST_CHECK(out.count("f") == 0); // 32bits Float arrays not yet implemented
    BOOST_CHECK(out.count("d") == 1);
    BOOST_CHECK(out.count("s") == 1);
    // BOOST_CHECK(xmap == out); // this should not fail if fully implemented
}

BOOST_AUTO_TEST_CASE(test_XDataMap_vectors_avro) {
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};

    xmb.emplace("B", vec<uint8_t>{ 0x00, 0xFF, 0x12, 0x90, 0xCA, 0xFE, 0xBA, 0xBE });
    xmb.emplace("b", vec<bool>{ true, false });
    xmb.emplace("i", vec<int>{ -1, 0, +1, (1<<31) });
    xmb.emplace("l", vec<int64_t>{ -1, 0, +1, (int64_t(1)<<63) });
    xmb.emplace("f", vec<float>{ -1.0f, 0.0f, +1.0f, 123456.789f });
    xmb.emplace("d", vec<double>{ -1.0, 0.0, +1.0, 123456.789 });
    xmb.emplace("s", vec<std::string>{
        "", "a", "AB", "The quick brown fox jumps over the lazy dog" }
    );

    std::vector<uint8_t> avro = xdataToAvroBinary(xmap);

    XDataMap out;
    xdataFromAvroBinary(out, avro);
    BOOST_CHECK(xmap == out);
}

template <typename T>
using nvec = std::vector<std::optional<T>>;

constexpr auto& nil = std::nullopt;

BOOST_AUTO_TEST_CASE(test_XDataMap_nullable_vectors_avro) {
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};

    xmb.emplace("[]b?", nvec<bool>{ {true}, nil, {false} });
    xmb.emplace("[]i?", nvec<int>{ nil, {-1}, {0}, {+1}, (1<<31), nil });
    xmb.emplace("[]l?", nvec<int64_t>{ {-1}, {0}, {+1}, nil, nil, nil, {(int64_t(1)<<63)} });

    std::vector<uint8_t> avro = xdataToAvroBinary(xmap);

    XDataMap out;
    xdataFromAvroBinary(out, avro);
    BOOST_CHECK(xmap == out);
}

BOOST_AUTO_TEST_CASE(test_XDataMap_hybrid) {
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};

    xmap.emplace("nil", DataValue());

    xmb.emplace("b1", true);
    xmb.emplace("b2", false);

    xmb.emplace("i1", -1);
    xmb.emplace("i2", 0);
    xmb.emplace("i3", +1);
    xmb.emplace("i4", (1<<31));

    xmb.emplace("l1", int64_t(-1));
    xmb.emplace("l2", int64_t(0));
    xmb.emplace("l3", int64_t(+1));
    xmb.emplace("l4", int64_t(1)<<63);

    xmb.emplace("f1", -1.0f);
    xmb.emplace("f2", 0.0f);
    xmb.emplace("f3", +1.0f);
    xmb.emplace("f4", 123456.789f);

    xmb.emplace("d1", -1.0);
    xmb.emplace("d2", 0.0);
    xmb.emplace("d3", +1.0);
    xmb.emplace("d4", 123456.789);

    xmb.emplace("s1", "");
    xmb.emplace("s2", "a");
    xmb.emplace("s3", "AB");
    xmb.emplace("s4", "The quick brown fox jumps over the lazy dog");

    xmb.emplace("B", vec<uint8_t>{ 0x00, 0xFF, 0x12, 0x90, 0xCA, 0xFE, 0xBA, 0xBE });
    xmb.emplace("b", vec<bool>{ true, false });
    xmb.emplace("i", vec<int>{ -1, 0, +1, (1<<31) });
    xmb.emplace("l", vec<int64_t>{ -1ll, 0ll, +1ll, (1ll<<63) });
    xmb.emplace("f", vec<float>{ -1.0f, 0.0f, +1.0f, 123456.789f });
    xmb.emplace("d", vec<double>{ -1.0, 0.0, +1.0, 123456.789 });
    xmb.emplace("s", vec<std::string>{
        "", "a", "AB", "The quick brown fox jumps over the lazy dog" }
    );

    xmb.emplace("[]b?", nvec<bool>{ {true}, nil, {false} });
    xmb.emplace("[]i?", nvec<int>{ nil, {-1}, {0}, {+1}, (1<<31), nil });
    xmb.emplace("[]l?", nvec<int64_t>{ {-1}, {0}, {+1}, nil, nil, nil, {(int64_t(1)<<63)} });

    // TODO: AnySparseArray

    std::string json;
    std::vector<uint8_t> avro;
    std::tie(json, avro) = xdataToJsonAndAvroBinary(xmap);

    XDataMap out;
    xdataFromJson(out, json);

    const size_t conflicts = xdataFromAvroBinary(out, avro);
    BOOST_CHECK_EQUAL(0, conflicts);

    BOOST_CHECK(xmap == out);
}

BOOST_AUTO_TEST_CASE(test_xdataTypeHint) {
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};

    xmap.emplace("nil", DataValue());

    xmb.emplace("b1", true);
    xmb.emplace("b2", false);

    xmb.emplace("i1", -1);
    xmb.emplace("i2", 0);
    xmb.emplace("i3", +1);
    xmb.emplace("i4", (1<<31));

    xmb.emplace("l1", int64_t(-1));
    xmb.emplace("l2", int64_t(0));
    xmb.emplace("l3", int64_t(+1));
    xmb.emplace("l4", int64_t(1)<<63);

    xmb.emplace("f1", -1.0f);
    xmb.emplace("f2", 0.0f);
    xmb.emplace("f3", +1.0f);
    xmb.emplace("f4", 123456.789f);

    xmb.emplace("d1", -1.0);
    xmb.emplace("d2", 0.0);
    xmb.emplace("d3", +1.0);
    xmb.emplace("d4", 123456.789);

    xmb.emplace("s1", "");
    xmb.emplace("s2", "a");
    xmb.emplace("s3", "AB");
    xmb.emplace("s4", "The quick brown fox jumps over the lazy dog");

    xmb.emplace("B", vec<uint8_t>{ 0x00, 0xFF, 0x12, 0x90, 0xCA, 0xFE, 0xBA, 0xBE });
    xmb.emplace("b", vec<bool>{ true, false });
    xmb.emplace("i", vec<int>{ -1, 0, +1, (1<<31) });
    xmb.emplace("l", vec<int64_t>{ -1ll, 0ll, +1ll, (1ll<<63) });
    xmb.emplace("f", vec<float>{ -1.0f, 0.0f, +1.0f, 123456.789f });
    xmb.emplace("d", vec<double>{ -1.0, 0.0, +1.0, 123456.789 });
    xmb.emplace("s", vec<std::string>{
        "", "a", "AB", "The quick brown fox jumps over the lazy dog" }
    );

    xmb.emplace("[]b?", nvec<bool>{ {true}, nil, {false} });
    xmb.emplace("[]i?", nvec<int>{ nil, {-1}, {0}, {+1}, (1<<31), nil });
    xmb.emplace("[]l?", nvec<int64_t>{ {-1}, {0}, {+1}, nil, nil, nil, {(int64_t(1)<<63)} });

    // TODO: AnySparseArray

    BOOST_CHECK_EQUAL("null", xdataTypeHint(xmap["nil"]));
    BOOST_CHECK_EQUAL("bool", xdataTypeHint(xmap["b1"]));
    BOOST_CHECK_EQUAL("int", xdataTypeHint(xmap["i2"]));
    BOOST_CHECK_EQUAL("long", xdataTypeHint(xmap["l3"]));
    BOOST_CHECK_EQUAL("float", xdataTypeHint(xmap["f4"]));
    BOOST_CHECK_EQUAL("double", xdataTypeHint(xmap["d1"]));
    BOOST_CHECK_EQUAL("[8]byte", xdataTypeHint(xmap["B"]));
    BOOST_CHECK_EQUAL("[2]bool", xdataTypeHint(xmap["b"]));
    BOOST_CHECK_EQUAL("[4]int", xdataTypeHint(xmap["i"]));
    BOOST_CHECK_EQUAL("[4]long", xdataTypeHint(xmap["l"]));
    BOOST_CHECK_EQUAL("[4]float", xdataTypeHint(xmap["f"]));
    BOOST_CHECK_EQUAL("[4]double", xdataTypeHint(xmap["d"]));
    BOOST_CHECK_EQUAL("[4]string", xdataTypeHint(xmap["s"]));

    BOOST_CHECK_EQUAL("[3]bool?", xdataTypeHint(xmap["[]b?"]));
    BOOST_CHECK_EQUAL("[6]int?", xdataTypeHint(xmap["[]i?"]));
    BOOST_CHECK_EQUAL("[7]long?", xdataTypeHint(xmap["[]l?"]));
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
