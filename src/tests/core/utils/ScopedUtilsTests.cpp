// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/ScopedUtils.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <string>

using namespace oes::core;
namespace {
BOOST_AUTO_TEST_SUITE(ScopedUtilsTests)

/******************************************************************************/

BOOST_AUTO_TEST_CASE(test_scoped_value_simple_types) {
    bool bool_true = true;
    bool bool_false = false;
    
    char c1 = 'a';
    char c2 = 'Z';
    
    int i1 = 0;
    int i2 = 42;
    
    double d1 = 3.14;
    double d2 = 2.71;
    
    // create a local scope
    {
        ScopedValue<bool> scoped_bool_true(bool_true, false);
        ScopedValue<bool> scoped_bool_false(bool_false, true);
        
        ScopedValue<char> scoped_c1(c1, '0');
        ScopedValue<char> scoped_c2(c2, 'p');
        
        ScopedValue<int> scoped_i1(i1, 4729);
        ScopedValue<int> scoped_i2(i2, -25);
        
        ScopedValue<double> scoped_d1(d1, 0.15);
        ScopedValue<double> scoped_d2(d2, -273.15);
        
        BOOST_CHECK_EQUAL(bool_true, false);
        BOOST_CHECK_EQUAL(bool_false, true);
        
        BOOST_CHECK_EQUAL(c1, '0');
        BOOST_CHECK_EQUAL(c2, 'p');
        
        BOOST_CHECK_EQUAL(i1, 4729);
        BOOST_CHECK_EQUAL(i2, -25);

        BOOST_CHECK_EQUAL(d1, 0.15);
        BOOST_CHECK_EQUAL(d2, -273.15);
    }
    
    BOOST_CHECK_EQUAL(bool_true, true);
    BOOST_CHECK_EQUAL(bool_false, false);
    
    BOOST_CHECK_EQUAL(c1, 'a');
    BOOST_CHECK_EQUAL(c2, 'Z');
    
    BOOST_CHECK_EQUAL(i1, 0);
    BOOST_CHECK_EQUAL(i2, 42);

    BOOST_CHECK_EQUAL(d1, 3.14);
    BOOST_CHECK_EQUAL(d2, 2.71);
    
    
    // create a local scope
    {
        ScopedValue<bool> scoped_bool_true(bool_true, false, true);
        ScopedValue<bool> scoped_bool_false(bool_false, true, false);
        
        ScopedValue<char> scoped_c1(c1, '0', 'A');
        ScopedValue<char> scoped_c2(c2, 'p', 'z');
        
        ScopedValue<int> scoped_i1(i1, 4729, 13);
        ScopedValue<int> scoped_i2(i2, -25, 666);
        
        ScopedValue<double> scoped_d1(d1, 0.15, 9.81);
        ScopedValue<double> scoped_d2(d2, -273.15, 1.41);
        
        BOOST_CHECK_EQUAL(bool_true, false);
        BOOST_CHECK_EQUAL(bool_false, true);
        
        BOOST_CHECK_EQUAL(c1, '0');
        BOOST_CHECK_EQUAL(c2, 'p');
        
        BOOST_CHECK_EQUAL(i1, 4729);
        BOOST_CHECK_EQUAL(i2, -25);

        BOOST_CHECK_EQUAL(d1, 0.15);
        BOOST_CHECK_EQUAL(d2, -273.15);
    }
    
    BOOST_CHECK_EQUAL(bool_true, true);
    BOOST_CHECK_EQUAL(bool_false, false);
    
    BOOST_CHECK_EQUAL(c1, 'A');
    BOOST_CHECK_EQUAL(c2, 'z');
    
    BOOST_CHECK_EQUAL(i1, 13);
    BOOST_CHECK_EQUAL(i2, 666);

    BOOST_CHECK_EQUAL(d1, 9.81);
    BOOST_CHECK_EQUAL(d2, 1.41);
    
}

/******************************************************************************/

BOOST_AUTO_TEST_CASE(test_scoped_value_string) {
    std::string str1 = "hello";
    std::string str2 = "plop";
    
    std::string cstr1 = "aws";
    std::string cstr2 = "azure";
    
    // create a local scope
    {
        ScopedValue<std::string> scoped_str1(str1, "goodbye");
        ScopedValue<std::string> scoped_str2(str2, "groumpf");

        ScopedValue<std::string> scoped_cstr1(cstr1, "product1");
        ScopedValue<std::string> scoped_cstr2(cstr2, "product2");
    
        BOOST_CHECK_EQUAL(str1, std::string("goodbye"));
        BOOST_CHECK_EQUAL(str2, std::string("groumpf"));
        
        BOOST_CHECK_EQUAL(cstr1, std::string("product1"));
        BOOST_CHECK_EQUAL(cstr2, std::string("product2"));
    }
    
    BOOST_CHECK_EQUAL(str1, std::string("hello"));
    BOOST_CHECK_EQUAL(str2, std::string("plop"));
    
    BOOST_CHECK_EQUAL(cstr1, std::string("aws"));
    BOOST_CHECK_EQUAL(cstr2, std::string("azure"));

    // create a local scope
    {
        ScopedValue<std::string> scoped_str1(str1, "sun", "moon");
        ScopedValue<std::string> scoped_str2(str2, "dry", "rain");

        ScopedValue<std::string> scoped_cstr1(cstr1, "microsoft", "linux");
        ScopedValue<std::string> scoped_cstr2(cstr2, "apple", "google");
    
        BOOST_CHECK_EQUAL(str1, std::string("sun"));
        BOOST_CHECK_EQUAL(str2, std::string("dry"));
        
        BOOST_CHECK_EQUAL(cstr1, std::string("microsoft"));
        BOOST_CHECK_EQUAL(cstr2, std::string("apple"));
    }
    
    BOOST_CHECK_EQUAL(str1, std::string("moon"));
    BOOST_CHECK_EQUAL(str2, std::string("rain"));
    
    BOOST_CHECK_EQUAL(cstr1, std::string("linux"));
    BOOST_CHECK_EQUAL(cstr2, std::string("google"));
}

/******************************************************************************/

struct ScopedValueComplexType {
    ScopedValueComplexType(int ii, float ff, const std::string& sstr) :
        i(ii),
        f(ff),
        str(sstr)
    {
        ++counter_;
    }

