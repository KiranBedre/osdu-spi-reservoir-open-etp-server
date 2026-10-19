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

#include <oes/core/utils/String.h>
#include <oes/core/utils/Assert.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <regex>
#include <istream>

/*! \file oes/core/utils/String.h
 * \brief String definitions and utilities.
 * \ingroup oes_core_utils
 */

namespace oes {
namespace core {

    using namespace boost;

    /*! \class StringUtils oes/core/utils/String.h
     * \brief A collection of utilities to manipulate strings and list of strings
     * \ingroup oes_core_utils
     */

    /*!
     * \brief Adds a string at the end of the input \p list
     * \details
     * Adds string \p strToPush at the end of the input \p list, if and only if \p strToPush is does not
     * already exist is the list.
     * \param[in,out] list the list to augment with \p strToPush
     * \param[in] strToPush the string to add at the end
     * \retval true if \p strToPush has been successfully added
     * \retval false if \p strToPush was already in the list
     */
    bool StringUtils::push_back_unique(StringList& list, const String& strToPush) {
        if( std::find(list.begin(), list.end(), strToPush) == list.end() ) {
            list.push_back(strToPush);
            return true;
        }

        return false;
    }

    /*!
     * \brief Adds a list of strings at the end of the input \p list
     * \details
     * Adds all the strings in \p listToPush at the end of the input \p list. Elements of \p
     * listToPush are added if and only if they do not already exist in this list.
     * \param[in,out] list the list to augment with \p listToPush
     * \param[in] listToPush strings to add at the end
     * \see push_back_unique(StringList&,const String&)
     */
    void StringUtils::push_back_unique(StringList& list, const StringList& listToPush) {
        push_back_unique(list, listToPush.begin(), listToPush.end());
    }

    /*!
     * \fn void StringUtils::push_back_unique(StringList& list, InputIterator first, InputIterator last)
     * \brief Adds a range of strings at the end of the input \p list
     * \details
     * Adds all the strings between iterator \p first and iterator \p last at
     * the end of the input \p list. Elements of the input range are added if and only if they
     * do not already exist in this list.
     * \param[in,out] list the list to augment with the input range
     * \param[in] first iterator to the fist string to add
     * \param[in] last iterator past the last string to add
     * \tparam InputIterator type of the input iterator pair to add
     * \see push_back_unique(StringList&const StringList&)
     */

    /*!
     * \brief Insert a string at a given position of the input \p list
     * \details
     * Inserts string \p strToInsert at position \p pos in the input \p list, if and only if \p strToInsert
     * does not already exist is the list.
     * \param[in,out] list the list to augment with \p strToInsert
     * \param[in] pos position in the list where the string is inserted
     * \param[in] strToInsert the string to add
     * \retval true if \p strToInsert has been successfully added
     * \retval false if \p strToInsert was already in the list
     */
    bool StringUtils::insert_unique(StringList& list, StringList::iterator pos, const String& strToInsert) {
        if( std::find(list.begin(), list.end(), strToInsert) == list.end() ) {
            list.insert(pos, strToInsert);
            return true;
        }

        return false;
    }

    /*!
     * \brief Adds a list of strings at a given position of the input \p list
     * \details
     * Inserts all the strings in \p listToInsert at position \p pos in the \p list.
     * Strings are inserted if and only if they do not already
     * exist in this list.
     * \param[in,out] list the list to augment with \p listToInsert
     * \param[in] position in this list position where the strings are inserted
     * \param[in] listToInsert strings to insert
     * \see insert_unique(iterator, const String&)
     */
    void StringUtils::insert_unique(StringList& list, StringList::iterator position, const StringList& listToInsert) {
        insert_unique(list, position, listToInsert.begin(), listToInsert.end());
    }

    /*!
     * \fn void StringUtils::insert_unique(StringList& list, StringList::iterator pos, InputIterator first, InputIterator last)
     * \brief Adds a range of strings at a given position of the input \p list
     * \details
     * Adds all the strings between iterator \p first and iterator \p last at
     * position \p pos in the \p list. Strings are added if and only if they
     * do not already exist in the \p list.
     * \param[in,out] list the list to augment with the input range
     * \param[in] pos position in this list where the strings are inserted
     * \param[in] first iterator to the fist string to add
     * \param[in] last iterator past the last string to add
     * \tparam InputIterator type of the input iterator pair to insert
     * \see insert_unique(iterator, const StringList&)
     */


