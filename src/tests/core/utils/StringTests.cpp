// ============================================================================
// Copyright 2014-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/String.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

#include <oes/core/utils/Logger.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <algorithm>
#include <cstdlib>
#include <random>
#include <sstream>
#include <vector>

namespace {

using namespace oes::core;

BOOST_AUTO_TEST_SUITE(StringTests)

enum CmpRC { CmpEQ = 0, CmpLT, CmpGT };

CmpRC naturalCmpOld(const char* lhs, const char* rhs) {
    const int rc = StringUtils::naturalStringCompare(lhs, rhs);
    return (rc < 0)? CmpLT: (rc == 0)? CmpEQ: CmpGT;
}

BOOST_AUTO_TEST_CASE(testNaturalStringCompare) {
    // test w/o embedded number (i.e. degenerates to lexicographic compare)
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("abc", "abc"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("abc", "abz"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("zabc", "abc"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("Abc", "abc"));

    // test number as suffix
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a", "a1"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a1", "a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("a1", "a1"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a1", "a10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a1", "a2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2", "a10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a01", "a2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2", "a010"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("a010", "a010"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a010", "a2"));

    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("a010", "a010"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a010", "a0010"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a0010", "a00010"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a0010", "a010"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a00010", "a0010"));

    // test number as prefix
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("1a", "2a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("2a", "2a"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("2a", "1a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("1a", "10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("2a", "10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("01a", "2a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("01a", "01a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("2a", "010a"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("010a", "2a"));

    // test number at the middle of the string
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a1a", "a2a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a1a", "a10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2a", "a10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a01a", "a2a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2a", "a010a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("a2a", "a2a"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a010a", "a2a"));

    // test compare when 2 numbers exists
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a1a", "a1a1"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a10a1", "a10a2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a10a2", "a10a10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2a2", "a2a010"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a010a01", "a010a2"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("a010a2", "a010a2"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a010a2", "a010a01"));

    // test compare when punctuation sign in strings
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a,1a", "a1a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a10a,1", "a10a,2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a,10,a,2", "a,10,a,10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a,2,a2", "a,2,a010,"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a010a01,", "a010a2,"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("a010a2,", "a010a2,"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a010a2,", "a010a01,"));

    // test max ulong overflow
#define UL_OF9 "99999999999999999999999999999999999999999999999999999999999999999999999999999999999999999999"
#define UL_OF8 "88888888888888888888888888888888888888888888888888888888888888888888888888888888888888888888"

    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a", "a" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a" UL_OF9, "a2" UL_OF9 ));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2", "a2" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2" UL_OF9, "a10" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a010", "a010" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a2" UL_OF9, "a010" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a" UL_OF9 "a", "a" UL_OF9 "b"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a" UL_OF9 "b", "a" UL_OF9 "a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpOld("a" UL_OF9 "b", "a" UL_OF9 "b"));

    /*
    ** Old code fails this test. New code OK (see below).
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpOld("a" UL_OF9, "a" UL_OF8));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpOld("a" UL_OF8, "a" UL_OF9));
    */
}

CmpRC naturalCmpNew(const char* lhs, const char* rhs) {
    const int rc = StringUtils::naturalStringCompare(lhs, strlen(lhs), rhs, strlen(rhs));
    return (rc < 0)? CmpLT: (rc == 0)? CmpEQ: CmpGT;
}

BOOST_AUTO_TEST_CASE(testNaturalStringCompareNew) {
    // test w/o embedded number (i.e. degenerates to lexicographic compare)
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("abc", "abc"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("abc", "abz"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("zabc", "abc"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("Abc", "abc"));

    // test number as suffix
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a", "a1"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a1", "a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("a1", "a1"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a1", "a10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a1", "a2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2", "a10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a01", "a2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2", "a010"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("a010", "a010"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a010", "a2"));

    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("a010", "a010"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a010", "a0010"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a0010", "a00010"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a0010", "a010"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a00010", "a0010"));

    // test number as prefix
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("1a", "2a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("2a", "2a"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("2a", "1a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("1a", "10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("2a", "10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("01a", "2a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("01a", "01a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("2a", "010a"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("010a", "2a"));

    // test number at the middle of the string
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a1a", "a2a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a1a", "a10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2a", "a10a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a01a", "a2a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2a", "a010a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("a2a", "a2a"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a010a", "a2a"));

    // test compare when 2 numbers exists
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a1a", "a1a1"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a10a1", "a10a2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a10a2", "a10a10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2a2", "a2a010"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a010a01", "a010a2"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("a010a2", "a010a2"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a010a2", "a010a01"));

    // test compare when punctuation sign in strings
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a,1a", "a1a"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a10a,1", "a10a,2"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a,10,a,2", "a,10,a,10"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a,2,a2", "a,2,a010,"));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a010a01,", "a010a2,"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("a010a2,", "a010a2,"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a010a2,", "a010a01,"));

    // test max ulong overflow
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a", "a" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a" UL_OF9, "a2" UL_OF9 ));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2", "a2" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2" UL_OF9, "a10" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a010", "a010" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a2" UL_OF9, "a010" UL_OF9));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a" UL_OF9 "a", "a" UL_OF9 "b"));
    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a" UL_OF9 "b", "a" UL_OF9 "a"));
    BOOST_CHECK_EQUAL(CmpEQ, naturalCmpNew("a" UL_OF9 "b", "a" UL_OF9 "b"));

