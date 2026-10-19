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

#ifndef OES_CORE_UTILS_ENUM_DECL_H
#define OES_CORE_UTILS_ENUM_DECL_H

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_enum.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <string>
#include <vector>
#include <stdexcept>

namespace oes {
namespace core {

/*****************************************************************************/

// Inspired from SO's 5093460: http://goo.gl/WBLL6
// OES_DEFINE_ENUM_IMPL_* macros below are implementation details.
#define OES_DEFINE_ENUM_IMPL_TOSTRING(r, data, elem) \
    case elem: return BOOST_PP_STRINGIZE(elem);
#define OES_DEFINE_ENUM_IMPL_VALUES(r, data, elem) elem,
#define OES_DEFINE_ENUM_IMPL_ISVALID(r, data, elem) case elem: return true;
#define OES_DEFINE_ENUM_IMPL_TOENUM(r, data, elem) \
    else if (enum_string == BOOST_PP_STRINGIZE(elem)) { return elem; }

/*****************************************************************************/

/*!
 * \brief Macro declaring an enum type, with implicit conversion to/from string.
 * \ingroup oes_core_utils
 *
 * \details
 * This macro uses the preprocessor, to declare an enumeration as usual but with
 * a slightly altered syntax which allows to implicitly define conversion
 * methods of the enum values into their string representations, and from those
 * strings back to the enum.
 *
 * \param enum_type the enum type name.
 * \param invalid_enum the \em invalid enum value, returned for invalid enum
 *     strings or codes. If no such \em invalid value exists, use any invalid
 *     integer (like -1), to signal the methods converting strings or codes to
 *     enum values they must throw an exception on bad values, rather than
 *     returning the known \em invalid enum value.
 * \param enum_values the list of enum values, of the form (val1)(val2)...,
 *     i.e. a Boost.Preprocessor-compatible Sequence.
 *
 * Example enum declaration:
 * \code
 * OES_DEFINE_ENUM(MyDomain, -1, (Time)(Depth)(TimeMigrated));
 * \endcode
 *
 * The above macro expands into:
 * \code
 * enum MyDomain { Time, Depth, TimeMigrated };
 *
 * struct MyDomainEnumTraits {
 *     typedef MyDomain EnumType;
 *     typedef std::string (*EnumMapper)(MyDomain);
 *
 *     static inline bool hasInvalid() {...}
 *     static inline enum_type invalid() {...}
 *     static inline std::vector<enum_type> values() {...}
 *     static inline const char* toString(enum_type enum_value) {...}
 *     static inline enum_type fromString(const std::string& enum_string) {...}
 *     static inline bool isValidCode(int v_code) {...}
 * };
 * \endcode
 *
 * The generated EnumTraits struct can be used by persistence code to implicitly
 * convert the enum values to string for persistence (on save), and back to
 * enum values (on load), thanks to the preprocessor-generated toString() and
 * fromString() functions.
 *
 * The preprocessor-generated values() method allows to get all valid enum values
 * and for example automaticall ygenerate an GUI option menu, or a persistence
 * \em catalog table for that enum in a RDBMS.
 *
 * \sa OES_DEFINE_ENUM_TRAITS
 * \sa enum_values_of
 * \sa enum_string_of
 * \sa enum_value_of
 *
 */
#define OES_DEFINE_ENUM(enum_type, invalid_enum, enum_values)                \
    enum /*class*/ enum_type {                                                \
        BOOST_PP_SEQ_ENUM(enum_values)                                        \
    };                                                                        \
                                                                              \
    struct enum_type##EnumTraits {                                            \
        typedef enum_type EnumType;                                           \
        typedef std::string (*EnumMapper)(enum_type);                         \
                                                                              \
        static inline const char* name() {                                    \
            return BOOST_PP_STRINGIZE(enum_type);                             \
        }                                                                     \
        static inline bool hasInvalid() {                                     \
            return isValidCode(invalid_enum);                                 \
        }                                                                     \
        static inline enum_type invalid() {                                   \
            if (hasInvalid()) {                                               \
                return static_cast<enum_type>(invalid_enum);                  \
            }                                                                 \
            throw std::runtime_error(                                         \
                "Unknown " BOOST_PP_STRINGIZE(enum_type) " enum value"        \
            );                                                                \
        }                                                                     \
        static inline std::vector<enum_type> values() {                       \
            const enum_type value_array[] = {                                 \
                BOOST_PP_SEQ_FOR_EACH(                                        \
                    OES_DEFINE_ENUM_IMPL_VALUES, enum_type, enum_values      \
                )                                                             \
            };                                                                \
            const size_t count = sizeof(value_array)/sizeof(*value_array);    \
            return std::vector<enum_type>(value_array, value_array + count);  \
        }                                                                     \
        static inline const char* toString(enum_type enum_value) {            \
            switch (enum_value) {                                             \
                BOOST_PP_SEQ_FOR_EACH(                                        \
                    OES_DEFINE_ENUM_IMPL_TOSTRING, enum_type, enum_values    \
                )                                                             \
                default: return "";                                           \
            }                                                                 \
        }                                                                     \
        static inline enum_type fromString(const std::string& enum_string) {  \
            if (enum_string.empty()) { return invalid(); }                    \
            BOOST_PP_SEQ_FOR_EACH(                                            \
                OES_DEFINE_ENUM_IMPL_TOENUM, enum_type, enum_values          \
            )                                                                 \
            return invalid();                                                 \
        }                                                                     \
        static inline bool isValidCode(int v_code) {                          \
            switch (v_code) {                                                 \
                BOOST_PP_SEQ_FOR_EACH(                                        \
                    OES_DEFINE_ENUM_IMPL_TOSTRING, enum_type, enum_values    \
                )                                                             \
                default: return false;                                        \
            }                                                                 \
        }                                                                     \
    } // struct enum_type##EnumTraits

/*****************************************************************************/

/*!
 * \brief Type trait struct to define the relation between a C++ enum and
 *     a helper \em traits struct implementing conversions from/to strings.
 *
 * \tparam TEnum a C++ enum type.
 *
 * Must have the following :
 * - typedef to EnumType
 * - typedef to MapperType
 * - static method with signature: bool hasInvalid()
 * - static method with signature: enum_type invalid()
 * - static method with signature: std::vector<enum_type> values();
 * - static method with signature: const char* toString(enum_type enum_value)
 * - static method with signature: enum_type fromString(const std::string&)
 * - static method with signature: bool isValidCode(int v_code)
 *
 * \tparam the C++ class to persist.
 */
template <typename TEnum>
struct EnumTypeTraits {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));

    // Error below? You are using code that depends on EnumTypeTraits, but did
    // not define your enum using OES_DEFINE_ENUM, or forgot to also use the
    // OES_DEFINE_ENUM_TRAITS macro (at global scope, outside any namespace!)
    BOOST_STATIC_ASSERT(sizeof(typename TEnum::please_specialize_EnumTypeTrait) == 0);
};

