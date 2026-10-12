// ============================================================================
// Copyright 2012-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#pragma once
#include <memory>


/*!
 * \brief Forward-declares T and its shared_ptr typedefs, using a nickname.
 *
 * Declares shared_ptr-to-T and shared_ptr-to-const-T typedefs, of the form
 * {{AKA}}_ptr and Const{{AKA}}_ptr, i.e based on a nickname of T (AKA), instead
 * of T itself. Typically used because T is long and a commonly used and
 * shorter alternative exists.
 *
 * \code
 * // macro example:
 * OES_FWD_DECL_SHARED_PTR_AKA(VeryLongTypeName, NickName);
 *
 * // what the macro above expands to:
 * typedef ::std::shared_ptr<VeryLongTypeName> NickName_ptr;
 * typedef ::std::shared_ptr<VeryLongTypeName const> ConstNickName_ptr;
 * \endcode
 *
 * \sa OES_FWD_DECL_SHARED_PTR if T and AKA should be the same.
 */
#define OES_FWD_DECL_SHARED_PTR_AKA(T,AKA) \
    class T; \
    typedef ::std::shared_ptr<T> AKA ## _ptr; \
    typedef ::std::shared_ptr<T const> Const ## AKA ## _ptr

/*!
 * \brief Forward-declares T and its shared_ptr typedefs.
 *
 * Declares shared_ptr-to-T and shared_ptr-to-const-T typedefs, of the form
 * {{T}}_ptr and Const{{T}}_ptr.
 *
 * \code
 * // macro example:
 * OES_FWD_DECL_SHARED_PTR(TypeName);
 *
 * // what the macro above expands to:
 * typedef ::std::shared_ptr<TypeName> TypeName_ptr;
 * typedef ::std::shared_ptr<TypeName const> ConstTypeName_ptr;
 * \endcode
 *
 * \sa OES_FWD_DECL_SHARED_PTR_AKA
 */
#define OES_FWD_DECL_SHARED_PTR(T) \
    OES_FWD_DECL_SHARED_PTR_AKA(T,T)

/*!
 * \brief Forward-declares the concatenation of T and S and its shared_ptr
 *     typedefs a nickname and a suffix instead of T and S.
 *
 * Variation of OES_FWD_DECL_SHARED_PTR_AKA which first concatenates T with
 * suffix S and AKA with suffix S before calling OES_FWD_DECL_SHARED_PTR_AKA
 * on the result.
 *
 * Useful to declare families of related types, for example an entity Foo,
 * its record FooRec, its manager FooMgr, its identity FooId, etc...
 *
 * \code
 * // macro example:
 * OES_FWD_DECL_SHARED_PTR2_AKA(VeryLongTypeName, NickName, Suffix);
 *
 * // what the macro above expands to:
 * typedef ::std::shared_ptr<VeryNameSuffix> NickNameSuffix_ptr;
 * typedef ::std::shared_ptr<VeryLongTypeNameSuffix const> ConstNickNameSuffix_ptr;
 * \endcode
 *
 * \sa OES_FWD_DECL_SHARED_PTR_AKA
 */
#define OES_FWD_DECL_SHARED_PTR2_AKA(T,AKA,S) \
    OES_FWD_DECL_SHARED_PTR_AKA( \
        T ## S,  \
        AKA ## S \
    )

/*!
 * \brief Forward-declares shared_ptr typedefs, using a suffix.
 *
 * Variation of OES_FWD_DECL_SHARED_PTR which first concatenates T with
 * suffix S before calling OES_FWD_DECL_SHARED_PTR on the result.
 *
 * \code
 * // macro example:
 * OES_FWD_DECL_SHARED_PTR2(TypeName, Suffix);
 *
 * // what the macro above expands to:
 * typedef ::std::shared_ptr<TypeNameSuffix> TypeNameSuffix_ptr;
 * typedef ::std::shared_ptr<TypeNameSuffix const> ConstTypeNameSuffix_ptr;
 * \endcode
 *
 * \sa OES_FWD_DECL_SHARED_PTR
 */
#define OES_FWD_DECL_SHARED_PTR2(T,S) \
    OES_FWD_DECL_SHARED_PTR(T ## S)
