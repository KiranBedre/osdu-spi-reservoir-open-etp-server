// ============================================================================
// Copyright 2016-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/testing/core/AssertTestTools.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::testing;

namespace {

BOOST_AUTO_TEST_SUITE(AssertTestToolsTests)

BOOST_AUTO_TEST_CASE(test_compareMultiLineText) {
    std::string diff;

    BOOST_CHECK_EQUAL(true, compareMultiLineText(
        "",
        "",
        diff
    ));
    BOOST_CHECK(diff.empty());

    BOOST_CHECK_EQUAL(true, compareMultiLineText(
        "a",
        "a",
        diff
    ));
    BOOST_CHECK(diff.empty());

    BOOST_CHECK_EQUAL(false, compareMultiLineText(
        "a",
        "ab",
        diff
    ));
    BOOST_CHECK_EQUAL(diff.c_str(),
        "\n" // All diffs start with this, for nice Boost.Test message
        "L <<a>>\n"
        "R >>ab<<\n"
    );

    BOOST_CHECK_EQUAL(true, compareMultiLineText(
        "\n",
        "\n",
        diff
    ));
    BOOST_CHECK(diff.empty());

    BOOST_CHECK_EQUAL(true, compareMultiLineText(
        "a\n",
        "a\n",
        diff
    ));
    BOOST_CHECK(diff.empty());

    // one is a prefix of the other. Used to not be detected
    BOOST_CHECK_EQUAL(false, compareMultiLineText(
        "a\n",
        "a\n"
        "b\n",
        diff
    ));
    // Actually returns this... Which is not what it should be IMHO
    // It all depends if the trailing \n is part of the line, or not.
    // i.e. is "\n" two empty lines, or a single one. FIXME
    //     a
    // L <<>>
    // R >>b<<
    // R ++++
    /*
    ** FIXME
    BOOST_CHECK_EQUAL(diff.c_str(),
        "\n" // All diffs start with this, for nice Boost.Test message
        "    a\n" // identical lines as-is, with whitespace prefix
        "R ++b++\n" // extra lines on the right
    );
    */

    BOOST_CHECK_EQUAL(false, compareMultiLineText(
        "\n",
        "",
        diff
    ));
    BOOST_CHECK_EQUAL(diff.c_str(),
        "\n" // All diffs start with this, for nice Boost.Test message
        "    \n" // here again, like above, dubious FIXME
        "L ++++\n"
    );

    BOOST_CHECK_EQUAL(false, compareMultiLineText(
        "abc",
        "abc ", // trailing (invisible) whitespace
        diff
    ));
    BOOST_CHECK_EQUAL(diff.c_str(),
        "\n" // All diffs start with this, for nice Boost.Test message
        "L <<abc>>\n"
        "R >>abc <<\n"
    );
}

BOOST_AUTO_TEST_CASE(test_compareMultiLineText_if) {
    std::string diff;
    BOOST_CHECK_EQUAL(false, compareMultiLineText(
        "abc",
        "ABC", // same, but upper-case
        diff
    ));
    BOOST_CHECK_EQUAL(diff.c_str(),
        "\n" // All diffs start with this, for nice Boost.Test message
        "L <<abc>>\n"
        "R >>ABC<<\n"
    );

    // same, but with a custom-compartor. Considered equal now.
    BOOST_CHECK_EQUAL(true, compareMultiLineText(
        "abc",
        "ABC", // same, but upper-case
        diff,
        [](size_t lineno, const std::string* lhs, const std::string* rhs) {
            BOOST_CHECK(lineno == 0); // 0-based, as expected
            return lhs && rhs && *lhs == boost::to_lower_copy(*rhs);
        }
    ));
    BOOST_CHECK(diff.empty());
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
