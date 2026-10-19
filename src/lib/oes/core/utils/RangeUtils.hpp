// ============================================================================
// Copyright 2015-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_UTILS_RANGEUTILS_HPP
#define OES_CORE_UTILS_RANGEUTILS_HPP

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/range/iterator_range_core.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <utility>

namespace oes {
namespace core {

/*****************************************************************************/

/*!
 * \brief Adapts Boost.Range-compatible \em collection for C++11 range-for iteration.
 *
 * For example, std::multimap<T>::equal_range() returns a pair of iterators
 * which cannot be directly iterated on using range-for. range_of() fixes that:
 * \code
 * std::multimap<int, std::string> multilang{
 *     { 1, "one" },
 *     { 1, "uno" },
 *     { 2, "two" },
 * };
 * for (const auto& entry : range_of(multilang.equal_range(1))) {
 *     int key = entry.first;
 *     const std::string& val = entry.second;
 *     std::cout << key << " = " << val << std::endl;
 * }
 * \endcode
 * The above will print two lines that read 1 = one and 1 = uno.
 *
 * \param c the collection to iterate on.
 * \return the Boost.Range adaptor providing C++11's range-for compatible API.
 *
 * \sa equal_range_of
 */
template <typename C>
auto range_of(C&& c) {
    return boost::make_iterator_range(std::forward<C>(c));
}

/*!
 * \brief Convenience to iterate over a collection in reverse order.
 *
 * \param c the collection to iterate on.
 * \return the Boost.Range adaptor providing C++11's range-for compatible API,
 *     which reverses the normal iteration order of \a c.
 */
template <typename C>
auto reverse_of(C&& c) {
    return boost::adaptors::reverse(std::forward<C>(c));
}

/*!
 * \brief Convenience to iterate over the entries for a specific key
 *     of an unordered collection.
 *
 * Equivalent to range_of(c.equal_range(k)).
 *
 * \param c unordered collection (std::multimap, boost::multi_index
 *     unordered indexes, etc...) to iterate on.
 * \param k the key in \a c whose values to iterator on.
 * \return the Boost.Range adaptor providing C++11's range-for compatible API.
 */
template <typename C, typename K>
auto equal_range_of(C&& c, K&& k) {
    return boost::make_iterator_range(c.equal_range(std::forward<K>(k)));
}

/*****************************************************************************/

} // namespace core
} // namespace oes

#endif