    /****************************************************************************/

    /*!
     * \brief Splits a string to a list of strings
     * \details
     * Splits the input string \p s into tokens separated by \p separator. Each
     * token is added at the end of \p list list. If \p compressTokens is true,
     * adjacent separators are merged together, otherwise each two separator
     * delimit a new token.
     * \param[out] list a list that receives copies of the tokens
     * \param[in] s string to tokenize
     * \param[in] separator the separator between each token
     * \param[in] compressTokens if true, adjacent separators are merged
     * together. Default is false.
     * \see StringList::split()
     */
    void StringUtils::split(StringList& list, const String& s, const char* separator, bool compressTokens) {
        if( s.length() == 0 ) {
            // For compatibility with boost pre-1.45: algorithm::split returned an empty list if the
            // input chain was empty. From 1.46 on it returns a list containing ONE empty string.
            // Justification was that algorithm::split needed to be consistent and always return N+1
            // elements, N being the number of occurrences of the separators.
            return ;
        }
        algorithm::split(list, s,
            algorithm::is_any_of(separator),
            compressTokens ? algorithm::token_compress_on : algorithm::token_compress_off);
    }

    /*!
     * \fn String StringUtils::join(const Container& c, const String& separator, bool ignoreEmptyStrings = false) {
     * \brief Joins list elements into a string
     * \details
     * Joins all strings in the container \p c into a single string. Elements in
     * the resulting string are separated by \p separator. If
     * ignoreEmptyStrings is true, empty strings are not concatenated.
     * \param[in] c the container.
     * \param[in] separator string used to separate elements of the container \p c in
     * the resulting string
     * \param[in] ignoreEmptyStrings if true, empty strings are ignored. Default is false.
     * \return a string that contains elements of \p c separated by \p
     * separator
     * \see StringUtils::split()
     * \see StringList::join()
     */


    /*!
     * \brief Returns the C++ namespace separator
     * Returns the namespace separator used to separate the names in a fully
     * C++ qualified. The return string is "::"
     */
    const String& StringUtils::nameSpaceSeparator() {
        static const String s("::");
        return s;
    }

    /*!
     * \brief Test if a name is qualified
     * \param[in] name name to test
     * \retval true if name \p contains at least an occurrence of the namespace separator
     * \retval false otherwise
     * \see nameSpaceSeparator()
     */
    bool StringUtils::isQualifiedName(const String& name) {
        return name.find(nameSpaceSeparator()) != String::npos;
    }

    /*!
     * \brief Build a qualified name
     * Returns the qualified name of \p name in namespace \p ns. The resulting
     * name is the concatenation of the namespace name \p ns, the namespace
     * separator and the name \p name.
     * \param[in] ns the namespace name
     * \param[in] name name to test
     * \return string that contains the qualified name "\p ns::\p name"
     */
    String StringUtils::qualifiedName(const String& ns, const String& name) {
        if( isQualifiedName(name) ) {
            return name;
        }

        if( ns.empty() ) {
            return name;
        }

        return ns + nameSpaceSeparator() + name;
    }

    /*!
     * \brief Returns the local name of a qualified name
     * Returns the local name part of the fully qualified name \p name.
     * If \p name is of the for "namespace_name\::local_name" then the
     * function returns "local_name", otherwise the function returns \p name.
     * \param[in] name qualified name of the form "namespace_name\::local_name"
     * \return the part after the last namespace separator if the name is
     * qualified, or \p name otherwise.
     * \see isQualifiedName()
     */
    String StringUtils::localName(const String& name) {
        String::size_type pos = name.rfind(nameSpaceSeparator());
        if( pos == String::npos ) {
            return name;
        }

        return name.substr(pos+2);
    }

