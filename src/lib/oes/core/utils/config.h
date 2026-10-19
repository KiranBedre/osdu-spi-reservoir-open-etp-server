// ============================================================================
// Copyright 2008-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_UTILS_CONFIG_H
#define OES_CORE_UTILS_CONFIG_H

/*!
 * \file oes/core/utils/config.h
 * \brief Platform specific configuration
 */

// Preprocessing utilities
#define OES_CPP_STRING(x) OES_CPP_STRING_AUX(x)
#define OES_CPP_STRING_AUX(x) #x

// Define build platform configuration macros
#define OES_PROJECT_PLATFORM_STRING OES_CPP_STRING(OES_PROJECT_PLATFORM)
#define OES_PROJECT_BUILD_TYPE_STRING OES_CPP_STRING(OES_PROJECT_BUILD_TYPE)

#if defined(_WIN32)
#define OES_LIBDIR_NAME    "bin"
#define OES_PATH_SEPARATOR ";"
#define OES_SLASH          "\\"
#else
#define OES_LIBDIR_NAME    "lib"
#define OES_PATH_SEPARATOR ":"
#define OES_SLASH          "/"
#endif

#if defined(_WIN32)

// Include C++ standard alternative token definitions
#include <iso646.h>

// Disable some annoying warnings

// Eliminate security warnings about standard C library functions
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// Warning C4251: 'ClassA ClassB::variable_' : class 'ClassA' needs to have
// dll-interface to be used by clients of class 'ClassB'
#pragma warning(disable : 4251)

 // Warning C4275: non dll-interface class '<...>' used as base for dll-interface
 // class '<...>'
#pragma warning(disable : 4275)

// warning C4503: '<...>' : decorated name length exceeded, name was 
// truncated
#pragma warning(disable : 4503)

// warning C4800: '<...>' : forcing value to bool 'true' or 'false
// (performance warning)
#pragma warning(disable : 4800)

#endif

#if defined(__INTEL_COMPILER)
// Warning #177: function "<...>" was declared but never referenced
#pragma warning(disable : 177)
#endif

#if defined(__clang__)
    // Remove compatibility warnings with C++98
    #pragma GCC diagnostic ignored "-Wc++98-compat"
    #pragma GCC diagnostic ignored "-Wc++98-compat-pedantic"

    // Remove GNU extension warnings
    #pragma GCC diagnostic ignored "-Wgnu-statement-expression"
    #pragma GCC diagnostic ignored "-Wvariadic-macros"
    #pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

    // Remove inaccurate warnings about doxygen comments
    #pragma GCC diagnostic ignored "-Wdocumentation"
    #pragma GCC diagnostic ignored "-Wdocumentation-unknown-command"

    // Remove information about structure padding (maybe too radical)
    #pragma GCC diagnostic ignored "-Wpadded"

    // Remove warnings about exit destructors
    // This warning is purely informational and can be safely removed
    // (see: http://stackoverflow.com/questions/14335494/how-to-deal-with-exit-time-destructor-warning-in-clang)
    #pragma GCC diagnostic ignored "-Wexit-time-destructors"

    // Remove warnings about global constructors
    // This warning is purely informational and can be safely removed
    // (see: http://stackoverflow.com/questions/15708411/how-to-deal-with-global-constructor-warning-in-clang)
    #pragma GCC diagnostic ignored "-Wglobal-constructors"

    // 'xxx' has no out-of-line virtual method definitions; its vtable will be emitted in every translation unit
    #pragma GCC diagnostic ignored "-Wweak-vtables"

    // Warning: "n enumeration values not explicitly handled in switch"
    // This warning is emitted even if there is a 'default' case to catch enum
    // values not handled explicitly
    #pragma GCC diagnostic ignored "-Wswitch-enum"

    // Warning: default label in switch which covers all enumeration values
    #pragma GCC diagnostic ignored "-Wcovered-switch-default"

    // Warning: implicit conversion turns string literal into bool
    // This warning is emitted during expansion of OES_DEFINE_ENUM and cannot be
    // fixed
    #pragma GCC diagnostic ignored "-Wstring-conversion"

    // Warning: comparing floating point with == or != is unsafe
    // This warning appears in template instantiations in oes_core_types and
    // cannot be easily fixed
    #pragma GCC diagnostic ignored "-Wfloat-equal"

    // *** Warnings disabled TEMPORARILY ***
    // They correspond to C++11 best practice warnings that we can implemented later.
    // Clang provides a source code modernization tool clang-tidy-3.8 that does
    // a really good job
    #pragma GCC diagnostic ignored "-Winconsistent-missing-override"

#endif

/*! \def GCC_DIAGNOSTIC_ON(warning)
 * \brief Turns a GCC warning on
 * \details Turns a GCC warning on for the code that follows the macro. This
 * macro has no effect for non GCC compilers.
 */

/*! \def CLANG_DIAGNOSTIC_ON(warning)
 * \brief Turns a CLANG warning on
 * \details Turns a CLANG warning on for the code that follows the macro. This
 * macro has no effect for non CLANG compilers.
 */

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (defined(__clang__) || ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402)
#  define GCC_DIAGNOSTIC_STR(s) #s
#  define GCC_DIAGNOSTIC_JOINSTR(x,y) GCC_DIAGNOSTIC_STR(x ## y)
#  define GCC_DIAGNOSTIC_DO_PRAGMA(x) _Pragma (#x)
#  define GCC_DIAGNOSTIC_PRAGMA(x)    GCC_DIAGNOSTIC_DO_PRAGMA(GCC diagnostic x)
#  if defined(__clang__)
#    define CLANG_DIAGNOSTIC_ERROR(x) GCC_DIAGNOSTIC_PRAGMA(push) GCC_DIAGNOSTIC_PRAGMA(error GCC_DIAGNOSTIC_JOINSTR(-W,x))
#    define CLANG_DIAGNOSTIC_OFF(x)   GCC_DIAGNOSTIC_PRAGMA(push) GCC_DIAGNOSTIC_PRAGMA(ignored GCC_DIAGNOSTIC_JOINSTR(-W,x))
#    define CLANG_DIAGNOSTIC_ON(x)    GCC_DIAGNOSTIC_PRAGMA(pop)
#    define GCC_DIAGNOSTIC_ERROR(x)
#    define GCC_DIAGNOSTIC_OFF(x)
#    define GCC_DIAGNOSTIC_ON(x)
#  elif ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#    define GCC_DIAGNOSTIC_ERROR(x) GCC_DIAGNOSTIC_PRAGMA(push) GCC_DIAGNOSTIC_PRAGMA(error GCC_DIAGNOSTIC_JOINSTR(-W,x))
#    define GCC_DIAGNOSTIC_OFF(x)   GCC_DIAGNOSTIC_PRAGMA(push) GCC_DIAGNOSTIC_PRAGMA(ignored GCC_DIAGNOSTIC_JOINSTR(-W,x))
#    define GCC_DIAGNOSTIC_ON(x)    GCC_DIAGNOSTIC_PRAGMA(pop)
#    define CLANG_DIAGNOSTIC_ERROR(x)
#    define CLANG_DIAGNOSTIC_OFF(x)
#    define CLANG_DIAGNOSTIC_ON(x)
#  else
#    define GCC_DIAGNOSTIC_ERROR(x) GCC_DIAGNOSTIC_PRAGMA(error GCC_DIAGNOSTIC_JOINSTR(-W,x))
#    define GCC_DIAGNOSTIC_OFF(x)   GCC_DIAGNOSTIC_PRAGMA(ignored GCC_DIAGNOSTIC_JOINSTR(-W,x))
#    define GCC_DIAGNOSTIC_ON(x)    GCC_DIAGNOSTIC_PRAGMA(warning GCC_DIAGNOSTIC_JOINSTR(-W,x))
#    define CLANG_DIAGNOSTIC_ERROR(x)
#    define CLANG_DIAGNOSTIC_OFF(x)
#    define CLANG_DIAGNOSTIC_ON(x)
#  endif
#else
#  define GCC_DIAGNOSTIC_ERROR(x)
#  define GCC_DIAGNOSTIC_OFF(x)
#  define GCC_DIAGNOSTIC_ON(x)
#  define CLANG_DIAGNOSTIC_ERROR(x)
#  define CLANG_DIAGNOSTIC_OFF(x)
#  define CLANG_DIAGNOSTIC_ON(x)
#endif

GCC_DIAGNOSTIC_OFF(ignored-qualifiers)

/*!
 * \def CXX11_NORETURN
 * \brief Specifies that a function does not return
 * \details
 * This macro must be used instead of the standard C++11 attribute
 * [[noreturn]] since it is not uniformly supported across compilers
 */

#if defined(_MSC_VER)
    // [[noreturn]] is supported in Visual studio 2015
    #if _MSC_VER >= 1900
        #define CXX11_NORETURN [[noreturn]]
    #else
        #define CXX11_NORETURN __declspec(noreturn)
    #endif
