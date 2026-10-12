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

#include "EmlUri.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <sstream>
#include <tuple>
#include <iomanip>
#include <stdexcept>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/io/ios_state.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

// Control handling of pre-ETP 1.2 RC2 URIs
bool parseLegacyUris__ = true;  // I.e. accept OLD or NEW URIs.
bool printLegacyUris__ = false; // Printf NEW URIs only.

// Finds \a c character in \a s, walking the string backwards,
// starting at \a from until \a up_to included.
// \return position of character \a c in \a s if found; std::string::npos otherwise.
size_t reverseFind(const char* s, char c, size_t from, size_t up_to = 0) {
    for (; (from > up_to) && (s[from] != c); --from);
    if ((from == up_to) && s[from] != c) {
        return std::string::npos;
    }
    return from;
}

void percentDecodeInPlace(std::string& s) {
    std::ostringstream oss;
    oes::common::etp12::EmlUri::percentDecode(oss, s);
    s = oss.str();
}

template <size_t N>
bool findAndConsumePrefix(const char (&key)[N], const char* s, size_t len, size_t& c) {
    bool has_key = (len - c) > (N - 1) && std::strncmp(s + c, key, (N - 1)) == 0;
    if (has_key) {
        c += (N - 1);
    }
    return has_key;
}

} // namespace

