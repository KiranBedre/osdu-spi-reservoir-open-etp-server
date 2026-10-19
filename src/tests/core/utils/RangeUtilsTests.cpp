// ============================================================================
// Copyright 2015-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/RangeUtils.hpp>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <boost/unordered_set.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <vector>
#include <map>

using namespace std;
using namespace oes::core;

namespace {

BOOST_AUTO_TEST_SUITE(RangeUtilsTests)

BOOST_AUTO_TEST_CASE(test_range_of_multimap) {
    std::multimap<int, std::string> multilang{
        { 1, "one" },
        { 1, "uno" },
        { 2, "two" },
    };
    std::ostringstream oss;
    for (const auto& entry : range_of(multilang.equal_range(1))) {
        int key = entry.first;
        const std::string& val = entry.second;
        oss << key << " = " << val << ';';
    }
    BOOST_REQUIRE_EQUAL(std::string("1 = one;1 = uno;"), oss.str());
}

BOOST_AUTO_TEST_CASE(test_reverse_of_vector) {
    std::vector<int> v{ 1, 42, 3, 7 };

    int i = 0;
    const size_t len = v.size();
    for (auto value : reverse_of(v)) {
        BOOST_REQUIRE_EQUAL(value, v[len - ++i]);
    }

    i = 0;
    const std::vector<int> copy(v); // const
    for (auto value : reverse_of(copy)) {
        BOOST_REQUIRE_EQUAL(value, v[len - ++i]);
    }
}

BOOST_AUTO_TEST_CASE(test_reverse_of_map) {
    using Map = std::map<std::string, int>;
    Map m{ { "one", 1 }, { "three", 3 }, { "seven", 7 } };

    // assert forward-order" (alphabetical)
    int i = 0;
    const std::vector<std::string> expected_key{ "one", "seven", "three"};
    for (auto entry : m) {
        BOOST_REQUIRE_EQUAL(entry.first, expected_key[i++]);
    }

    i = 0;
    const size_t len = m.size();
    for (auto entry : reverse_of(m)) {
        BOOST_REQUIRE_EQUAL(entry.first, expected_key[len - ++i]);
    }

    i = 0;
    const Map copy(m); // const
    for (auto entry : reverse_of(copy)) {
        BOOST_REQUIRE_EQUAL(entry.first, expected_key[len - ++i]);
    }
}

BOOST_AUTO_TEST_CASE(test_equal_range_of_multimap) {
    using MMap = std::multimap<std::string, int>;
    MMap mm{ { "one", 1 }, { "one", 1 }, { "seven", 7 } };

    int count = 0;
    for (auto entry : equal_range_of(mm, "one")) {
        BOOST_REQUIRE_EQUAL(entry.first, std::string("one"));
        BOOST_REQUIRE_EQUAL(entry.second, 1);
        ++count;
    }
    BOOST_REQUIRE_EQUAL(count, 2);

    count = 0;
    for (auto entry : equal_range_of(mm, "seven")) {
        BOOST_REQUIRE_EQUAL(entry.first, std::string("seven"));
        BOOST_REQUIRE_EQUAL(entry.second, 7);
        ++count;
    }
    BOOST_REQUIRE_EQUAL(count, 1);

    const MMap copy(mm);
    count = 0;
    for (auto entry : equal_range_of(copy, "one")) {
        BOOST_REQUIRE_EQUAL(entry.first, std::string("one"));
        BOOST_REQUIRE_EQUAL(entry.second, 1);
        ++count;
    }
    BOOST_REQUIRE_EQUAL(count, 2);
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace

