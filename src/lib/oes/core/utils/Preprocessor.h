// ============================================================================
// Copyright 2011-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_UTILS_PREPROCESSOR_H
#define OES_CORE_UTILS_PREPROCESSOR_H

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/preprocessor/cat.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

/*! \brief Macro to embed pragma compiler directives within other macros
 * \details
 * - Pragmas cannot be typically embedded within macros as it contains
 * special characters, workaround is compiler-dependent.
 * - This is meant to provide such workaround transparently, using
 * available facilities.
 * \ingroup oes_core_utils
 */
#if defined(_WIN32)
    // Visual VC
    #define OES_PP_PRAGMA(x) __pragma(x)
#else
    // Intel ICC and GNU CC, using ANSI C99 operator
    #define OES_PP_PRAGMA(x) _Pragma(#x)
#endif

/*! \brief Empty macro except when using Intel compiler
 * \ingroup oes_core_utils
 */
#if defined(__INTEL_COMPILER)
    #define OES_PP_IF_INTEL(x) x
#else
    #define OES_PP_IF_INTEL(x)
#endif

/*! \brief Empty macro except when using Microsoft VC compiler
 * \ingroup oes_core_utils
 */
#if defined(_WIN32) // Not using _MSC_VER, as within SharedComponents, we equate WIN32 to VC
    #define OES_PP_IF_MSVC(x) x
#else
    #define OES_PP_IF_MSVC(x)
#endif

/*! \brief Empty macro except when using GNU GCC compiler
 * \ingroup oes_core_utils
 */
#if defined(__GNUC__) and not defined(__INTEL_COMPILER)
    #define OES_PP_IF_GCC(x) x
#else
    #define OES_PP_IF_GCC(x)
#endif

/*! \brief Provides a unique identifier from an input prefix
 * \details
 * - Essentially meant to generate identifiers (notably for local/static) variable
 * names within other macros.
 * \par Known limitations:
 * - The generated identifier is essentially based on __LINE__ ANSI predefined
 * macro until __COUNTER__ becomes more widely available.
 * - As a consequence, given a same prefix, the macro may expand to the same
 * identifier if you use it several times within the same line. Use a different prefix.
 * \ingroup oes_core_utils
 */
#define OES_PP_UNIQUE_ID(Prefix) BOOST_PP_CAT(Prefix,__LINE__)

#endif
