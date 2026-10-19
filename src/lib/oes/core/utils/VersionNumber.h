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

#ifndef OES_CORE_UTILS_VERSIONNUMBER_H
#define OES_CORE_UTILS_VERSIONNUMBER_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/fwd.h>

namespace oes {
namespace core {

class OES_CORE_UTILS_EXPORT VersionNumber {
public:
    typedef unsigned int VersionComponent;
    enum { max_components = 4 };
    enum VersionStatus { invalid = 0, alpha, beta, candidate, release, patch };

    VersionNumber();
    VersionNumber(
        VersionComponent major,
        VersionComponent minor,
        VersionComponent change
    );
    explicit VersionNumber(const oes::core::String&);
    VersionNumber(const VersionNumber&);
    VersionNumber& operator=(const VersionNumber& version);
    VersionNumber& operator=(const oes::core::String&);

    oes::core::String prettify() const;
    oes::core::String serialize() const;
    VersionComponent getComponent(int index) const;
    VersionStatus getStatus() const;

    friend inline int compare(const VersionNumber& a, const VersionNumber& b);

    friend inline bool operator==(const VersionNumber& a, const VersionNumber& b);
    friend inline bool operator!=(const VersionNumber& a, const VersionNumber& b);
    friend inline bool operator< (const VersionNumber& a, const VersionNumber& b);
    friend inline bool operator<=(const VersionNumber& a, const VersionNumber& b);
    friend inline bool operator> (const VersionNumber& a, const VersionNumber& b);
    friend inline bool operator>=(const VersionNumber& a, const VersionNumber& b);

    friend inline VersionNumber& operator<<(VersionNumber& version, const oes::core::String& str);
    friend inline VersionNumber& operator>>(const oes::core::String& str, VersionNumber& version);

    friend inline const VersionNumber& operator>>(const VersionNumber& version, oes::core::String& str);
    friend inline const VersionNumber& operator<<(oes::core::String& str, const VersionNumber& version);

private:
    VersionComponent components_[max_components];
    VersionStatus status_;
};


inline int compare(const VersionNumber& a, const VersionNumber& b) {
    static const int last_component = VersionNumber::max_components - 1;

    if (a.status_ == VersionNumber::invalid) {
        return b.status_;
    }
    else if (b.status_ == VersionNumber::invalid) {
        return a.status_;
    }

    for (int i = 0; i < last_component; ++i) {
        int n = a.components_[i] - b.components_[i];
        if (n != 0) {
            return n;
        }
    }

    int n = a.status_ - b.status_;
    if (n != 0) {
        return n;
    }

    return a.components_[last_component] - b.components_[last_component];
}

inline bool operator ==(const VersionNumber& a, const VersionNumber& b) {
    return compare(a, b) == 0;
}
inline bool operator !=(const VersionNumber& a, const VersionNumber& b) {
    return compare(a, b) != 0;
}
inline bool operator <(const VersionNumber& a, const VersionNumber& b) {
    return compare(a, b) < 0;
}
inline bool operator <=(const VersionNumber& a, const VersionNumber& b) {
    return compare(a, b) <= 0;
}
inline bool operator >(const VersionNumber& a, const VersionNumber& b) {
    return compare(a, b) > 0;
}
inline bool operator >=(const VersionNumber& a, const VersionNumber& b) {
    return compare(a, b) >= 0;
}

inline VersionNumber& operator<<(VersionNumber& version, const oes::core::String& str){
    version = VersionNumber( str );
    return version;
}
inline VersionNumber& operator>>(const oes::core::String& str, VersionNumber& version){
    return version << str;
}

inline const VersionNumber& operator>>(const VersionNumber& version, oes::core::String& str){
    str = version.prettify();
    return version;
}
inline const VersionNumber& operator<<(oes::core::String& str, const VersionNumber& version){ // cppcheck-suppress constParameter
    return version >> str;
}
}
} // namespace oes::core

#endif
