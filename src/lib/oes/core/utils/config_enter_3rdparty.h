// ============================================================================
// Copyright 2010-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

// WARNING: please, keep this file without any OES or 3rd-party includes

/*!
 * \file oes/core/utils/config_enter_3rdparty.h
 * \brief Include this file BEFORE any 3rd-party includes to push new disabled warnings
 */

// To prevent improper use of enter/leave 3rdparty headers
// Any change of logic here should be duplicated in the other
// header config_leave_3rdparty.h
#ifdef OES_CORE_UTILS_CONFIG_ENTER_3RDPARTY_H
    #error "Enter/leave 3rd-party header mismatch - You probably missed an include of config_leave_3rdparty.h"
#endif
#define OES_CORE_UTILS_CONFIG_ENTER_3RDPARTY_H

#if defined(_WIN32)

    // Warning issues from within MSVC STL implementation, when calling any one of the
    // potentially unsafe methods in the Standard C++ Library.
    // http://msdn.microsoft.com/en-us/library/aa985974.aspx
    #if !defined(_SCL_SECURE_NO_WARNINGS)
        #define _SCL_SECURE_NO_WARNINGS
        // To make sure we restore state as it was before, we flag
        // it as needing an undef
        #define _SCL_SECURE_NO_WARNINGS_UNDEFINE
    #endif

    #pragma warning(push, 0)

    // Please, keep pragma-s arranged by increasing warning #

    // warning C4005: '<...>' : macro redefinition '<...>'
    #pragma warning(disable : 4005)

    // warning C4065: switch statement contains 'default' but no 'case' labels
    #pragma warning(disable :4065)

    // warning C4068: unknown pragma
    #pragma warning(disable : 4068)

    // warning C4100: '<...>' : unreferenced formal parameter
    #pragma warning(disable : 4100)

    // warning C4127: conditional expression is constant
    #pragma warning(disable : 4127)

    // warning C4189: local variable is initialized but not referenced
    #pragma warning(disable : 4189)

    // warning C4244: '<...>' : conversion from '<...>' to '<....>', possible loss of data
    #pragma warning(disable : 4244)

    // warning C4245: '<...>' : conversion from '<...>' to '<...>', signed/unsigned mismatch
    #pragma warning(disable : 4245)

    // warning C4250: '<...>' : inherits '<...>' via dominance
    #pragma warning(disable : 4250)

    // warning C4251: '<...>' : class '<...>' needs to have dll-interface to be used by clients of class '<...>'
    #pragma warning(disable : 4251)

    // warning C4263: member function does not override any base class virtual member function
    #pragma warning(disable : 4263)

    // warning C4264: no override available for virtual member function from base; function is hidden
    #pragma warning(disable : 4264)

    // warning C4265 : '<...>' : class has virtual functions, but destructor is not virtual
    #pragma warning(disable : 4265)

    // warning C4267: ' argument ': conversion from ' size_t ' to ' int ', possible loss of data
    #pragma warning(disable:4267)

    // warning C4275: non dll-interface class '<...>' used as base for dll-interface class '<...>'
    #pragma warning(disable : 4275)

    // warning C4311: 'reinterpret_cast' : pointer truncation from 'void *' to 'long'
    #pragma warning(disable : 4311)

    // warning C4312: 'reinterpret_cast' : conversion from 'long' to 'void *' of greater size
    #pragma warning(disable : 4312)

    // warning C4355: 'this' : used in base member initializer list
    #pragma warning(disable : 4355)

    // warning C4503: '<...>' : decorated name length exceeded, name was truncated
    #pragma warning(disable : 4503)

    // warning C4510: '<...>' : default constructor could not be generated
    #pragma warning(disable : 4510)

    // warning C4512: '<...>' : assignment operator could not be generated
    #pragma warning(disable : 4512)

    // warning C4515: '<...>' : namespace uses itself '<...>'
    #pragma warning(disable : 4515)

    // warning C4535: calling _set_se_translator() requires /EHa (Note: occurs when using Boost.Test)
    #pragma warning(disable : 4535)

    // warning C4610: class '<...>' can never be instantiated - user defined constructor required
    #pragma warning(disable : 4610)

    // warning C4701 (level 4): potentially uninitialized local variable <...> used (Note: occurs when using boost\crc.hpp)
    #pragma warning(disable : 4701)

    // warning C4702 (level 4): unreachable code (Note: occurs when using Avro-cpp)
    #pragma warning(disable : 4702)

    // warning C4706: assignment within conditional expression
    #pragma warning(disable : 4706)

    // warning C4800: '<...>' : forcing value to bool 'true' or 'false' (performance warning)
    #pragma warning(disable : 4800)

    // warning C4996: obsolete function call
    #pragma warning(disable : 4996)

    // warning C6011: Dereferencing NULL pointer
    #pragma warning(disable : 6011)

    // warning C6246: Local declaration of '<...>' hides declaration of the same name in outer scope.
    #pragma warning(disable : 6246)

    // warning C6326: Potential comparison of a constant with another constant
    #pragma warning(disable : 6326)

    // warning C6334: sizeof operator applied to an expression with an operator might yield unexpected results
    #pragma warning(disable : 6334)

    // warning C6385: Invalid data: accessing
    #pragma warning(disable : 6385)


