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

#include <oes/core/utils/Json.h>
#include <oes/core/utils/Json.hpp>
#include <oes/core/utils/System.h>
#include <oes/core/utils/Assert.h>
#include <oes/testing/core/AssertTestTools.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/enable_shared_from_this.hpp>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <fstream>
#include <sstream>
#include <iostream>

using namespace std;
using namespace oes::core;
using namespace oes::testing;

namespace {

struct MyStringPair {
    const char* input;
    const char* output;
};

// struct declared in anon-NS
struct MyFoo { int v; MyFoo() : v(42) {} };
void json_write(JSONWriter& json, const MyFoo& foo) {
    json.number(foo.v);
}

namespace acme {
    // struct declared in acme-NS
    struct MyWidget { bool ok; explicit MyWidget(bool ok) : ok(ok) {} };
    void json_write(JSONWriter& json, const MyWidget& w) {
        json.json_bool(w.ok);
    }
} // acme

BOOST_AUTO_TEST_SUITE(JSONWriterTests)

BOOST_AUTO_TEST_CASE(test_json_writer) {
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.json_null();

    BOOST_CHECK_EQUAL(std::string("null"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.json_true();

    BOOST_CHECK_EQUAL(std::string("true"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.json_false();

    BOOST_CHECK_EQUAL(std::string("false"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.number(1);

    BOOST_CHECK_EQUAL(std::string("1"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array();
        json.value(1).value(2);
    json.pop_array();

    BOOST_CHECK_EQUAL(std::string("[1,2]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array();
        json.value(1).value(2).value(-1).value(2.3);
    json.pop_array();

    BOOST_CHECK_EQUAL(std::string("[1,2,-1,2.3]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.number(1.1);

    BOOST_CHECK_EQUAL(std::string("1.1"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array().pop_array();

    BOOST_CHECK_EQUAL(std::string("[]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object().pop_object();

    BOOST_CHECK_EQUAL(std::string("{}"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array().string("foo").pop_array();

    BOOST_CHECK_EQUAL(std::string("[\"foo\"]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array();
        json.value("foo").value("bar").value("baz");
    json.pop_array();

    BOOST_CHECK_EQUAL(std::string("[\"foo\",\"bar\",\"baz\"]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array();
        json.value("foo").value("bar");
        json.push_object();
            json.key("one").value(1);
        json.pop_object();
    json.pop_array();

    BOOST_CHECK_EQUAL(std::string("[\"foo\",\"bar\",{\"one\":1}]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array();
        json.push_object();
            json.key("one").value(1);
        json.pop_object();
    json.pop_array();

    BOOST_CHECK_EQUAL(std::string("[{\"one\":1}]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array();
        json.push_object().pop_object();
    json.pop_array();

    BOOST_CHECK_EQUAL(std::string("[{}]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array();
        json.push_object().pop_object();
        json.push_object().pop_object();
    json.pop_array();

    BOOST_CHECK_EQUAL(std::string("[{},{}]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object();
        json.key("array").push_array().pop_array();
    json.pop_object();

    BOOST_CHECK_EQUAL(std::string("{\"array\":[]}"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array().push_object();
        json.key("array").push_array().pop_array();
    json.pop_object().pop_array();

    BOOST_CHECK_EQUAL(std::string("[{\"array\":[]}]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array().push_object()
        .key("obj").push_object()
            .key("subobj").push_object()
                .key("inner1").push_array().pop_array()
                .key("inner2").push_object().pop_object()
            .pop_object()
        .pop_object()
    .pop_object().pop_array();

    BOOST_CHECK_EQUAL(std::string("[{\"obj\":{\"subobj\":{\"inner1\":[],\"inner2\":{}}}}]"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object();
        json.key("op[]").push_array().pop_array();
    json.pop_object();

    BOOST_CHECK_EQUAL(std::string("{\"op[]\":[]}"), oss.str());
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object();
        json.key("one").value(1);
        json.key("two").value(2);
        json.key("three").push_array();
            json.json_null().json_false().json_true();
            json.push_object();
                json.key("null").json_null();
                json.key("foo").value("bar");
                json.key("int").value(-999);
                json.key("PI").value(3.14159);
            json.pop_object();
        json.pop_array();
    json.pop_object();

    BOOST_CHECK_EQUAL(std::string(
        "{\"one\":1,\"two\":2,\"three\":"
            "[null,false,true,"
                "{\"null\":null,\"foo\":\"bar\",\"int\":-999,\"PI\":3.14159}"
            "]"
        "}"), oss.str());
    }
    { // example from the doc
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object()
        .key("version").string("1.2.3")
        .key("values").push_array()
            .number(1).number(2.3).json_true().json_null()
            .push_object().pop_object()
        .pop_array()
    .pop_object();

    BOOST_CHECK_EQUAL(std::string(
        "{\"version\":\"1.2.3\",\"values\":[1,2.3,true,null,{}]}"
        ), oss.str());
    }
}

BOOST_AUTO_TEST_CASE(test_to_json_text) {
    std::vector<std::string> strings;
    strings.push_back("foo");
    strings.push_back("bar");
    strings.push_back("baz");
    BOOST_CHECK_EQUAL(
        std::string("[\"foo\",\"bar\",\"baz\"]"),
        to_json_text(strings)
    );
}

BOOST_AUTO_TEST_CASE(test_json_write_value) {
    std::vector<std::string> strings;
    strings.push_back("foo");
    strings.push_back("bar");
    strings.push_back("baz");

    acme::MyWidget w(true);
    MyFoo foo;

    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object()
        .key("strings").value(strings)
        .key("acme-true").value(w)
        .key("acme-false").value(acme::MyWidget(false))
        .key("foo").value(foo)
    .pop_object();

    BOOST_CHECK_EQUAL(
        std::string("{"
            "\"strings\":[\"foo\",\"bar\",\"baz\"],"
            "\"acme-true\":true,"
            "\"acme-false\":false,"
            "\"foo\":42"
        "}"),
        oss.str()
    );
}

BOOST_AUTO_TEST_CASE(test_invalid_input) {

    {
    std::ostringstream oss;
    JSONWriter json(oss);
    BOOST_CHECK_THROW(json.value(1).value(2), std::runtime_error);
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    BOOST_CHECK_THROW(json.push_object().pop_object().value(1), std::runtime_error);
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object().pop_object();
    BOOST_CHECK_THROW(json.push_object().pop_object(), std::runtime_error);
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_object().pop_object();
    BOOST_CHECK_THROW(json.push_array().pop_array(), std::runtime_error);
    }
    {
    std::ostringstream oss;
    JSONWriter json(oss);
    json.push_array().pop_array();
    BOOST_CHECK_THROW(json.push_object().pop_object(), std::runtime_error);
    }

    {
    std::ostringstream oss;
    BOOST_CHECK_THROW(JSONWriter(oss).push_object(), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE(test_string_encoding) {
    { // control characters
    std::ostringstream oss;
    JSONWriter json(oss);
    json.string("foo\tbar\n\r\f\"baz\b");

    BOOST_CHECK_EQUAL(std::string("\"foo\\tbar\\n\\r\\f\\\"baz\\b\""), oss.str());
    }
    { // forward and backward slashes (aka solidus)
    std::ostringstream oss;
    JSONWriter json(oss);
    json.string("foo/bar/baz\\");

    BOOST_CHECK_EQUAL(std::string("\"foo\\/bar\\/baz\\\\\""), oss.str());
    }
    { // non-printable characters (below ASCII code 32)
    std::ostringstream oss;
    JSONWriter json(oss);
    const char text[] = {
        0x01, 0x17, 0x20 /* 32 i.e. space */,
        'a', 'Z', '9', 0x7f, /* 0x7F, i.e. DEL */
        '\0'
    };
    json.string(text);

    BOOST_CHECK_EQUAL(std::string("\"\\u0001\\u0017 aZ9\\u007f\""), oss.str());
    }
    { // UTF-8 encoded codepoints above 127 / 0x7F
    std::ostringstream oss;
    JSONWriter json(oss);
    json.string(
        "\xE2\x82\xAC" // Euro Sign: U+20AC, 11100010 10000010 10101100
        " 123\n"
        "\xF0\xA4\xAD\xA2" // Chinese Char above BMP, U+24B62, 11110000 10100100 10101101 10100010
    );

    // we currently do not transform UTF-8 encoded multi-byte codepoints
    // and pass them as-is to the stream. JSON documents are supposed to be
    // UTF-8 themselves, although some JSON encoder do convert them all to
    // be full 7-bit ASCII safe, and thus portable.
    const char expected[] =
        "\""
        "\xE2\x82\xAC" // U+20AC
        " 123\\n"
        "\xF0\xA4\xAD\xA2" // U+24B62
        "\""
    ;
    BOOST_CHECK_EQUAL(std::string(expected), oss.str());
    }
}

BOOST_AUTO_TEST_SUITE_END()

/*************************************************************************/

template <typename T>
struct IntegerValueHandler : public JSONHandler {
    T expected_;
    explicit IntegerValueHandler(T expected) : expected_(expected) {}
    bool handle_number(T value) override { return value == expected_; }
};

void assert_parse_ok(const char* fixture, JSONHandler& handler) {
    BOOST_REQUIRE(json_parse(fixture, strlen(fixture), handler));
}

struct JSONStringHandler : public JSONHandler {
    std::string s;
    bool handle_string(const std::string& cs) override { s = cs; return true; }
};

struct JSONDoubleHandler : public JSONHandler {
    double d;
    JSONDoubleHandler() : d(std::numeric_limits<double>::quiet_NaN()) {}
    bool handle_number(double value) override { d = value; return true; }
    bool handle_number(int64_t value) override { d = static_cast<double>(value); return true; }
    bool handle_number(int value) override { d = value; return true; }
};

/*************************************************************************/

BOOST_AUTO_TEST_SUITE(JSONParserTests)

BOOST_AUTO_TEST_CASE(test_parse_scalar) {
    const char* json_inputs[] = {
        // literals
        "null", "true", "false",
        // integers
        "0", "1", "1234", "9999",
        "0", "-1", "-1234","-9999",
        "2147483647", // +2,147,483,647 i.e. +MAX_INT
        "-2147483648", // +2,147,483,648 i.e. -MAX_INT-1
        "4294967295", // +4,294,967,295 i.e.  MAX_UINT
        "9223372036854775807", //  +9,223,372,036,854,775,807
        "-9223372036854775808", //  -9,223,372,036,854,775,808
        "18446744073709551615", // +18,446,744,073,709,551,615
        // reals. Note that because of the way ostream works, how the real
        // gets rewritten as text can vary quite a bit, like 1.0 written as "1"
        // so we separately text conversion in test_parse_reals().
        "1.000001", // requires oss.precision(15) otherwise just "1".
        "0.1", "0.123", "-102.1234", "6200400.759",
        // strings
        "\"foo\"",
        "\"foo bar\"",
        "\"foo\"",
        "\"foo bar\\t\\nbaz\"" // '\t' and '\n' must be json-encoded.
    };
    for (const char* json_input : json_inputs) {
        std::ostringstream oss;
        oss.precision(15); // default precision looses values
        //oss.setf(ios::fixed); // Gets 1.000001000000000 instead of 1.000001
        JSONHandler_ptr handler(json_handler_writer(oss));
        json_parse(json_input, strlen(json_input), *handler);
        BOOST_CHECK_EQUAL(std::string(json_input), oss.str());
    }
}

BOOST_AUTO_TEST_CASE(test_parse_reals) {
    const double values[] = {
        0.0, 1.0, 1.000001, 0.1, 0.123, -102.1234, 6200400.759,
        -1, 1e6, 1.1e10
    };
    for (double d : values) {
        std::ostringstream oss;
        oss.precision(15); // default precision looses values
        //oss.setf(ios::fixed); // Gets 1.000001000000000 instead of 1.000001
        JSONWriter writer(oss);
        writer.number(d);
        const std::string json = oss.str();
        JSONDoubleHandler handler;
        json_parse(json.data(), json.size(), handler);
        BOOST_CHECK_EQUAL(d, handler.d);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_composite) {
    const char* inputs[] = {
        "{}",
        "{\"\":null}", // empty key allowed
        "{\" \":[ ]}",
        "{\"foo\":\"bar\"}",
        "{\"map\":{}}",
        "{\"vec\":[]}",
        "{\"map\":{},\"vec\":[]}",
        "{\"one\":1,\"two\":2,\"more\":[3,4,5,\"six\"]}",
        "[]", "[\"foo\"]", "[0]", "[-1]", "[1]",
        "[\"foo\",\"bar\"]", "[1, 2, 3]",
        "[[]]", "[[[]]]",
        "[null,true,false]",
        "[{},[],null,true,false,[[]],{\"key\":\"1\"}]"
    };
    for (const char* input : inputs) {
        std::ostringstream oss;
        JSONHandler_ptr handler(json_handler_writer(oss));
        json_parse(input, strlen(input), *handler);
        BOOST_CHECK_EQUAL(std::string(input), oss.str());
    }

    const char* invalid_inputs[] = {
        // Invalid inputs
        "]",
        ",]",
        "}",
        ",}",

        // Invalid vectors
        "[",
        "[,",
        "[,]",
        "[1,",
        "[1,]",
        "[,1",
        "[,1]",
        "[1 1]",

        // Invalid maps
        "{",
        "{\"a\"",
        "{\"a\",",
        "{\"a\":",
        "{\"a\":,",
        "{\"a\":}",
        "{\"a\":1",
        "{\"a\":1,",
        "{\"a\":1,}",
        "{,",
        "{,\"a\"",
        "{,\"a\":",
        "{,\"a\":1",
        "{,\"a\":1}",
        "{,}",
        "{:",
        "{:1",
        "{:}",
        "{\"a\":1 \"b\":2}",

        // Invalid map keys
        "{1:1}",
        "{true:1}",
        "{false:1}",
        "{null:1}"
    };
    for (const char* input : invalid_inputs) {
        BOOST_CHECK_EQUAL(json_validate_syntax(input, strlen(input)), false);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_with_spaces) {
    const char* inputs[] = {
        " { } ",
        " {  \"k\"   :    [    ]     }      ",
        " {  \"k\"   :    {     \"x\"      :       1       }        }         ",
        " [ ] ",
        " [  [   ]    ]     ",
        " [  [   [    ]     ]      ]       ",
        "{ \"foo\":\"bar\" }",
        "{\"map\" : {}}",
        " {\"vec\":[]} ",
        " {\n\"map\":\t{},\n\"vec\":\t[]\n}",
        "[\r\t\n]", "  [\"foo\"]  ",
        "[null,true , false]",
        "[{ },[  ], null,true , false , [ [] ], { \"key\" : \"1\"}]"
    };
    for (const char* input : inputs) {
        std::ostringstream oss;
        JSONHandler_ptr handler(json_handler_writer(oss));
        BOOST_CHECK(json_parse(input, strlen(input), *handler));
        BOOST_CHECK_EQUAL(std::string(input), oss.str());
    }
}

BOOST_AUTO_TEST_CASE(test_string_decoding) {
    const char cent_sign_utf8[] = "\xC2\xA2";
    const char euro_sign_utf8[] = "\xE2\x82\xAC";

    // input is JSON-encoded string (w/o surrounding double-quotes)
    // output is expected decoded/raw string (0 if same as input)
    const MyStringPair fixtures[] = {
        // empty string
        { "", 0 },

        // "normal" strings (no escape, unicode, non-printable, etc...)
        { "a", 0 },
        { "Z", 0 },
        { "0", 0 },
        { "9", 0 },
        { "/", "/" }, // forward-slash, unescaped

        // escapes
        { "\\/", "/" },
        { "\\\"", "\"" },
        { "\\\\", "\\" },
        { "\\/", "/" },  // forward-slash, escaped (both valid)
        { "\\b", "\b" },
        { "\\f", "\f" },
        { "\\n", "\n" },
        { "\\r", "\r" },
        { "\\t", "\t" },

        // unicode escapes \uFFFF
        { "\\u0024", "$" },          // Dollar Sign: U+0024, 00100100
        { "\\u00A2", cent_sign_utf8 }, // Cent Sign: U+00A2, 11000010 10100010
        { "\\u00a2", cent_sign_utf8 },
        { "\\u20AC", euro_sign_utf8 }, // Euro Sign: U+20AC, 11100010 10000010 10101100
        { "\\u20ac", euro_sign_utf8 }
    };
    for (const MyStringPair& fixture : fixtures) {
        // wrap input in double-quotes, to get single string value
        std::ostringstream input_oss;
        input_oss << '"' << fixture.input << '"';
        const std::string input = input_oss.str();

        JSONStringHandler handler;
        json_parse(input.data(), input.size(), handler);

        const std::string expected_output = fixture.output? fixture.output: fixture.input;
        BOOST_CHECK_EQUAL(expected_output, handler.s);
    }

    // TODO: unicode surrogate-pair escapes \uFFFF\uFFFF
    {
    // U+1000 (first non-BMP codepoint)
    const char* input = "\"\\uD800\\uDC00\"";
    JSONStringHandler handler;
    BOOST_CHECK_THROW(json_parse(input, strlen(input), handler), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_integer) {
    struct ObjIntValueHandler : public JSONHandler {
        bool handle_object_begin() override { return true; }
        bool handle_object_key(const std::string& key) override { return key == "m"; }
        bool handle_number(int value) override { return value == 1; }
        bool handle_object_end() override { return true; }
    } handler1;
    assert_parse_ok("{\"m\":1}", handler1);

    struct AryIntValueHandler : public JSONHandler {
        bool handle_array_begin() override { return true; }
        bool handle_number(int value) override { return value == 1; }
        bool handle_array_end() override { return true; }
    } handler2;
    assert_parse_ok("[1]", handler2);
}

BOOST_AUTO_TEST_CASE(test_parse_integer_overload_selection) {
    const int max_int32 = std::numeric_limits<int>::max();
    const int min_int32 = -max_int32 - 1;

    const int64_t max_int64 = std::numeric_limits<int64_t>::max();
    const int64_t min_int64 = -max_int64 - 1;

    // Min/Max signed 32-bit
    IntegerValueHandler<int> max_int_handler(max_int32);
    assert_parse_ok("2147483647", max_int_handler);

    IntegerValueHandler<int> min_int_handler(min_int32);
    assert_parse_ok("-2147483648", min_int_handler);

    // Crossing into 64-bit
    IntegerValueHandler<int64_t> max_int_plus1_handler(1 + (int64_t)max_int32);
    assert_parse_ok("2147483648", max_int_plus1_handler);

    IntegerValueHandler<int64_t> min_int_minus1_handler(-1 + (int64_t)min_int32);
    assert_parse_ok("-2147483649", min_int_minus1_handler);

    // Min/Max signed 64-bit
    IntegerValueHandler<int64_t> max_int64_handler(max_int64);
    assert_parse_ok("9223372036854775807", max_int64_handler);

    IntegerValueHandler<int64_t> min_int64_handler(min_int64);
    assert_parse_ok("-9223372036854775808", min_int64_handler);

    // Cross into large unsigned 64-bit, beyond max signed 64-bit
    IntegerValueHandler<uint64_t> max_int64_plus1_handler(1 + (uint64_t)max_int64);
    assert_parse_ok("9223372036854775808", max_int64_plus1_handler);
}

BOOST_AUTO_TEST_CASE(test_parse_integer_overflow) {
    // Max unsigned 64-bit overflow
    {
    JSONHandler handler;
    BOOST_CHECK_THROW(json_parse("18446744073709551616", 20, handler), std::runtime_error);
    }

    // Min signed 64-bit overflow
    {
    JSONHandler handler;
    BOOST_CHECK_THROW(json_parse("-9223372036854775809", 20, handler), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_map) {
    std::map<std::string, std::string> map;
    {
        map.clear();
        const char* s = "{}";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(0), map.size());
    }
    {
        map.clear();
        const char* s = "{\"k\":\"v\"}";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("v"), map["k"]);
    }
    {
        map.clear();
        const char* s = "{\"k\": 1.235789012345}";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("1.235789012345"), map["k"]);
    }
    {
        map.clear();
        const char* s = "{\"k\": -3.14e-5}";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("-3.14e-5"), map["k"]);
    }
    {
        map.clear();
        const char* s = "{\"k\":true }";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("true"), map["k"]);
    }
    {
        map.clear();
        const char* s = "{\"k\": false }";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("false"), map["k"]);
    }
    {
        map.clear();
        const char* s = "{\"f\": false , \"t\": true, \"d\": 1.1, \"i\": 2, \"e\": \"\", \"s\": \"s\"}";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(6), map.size());
        BOOST_CHECK_EQUAL(std::string("false"), map["f"]);
        BOOST_CHECK_EQUAL(std::string("true"), map["t"]);
        BOOST_CHECK_EQUAL(std::string("1.1"), map["d"]);
        BOOST_CHECK_EQUAL(std::string("2"), map["i"]);
        BOOST_CHECK_EQUAL(std::string(""), map["e"]);
        BOOST_CHECK_EQUAL(std::string("s"), map["s"]);
    }
    {
        map.clear();
        const char* s = "";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "{\"k\": null }";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "{\"k\": {} }";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "{\"k\": [] }";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "[]";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "1";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "true";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "false";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "null";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "\"foo\"";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "{\"k\": 1, \"k\": 2}";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("1"), map["k"]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, true), std::runtime_error);
    }
    // Type constraints
    {
        map.clear();
        const char* s = "{\"k\": \"v\"}";
        BOOST_CHECK(json_parse(s, strlen(s), map, false, JSON_TYPE_STRING));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("v"), map["k"]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_NUMBER), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_BOOLEAN), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "{\"k\": 1}";
        BOOST_CHECK(json_parse(s, strlen(s), map, false, JSON_TYPE_NUMBER));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("1"), map["k"]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_STRING), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_BOOLEAN), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "{\"k\": true}";
        BOOST_CHECK(json_parse(s, strlen(s), map, false, JSON_TYPE_BOOLEAN));
        BOOST_CHECK_EQUAL(size_t(1), map.size());
        BOOST_CHECK_EQUAL(std::string("true"), map["k"]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_STRING), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_NUMBER), std::runtime_error);
    }
    {
        map.clear();
        const char* s = "{\"a\": \"v\", \"b\": 1, \"c\": true}";
        BOOST_CHECK(json_parse(s, strlen(s), map));
        BOOST_CHECK_EQUAL(size_t(3), map.size());
        BOOST_CHECK_EQUAL(std::string("v"), map["a"]);
        BOOST_CHECK_EQUAL(std::string("1"), map["b"]);
        BOOST_CHECK_EQUAL(std::string("true"), map["c"]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_STRING), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_NUMBER), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), map, false, JSON_TYPE_BOOLEAN), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_vector) {
    std::vector<std::string> vec;
    {
        vec.clear();
        const char* s = "[]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(0), vec.size());
    }
    {
        vec.clear();
        const char* s = "[\"k\", \"v\"]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(2), vec.size());
        BOOST_CHECK_EQUAL(std::string("k"), vec[0]);
        BOOST_CHECK_EQUAL(std::string("v"), vec[1]);
    }
    {
        vec.clear();
        const char* s = "[ 1.235789012345 ]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(1), vec.size());
        BOOST_CHECK_EQUAL(std::string("1.235789012345"), vec[0]);
    }
    {
        vec.clear();
        const char* s = "[ -3.14e-5 ]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(1), vec.size());
        BOOST_CHECK_EQUAL(std::string("-3.14e-5"), vec[0]);
    }
    {
        vec.clear();
        const char* s = "[ true ]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(1), vec.size());
        BOOST_CHECK_EQUAL(std::string("true"), vec[0]);
    }
    {
        vec.clear();
        const char* s = "[ false ]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(1), vec.size());
        BOOST_CHECK_EQUAL(std::string("false"), vec[0]);
    }
    {
        vec.clear();
        const char* s = "[ false ,  true,  1.1,  2,  \"\",  \"s\"]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(6), vec.size());
        BOOST_CHECK_EQUAL(std::string("false"), vec[0]);
        BOOST_CHECK_EQUAL(std::string("true"), vec[1]);
        BOOST_CHECK_EQUAL(std::string("1.1"), vec[2]);
        BOOST_CHECK_EQUAL(std::string("2"), vec[3]);
        BOOST_CHECK_EQUAL(std::string(""), vec[4]);
        BOOST_CHECK_EQUAL(std::string("s"), vec[5]);
    }
    {
        vec.clear();
        const char* s = "";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "[\"k\" \"v\"]";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "[ null ]";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "[ {} ]";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "[ [] ]";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "{}";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "1";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "true";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "false";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "null";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "\"foo\"";
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec), std::runtime_error);
    }
    // Type constraints
    {
        vec.clear();
        const char* s = "[\"v\"]";
        BOOST_CHECK(json_parse(s, strlen(s), vec, JSON_TYPE_STRING));
        BOOST_CHECK_EQUAL(size_t(1), vec.size());
        BOOST_CHECK_EQUAL(std::string("v"), vec[0]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_NUMBER), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_BOOLEAN), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "[1]";
        BOOST_CHECK(json_parse(s, strlen(s), vec, JSON_TYPE_NUMBER));
        BOOST_CHECK_EQUAL(size_t(1), vec.size());
        BOOST_CHECK_EQUAL(std::string("1"), vec[0]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_STRING), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_BOOLEAN), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "[true]";
        BOOST_CHECK(json_parse(s, strlen(s), vec, JSON_TYPE_BOOLEAN));
        BOOST_CHECK_EQUAL(size_t(1), vec.size());
        BOOST_CHECK_EQUAL(std::string("true"), vec[0]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_STRING), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_NUMBER), std::runtime_error);
    }
    {
        vec.clear();
        const char* s = "[\"v\", 1, true]";
        BOOST_CHECK(json_parse(s, strlen(s), vec));
        BOOST_CHECK_EQUAL(size_t(3), vec.size());
        BOOST_CHECK_EQUAL(std::string("v"), vec[0]);
        BOOST_CHECK_EQUAL(std::string("1"), vec[1]);
        BOOST_CHECK_EQUAL(std::string("true"), vec[2]);

        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_STRING), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_NUMBER), std::runtime_error);
        BOOST_CHECK_THROW(json_parse(s, strlen(s), vec, JSON_TYPE_BOOLEAN), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE(test_format) {
    const char* inputs[] = {
        "{}",
        "[]",
        "[null,true,false,1]",
        "{\"1\":1}",
        "{\"1\":1,\"2\": true}",
        "{\"1\":1,\"2\":  [   true   ]}",
        "{\"1\":1,\"2\":{\"3\":  [   true, false   ]}  , \"4\":null}",
        "[1,[2,[3,[4,[5,[6,[7,[8,[9,[10,[11,[12,[13,[14,[15,[16,[17,[]]]]]]]]]]]]]]]]]]",
    };
    const char* outputs[] = {
        "{\n}",
        "[\n]",
        "[\n\tnull,\n\ttrue,\n\tfalse,\n\t1\n]",
        "{\n\t\"1\": 1\n}",
        "{\n\t\"1\": 1,\n\t\"2\": true\n}",
        "{\n\t\"1\": 1,\n\t\"2\": [\n\t\ttrue\n\t]\n}",
        "{\n\t\"1\": 1,\n\t\"2\": {\n\t\t\"3\": [\n\t\t\ttrue,\n\t\t\tfalse\n\t\t]\n\t},\n\t\"4\": null\n}",
        "[\n\t1,\n\t"
        "[\n\t\t2,\n\t\t"
        "[\n\t\t\t3,\n\t\t\t"
        "[\n\t\t\t\t4,\n\t\t\t\t"
        "[\n\t\t\t\t\t5,\n\t\t\t\t\t"
        "[\n\t\t\t\t\t\t6,\n\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t7,\n\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t8,\n\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t9,\n\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t10,\n\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t11,\n\t\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t\t12,\n\t\t\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t\t\t13,\n\t\t\t\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t14,\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t15,\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t16,\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t17,\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
        "[\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t\t]\n"
        "\t\t\t\t\t\t]\n"
        "\t\t\t\t\t]\n"
        "\t\t\t\t]\n"
        "\t\t\t]\n"
        "\t\t]\n"
        "\t]\n"
        "]"
    };

    const size_t count = sizeof(inputs) / sizeof(*inputs);
    for (size_t i = 0; i < count; ++i) {
        const std::string expected = outputs[i];
        const std::string& actual = json_format(inputs[i], strlen(inputs[i]));
        BOOST_CHECK(json_validate_syntax(actual.c_str(), actual.size()));

        const std::string actualNoWS = json_remove_whitespace(actual.c_str(), actual.size());
        const std::string expectedNoWS = json_remove_whitespace(expected.c_str(), expected.size());
        BOOST_CHECK_EQUAL(actualNoWS, expectedNoWS);

        BOOST_CHECK_EQUAL(expected, actual);
    }
}

BOOST_AUTO_TEST_CASE(test_json_type) {
    BOOST_CHECK_EQUAL(json_type_string_of(JSON_TYPE_INVALID), "");
    BOOST_CHECK_EQUAL(json_type_string_of(JSON_TYPE_OBJECT),  "object");
    BOOST_CHECK_EQUAL(json_type_string_of(JSON_TYPE_ARRAY),   "array");
    BOOST_CHECK_EQUAL(json_type_string_of(JSON_TYPE_STRING),  "string");
    BOOST_CHECK_EQUAL(json_type_string_of(JSON_TYPE_NUMBER),  "number");
    BOOST_CHECK_EQUAL(json_type_string_of(JSON_TYPE_BOOLEAN), "boolean");
    BOOST_CHECK_EQUAL(json_type_string_of((JSONType)1000), "");
    BOOST_CHECK_EQUAL(json_type_string_of((JSONType)-1), "");

    BOOST_CHECK_EQUAL(json_type_value_of("object"), JSON_TYPE_OBJECT);
    BOOST_CHECK_EQUAL(json_type_value_of("array"), JSON_TYPE_ARRAY);
    BOOST_CHECK_EQUAL(json_type_value_of("string"), JSON_TYPE_STRING);
    BOOST_CHECK_EQUAL(json_type_value_of("number"), JSON_TYPE_NUMBER);
    BOOST_CHECK_EQUAL(json_type_value_of("boolean"), JSON_TYPE_BOOLEAN);
    BOOST_CHECK_EQUAL(json_type_value_of("anytype"), JSON_TYPE_INVALID);
    BOOST_CHECK_EQUAL(json_type_value_of(""), JSON_TYPE_INVALID);

    BOOST_CHECK_EQUAL(json_type_of("  \"a\"  "), JSON_TYPE_STRING);
    BOOST_CHECK_EQUAL(json_type_of("  1  "), JSON_TYPE_NUMBER);
    BOOST_CHECK_EQUAL(json_type_of("  1   "), JSON_TYPE_NUMBER);
    BOOST_CHECK_EQUAL(json_type_of("  1.2  "), JSON_TYPE_NUMBER);
    BOOST_CHECK_EQUAL(json_type_of("  1.2e1  "), JSON_TYPE_NUMBER);
    BOOST_CHECK_EQUAL(json_type_of("  true  "), JSON_TYPE_BOOLEAN);
    BOOST_CHECK_EQUAL(json_type_of("  false  "), JSON_TYPE_BOOLEAN);
    BOOST_CHECK_EQUAL(json_type_of("  null  "), JSON_TYPE_OBJECT);
    BOOST_CHECK_EQUAL(json_type_of("  { }  "), JSON_TYPE_OBJECT);
    BOOST_CHECK_EQUAL(json_type_of("  { \"a\":1 }  "), JSON_TYPE_OBJECT);
    BOOST_CHECK_EQUAL(json_type_of("  { \"a\":[1], \"b\":[2] }  "), JSON_TYPE_OBJECT);
    BOOST_CHECK_EQUAL(json_type_of("  [ ]  "), JSON_TYPE_ARRAY);
    BOOST_CHECK_EQUAL(json_type_of("  [ 1 ]  "), JSON_TYPE_ARRAY);
    BOOST_CHECK_EQUAL(json_type_of("  [ {\"a\":1} ]  "), JSON_TYPE_ARRAY);
    BOOST_CHECK_EQUAL(json_type_of(""), JSON_TYPE_INVALID);
    BOOST_CHECK_EQUAL(json_type_of("a"), JSON_TYPE_INVALID);
    BOOST_CHECK_EQUAL(json_type_of("1,"), JSON_TYPE_INVALID);
    BOOST_CHECK_EQUAL(json_type_of("1 1"), JSON_TYPE_INVALID);
}

BOOST_AUTO_TEST_SUITE_END()

/*************************************************************************/

struct AdHocHandler {
    enum State {
        AH_BEFORE_KEY, AH_TARGET_KEY, AH_INSIDE_KEY, AH_AFTER_KEY
    };

    std::ostream& os_;
    std::string key_;
    State state_;
    bool first_;
    int level_;

    AdHocHandler(
        std::ostream& os,
        const std::string& key
    ) : os_(os)
      , key_(key)
      , state_(AH_BEFORE_KEY)
      , first_(true)
      , level_(0)
    {
    }

    void assert_state_transition(State expected_state, State next_state) {
        if (state_ != expected_state) {
            throw std::runtime_error("Unexpected JSON content");
        }
        state_ = next_state;
    }

    void operator()(JSONParsingEventType evt, const JSONRawStringRef& text) {
        // adjust the level
        switch (evt) {
        case JSON_OBJECT_BEGIN:
        case JSON_ARRAY_BEGIN:
            ++level_;
            break;

        case JSON_OBJECT_END:
        case JSON_ARRAY_END:
            --level_;
            break;

        default:
            break;
        }

        switch (level_) {
        case 0:
            if (evt == JSON_OBJECT_END) { // can be JSON_SYNTAX_WHITESPACE too
                assert_state_transition(AH_AFTER_KEY, AH_BEFORE_KEY);
            }
            break;

        case 1:
            if (evt == JSON_OBJECT_KEY && 0 == strncmp(text.beg_ + 1, key_.c_str(), key_.size())) {
                assert_state_transition(AH_BEFORE_KEY, AH_TARGET_KEY);
            } else if (evt == JSON_ARRAY_END   && state_ == AH_INSIDE_KEY) {
                assert_state_transition(AH_INSIDE_KEY, AH_AFTER_KEY);
                first_ = false;
            }
            break;

        case 2:
            if (state_ == AH_TARGET_KEY && evt == JSON_ARRAY_BEGIN) {
                state_ = AH_INSIDE_KEY;
                if (!first_) {
                    // Convert array-start into array-comma to append to array.
                    // For proper formatting, we should also print the next
                    // whitespace, if any, but that complicates things.
                    os_ << ',';
                    return;
                }
            }
            break;
        }

        switch (state_) {
        case AH_BEFORE_KEY:
        case AH_TARGET_KEY: if (first_) os_ << text; break; // output first-doc only
        case AH_INSIDE_KEY:             os_ << text; break; // always output
        case AH_AFTER_KEY:                           break; // always ignore
        }
    }

    static void handle_event(
        JSONParsingEventType evt, const JSONRawStringRef& text, void* user_data
    ) {
        BOOST_ASSERT(!text.empty());
        AdHocHandler& self = *static_cast<AdHocHandler*>(user_data);
        self(evt, text);
    }

private:
    AdHocHandler& operator=(const AdHocHandler&);
};

/*************************************************************************/

BOOST_AUTO_TEST_SUITE(JSONAdHocHandlerTests)

BOOST_AUTO_TEST_CASE(test_handler) {
    const char input1[] =
"{\n"
" \"objectIdFieldName\": \"objectid\",\n"
" \"globalIdFieldName\": \"\",\n"
" \"geometryType\": \"esriGeometryPoint\",\n"
" \"spatialReference\": {\n"
"  \"wkid\": 102100,\n"
"  \"latestWkid\": 3857\n"
" },\n"
" \"fields\": [\n"
"  {\n"
"   \"name\": \"objectid\",\n"
"   \"alias\": \"OBJECTID\",\n"
"   \"type\": \"esriFieldTypeOID\"\n"
"  },\n"
"  {\n"
"   \"name\": \"status\",\n"
"   \"alias\": \"Status\",\n"
"   \"type\": \"esriFieldTypeString\",\n"
"   \"length\": 30\n"
"  }\n"
" ],\n"
" \"features\": [\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809842.1384958792,\n"
"    \"y\": 5130819.1852016831\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251124,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1283 E Ogden Ave\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809689.7604571786,\n"
"    \"y\": 5130828.8896024479\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251125,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1563 Naperville/wheaton Rd\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9810297.9148924593,\n"
"    \"y\": 5129638.513603243\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8252123,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1131 Dickens Ave\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  }\n"
" ]\n"
"}\n"
      ;

    const char input2[] =
"{\n"
" \"objectIdFieldName\": \"objectid\",\n"
" \"globalIdFieldName\": \"\",\n"
" \"geometryType\": \"esriGeometryPoint\",\n"
" \"spatialReference\": {\n"
"  \"wkid\": 102100,\n"
"  \"latestWkid\": 3857\n"
" },\n"
" \"fields\": [\n"
"  {\n"
"   \"name\": \"objectid\",\n"
"   \"alias\": \"OBJECTID\",\n"
"   \"type\": \"esriFieldTypeOID\"\n"
"  },\n"
"  {\n"
"   \"name\": \"status\",\n"
"   \"alias\": \"Status\",\n"
"   \"type\": \"esriFieldTypeString\",\n"
"   \"length\": 30\n"
"  }\n"
" ],\n"
" \"features\": [\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809842.1384958792,\n"
"    \"y\": 5130819.1852016831\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251124,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1283 E Ogden Ave\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809689.7604571786,\n"
"    \"y\": 5130828.8896024479\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251125,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1563 Naperville/wheaton Rd\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809680.7851078119,\n"
"    \"y\": 5130824.3444647249\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251126,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1565 Naperville/wheaton Rd\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  }\n"
" ]\n"
"}\n"
      ;

    const char expected[] =
"{\n"
" \"objectIdFieldName\": \"objectid\",\n"
" \"globalIdFieldName\": \"\",\n"
" \"geometryType\": \"esriGeometryPoint\",\n"
" \"spatialReference\": {\n"
"  \"wkid\": 102100,\n"
"  \"latestWkid\": 3857\n"
" },\n"
" \"fields\": [\n"
"  {\n"
"   \"name\": \"objectid\",\n"
"   \"alias\": \"OBJECTID\",\n"
"   \"type\": \"esriFieldTypeOID\"\n"
"  },\n"
"  {\n"
"   \"name\": \"status\",\n"
"   \"alias\": \"Status\",\n"
"   \"type\": \"esriFieldTypeString\",\n"
"   \"length\": 30\n"
"  }\n"
" ],\n"
" \"features\": [\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809842.1384958792,\n"
"    \"y\": 5130819.1852016831\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251124,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1283 E Ogden Ave\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809689.7604571786,\n"
"    \"y\": 5130828.8896024479\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251125,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1563 Naperville/wheaton Rd\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9810297.9148924593,\n"
"    \"y\": 5129638.513603243\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8252123,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1131 Dickens Ave\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809842.1384958792,\n"
"    \"y\": 5130819.1852016831\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251124,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1283 E Ogden Ave\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809689.7604571786,\n"
"    \"y\": 5130828.8896024479\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251125,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1563 Naperville/wheaton Rd\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  },\n"
"  {\n"
"   \"geometry\": {\n"
"    \"x\": -9809680.7851078119,\n"
"    \"y\": 5130824.3444647249\n"
"   },\n"
"   \"attributes\": {\n"
"    \"objectid\": 8251126,\n"
"    \"unittype\": null,\n"
"    \"unitid\": null,\n"
"    \"altunittype\": null,\n"
"    \"altunitid\": null,\n"
"    \"fulladdr\": \"1565 Naperville/wheaton Rd\",\n"
"    \"municipality\": \"City of Naperville\",\n"
"    \"status\": \"Current\"\n"
"   }\n"
"  }\n"
" ]\n"
"}\n"
      ;

    BOOST_CHECK(json_validate_syntax(input1, sizeof(input1) -1));
    BOOST_CHECK(json_validate_syntax(input2, sizeof(input2) -1));
    BOOST_CHECK(json_validate_syntax(expected, sizeof(expected) -1));

    // explicitly set fixed notation and 10 fractional digits for these docs
    std::ostringstream oss;
    AdHocHandler handler(oss, "features");
    BOOST_REQUIRE(0 == json_parse_syntax(input1, sizeof(input1) - 1, &AdHocHandler::handle_event, &handler));
    BOOST_REQUIRE(0 == json_parse_syntax(input2, sizeof(input2) - 1, &AdHocHandler::handle_event, &handler));
    oss << ']' << '}'; // finish to doc, so it's well-formed

    const std::string result = oss.str();
    BOOST_CHECK(json_validate_syntax(result.c_str(), result.size()));

    const std::string resultNoWS = json_remove_whitespace(result.c_str(), result.size());
    const std::string expectedNoWS = json_remove_whitespace(expected, sizeof(expected) - 1);
    BOOST_CHECK_EQUAL(resultNoWS, expectedNoWS);
}

BOOST_AUTO_TEST_CASE(bench_adhoc_handler) {
    // For DD,  on WIN32: C:/oes/misc/giga-json-cat
    //          on LINUX: /oradata1/ddevienne/giga-json-cat
    const std::string suffix = ".json";
    const std::string& data_dir = oes::core::System::getEnv(
        "OES_JSON_ADHOC_HANDLER_TEST_DATA_DIR"
    );
    if (data_dir.empty()) {
        return; // silently ignore
    }

    const std::string& outFilename = data_dir + "/json-ALL" + suffix;
    std::ofstream ofs(outFilename.c_str(), std::ios::binary|std::ios::trunc);
    BOOST_REQUIRE(ofs.good());

    AdHocHandler handler(ofs, "features");

    std::vector<char> buffer;
    buffer.reserve(64 * 1024);
    for (int i = 1; i <= 1000; ++i) {
        // build the filename
        std::ostringstream oss;
        oss << data_dir << "/json" << i << suffix;
        const std::string& filename = oss.str();

        // read the whole file into a buffer.
        // FIXME: current json_parse requires an in-memory buffer
        std::ifstream ifs(filename.c_str(), std::ios::binary|std::ios::ate);
        if (!ifs.good()) {
            // If we could read at least 2 input files, we did concat something
            BOOST_REQUIRE(i > 2);
            break;
        }
        std::ifstream::pos_type pos = ifs.tellg();

        buffer.resize(pos);

        ifs.seekg(0, std::ios::beg);
        ifs.read(&buffer[0], pos);

        if (0 != json_parse_syntax(&buffer[0], pos, &AdHocHandler::handle_event, &handler)) {
            throw std::runtime_error("Reading " + filename);
        }
    }
    ofs << ']' << '\n' << '}'; // finish to doc, so it's well-formed

    //const std::string result = oss.str();
    //BOOST_CHECK(json_validate_syntax(result.c_str(), result.size()));
}

BOOST_AUTO_TEST_SUITE_END()

/*************************************************************************/

BOOST_AUTO_TEST_SUITE(JSONReaderTests)

BOOST_AUTO_TEST_CASE(test_get_literals) {
    {
    const char json_text[] = "true";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_TRUE);
    BOOST_REQUIRE_EQUAL(json.token().to_string(), std::string(json_text));
    BOOST_REQUIRE_EQUAL(json.get_boolean(), true);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    // JSON_END ad vitam aeternam...
    }

    {
    const char json_text[] = "false";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_FALSE);
    BOOST_REQUIRE_EQUAL(json.token().to_string(), std::string(json_text));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }

    {
    const char json_text[] = "null";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_NULL);
    BOOST_REQUIRE_EQUAL(json.token().to_string(), std::string(json_text));
    BOOST_REQUIRE_THROW(json.get_boolean(), std::runtime_error);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }
}

BOOST_AUTO_TEST_CASE(test_get_integers_32) {
    const int values[] = {
        std::numeric_limits<int>::min(),
        -1, 0, +1,
        std::numeric_limits<int>::max()
    };
    for (int value : values) {
        char buffer[16];
        sprintf(buffer, "%d", value);
        JSONReader json(buffer, strlen(buffer));

        BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
        BOOST_REQUIRE_EQUAL(json.get_int(), value);
        BOOST_REQUIRE_EQUAL(json.get_int64_t(), int64_t(value));
        if (value < 0) {
            BOOST_REQUIRE_THROW(json.get_uint64_t(), std::runtime_error);
        } else {
            BOOST_REQUIRE_EQUAL(json.get_uint64_t(), uint64_t(value));
        }
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }
}

BOOST_AUTO_TEST_CASE(test_get_integers_64) {
    const int64_t values[] = {
        std::numeric_limits<int64_t>::min(),
        int64_t(std::numeric_limits<int>::min()) - 1,
        int64_t(std::numeric_limits<int>::max()) + 1,
        std::numeric_limits<int64_t>::max()
    };
    for (int64_t value : values) {
        std::ostringstream oss;
        oss << value;
        const std::string& json_text = oss.str();
        JSONReader json(json_text.c_str(), json_text.size());

        BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
        BOOST_REQUIRE_THROW(json.get_int(), std::runtime_error); // Overflow
        BOOST_REQUIRE_EQUAL(json.get_int64_t(), value);
        if (value < 0) {
            BOOST_REQUIRE_THROW(json.get_uint64_t(), std::runtime_error);
        } else {
            BOOST_REQUIRE_EQUAL(json.get_uint64_t(), uint64_t(value));
        }
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }
}

BOOST_AUTO_TEST_CASE(test_get_float) {
    // sprintf not quite compatible with num-limits min/max
    const float values[] = {
        //std::numeric_limits<float>::min(),
        -1.0f, -1.23f, 0.0f, +1.0f, +4.56f
        //std::numeric_limits<float>::max()
    };
    for (float value : values) {
        char buffer[64];
        sprintf(buffer, "%f", value);
        JSONReader json(buffer, strlen(buffer));

        BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
        BOOST_REQUIRE_THROW(json.get_int(), std::runtime_error);
        BOOST_REQUIRE_THROW(json.get_int64_t(), std::runtime_error);
        BOOST_REQUIRE_THROW(json.get_uint64_t(), std::runtime_error);
        BOOST_REQUIRE_EQUAL(json.get_float(), value);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }
}

BOOST_AUTO_TEST_CASE(test_get_double) {
    // sprintf not quite compatible with num-limits min/max
    const double values[] = {
        //std::numeric_limits<double>::min(),
        -1.0, -1.23, 0.0, +1.0, +4.56
        //std::numeric_limits<double>::max()
    };
    for (double value : values) {
        char buffer[64];
        sprintf(buffer, "%lf", value);
        JSONReader json(buffer, strlen(buffer));

        BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
        BOOST_REQUIRE_THROW(json.get_int(), std::runtime_error);
        BOOST_REQUIRE_THROW(json.get_int64_t(), std::runtime_error);
        BOOST_REQUIRE_THROW(json.get_uint64_t(), std::runtime_error);
        BOOST_REQUIRE_EQUAL(json.get_double(), value);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }
}

BOOST_AUTO_TEST_CASE(test_get_string) {
    const char cent_sign_utf8[] = "\xC2\xA2";
    const char euro_sign_utf8[] = "\xE2\x82\xAC";

    // input is JSON-encoded string (w/o surrounding double-quotes)
    // output is expected decoded/raw string (0 if same as input)
    const MyStringPair fixtures[] = {
        // empty string
        { "", 0 },

        // "normal" strings (no escape, unicode, non-printable, etc...)
        { "a", 0 },
        { "Z", 0 },
        { "0", 0 },
        { "9", 0 },
        { "/", "/" }, // forward-slash, unescaped

        // escapes
        { "\\/", "/" },
        { "\\\"", "\"" },
        { "\\\\", "\\" },
        { "\\/", "/" },  // forward-slash, escaped (both valid)
        { "\\b", "\b" },
        { "\\f", "\f" },
        { "\\n", "\n" },
        { "\\r", "\r" },
        { "\\t", "\t" },

        // unicode escapes \uFFFF
        { "\\u0024", "$" },          // Dollar Sign: U+0024, 00100100
        { "\\u00A2", cent_sign_utf8 }, // Cent Sign: U+00A2, 11000010 10100010
        { "\\u00a2", cent_sign_utf8 },
        { "\\u20AC", euro_sign_utf8 }, // Euro Sign: U+20AC, 11100010 10000010 10101100
        { "\\u20ac", euro_sign_utf8 }
    };
    for (const MyStringPair& fixture : fixtures) {
        // wrap input in double-quotes, to get single string value
        std::ostringstream input_oss;
        input_oss << '"' << fixture.input << '"';
        const std::string& input = input_oss.str();

        JSONReader json(input.c_str(), input.size());

        const std::string expected = fixture.output? fixture.output: fixture.input;

        BOOST_REQUIRE_EQUAL(json.next(), JSON_STRING);
        BOOST_REQUIRE_EQUAL(json.get_string(), expected);
        BOOST_REQUIRE_EQUAL(json.token().to_string(), input);
        BOOST_REQUIRE_EQUAL(json.token().to_string(true), std::string(fixture.input));
        BOOST_REQUIRE_THROW(json.get_int(), std::runtime_error);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }

    //// TODO: unicode surrogate-pair escapes \uFFFF\uFFFF
    //{
    //// U+1000 (first non-BMP codepoint)
    //const char* input = "\"\\uD800\\uDC00\"";
    //JSONStringHandler handler;
    //BOOST_CHECK_THROW(json_parse(input, strlen(input), handler), std::runtime_error);
    //}
}

BOOST_AUTO_TEST_CASE(test_arrays) {
    {
    const char json_text[] = "[]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }

    {
    const char json_text[] = "[1]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    }

    {
    const char json_text[] = "[1,null]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NULL);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    }

    {
    const char json_text[] = "[[]]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    }

    {
    const char json_text[] = "[[],[]]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    }

    {
    const char json_text[] = "[[],[null],[true,  false] , [ 1 ,[2,[3] ]  ]   ]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN); // []
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN); // [null]
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NULL);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN); // [true, false]
    BOOST_REQUIRE_EQUAL(json.next(), JSON_TRUE);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_FALSE);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN); // [ 1, ...]
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN); // [[2,...]]
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN); // [[[3]]]
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    }
}

BOOST_AUTO_TEST_CASE(test_objects) {
    {
    const char json_text[] = "{}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }

    {
    const char json_text[] = "{\"one\":1}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    }

    {
    const char json_text[] = "{\"\":{}}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    }
}

BOOST_AUTO_TEST_CASE(test_next) {
    {
        const char json_text[] = "{}";
        JSONReader json(json_text, sizeof(json_text) - 1);

        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }

    {
        const char json_text[] = "{}\n"; // used to throw...
        JSONReader json(json_text, sizeof(json_text) - 1);

        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }
}

BOOST_AUTO_TEST_CASE(test_current) {
    {
        const char json_text[] = "{}";
        JSONReader json(json_text, sizeof(json_text) - 1);

        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_END);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_END);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_END);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_END);
    }

    {
    const char json_text[] = "{\"a\":[1,2,3],\"b\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_BEGIN);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_END);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_ARRAY_END);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_END);
    }
}

BOOST_AUTO_TEST_CASE(test_depth) {
    {
    const char json_text[] = "[[],[]]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.depth(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.depth(), size_t(1));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.depth(), size_t(2));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.depth(), size_t(1));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.depth(), size_t(2));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.depth(), size_t(1));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.depth(), size_t(0));
    }
}

BOOST_AUTO_TEST_CASE(test_count) {
    {
    const char json_text[] = "[[],[]]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(2));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));
    }

    {
    const char json_text[] = "{\"\":{}}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));
    }

    {
    const char json_text[] = "{\"a\":[1,2,3],\"b\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(2));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(3));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(0));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(2));

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(1));
    }
}

struct MockColor {
    float r, g, b, a;

    MockColor() : r(0), g(0), b(0), a(1) {}
};
typedef std::map<int, MockColor> MockIndexedColorMap;

inline void json_write(JSONWriter& json, const MockColor& value) {
    json.push_object();
    json.key("r").number(value.r);
    json.key("g").number(value.g);
    json.key("b").number(value.b);
    json.key("a").number(value.a);
    json.pop_object();
}

inline bool operator==(const MockColor& lhs, const MockColor& rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline std::ostream& operator<<(std::ostream& os, const MockColor& color) {
    JSONWriter json(os);
    json_write(json, color);
    return os;
}

inline bool json_parse(JSONReader& json, MockColor& value) {
    OES_ASSERT(json.next() == JSON_OBJECT_BEGIN, return false;);

    int primes_seen = 1;
    JSONParsingEventType tt = json.next(); // token_type
    for (; tt != JSON_OBJECT_END && primes_seen <= 2*3*5*7; tt = json.next()) {
        OES_ASSERT(tt == JSON_OBJECT_KEY, return false;);
        JSONToken tok = json.token();
        OES_ASSERT(tok.size() == 3 && tok.is_quoted(), return false;);
        const char key = tok.beg_[1];

        tt = json.next();
        OES_ASSERT(tt == JSON_NUMBER, return false;);
        const float val = json.get_float();

        switch (key) {
        case 'r': value.r = val; primes_seen *= 2; break;
        case 'g': value.g = val; primes_seen *= 3; break;
        case 'b': value.b = val; primes_seen *= 5; break;
        case 'a': value.a = val; primes_seen *= 7; break;
        default:  return false;
        }
    }
    OES_ASSERT(primes_seen == 2*3*5*7, return false;);
    return tt == JSON_OBJECT_END;
}

struct MockColorLookup {
    MockIndexedColorMap colorMap;
};

inline void json_write(JSONWriter& json, const MockColorLookup& value) {
    json.push_object();
    for (const MockIndexedColorMap::value_type& e : value.colorMap) {
        std::ostringstream oss;
        oss << e.first;
        json.key(oss.str()).value(e.second);
    }
    json.pop_object();
}

inline bool operator==(const MockColorLookup& lhs, const MockColorLookup& rhs) {
    return lhs.colorMap == rhs.colorMap;
}

inline std::ostream& operator<<(std::ostream& os, const MockColorLookup& colorLookup) {
    JSONWriter json(os);
    json_write(json, colorLookup);
    return os;
}

inline bool json_parse(JSONReader& json, MockColorLookup& value) {
    OES_ASSERT(json.next() == JSON_OBJECT_BEGIN, return false;);

    JSONParsingEventType tt = json.next(); // token_type
    for (; tt != JSON_OBJECT_END; tt = json.next()) {
        OES_ASSERT(tt == JSON_OBJECT_KEY, return false;);
        const std::string& key = json.get_string();

        MockColor color;
        OES_ASSERT(json_parse(json, color), return false;);

        int index = -1;
        std::istringstream iss(key);
        iss >> index;
        OES_ASSERT(!iss.fail() && iss.eof(), return false;);
        const bool color_inserted = value.colorMap.insert(
            std::make_pair(index, color)
        ).second;
        OES_ASSERT(color_inserted, return false;);
    }
    return tt == JSON_OBJECT_END;
}

BOOST_AUTO_TEST_CASE(test_composition) {
    MockColor red; red.r = 1;
    MockColor green; green.g = 1;
    MockColor blue; blue.b = 1;
    MockColor white; white.r = white.g = white.b = 1;

    // test Color round-trip
    MockColor colors[] = { red, green, blue, white };
    for (const MockColor& color : colors) {
        const std::string& color_json = to_json_text(color);
        MockColor read_color;
        JSONReader json(color_json.c_str(), color_json.length());
        BOOST_REQUIRE(json_parse(json, read_color));
        BOOST_REQUIRE_EQUAL(color, read_color);
    }
    const char* bad_colors[] = {
        "{"      "\"g\":0,\"b\":0,\"a\":1}", // missing key
        "{\"r\":0,\"r\":1,\"b\":0,\"a\":1}", // duplicate key
        "[{\"r\":0,\"g\":0,\"b\":1,\"a\":1}]", // outer array
        "{\"r\":1,\"g\":1,\"b\":1,\"a\":null}" // null not a number
    };
    for (const char* bad_color : bad_colors) {
        MockColor read_color;
        JSONReader json(bad_color, strlen(bad_color));
        ScopedExpectedAssert one_assert;
        BOOST_REQUIRE(!json_parse(json, read_color));
    }

    // test ColorLookup round-trip
    MockColorLookup empty;
    const std::string& empty_json = to_json_text(empty);
    BOOST_REQUIRE_EQUAL(std::string("{}"), empty_json);
    {
        MockColorLookup read_lookup;
        JSONReader json(empty_json.c_str(), empty_json.length());
        BOOST_REQUIRE(json_parse(json, read_lookup));
        BOOST_REQUIRE_EQUAL(empty, read_lookup);
    }

    MockColorLookup sample;
    sample.colorMap.insert(std::make_pair(42, white));
    sample.colorMap.insert(std::make_pair(4, green));
    sample.colorMap.insert(std::make_pair(3, red));
    sample.colorMap.insert(std::make_pair(40, blue));
    const std::string& sample_json = to_json_text(sample);
    const char* expected_sample = // std::map, so ordered in keys
        "{"
            "\"3\":{\"r\":1,\"g\":0,\"b\":0,\"a\":1},"
            "\"4\":{\"r\":0,\"g\":1,\"b\":0,\"a\":1},"
            "\"40\":{\"r\":0,\"g\":0,\"b\":1,\"a\":1},"
            "\"42\":{\"r\":1,\"g\":1,\"b\":1,\"a\":1}"
        "}"
    ;
    BOOST_REQUIRE_EQUAL(std::string(expected_sample), sample_json);
    {
        MockColorLookup read_lookup;
        JSONReader json(sample_json.c_str(), sample_json.length());
        BOOST_REQUIRE(json_parse(json, read_lookup));
        BOOST_REQUIRE_EQUAL(sample, read_lookup);
    }
}

bool exhaust(JSONReader& json) {
    int i = 0; // avoid infinite loop
    while (json.next() && ++i < 1000);
    return i < 1000;
}

BOOST_AUTO_TEST_CASE(test_max_depth) {
    JSONParserOptions options;
    options.max_depth_ = 1;

    { // nested objects
    const char json_text[] = "{\"\":{}}";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    JSONParserOptions options2(options);
    options2.max_depth_ = 2;
    JSONReader json2(json_text, sizeof(json_text) - 1, options2);
    BOOST_REQUIRE(exhaust(json2));
    }

    { // nested arrays
    const char json_text[] = "[1,[]]";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    JSONParserOptions options2(options);
    options2.max_depth_ = 2;
    JSONReader json2(json_text, sizeof(json_text) - 1, options2);
    BOOST_REQUIRE(exhaust(json2));
    }

    { // nested array, in object
    const char json_text[] = "{\"\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    JSONParserOptions options2(options);
    options2.max_depth_ = 2;
    JSONReader json2(json_text, sizeof(json_text) - 1, options2);
    BOOST_REQUIRE(exhaust(json2));
    }

    { // nested object, in array
    const char json_text[] = "[1,{}]";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    JSONParserOptions options2(options);
    options2.max_depth_ = 2;
    JSONReader json2(json_text, sizeof(json_text) - 1, options2);
    BOOST_REQUIRE(exhaust(json2));
    }
}

BOOST_AUTO_TEST_CASE(test_max_count) {
    JSONParserOptions options;
    options.max_array_size_ = 3;
    options.max_object_size_ = 2;

    { // object KVPs
    const char json_text[] = "{\"1\":null, \"2\":null, \"3\":null}";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    JSONParserOptions options2(options);
    options2.max_object_size_ = 3;
    JSONReader json2(json_text, sizeof(json_text) - 1, options2);
    BOOST_REQUIRE(exhaust(json2));
    }

    { // array entries
    const char json_text[] = "[1, 2, 3, 4]";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    JSONParserOptions options2(options);
    options2.max_array_size_ = 4;
    JSONReader json2(json_text, sizeof(json_text) - 1, options2);
    BOOST_REQUIRE(exhaust(json2));
    }
}

BOOST_AUTO_TEST_CASE(test_max_string_byte_size) {
    JSONParserOptions options;
    options.max_objkey_byte_size_ = 3;
    options.max_string_byte_size_ = 4;

    { // object key max byte length
    const char json_text[] = "{\"22\":null, \"333\":null, \"4444\":null}";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    options.max_objkey_byte_size_ = 4;
    JSONReader json2(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE(exhaust(json2));
    }

    { // object key max byte length
    const char json_text[] = "{\"22\":\"two\", \"333\":\"three\", \"4444\":\"four\"}";
    JSONReader json(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE_THROW(exhaust(json), std::runtime_error);

    options.max_string_byte_size_ = 5;
    JSONReader json2(json_text, sizeof(json_text) - 1, options);
    BOOST_REQUIRE(exhaust(json2));
    }
}

BOOST_AUTO_TEST_CASE(test_skip) {
    {
        const char json_text[] = "{}";
        JSONReader json(json_text, sizeof(json_text) - 1);

        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.skip_current(), size_t(2));
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_END);
    }

    {
        const char json_text[] = "{}";
        JSONReader json(json_text, sizeof(json_text) - 1);

        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.current(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
        // Not OK to skip on a on-beging
        BOOST_REQUIRE_THROW(json.skip_current(), std::runtime_error);
    }

    {
        const char json_text[] = "{}";
        JSONReader json(json_text, sizeof(json_text) - 1);

        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
        BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
        // OK at the end
        BOOST_REQUIRE_EQUAL(json.skip_current(), size_t(0));
    }

    {
    const char json_text[] = "{\"a\":[1,2,3],\"b\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    // skip the whole thing
    BOOST_REQUIRE_EQUAL(json.skip_next(), size_t(11));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_END);
    }

    {
    const char json_text[] = "{\"a\":[1,2,3],\"b\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);

    BOOST_REQUIRE_EQUAL(json.skip_next(), size_t(5));
    //BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    //BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    //BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    //BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
    //BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
    }
}

BOOST_AUTO_TEST_CASE(test_peek) {
    const char* inputs[] = {
        "{}",
        "[]",
        "[null,true,false,1]",
        "{\"1\":1}",
        "{\"1\":1,\"2\": true}",
        "{\"1\":1,\"2\":  [   true   ]}",
        "{\"1\":1,\"2\":{\"3\":  [   true, false   ]}  , \"4\":null}",
        "[1,[2,[3,[4,[5,[6,[7,[8,[9,[10,[11,[12,[13,[14,[15,[16,[17,[]]]]]]]]]]]]]]]]]]",
    };

    for (auto input : inputs) {
        JSONReader json(input, strlen(input));
        for (int i = 0; i < 1000; ++i) { // avoid infinite loop, just in case
            JSONParsingEventType got_peek = json.peek();
            JSONParsingEventType got_next = json.next();
            JSONParsingEventType got_curr = json.current();

            BOOST_REQUIRE_EQUAL(got_peek, got_next);
            BOOST_REQUIRE_EQUAL(got_curr, got_next);
            BOOST_REQUIRE_EQUAL(got_curr, got_peek);

            if (got_next == JSON_END) {
                break;
            }
        }
    }
}

struct AValue {
    std::vector<int> ints_;
    bool ignore_non_ints = false;
};

bool json_parse(JSONReader& json, AValue& value) {
    if (json.next() != JSON_ARRAY_BEGIN) {
        return false;
    }
    while (json.next() != JSON_ARRAY_END) {
        if (json.current() == JSON_NUMBER && json.is_integral()) {
            value.ints_.push_back(json.get_int());
        } else {
            if (!value.ignore_non_ints) {
                return false;
            }
            json.skip_current();
        }
    }
    return json.current() == JSON_ARRAY_END;
}

BOOST_AUTO_TEST_CASE(test_parse_array) {
    const char json_text[] = "{\"a\":[1,2,3],\"b\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.is_key("a"), true);
    BOOST_REQUIRE_EQUAL(json.is_key("a", 1), true);
    BOOST_REQUIRE_EQUAL(json.get_key() == "a", true);
    AValue value_a;
    const bool try_parse_array_ok = json.try_parse_array(
        [&value_a, &json]() {
            BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
            value_a.ints_.push_back(json.get_int());
            return true;
        }
    );
    BOOST_REQUIRE(try_parse_array_ok);
    std::vector<int> want_a{ 1, 2, 3 };
    BOOST_REQUIRE(value_a.ints_ == want_a);

    // with empty array
    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.is_key("b"), true);
    BOOST_REQUIRE_EQUAL(json.is_key("b", 1), true);
    BOOST_REQUIRE_EQUAL(json.get_key() == "b", true);
    AValue value_b;
    const bool try_parse_array_ok2 = json.try_parse_array(
        [&value_b, &json]() {
            BOOST_REQUIRE_EQUAL(json.next(), JSON_NUMBER);
            value_b.ints_.push_back(json.get_int());
            return true;
        }
    );
    BOOST_REQUIRE(try_parse_array_ok2);
    std::vector<int> want_b;
    BOOST_REQUIRE(value_b.ints_ == want_b);
}

BOOST_AUTO_TEST_CASE(test_savepoint) {
    const char json_text[] = "{\"a\":[1,null,3],\"b\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.is_key("a"), true); // for code coverage :)
    BOOST_REQUIRE_EQUAL(json.is_key("a", 1), true);
    BOOST_REQUIRE_EQUAL(json.get_key() == "a", true);

    // delegate parsing a's value to someone else.
    // First it fails, but we setup a savepoint...
    AValue a;
    JSONReader::SavePoint a_array(json.savepoint());
    BOOST_REQUIRE_EQUAL(size_t(0), a.ints_.size());
    BOOST_REQUIRE_EQUAL(false, json_parse(json, a));
    BOOST_REQUIRE_EQUAL(size_t(1), a.ints_.size()); // partial read!
    a.ints_.clear();

    // So we can "rollback" to the savepoint and try again.
    // This time it will work, and we'll have our 2 ints (minus the null)
    json.rollback_to(a_array);
    a.ignore_non_ints = true;
    BOOST_REQUIRE_EQUAL(size_t(0), a.ints_.size());
    BOOST_REQUIRE_EQUAL(true, json_parse(json, a));
    BOOST_REQUIRE_EQUAL(size_t(2), a.ints_.size());

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);
}

BOOST_AUTO_TEST_CASE(test_nested_reader_scope) {
    {
    const char json_text[] = "{\"a\":[1,null,3],\"b\":[]}";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_BEGIN);

    // Cannot scope() when not expected a JSON *value*
    BOOST_REQUIRE_THROW(json.scope(), std::runtime_error);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.is_key("a"), true); // for code coverage :)
    BOOST_REQUIRE_EQUAL(json.is_key("a", 1), true);
    BOOST_REQUIRE_EQUAL(json.get_key() == "a", true);

    // Call a "rogue" nested reader that tries to read more than it should
    JSONReader::Scope scope = json.scope();
    exhaust(*scope);
    scope.reset();

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_KEY);
    BOOST_REQUIRE_EQUAL(json.is_key("b", 1), true);

    exhaust(*json.scope()); // inline use OK

    BOOST_REQUIRE_EQUAL(json.next(), JSON_OBJECT_END);

    // scope() at the end not OK either
    BOOST_REQUIRE_THROW(json.scope(), std::runtime_error);
    }

    {
    const char json_text[] = "[\"a\", [1,null,3], {\"b\":[]}, null, true, false, {}, []]";
    JSONReader json(json_text, sizeof(json_text) - 1);

    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_BEGIN);

    int count = 0;
    while (json.peek() != JSON_ARRAY_END) {
        BOOST_REQUIRE(exhaust(*json.scope()));
        ++count;
        BOOST_REQUIRE_EQUAL(json.count(), size_t(count));
    }
    BOOST_REQUIRE_EQUAL(count, 8);
    BOOST_REQUIRE_EQUAL(json.count(), size_t(count));
    BOOST_REQUIRE_EQUAL(json.next(), JSON_ARRAY_END);
    }
}

bool readFunction(JSONReader& json) {
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_BEGIN, json.current());
    // Name
    JSONParsingEventType type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_KEY, type);
    BOOST_REQUIRE_EQUAL("Name", json.get_string());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_STRING, type);
    BOOST_REQUIRE_EQUAL("3D Viewer", json.get_string());
    // Features
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_KEY, type);
    BOOST_REQUIRE_EQUAL("Features", json.get_string());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_ARRAY_BEGIN, type);
    BOOST_REQUIRE_EQUAL(0, json.count());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_STRING, type);
    BOOST_REQUIRE_EQUAL("A,B", json.get_string()); //(SHARED-1531)
    BOOST_REQUIRE_EQUAL(1, json.count());
    type = json.next();
    BOOST_REQUIRE(JSON_STRING == type);
    BOOST_REQUIRE_EQUAL("A+C", json.get_string());
    BOOST_REQUIRE_EQUAL(2, json.count());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_ARRAY_END, type);

    // finish reading the function token
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_END, type);
    return true;
}

bool readPlugin(JSONReader& json) {
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_BEGIN,json.current());
    // Name
    JSONParsingEventType type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_KEY, type);
    BOOST_REQUIRE_EQUAL("Name", json.get_string());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_STRING, type);
    BOOST_REQUIRE_EQUAL("Product", json.get_string());
    // Library
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_KEY, type);
    BOOST_REQUIRE_EQUAL("Library", json.get_string());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_STRING, type);
    BOOST_REQUIRE_EQUAL("Product_gui", json.get_string());
    // Functions
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_KEY, type);
    BOOST_REQUIRE_EQUAL("Functions", json.get_string());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_ARRAY_BEGIN, type);
    BOOST_REQUIRE_EQUAL(0, json.count());
    size_t depth = json.depth();
    type = json.next();
    while (type != JSON_ARRAY_END) {
        // read the array of functions
        readFunction(json);
        BOOST_REQUIRE_EQUAL(1, json.count()); // only one function in the array
        BOOST_REQUIRE_EQUAL(depth, json.depth());
        type = json.next();
    }
    // Dependencies
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_KEY, type);
    BOOST_REQUIRE_EQUAL("Dependencies", json.get_string());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_ARRAY_BEGIN, type);
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_ARRAY_END, type); // no dependencies

    // finish reading the plugin token
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_END, type);
    return true;
}