    BOOST_CHECK_EQUAL(CmpGT, naturalCmpNew("a" UL_OF9, "a" UL_OF8));
    BOOST_CHECK_EQUAL(CmpLT, naturalCmpNew("a" UL_OF8, "a" UL_OF9));
}

struct NatCmpPredOld {
    bool operator()(const char* lhs, const char* rhs) const {
        return StringUtils::naturalStringCompare(lhs, rhs) < 0;
    }
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return StringUtils::naturalStringCompare(lhs.c_str(), rhs.c_str()) < 0;
    }
};

struct NatCmpPredNew {
    bool operator()(const char* lhs, const char* rhs) const {
        return StringUtils::naturalStringCompare(lhs, strlen(lhs), rhs, strlen(rhs)) < 0;
    }
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return StringUtils::naturalStringCompare(lhs, rhs) < 0;
    }
};

BOOST_AUTO_TEST_CASE(benchNaturalStringCompare) {
    using namespace oes::core;
    OES_TEST_CHECK_PERF_ENABLE();

    const int count = 100*1000;
    std::vector<std::string> strings1;
    strings1.reserve(count);
    for (int i = 0; i < count; ++i) {
        std::ostringstream oss;
        oss << "foo" << i << "bar";
        strings1.push_back(oss.str());
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(strings1.begin(), strings1.end(), g);

    std::vector<const char*> cstrings1;
    cstrings1.reserve(count);
    for (int i = 0; i < count; ++i) {
        cstrings1.push_back(strings1[i].c_str());
    }

    {
    std::vector<std::string> strings(strings1);
    oes::testing::ScopedTimer timer("sort std::string w/Old", true);
    std::sort(strings.begin(), strings.end(), NatCmpPredOld());
    }

    {
    std::vector<std::string> strings(strings1);
    oes::testing::ScopedTimer timer("sort std::string w/New", true);
    std::sort(strings.begin(), strings.end(), NatCmpPredNew());
    }

    {
    std::vector<const char*> cstrings(cstrings1);
    oes::testing::ScopedTimer timer("sort const char  w/Old", true);
    std::sort(cstrings.begin(), cstrings.end(), NatCmpPredOld());
    }

    {
    std::vector<const char*> cstrings(cstrings1);
    oes::testing::ScopedTimer timer("sort const char  w/New", true);
    std::sort(cstrings.begin(), cstrings.end(), NatCmpPredNew());
    }
}

BOOST_AUTO_TEST_CASE(test_split) {
    StringList list;
    // simple
    StringUtils::split(list, "a,b,a", ",", false);
    BOOST_REQUIRE_EQUAL(3, (int)list.size());
    BOOST_REQUIRE_EQUAL("a", list[0]);
    BOOST_REQUIRE_EQUAL("b", list[1]);
    BOOST_REQUIRE_EQUAL("a", list[2]);
    list.clear();
    // with empty item
    StringUtils::split(list, "a,,a", ",", false);
    BOOST_REQUIRE_EQUAL(3, (int)list.size());
    BOOST_REQUIRE_EQUAL("a", list[0]);
    BOOST_REQUIRE_EQUAL("", list[1]);
    BOOST_REQUIRE_EQUAL("a", list[2]);
    list.clear();
    // with empty item and compress
    StringUtils::split(list, "a,,a", ",", true);
    BOOST_REQUIRE_EQUAL(2, (int)list.size());
    BOOST_REQUIRE_EQUAL("a", list[0]);
    BOOST_REQUIRE_EQUAL("a", list[1]);
    list.clear();
    // empty list
    StringUtils::split(list, "", ",", false);
    BOOST_REQUIRE_EQUAL(0, (int)list.size());
    list.clear();
    // list of empty strings
    StringUtils::split(list, ",,", ",", false);
    BOOST_REQUIRE_EQUAL(3, (int)list.size());
    BOOST_REQUIRE_EQUAL("", list[0]);
    BOOST_REQUIRE_EQUAL("", list[1]);
    BOOST_REQUIRE_EQUAL("", list[2]);
    list.clear();
    // list of empty strings + compress
    StringUtils::split(list, ",,", ",", true);
    BOOST_REQUIRE_EQUAL(2, (int)list.size()); // BUG???
    list.clear();
}

BOOST_AUTO_TEST_CASE(test_toBool) {
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("1"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("on"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("ON"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("Yes"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("true"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("True"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("TRUE"));

    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("0"));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("ofF"));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("Off"));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("NO"));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("false"));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("False"));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("FALSE"));

    // with spaces around
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("true "));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool(" True"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool(" TRUE "));

    // invalid
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("an invalid long name"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("an invalid long name", true));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("boo"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("boo", true));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("boo"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("", true));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool(""));

    // invalid with spaces
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("  boo  ", true));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool("boo "));
    BOOST_REQUIRE_EQUAL(true, StringUtils::toBool("\t\n ", true));
    BOOST_REQUIRE_EQUAL(false, StringUtils::toBool(" \t\n"));
}