#elif defined(__INTEL_COMPILER)

    #pragma warning(push)

    // Please, keep pragma-s arranged by increasing warning #
    
    // warning #161: unrecognized #pragma
    #pragma warning(disable : 161)

    // warning #177: variable "..." was declared but never referenced
    #pragma warning(disable : 177)

    // warning #279: controlling expression is constant
    #pragma warning(disable : 279)

    // warning #593: '<...>' : variable '<...>' was set but never used
    #pragma warning(disable : 593)

    // warning #858: type qualifier on return type is meaningless
    #pragma warning(disable : 858)

    // warning #869: parameter "<...>" was never referenced
    #pragma warning(disable : 869)

    // warning #1565: attributes are ignored on a class declaration that is not also a definition
    #pragma warning(disable : 1565)
    
    // warning #2196: routine is both "inline" and "noinline"
    #pragma warning(disable : 2196)

    // warning #2415: variable "<...>" of static storage duration was declared but never referenced
    #pragma warning(disable : 2415)

#elif defined(__clang__)

    #pragma GCC diagnostic push

    #pragma GCC diagnostic ignored "-Wc++98-compat"
    #pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"

    // warning: 'xxx' is not defined, evaluates to 0
    #pragma GCC diagnostic ignored "-Wundef"

    // warning: macro name is a reserved identifier
    #pragma GCC diagnostic ignored "-Wreserved-id-macro"
    #pragma GCC diagnostic ignored "-Wdisabled-macro-expansion"
    #pragma GCC diagnostic ignored "-Wkeyword-macro"
    #pragma GCC diagnostic ignored "-Wextra-semi"
    #pragma GCC diagnostic ignored "-Wunused-macros"

    #pragma GCC diagnostic ignored "-Wold-style-cast"
    #pragma GCC diagnostic ignored "-Wswitch-enum"
    #pragma GCC diagnostic ignored "-Wdeprecated"

    #pragma GCC diagnostic ignored "-Wconversion"
    #pragma GCC diagnostic ignored "-Wsign-conversion"
    #pragma GCC diagnostic ignored "-Wshorten-64-to-32"
    #pragma GCC diagnostic ignored "-Wdouble-promotion"
    #pragma GCC diagnostic ignored "-Wunused-local-typedef"
    #pragma GCC diagnostic ignored "-Wgnu-include-next"
    #pragma GCC diagnostic ignored "-Wcast-align"
    #pragma GCC diagnostic ignored "-Wcast-qual"
    #pragma GCC diagnostic ignored "-Wfloat-equal"
    #pragma GCC diagnostic ignored "-Wunused-parameter"
    #pragma GCC diagnostic ignored "-Wunused-const-variable"
    #pragma GCC diagnostic ignored "-Wmissing-prototypes"
    #pragma GCC diagnostic ignored "-Wformat-nonliteral"

    // default label in switch which covers all enumeration values
    #pragma GCC diagnostic ignored "-Wcovered-switch-default"

    // 'xxx' has no out-of-line virtual method definitions; its vtable will be emitted in every translation unit
    #pragma GCC diagnostic ignored "-Wweak-vtables"

    #pragma GCC diagnostic ignored "-Wunreachable-code"
    #pragma GCC diagnostic ignored "-Wunreachable-code-break"
    #pragma GCC diagnostic ignored "-Wmissing-noreturn"
    #pragma GCC diagnostic ignored "-Wshadow"
    #pragma GCC diagnostic ignored "-Wpadded"


