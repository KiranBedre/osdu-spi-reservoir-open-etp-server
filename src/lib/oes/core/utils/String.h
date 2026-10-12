// ============================================================================
// Copyright 2008-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_UTILS_STRING_H
#define OES_CORE_UTILS_STRING_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/fwd.h>

#include <iosfwd>

namespace oes {
namespace core {

class OES_CORE_UTILS_EXPORT StringUtils {
public:
    /*! \name StringList utilities */
    //@{
    static bool push_back_unique(StringList& list, const String& strToPush);
    static void push_back_unique(StringList& list, const StringList& listToPush);
    static bool insert_unique(StringList& list, StringList::iterator position, const String& strToInsert);
    static void insert_unique(StringList& list, StringList::iterator position, const StringList& listToInsert);

    template<typename InputIterator>
    static void push_back_unique(StringList& list, InputIterator first, InputIterator last) {
        for( /**/; first != last; ++first ) {
            push_back_unique(list, *first);
        }
    }

    template<typename InputIterator>
    static void insert_unique(StringList& list, StringList::iterator position, InputIterator first, InputIterator last) {
        StringList::size_type pos_index = position-list.begin();
        for( /**/; first != last; ++first ) {
            if( insert_unique(list, list.begin()+pos_index, *first) ) {
                ++pos_index;
            }
        }
    }
    //@}

    static void split(StringList& list, const String& s, const char* separator, bool compressTokens = false);

    template<typename Container>
    static String join(const Container& c, const String& separator, bool ignoreEmptyStrings = false) {
        String s;
        bool first = true;
        for( typename Container::const_iterator i = c.begin(); i != c.end(); ++i ) {
            if( ignoreEmptyStrings && i->empty() ) {
                continue;
            }

            if( first ) {
                first = false;
                s = *i;
            } else {
                s += separator;
                s += *i;
            }
        }

        return s;
    }

    static const String& nameSpaceSeparator();
    static bool isQualifiedName(const String& name);
    static String qualifiedName(const String& ms, const String& name);
    static String nameSpace(const String& name);
    static String localName(const String& name);
    static void splitName(const String& name, String& ns, String& localName);

    static int caseInsensitiveCompare(const String& s1, const String& s2);

    static int naturalStringCompare(const char* lhs, const char* rhs);
    static int naturalStringCompare(const String& lhs, const String& rhs);
    static int naturalStringCompare(const char* lhs, size_t lhsSize, const char* rhs, size_t rhsSize);

    static String escape(const String& s);
    static String unescape(const String& s);
    static String escapeList(const StringList& list);
    static void unescapeList(const String& s, StringList& list);
    static std::istream& readEscapedString(std::istream& is, String& s);
    static std::istream& readEscapedStringList(std::istream& is, StringList& list);

    static bool toBool(const std::string& text, bool value_on_invalid = false);
    static char toTribool(const std::string& text, bool trim = true);

    static std::string urlEncode(const std::string& s);

    static std::string abbreviate(const std::string& s, const std::size_t keepFront = 4, const std::size_t keepBack = 4, const std::string& replacement = "...");
};

} // namespace core
} // namespace oes


#endif