    /*!
     * \brief Returns the namespace name of a qualified name
     * Returns the local name part of the fully qualified name \p name.
     * If \p name is of the for "namespace_name\::local_name" then the
     * function returns "namespace_name", otherwise the function returns \p
     * name.
     * \param[in] name qualified name of the form "namespace_name\::local_name"
     * \return the part before the last namespace separator if the name is
     * qualified, or \p name otherwise.
     * \see isQualifiedName()
     */
    String StringUtils::nameSpace(const String& name) {
        String::size_type pos = name.rfind(nameSpaceSeparator());
        if( pos == String::npos ) {
            return String();
        }

        return name.substr(0, pos);
    }

    /*!
     * \brief Splits a qualified name
     * Returns the local name part of the fully qualified name \p name.
     * If \p name is of the for "namespace_name\::local_name" then the
     * function stores "namespace_name" into \p ns and "local_name" into \p
     * localName. Otherwise, \p ns is set to the empty string and \p localName
     * receives a copy of \p name.
     * \param[in] name qualified name of the form "namespace_name\::local_name"
     * \param[out] ns namespace part of the \p name
     * \param[out] localName local part of the \p name
     * \see isQualifiedName()
     * \see localName()
     * \see nameSpace()
     */
    void StringUtils::splitName(const String& name, String& ns, String& localName) {
        String::size_type pos = name.rfind(nameSpaceSeparator());
        if( pos == String::npos ) {
            ns = String();
            localName = name;
        } else {
            ns = name.substr(0, pos);
            localName = name.substr(pos+2);
        }
    }

    /*!
     * \brief Escape special characters in a string
     * \details
     * Prepares string \p s for serialization by escaping characters as
     * follows:
     * - if the string contains no white spaces, '\' (backslashes) or '"'
     * (double quotes), the string is left as is
     * - otherwise the escaped string is enclosed within double quotes and
     * characters '"' and '\' are escaped with a leading '\'
     * The resulting string can be unescaped into a string with unescape().
     *
     * \par Examples:
     * \code
     * using namespace oes::core;
     * std::cout << StringUtils::escape("abcd") -> abcd
     * std::cout << StringUtils::escape("ab cd") -> "ab cd"
     * std::cout << StringUtils::escape("ab\\cd") -> "ab\\cd"
     * std::cout << StringUtils::escape("ab\"cd") -> "ab\"cd"
     * \endcode
     *
     * \param[in] s the string to escape
     * \return a copy of \p s with special chars being escaped.
     * \see unescape()
     */
    String StringUtils::escape(const String& s) {
        if( s.find_first_of(" \"\\") == String::npos ) {
            return s;
        }

        String r = std::regex_replace(s, std::regex("[\"\\\\]"), "\\\\$&");

        const String quote = "\"";
        return quote + r + quote;
    }

    /*!
     * \brief Unescape special characters in a string
     * \details
     * Does the reverse operation of escape(): removes enclosing quotes from
     * string \p s if needed and removed leading backslashes before special
     * characters '"' and '\'.
     *
     * \param[in] s the string to unescape
     * \return A copy of \p s with special chars being unescaped.
     * \see escape()
     */
    String StringUtils::unescape(const String& s) {
        if( s.length() < 2 ) {
            return s;
        }

        if( s[0] == '"' and s[s.length()-1] == '"' ) {
            String r = s.substr(1, s.length()-2);
            return std::regex_replace(r, std::regex("\\\\([\"\\\\])"), "$1");
        }

        return s;
    }

    /*!
     * \brief Escape special characters in a list of strings
     * \details
     * Prepares the list of string \p list for serialization as follows: all
     * the strings in \p list are escaped then joined together using the
     * white space separator. The resulting string is escaped one again.
     * The resulting string can be unescaped into a list of elements with
     * unescapeList().
     *
     * \par Examples:
     * \code
     * using namespace oes::core;
     * std::cout << StringUtils::escapeList( {"a","b","c"} ) -> "a b c"
     * std::cout << StringUtils::escapeList( {"a","b c"}   ) -> "a \"b c\""
     * std::cout << StringUtils::escapeList( {"a","b\\c"}  ) -> "a \"b\\\\c\""
     * std::cout << StringUtils::escapeList( {"a","c\"d"}  ) -> "a \"b\\\"c\""
     * \endcode
     *
     * \param[in] list a list of strings to be serialized.
     * \return a string that contains the "serialized version \p list as
     * explained above
     * \see escape()
     * \see unescapeList()
     */
    String StringUtils::escapeList(const StringList& list) {
        const String separator = " ";

        String s;
        bool first = true;
        for( StringList::const_iterator i = list.begin(); i < list.end(); ++i ) {
            if( first ) {
                first = false;
                s = escape(*i);
            } else {
                s += separator;
                s += escape(*i);
            }
        }

        return escape(s);
    }