#elif defined(__INTEL_COMPILER)
    #if __INTEL_COMPILER >= 1310 && defined(__INTEL_CXX11_MODE__)
        #define CXX11_NORETURN [[noreturn]]
    #endif
#elif defined(__clang__)
    // [[noreturn]] is supported from clang 3.3
    #if (__clang_major__ * 100 + __clang_minor__) >= 303
        #define CXX11_NORETURN [[noreturn]]
    #endif
#elif defined(__GNUC__)
    // [[noreturn]] is supported from gcc 4.8
    #if (__GNUC__ * 100 + __GNUC_MINOR__) >= 408
        #define CXX11_NORETURN [[noreturn]]
    #endif
#endif

#if !defined(CXX11_NORETURN)
    #define CXX11_NORETURN
#endif

/*!
 * \def CXX11_NOEXCEPT
 * \brief Specifies that a function does not throw exceptions
 * \details
 * This macro must be used instead of the standard C++11 extension
 * noexcept since it is not uniformly supported across compilers
 */

#if defined(_MSC_VER)
    // noexcept is supported in Visual studio 2015
    #if _MSC_VER >= 1900
        #define CXX11_NOEXCEPT noexcept
    #endif
#elif defined(__INTEL_COMPILER)
    #if __INTEL_COMPILER >= 1310 && defined(__INTEL_CXX11_MODE__)
        #define CXX11_NOEXCEPT noexcept
    #endif
#elif defined(__clang__)
    // noexcept is supported from clang 3.0
    #if (__clang_major__ * 100 + __clang_minor__) >= 300
        #define CXX11_NOEXCEPT noexcept
    #endif
#elif defined(__GNUC__)
    // noexcept is supported from gcc 4.6
    #if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406
        #define CXX11_NOEXCEPT noexcept
    #endif
#endif

#if !defined(CXX11_NOEXCEPT)
    #define CXX11_NOEXCEPT
#endif


/*!
 * \brief Function to avoid "unused parameter" warnings
 * \details
 * This do nothing template function prevents compilers from issues a warning
 * when declaring <em>on purpose</em> a variable that is not used, typically
 * to ease debugging. It is identical to boost::ignore_unused_variable_warning
 * from concept_check.hpp, but without all the includes that header needs.
 *
 * \note also similar in purpose to Q_UNUSED.
 * \note looks like a macro, but it is an actual function in the global namespace.
 */
#ifdef __cplusplus
template <class T>
inline void OES_UNUSED(T const&) { }
#endif

/*!
 * \def OES_DEBUG_UNUSED(x)
 * \brief Function to avoid "unused parameter" warnings in debug mode
 * \see OES_UNUSED
 */

/*!
 * \def OES_RELEASE_UNUSED(x)
 * \brief Function to avoid "unused parameter" warnings in release mode
 * \see OES_UNUSED
 */

#if defined(OES_DEBUG_BUILD)
#  define OES_DEBUG_UNUSED(x)   OES_UNUSED(x)
#  define OES_RELEASE_UNUSED(x) ((void)0)
#else
#  define OES_DEBUG_UNUSED(x)   ((void)0)
#  define OES_RELEASE_UNUSED(x) OES_UNUSED(x)
#endif

/*! \def nullptr
 * \brief nullptr is the null pointer as defined in C++0x.
 * \details
 * - It should be used in place of 0 or NULL. This definition will be removed
 * once each supported compiler will define and support nullptr keyword.
 * \ingroup oes_core_utils
 */

// For future re-factoring, note that BOOST_NO_NULLPTR provides indication on compiler support 
// of nullptr_t (careful through since, as of today, we cannot include boost/config.hpp here)

// INTEL
#if defined(__INTEL_COMPILER) && __INTEL_COMPILER >= 1310 && defined(__INTEL_CXX11_MODE__)
#define OES_HAS_NULLPTR
// CLANG
#elif defined(__clang__)
    #if __has_extension(cxx_nullptr)
        #define OES_HAS_NULLPTR
    #endif
// GCC
#elif __GNUC__ >= 6 || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 6 && defined(__GXX_EXPERIMENTAL_CXX0X__))
#define OES_HAS_NULLPTR
// VS
#elif defined(_WIN32) && defined(_NATIVE_NULLPTR_SUPPORTED)
#define OES_HAS_NULLPTR
#endif

#if !defined(OES_HAS_NULLPTR)
#define nullptr NULL
#endif

#if (__cplusplus >= 201703L) || (defined (_MSVC_LANG) && (_MSVC_LANG >= 201703L))
#define OES_CPP17_BUILD 
#endif

#endif