/*!
 * \brief Allows generic template code to access the OES_DEFINE_ENUM-generated
 *     \em EnumTraits struct, via partial template specialization.
 *
 * \warning Must be used at global scope only, outside any namespace.
 *
 * \code
 * OES_DEFINE_ENUM(MyGlobalScopeEnum, -1, (Foo)(Bar));
 *
 * namespace acme {
 *     OES_DEFINE_ENUM(MyNamespaceScopeEnum, -1, (Foo)(Bar));
 *
 *     struct AcmeBuilder {
 *         OES_DEFINE_ENUM(MyClassScopeEnum, -1, (Foo)(Bar));
 *     };
 * } // namespace acme
 *
 * OES_DEFINE_ENUM_TRAITS(MyGlobalScopeEnum);
 * OES_DEFINE_ENUM_TRAITS(acme::MyNamespaceScopeEnum);
 * OES_DEFINE_ENUM_TRAITS(acme::AcmeBuilder::MyClassScopeEnum);
 * \endcode
 */
#define OES_DEFINE_ENUM_TRAITS(enum_type)                                    \
    namespace oes { namespace core {                                         \
        template<> struct EnumTypeTraits<enum_type> :                         \
                public enum_type##EnumTraits                                  \
        {                                                                     \
            BOOST_STATIC_ASSERT((boost::is_enum<enum_type>::value));          \
        };                                                                    \
    }} // namespace oes::core

/*****************************************************************************/

/*!
 * \brief Gets the textual name of an enum type TEnum.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 *
 * \return the enum type name, as declared in the code, as the first parameter
 *     of the OES_DEFINE_ENUM macro.
 *
 * \note that this name ignores the C++ namespace the enum may be part of.
 *     This implies that different enums can map to the same name, if those
 *     enums are declared in different C++ namespace.
 */
template <typename TEnum>
std::string enum_name_of() {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    return EnumTypeTraits<TEnum>::name();
}