    /*!
     * \brief Unescape a string into a list of strings
     * \details
     * Does the reverse operation of escapeList(): the input string \p s is
     * unescape()'d then split around the white space separator. Each
     * obtained element is then unescape()'d again and stored in output list \p
     * list.
     *
     * \param[in] s the string to unescape
     * \param[out] list a list of strings to hold the result of the operation
     * \see escapeList()
     * \see unescape()
     */
    void StringUtils::unescapeList(const String& s, StringList& list) {
        list.clear();
        String r = unescape(s);

        String::const_iterator start = r.begin();
        String::const_iterator end = r.end();

        std::regex re("\\s*(\"(?:\\\\.|[^\\n])*?\"|\\S+)");
        std::smatch result;

        while( regex_search(start, end, result, re) ) {
            list.push_back(unescape(result.str(1)));
            start = result.suffix().first;
        }
    }


    /*!
     * \brief Reads an escaped string from an input stream
     * \details
     * Reads an escaped string from input stream \p is and decodes it on the
     * fly. The result is stored in output parameter \p s. If the first
     * character in the stream is a '"' (double quote), the function reads
     * characters, unescaping escaped sequence (\" and \\) on the fly until it
     * reaches the next double quote or the end of the stream.
     * If If the first character in the stream is not a '"' (double quote),
     * the function behaves just like the string extraction operator:
     * \code
     * is >> s;
     * \endcode
     *
     * \param[in] is the input stream to read from
     * \param[out] s string that contains the first escaped string available
     * in the stream.
     * \see unescape()
     */
    std::istream& StringUtils::readEscapedString(std::istream& is, String& s) {
        if( not std::istream::sentry(is) ) {
            return is;
        }

        String::value_type c;
        if( not is.get(c) ) {
            return is;
        }

        if( c != '"' ) {
            is.putback(c);
            return is >> s;
        }

        const String::size_type bufsize = 256;
        String::value_type buf[bufsize];
        String::size_type n = 0;
        buf[n++] = c;

        String r;
        bool escaped = false;

        while( is.get(c) ) {
            if( n == bufsize ) {
                r.append(buf, n);
                n = 0;
            }

            buf[n++] = c;

            if( escaped ) {
                escaped = false;
            }
            else if( c == '\\' ) {
                escaped = true;
            }
            else if( c == '"' ) {
                r.append(buf, n);
                s = r;
                return is;
            }
        }

        return is;
    }

    /*!
     * \brief Reads an escaped list of strings from an input stream
     * \details
     * Reads an escaped string from input stream \p is and decodes it into a
     * list of strings. This is equivalent to calling readEscapedString() to
     * get the first escaped string available and decode this string using
     * unescapeList().
     *
     * \param[in] is the input stream to read from
     * \param[out] list list of strings obtained after the unescaping of the
     * first escaped string available in the stream.
     * \see unescape()
     */
    std::istream& StringUtils::readEscapedStringList(std::istream& is, StringList& list) {
        String s;
        if( readEscapedString(is, s) ) {
            unescapeList(s, list);
        }

        return is;
    }

    /*!
     * \brief Compares 2 strings case insensitive
     * Compares strings \p s1 and \p s2 case insensitive, and returns 0 if
     * they are equal, a < 0 number if s1 < s2 and a > 0 number if s1 > s2.
     * \param[in] s1 first string to compare
     * \param[in] s2 second string to compare
     * \return the result of the comparison.
     */
    int StringUtils::caseInsensitiveCompare(const String& s1, const String& s2) {
        // Make sure we don't pass null strings to string functions: most of
        // the time, we get unexpected behaviors, if not crashes.
        // -- TV

        const char* c_str1 = s1.c_str();
        const char* c_str2 = s2.c_str();
#ifdef _WIN32
        return _stricmp(c_str1, c_str2);
#else
        return strcasecmp(c_str1, c_str2);
#endif
    }