BOOST_AUTO_TEST_CASE(test_push_back_unique) {
    StringList list = {"a","b","c"}; 
    BOOST_REQUIRE_EQUAL(3, (int)list.size());

    // Test simple add of a string
    // ---------------------------
    const String strToPush="d";
    // Adding a new string
    BOOST_REQUIRE_EQUAL( true, StringUtils::push_back_unique(list, strToPush) );
    BOOST_REQUIRE_EQUAL( 4, (int)list.size() );
    BOOST_REQUIRE_EQUAL( strToPush, list.back());
    // Check sending existing strings
    BOOST_REQUIRE_EQUAL( false, StringUtils::push_back_unique(list, strToPush) );
    BOOST_REQUIRE_EQUAL( 4, (int)list.size() );
    BOOST_REQUIRE_EQUAL( false, StringUtils::push_back_unique(list, "b") );
    BOOST_REQUIRE_EQUAL( 4, (int)list.size() );
    BOOST_REQUIRE_EQUAL( strToPush, list.back());

    // Test simple add of a list of strings
    // -----------------------------------
    StringList listToPush = {"e", "f"};
    // Adding a non intersecting list
    StringUtils::push_back_unique(list, listToPush);
    BOOST_REQUIRE_EQUAL( 6, (int)list.size() );
    BOOST_REQUIRE_EQUAL( "f", list.back());

    // Adding a partially overlaping list
    listToPush = {"a", "g", "b"};
    StringUtils::push_back_unique(list, listToPush);
    BOOST_REQUIRE_EQUAL( 7, (int)list.size() );
    BOOST_REQUIRE_EQUAL( "g", list.back());

}