namespace oes::common::etp12 {

EmlUri EmlUri::parse(std::string uri) {
    percentDecodeInPlace(uri);

    EmlUri eml;
    const size_t len = uri.size();
    const char*const s = uri.c_str();

    if (len < 5 || std::strncmp(s, "eml:/", 5) != 0) {
        return eml; // invalid
    }

    if (len == 5) {
        eml.type_ = eDATA_SPACE;
        return eml; // the "root" URI
    }

    size_t c = 5;
    if (std::strncmp(s+c, "//", 2) == 0) {
        c += 2;
    }

    bool legacy = false;
    bool has_dataspace = findAndConsumePrefix("dataspace(", s, len, c);
    if (has_dataspace) {
        if (s[c] != '\'') {
            if (!parseLegacyUris__) {
                return EmlUri(); // dataspace must start with single-quote
            }
            legacy = true;
            int parentheses = 1;
            std::ostringstream osdataspace;
            while (c < len) {
                if (s[c] == ')') {
                    parentheses--;
                    if (parentheses == 0) {
                        c++;
                        break;
                    }
                } else if (s[c] == '(') {
                    parentheses++; // allow nested but balanced parenthese
                }
                osdataspace << s[c];
                c++;
            }
            eml.dataspace_ = osdataspace.str();
        } else {
            assert(s[c] == '\'');
            ++c; // consume opening single quote
            const char* space_begin = s + c;
            const char* space_end = std::strchr(space_begin, '\'');
            if (!space_end) {
                return EmlUri(); // dataspace must end with single-quote
            }
            if (space_end[1] != ')') {
                return EmlUri(); // dataspace must have close-paren
            }
            eml.dataspace_.assign(space_begin, space_end - space_begin);
            c += eml.dataspace_.size();
            ++c; // consume closing single quote
            ++c; // consume closing parenthesis
        }
    }

    if (c==len) {
        /// URI is eml:///dataspace(...)
        eml.type_ = eDATA_SPACE;
        return eml;
    }

    if (has_dataspace){
        if (s[c] != '/') {
            // / is needed to separate dataspace from type
            return EmlUri();
        }
        c++;
    }

    std::ostringstream osprotocol;
    while (c < len) {
        if (s[c] == '.') {
            c++;
            break;
        } else {
            osprotocol << s[c];
            c++;
        }
    }
    const std::string ml = osprotocol.str();
    eml.ml_ = parseML(ml.c_str(), ml.size());

    const char* type_begin = s + c;
    const char* type_end = std::strchr(type_begin, '(');
    if (!type_end) {
        eml.obj_type_.assign(type_begin, s + len - type_begin);
        eml.type_ = EmlUri::eOBJ_TYPE;
        return eml;
    }

    eml.obj_type_.assign(type_begin, type_end - type_begin);
    c += eml.obj_type_.size();
    c++; // consume uuid+version opening paren

    if (uri.back() != ')') {
        return EmlUri(); // missing closing paren
    }

    const bool has_uuid_key = findAndConsumePrefix("uuid=", s, len, c);
    if (has_uuid_key) {
        if (legacy) {
            return EmlUri(); // Cannot mix NEW and OLD format
        }
    }

    if (len < (c + 37) || s[len-1] != ')') {
        // Guid does not have the dash format so invalid
        return EmlUri();
    }
    eml.guid_.reparse(s + c, 36, false);
    eml.type_ = EmlUri::eOBJ_INSTANCE;
    c += 36;

    if (s[c] == ',') {
        c++;

        const bool has_version_key = findAndConsumePrefix("version='", s, len, c);
        if (!has_version_key) {
            if (!parseLegacyUris__) {
                return EmlUri(); // version must be prefixed with version='
            }

            if (has_uuid_key) {
                return EmlUri(); // Cannot mix NEW and OLD format
            }
            if (has_dataspace && !legacy) {
                return EmlUri(); // Cannot mix NEW and OLD format
            }

            // legacy parse
            const char* version_begin = s + c;
            const char* version_end = std::strchr(version_begin, ')');
            if (!version_end) {
                return EmlUri(); // version must end with single-quote
            }
            eml.version_.assign(version_begin, version_end - version_begin);
            c += eml.version_.size();
        } else {
            if (!has_uuid_key) {
                return EmlUri(); // Cannot mix NEW and OLD format
            }
            if (has_dataspace && legacy) {
                return EmlUri(); // Cannot mix NEW and OLD format
            }

            const char* version_begin = s + c;
            const char* version_end = std::strchr(version_begin, '\'');
            if (!version_end) {
                return EmlUri(); // version must end with single-quote
            }
            eml.version_.assign(version_begin, version_end - version_begin);
            c += eml.version_.size();
            ++c; // consume single-quote
        }
    } else if (s[c] != ')' || c != (len - 1)) {
        // No version, we should encounter closing parenthesis
        return EmlUri();
    } else if (has_uuid_key) {
        return EmlUri(); // should not have uuid= if no version
    }

    assert(c == (len - 1) && s[c] == ')');
    return eml;
}

EmlUri EmlUri::parse(const char* curi, size_t len) {
    if (!curi || !len || *curi != 'e') {
        return EmlUri();
    }
    len = (len == std::string::npos)? std::strlen(curi): len;
    return parse(std::string(curi, len));
}

EmlUri::ML EmlUri::parseML(const char* ml, size_t len) {
    if (!ml || !len) {
        return EmlUri::eNO_ML;
    }
    switch (len) {
    case  5: 
        if (0 == strncmp(ml, "eml20", 5)) return eEML20;
        if (0 == strncmp(ml, "eml23", 5)) return eEML23;
        break;
    case  8: 
        if (0 == strncmp(ml, "resqml20", 8)) return eRESQML20;
        if (0 == strncmp(ml, "resqml22", 8)) return eRESQML22;
        if (0 == strncmp(ml, "witsml21", 8)) return eWITSML21;
        if (0 == strncmp(ml, "prodml22", 8)) return ePRODML22;
        break;
    }
    return eUNKNOWN_ML;
}

EmlUri EmlUri::make_space(std::string dataspace) {
    EmlUri uri;
    uri.dataspace_ = std::move(dataspace);
    uri.type_ = eDATA_SPACE;
    return uri;
}

EmlUri EmlUri::make_type(
    std::string dataspace, std::string obj_type, ML ml
) {
    EmlUri uri;
    uri.dataspace_ = std::move(dataspace);
    uri.ml_ = ml;
    uri.type_ = eOBJ_TYPE;
    uri.obj_type_ = std::move(obj_type);
    return uri;
}

EmlUri EmlUri::make_obj(
    std::string dataspace, std::string obj_type,
    oes::core::Guid guid, EmlUri::ML ml, std::string version
) {
    EmlUri uri;
    uri.dataspace_ = std::move(dataspace);
    uri.ml_ = ml;
    uri.type_ = eOBJ_INSTANCE;
    uri.obj_type_= std::move(obj_type);
    uri.guid_ = guid;
    uri.version_ = std::move(version);
    return uri;
}

bool EmlUri::isValid() const {
    return type_ != eNO_TYPE;
}

bool EmlUri::isRoot() const {
    return type_ == eDATA_SPACE && dataspace_.empty();
}

EmlUri EmlUri::child_space(const std::string& dataspace) const {
    if (isRoot()) {
        return EmlUri::make_space(dataspace);
    }
    if (type_ == eDATA_SPACE) {
        return EmlUri::make_space(dataspace_ + '/' + dataspace);
    }
    throw std::runtime_error("Expected eDATA_SPACE EmlUri");
}

EmlUri EmlUri::child_type(std::string obj_type, ML ml) const {
    if (type_ == eDATA_SPACE) {
        return EmlUri::make_type(dataspace_, std::move(obj_type), ml);
    }
    throw std::runtime_error("Expected eDATA_SPACE EmlUri");
}

EmlUri EmlUri::child_obj(oes::core::Guid guid) const {
    if (type_ == eOBJ_TYPE) {
        return EmlUri::make_obj(dataspace_, obj_type_, guid, ml_);
    }
    throw std::runtime_error("Expected eOBJ_TYPE EmlUri");
}

size_t EmlUri::dataspacePartCount() const {
    switch (dataspace_.size()) {
    case  0: return 0;
    case  1:
    case  2: return 1; // since "a/b" is at least 3 chars
    default: return 1 + std::count(dataspace_.begin(), dataspace_.end(), '/');
    }
}

std::string EmlUri::dataspaceName() const {
    switch (dataspacePartCount()) {
    case  0:
    case  1: return dataspace_;
    default:
        return dataspace_.substr(
            1 + reverseFind(dataspace_.c_str(), '/', dataspace_.size() - 1, 1)
        );
    }
}

std::string EmlUri::dataspaceFolder() const {
    switch (dataspacePartCount()) {
    case  0:
    case  1: return std::string();
    default:
        return dataspace_.substr(
            0,
            reverseFind(dataspace_.c_str(), '/', dataspace_.size() - 1, 1)
        );
    }
}

void EmlUri::swap(EmlUri& other) {
    dataspace_.swap(other.dataspace_);
    obj_type_.swap(other.obj_type_);
    std::swap(guid_, other.guid_);
    std::swap(ml_, other.ml_);
    std::swap(version_, other.version_);
    std::swap(type_, other.type_);
}

bool EmlUri::equals(const EmlUri& other) const {
    return // does ml_ really matter? maybe not...
        std::tie(this->dataspace_, this->ml_, this->obj_type_, this->guid_, this->version_) ==
        std::tie(other.dataspace_, other.ml_, other.obj_type_, other.guid_, other.version_)
    ;
}
bool EmlUri::lessThan(const EmlUri& other) const {
    return
        std::tie(this->dataspace_, this->ml_, this->obj_type_, this->guid_, this->version_) <
        std::tie(other.dataspace_, other.ml_, other.obj_type_, other.guid_, other.version_)
    ;
}

std::ostream& EmlUri::print(std::ostream& os) const {
    switch (type_) {
    case eNO_TYPE:
        return os; // nothing? something else?

    case eDATA_SPACE:
        return printSpace(os, dataspace_);

    case eOBJ_TYPE:
        return printML(os) << '.' << obj_type_;

    case eOBJ_INSTANCE:
        printML(os) << '.';
        percentEncode(os, obj_type_);
        if (!hasVersion()) {
            return os << '(' << guid_.toXmlString() << ')';
        } else if (printLegacyUris__) {
            os << '(' << guid_.toXmlString() << ',';
            return percentEncode(os, version_) << ')';
        } else {
            os << "(uuid=" << guid_.toXmlString() << ",version='";
            return percentEncode(os, version_) << "')";
        }
    }

    assert(false); // impossible
    return os;
}

std::ostream& EmlUri::printML(std::ostream& os) const {
    if (defaultDataspace()) {
        return os << "eml:///" << ml_;
    }
    return printSpace(os, dataspace_) << '/' << ml_;
}

std::ostream& EmlUri::printML(std::ostream& os, EmlUri::ML ml) {
    switch (ml) {
    case eNO_ML:        return os;
    case eEML20:        return os << "eml20";
    case eEML23:        return os << "eml23";
    case eRESQML20:     return os << "resqml20";
    case eRESQML22:     return os << "resqml22";
    case eWITSML21:     return os << "witsml21";
    case ePRODML22:     return os << "prodml22";
    case eUNKNOWN_ML:   return os << "unknownML";
    }
    assert(false); // impossible
    return os;
}

std::ostream& EmlUri::printSpace(std::ostream& os, const std::string& space) {
    os << "eml:///";
    if (space.empty()) {
        return os;
    }
    if (printLegacyUris__) {
        os << "dataspace(";
        return percentEncode(os, space) << ')';
    }
    os << "dataspace('";
    return percentEncode(os, space) << "')";
}

std::string EmlUri::to_string() const {
    std::ostringstream oss;
    oss << *this;
    return oss.str();
}

/*!
 * \brief Get the ETP 1.2 compatible Resource or DataObject type.
 * \return for example, "resqml20.obj_MdDatum", or "eml20.obj_EpcExternalPartReference".
 */
std::string EmlUri::to_type() const {
    switch (type_) {
    case eOBJ_TYPE:
    case eOBJ_INSTANCE:
        break;
    default:
        throw std::runtime_error("Expected eOBJ_TYPE or eOBJ_INSTANCE EmlUri");
    }
    std::ostringstream oss;
    printML(oss, ml_) << '.' << obj_type_;
    return oss.str();
}

std::ostream& EmlUri::percentEncode(std::ostream& os, const std::string& s) {
    boost::io::ios_flags_saver ifs1(os);
    boost::io::ios_fill_saver  ifs2(os);

    os.fill('0');
    os << std::hex;

    for (const char c : s) {
        const int cp = static_cast<unsigned char>(c);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(cp) ||
            c == ';' || c == ',' || c == '/' || c == '?' ||
            c == ':' || c == '@' || c == '&' || c == '=' ||
            c == '+' || c == '$' || c == '-' || c == '_' ||
            c == '.' || c == '!' || c == '~' || c == '*' ||
            c == '`' || c == '(' || c == ')' || c == '#'
        ) {
            os.write(&c, 1);
            continue;
        }

        // Any other characters are percent-encoded.
        // Includes UTF-8 multi-byte encoded codepoints
        os << '%' << std::setw(2) << cp;
    }

