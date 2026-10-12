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

#ifndef oes_postgresql_physicalname_h
#define oes_postgresql_physicalname_h

#include <oes/postgresql/export.h>

#include <oes/postgresql/fwd.h>
#include <oes/core/utils/Json.h>


namespace oes {
namespace postgresql {
namespace relational {

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

enum NameConversionStrategy {
    NONE_CASE_SENSITIVE,
    IMPLICIT_UPPERCASE,  // i.e. Oracle
    IMPLICIT_LOWERCASE   // i.e. PostgreSQL
};

enum NameOrigin {
    DBMS_GENERATED,
    USER_GENERATED
};

class OES_POSTGRESQL_EXPORT PhysicalName {
public:
    explicit PhysicalName(
        const std::string& value = std::string(),
        NameConversionStrategy conversion_strategy = IMPLICIT_UPPERCASE,
        NameOrigin origin = USER_GENERATED
    );
    explicit PhysicalName(const char* value); // same as ctor above

    static PhysicalName asIs(const char* value);
    static PhysicalName asIs(const std::string& value);

    void reset(
        const std::string& value = std::string(),
        NameConversionStrategy conversion_strategy = IMPLICIT_UPPERCASE,
        NameOrigin origin = USER_GENERATED
    );
    void clear(); // same as reset, for compatibility with std::string

    static const PhysicalName& invalid();
    static PhysicalName parse(
        const std::string& from_sql,
        NameConversionStrategy default_case = IMPLICIT_UPPERCASE
    );

    PhysicalName& operator+=(const std::string& value);
    PhysicalName operator+(const std::string& value) const;
    PhysicalName operator+(const char* value) const;

    int compare(const PhysicalName& other) const;

    bool operator==(const char* value) const;
    bool operator==(const std::string& value) const;
    bool operator==(const PhysicalName& other) const;

    bool operator!=(const PhysicalName& other) const;

    bool operator<(const PhysicalName& other) const;
    bool operator>(const PhysicalName& other) const;

    bool isValid() const;
    bool empty() const;
    size_t length() const;

    std::string sqlText() const;
    std::string sqlLiteral() const;
    const std::string& getRaw() const;
    std::string sqlName() const;
    std::string quotedName() const;
    std::string singleQuotedName() const;
    std::string pqSqlName() const;
    std::string pqSqlText() const;

    bool noConversion() const;
    bool implicitUppercase() const;
    NameConversionStrategy conversionStrategy() const;

    NameOrigin origin() const;
    bool isUserGenerated() const;

    void write(oes::core::JSONWriter& json) const;
    std::ostream& dump(std::ostream& os, bool as_json = false) const;
    static bool parse(oes::core::JSONReader& json, PhysicalName& value);

    bool canSkipQuote() const;

private:
    void updateCanSkipQuote(NameConversionStrategy alt_ncs = NONE_CASE_SENSITIVE);

private:
    std::string value_;
    NameConversionStrategy conversion_strategy_;
    NameOrigin origin_;
    bool can_skip_quote_;
};

inline bool operator==(const char* left, const PhysicalName& right) {
    return right == left;
}
inline bool operator==(const std::string& left, const PhysicalName& right) {
    return right == left;
}
inline PhysicalName operator+(const std::string& left, const PhysicalName& right) {
    return PhysicalName(left, right.conversionStrategy(), right.origin()) + right.getRaw();
}

inline void json_write(oes::core::JSONWriter& json, const PhysicalName& name) {
    name.write(json);
}
inline bool json_parse(oes::core::JSONReader& json, PhysicalName& name) {
    return PhysicalName::parse(json, name);
}

// warning: use this method only for user friendly output. For SQL query, use
//    either sqlText(), sqlLiteral() or sqlName()
inline std::ostream& operator<<(std::ostream& os, const PhysicalName& name) {
    return name.dump(os);
}

OES_POSTGRESQL_EXPORT std::size_t hash_value(const PhysicalName& name);
OES_POSTGRESQL_EXPORT std::string joinAsLiterals(const PhysicalNameVec& v);
OES_POSTGRESQL_EXPORT std::string joinAsNames(const PhysicalNameVec& v);
OES_POSTGRESQL_EXPORT std::string joinAsTexts(const PhysicalNameVec& v);


} // namespace relational
} // namespace postgresql
} // namespace oes

#endif