    /**
     *     compare left and right with strcmp () semantics, but taking numerical values (only integers!) into account and not only lexical compare.
     *     This function is designed to read through the left and right strings only one time, for
     *     maximum performance. It does not allocate memory for
     *     substrings. It uses the C-library functions isdigit()
     *     and strtol ().
     *      strcmp sorts name1, name2, name 10 lexicographically:
     *     name1
     *     name10
     *     name2
     *    alphaNumericCompare sorts them lexicographically-numerically:
     *     name1
     *     name2
     *     name10
     *
     *     \param lhs NULL-terminated C-style string
     *     \param rhs NULL-terminated C-style string
     *     \return negative if lhs < rhs, 0 if lhs equals rhs, positive if lhs > rhs
     */
    int StringUtils::naturalStringCompare (const char* lhs, const char* rhs) {
        OES_DEBUG_ASSERT (lhs && rhs);
        if (lhs == nullptr && rhs == nullptr) return 0;
        else if (lhs == nullptr) return 1;
        else if (rhs == nullptr) return -1;

        bool string_cmp_mode = true;
        int first_diff_in_digits_len = 0;
        while(*lhs && *rhs) {

            if(string_cmp_mode) {

                unsigned char l_char, r_char;
                while ((l_char = *lhs)!=0 && (r_char = *rhs)!=0) {

                    // check if this are digit characters
                    const bool l_digit = isdigit (l_char);
                    const bool r_digit = isdigit (r_char);
                    // if both characters are digits, we continue in NUMBER mode
                    if (l_digit && r_digit) {
                        string_cmp_mode = false;
                        break;
                    }


                    const bool equal = l_char == r_char;
                    // if they differ we have a result
                    if(!equal)
                        return l_char > r_char ? 1 : -1 ;;
                    // otherwise process the next characters
                    ++lhs;
                    ++rhs;
                }
            }
            else {// string_cmp_mode false, we're comparing numbers

                char* end_char;
                unsigned long l_int = strtoul (lhs, &end_char, 10/*base*/);
                int l_digits_len = int(end_char - lhs);
                lhs = end_char;
                // get the right number
                unsigned long r_int = strtoul (rhs, &end_char, 10/*base*/);
                int r_digits_len = int(end_char - rhs);
                rhs = end_char;
                if (first_diff_in_digits_len == 0)
                    first_diff_in_digits_len = l_digits_len - r_digits_len;


                // if they are equal we have a comparison result
                //we're not using subtraction and check against the difference because in case of large numbers we might get overflow
                const bool equal = l_int == r_int;
                if(!equal)
                    return l_int > r_int ? 1 : -1 ;

                // otherwise we process the next substring
                string_cmp_mode = true;
            }
        }
        if (first_diff_in_digits_len != 0)
            return first_diff_in_digits_len;
        if(*rhs)
            return -1;
        if(*lhs)
            return 1;
        return 0;
    }

    /*!
     * \brief Compare two strings, lexicographically for non-digit ranges, but numerically
     *     for digit-ranges in the same position inside the two string.
     *
     * Strings can be ascii, or UTF-8 encoded. Recognized digits are only the
     * zero (0) to nine (9) ascii/UTF-8 characters, converted to base-10 integers
     * of arbitrary length (i.e. not limited to 64-bit integer) and \em numerically compared.
     *
     * \param lhs the left string to compare.
     * \param rhs the right string to compare.
     * \return negative integer when lhs < rhs; positive integer if lhs > rhs;
     *     0 if lhs \em equals rhs (despite lhs == rhs possible false).
     *
     * \sa naturalStringCompare(const char*, const char*)
     */
    int StringUtils::naturalStringCompare(const String& lhs, const String& rhs) {
        return naturalStringCompare(lhs.data(), lhs.size(), rhs.data(), rhs.size());
    }