    return os;
}

std::ostream& EmlUri::percentDecode(std::ostream& os, const std::string& str) {
    const size_t n = str.size();
    if (n < 3) {
        return os << str; // as-is
    }

    const char* s = str.data();
    const char*const end = s + n;
    for (; s < end; ) {
        const char c = *s;
        if (c != '%') {
            os.write(s, 1);
            ++s;
            continue;
        }
        if (s + 2 >= end) {
            return os; // truncated encoding...
        }
        char* num_end = nullptr;
        const char num[] = { /* s[0] == '%' */ s[1], s[2], '\0' };
        const unsigned long cp = std::strtoul(num, &num_end, 16);
        assert(num_end == (num + 2));
        const char decoded = static_cast<char>(cp);
        os.write(&decoded, 1);
        s += 3;
    }

    return os;
}

// Control handling of pre-ETP 1.2 RC2 URIs
bool EmlUri::parseLegacyUris(bool flag) {
    std::swap(parseLegacyUris__, flag);
    return flag;
}
bool EmlUri::printLegacyUris(bool flag)  {
    std::swap(printLegacyUris__, flag);
    return flag;
}

EmlUri::ScopedLegacyFlags::ScopedLegacyFlags(
    bool parse, bool print
) : parse_(parseLegacyUris(parse))
  , print_(printLegacyUris(print))
{
}

EmlUri::ScopedLegacyFlags::~ScopedLegacyFlags() {
    parseLegacyUris__ = parse_;
    printLegacyUris__ = print_;
}

} // namespace oes::common::etp12
