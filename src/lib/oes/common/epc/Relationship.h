/*-----------------------------------------------------------------------
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"; you may not use this file except in compliance
with the License.  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
-----------------------------------------------------------------------*/

#ifndef OES_COMMON_EPC_RELATIONSHIP
#define OES_COMMON_EPC_RELATIONSHIP

#include "../export.h"

#include <string>
#include <vector>

namespace oes::common::epc {

/****************************************************************************/

enum class RelMode: unsigned char {
    eInternal = 0,
    eExternal = 1,
};

enum class RelType : unsigned char {
    eUnknownType = 0,
    eDestinationObject,
    eSourceObject,
    eDestinationMedia,
    eSourceMedia,
    eChunkedPart,
    eExternalPartProxyToMl,
    eMlToExternalPartProxy,
    eExternalResource,
};

/****************************************************************************/

OES_COMMON_EXPORT
const std::vector<std::string>& allBuiltinRelationTypes();

OES_COMMON_EXPORT
const std::string& relationTypeAsString(RelType type);

OES_COMMON_EXPORT
RelType relationTypeFromString(const char* s, size_t n);

inline RelType relationTypeFromString(const std::string& s) {
    return relationTypeFromString(s.data(), s.length());
}

/****************************************************************************/

/**
 * This class intends to represent a relationship entry of a rel file of a package part.
 */
class OES_COMMON_EXPORT Relationship {
private:
    RelMode mode_;          /// whether destination part is external or internal to package.
    RelType builtin_type_;  /// type of relationship. (if "builtin", type_ otherwise)
    std::string target_;    /// destination part path.
    std::string type_;      /// type of relationship. (if arbitrary, builtin_type_ otherwise)
    std::string id_;        /// uniquely identify relationship in a rel file. For epc, also indicates uuid of destination object.

public:
    explicit Relationship(bool internalTarget = true);
    Relationship(std::string rsTarget, std::string rsType, std::string rsId, bool internalTarget = true);

    Relationship(Relationship&&) = default;
    Relationship(const Relationship&) = default;

    Relationship& operator=(Relationship&&) = default;
    Relationship& operator=(const Relationship&) = default;

    /**
     * Serialize the core property into an XML element embedded into a string.
     */
    std::string toString() const;
    std::ostream& print(std::ostream&) const;

    RelMode relMode() const { return mode_; }
    RelType relType() const { return builtin_type_; }

    bool isInternalTarget() const;
    const std::string& getTarget() const;
    const std::string& getType() const;
    const std::string& getId() const;

    void setMode(RelMode mode);
    void setType(RelType type);
    void setIsExternalTarget();
    void setIsInternalTarget();
    void setTarget(std::string rsTarget);

    /**
     * Set a (possibly arbitrary) type to the relationship
     */
    void setType(std::string rsType);
    void setType(const char* type, size_t type_len);

    // Set predefined types to the relationship
    void setDestinationObjectType()     { setType(RelType::eDestinationObject); }
    void setSourceObjectType()          { setType(RelType::eSourceObject); }
    void setDestinationMediaType()      { setType(RelType::eDestinationMedia); }
    void setSourceMediaType()           { setType(RelType::eSourceMedia); }
    void setChunkedPartType()           { setType(RelType::eChunkedPart); }
    void setExternalPartProxyToMlType() { setType(RelType::eExternalPartProxyToMl); }
    void setMlToExternalPartProxyType() { setType(RelType::eMlToExternalPartProxy); }
    void setExternalResourceType()      { setType(RelType::eExternalResource); }

    /**
     * Set an arbitrary id to the relationship
     */
    void setId(std::string rsId);

    bool equals(const Relationship& rhs, bool ignoreId = false) const;
    bool lessThan(const Relationship& rhs, bool ignoreId = false) const;
};

/****************************************************************************/

struct OES_COMMON_EXPORT RelEqualsIgnoreId {
    bool operator()(const Relationship& lhs, const Relationship& rhs) const {
        return lhs.equals(rhs, true);
    }
};
struct OES_COMMON_EXPORT RelLessThanIgnoreId {
    bool operator()(const Relationship& lhs, const Relationship& rhs) const {
        return lhs.lessThan(rhs, true);
    }
};

/****************************************************************************/

inline bool operator==(const Relationship& lhs, const Relationship& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const Relationship& lhs, const Relationship& rhs) {
    return !lhs.equals(rhs);
}

inline bool operator<(const Relationship& lhs, const Relationship& rhs) {
    return lhs.lessThan(rhs);
}

/****************************************************************************/

} // namespace oes::common::epc

#endif
