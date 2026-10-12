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

#ifndef OES_CORE_TYPES_CORE_GUID_H
#define OES_CORE_TYPES_CORE_GUID_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/fwd.h>

#include <cstring>
#include <ostream>
#include <iterator>

// When compiling CUDA source on Intel 15 and earlier platforms,
// certain C++11 experimental features need to be excluded from the compilation. 
// In this case, unordered_map include breaks CUDA compilation.
#if (defined PG_SYS_OS_Linux) && !(defined __GXX_EXPERIMENTAL_CXX0X__)
#  define OES_OLD_CUDA_PLATFORM
#endif

#if !(defined OES_OLD_CUDA_PLATFORM)
#  include <set>
#  include <vector>
#  include <unordered_map>
#endif

namespace oes {
namespace core {

class OES_CORE_UTILS_EXPORT Guid {
public:
    typedef unsigned char value_type;
    typedef unsigned char& reference;
    typedef unsigned char const& const_reference;
    typedef unsigned char* iterator;
    typedef unsigned char const* const_iterator;
    typedef size_t size_type;

public:
    Guid();
    explicit Guid(const String& uid);
    Guid(const_iterator begin, const_iterator end);

    // ~Guid() = default;
    // Guid(const Guid&) = default;
    // Guid& operator=(const Guid&) = default;

    template <typename T>
    static Guid from(const T& uid) {
        const auto first = uid.begin();
        return Guid(&*first, (&*first)+std::distance(first, uid.end()));
    }

    static Guid make();
    static Guid parse(const String& uid_string);
    static const Guid& invalid();
    static size_type static_size() {
        return 16;
    }

    static Guid dns_namespace();
    static Guid url_namespace();
    static Guid oid_namespace();
    static Guid x500_namespace();
    static Guid oes_namespace(); // default namespace
    static Guid make_deterministic(const std::string& arbitrary_text);
    static Guid make_deterministic(const Guid& namespace_guid, const std::string& arbitrary_text);
    static Guid make_deterministic(const std::string& namespace_text, const std::string& arbitrary_text);

    // explicit operator bool() const;
    bool isValid() const;
    size_type size() const;
    const_iterator begin() const;
    const_iterator end() const;

    String toXmlString() const;
    String toStringNoDash() const;

    bool reparse(const char* str, size_t len, bool lax);

    void reset();
    iterator begin();
    iterator end();

    int compare(const Guid& other) const;

private:
    unsigned char guid_[16];
};


inline Guid make_guid() {
    return Guid::make();
}

OES_CORE_UTILS_EXPORT void swap(Guid& lhs, Guid& rhs);
OES_CORE_UTILS_EXPORT std::size_t hash_value(const Guid& uid);

#if !(defined OES_OLD_CUDA_PLATFORM)

// type definitions
using SetOfGuid = std::set <Guid>;
using VectorOfGuid = std::vector <Guid>;

struct GuidHashFunctor {
    std::size_t operator()(const Guid& guid) const {
        return (hash_value (guid));
    }
};

template<typename VALUE_TYPE>
using UnorderedMapOfGuid = std::unordered_map < Guid, VALUE_TYPE, GuidHashFunctor>;

#else
#  undef OES_OLD_CUDA_PLATFORM
#endif

// inline Guid::operator bool() const {
//     return isValid();
// }

inline bool Guid::isValid() const {
    const unsigned char*const d = guid_;
    return
        d[0x0] || d[0x1] || d[0x2] || d[0x3] || d[0x4] || d[0x5] || d[0x6] || d[0x7] ||
        d[0x8] || d[0x9] || d[0xa] || d[0xb] || d[0xc] || d[0xd] || d[0xe] || d[0xf];
}

inline Guid Guid::make_deterministic(const std::string& arbitrary_text) {
    return make_deterministic(oes_namespace(), arbitrary_text);
}

inline Guid::iterator Guid::begin() {
    return guid_;
}

inline Guid::const_iterator Guid::begin() const {
    return guid_;
}

inline Guid::iterator Guid::end() {
    return guid_ + static_size();
}

inline Guid::const_iterator Guid::end() const {
    return guid_ + static_size();
}

inline Guid::size_type Guid::size() const {
    return static_size();
}

inline void Guid::reset() {
    std::memset(guid_, 0, static_size()); // zero-init, thus isValid() == false
}

inline int Guid::compare(const Guid& other) const {
    if (*guid_ == *other.guid_) {
        return std::memcmp(guid_, other.guid_, static_size());
    }
    return (*guid_ < *other.guid_)? -1: +1;
}

inline bool operator==(const Guid& lhs, const Guid& rhs) {
    return 0 == lhs.compare(rhs);
}

inline bool operator!=(const Guid& lhs, const Guid& rhs) {
    return 0 != lhs.compare(rhs);
}

inline bool operator<(const Guid& lhs, const Guid& rhs) {
    return lhs.compare(rhs) < 0;
}

inline bool operator>(const Guid& lhs, const Guid& rhs) {
    return lhs.compare(rhs) > 0;
}

inline bool operator<=(const Guid& lhs, const Guid& rhs) {
    return lhs.compare(rhs) <= 0;
}

inline bool operator>=(const Guid& lhs, const Guid& rhs) {
    return lhs.compare(rhs) >= 0;
}

inline std::ostream& operator<<(std::ostream& os, const Guid& uid) {
    return os << uid.toXmlString();
}

} // namespace core
} // namespace oes

#endif