BOOST_AUTO_TEST_CASE(test_insert_unique) {
    StringList list = {"a","b","c"}; 
    BOOST_REQUIRE_EQUAL(3, (int)list.size());
    StringList::size_type insert_pos = 2;

    // Test simple insert of a string
    // ---------------------------
    const String strToInsert="d";
    // Adding a new string
    BOOST_REQUIRE_EQUAL( true, StringUtils::insert_unique(list, list.begin()+insert_pos, strToInsert) );
    BOOST_REQUIRE_EQUAL( 4, (int)list.size() );
    BOOST_REQUIRE_EQUAL( strToInsert, list[insert_pos]);
    // Sending existing strings
    BOOST_REQUIRE_EQUAL( false, StringUtils::insert_unique(list, list.begin()+insert_pos, strToInsert) );
    BOOST_REQUIRE_EQUAL( 4, (int)list.size() );
    BOOST_REQUIRE_EQUAL( false, StringUtils::insert_unique(list, list.begin()+insert_pos, "b") );
    BOOST_REQUIRE_EQUAL( 4, (int)list.size() );
    BOOST_REQUIRE_EQUAL( strToInsert, list[insert_pos]);

    // Test simple insert of a list of strings
    // -----------------------------------
    StringList listToInsert = {"e", "f"};
    // Inserting a non intersecting list
    StringUtils::split(listToInsert, "e,f", ",", false);
    StringUtils::insert_unique(list, list.begin()+insert_pos, listToInsert);
    BOOST_REQUIRE_EQUAL( 6, (int)list.size() );
    BOOST_REQUIRE_EQUAL( "e", list[insert_pos]);
    BOOST_REQUIRE_EQUAL( "f", list[insert_pos+1]);
    // Inserting a partially overlaping list
    listToInsert = {"a", "g", "b"};
    StringUtils::insert_unique(list, list.begin()+insert_pos, listToInsert);
    BOOST_REQUIRE_EQUAL( 7, (int)list.size() );
    BOOST_REQUIRE_EQUAL( "g", list[insert_pos]);
}

BOOST_AUTO_TEST_CASE(test_join) {
    const StringList list = {"a","b","c"};
    const String comma_sep_list = StringUtils::join(list,",");
    BOOST_REQUIRE_EQUAL( "a,b,c", comma_sep_list);

    const String word_sep_list = StringUtils::join(list,"word");
    BOOST_REQUIRE_EQUAL( "awordbwordc", word_sep_list);

    const StringList list_with_empty_string = {"a","","c"};
    String joined_list = StringUtils::join(list_with_empty_string,",");
    BOOST_REQUIRE_EQUAL( "a,,c", joined_list);
    joined_list = StringUtils::join(list_with_empty_string,",", true);
    BOOST_REQUIRE_EQUAL( "a,c", joined_list);

}