    /*!
     * \brief Compare two strings, lexicographically for non-digit ranges, but numerically
     *     for digit-ranges in the same position inside the two string.
     *
     * Strings can be ascii, or UTF-8 encoded. Recognized digits are only the
     * zero (0) to nine (9) ascii/UTF-8 characters, converted to base-10 integers
     * of arbitrary length (i.e. not limited to 64-bit integer) and \em numerically compared.
     *
     * \param lhs the left string to compare. Does not need to be null-terminated.
     * \param lhsSize byte-size of \a lhs string.
     * \param rhs the right string to compare. Does not need to be null-terminated.
     * \param rhsSize byte-size of \a rhs string.
     * \return negative integer when lhs < rhs; positive integer if lhs > rhs;
     *     0 if lhs \em equals rhs (despite lhs == rhs possible false).
     *
     * \sa naturalStringCompare(const char*, const char*)
     */
    int StringUtils::naturalStringCompare(
        const char* lhs, size_t lhsSize, const char* rhs, size_t rhsSize
    ) {
        OES_DEBUG_ASSERT(lhs && rhs);
        size_t iLhs = 0, iRhs = 0;
        for (; iLhs < lhsSize && iRhs < rhsSize;) {
            const char l_char = lhs[iLhs];
            const char r_char = rhs[iRhs];
            const int diff = l_char - r_char;
            if (-9 > diff || diff > 9) {
                // *both* chars cannot be digits, so normal lexicographical compare,
                // even for the mixed char-digit or digit-char cases.
                return diff;
            }

            // both chars *could* be digits. Double-check they really are
            const bool l_digit = isdigit(static_cast<unsigned char>(l_char));
            const bool r_digit = isdigit(static_cast<unsigned char>(r_char));
            if (l_digit && r_digit) {
                // ignore leading zeros. if number-part only zeros, reduce to single 0.
                //size_t lhsSkipped = 0;
                if (lhs[iLhs] == '0') {
                    for (size_t i = iLhs + 1; i < lhsSize && isdigit(static_cast<unsigned char>(lhs[i])); ++i) {
                        if (lhs[i] != '0') {
                            //lhsSkipped = i - iLhs;
                            iLhs = i;
                            break;
                        }
                    }
                }
                //size_t rhsSkipped = 0;
                if (rhs[iRhs] == '0') {
                    for (size_t i = iRhs + 1; i < rhsSize && isdigit(static_cast<unsigned char>(rhs[i])); ++i) {
                        if (rhs[i] != '0') {
                            //rhsSkipped = i - iRhs;
                            iRhs = i;
                            break;
                        }
                    }
                }

                // walk the numbers to the end, i.e. to the first non-digit
                size_t iLhsEnd = iLhs + 1;
                for (; iLhsEnd < lhsSize && isdigit(static_cast<unsigned char>(lhs[iLhsEnd])); ++iLhsEnd);

                size_t iRhsEnd = iRhs + 1;
                for (; iRhsEnd < rhsSize && isdigit(static_cast<unsigned char>(rhs[iRhsEnd])); ++iRhsEnd);

                // compare the digit lengths (but those length ignore leading "extra" zeros)
                const size_t lenLhs = iLhsEnd - iLhs;
                const size_t lenRhs = iRhsEnd - iRhs;
                if (lenLhs != lenRhs) {
                    return (lenLhs < lenRhs)? -1: +1;
                }

                // rescan the equal-length digit ranges for the first "most-significant" diff
                // TODO: single-pass algo. (allows scanning min lengths, not max lengths too).
                for (; iLhs < iLhsEnd && iRhs < iRhsEnd; ++iLhs, ++iRhs) {
                    const int digitDiff = lhs[iLhs] - rhs[iRhs];
                    if (digitDiff != 0) {
                        return digitDiff;
                    }
                }
                OES_DEBUG_ASSERT(iLhs == iLhsEnd);
                OES_DEBUG_ASSERT(iRhs == iRhsEnd);

                // equal digits. But could have varying count of leading zeros each.
                // ensure deterministic ordering by taking those into account;
                //if (lhsSkipped != rhsSkipped) {
                //    return (lhsSkipped < rhsSkipped)? -1: +1;
                //}
            } else {
                if (diff != 0) {
                    return diff;
                }
                ++iLhs;
                ++iRhs;
            }
        }

        // we get here when one or both strings is/are exhausted.
        return (lhsSize < rhsSize)? -1: (lhsSize == rhsSize)? 0: +1;
    }

