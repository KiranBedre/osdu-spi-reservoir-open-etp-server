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

#ifndef OES_CORE_TYPES_CORE_GUID_HPP
#define OES_CORE_TYPES_CORE_GUID_HPP

#include <oes/core/utils/Guid.h>

#include <functional> // for std::hash<T>
#include <unordered_set>

namespace std {

template <> struct hash<oes::core::Guid> {
    typedef oes::core::Guid argument_type;
    typedef std::size_t result_type;
 
    result_type operator()(argument_type const& s) const {
        return oes::core::hash_value(s);
    }
};

} // namespace std

namespace oes {
namespace core {
    using GuidHashSet = std::unordered_set<Guid>;
} // namespace core
} // namespace oes

#endif