/*!
 * \brief Gets all valid enum values for enum type TEnum.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 *
 * \return the vector of valid TEnum values. The values can be converted into
 *     strings, using enum_string_of()
 */
template <typename TEnum>
std::vector<TEnum> enum_values_of() {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    return EnumTypeTraits<TEnum>::values();
}

/*!
 * \brief Converts an enum value into its string equivalent.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 *
 * \param value the enum value.
 * \return the string matching \a value, exactly as defined in the code.
 *     An empty string, if \a value is not a valid enum value.
 *
 * \note \b Silently returns an empty string on invalid enum values.
 *
 * \sa enum_value_of for the reverse conversion.
 */
template <typename TEnum>
std::string enum_string_of(TEnum value) {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    return EnumTypeTraits<TEnum>::toString(value);
}

/*!
 * \brief Converts an enum value-string into its enum value equivalent.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 *
 * \param value_string the string corresponding to an enum value.
 * \return the enum value \a value_string. If value_string is invalid (empty,
 *     or uses the wrong case, or a previously defined enum value was removed
 *     in the OES_DEFINE_ENUM macro that defined the enum type, this method
 *     either returns the \em invalid enum value, if TEnum has one, otherwise
 *     this method throws an std::runtime_error.
 * \throw std::runtime_error if \a value_string is invalid and TEnum has no
 *     \em invalid enum value.
 *
 * \note \b Silently returns the \em invalid enum on invalid \a value_string,
 *     if TEnum has one. Throws otherwise.
 *
 * \sa enum_string_of for the reverse conversion.
 */
template <typename TEnum>
TEnum enum_value_of(const std::string& value_string) {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    return EnumTypeTraits<TEnum>::fromString(value_string);
}

/*!
 * \brief Converts an enum value-string into its enum value equivalent.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 *
 * \param value_string the string corresponding to an enum value.
 * \param[out] value the resulting enumerated value.
 * \return the enum value \a value_string. If value_string is invalid (empty,
 *     or uses the wrong case, or a previously defined enum value was removed
 *     in the OES_DEFINE_ENUM macro that defined the enum type, this method
 *     either returns the \em invalid enum value, if TEnum has one, otherwise
 *     this method throws an std::runtime_error.
 * \throw std::runtime_error if \a value_string is invalid and TEnum has no
 *     \em invalid enum value.
 *
 * \note \b Silently returns the \em invalid enum on invalid \a value_string,
 *     if TEnum has one. Throws otherwise.
 *
 * \sa enum_string_of for the reverse conversion.
 */
template <typename TEnum>
void set_enum_value_from(const std::string& value_string, TEnum& value) {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    value = enum_value_of<TEnum>(value_string);
}

/*!
 * \brief Determines whether an enum type has a known \em invalid value.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 *
 * \return true if has an invalid value; false otherwise.
 */
template <typename TEnum>
bool enum_has_invalid_value() {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    return EnumTypeTraits<TEnum>::hasInvalid();
}

/*!
 * \brief Gets the known invalid enum of an enum type, if any.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 *
 * \return the invalid enum value.
 * \throw std::runtime_error if TEnum has no known invalid enum value.
 */
template <typename TEnum>
TEnum enum_invalid_value() {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    return EnumTypeTraits<TEnum>::invalid();
}

/*!
 * \brief Convenient function to check whether a value is part of an enumerated.
 *
 * \tparam TEnum the enum type defined using the OES_DEFINE_ENUM macro,
 *     and exposed to template code using the OES_DEFINE_ENUM_TRAITS macro.
 * \param value the value we want to validate.
 *
 * \return true if \p value is valid; false otherwise.
 *
 * \note the \p value is explicitly as a enumerated type to ease client code
 * so they can call it without the template parameter. For instance:
 * \code
 * MyEnum type_=...
 * oes::core::enum_is_valid_value<MyType>(type_);
 * \endcode
 * can be reduced to:
 * \code
 * oes::core::enum_is_valid_value(type_);
 * \endcode
 * To validate an arbitrary integer value, you can cast it:
 * \code
 * oes::core::enum_is_valid_value(static_cast<MyType>(-1));
 * \endcode
 */
template <typename TEnum>
bool enum_is_valid_value(TEnum value) {
    BOOST_STATIC_ASSERT((boost::is_enum<TEnum>::value));
    return EnumTypeTraits<TEnum>::isValidCode(value);
}

/*****************************************************************************/

} // namespace core
} // namespace oes

#endif
