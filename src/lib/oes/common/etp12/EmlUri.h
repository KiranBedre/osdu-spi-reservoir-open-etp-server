// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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
#ifndef OES_COMMON_ETP12_EMLURI_H
#define OES_COMMON_ETP12_EMLURI_H

#include "../export.h"

#include <oes/core/utils/Guid.h>

#include <iosfwd>
#include <string>

namespace oes::common::etp12 {

struct OES_COMMON_EXPORT EmlUri {
    enum ML   : uint8_t { eNO_ML, eEML20, eRESQML20, eUNKNOWN_ML, eEML23, eRESQML22, eWITSML21, ePRODML22 };
    enum Type : uint8_t { eNO_TYPE, eDATA_SPACE, eOBJ_TYPE, eOBJ_INSTANCE };

    std::string dataspace_;
    std::string obj_type_;
    oes::core::Guid guid_;
    std::string version_;
    ML ml_ = eNO_ML;
    Type type_ = eNO_TYPE;

    static EmlUri parse(std::string uri);
    static EmlUri parse(const char* curi, size_t len);
    static EmlUri::ML parseML(const char* ml, size_t len);

    static EmlUri make_space(std::string dataspace = std::string());
    static EmlUri make_type(
        std::string dataspace, std::string obj_type,
        ML ml = eEML20
    );
    static EmlUri make_obj(
        std::string dataspace, std::string obj_type, oes::core::Guid guid,
        ML ml = eEML20, std::string version = {}
    );

    bool isValid() const;
    bool isRoot() const;

    bool isSpace() const { return type_ == eDATA_SPACE; }
    bool isType() const { return type_ == eOBJ_TYPE; }
    bool isObj() const { return type_ == eOBJ_INSTANCE; }

    bool defaultDataspace() const { return dataspace_.empty(); }
    bool hasVersion() const { return !version_.empty(); }

    EmlUri child_space(const std::string& dataspace) const;
    EmlUri child_type(std::string obj_type, ML ml = eEML20) const;
    EmlUri child_obj(oes::core::Guid guid) const;

    size_t dataspacePartCount() const;
    std::string dataspaceName() const;
    std::string dataspaceFolder() const;

    explicit operator bool() const {
        return isValid();
    }

    void swap(EmlUri& other);
    bool equals(const EmlUri& other) const;
    bool lessThan(const EmlUri& other) const;
    std::ostream& print(std::ostream& os) const;
    std::ostream& printML(std::ostream& os) const;
    std::string to_string() const;
    std::string to_type() const;

    static std::ostream& printML(std::ostream& os, ML ml);
    static std::ostream& printSpace(std::ostream& os, const std::string& space);
    static std::ostream& percentEncode(std::ostream& os, const std::string& s);
    static std::ostream& percentDecode(std::ostream& os, const std::string& s);

    // Control handling of pre-ETP 1.2 RC2 URIs
    static bool parseLegacyUris(bool flag = true);
    static bool printLegacyUris(bool flag = true);
    struct OES_COMMON_EXPORT ScopedLegacyFlags {
        const bool parse_, print_;
        explicit ScopedLegacyFlags(bool parse = true, bool print = true);
        ~ScopedLegacyFlags();
    };
};

inline bool operator==(const EmlUri& lhs, const EmlUri& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const EmlUri& lhs, const EmlUri& rhs) {
    return !lhs.equals(rhs);
}

inline bool operator<(const EmlUri& lhs, const EmlUri& rhs) {
    return lhs.lessThan(rhs);
}

inline std::ostream& operator<<(std::ostream& os, const EmlUri& uri) {
    return uri.print(os);
}

inline std::ostream& operator<<(std::ostream& os, EmlUri::ML uri_ml) {
    return EmlUri::printML(os, uri_ml);
}

} // namespace oes::common::etp12

namespace std {
    template <> inline void swap(
        oes::common::etp12::EmlUri& lhs,
        oes::common::etp12::EmlUri& rhs
    ) noexcept {
        lhs.swap(rhs);
    }
}

#endif // OES_COMMON_ETP12_EMLURI_H
