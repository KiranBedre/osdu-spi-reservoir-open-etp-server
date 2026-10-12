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

#ifndef OES_CORE_UTILS_SAFEBOOLOPERATOR_H
#define OES_CORE_UTILS_SAFEBOOLOPERATOR_H

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/config.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

/*****************************************************************************/

/* \def OES_SAFE_BOOL_OPERATOR(predicate)
 * \brief Macro used to implement the safe-bool idiom for classes.
 * \ingroup oes_core_utils
 *
 * \details
 * To be used in the declaration of a class, without a training semi-colon.
 *
 * If that class can benefit from an implicit conversion to a boolean type,
 * for checking in if-conditions or for or while loops, this macros defines
 * a \em safe conversion to bool, using either an explicit operator bool() in
 * C++11 compilers, or the so-called \em Safe-Bool idiom which avoids most
 * undesirable side-effets of an implicit operator bool().
 *
 * \param predicate a call to a member function return a boolean status for
 *     the current instance.
 *
 * Example class declaration:
 * \code
 * struct MyType {
 *     bool isValid() const;
 *     OES_SAFE_BOOL_OPERATOR(isValid())
 *     static const MyType& invalid();
 *     ...
 * };
 * \encode
 *
 * Example of use:
 * \code
 * MyType getType(bool validOne) {
 *     return validOne? MyType(...): MyType::invalid();
 * }
 * void doSomething() {
 *     MyType type1 = getType(false);
 *     if (type1.isValid()) { ... }
 *     // the above and the below are equivalent.
 *     // (code below is an initialization (not an assignment), so no warning)
 *     if (MyType type2 = getType(false)) { ... }
 * }
 * \encode
 *
 * \warning using this macro allows to compare 2 instances of the classes:
 * \code
 * MyType a1, a2;
 * a1 == a2; // compile!
 * \encode
 * The comparison will check if the 2 instances are both valid or both invalid.
 * It's advised to define the == and != operators, either as private without
 * implementation (hidden) or as public, with implementation.
 */

#if !defined(BOOST_NO_EXPLICIT_CONVERSION_OPERATORS) \
 && !defined(BOOST_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS)
#define OES_SAFE_BOOL_OPERATOR(predicate) \
    explicit operator bool() const { \
        return predicate; \
    }
#else
#define OES_SAFE_BOOL_OPERATOR(predicate) \
    private: \
        struct safe_bool_helper { int x; }; \
        typedef int safe_bool_helper::* safe_bool; \
    public: \
        operator safe_bool() const { \
            return predicate? &safe_bool_helper::x: 0; \
        }
#endif

/*****************************************************************************/

#endif
