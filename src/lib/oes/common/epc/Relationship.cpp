
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

#include "Relationship.h"

#include <cstring>
#include <sstream>
#include <tuple>

namespace oes::common::epc {

/****************************************************************************/

const std::vector<std::string>& allBuiltinRelationTypes() {
    static const std::vector<std::string> builtins{
        "",
        "http://schemas.energistics.org/package/2012/relationships/destinationObject",
        "http://schemas.energistics.org/package/2012/relationships/sourceObject",
        "http://schemas.energistics.org/package/2012/relationships/destinationMedia",
        "http://schemas.energistics.org/package/2012/relationships/sourceMedia",
        "http://schemas.energistics.org/package/2012/relationships/chunkedPart",
        "http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl",
        "http://schemas.energistics.org/package/2012/relationships/mlToExternalPartProxy",
        "http://schemas.energistics.org/package/2012/relationships/externalResource",
    };
    return builtins;
}

const std::string& relationTypeAsString(RelType type) {
    const std::vector<std::string>& builtins = allBuiltinRelationTypes();

    switch (type) {
    case RelType::eUnknownType:
    case RelType::eDestinationObject:
    case RelType::eSourceObject:
    case RelType::eDestinationMedia:
    case RelType::eSourceMedia:
    case RelType::eChunkedPart:
    case RelType::eExternalPartProxyToMl:
    case RelType::eMlToExternalPartProxy:
    case RelType::eExternalResource:
        return builtins[size_t(type)];

    // no default on purpose
    }

    return builtins[size_t(RelType::eUnknownType)];
}

RelType relationTypeFromString(const char* s, size_t n) {
    const auto isBuiltin = [](size_t n, const char* s, RelType type) {
        const auto& builtin = allBuiltinRelationTypes()[size_t(type)];
        return (std::strncmp(s, builtin.c_str(), n) == 0);
    };

    if (!(69 <= n && n < 80)) {
        return RelType::eUnknownType;
    }

    switch (n - 69) {
    case 0:
        if (isBuiltin(n, s, RelType::eSourceMedia)) {
            return RelType::eSourceMedia;
        }
        if (isBuiltin(n, s, RelType::eChunkedPart)) {
            return RelType::eChunkedPart;
        }
        break;

    case 1:
        if (isBuiltin(n, s, RelType::eSourceObject)) {
            return RelType::eSourceObject;
        }
        break;

    case 2:
    case 3:
    case 4:
        break;

    case 5:
        if (isBuiltin(n, s, RelType::eDestinationMedia)) {
            return RelType::eDestinationMedia;
        }
        if (isBuiltin(n, s, RelType::eExternalResource)) {
            return RelType::eExternalResource;
        }
        break;

    case 6:
        if (isBuiltin(n, s, RelType::eDestinationObject)) {
            return RelType::eDestinationObject;
        }
        break;

    case 7:
    case 8:
    case 9:
        break;

    case 10:
        if (isBuiltin(n, s, RelType::eExternalPartProxyToMl)) {
            return RelType::eExternalPartProxyToMl;
        }
        if (isBuiltin(n, s, RelType::eMlToExternalPartProxy)) {
            return RelType::eMlToExternalPartProxy;
        }
        break;
    }

    return RelType::eUnknownType;
}

/****************************************************************************/

Relationship::Relationship(
    bool internalTarget
) : mode_(internalTarget? RelMode::eInternal: RelMode::eExternal)
  , builtin_type_(RelType::eUnknownType)
{
}

Relationship::Relationship(
    std::string rsTarget, std::string rsType, std::string rsId, bool internalTarget
) : mode_(internalTarget? RelMode::eInternal: RelMode::eExternal)
  , target_(std::move(rsTarget))
  , type_(std::move(rsType))
  , id_(std::move(rsId))
{
    if (isdigit(id_[0]) != 0) {
        id_ = '_' + id_;
    }

    // to avoid storing well-known builtin type strings
    setType(std::move(type_));
}

std::string Relationship::toString() const {
    std::ostringstream oss;
    print(oss);
    return oss.str();
}

std::ostream& Relationship::print(std::ostream& os) const {
    os << "<Relationship Id=\"" << id_ << "\" Type=\"" << getType() << "\" Target=\"" << target_ << "\"";
    if (mode_ == RelMode::eExternal) {
        return os << " TargetMode=\"External\"/>";
    } else {
        return os << "/>";
    }
}

bool Relationship::isInternalTarget() const {
    return mode_ == RelMode::eInternal;
}

const std::string& Relationship::getTarget() const {
    return target_;
}

const std::string& Relationship::getType() const {
    if (builtin_type_ != RelType::eUnknownType) {
        return relationTypeAsString(builtin_type_);
    }
    return type_;
}

const std::string& Relationship::getId() const {
    return id_;
}

void Relationship::setMode(RelMode mode) {
    mode_ = mode;
}

void Relationship::setIsExternalTarget() {
    mode_ = RelMode::eExternal;
}

void Relationship::setIsInternalTarget() {
    mode_ = RelMode::eInternal;
}

void Relationship::setTarget(std::string rsTarget) {
    target_ = std::move(rsTarget);
}

void Relationship::setType(std::string rsType) {
    RelType type_enum = relationTypeFromString(rsType);
    if (type_enum != RelType::eUnknownType) {
        return setType(type_enum);
    }
    builtin_type_ = RelType::eUnknownType;
    type_ = std::move(rsType);
}

void Relationship::setType(const char* type, size_t type_len) {
    RelType type_enum = relationTypeFromString(type, type_len);
    if (type_enum != RelType::eUnknownType) {
        return setType(type_enum);
    }
    builtin_type_ = RelType::eUnknownType;
    type_.assign(type, type_len);
}

void Relationship::setType(RelType type) {
    builtin_type_ = type;
    type_.clear();
}

void Relationship::setId(std::string rsId) {
    if (isdigit(rsId[0]) != 0) {
        id_ = '_' + rsId;
    } else {
        id_ = std::move(rsId);
    }
}

bool Relationship::equals(const Relationship& rhs, bool ignoreId) const {
    const auto& lhs = *this;
    if( ignoreId ) {
        return
            std::tie(lhs.mode_, lhs.builtin_type_, lhs.target_, lhs.type_) ==
            std::tie(rhs.mode_, rhs.builtin_type_, rhs.target_, rhs.type_)
        ;
    }
    return
        std::tie(lhs.mode_, lhs.builtin_type_, lhs.target_, lhs.type_, lhs.id_) ==
        std::tie(rhs.mode_, rhs.builtin_type_, rhs.target_, rhs.type_, rhs.id_)
    ;
}

bool Relationship::lessThan(const Relationship& rhs, bool ignoreId) const {
    const auto& lhs = *this;
    if( ignoreId ) {
        return
            std::tie(lhs.mode_, lhs.builtin_type_, lhs.target_, lhs.type_) <
            std::tie(rhs.mode_, rhs.builtin_type_, rhs.target_, rhs.type_)
        ;
    }
    return
        std::tie(lhs.mode_, lhs.builtin_type_, lhs.target_, lhs.type_, lhs.id_) <
        std::tie(rhs.mode_, rhs.builtin_type_, rhs.target_, rhs.type_, rhs.id_)
    ;
}

} // namespace oes::common::epc
