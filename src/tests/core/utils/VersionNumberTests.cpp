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

#include <oes/core/utils/VersionNumber.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

/*****************************************************************************/
using namespace oes::core;


BOOST_AUTO_TEST_SUITE(VersionNumber_Tests)

BOOST_AUTO_TEST_CASE(test_VersionNumber_constructor) {
    {VersionNumber v1(String("12.0"));
    BOOST_REQUIRE_EQUAL(12, v1.getComponent(0));
    BOOST_REQUIRE_EQUAL(0, v1.getComponent(1));
    BOOST_REQUIRE_EQUAL(0, v1.getComponent(2));
    BOOST_REQUIRE_EQUAL(0, v1.getComponent(3));
    BOOST_REQUIRE_EQUAL(VersionNumber::release, v1.getStatus()); }
    {VersionNumber v1(String("12.2.1"));
    BOOST_REQUIRE_EQUAL(12, v1.getComponent(0));
    BOOST_REQUIRE_EQUAL(2, v1.getComponent(1));
    BOOST_REQUIRE_EQUAL(1, v1.getComponent(2));
    BOOST_REQUIRE_EQUAL(0, v1.getComponent(3));
    BOOST_REQUIRE_EQUAL(VersionNumber::release, v1.getStatus()); }
    {VersionNumber v1(String("12.2a3"));
    BOOST_REQUIRE_EQUAL(12, v1.getComponent(0));
    BOOST_REQUIRE_EQUAL(2, v1.getComponent(1));
    BOOST_REQUIRE_EQUAL(0, v1.getComponent(2));
    BOOST_REQUIRE_EQUAL(3, v1.getComponent(3));
    BOOST_REQUIRE_EQUAL(VersionNumber::alpha, v1.getStatus()); }
    {VersionNumber v1(String("12.2.1a3"));
    BOOST_REQUIRE_EQUAL(12, v1.getComponent(0));
    BOOST_REQUIRE_EQUAL(2, v1.getComponent(1));
    BOOST_REQUIRE_EQUAL(1, v1.getComponent(2));
    BOOST_REQUIRE_EQUAL(3, v1.getComponent(3));
    BOOST_REQUIRE_EQUAL(VersionNumber::alpha, v1.getStatus()); }
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_status) {
    std::list < std::pair<VersionNumber, VersionNumber::VersionStatus> > all_tests;
    all_tests.push_back(std::make_pair(VersionNumber("12.2"), VersionNumber::release));
    all_tests.push_back(std::make_pair(VersionNumber("12.2.1"), VersionNumber::release));
    all_tests.push_back(std::make_pair(VersionNumber("12.2.1a3"), VersionNumber::alpha));
    all_tests.push_back(std::make_pair(VersionNumber("12.2.1b3"), VersionNumber::beta));
    all_tests.push_back(std::make_pair(VersionNumber("12.2.1c3"), VersionNumber::candidate));
    all_tests.push_back(std::make_pair(VersionNumber("12.2.1p3"), VersionNumber::patch));

    for (std::pair<VersionNumber, VersionNumber::VersionStatus> test : all_tests){
        const VersionNumber& version = test.first;
        const VersionNumber::VersionStatus& status = test.second;
        BOOST_REQUIRE_EQUAL(status, version.getStatus());
    }
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_equal) {
    {VersionNumber v1(String("1.2.0"));
    VersionNumber v2(String("1.2"));
    BOOST_REQUIRE_EQUAL(true, (v1 == v2));
    BOOST_REQUIRE_EQUAL(false, (v1 != v2));
    }
    {VersionNumber v1(String("1.2"));
    VersionNumber v2(String("1.2"));
    BOOST_REQUIRE_EQUAL(true, (v1 == v2));
    BOOST_REQUIRE_EQUAL(false, (v1 != v2));
    }
    {VersionNumber v1(String("2.2"));
    VersionNumber v2(String("1.2"));
    BOOST_REQUIRE_EQUAL(false, (v1 == v2));
    BOOST_REQUIRE_EQUAL(true, (v1 != v2));
    }
}
BOOST_AUTO_TEST_CASE(test_VersionNumber_greater_than) {
    std::list < std::pair<VersionNumber, VersionNumber> > all_tests;
    all_tests.push_back(std::make_pair(VersionNumber("1.2"), VersionNumber("1.2.1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2a1"), VersionNumber("1.2")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2"), VersionNumber("1.2p1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.1b1"), VersionNumber("1.2.1b2")));

    for (std::pair<VersionNumber, VersionNumber>& test : all_tests){
        const VersionNumber& v1 = test.first;
        const VersionNumber& v2 = test.second;
        // greater operator
        BOOST_REQUIRE_EQUAL(true, (v2 > v1));
        BOOST_REQUIRE_EQUAL(false, (v1 > v2));
        BOOST_REQUIRE_EQUAL(false, (v1 > v1));
        BOOST_REQUIRE_EQUAL(false, (v2 > v2));

        // greater or equal operator
        BOOST_REQUIRE_EQUAL(true, (v2 >= v1));
        BOOST_REQUIRE_EQUAL(false, (v1 >= v2));
        BOOST_REQUIRE_EQUAL(true, (v1 >= v1));
        BOOST_REQUIRE_EQUAL(true, (v2 >= v2));
    }
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_less_than) {
    std::list < std::pair<VersionNumber, VersionNumber> > all_tests;
    all_tests.push_back(std::make_pair(VersionNumber("1.2"), VersionNumber("1.2.1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2a1"), VersionNumber("1.2")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2"), VersionNumber("1.2p1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.1b1"), VersionNumber("1.2.1b2")));

    for (std::pair<VersionNumber, VersionNumber>& test : all_tests){
        const VersionNumber& v2 = test.first;
        const VersionNumber& v1 = test.second;
        // less operator
        BOOST_REQUIRE_EQUAL(true, (v2 < v1));
        BOOST_REQUIRE_EQUAL(false, (v1 < v2));
        BOOST_REQUIRE_EQUAL(false, (v1 < v1));
        BOOST_REQUIRE_EQUAL(false, (v2 < v2));

        // less or equal operator
        BOOST_REQUIRE_EQUAL(true, (v2 <= v1));
        BOOST_REQUIRE_EQUAL(false, (v1 <= v2));
        BOOST_REQUIRE_EQUAL(true, (v1 <= v1));
        BOOST_REQUIRE_EQUAL(true, (v2 <= v2));
    }
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_prettify) {
    std::list < std::pair<VersionNumber, String> > all_tests;
    all_tests.push_back(std::make_pair(VersionNumber("1.2"), String("1.2")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2a1"), String("1.2 Alpha 1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.0a1"), String("1.2 Alpha 1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2b2"), String("1.2 Beta 2")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.3c1"), String("1.2.3 Release Candidate 1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.3p4"), String("1.2.3 Patch 4")));

    for (std::pair<VersionNumber, String> test : all_tests){
        const VersionNumber& version = test.first;
        const String& string = test.second;
        BOOST_REQUIRE_EQUAL(true, (string == version.prettify()) );
    }
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_serialize) {
    std::list < std::pair<VersionNumber, String> > all_tests;
    all_tests.push_back(std::make_pair(VersionNumber("1.2"), String("1.2")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2a1"), String("1.2a1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.0a1"), String("1.2a1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2b2"), String("1.2b2")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.3c1"), String("1.2.3c1")));
    all_tests.push_back(std::make_pair(VersionNumber("1.2.3p4"), String("1.2.3p4")));

    for (std::pair<VersionNumber, String> test : all_tests){
        const VersionNumber& version = test.first;
        const String& string = test.second;
        BOOST_REQUIRE_EQUAL(true, (string == version.serialize()) );
        VersionNumber deserialized(version.serialize());
        BOOST_REQUIRE_EQUAL(true, (deserialized == version) );
    }
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_equal_operator) {
    VersionNumber v1(String("2.2"));
    VersionNumber v2(String("1.2"));
    VersionNumber v3;
    BOOST_REQUIRE_EQUAL(true, (v1 != v3));
    BOOST_REQUIRE_EQUAL(true, (v2 != v3));
    v3 = v2;
    BOOST_REQUIRE_EQUAL(false, (v1 == v3));
    BOOST_REQUIRE_EQUAL(true, (v2 == v3));
    v3 = v1;
    BOOST_REQUIRE_EQUAL(true, (v1 == v3));
    BOOST_REQUIRE_EQUAL(false, (v2 == v3));
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_assignment_operator) {
    String v1_str("2.2");
    VersionNumber v1;
    VersionNumber v1_actual(v1_str);
    BOOST_REQUIRE_EQUAL(true, (v1 != v1_actual));
    v1 << v1_str;
    BOOST_REQUIRE_EQUAL(true, (v1 == v1_actual));
    String v2_str("1.2");
    VersionNumber v2;
    VersionNumber v2_actual(v2_str);
    BOOST_REQUIRE_EQUAL(true, (v2 != v2_actual));
    v2_str >> v2;
    BOOST_REQUIRE_EQUAL(true, (v2 == v2_actual));
}

BOOST_AUTO_TEST_CASE(test_VersionNumber_print_operator) {
    String v_str("1.2");
    VersionNumber v(v_str);
    String destination("aaaa");
    BOOST_REQUIRE_EQUAL(true, (v_str != destination));
    v >> destination;
    BOOST_REQUIRE_EQUAL(true, (v_str == destination));

    destination = "aaaa";
    BOOST_REQUIRE_EQUAL(true, (v_str != destination));
    destination << v;
    BOOST_REQUIRE_EQUAL(true, (v_str == destination));
    
}

BOOST_AUTO_TEST_SUITE_END()

/*****************************************************************************/

} // namespace