#elif defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406

    // NOTE: diagnostic pragmas are only supported from GCC 4.2
    // NOTE: GCC earlier than 4.6 cannot push/pop the warning state


    #pragma GCC diagnostic push


    // We can ignore all warnings from third parties
    // as we have no control over them.
    // Those warnings should be fixed by upgrading third parties.

    // warning: array subscript is below array bounds
    #pragma GCC diagnostic ignored "-Warray-bounds"

    // warning: type qualifiers ignored on function return type
    // This warning frequently occurs in Boost template instantiations
    #pragma GCC diagnostic ignored "-Wignored-qualifiers"

    // warning: '...' may be used uninitialized in this function
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

    // warning: comparison between signed and unsigned integer expressions
    // This warning occurs in speedtest1.c
    #pragma GCC diagnostic ignored "-Wsign-compare"

    // warning: dereferencing type-punned pointer will break strict-aliasing rules
    // This warning occurs in Boost make_shared()
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"

    // warning: variable '...' set but not used
    // This warning frequently occurs in Boost.Time
    #pragma GCC diagnostic ignored "-Wunused-but-set-variable"

    // warning '...' defined but not used [-Wunused-function]
    // This warning occurs in speedtest1.c
    #pragma GCC diagnostic ignored "-Wunused-function"

    // warning: typedef 'value_type' locally defined but not used
    // This warning occurs in Boost multi_index
    #pragma GCC diagnostic ignored "-Wunused-local-typedefs"

    // warning: unused parameter '...'
    // This warning frequently occurs in Boost.Time
    #pragma GCC diagnostic ignored "-Wunused-parameter"

    #if (__GNUC__ >= 6)

    // warning:  '...' defined but not used
    // This warning frequently occurs in Boost
    #pragma GCC diagnostic ignored "-Wunused-variable"

    // warning: this 'if' clause does not guard...
    #pragma GCC diagnostic ignored "-Wmisleading-indentation"

    // warning: '...' defined but not used
    // This warning occurs in sqlite
    #pragma GCC diagnostic ignored "-Wunused-const-variable"

    // warning: ‘template<class> class std::auto_ptr’ is deprecated warning:
    // This warning occurs in avro
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    #pragma GCC diagnostic ignored "-Wunused"

    #pragma GCC diagnostic ignored "-Wimplicit-fallthrough"

    // This warning occurs in boost 1.64 headers
    #pragma GCC diagnostic ignored "-Wparentheses"

    // These warnings occur in stdsoap2.cpp
    #pragma GCC diagnostic ignored "-Wformat-truncation"
    #pragma GCC diagnostic ignored "-Wrestrict"

    #endif

    #if (__GNUC__ >= 9)
        // warning: cast between incompatible function types
        // This warning occurs in sqlite
        #pragma GCC diagnostic ignored "-Wcast-function-type"

        #ifdef __cplusplus
            // This warning occurs in azure-storage-cpp
            #pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
            #pragma GCC diagnostic ignored "-Wreorder"
            #pragma GCC diagnostic ignored "-Wunused-value"

        #endif

    #endif

#endif
