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

/*!
 * Tests PhysicalName class.
 */
#include <oes/postgresql/PhysicalName.h>


#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::postgresql::relational;
namespace {

BOOST_AUTO_TEST_SUITE(PhysicalNameTests)

/*****************************************************************************/

BOOST_AUTO_TEST_CASE(test_physical_name) {

    PhysicalName name1("MY_TABLE", IMPLICIT_UPPERCASE);
    PhysicalName name2("MY_TABLE", NONE_CASE_SENSITIVE);
    PhysicalName name3("my_TABLE", IMPLICIT_UPPERCASE);
    PhysicalName name4("my_TABLE", NONE_CASE_SENSITIVE);
    PhysicalName name5("a_table", NONE_CASE_SENSITIVE);
    PhysicalName name6("z_table", IMPLICIT_UPPERCASE);
    PhysicalName name7("mY_Table123", IMPLICIT_UPPERCASE);
    PhysicalName name8("_123a", IMPLICIT_UPPERCASE);
    PhysicalName name9("_123a", IMPLICIT_LOWERCASE);
    // test ==
    BOOST_CHECK(name1 == name1);
    BOOST_CHECK(name1 == name2);
    BOOST_CHECK(name2 == name1);
    BOOST_CHECK(name1 == name3);
    BOOST_CHECK(name3 == name1);
    BOOST_CHECK(name1 != name4);
    BOOST_CHECK(name2 == name2);
    BOOST_CHECK(name2 == name3);
    BOOST_CHECK(name3 == name2);
    BOOST_CHECK(name2 != name4);
    BOOST_CHECK(name3 == name3);
    BOOST_CHECK(name3 != name4);
    BOOST_CHECK(name4 == name4);
    // test <
    // this never < this
    BOOST_CHECK(!(name2 < name2));
    BOOST_CHECK(!(name3 < name3));
    // test uppercase vs case sensitive
    BOOST_CHECK(!(name3 < name2));
    // test a_table < my_table and > MY_TABLE (implicit or not)
    BOOST_CHECK(name1 < name5);
    BOOST_CHECK(name2 < name5);
    BOOST_CHECK(name3 < name5);
    BOOST_CHECK(name5 < name4);
    // test z_table always > my_table, and < MY_TABLE
    BOOST_CHECK(name1 < name6);
    BOOST_CHECK(name2 < name6);
    BOOST_CHECK(name3 < name6);
    BOOST_CHECK(name6 < name4);
    // test with name different size
    BOOST_CHECK(name1 < name7);
    BOOST_CHECK(name2 < name7);
    BOOST_CHECK(name3 < name7);
    BOOST_CHECK(name7 < name4);
    // test with mix of upper/lower case and special chars
    BOOST_CHECK(name8 < name9);
}

BOOST_AUTO_TEST_CASE(test_physical_name_lowercase) {

    PhysicalName name1("MY_TABLE", IMPLICIT_LOWERCASE);
    PhysicalName name2("my_table", NONE_CASE_SENSITIVE);
    PhysicalName name3("my_TABLE", IMPLICIT_LOWERCASE);
    PhysicalName name4("my_TABLE", NONE_CASE_SENSITIVE);
    PhysicalName name4_2("MY_table", NONE_CASE_SENSITIVE);
    PhysicalName name5("a_table", NONE_CASE_SENSITIVE);
    PhysicalName name5_2("A_TABLE", NONE_CASE_SENSITIVE);
    PhysicalName name6("Z_TABLE", IMPLICIT_LOWERCASE);
    PhysicalName name7("mY_Table123", IMPLICIT_LOWERCASE);
    PhysicalName name8("MY_TABLE", IMPLICIT_UPPERCASE);
    PhysicalName name9("", IMPLICIT_UPPERCASE);
    // test ==
    BOOST_CHECK(name1 == name1);
    BOOST_CHECK(name1 == name2);
    BOOST_CHECK(name1 == name3);
    BOOST_CHECK(name1 != name4);
    BOOST_CHECK(name2 == name2);
    BOOST_CHECK(name2 == name3);
    BOOST_CHECK(name2 != name4);
    BOOST_CHECK(name3 == name3);
    BOOST_CHECK(name3 != name4);
    BOOST_CHECK(name4 == name4);
    BOOST_CHECK(name1 != name4_2);
    BOOST_CHECK(name2 != name4_2);
    BOOST_CHECK(name3 != name4_2);
    // test <
    // this never < this
    BOOST_CHECK(!(name2 < name2));
    BOOST_CHECK(!(name3 < name3));
    // test lowercase vs case sensitive
    BOOST_CHECK(!(name3 < name2));
    BOOST_CHECK(!(name2 < name3));
    // test a_table < my_table (implicit or not) and > z_table 
    BOOST_CHECK(name5 < name1);
    BOOST_CHECK(name5 < name2);
    BOOST_CHECK(name5 < name3);
    BOOST_CHECK(name5 < name6);
    BOOST_CHECK(name5_2 < name1);
    BOOST_CHECK(name5_2 < name2);
    BOOST_CHECK(name5_2 < name6);
    BOOST_CHECK(name5_2 < name5);
    BOOST_CHECK(name5_2 < name4_2);
    // test z_table always > my_table, and < MY_TABLE
    BOOST_CHECK(name1 < name6);
    BOOST_CHECK(name2 < name6);
    BOOST_CHECK(name3 < name6);
    BOOST_CHECK(name4 < name6);
    // test with name different size
    BOOST_CHECK(name1 < name7);
    BOOST_CHECK(name2 < name7);
    BOOST_CHECK(name3 < name7);
    BOOST_CHECK(name4 < name7);
    // test with lower vs upper case string
    BOOST_CHECK(name8 < name1);
    // test empty
    BOOST_CHECK(name9 == name9);
    BOOST_CHECK(name9 < name8);
}

BOOST_AUTO_TEST_CASE(test_perf_physical_name_less_than) {

    OES_TEST_CHECK_PERF_ENABLE();

    const bool print_to_stdout = true;
    PhysicalName name1("my_Table", IMPLICIT_UPPERCASE);
    PhysicalName name2("my_tablE", NONE_CASE_SENSITIVE);
    {
    oes::testing::ScopedTimer timer("Less with 2 IMPLICIT_UPPERCASE PhysicalNames", print_to_stdout);
    for(int i = 0; i < 1000000; ++i) {
        name1 < name1;
    }
    }
    {
    oes::testing::ScopedTimer timer("Less with 2 NONE_CASE_SENSITIVE PhysicalNames", print_to_stdout);
    for(int i = 0; i < 1000000; ++i) {
        name2 < name2;
    }
    }
    {
    oes::testing::ScopedTimer timer("Less IMPLICIT_UPPERCASE vs NONE_CASE_SENSITIVE", print_to_stdout);
    for(int i = 0; i < 1000000; ++i) {
        name1 < name2;
    }
    }
    {
    oes::testing::ScopedTimer timer("Equal with 2 IMPLICIT_UPPERCASE PhysicalNames", print_to_stdout);
    for(int i = 0; i < 1000000; ++i) {
        name1 == name1;
    }
    }
    {
    oes::testing::ScopedTimer timer("Equal with 2 NONE_CASE_SENSITIVE PhysicalNames", print_to_stdout);
    for(int i = 0; i < 1000000; ++i) {
        name2 == name2;
    }
    }
    {
    oes::testing::ScopedTimer timer("Equal IMPLICIT_UPPERCASE vs NONE_CASE_SENSITIVE", print_to_stdout);
    for(int i = 0; i < 1000000; ++i) {
        name1 == name2;
    }
    }
}

/*****************************************************************************/

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