    ~ScopedValueComplexType() {
        --counter_;
    }

    ScopedValueComplexType(const ScopedValueComplexType& ct) :
        ScopedValueComplexType(ct.i, ct.f, ct.str) {}
    ScopedValueComplexType(ScopedValueComplexType&& ct) :
        ScopedValueComplexType(ct.i, ct.f, ct.str) {}

    ScopedValueComplexType& operator=(const ScopedValueComplexType& rhs) = default;
    ScopedValueComplexType& operator=(ScopedValueComplexType&& rhs) = default;

    bool operator==(const ScopedValueComplexType& rhs) const {
        return
            i == rhs.i &&
            f == rhs.f &&
            str == rhs.str;
    }

    int i;
    float f;
    std::string str;
    static int counter_;
};
int ScopedValueComplexType::counter_ = 0;

BOOST_AUTO_TEST_CASE(test_scoped_value_complex_types) {
    ScopedValueComplexType svct(42, 3.14f, "eingousef");

    const ScopedValueComplexType reference_svct = svct;

    BOOST_CHECK(svct == reference_svct);
    BOOST_CHECK_EQUAL(ScopedValueComplexType::counter_, 2);

    // create a local scope
    {
        ScopedValue<ScopedValueComplexType> scoped_complex_type(
            svct, ScopedValueComplexType(13, 2.71f, "tomate")
        );

        BOOST_CHECK_EQUAL(svct.i, 13);
        BOOST_CHECK_EQUAL(svct.f, 2.71f);
        BOOST_CHECK_EQUAL(svct.str, std::string("tomate"));
        BOOST_CHECK_EQUAL(ScopedValueComplexType::counter_, 3);
    }

    BOOST_CHECK(svct == reference_svct);
    BOOST_CHECK_EQUAL(ScopedValueComplexType::counter_, 2);

    // create a local scope
    {
        ScopedValue<ScopedValueComplexType> scoped_complex_type(
            svct, ScopedValueComplexType(7, 9.81f, "voiture"),
            ScopedValueComplexType(299792458, 6.67f, "splash"));

        BOOST_CHECK_EQUAL(svct.i, 7);
        BOOST_CHECK_EQUAL(svct.f, 9.81f);
        BOOST_CHECK_EQUAL(svct.str, std::string("voiture"));
        BOOST_CHECK_EQUAL(ScopedValueComplexType::counter_, 3);
    }

    BOOST_CHECK_EQUAL(svct.i, 299792458);
    BOOST_CHECK_EQUAL(svct.f, 6.67f);
    BOOST_CHECK_EQUAL(svct.str, std::string("splash"));
    BOOST_CHECK_EQUAL(ScopedValueComplexType::counter_, 2);
}

/******************************************************************************/

BOOST_AUTO_TEST_CASE(test_scoped_functor) {
    int* n = new int(0);
    {
        ScopedFunctor delete_n( [&]{ delete n; n = nullptr; } );
    }
    BOOST_CHECK( n == nullptr );

    int* n1 = new int(0);
    int* n2 = new int(0);
    std::string order;
    {
        ScopedFunctor delete_n1( [&]{ delete n1; n1 = nullptr; order += "n1"; } );
        ScopedFunctor delete_n2( [&]{ delete n2; n2 = nullptr; order += "n2"; } );
    }
    BOOST_CHECK( n1 == nullptr );
    BOOST_CHECK( n2 == nullptr );
    BOOST_CHECK_EQUAL( order, "n2n1" );

    // test exception handling
    {
        try {
            ScopedFunctor should_throw( nullptr );
            BOOST_FAIL( "Should have thrown" );
        }
        catch( const std::invalid_argument& ) {
            // expected
        }
        catch( ... ) {
            BOOST_FAIL( "Should have thrown a std::invalid_argument");
        }
    }
}

/******************************************************************************/

BOOST_AUTO_TEST_CASE(test_scoped_functors) {
    int* n1 = new int(0);
    int* n2 = new int(0);
    std::string order;
    {
        ScopedFunctors delete_n;
        delete_n.defer( [&]{ delete n1; n1 = nullptr; order += "n1"; } );
        delete_n.defer( [&]{ delete n2; n2 = nullptr; order += "n2"; } );
    }
    BOOST_CHECK_EQUAL( order, "n2n1" );

    // test exception handling
    {
        try {
            ScopedFunctors should_throw;
            should_throw.defer( nullptr );
            BOOST_FAIL( "Should have thrown" );
        }
        catch( const std::invalid_argument& ) {
            // expected
        }
        catch( ... ) {
            BOOST_FAIL( "Should have thrown a std::invalid_argument");
        }
    }
}

/******************************************************************************/

BOOST_AUTO_TEST_SUITE_END()
}
