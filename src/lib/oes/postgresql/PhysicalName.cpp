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

#include <oes/postgresql/PhysicalName.h>

#include <sstream>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/String.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string.hpp>
#include <boost/functional/hash.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes {
namespace postgresql {
namespace relational {
using namespace oes::core;


/*!
 * \defgroup oes_postgresql_relational Relational Modeling types and utilities.
 * \ingroup oes_postgresql
 */

/*! \file oes/postgresql/PhysicalName.h
 * \ingroup oes_postgresql_relational
 * \brief Allows describing a \em logical model of related types,
 *     and converting that model into a \em physical SQL model implementation.
 *
 * A LogicalSchema is a composite of complex LogicalType%s, named \em entities
 * themselves composites of named and typed fields. Such a \em logical model
 * allows to describe at a conceptual or abstract level most type hierarchies,
 * in such a way that is usable at runtime, therefore working-around the lack
 * runtime introspection of the C++ language. LogicalType%s are divided in
 * SimpleType, modeling a non-composite \em primitive type, and ComplexType
 * modeling composite types, which can have BY_VALUE or BY_REFERENCE semantic.
 *
 * A PhysicalSchema aims to be a Database-agnostic representation of an actual
 * SQL implementation of a LogicalSchema in a given RDBMS. It will often refer
 * to Database-specific SQL types however (like XMLTYPE for Oracle).
 *
 * PhysicalSchemaGenerator is a class which converts a LogicalSchema into a
 * PhysicalSchema. There is no single way to do this conversion however, and
 * there will typically be a custom converter per logical schema instance that
 * best suits the business objectives for that schema (enforcing or not foreign
 * keys, modeling polymorphic foreign keys differently, etc...).
 *
 * DdlScriptWriter is a class which generates a Data-Definition-Language script
 * (i.e. a bunch of CREATE TABLE / VIEW / INDEX / etc... SQL statements) given
 * a PhysicalSchema. Each DDL SQL statement is handled to a DdlScriptHandler,
 * which can just output it as text (DdlScriptStringer) but can also directly
 * execute the statements at runtime as well.
 */

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/
            
/*! \class PhysicalName oes/postgresql/PhysicalName.h
 * \brief PhysicalName represents a name of a DBMS Object.
 *
 * This class wraps a String and some flags to specify if name is case sensitive
 * and/or generated server side.
 *
 * Names with IMPLICIT_UPPERCASE are stored upper case server side, but
 * are typically manipulated in camel case in memory to be more user friendly.
 *
 * Strict case sensitivity names are meant to be identical in memory and in
 * server. In Oracle, such names must be bound in SQL query with double
 * quotes.
 *
 * System generated names means the server has generated a name when creating
 * the object (for instance, Oracle generates index for primary key columns).
 *
 * There are 3 methods to print a PhysicalName:
 * - sqlName is to use for sql statement (except as bind values, for instance owner=:1)
 * - sqlText or sqlLiteral is used when we need to compare 2 names, or to bind value in a statement
 * - getRaw should be rarely used, most of the time it's when one wants to ignore
 * case sensitivity and output all upper-case, for instance.
 *
 * TODO: Properly introduce the notion of sqlDialect in this class,
 * to have an accurate can_skip_quotes_ on any RDBMS, not just Oracle
 */

PhysicalName::PhysicalName(
    const std::string& value,
    NameConversionStrategy conversion_strategy,
    NameOrigin origin
) : value_(value)
  , conversion_strategy_(conversion_strategy)
  , origin_(origin)
  , can_skip_quote_(true)
{
    updateCanSkipQuote();
}

PhysicalName::PhysicalName(const char* value)
    : value_(value)
    , conversion_strategy_(IMPLICIT_UPPERCASE)
    , origin_(USER_GENERATED)
    , can_skip_quote_(true)
{
    updateCanSkipQuote();
}

PhysicalName PhysicalName::asIs(const char* value) {
    return PhysicalName(value, NONE_CASE_SENSITIVE);
}

PhysicalName PhysicalName::asIs(const std::string& value) {
    return PhysicalName(value, NONE_CASE_SENSITIVE);
}

const PhysicalName& PhysicalName::invalid() {
    static PhysicalName invalid_;
    return invalid_;
}

/*!
 * \brief Parses a physical name.
 *
 * \param from_sql double-quoted or not physical name.
 * \return unquoted (if necessary) physical name, with NameConversionStrategy
 *     of NONE_CASE_SENSITIVE if double-quoted, and IMPLICIT_UPPERCASE otherwise.
 *     invalid() on empty strings.
 */
PhysicalName PhysicalName::parse(const std::string& from_sql, NameConversionStrategy default_case) {
    if (from_sql.empty()) {
        return invalid();
    }

    if (from_sql[0] == '"') {
        OES_ASSERT(from_sql.size() > 2 && from_sql[from_sql.size() - 1] == '"', return invalid(););
        PhysicalName pname(
            from_sql.substr(1, from_sql.size() - 2), // unquote
            NONE_CASE_SENSITIVE
        );
        pname.updateCanSkipQuote(default_case);
        return pname;
    }

    return PhysicalName(from_sql, default_case);
}

void PhysicalName::reset(
    const std::string& value,
    NameConversionStrategy conversion_strategy,
    NameOrigin origin
) {
    value_ = value;
    conversion_strategy_ = conversion_strategy;
    origin_ = origin;
    updateCanSkipQuote();
}

void PhysicalName::clear() {
    value_.clear();
    can_skip_quote_ = true;
}

PhysicalName& PhysicalName::operator+=(const std::string& value) {
    value_ += value;
    updateCanSkipQuote();
    return *this;
}

PhysicalName PhysicalName::operator+(const std::string& value) const {
    PhysicalName result(*this);
    result += value;
    return result;
}

PhysicalName PhysicalName::operator+(const char* value) const {
    PhysicalName result(*this);
    result += value;
    return result;
}

bool PhysicalName::operator==(const std::string& value) const {
    OES_DEBUG_ASSERT(origin_ != DBMS_GENERATED);
    return (value_ == value) /*optim*/ || (*this == PhysicalName(value));
}

bool PhysicalName::operator==(const char* value) const {
    OES_DEBUG_ASSERT(origin_ != DBMS_GENERATED);
    return (value_ == value) /*optim*/ || (*this == PhysicalName(value));
}

int PhysicalName::compare(const PhysicalName& other) const {
    if (value_.empty() || other.value_.empty()) {
        return static_cast<int>(value_.length()) - static_cast<int>(other.value_.length());
    }

    if (this->conversion_strategy_ == other.conversion_strategy_) {
        switch (conversion_strategy_) {
        case NONE_CASE_SENSITIVE:
            return this->value_.compare(other.value_);
        case IMPLICIT_UPPERCASE:
        case IMPLICIT_LOWERCASE:
            return StringUtils::caseInsensitiveCompare(value_, other.value_);
        }
    }

    // Note : to simplify the code, we could have decided to duplicate the
    //   converted value as string in this PhysicalName, as member of the class.
    //   A trial has been done (review#43663) but was a little bit less
    //   efficient.
    if (
        (conversion_strategy_ == IMPLICIT_UPPERCASE) &&
        (other.conversion_strategy_ == NONE_CASE_SENSITIVE)
    ) {
        // copy _stricmp, but only left part is set to lower case
        const char* dst = value_.c_str();
        const char* src = other.value_.c_str();
        int f, l;
        do
        {
            if (((f = (unsigned char)(*(dst++))) >= 'a') && (f <= 'z'))
                f += 'A' - 'a'; // upper case it
            l = (unsigned char)(*(src++));
        } while (f && (f == l));
        return (f - l);
    }
    if (
        (conversion_strategy_ == NONE_CASE_SENSITIVE) &&
        (other.conversion_strategy_ == IMPLICIT_UPPERCASE)
    ) {
        // copy _stricmp, but only right part is set to lower case
        const char* dst = value_.c_str();
        const char* src = other.value_.c_str();
        int f, l;
        do
        {
            f = (unsigned char)(*(dst++));
            if (((l = (unsigned char)(*(src++))) >= 'a') && (l <= 'z'))
                l += 'A' - 'a'; // upper case it
        } while (f && (f == l));
        return (f - l);
    }
    if (
        (conversion_strategy_ == IMPLICIT_LOWERCASE) &&
        (other.conversion_strategy_ == NONE_CASE_SENSITIVE)
    ) {
        // copy _stricmp, but only left part is set to lower case
        const char* dst = value_.c_str();
        const char* src = other.value_.c_str();
        int f, l;
        do
        {
            if (((f = (unsigned char)(*(dst++))) >= 'A') && (f <= 'Z'))
                f += 'a' - 'A'; // lower case it
            l = (unsigned char)(*(src++));
        } while (f && (f == l));
        return (f - l);
    }
    if (
        (conversion_strategy_ == NONE_CASE_SENSITIVE) &&
        (other.conversion_strategy_ == IMPLICIT_LOWERCASE)
    ) {
        // copy _stricmp, but only right part is set to lower case
        const char* dst = value_.c_str();
        const char* src = other.value_.c_str();
        int f, l;
        do
        {
            f = (unsigned char)(*(dst++));
            if (((l = (unsigned char)(*(src++))) >= 'A') && (l <= 'Z'))
                l += 'a' - 'A'; // lower case it
        } while (f && (f == l));
        return (f - l);
    }

    return sqlText().compare(other.sqlText());
}

bool PhysicalName::operator==(const PhysicalName& other) const {
    if (origin_ != other.origin_) {
        // early exit: a generated name cannot be equal to user defined one
        // (or if it does, it's pure hazard)

        // About the case both name are system generated :
        //    - we are not supposed to compare 2 generated names. When
        // comparing 2 remote schema, the object names are supposed to be
        // ignored, they do not matter (index or constraint can be equal
        // if name are different)
        //    - on the other hand, PhysicalName can be used in container (vector,
        // set,...) and lookup should not fail in that case.
        return false;
    }

    return this->compare(other) == 0;
}

bool PhysicalName::operator!=(const PhysicalName& other) const {
    return this->compare(other) != 0;
}

bool PhysicalName::operator<(const PhysicalName& other) const {
    return this->compare(other) < 0;
}

bool PhysicalName::operator>(const PhysicalName& other) const {
    return other < *this;
}

bool PhysicalName::empty() const {
    return value_.empty();
}

size_t PhysicalName::length() const {
    return value_.length();
}

/*!
 * \brief Determines whether this name is valid or not.
 *
 * \return true if not empty, and does not contain double-quote (")
 *     or embedded NULL (\0) characters; false otherwise.
 *
 * \warning Does not check the length is 30 characters or less for Oracle.
 */
bool PhysicalName::isValid() const {
    return !empty() &&
        value_.find('"')  == std::string::npos &&
        value_.find('\0') == std::string::npos
    ;
}

/*!
 * \brief Gets the value of this name usable in SQL query as a value.
 * \return getRaw() when noConversion;
 * Upper-cased or lower-case name when IMPLICIT_UPPERCASE or IMPLICIT_LOWERCASE, respectively.
 *
 * Example:
 * \code
 * "select name from users where name like '%" + name.sqlText() + "%'";
 * \endcode
 */
std::string PhysicalName::sqlText() const {
    switch (conversion_strategy_) {
    case NONE_CASE_SENSITIVE:
        return value_;
    case IMPLICIT_UPPERCASE:
        return boost::to_upper_copy(value_);
    case IMPLICIT_LOWERCASE:
        return boost::to_lower_copy(value_);
    }
    return value_;
}

/*!
 * \brief Gets the name surrounded by single quote.
 *
 * Example:
 * \code
 * "select name from users where name = " + name.sqlLiteral();
 * \endcode
 */
std::string PhysicalName::sqlLiteral() const {
    const auto& text = sqlText();
    if (text.find('\'') != std::string::npos) {
        return "q'[" + text + "]'";
    }
    return "'" + text + "'";
}

/*!
 * Gets this name, without case conversion.
 */
const std::string& PhysicalName::getRaw() const {
    return value_;
}

/*!
 * Gets this name, in a form that can be used in SQL statement.
 * Adds quotes if no conversion should be done.
 *
 * Example:
 * \code
 * "select " + name.sqlName() + " from users";
 * \endcode
 */
std::string PhysicalName::sqlName() const {
    // TODO : add [] for conversion preference (SQL server)

    // TODO : try to always output with double quotes. That way a
    //    PhysicalName("foo", IMPLICIT_UPPERCASE).sqlName() ==
    //    PhysicalName("FOO", NONE_CASE_SENSITIVE).sqlName()
    //    and that would make some code simpler (operators == and <, ...)

    if (can_skip_quote_) {
        return value_;
    }
    return '"' + sqlText() + '"';
}

/*!
 * Gets this name with double quotes.
 * e.g. "MyTable"
 */
std::string PhysicalName::quotedName() const {
    return '"' + sqlText() + '"';
}

/*!
 * Gets this name with simple quotes.
 * e.g. 'MyTable'
 */
std::string PhysicalName::singleQuotedName() const {
    return '\'' + sqlText() + '\'';
}

namespace {

bool oraNameNeedsQuotes(const std::string& oraName, bool noConversion) {
    // rules come from Oracle: see http://goo.gl/222AEj
    if (oraName.empty()) {
        return false;
    }
    // Nonquoted identifiers must begin with an alphabetic character.
    if (!std::isalpha(oraName[0])) {
        return true;
    }
    // Nonquoted identifiers can contain only alphanumeric characters and the
    // underscore(_), dollar sign($), and pound sign(#).
    for (char c : oraName) {
        if (!std::isalnum(c) && c != '$' && c != '_' && c != '#') {
            return true;
        }
        // cannot contain double quotation marks or the null character (\0).
        OES_DEBUG_ASSERT(c != '"' && c != '\0');
    }

    if (noConversion) {
        return !(boost::to_upper_copy(oraName) == oraName);
    }

    return false;
}

bool pgNameNeedsQuotes(const std::string& pgName, bool noConversion) {
    // TODO: PostgreSQL rules. See https://www.postgresql.org/docs/current/static/sql-syntax-lexical.html
    if (pgName.empty()) {
        return false;
    }
    // Nonquoted identifiers must begin with an alphabetic character or an underscore
    if (!std::isalpha(pgName[0]) && pgName[0] != '_' ) {
        return true;
    }
    // Nonquoted identifiers can contain only alphanumeric characters, the
    // underscore(_), and the dollar sign($).
    for (char c : pgName) {
        if (!std::isalnum(c) && c != '$' && c != '_') {
            return true;
        }
        // cannot contain double quotation marks or the null character (\0).
        OES_DEBUG_ASSERT(c != '"' && c != '\0');
    }
    // PostgreSQL uses quotes whenever the . character is found in a name
    // TODO: PostgreSQL is supporting non-quoted '$' character. (See above)
    if (pgName.find('$') != std::string::npos || pgName.find('.') != std::string::npos) {
        return true;
    }

    if (noConversion) {
        return !(boost::to_lower_copy(pgName) == pgName);
    }

    return false;
}

//! PostgreSQL uses quotes whenever the $ character is found in a name
std::string pgSqlName(const PhysicalName& pname) {
    return pgNameNeedsQuotes(pname.getRaw(), pname.noConversion())
        ? pname.quotedName()
        : pname.sqlName();
}

//! PostgreSQL uses quotes whenever the $ character is found in a name
std::string pgSqlText(const PhysicalName& pname) {
    return pgNameNeedsQuotes(pname.getRaw(), pname.noConversion())
        ? pname.quotedName()
        : pname.sqlText();
}
}

std::string PhysicalName::pqSqlName() const {
    return pgSqlName(*this);
}

std::string PhysicalName::pqSqlText() const {
    return pgSqlText(*this);
}

/*!
 * Checks the name can be used in SQL statement without quotes.
 *
 * If the name contains minor case but is noConversion(), or if
 * it contains special characters or does not start with an alphabetic char,
 * returns false; true otherwise.
 *
 */
bool PhysicalName::canSkipQuote() const {
    return can_skip_quote_;
}

void PhysicalName::updateCanSkipQuote(NameConversionStrategy alt_ncs) {
    NameConversionStrategy ncs = conversion_strategy_;
    if (ncs == NONE_CASE_SENSITIVE && alt_ncs != NONE_CASE_SENSITIVE) {
        ncs = alt_ncs;
    }

    switch (ncs) {
    case IMPLICIT_LOWERCASE:
        can_skip_quote_ = !pgNameNeedsQuotes(value_, noConversion());
        break;
    case IMPLICIT_UPPERCASE:
        can_skip_quote_ = !oraNameNeedsQuotes(value_, noConversion());
        break;
    case NONE_CASE_SENSITIVE: // be conservative?
        // Do as before (Oracle only) for backward compatibility.
        can_skip_quote_ = !oraNameNeedsQuotes(value_, noConversion());
        break;
        // no default on purpose
    }
}

NameConversionStrategy PhysicalName::conversionStrategy() const {
    return conversion_strategy_;
}

bool PhysicalName::noConversion() const {
    return conversion_strategy_ == NONE_CASE_SENSITIVE;
}

bool PhysicalName::implicitUppercase() const {
    return conversion_strategy_ == IMPLICIT_UPPERCASE;
}

NameOrigin PhysicalName::origin() const {
    return origin_;
}

bool PhysicalName::isUserGenerated() const {
    return origin_ == USER_GENERATED;
}

void PhysicalName::write(JSONWriter& json) const {
    json.push_object();
    json.key("raw").string(getRaw());
    std::string xform;
    switch (conversion_strategy_) {
    case NONE_CASE_SENSITIVE:
        xform = "none"; break;
    case IMPLICIT_UPPERCASE:
        xform = "upper"; break;
    case IMPLICIT_LOWERCASE:
        xform = "lower"; break;
    }
    json.key("xform").string(xform);
    json.key("origin").string(isUserGenerated()? "user": "dbms");
    json.pop_object();
}

bool PhysicalName::parse(JSONReader& json, PhysicalName& value) {
    OES_ASSERT(json.next() == JSON_OBJECT_BEGIN, return false;);

    std::string raw;
    NameConversionStrategy xform = IMPLICIT_UPPERCASE;
    NameOrigin origin = USER_GENERATED;

    for (int i = 0; i < 3 && json.current() != JSON_OBJECT_END; ++i) {
        OES_ASSERT(json.next() == JSON_OBJECT_KEY, return false;);
        if (json.is_key("raw")) {
            OES_ASSERT(raw.empty(), return false;);
            json.next();
            std::string val = json.get_string();
            raw.swap(val);
        } else if (json.is_key("xform")) {
            json.next();
            std::string val = json.get_string();
            if (val == "none") {
                xform = NONE_CASE_SENSITIVE;
            } else if (val == "upper") {
                xform = IMPLICIT_UPPERCASE;
            } else if (val == "lower") {
                xform = IMPLICIT_LOWERCASE;
            }else {
                return false;
            }
        } else if (json.is_key("origin")) {
            json.next();
            std::string val = json.get_string();
            if (val == "user") {
                origin = USER_GENERATED;
            } else if (val == "dbms") {
                origin = DBMS_GENERATED;
            } else {
                return false;
            }
        } else {
            return false; // unexpected key
        }
    }
    const bool ok = json.next() == JSON_OBJECT_END;
    if (ok) {
        OES_ASSERT(!raw.empty(), return false;);

        value.value_ = raw;
        value.conversion_strategy_ = xform;
        value.origin_ = origin;
        value.updateCanSkipQuote();
    }
    return ok;
}

/*!
 * Writes in \p os this name, adding quotes if case sensitivity should be protected.
 */
std::ostream& PhysicalName::dump(std::ostream& os, bool as_json) const {
    if (as_json) {
        JSONWriter json(os);
        write(json);
        return os;
    }

    // TODO: should be able to decide whether to add quote or upper-case
    if (noConversion()) {
        return os << '"' << value_ << '"';
    }
    return os << value_;
}

std::size_t hash_value(const PhysicalName& name) {
    return boost::hash_value(std::make_pair(
        static_cast<int>(name.origin()), name.sqlText()
    ));
}

std::string join(
    const PhysicalNameVec& v,
    const std::function<std::string(const PhysicalName&)>& to_str = nullptr
) {
    bool first = true;
    std::ostringstream oss;
    for (const auto& name : v) {
        if (first) {
            first = false;
        } else {
            oss << ',';
        }
        oss << (to_str ? to_str(name) : name.getRaw());
    }
    return oss.str();
}

std::string joinAsLiterals(const PhysicalNameVec& v) {
    return relational::join(v, [](const PhysicalName& name){ return name.sqlLiteral(); });
}

std::string joinAsNames(const PhysicalNameVec& v) {
    return relational::join(v, [](const PhysicalName& name) { return name.sqlName(); });
}

std::string joinAsTexts(const PhysicalNameVec& v) {
    return relational::join(v, [](const PhysicalName& name) { return name.sqlText(); });
}


} // namespace relational
} // namespace postgresql
} // namespace oes