// simulate reading a Plugin.def file
BOOST_AUTO_TEST_CASE(test_nested_structure) {
    const char data[] = "{\"plugins\":["
     "{"
        "\"Name\": \"Product\","
        "\"Library\": \"Product_gui\","
        "\"Functions\": ["
          "{"
            "\"Name\":\"3D Viewer\","
            "\"Features\":["
                "\"A,B\",\"A+C\"" // test comma in value (SHARED-1531)
            "]"
          "}"
        "],"
        "\"Dependencies\": []"
    "}"
    "]"
    "}";
    JSONReader json(data, sizeof(data) - 1);
    JSONParsingEventType type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_BEGIN, type);
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_KEY, type);
    BOOST_REQUIRE_EQUAL("plugins", json.get_string());
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_ARRAY_BEGIN, type);
    BOOST_REQUIRE_EQUAL(0, json.count());
    size_t depth = json.depth();
    type = json.next();
    while (type != JSON_ARRAY_END) {
        readPlugin(json);
        BOOST_REQUIRE_EQUAL(1, json.count()); // only one plugin in the array
        BOOST_REQUIRE_EQUAL(depth, json.depth());
        type = json.next();
    }
    type = json.next();
    BOOST_REQUIRE_EQUAL(JSON_OBJECT_END, type);
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
