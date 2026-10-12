// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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
#include <type_traits>


/*!
 * \brief Type traits for `fmt` library.
 * \details Should be inside `fmt` namespace for correct work.
 */
namespace fmt
{
    /*!
     * \brief Test availability of format function for specified type of string.
     */
    template <typename TFormat>
    struct has_fmt_format
    {
    private:
        template <typename U>
        static decltype(static_cast<void>(format(std::declval<U>())), std::true_type()) test(int);

        template <typename>
        static std::false_type test(...);

    public:
        typedef decltype(test<TFormat>(0)) type;
        enum { value = type::value };
    };

    template <typename TFormat>
    constexpr bool has_fmt_format_v = has_fmt_format<TFormat>::value;
}