BOOST_AUTO_TEST_CASE(test_qualified_name) {
    BOOST_REQUIRE_EQUAL(true, StringUtils::isQualifiedName("::"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::isQualifiedName("aa::"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::isQualifiedName("aa::bb"));
    BOOST_REQUIRE_EQUAL(true, StringUtils::isQualifiedName("aa::bb::cc"));
    BOOST_REQUIRE_EQUAL(false, StringUtils::isQualifiedName("aa:"));

    // Simple test with a namespace and a regular name
    String ns = "NameSpace";
    String name = "Name";
    BOOST_REQUIRE_EQUAL(false, StringUtils::isQualifiedName(name));
    String qualified_name = StringUtils::qualifiedName(ns, name);
    BOOST_REQUIRE_EQUAL(true, StringUtils::isQualifiedName(qualified_name));
    BOOST_REQUIRE_EQUAL(ns, StringUtils::nameSpace(qualified_name));
    BOOST_REQUIRE_EQUAL(name, StringUtils::localName(qualified_name));
    String split_ns, split_name;
    StringUtils::splitName(qualified_name, split_ns, split_name);
    BOOST_REQUIRE_EQUAL(ns, split_ns);
    BOOST_REQUIRE_EQUAL(name, split_name);

    // Test with empty namespace
    ns = "";
    qualified_name = StringUtils::qualifiedName(ns, name);
    BOOST_REQUIRE_EQUAL(name, qualified_name);
    BOOST_REQUIRE_EQUAL(false, StringUtils::isQualifiedName(qualified_name)); // this is no longer a qualified name
    BOOST_REQUIRE_EQUAL(ns, StringUtils::nameSpace(qualified_name));
    BOOST_REQUIRE_EQUAL(name, StringUtils::localName(qualified_name));
    StringUtils::splitName(qualified_name, split_ns, split_name);
    BOOST_REQUIRE_EQUAL(ns, split_ns);
    BOOST_REQUIRE_EQUAL(name, split_name);

    // Test with empty namespace and a qualified name
    ns = "";
    name = "Qualified::Name";
    qualified_name = StringUtils::qualifiedName(ns, name);
    BOOST_REQUIRE_EQUAL(name, qualified_name);
    BOOST_REQUIRE_EQUAL(true, StringUtils::isQualifiedName(name));
    BOOST_REQUIRE_EQUAL(true, StringUtils::isQualifiedName(qualified_name));
    BOOST_REQUIRE_NE(ns, StringUtils::nameSpace(qualified_name));
    BOOST_REQUIRE_NE(name, StringUtils::localName(qualified_name));
    StringUtils::splitName(qualified_name, split_ns, split_name);
    BOOST_REQUIRE_EQUAL("Qualified", split_ns);
    BOOST_REQUIRE_EQUAL("Name", split_name);
}

BOOST_AUTO_TEST_CASE(test_abbreviate) {
    std::string testString = "This string is a test for the abbreviate function";

    // Test emtpy string
    BOOST_REQUIRE_EQUAL("", StringUtils::abbreviate(""));
    // Test typical usage
    BOOST_REQUIRE_EQUAL("This...tion", StringUtils::abbreviate(testString));
    // Test keepFront = 0
    BOOST_REQUIRE_EQUAL("...tion", StringUtils::abbreviate(testString, 0, 4));
    // Test keepBack = 0
    BOOST_REQUIRE_EQUAL("This...", StringUtils::abbreviate(testString, 4, 0));
    // Test keepFront = keepBack = 0
    BOOST_REQUIRE_EQUAL("...", StringUtils::abbreviate(testString, 0, 0));
    // Test keepFront > s.size()
    BOOST_REQUIRE_EQUAL(testString, StringUtils::abbreviate(testString, testString.size() + 1, 0));
    // Test keepBack > s.size()
    BOOST_REQUIRE_EQUAL(testString, StringUtils::abbreviate(testString, 0, testString.size() + 1));
    // Test keepBack < s.size(), keepFront < s.size(), but keepBack + keepFront > s.size()
    BOOST_REQUIRE_EQUAL(testString, StringUtils::abbreviate(testString, testString.size() - 1, testString.size() - 1));
    // Test custom replacement
    std::string customReplacement = "****";
    BOOST_REQUIRE_EQUAL("This" + customReplacement + "tion", StringUtils::abbreviate(testString, 4, 4, customReplacement));
}


BOOST_AUTO_TEST_SUITE_END()

} // namespace