    /*!
     * \brief Human-friendly conversion from text to true/false.
     *
     * \param text the text to convert to true/false.
     * \param value_on_invalid (Optional; defaults to false) value to return on
     *     invalid \a text, when empty for example.
     * \retval true when \a text is one of ON, Y, YES, 1, TRUE.
     * \retval false when \a text is one of OFF, N, NO, 0, FALSE.
     * \return value_on_invalid for every other string, including an empty string.
     *
     * \note trims \a text of head/tail whitespace.
     *     Use toTribool() directly for whitespace control.
     */
    bool StringUtils::toBool(const std::string& text, bool value_on_invalid) {
        switch (toTribool(text)) {
        case 'T': return true;
        case 'F': return false;
        case 'N': return value_on_invalid;
        default:  return value_on_invalid;
        }
    }

    /*!
     * \brief Human-friendly conversion from text to a tristate value.
     *
     * \param text the text to convert to true/false.
     * \param trim (Optional; defaults to true) whether to trim \a text
     *     of head/tail whitespace
     * \retval 'T' when \a text is one of ON, Y, YES, 1, TRUE.
     * \retval 'F' when \a text is one of OFF, N, NO, 0, FALSE.
     * \return 'N' (which stands for \em NULL) for every other string,
     *     including an empty string.
     *
     * \note trims \a text of head/tail whitespace.
     *     Use toTribool() directly for whitespace control.
     *
     * \note Compatible with Boost.ProgramOptions' handling of boolean arguments.
     */
    char StringUtils::toTribool(const std::string& text, bool trim) {
        if (!trim && text.length() > 5) {
            return 'N';
        }

        auto s = boost::to_lower_copy(text);
        if (trim) {
            boost::trim(s);
        }

        // See http://stackoverflow.com/questions/15629771
        if        (s == "on"  || s == "y" || s == "yes" || s == "1" || s == "true") {
            return 'T';
        } else if (s == "off" || s == "n" || s == "no"  || s == "0" || s == "false") {
            return 'F';
        } else {
            return 'N';
        }
    }

    /*!
     * \brief URL-encode a string by replacing URL-unsafe characters with encoded equivalents
     *
     * \param s the string to encode
     * \return the URL-encoded string
     */
    std::string StringUtils::urlEncode(const std::string& s) {
        const char *str = s.c_str();
        std::vector<char> v(s.size());
        v.clear();
        for (size_t i = 0, l = s.size(); i < l; i++){
            char c = str[i];
            if ((c >= '0' && c <= '9') ||
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
                c == '*' || c == '\'' || c == '(' || c == ')'){
                v.push_back(c);
            }else if (c == ' '){
                v.push_back('+');
            }else{
                v.push_back('%');
                unsigned char hex1 = c / 16;
                unsigned char hex2 = c % 16;
                hex1 += hex1 <= 9 ? '0' : 'a' - 10;
                hex2 += hex2 <= 9 ? '0' : 'a' - 10;
                v.push_back(hex1);
                v.push_back(hex2);
            }
        }

        return std::string(v.cbegin(), v.cend());
    }

    /*!
     * \brief Abbreviates a string by keeping a specified number of characters from the start and end and replacing everything else in the middle.
     *
     * \param s the string to abbreviate
     * \param keepFront the number of characters from the front of the string to keep
     * \param keepBack the number of characters from the back of the string to keep
     * \param replacement the string to use to replace excluded characters
     * \return a string containing the first \a keepFront chars of \a s, possibly followed by \a replacement (see note), then the last \a keepBack characters of \a s
     *
     * \note If the length of \a s is less than or equal to the sum of \a keepFront and \a keepBack, this function will return \a s as-is. 
     * \note For example, `abbreviate("1234567890", 4, 4)` would return `"1234...7890"`, but `abbreviate("12345678", 4, 4)` would return `"12345678"`
     */
    std::string StringUtils::abbreviate(const std::string& s, const std::size_t keepFront, const std::size_t keepBack, const std::string& replacement) {
        if (s.size() <= keepFront + keepBack) {
            return s;
        }

        return s.substr(0, keepFront) + replacement + s.substr(s.size() - keepBack);
    }

} // namespace core
} // namespace oes

