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

/*! \file oes/core/utils/Json.h
 * \brief Helper classes to generate and parse well formatted JSON document.
 *
 * Use JSONWriter to build a JSON document.
 * Use json_parse() with a custom JSONHandler to interpret the structure
 * of a JSON document.
 */

#include <oes/core/utils/Json.h>
#include <oes/core/utils/Json.hpp>
#include <oes/core/utils/Assert.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/assert.hpp>
#include <boost/shared_ptr.hpp>

#include <streambuf>
#include <exception>
#include <stdexcept>
#include <numeric>
#include <iomanip>
#include <limits>

#include <math.h>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

/*! \internal
 * \brief Array of JSON type strings.
 * \warning Elements must be declared in the same order as in the JSONType enum
 */
const std::string json_type_strings[] = {
    "",
    "object",
    "array",
    "string",
    "number",
    "boolean"
    // note no entry for JSON_ANY
};

const size_t json_type_count = sizeof(json_type_strings) / sizeof(*json_type_strings);

/*!
 * \internal
 * \brief Determines whether a character is JSON insignificant whitespace.
 *
 * \sa http://tools.ietf.org/html/rfc4627, 2., insignificant whitespace.
 * \note that this differs from isspace, which adds Vertical Tab 0x0B and
 *     Form Feed 0x0C, to Space 0x20, Horizontal Tab 0x09, New Line /
 *     Line Feed 0x0A, and Carriage Return 0x0D.
 */
inline bool is_json_whitespace(unsigned char c) {
    return c == 0x20 || c == 0x09 || c == 0x0A || c == 0x0D;
}

/*!
 * \internal
 * \brief Encodes arbitrary string into a JSON string in double-quotes.
 *
 * Special single-byte characters like ", \, /, \b, \f, \n, \r, and \t are
 * replaced with their classic two-bytes C-literal equivalent.
 *
 * Unicode codepoints encoded on multiple bytes are left unchanged.
 * Non-printable 7-bit ASCII characters are converted to \u00FF.
 *
 * \param os the stream to write the encoded string to.
 * \param str UTF-8 encoded string.
 */
void encode_json_string(std::ostream& os, const std::string& str) {
    os << '"'; // opening double-quote
    const size_t len = str.length();
    for (size_t n = 0; n < len; ++n) {
        const unsigned char c = str[n];
        if (c > 127) {
            // All multi-byte UTF-8 encoded codepoints are > 127 return
            // them as-is for now instead of converting them to \uFFFF
            // (or a \uFFFF\uFFFF surrogate pair above the BMP) for now.
            os.put(c);
            continue;
        }
        switch (c) {
        // double-quote, reverse solidus, and control characters
        case  '"': os.put('\\'); os.put('"'); break;
        case '\\': os.put('\\'); os.put('\\'); break;
        case  '/': os.put('\\'); os.put('/'); break;
        case '\b': os.put('\\'); os.put('b'); break;
        case '\f': os.put('\\'); os.put('f'); break;
        case '\n': os.put('\\'); os.put('n'); break;
        case '\r': os.put('\\'); os.put('r'); break;
        case '\t': os.put('\\'); os.put('t'); break;
        default:
            if (isprint(c)) {
                os.put(c);
            } else {
                os.put('\\');
                os.put('u');
                os  << std::setw(4) << std::setfill('0') // not "sticky"
                    << std::hex << (((unsigned int)c) & 0xFF)
                    << std::dec; // reset to decimal! std::hex is "sticky"!
            }
        }
    }
    os << '"'; // closing double-quote
}

/*!
 * \internal
 * \brief Custom stream buffer wrapped around a memory region.
 * \sa http://goo.gl/ZmUWl
 */
struct imembuf : public std::streambuf {
    imembuf(const char* beg, const char* end) {
        char* s = const_cast<char*>(beg);
        setg(s, s, const_cast<char*>(end));
    }
};

template <typename T>
T parse_number(const char*const s, const char*const end) {
    // avoid std::string (heap-alloc+copy) and another copy in istringstream
    // by using a one-time stack-allocated custom stream-buffer + istream.
    //std::string buf(s, end);
    //std::istringstream is(buf);
    imembuf rd(s, end);
    std::istream is(&rd);
    T value = 0; // use uint64_t instead?
    is >> value;
    if (is.fail()) { // handles integer overflow
        throw std::runtime_error("Invalid number");
    }
    return value;
}

uint64_t parser_integer(const char*& s, const char*const end) {
    // skip leading zeros (to get the magnitude right).
    const char*const prev_s0 = s;
    for (unsigned char c = *s; s < end && c == '0'; c = *++s);

    const char*const prev_s = s;
    for (unsigned char c = *s; s < end; c = *++s) {
        if (c < '0' || '9' < c) {
            break;
        }
    }
    const size_t magnitude10 = s - prev_s;
    if (magnitude10 == 0) {
        if (prev_s0 < prev_s) {
            return 0; // one-or-more zero(s)
        }
        throw std::runtime_error("Invalid number: Expected digits");
    }

    // Max 64-bit size_t 18,446,744,073,709,551,615 (magnitude10 = 20),
    // so anything larger will not fit into a size_t / uint64_t.
    if (magnitude10 > 20) {
        // (if a bunch of trailing zeros, might fit into a double...)
        throw std::runtime_error("Invalid number: overflows uint64_t");
    }

    // if it easily fits into a 32-bit int (below one billion),
    // use the cheaper atoi conversion (Max 32-bit size_t 4,294,967,295).
    if (magnitude10 < 10) {
        char buf[10] = { 0 }; // null-terminated
        strncpy(buf, prev_s, magnitude10);
        return atoi(buf);
    }

    // pay for the more expensive istream::operator>> conversion,
    // which will detect/handle integer overflow, on which we throw.
    return parse_number<uint64_t>(prev_s, s);
}

} // namespace

namespace oes {
namespace core {

/*! \class JSONWriterOptions oes/core/utils/Json.h
 * \brief Options for JSONWriter.
 */

/*! \var JSONWriterOptions::version_
 * Indicates the version number of the JSon document to write.
 */

/*! \var JSONWriterOptions::omit_defaults_
 * For values supporting this option (typically serialized as objects),
 * do not serialize or output anything for fields who's value is that
 * fields default value. Taking this JSONWriterOptions class as an example
 * the default created instance would serialize as just {}, and one
 * where omit_defaults_ is true would serialize as {"omit-defaults":true},
 * when this option is true.
 *
 * Defaults to false.
 *
 * \note this is only a hint, and JSON serialization code is free to ignore it.
 */

/*! \var JSONWriterOptions::null_on_empty_
 * For values supporting this option, for fields of types object, array,
 * or string, use null rather than the \em empty representation for that
 * field value, i.e. {}, [], and "" respectively, when this option is true.
 *
 * Defaults to false.
 *
 * \note this is only a hint, and JSON serialization code is free to ignore it.
 */

/*! \class JSONWriter oes/core/utils/Json.h
 * \brief Builds a JSON document.
 *
 * JavaScript Object Notation (JSON) is a text format for the
 * serialization of structured data.  It is derived from the object
 * literals of JavaScript, as defined in the ECMAScript Programming
 * Language Standard, Third Edition.
 *
 * JSON can represent four primitive types (strings, numbers, booleans,
 * and null) and two structured types (objects and arrays).
 *
 * A string is a sequence of zero or more Unicode characters [UNICODE].
 *
 * An object is an unordered collection of zero or more name/value
 * pairs, where a name is a string and a value is a string, number,
 * boolean, null, object, or array.
 *
 * An array is an ordered sequence of zero or more values.
 *
 * JSONWriter will write out a well-formed JSON document (which is also a
 * value) to a given stream. No decorations like spaces or line-separators
 * are added to the output text, which thus is composed of a single line.
 *
 * For example, the calls below result in the text {"version":"1.2.3","values":[1,2.3,true,null,{}]}.
 * \code
 * void print_json() {
 *     JSONWriter json(std::cout);
 *     json.push_object()
 *        .key("version").string("1.2.3")
 *        .key("values").push_array()
 *            .number(1).number(2.3).json_true().json_null()
 *            .push_object().pop_object()
 *        .pop_array()
 *     .pop_object();
 * }
 * \endcode
 *
 * \note When instead of writing the JSON text to a std::ostream, you want to
 *     directly get the JSON text itself, use the to_json_text() convenience
 *     template function (defined in json.hpp) which
 *     encapsulates the 4 lines of code necessary to do just that.
 *
 * \note This writer allows writing a single \em primitive value (either a
 *     number, a string, a boolean, or null) by itself, outside of an array
 *     or an object.
 *
 * \sa http://tools.ietf.org/html/rfc4627
 */

/*!
 * Constructs a new JSONWriter to write around a given ostream.
 * \param os Stream to write to. Must remain valid during this writer's lifetime.
 * \param options the writing options.
 *
 * \warning floating-point values will be written using that streams formatting
 *     rules, which may result in truncation of some decimals. You may want to
 *     call os.precision(17) (for example) to increase the default precision
 *     used, and/or os.setf(ios::fixed) to force fixed-precision.
 */
JSONWriter::JSONWriter(
    std::ostream& os, const JSONWriterOptions& options
) : os_(os)
  , options_(options)
{
}

/*!
 * \brief Deletes this writer.
 *
 * \throw std::uncaught_exception on an incomplete document, if not reset(),
 *     unless std::uncaught_exception() returns true, in which case the
 *     exception is not thrown to avoid std::terminate().
 *
 * \warning If the state of this writer is not ok(), i.e. an invalid JSON
 *     value results from the series of calls made on this writer, then this
 *     destructor throws (unless another exception is pending to avoid
 *     std::terminate()ing the application). Use reset() prior to destruction
 *     to avoid this possible exception if appropriate.
 */
#if (__cplusplus > 199711L) || (defined (_MSVC_LANG) && (_MSVC_LANG >= 201402L))
JSONWriter::~JSONWriter() noexcept(false) {
#else
    JSONWriter::~JSONWriter() {
#endif
#if (__cplusplus >= 201703L) || (defined (_MSVC_LANG) && (_MSVC_LANG >= 201703L))
        if (!ok () && (std::uncaught_exceptions () == 0)) {
#else
        if (!ok() && !std::uncaught_exception()) {
#endif
        // cppcheck-suppress exceptThrowInDestructor
        throw std::runtime_error("Invalid or incomplete JSON value");
    }
}

/*!
 * \brief Determines whether we have written a valid and complete JSON value.
 * \return true if valid and complete; false otherwise.
 * \sa reset()
 */
bool JSONWriter::ok() const {
    return end_.empty() || (end_.size() == 1 && end_[0] == ',');
}

/*!
 * \brief Resets the state of this class.
 * \note After a reset(), the destruction (implicit or not) of this writer
 *     is guaranteed not to throw.
 * \warning the output-stream this writer wraps is left unchanged, with whatever
 *     content was written so far already \em consumed by that stream.
 * \sa ok()
 */
void JSONWriter::reset() {
    end_.clear();
    pending_ws_.clear();
}

/*!
 * \brief gets the writing options.
 */
const JSONWriterOptions& JSONWriter::options() const {
    return options_;
}

/*!
 * \internal
 * \brief Asserts that we expect an object key.
 */
void JSONWriter::assert_expects_key() {
    if (end_.empty()) {
        throw std::runtime_error("Unexpected top-level object key");
    }

    switch (end_.back()) {
    case ']':
        throw std::runtime_error("Expecting a value, not an object key");
    case ':':
        throw std::runtime_error("Expecting a value, following an object key");
    case ',':
        if (end_.size() == 1) {
            throw std::runtime_error("Unexpected top-level object key");
        }
        if (end_[end_.size()-2] != '}') {
            throw std::runtime_error("Unexpected object key outside object");
        }
        os_.put(',');
        end_.pop_back();
        break;
    }
    flush_pending_ws();
}

/*!
 * \internal
 * \brief Asserts that we expect a value.
 * Could be a naked value, a value of an array, or the value of a given object key.
 */
void JSONWriter::assert_expects_value() {
    // accept "naked" values, not only object as the top-level value.
    if (end_.empty()) {
        //flush_pending_ws();
        return;
    }

    switch (end_.back()) {
    case '}':
        throw std::runtime_error("Expecting an object key, not a value");
    case ':':
        BOOST_ASSERT(end_.size() >= 2);
        os_.put(':');
        end_.pop_back();
        break;
    case ',':
        if (end_.size() == 1) {
            throw std::runtime_error("Unexpected top-level value");
        }
        if (end_[end_.size()-2] == '}') {
            throw std::runtime_error("Expecting an object key, not a value");
        }
        os_.put(',');
        end_.pop_back();
        break;
    }
    flush_pending_ws();
}

/*!
 * \internal
 * \brief Asserts that we expect a value, and writes that value.
 */
template <typename T> JSONWriter& JSONWriter::push_value(T t) {
    assert_expects_value();
    os_ << t;
    end_.push_back(',');
    return *this;
}

/*!
 * \brief Appends an integer value to the JSON document.
 *
 * \param value the integer to append.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if an primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::number(int value) {
    return push_value(value);
}

/*!
 * \brief Appends an integer value to the JSON document.
 *
 * \param value the integer to append.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if an primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::number(uint64_t value) {
    return push_value(value);
}

/*!
 * \brief Appends an unsigned integer value to the JSON document.
 *
 * \param value the unsigned integer to append.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if an primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::number(unsigned int value) {
    return push_value(value);
}

#ifdef _WIN32
/*!
 * \brief Appends an unsigned long value to the JSON document.
 *
 * \param value the unsigned long to append.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if an primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::number(unsigned long value) {
    return push_value(value);
}
#endif

/*!
 * \brief Appends an integer value to the JSON document.
 *
 * \param value the integer to append.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if an primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::number(int64_t value) {
    return push_value(value);
}

/*!
 * \brief Appends a floating-point value to the JSON document.
 *
 * \param value the floating-point to append.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if a primitive value is not allowed,
 *     typically because an object key is expected instead.
 *
 * \warning Since JSON is a textual document, and we write it via a
 *     std::ostream, that stream's current formatting rules are used as-is,
 *     in particular its std::ostream::precision() and formatting flags,
 *     which could result in loss of precision of \a value when read back.
 *     Client is in charge of configuring its stream suitably.
 */
JSONWriter& JSONWriter::number(float value) {
    return push_value(value);
}

/*!
 * \brief Appends a floating-point value to the JSON document.
 *
 * \param value the floating-point to append.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if a primitive value is not allowed,
 *     typically because an object key is expected instead.
 *
 * \warning Since JSON is a textual document, and we write it via a
 *     std::ostream, that stream's current formatting rules are used as-is,
 *     in particular its std::ostream::precision() and formatting flags,
 *     which could result in loss of precision of \a value when read back.
 *     Client is in charge of configuring its stream suitably.
 */
JSONWriter& JSONWriter::number(double value) {
    return push_value(value);
}

/*!
 * \brief Appends a string value to the JSON document.
 *
 * Special single-byte characters like ", \\, /, \\b, \\f, \\n, \\r, and \\t are
 * replaced with their classic two-bytes C-literal equivalent.
 *
 * Unicode codepoints UTF-8 encoded on multiple bytes are left unchanged.
 * Non-printable 7-bit ASCII characters are converted to \\u00FF (where
 * FF denotes that non-printable character hexadecimal ASCII code).
 *
 * \param value the string to append.
 * \param null_on_empty (Optional; defaults to false) whether to write an empty
 *     string (""), or null in case of an empty string.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if a primitive value is not allowed,
 *     typically because an object key is expected instead.
 *
 * \note One should use key(), not string() to setup the key of an
 *     object's key-value pair.
 */
JSONWriter& JSONWriter::string(const std::string& value, bool null_on_empty) {
    assert_expects_value();
    if (null_on_empty && value.empty()) {
        os_ << "null";
    } else {
        encode_json_string(os_, value);
    }
    end_.push_back(',');
    return *this;
}

/*!
 * \brief Appends the literal "true" or "false" to the JSON document.
 *
 * \param value if \a value is true, appends "true"; "false" otherwise.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if a primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::json_bool(bool value) {
    return value? json_true(): json_false();
}

/*!
 * \brief Appends the literal "null" to the JSON document.
 *
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if a primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::json_null() {
    return push_value("null");
}

/*!
 * \brief Appends the literal "true" to the JSON document.
 *
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if a primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::json_true() {
    return push_value("true");
}

/*!
 * \brief Appends the literal "false" to the JSON document.
 *
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if a primitive value is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::json_false() {
    return push_value("false");
}

/*!
 * \brief Starts / opens / pushes a new array in the JSON document.
 *
 * A JSON array is an ordered sequence of zero or more values.
 *
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if an array is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::push_array() {
    assert_expects_value();
    os_ << '[';
    end_.push_back(']');
    return *this;
}

/*!
 * \brief Finishes / closes / pops the current array of the JSON document.
 *
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if the current \em scope is not
 *     inside an array.
 */
JSONWriter& JSONWriter::pop_array() {
    if (end_.empty()) {
        throw std::runtime_error("No open array to end");
    }
    if (end_.back() == ',') end_.pop_back(); // discard
    if (end_.empty() || end_.back() != ']') {
        throw std::runtime_error("No open array in this scope");
    }
    flush_pending_ws();
    os_ << end_.back();
    end_.pop_back();
    end_.push_back(',');
    return *this;
}

/*!
 * \brief Starts / opens / pushes a new object in the JSON document.
 *
 * A JSON object is an unordered collection of zero or more name/value
 * pairs, where a name is a string and a value is a string, number,
 * boolean, null, object, or array.
 *
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if an object is not allowed,
 *     typically because an object key is expected instead.
 */
JSONWriter& JSONWriter::push_object() {
    assert_expects_value();
    os_ << '{';
    end_.push_back('}');
    return *this;
}

/*!
 * \brief Sets the key name of an object's key-value pair.
 *
 * The key must be followed by a primitive value (number(), string(),
 * json_bool(), json_null()), or an array (push_array()) or object (push_object()).
 * Failure to provide this value is likely to make this writer throw an
 * exception in its destructor, unless reset() is called prior to this writer
 * going out of scope.
 *
 * \param key the key-value pair's name.
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if the current \em scope is not
 *     inside an object, or a value is expected instead of a key.
 *
 * \note See string(const std::string&) for the encoding rules of keys
 *     and string values.
 */
JSONWriter& JSONWriter::key(const std::string& key) {
    assert_expects_key();
    encode_json_string(os_, key);
    end_.push_back(':');
    return *this;
}

/*!
 * \brief Finishes / closes / pops the current object of the JSON document.
 *
 * \return this JSON writer, for call chaining.
 * \throw std::runtime_exception() if the current \em scope is not
 *     inside an object.
 */
JSONWriter& JSONWriter::pop_object() {
    if (end_.empty()) {
        throw std::runtime_error("No open object to end");
    }
    if (end_.back() == ',') end_.pop_back(); // discard
    if (end_.empty() || end_.back() != '}') {
        throw std::runtime_error("No open object in this scope");
    }
    flush_pending_ws();
    os_ << end_.back();
    end_.pop_back();
    end_.push_back(',');
    return *this;
}

/*!
 * \brief Writes explicit whitespace to the underlying stream.
 *
 * This writer does not add any whitespace when generating the text of the
 * JSON document, but explicit whitespace can be added via this method.
 *
 * \param ws the whitespace to append to the stream.
 * \param before_comma_colon whether the whitespace should come before
 *   a pending comma (separating values of an array, or key-value pairs
 *   of an object), or pending colon (separating the key and value in an
 *   object's key-value pair).
 * \return this JSON writer, for call chaining.
 *
 * \note JSON recognizes only four characters as insignificant whitespace:
 *     Space 0x20, Horizontal Tab 0x09, New Line / Line Feed 0x0A, and
 *     Carriage Return 0x0D. This differs from the C/C++ isspace() method
 *     which adds Vertical Tab 0x0B and Form Feed 0x0C.
 *
 * \throw std::runtime_exception() if the current \em scope is not
 *     inside an object or array.
 */
JSONWriter& JSONWriter::whitespace(const std::string& ws, bool before_comma_colon) {
#if defined (OES_DEBUG_BUILD)
    for (size_t i = 0; i < ws.length(); ++i) {
        unsigned char c = ws[i];
        if (c > 127 || !is_json_whitespace(c)) {
            throw std::runtime_error("invalid whitespace");
        }
    }
#endif

    // the ok() test deals with whitespace before/after the top-level value
    if (before_comma_colon || ok()) {
        os_ << ws;
    } else {
        const char* s = ws.c_str();
        pending_ws_.insert(pending_ws_.end(), s, s + ws.length());
    }
    return *this;
}

/*!
 * \internal
 * \brief Flush any pending whitespace.
 */
void JSONWriter::flush_pending_ws() {
    if (!pending_ws_.empty()) {
        os_.write(&pending_ws_.front(), pending_ws_.size());
        pending_ws_.clear();
    }
}

/*!
 * \fn template<typename V> JSONWriter& JSONWriter::value(const std::vector<V>&)
 * \brief Writes a value for which there is an available json_write() overload.
 *
 * This function expects a json_write(JSONWriter& json, const V&) to be
 * defined in the oes::core namespace, or defined in the same namespace
 * as V, to be found via argument-dependent lookup (ADL).
 *
 * \tparam V type of the value
 * \param value the value to write.
 * \return this JSON writer, for call chaining.
 *
 * \sa to_json_text()
 */

/*!
 * \fn template<typename C> JSONWriter& JSONWriter::values(const C& container)
 * \brief Writes a (non-associative) container of values.
 *
 * Writes \p values to this JSON writer as a JSON array.
 *
 * This function expects a json_write(JSONWriter& json, const V&) to
 * write the vector values, where V stands for \em typename \em C::value_type.
 * Overloads already exist for all JSON basic types, and additional overloads
 * for custom types can be used as well (See notes in to_json_text()).
 *
 * \tparam C STL container of values.
 * \param container a container of values
 * \return this JSON writer, for call chaining.
 */

/*!
 * \fn template<typename C> JSONWriter& JSONWriter::size(const C& container)
 * \brief Writes the size of an STL container.
 *
 * Convenience for writing the size of an STL container.
 * Equivalent to: \code .size(container.size()) \endcode.
 *
 * \tparam C STL container of values.
 * \param container a container of values
 * \return this JSON writer, for call chaining.
 */

/*!
 * \fn JSONWriter& JSONWriter::size(size_t value)
 * \brief Writes the size of an STL container.
 *
 * Convenience for writing a size in a 32/64-bit portable manner.
 * Equivalent to:  \code .size(static_cast<uint64_t>(value)) \endcode.
 *
 * \tparam C STL container of values.
 * \param value the size to write as a integral number.
 * \return this JSON writer, for call chaining.
 */

/*************************************************************************/

/*! \class JSONHandler oes/core/utils/Json.h
 * \brief Handles parsing events from a JSON document.
 *
 * Abstract class to be extended by a client to interpret a JSON document
 * logically, independently of the low-level decoding of the text of that
 * document to restore proper primitive values as numbers, string, booleans,
 * and without having to check that the JSON document is well-formed (the
 * parser checks well-formed-ness, and will throw an exception in that case).
 *
 * A JSON parser calls back a different method depending on the parsing event,
 * and that methods implementation can either accept that event by returning
 * true, or reject it by returning false, which aborts parsing. This allows
 * for example a handler to accept only some JSON documents which conform to
 * an expected structure, like for example an array of strings only.
 *
 * By default all handling methods return false, thus our array of strings
 * handler needs only overriding handle_array_begin(), handle_array_end(),
 * and handle_string() and in effect reject any document that contains
 * primitives other than a string, or that contains an array.
 *
 * A JSONHandler is similar to a SAX handler for XML documents.
 */

/*!
 * \brief Do nothing constructor.
 *
 * \param return_code (Optional; defaults to false) the fixed return code for
 *     all handle_* methods of this handler.
 */
JSONHandler::JSONHandler(bool return_code) : return_code(return_code) {
}

/*!
 * \brief Do nothing destructor.
 */
JSONHandler::~JSONHandler() {
}

/*!
 * \brief Accept the start of an object.
 * \return true if an object is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 */
bool JSONHandler::handle_object_begin() {
    return return_code;
}

/*!
 * \brief Accept an object's key.
 * \param key the object's key. Unicity of the key within the current object
 *     is not verified.
 * \return true if an object key is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 */
bool JSONHandler::handle_object_key(const std::string& key) {
    OES_UNUSED(key);
    return return_code;
}

/*!
 * \brief Hook for handler to process the key's value the parser
 *     just finished dispatching to the handler.
 *
 * Unlike most handle_ methods, there is no equivalent method in JSONWriter
 * because the command is pure JSON syntax, not a value. As such, there is
 * nothing to \em handle really, but it allows the handler code to be notified
 * more simply when the value associated to a key, which might be complex,
 * finished being parsed/handled.
 *
 * \return always true.
 *
 * \note handle_object_comma() is not called after reading the last key's value
 *     so handler code depending on handle_object_comma() might call it explicitly
 *     from its handle_object_end() method, to \em process the last value using
 *     handle_object_comma() as well.
 */
bool JSONHandler::handle_object_comma() {
    return true;
}

/*!
 * \brief Accept the end of an object.
 * \return true if an object is complete in this context;
 *     false otherwise (default), which aborts the parsing.
 * \note the parser checks we are in the context of an open object, and
 *     this handler does not need to check that.
 */
bool JSONHandler::handle_object_end() {
    return return_code;
}

/*!
 * \brief Accept the start of an array.
 * \return true if an object is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 */
bool JSONHandler::handle_array_begin() {
    return return_code;
}

/*!
 * \brief Hook for handler to process the array's value the parser
 *     just finished dispatching to the handler.
 *
 * Unlike most handle_ methods, there is no equivalent method in JSONWriter
 * because the command is pure JSON syntax, not a value. As such, there is
 * nothing to \em handle really, but it allows the handler code to be notified
 * more simply when the current value of the array, which might be complex,
 * finished being parsed/handled.
 *
 * \return always true.
 *
 * \note handle_object_comma() is not called after reading the last array value
 *     so handler code depending on handle_object_comma() might call it explicitly
 *     from its handle_array_end() method, to \em process the last value using
 *     handle_object_comma() as well.
 */
bool JSONHandler::handle_array_comma() {
    return true;
}

/*!
 * \brief Accept the end of an array.
 * \return true if an array is complete in this context;
 *     false otherwise (default), which aborts the parsing.
 * \note the parser checks we are in the context of an open array, and
 *     this handler does not need to check that.
 */
bool JSONHandler::handle_array_end() {
    return return_code;
}

/*!
 * \brief Accept a primitive signed integer value.
 * \param value the integer to handle.
 * \return true if an integer is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 * \note the parser decide which handle_number overload to call based on the
 *     presence or absence of a decimal-point and exponent, and the value
 *     itself, preferring this method over the 64-bits ones for integer if
 *     it fits inside a 32-bit int.
 * \note A number originally written using JSONWriter::number(double), e.g. 1.0
 *     can result at parse-time to a call to handle_number(int) since was
 *     shortened to the equivalent text "1" with no loss of precision.
 */
bool JSONHandler::handle_number(int value) {
    OES_UNUSED(value);
    return return_code;
}

/*!
 * \brief Accept a primitive 64-bit unsigned integer value.
 * \param value the integer to handle.
 * \return true if an integer is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 * \note the parser decide which handle_number overload to call based on the
 *     presence or absence of a decimal-point and exponent, and the value
 *     itself, preferring this method over the 32-bit one for integer if
 *     it does not fit inside a 32-bit int.
 */
bool JSONHandler::handle_number(uint64_t value) {
    OES_UNUSED(value);
    return return_code;
}

/*!
 * \brief Accept a primitive 64-bit signed integer value.
 * \param value the integer to handle.
 * \return true if an integer is allowed in this context;
 *     false otherwise (default), which aborts the parsing.

 * \note the parser decide which handle_number overload to call based on the
 *     presence or absence of a decimal-point and exponent, and the value
 *     itself, preferring this method over the 32-bit one for integer if
 *     it does not fit inside a 32-bit int.
 */
bool JSONHandler::handle_number(int64_t value) {
    OES_UNUSED(value);
    return return_code;
}

/*!
 * \brief Accept a primitive floating-point value.
 * \param value the floating-point to handle.
 * \return true if an floating-point is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 * \note A number originally written using JSONWriter::number(double), e.g. 1.0
 *     can result at parse-time to a call to handle_number(int) instead of this
 *     method since was shortened to the equivalent text "1" with no loss of
 *     precision, and is read back as an integer.
 * \warning The precision used for the std::ostream, if using a JSONWriter,
 *     may result in truncation which could yield a different number later on
 *     at parse-time.
 */
bool JSONHandler::handle_number(double value) {
    OES_UNUSED(value);
    return return_code;
}

/*!
 * \brief Accept a primitive string value.
 * \param value the string to handle. This string is a view of an internal
 *     buffer of the parser that will be reused immediately after this call
 *     and should therefore be copied to a client-side buffer if it should
 *     be kept by the client. \a value is always null-terminated.
 * \return true if a string is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 * \note The string has been decoded, except for UTF-8 encoded multi-byte
 *     Unicode codepoints which are left unchanged.
 */
bool JSONHandler::handle_string(const std::string& value) {
    OES_UNUSED(value);
    return return_code;
}

/*!
 * \brief Accept a primitive boolean value.
 * \param value the boolean (literal "true" or "false") to handle.
 * \return true if a boolean is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 */
bool JSONHandler::handle_boolean(bool value) {
    OES_UNUSED(value);
    return return_code;
}

/*!
 * \brief Accept a primitive null value.
 * \return true if a null is allowed in this context;
 *     false otherwise (default), which aborts the parsing.
 */
bool JSONHandler::handle_null() {
    return return_code;
}

/*!
 * \brief Accept structurally insignificant whitespace.
 * \return true (default) if whitespace is allowed in this context;
 *     false otherwise, which aborts the parsing.
 */
bool JSONHandler::handle_whitespace(
    const std::string& ws, bool before_comma_colon
) {
    OES_UNUSED(ws);
    OES_UNUSED(before_comma_colon);
    return return_code; // i.e. silently ignore
}

/*************************************************************************/

namespace {

/*!
 * \brief Handler with forwards all JSON parsing events to a JSONWriter.
 *
 * Mostly useful for round-trip unit testing.
 */
class JSONWriterHandler : public JSONHandler {
public:
    explicit JSONWriterHandler(std::ostream& os);
    explicit JSONWriterHandler(const JSONWriter_ptr& writer);
    ~JSONWriterHandler() override;

    bool handle_object_begin() override;
    bool handle_object_key(const std::string& key) override;
    bool handle_object_end() override;

    bool handle_array_begin() override;
    bool handle_array_end() override;

    bool handle_number(int value) override;
    bool handle_number(int64_t value) override;
    bool handle_number(uint64_t value) override;
    bool handle_number(double value) override;
    bool handle_string(const std::string& value) override;
    bool handle_boolean(bool value) override;
    bool handle_null() override;

    bool handle_whitespace(const std::string& ws, bool before_comma_colon) override;

private:
    JSONWriter_ptr writer_ptr_;
    JSONWriter& writer_;
};

/*!
 * \brief Instantiates this handler, instantiating an internal JSONWriter
 *     around the given output stream.
 * \param os the output stream to rewrite the JSON document to.
 */
JSONWriterHandler::JSONWriterHandler(
    std::ostream& os
) : writer_ptr_(new JSONWriter(os))
  , writer_(*writer_ptr_)
{
}

/*!
 * \brief Instantiates this handler, around an existing JSONWriter.
 * \param writer the JSON writer to forward all parsing events to.
 */
JSONWriterHandler::JSONWriterHandler(
    const JSONWriter_ptr& writer
) : writer_ptr_(writer)
  , writer_(*writer_ptr_)
{
    BOOST_VERIFY(writer_ptr_);
}

/*!
 * \brief Deletes this handler.
 * Resets the internal JSONWriter
 * \sa JSONWriter::reset()
 */
JSONWriterHandler::~JSONWriterHandler() {
    writer_.reset();
}

/*!
 * \brief Handles a new object.
 * \return Always true.
 * \note calls JSONWriter::push_object()
 */
bool JSONWriterHandler::handle_object_begin() {
    writer_.push_object();
    return true;
}

/*!
 * \brief Handles an object key of a key-value pair.
 * \param key an object key name.
 * \return Always true.
 * \note calls JSONWriter::key(key)
 */
bool JSONWriterHandler::handle_object_key(const std::string& key) {
    writer_.key(key);
    return true;
}

/*!
 * \brief Handles the end of an object.
 * \return Always true.
 * \note calls JSONWriter::pop_object()
 */
bool JSONWriterHandler::handle_object_end() {
    writer_.pop_object();
    return true;
}

/*!
 * \brief Handles a new array.
 * \return Always true.
 * \note calls JSONWriter::push_array()
 */
bool JSONWriterHandler::handle_array_begin() {
    writer_.push_array();
    return true;
}

/*!
 * \brief Handles the end of an array.
 * \return Always true.
 * \note calls JSONWriter::pop_array()
 */
bool JSONWriterHandler::handle_array_end() {
    writer_.pop_array();
    return true;
}

/*!
 * \brief Handles a primitive integer number value.
 * \param value an integer value.
 * \return Always true.
 * \note calls JSONWriter::number(value)
 */
bool JSONWriterHandler::handle_number(int value) {
    writer_.number(value);
    return true;
}

/*!
 * \brief Handles a primitive integer number value.
 * \param value an integer value.
 * \return Always true.
 * \note calls JSONWriter::number(value)
 */
bool JSONWriterHandler::handle_number(int64_t value) {
    writer_.number(value);
    return true;
}

/*!
 * \brief Handles a primitive unsigned integer number value.
 * \param value an integer value.
 * \return Always true.
 * \note calls JSONWriter::number(value)
 */
bool JSONWriterHandler::handle_number(uint64_t value) {
    writer_.number(value);
    return true;
}

/*!
 * \brief Handles a primitive floating-point number value.
 * \param value an floating-point value.
 * \return Always true.
 * \note calls JSONWriter::number(value)
 */
bool JSONWriterHandler::handle_number(double value) {
    writer_.number(value);
    return true;
}

/*!
 * \brief Handles a primitive string value.
 * \param value a string value.
 * \return Always true.
 * \note calls JSONWriter::string(value)
 */
bool JSONWriterHandler::handle_string(const std::string& value) {
    writer_.string(value);
    return true;
}

/*!
 * \brief Handles a primitive boolean literal value.
 * \param value a boolean true or false value.
 * \return Always true.
 * \note calls JSONWriter::boolean(value)
 */
bool JSONWriterHandler::handle_boolean(bool value) {
    writer_.json_bool(value);
    return true;
}

/*!
 * \brief Handles a primitive null literal value.
 * \return Always true.
 * \note calls JSONWriter::json_null()
 */
bool JSONWriterHandler::handle_null() {
    writer_.json_null();
    return true;
}

/*!
 * \brief Handles insignificant whitespace.
 * \param ws the JSON whitespace.
 * \param before_comma_colon whether the whitespace was seen before or after
 *     a comma (separating array values or object key-value pairs) or a colon
 *     (separating an object key and value from one of its pairs).
 *     \a before_comma_colon is always false if either end of \a ws does not
 *     \em touch a comma or colon (for example in an empty array "[ ]")
 * \return Always true.
 * \note calls JSONWriter::whitespace(ws, before_comma_colon)
 */
bool JSONWriterHandler::handle_whitespace(
    const std::string& ws, bool before_comma_colon
) {
    writer_.whitespace(ws, before_comma_colon);
    return true;
}

} // namespace

/*************************************************************************/

JSONHandler_ptr json_handler_writer(std::ostream& os) {
    return JSONHandler_ptr(new JSONWriterHandler(os));
}

JSONHandler_ptr json_handler_writer(const JSONWriter_ptr& writer) {
    return JSONHandler_ptr(new JSONWriterHandler(writer));
}

/*************************************************************************/

JSONToken::JSONToken(
    const char* begin, const char* end
) : beg_(begin)
  , end_(end? end: begin)
{
}

JSONRawStringRef::JSONRawStringRef(
    const char* begin, const char* end
) : JSONToken(begin, end)
{
    num_.is_negative_ = num_.has_dot_ = num_.has_exp_ = false;
}

bool JSONToken::empty() const {
    BOOST_ASSERT(end_ >= beg_);
    return end_ == beg_;
}

size_t JSONToken::size() const {
    BOOST_ASSERT(end_ >= beg_);
    return end_ - beg_;
}

bool JSONToken::is_quoted() const {
    return size() >= 2 && *beg_ == '"' && *(end_ - 1) == '"';
}

const char* JSONToken::find(char c) const {
    for (const char* s = beg_; s < end_; ++s) {
        if (*s == c) {
            return s;
        }
    }
    return end_;
}

bool JSONToken::matches(const char* text, size_t len) const {
    return size() == len && 0 == strncmp(beg_, text, len);
}

bool JSONToken::matches_unquote(const char* text, size_t len) const {
    return size() == (len + 2) && 0 == strncmp(beg_ + 1, text, len);
}

JSONRawStringRef& JSONRawStringRef::reset(const char* begin) {
    beg_ = end_ = begin;
    num_.is_negative_ = num_.has_dot_ = num_.has_exp_ = false;
    return *this;
}

JSONRawStringRef& JSONRawStringRef::endsAt(const char* end) {
    end_ = end;
    return *this;
}

void JSONRawStringRef::decode(std::string& buffer, bool unquote) const {
    const char* s = unquote? (beg_ + 1): beg_;
    const char*const end = unquote? (end_ - 1): end_;

    // in the simplest case, no need to rescan the text.
    if (!str_.has_escape_ && !str_.has_unicode_) {
        buffer.append(s, end);
        return;
    }

    buffer.reserve(end - s);
    for (char c = *s; s < end; c = *++s) {
        if (c != '\\') {
            buffer.push_back(c);
            continue;
        }

        BOOST_ASSERT((s + 1) < end); // already checked by parser

        switch (c = *++s) {
        case  'u': {
            // encoded Unicode codepoint must be followed by 4 hex
            BOOST_ASSERT((s + 4) < end); // already checked by parser

            ++s;
            imembuf rd(s, s + 4);
            s += 3; // not 4, since loop itself will also ++
            std::istream is(&rd);
            unsigned short codepoint = 0;
            is >> std::hex >> codepoint;
            BOOST_ASSERT(!is.fail()); // impossible, 4 xdigit must work

            // decode codepoint
            if  (codepoint < 0x0080) {       // characters 0x0000 - 0x007F
                buffer.push_back(0x00 | (char)((codepoint & 0x007F) >>  0));
            } else if (codepoint < 0x0800) { // characters 0x0080 - 0x07FF
                buffer.push_back(0xC0 | (char)((codepoint & 0x07C0) >>  6));
                buffer.push_back(0x80 | (char)((codepoint & 0x003F) >>  0));
            } else if (0xD800 <= codepoint && codepoint <= 0xDFFF) {
                // surrogate codepoints
                // TODO: handle surrogate pairs > BMP!
                throw std::runtime_error("TODO: Support Unicode Surrogate Pairs");
            } else { // characters 0x0800 - 0xD7FF and 0xE000 - 0xFFFF
                buffer.push_back(0xE0 | (char)((codepoint & 0xF000) >> 12));
                buffer.push_back(0x80 | (char)((codepoint & 0x0FC0) >>  6));
                buffer.push_back(0x80 | (char)((codepoint & 0x003F) >>  0));
            }
            break;
        }

        case  '"': case '\\': case  '/':
            buffer.push_back(c);
            break;

        case 'b': buffer.push_back('\b'); break;
        case 'f': buffer.push_back('\f'); break;
        case 'n': buffer.push_back('\n'); break;
        case 'r': buffer.push_back('\r'); break;
        case 't': buffer.push_back('\t'); break;

        default: // only above characters are allowed after backslash
            throw std::runtime_error("Invalid String: Unexpected Escape");
        }
    }
}

std::string JSONToken::to_string(bool unquote) const {
    BOOST_ASSERT(!unquote || size() >= 2);
    return unquote? std::string(beg_ + 1, end_ - 1): std::string(beg_, end_);
}

std::ostream& JSONToken::print(std::ostream& os) const {
    return os.write(beg_, size());
}

/*************************************************************************/

namespace {

void do_nothing_handler(JSONParsingEventType, const JSONRawStringRef&, void*) {
    return;
}

void print_no_whitespace_handler(
    JSONParsingEventType evt, const JSONRawStringRef& text, void* user_data
) {
    BOOST_ASSERT(!text.empty());
    if (evt != JSON_SYNTAX_WHITESPACE) {
        std::ostream* os = static_cast<std::ostream*>(user_data);
        os->write(text.beg_, text.size());
        //std::string value = text.to_string();
        //(*os) << value;
    }
}

void type_detection_handler(
    JSONParsingEventType evt, const JSONRawStringRef& /*text*/, void* user_data
) {
    JSONType& type = *static_cast<JSONType*>(user_data);
    if (type != JSON_TYPE_INVALID) {
        // Type is already known
        return;
    }

    switch (evt) {
    case JSON_OBJECT_BEGIN:
        type = JSON_TYPE_OBJECT;
        break;
    case JSON_ARRAY_BEGIN:
        type = JSON_TYPE_ARRAY;
        break;
    case JSON_STRING:
        type = JSON_TYPE_STRING;
        break;
    case JSON_NUMBER:
        type = JSON_TYPE_NUMBER;
        break;
    case JSON_TRUE:
    case JSON_FALSE:
        type = JSON_TYPE_BOOLEAN;
        break;
    case JSON_NULL:
        type = JSON_TYPE_OBJECT;
        break;
    default:
        break;
    }
}

/*!
 * \internal
 * \brief SAX-like hand-written parser for JSON documents.
 */
class JSONSyntaxParser { // TODO: Rename JSONPushParser
    JSONSyntaxHandler handler_;
    void* user_data_;
    JSONRawStringRef text_;

public:
    explicit JSONSyntaxParser(JSONSyntaxHandler handler, void* user_data = 0);
    ~JSONSyntaxParser();

    // TODO: Pull text from some "input stream"
    // TOOD: Allow parsing suspend()/resume() for incomplete "input streams"
    size_t parse(const char* json_utf8_text, size_t len);

private:
    JSONSyntaxParser(const JSONSyntaxParser&);
    JSONSyntaxParser& operator=(const JSONSyntaxParser&);

    // WARNING: JSONPullParser copy/paste's (with slight modifications) these
    //          methods, so changes here must be reflected there, and vice-versa.
    void consume_char(const char*& s, const char*const end, bool at_end_ok = false);
    bool consume_whitespace(const char*& s, const char*const end);
    bool expect_unicode(const char*& s, const char*const end);
    void expect_string(const char*& s, const char*const end, bool is_key = false);
    void expect_literal(const char*& s, const char*const end, const char* lit, int lit_len);
    void expect_number(const char*& s, const char*const end);
    void expect_array_content(const char*& s, const char*const end);
    void expect_object_content(const char*& s, const char*const end);
    void expect_object_element(const char*& s, const char*const end);
    void expect_value(const char*& s, const char*const end);
};

JSONSyntaxParser::JSONSyntaxParser(
    JSONSyntaxHandler handler, void* user_data
) : handler_(handler)
  , user_data_(user_data)
{
}

JSONSyntaxParser::~JSONSyntaxParser()
{
}

void JSONSyntaxParser::consume_char(const char*& s, const char*const end, bool at_end_ok) {
    if (++s >= end) {
        if (!(at_end_ok && s == end)) {
            throw std::runtime_error("Unexpected end-of-value");
        }
    }
}

bool JSONSyntaxParser::consume_whitespace(const char*& s, const char*const end) {
    text_.reset(s);
    const char*const prev_s = s;
    for (; s < end; ++s) {
        unsigned char c = *s;
        if (c > 127 || !is_json_whitespace(c)) {
            if (s > text_.beg_) {
                text_.before_comma_colon_ = (*s == ',') || (*s == ':');
                handler_(JSON_SYNTAX_WHITESPACE, text_.endsAt(s), user_data_);
            }
            return false;
        }
    }
    if (end > prev_s) {
        // text_.before_comma_colon_ implicitly false from reset()
        handler_(JSON_SYNTAX_WHITESPACE, text_.endsAt(end), user_data_);
    }
    return true;
}

// only verifies the codepoint, does not extract its value
bool JSONSyntaxParser::expect_unicode(const char*& s, const char*const end) {
    BOOST_ASSERT(*s == 'u');
    if ((s + 4) >= end) {
        throw std::runtime_error("Truncated Unicode Codepoint");
    }
    if (!isxdigit(*++s) || !isxdigit(*++s) ||
        !isxdigit(*++s) || !isxdigit(*++s)
    ) {
        throw std::runtime_error("Invalid Unicode Codepoint");
    }
    return text_.str_.has_unicode_ = true;
}

// positions \a s one-char past the closing double-quote
void JSONSyntaxParser::expect_string(
    const char*& s, const char*const end, bool is_key
) {
    BOOST_ASSERT(*s == '"');
    text_.reset(s);
    consume_char(s, end);
    //const char* start = s;
    for (unsigned char c = *s; s < end; c = *++s) {
        if (c > 127) {
            // part of a multi-byte UTF-8 encoded string.
            // TODO: Check valid multi-byte.
            text_.str_.not_ascii_ = true;
            continue;
        }
        switch (c) {
        case '"': {
            // Reached the end of the string.
            consume_char(s, end, true);
            handler_(
                is_key? JSON_OBJECT_KEY: JSON_STRING,
                text_.endsAt(s), user_data_
            );
            return;
        }
        case '\\':
            if ((s + 1) >= end) {
                throw std::runtime_error("Unexpected Partial Escape");
            }
            switch (c = *++s) {
            case  'u': // encoded Unicode codepoint must be followed by 4 hex
                JSONSyntaxParser::expect_unicode(s, end);
                break;

            case  '"': case '\\': case  '/':
                text_.str_.has_escape_ = true;
                break;

            case 'b': case 'f': case 'n': case 'r': case 't':
                text_.str_.has_escape_ = true;
                break;

            default: // only above characters are allowed after backslash
                throw std::runtime_error("Invalid String: Unexpected Escape");
            }
            break;

        case '/':
            // Even though forward-slash can be escaped, as per the spec,
            // (and JSONWriter always escape it), it is also valid to have an
            // unescaped one in an encoded JSON string. See Errata ID: 3159
            // from http://www.rfc-editor.org/errata_search.php?rfc=4627
            //throw std::runtime_error("Invalid String: Unescaped forward-slash");
            break;

        default:
            if (!isprint(c)) {
                // should have been encoded in range \u0000-\u001f
                throw std::runtime_error("Invalid String: Unencoded non-printable character");
            }
        }
    }
    // reached the end without seeing the closing double-quote
    throw std::runtime_error("Invalid String: No closing double-quote");
}

void JSONSyntaxParser::expect_literal(
    const char*& s, const char*const end,
    const char* lit, int lit_len
) {
    if ((s + lit_len) > end) {
        throw std::runtime_error("Unexpected end-of-value");
    }
    if (0 != strncmp(s, lit, lit_len)) {
        throw std::runtime_error("Invalid Value");
    }
    text_.reset(s);
    text_.endsAt(s += lit_len);
}

void JSONSyntaxParser::expect_number(const char*& s, const char*const end) {
    BOOST_ASSERT(*s == '-' || isdigit(*s));

    text_.reset(s);

    if (*s == '-') {  // optional
        text_.num_.is_negative_ = true;
        consume_char(s, end);
    }

    // required <int> part
    if (*s == '0') {
        consume_char(s, end, true);
        if (s >= end) {
            // just 0 is possible in naked 0 integer value.
            handler_(JSON_NUMBER, text_.endsAt(s + 1), user_data_);
            return;
        }
        // a leading 0 must be followed by a decimal-point, as leading
        // zeros are not allowed in encoded JSON numbers.
        if (*s != '.' && isdigit(*s)) {
            throw std::runtime_error("Invalid number: Expected decimal-point");
        }
    } else {
        // expect_size_t
        if (!isdigit(*s)) {
            throw std::runtime_error("Invalid number: Expected digits");
        }
        for (unsigned char c = *s; s < end; c = *++s) {
            if (c < '0' || '9' < c) {
                break;
            }
        }
    }

    // optional fractional part (after the dot)
    if (s < end && *s == '.') {
        text_.num_.has_dot_ = true;
        consume_char(s, end); // must be followed by digits
        // consume the digits that follow
        const unsigned char first_c = *s;
        if (first_c < '0' || '9' < first_c) {
            throw std::runtime_error("Invalid number: expected decimals");
        }
        consume_char(s, end, true); // may have more digits
        for (unsigned char c = *s; s < end; c = *++s) {
            if (c < '0' || '9' < c) {
                break;
            }
        }
    }

    // optional exponential part
    if (s < end && (*s == 'e' || *s == 'E')) {
        text_.num_.has_exp_ = true;
        consume_char(s, end); // must be followed by +/- and/or digits
        if (*s == '-' || *s == '+') {
            consume_char(s, end); // must be followed by digits
        }
        const unsigned char first_c = *s;
        if (first_c < '0' || '9' < first_c) {
            throw std::runtime_error("Invalid number: expected decimals");
        }
        consume_char(s, end, true); // may have more digits
        for (unsigned char c = *s; s < end; c = *++s) {
            if (c < '0' || '9' < c) {
                break;
            }
        }
    }

    handler_(JSON_NUMBER, text_.endsAt(s), user_data_);
}

void JSONSyntaxParser::expect_array_content(const char*& s, const char*const end) {
    BOOST_ASSERT(*s == '[');
    handler_(JSON_ARRAY_BEGIN, text_.reset(s).endsAt(s + 1), user_data_);
    consume_char(s, end);

    bool expect_comma = false;
    while (s < end) {
        consume_whitespace(s, end);
        if (*s == ']') {
            handler_(JSON_ARRAY_END, text_.reset(s).endsAt(s + 1), user_data_);
            consume_char(s, end, true);
            return;
        }

        if (expect_comma) {
            if (*s == ',') {
                handler_(JSON_SYNTAX_ARRAY_COMMA, text_.reset(s).endsAt(s + 1), user_data_);
                consume_char(s, end);
            } else {
                throw std::runtime_error("Invalid array: missing comma separator");
            }
        }

        expect_value(s, end);
        expect_comma = true;
    }
    throw std::runtime_error("Unexpected end-of-array");
}

void JSONSyntaxParser::expect_object_element(const char*& s, const char*const end) {
    if (*s != '"') {
        throw std::runtime_error("Missing expected object key");
    }
    expect_string(s, end, true);
    consume_whitespace(s, end);

    if (*s != ':') {
        throw std::runtime_error("Missing expected object key:value separator");
    }
    text_.reset(s);
    consume_char(s, end);
    handler_(JSON_SYNTAX_OBJECT_COLON, text_.endsAt(s), user_data_);
    expect_value(s, end);
}

void JSONSyntaxParser::expect_object_content(const char*& s, const char*const end) {
    BOOST_ASSERT(*s == '{');
    handler_(JSON_OBJECT_BEGIN, text_.reset(s).endsAt(s + 1), user_data_);
    consume_char(s, end);

    bool expect_comma = false;
    while (s < end) {
        consume_whitespace(s, end);
        if (*s == '}') {
            handler_(JSON_OBJECT_END, text_.reset(s).endsAt(s + 1), user_data_);
            consume_char(s, end, true);
            return;
        }

        if (expect_comma) {
            if (*s == ',') {
                handler_(JSON_SYNTAX_OBJECT_COMMA, text_.reset(s).endsAt(s + 1), user_data_);
                consume_char(s, end);
                consume_whitespace(s, end);
            } else {
                throw std::runtime_error("Invalid object: missing comma separator");
            }
        }

        expect_object_element(s, end);
        expect_comma = true;
    }
    throw std::runtime_error("Unexpected end-of-object");
}

void JSONSyntaxParser::expect_value(const char*& s, const char*const end) {
    if (consume_whitespace(s, end)) {
        throw std::runtime_error("Unexpected end-of-value");
    }

    switch (*s) {
    case '{':
        expect_object_content(s, end);
        return;

    case '[':
        expect_array_content(s, end);
        return;

    case '"':
        expect_string(s, end);
        return;

    case '-': // case '+': leading plus not allowed by JSON spec
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        expect_number(s, end);
        return;

    case 't':
        expect_literal(s, end, "true", 4);
        handler_(JSON_TRUE, text_, user_data_);
        return;
    case 'f':
        expect_literal(s, end, "false", 5);
        handler_(JSON_FALSE, text_, user_data_);
        return;
    case 'n':
        expect_literal(s, end, "null", 4);
        handler_(JSON_NULL, text_, user_data_);
        return;
    }

    throw std::runtime_error("Invalid JSON value: Unexpected character");
}

size_t JSONSyntaxParser::parse(const char* json_utf8_text, size_t len) {
    const char* s = json_utf8_text;
    const char*const end = json_utf8_text + len;
    expect_value(s, end);
    consume_whitespace(s, end); // ignore trailing whitespace
    const ptrdiff_t remaining = end - s;
    return (remaining <= 0)? 0: remaining; // zero if we consumed all input
}

/*************************************************************************/

struct ValueHandler {
    JSONHandler& handler_;
    std::string buffer_;

    explicit ValueHandler(
        JSONHandler& handler
    ) : handler_(handler)
    {
        buffer_.reserve(15); // small string opt limit?
    }

    bool handle_whitespace(const JSONRawStringRef& text) {
        buffer_.assign(text.beg_, text.end_);
        return handler_.handle_whitespace(buffer_, text.before_comma_colon_);
    }

    bool handle_string(const JSONRawStringRef& text) {
        buffer_.clear();
        text.decode(buffer_, true);
        return handler_.handle_string(buffer_);
    }

    bool handle_object_key(const JSONRawStringRef& text) {
        buffer_.clear();
        text.decode(buffer_, true);
        return handler_.handle_object_key(buffer_);
    }

    bool handle_number(const JSONRawStringRef& text) {
        // Get the int part.
        const bool is_negative = text.num_.is_negative_;
        const char* beg = is_negative? text.beg_ + 1: text.beg_;
        const char* end = text.end_;
        if (text.num_.has_dot_) {
            end = text.find('.');
            BOOST_ASSERT(end != text.end_);
        } else if (text.num_.has_exp_) {
            end = text.find('e');
            if (end == text.end_) {
                end = text.find('E');
            }
            BOOST_ASSERT(end != text.end_);
        }

        const uint64_t int_part = parser_integer(beg, end);

        if (!text.num_.has_dot_ && !text.num_.has_exp_) {
            // naked integer value
            if (int_part <= (uint64_t)std::numeric_limits<int>::max()) {
                const int as_int32 = static_cast<int>(int_part);
                return handler_.handle_number(is_negative? -as_int32: as_int32);

            } else if (is_negative && int_part == (1 + (uint64_t)std::numeric_limits<int>::max())) {
                // smallest int, i.e. -2,147,483,648 does not fit in absolute value
                // as an int, so we cannot cast int_part into as_int32 as above.
                return handler_.handle_number(std::numeric_limits<int>::min());

            } else if (int_part <= (uint64_t)std::numeric_limits<int64_t>::max()) {
                const int64_t as_int64 = static_cast<int64_t>(int_part);
                return handler_.handle_number(is_negative? -as_int64: as_int64);

            } else if (is_negative && int_part == (1 + (uint64_t)std::numeric_limits<int64_t>::max())) {
                // smallest int64, i.e. -9,223,372,036,854,775,808 does not fit in absolute value
                // as an int64, so we cannot cast int_part into as_int64 as above.
                return handler_.handle_number(std::numeric_limits<int64_t>::min());

            } else if (!is_negative && int_part <= std::numeric_limits<uint64_t>::max()) {
                return handler_.handle_number(int_part);

            } else {
                if (is_negative) {
                    BOOST_ASSERT(int_part > (uint64_t)std::numeric_limits<int64_t>::max() + 1);
                    throw std::runtime_error("Numeric overflow: < MIN_SSIZE_T");
                } else {
                    // we already throw in parse_number(), so should never be reached
                    throw std::runtime_error("Numeric overflow: > MAX_SIZE_T");
                }
            }
        }

        // not an integer, so parse as a double
        const double value = parse_number<double>(text.beg_, text.end_);
        return handler_.handle_number(value);
    }

    static void check(bool rc) {
        if (!rc) {
            throw std::runtime_error("Handler Stop");
        }
    }

    static void handle_event(
        JSONParsingEventType evt, const JSONRawStringRef& text, void* user_data
    ) {
        BOOST_ASSERT(!text.empty());
        ValueHandler& self = *static_cast<ValueHandler*>(user_data);
        switch (evt) {
        case JSON_NULL:         check(self.handler_.handle_null());         break;
        case JSON_TRUE:         check(self.handler_.handle_boolean(true));  break;
        case JSON_FALSE:        check(self.handler_.handle_boolean(false)); break;

        case JSON_STRING:       check(self.handle_string(text));            break;
        case JSON_NUMBER:       check(self.handle_number(text));            break;

        case JSON_OBJECT_BEGIN: check(self.handler_.handle_object_begin()); break;
        case JSON_OBJECT_KEY:   check(self.handle_object_key(text));        break;
        case JSON_OBJECT_END:   check(self.handler_.handle_object_end());   break;

        case JSON_ARRAY_BEGIN:  check(self.handler_.handle_array_begin());  break;
        case JSON_ARRAY_END:    check(self.handler_.handle_array_end());    break;

        // syntax-only events
        case JSON_SYNTAX_WHITESPACE:    check(self.handle_whitespace(text));        break;
        case JSON_SYNTAX_OBJECT_COLON:                                              break;
        case JSON_SYNTAX_OBJECT_COMMA:  check(self.handler_.handle_object_comma()); break;
        case JSON_SYNTAX_ARRAY_COMMA:   check(self.handler_.handle_array_comma());  break;

        default:
            throw std::runtime_error("Unexpected JSON parsing event");
        }
    }

private:
    ValueHandler& operator=(const ValueHandler&);
};

} // namespace

/*************************************************************************/

size_t json_parse_syntax(
    const char* json_utf8_text, size_t len,
    JSONSyntaxHandler handler, void* handler_data
) {
    JSONSyntaxParser parser(handler, handler_data);
    return parser.parse(json_utf8_text, len);
}

/*************************************************************************/

/*! \enum JSONType oes/core/utils/Json.h
 * \brief The possible types of a JSON values.
 *
 * Valid JSON values are:
 * - JSON_TYPE_OBJECT;
 * - JSON_TYPE_ARRAY;
 * - JSON_TYPE_STRING;
 * - JSON_TYPE_NUMBER;
 * - JSON_TYPE_BOOLEAN.
 *
 * In addition, this enum also declares:
 * - JSON_TYPE_INVALID as the \em null (or \em invalid) value type;
 * - JSON_TYPE_ANY as the \em wildcard value type.
 *
 * \sa json_type_of()
 * \sa json_type_string_of()
 * \sa json_type_value_of()
 */

/****************************************************************************/

/*!
 * \brief Determines the type of a JSON expression
 *
 * Parses JSON expression \p value and returns its JSON type. Note that "null" is
 * considered as a separate JSON type in the JSON literature. Here it is
 * considered as an empty constant: if the expression is the string "null",
 * the type object is returned.
 *
 * \param[in] value a string containing a JSON expression
 * \return the JSON type of \p value if the expression is valid, or
 *    JSON_TYPE_INVALID otherwise.
 *
 * \see JSONType
 */
JSONType json_type_of(const std::string& value) {
    try {
        JSONType type = JSON_TYPE_INVALID;
        JSONSyntaxParser parser(type_detection_handler, &type);
        if (0 == parser.parse(value.c_str(), value.length())) {
            return type;
        }
    } catch (const std::runtime_error&) {
        // Do nothing
    }
    return JSON_TYPE_INVALID;
}

/*!
 * \brief Returns the string form of a JSON type
 *
 * \param[in] type_value a JSON type
 * \return the string form of \p type_value if the type is valid, or the empty
 * string otherwise
 */
std::string json_type_string_of(JSONType type_value) {
    return (size_t)type_value < json_type_count ?
        json_type_strings[type_value] :
        json_type_strings[JSON_TYPE_INVALID];
}

/*!
 * \brief Returns the value of a JSON type string
 *
 * \param[in] type_string a JSON type name
 * \return the value of \p type_string as defined in JSONType
 */
JSONType json_type_value_of(const std::string& type_string) {
    for (size_t type_value = 0; type_value < json_type_count; ++type_value) {
        if (type_string == json_type_strings[type_value]) {
            return static_cast<JSONType>(type_value);
        }
    }
    return JSON_TYPE_INVALID;
}

/*************************************************************************/

/*!
 * \brief Parses a JSON document and forwards parsing events to a handler.
 *
 * Instantiates an internal JSON parser to perform low-level tokenizing and
 * parsing of the base JSON grammar, and forwards logical parsing events to
 * a client-supplied \a handler to interpret these events into custom C++ data
 * structures. The parser performs well-formed-ness validation of the JSON
 * document, to make sure the opening and closing of arrays and objects are
 * correct, or the encoding of strings (and object keys) conforms to the JSON
 * specification (see http://tools.ietf.org/html/rfc4627), etc...
 *
 * The \a handler could assemble a generic document in a manner similar to
 * an XML Document Object Model (DOM) tree, or instantiate custom ad-hoc
 * C++ data structures if it accepts only JSON documents of a specific
 * structure; for example to instantiate a std::vector<std::string> from
 * a JSON array of strings, accepting "[]" (an empty array) and "["foo"]"
 * and "["foo", "bar"]", but refusing "{}" (an empty object), or "[1]"
 * (an array of one number), etc...
 *
 * Note that this parser accepts a single primitive value, outside of
 * an array or object, like "1" (a single number), or "true", or ""foo""
 * (a single string).
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \param handler Client-provided handler to receive all logical parsing
 *     events the internal parser will infer from the JSON text document.
 * \return true if \a json_utf8_text was entirely consumed by this parser,
 *     with leading and trailing JSON whitespace ignored; false if unparsed
 *     text remains.
 * \throw std::runtime_error if the JSON document is not well-formed (
 *     truncated, mismatched open/close for array/object, wrongly encoded
 *     string, invalid number like NaN, Infinity, etc...) or if \a handler
 *     returned false to any of the parsing events sent to it.
 */
bool json_parse(const char* json_utf8_text, size_t len, JSONHandler& handler) {
    ValueHandler state(handler);
    JSONSyntaxParser parser(&ValueHandler::handle_event, &state);
    return 0 == parser.parse(json_utf8_text, len);
}

/*************************************************************************/

namespace {

struct KVMapHandler {
    enum State { KV_EXPECT_NONE, KV_EXPECT_OBJ, KV_EXPECT_KEY, KV_EXPECT_VAL };

    std::map<std::string, std::string>& map_;
    JSONType value_type_;
    State state_;
    std::string key_;
    std::string value_;
    bool duplicates_ok_;

    KVMapHandler(
        std::map<std::string, std::string>& map,
        bool duplicates_ok, JSONType value_type
    ) : map_(map)
      , value_type_(value_type)
      , state_(KV_EXPECT_OBJ)
      , duplicates_ok_(duplicates_ok)
    {
    }

    void assert_state_transition(State expected_state, State next_state) {
        if (state_ != expected_state) {
            throw std::runtime_error("Unexpected JSON content");
        }
        state_ = next_state;
    }

    void assert_value_type(JSONType value_type) {
        if (value_type_ != JSON_TYPE_ANY && value_type_ != value_type) {
            throw std::runtime_error("Unexpected JSON content");
        }
    }

    void check(bool inserted) {
        if (!inserted && !duplicates_ok_) {
            throw std::runtime_error("Duplicate JSON key: " + key_);
        }
    }

    void operator()(JSONParsingEventType evt, const JSONRawStringRef& text) {
        switch (evt) {
        case JSON_SYNTAX_WHITESPACE:
        case JSON_SYNTAX_OBJECT_COLON:
        case JSON_SYNTAX_OBJECT_COMMA:
            return;

        case JSON_OBJECT_BEGIN:
            assert_state_transition(KV_EXPECT_OBJ, KV_EXPECT_KEY);
            return;
        case JSON_OBJECT_KEY:
            assert_state_transition(KV_EXPECT_KEY, KV_EXPECT_VAL);
            key_.clear();
            text.decode(key_, true);
            return;
        case JSON_OBJECT_END:
            assert_state_transition(KV_EXPECT_KEY, KV_EXPECT_NONE);
            return;

        case JSON_TRUE:
        case JSON_FALSE:
            assert_state_transition(KV_EXPECT_VAL, KV_EXPECT_KEY);
            assert_value_type(JSON_TYPE_BOOLEAN);
            check(map_.insert(std::make_pair(key_, text.to_string())).second);
            return;
        case JSON_NUMBER:
            assert_state_transition(KV_EXPECT_VAL, KV_EXPECT_KEY);
            assert_value_type(JSON_TYPE_NUMBER);
            check(map_.insert(std::make_pair(key_, text.to_string())).second);
            return;
        case JSON_STRING:
            assert_state_transition(KV_EXPECT_VAL, KV_EXPECT_KEY);
            assert_value_type(JSON_TYPE_STRING);
            value_.clear();
            text.decode(value_, true);
            check(map_.insert(std::make_pair(key_, value_)).second);
            return;

        default: // array events, *and* null
            throw std::runtime_error("Unexpected JSON content");
        }
    }

    static void handle_event(
        JSONParsingEventType evt, const JSONRawStringRef& text, void* user_data
    ) {
        BOOST_ASSERT(!text.empty());
        KVMapHandler& self = *static_cast<KVMapHandler*>(user_data);
        self(evt, text);
    }

private:
    KVMapHandler& operator=(const KVMapHandler&);
};

struct VectorHandler {
    enum State { EXPECT_NONE, EXPECT_ARRAY, EXPECT_VAL };

    std::vector<std::string>& vector_;
    JSONType value_type_;
    State state_;
    std::string value_;

    VectorHandler(std::vector<std::string>& vector, JSONType value_type) :
        vector_(vector),
        value_type_(value_type),
        state_(EXPECT_ARRAY)
    {
    }

    void assert_state_transition(State expected_state, State next_state) {
        if (state_ != expected_state) {
            throw std::runtime_error("Unexpected JSON content");
        }
        state_ = next_state;
    }

    void assert_value_type(JSONType value_type) {
        if (value_type_ != JSON_TYPE_ANY && value_type_ != value_type) {
            throw std::runtime_error("Unexpected JSON content");
        }
    }

    void operator()(JSONParsingEventType evt, const JSONRawStringRef& text) {
        switch (evt) {
        case JSON_SYNTAX_WHITESPACE:
        case JSON_SYNTAX_ARRAY_COMMA:
            return;

        case JSON_ARRAY_BEGIN:
            assert_state_transition(EXPECT_ARRAY, EXPECT_VAL);
            return;
        case JSON_ARRAY_END:
            assert_state_transition(EXPECT_VAL, EXPECT_NONE);
            return;

        case JSON_TRUE:
        case JSON_FALSE:
            assert_state_transition(EXPECT_VAL, EXPECT_VAL);
            assert_value_type(JSON_TYPE_BOOLEAN);
            vector_.push_back(text.to_string());
            return;
        case JSON_NUMBER:
            assert_state_transition(EXPECT_VAL, EXPECT_VAL);
            assert_value_type(JSON_TYPE_NUMBER);
            vector_.push_back(text.to_string());
            return;
        case JSON_STRING:
            assert_state_transition(EXPECT_VAL, EXPECT_VAL);
            assert_value_type(JSON_TYPE_STRING);
            value_.clear();
            text.decode(value_, true);
            vector_.push_back(value_);
            return;

        default: // object events, *and* null
            throw std::runtime_error("Unexpected JSON content");
        }
    }

    static void handle_event(
        JSONParsingEventType evt, const JSONRawStringRef& text, void* user_data
    ) {
        BOOST_ASSERT(!text.empty());
        VectorHandler& self = *static_cast<VectorHandler*>(user_data);
        self(evt, text);
    }

private:
    VectorHandler& operator=(const VectorHandler&);
};

} // namespace

/*!
 * \brief Parses a JSON document expected to be a list of key/values.
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \param key_values the parsed key/values.
 * \param throw_on_duplicates (Optional; defaults to false) whether a
 *     duplicate key triggers an exception or not. When false, the first
 *     key's value prevails over subsequent values for the same key.
 * \param value_type allowed value types, which can be JSON_TYPE_ANY,
 *   JSON_TYPE_STRING, JSON_TYPE_NUMBER or JSON_TYPE_BOOLEAN. If value_type is
 *   JSON_TYPE_ANY (the default), the vector accepts values of string,
 *   number and boolean JSON types. Otherwise, values not of the specified
 *   type cause the parsing to fail.
 * \return true if \a json_utf8_text was entirely consumed by this parser,
 *     with leading and trailing JSON whitespace ignored; false if unparsed
 *     text remains.
 *
 * \throw std::runtime_error if the JSON document is not well-formed (
 *     truncated, mismatched open/close for array/object, wrongly encoded
 *     string, invalid number like NaN, Infinity, etc...), or contains a
 *     duplicate key and \a throw_on_duplicates is true.
 *
 * \see json_write(JSONWriter&, const std::map<std::string, V>&)
 */
bool json_parse(
    const char* json_utf8_text, size_t len,
    std::map<std::string, std::string>& key_values,
    bool throw_on_duplicates, JSONType value_type
) {
    KVMapHandler state(key_values, !throw_on_duplicates, value_type);
    JSONSyntaxParser parser(&KVMapHandler::handle_event, &state);
    return 0 == parser.parse(json_utf8_text, len);
}

/*!
 * \brief Parses a JSON document expected to be a vector of values.
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \param values the parsed vector of values.
 * \param value_type allowed value types, which can be JSON_TYPE_ANY,
 *   JSON_TYPE_STRING, JSON_TYPE_NUMBER or JSON_TYPE_BOOLEAN. If value_type is
 *   JSON_TYPE_ANY (the default), the vector accepts values of string,
 *   number and boolean JSON types. Otherwise, values not of the specified
 *   type cause the parsing to fail.
 * \return true if \a json_utf8_text was entirely consumed by this parser,
 *     with leading and trailing JSON whitespace ignored; false if unparsed
 *     text remains.
 *
 * \throw std::runtime_error if the JSON document is not well-formed (
 *     truncated, mismatched open/close for array/object, wrongly encoded
 *     string, invalid number like NaN, Infinity, etc...), or contains a
 *     duplicate key and \a throw_on_duplicates is true.
 *
 * \see json_write(JSONWriter&, const std::vector<V>&)
 */
bool json_parse(
    const char* json_utf8_text, size_t len,
    std::vector<std::string>& values, JSONType value_type
) {
    VectorHandler state(values, value_type);
    JSONSyntaxParser parser(&VectorHandler::handle_event, &state);
    return 0 == parser.parse(json_utf8_text, len);
}

/*************************************************************************/

/*! \enum JSONParsingEventType oes/core/utils/Json.h
 * \brief The possible types of a JSON parsing event (token).
 *
 * Primitive JSON value tokens:
 * - JSON_NULL;
 * - JSON_TRUE;
 * - JSON_FALSE;
 * - JSON_NUMBER;
 * - JSON_STRING.
 *
 * Structured JSON value tokens (object and array):
 * - JSON_OBJECT_BEGIN;
 * - JSON_OBJECT_KEY;
 * - JSON_OBJECT_END;
 * - JSON_ARRAY_BEGIN;
 * - JSON_ARRAY_END.
 *
 * Syntax-related tokens:
 * - JSON_SYNTAX_WHITESPACE;
 * - JSON_SYNTAX_OBJECT_COLON;
 * - JSON_SYNTAX_OBJECT_COMMA;
 * - JSON_SYNTAX_ARRAY_COMMA.
 *
 * And finally, the JSONReader-specific JSON_END indicating end-of-stream.
 *
 * \sa json_parse_syntax()
 * \sa JSONReader::next()
 */

/*************************************************************************/

/*! \class JSONParserOptions oes/core/utils/Json.h
 * \brief Options for JSONReader.
 */

/*!
 * \brief Instantiates default options and limits for JSON parsing.
 */
JSONParserOptions::JSONParserOptions(
) : allow_scalar_(true)
  , syntax_mode_(false)
  , ignore_trailing_content_(false)
  , max_depth_              (1<<8)  // 256
  , max_array_size_         (1<<24) // 16M
  , max_object_size_        (1<<16) // 64K
  , max_string_byte_size_   (1<<24) // 16M
  , max_objkey_byte_size_   (1<<16) // 64K
{}

/*! \var JSONParserOptions::allow_scalar_
 * \brief Whether to allow parsing a primitive value on its one.
 *
 * Strict JSON requires a outer JSON object.
 * This extension allows \em naked primitive values.
 *
 * Default to true.
 */

/*! \var JSONParserOptions::syntax_mode_
 * \brief Whether to report whitespace and syntax related parsing events.
 *
 * Allows to reformat (indent, add or strip whitespace, etc...) JSON documents.
 *
 * Default to false.
 */

/*! \var JSONParserOptions::ignore_trailing_content_
 * \brief Whether to continue parsing past the top-level object or array.
 *
 * If true, reports JSON_END as soon as the top-level object or array is
 * \em closed (JSON_OBJECT_END or JSON_ARRAY_END), even if there is still
 * textual content available.
 *
 * Default to false.
 */

/*! \var JSONParserOptions::max_depth_
 * \brief Maximum nesting level (depth) allowed during parsing.
 *
 * Default to 256 levels.
 */

/*! \var JSONParserOptions::max_array_size_
 * \brief Maximum number of values within an array.
 *
 * Default to 2^24 entries (around 16 millions).
 */

/*! \var JSONParserOptions::max_object_size_
 * \brief Maximum number of key-value-pairs within an object.
 *
 * Default to 2^16 entries (around 64 thousands).
 */

/*! \var JSONParserOptions::max_string_byte_size_
 * \brief Maximum raw byte size of a string primitive value.
 *
 * This value does not match the maximum character of the strings, because
 * of UTF-8 encoding, and the use of escape characters (\\t, \\n, etc...) and
 * unicode escapes, but the byte length of the string as it appears in the
 * document (excluding the double-quotes required around the string).
 *
 * Default to 2^24 bytes (around 16 millions bytes).
 */

/*! \var JSONParserOptions::max_objkey_byte_size_
 * \brief Maximum raw byte size of an object-key string value.
 *
 * This value does not match the maximum character of the strings, because
 * of UTF-8 encoding, and the use of escape characters (\\t, \\n, etc...) and
 * unicode escapes, but the byte length of the string as it appears in the
 * document (excluding the double-quotes required around the string).
 *
 * Default to 2^16 bytes (around 64 thousands bytes).
 */

/*************************************************************************/

/*! \internal
 *
 * Partial state of a JSONPullParser which can be cheaply captured and which
 * JSONPullParser takes as explicit argument, to use the same code on either
 * the parser's own (full) state, or a partial snapshot state, to efficiently
 * implement peek().
 *
 * FIXME: Introduce JSONTokenizer instead, for efficient peek()ing.
 */
struct JSONPullParserDynamicState {
    const char* s = nullptr;

    JSONParsingEventType evt_ = JSON_END;
    JSONRawStringRef text_;

    bool is_peek(const JSONPullParserDynamicState& other) const {
        return this != &other;
    }

    const char* set_event(JSONParsingEventType evt) {
        evt_ = evt;
        // Keep a prev event?
        return nullptr; // no error
    }
};

//! \internal Pull parser implementation behind JSONReader.
struct JSONPullParser : JSONPullParserDynamicState {
    // Inherit from (partial) dynamic state, instead of composing it,
    // to have less code to update. Should go away with JSONTokenizer.
    using DynamicState = JSONPullParserDynamicState;

    std::weak_ptr<JSONPullParser> parent_;

    JSONParserOptions options_;

    const char* uft8_ = nullptr;
    const char* end = nullptr;
    size_t len_ = 0;

    // TODO: Cleanup, move to header (as JSONFrame),
    //       and expose as JSONReader::backtrace()
    struct Frame {
        explicit Frame(char type = 0) : type_(type), expect_key_(true), count_(0) {}
        char type_; //!< the frame type, either array '[' or object '{'
        bool expect_key_; //<! expect object key or value?
        size_t count_; //<! expect first entry? Requires leading comma when 0

        // TODO: Add frame start location (line:column)
    };
    std::vector<Frame> frames_;

    // WARNING: JSONSyntaxParser copy/paste's (with slight modifications) these
    //          methods, so changes here must be reflected there, and vice-versa.
    bool eof(DynamicState& _);
    bool consume_whitespace(DynamicState& _);
    void consume_char(DynamicState& _, bool at_end_ok = false);
    bool consume_unicode();

    JSONParsingEventType next(DynamicState& _);

    const char* expect_value(DynamicState& _);
    const char* expect_number();
    const char* expect_literal(DynamicState& _, const char* lit, int lit_len, JSONParsingEventType expected);
    const char* expect_string(bool is_key);
    const char* expect_array();
    const char* expect_array_content(DynamicState& _);
    const char* expect_object();
    const char* expect_object_content(DynamicState& _);

    JSONParsingEventType verify(const DynamicState& _, const char* err_msg);
    void verify(bool condition, const char* err_msg);
    void verify_event(JSONParsingEventType expected);

    void get_integer(int* i4_value, int64_t* i8_value, uint64_t* u8_value);
};

/*************************************************************************/

bool JSONPullParser::eof(DynamicState& _) {
    return _.s >= end || consume_whitespace(_);
}

JSONParsingEventType JSONPullParser::verify(const DynamicState& _, const char* err_msg) {
    if (err_msg) {
        throw std::runtime_error(err_msg);
    }
    return _.evt_;
}

void JSONPullParser::verify(bool condition, const char* err_msg) {
    if (!condition) {
        throw std::runtime_error(err_msg);
    }
}

void JSONPullParser::verify_event(JSONParsingEventType expected) {
    verify(expected == evt_, "Incompatible type requested");
}

void JSONPullParser::consume_char(DynamicState& _, bool at_end_ok) {
    if (++_.s >= end) {
        verify(at_end_ok && _.s == end, "Unexpected end-of-value");
    }
}

bool JSONPullParser::consume_whitespace(DynamicState& _) {
    _.text_.reset(_.s);
    const char*const prev_s = _.s;
    for (unsigned char c = *_.s; _.s < end; c = *++_.s) {
        if (c > 127 || !is_json_whitespace(c)) {
            if (_.s > _.text_.beg_) {
                _.text_.before_comma_colon_ = (*_.s == ',') || (*_.s == ':');
                _.text_.endsAt(_.s);
                _.evt_ = JSON_SYNTAX_WHITESPACE;
                // TODO: Syntax Mode
            }
            return false;
        }
    }
    if (end > prev_s) {
        // text_.before_comma_colon_ implicitly false from reset()
        _.text_.endsAt(_.s);
        _.evt_ = JSON_SYNTAX_WHITESPACE;
        // TODO: Syntax Mode
    }
    return true; // EOF
}

const char* JSONPullParser::expect_literal(
    DynamicState& _, const char* lit, int lit_len, JSONParsingEventType expected
) {
    if ((_.s + lit_len) > end) {
        return "Unexpected end-of-value";
    }
    if (0 != strncmp(_.s, lit, lit_len)) {
        return "Invalid Value";
    }
    _.text_.reset(_.s);
    _.text_.endsAt(_.s += lit_len);
    if (options_.ignore_trailing_content_ && frames_.empty()) {
        end = s; // force JSON_END on next next() call
    }
    return _.set_event(expected);
}

const char* JSONPullParser::expect_number() {
    BOOST_ASSERT(*s == '-' || isdigit(*s));

    text_.reset(s);

    if (*s == '-') {  // optional
        text_.num_.is_negative_ = true;
        consume_char(*this);
    }

    // required <int> part
    if (*s == '0') {
        consume_char(*this, true);
        if (s >= end) {
            // just 0 is possible in naked 0 integer value.
            text_.endsAt(s + 1);
            if (options_.ignore_trailing_content_ && frames_.empty()) {
                end = s; // force JSON_END on next next() call
            }
            return set_event(JSON_NUMBER);
        }
        // a leading 0 must be followed by a decimal-point, as leading
        // zeros are not allowed in encoded JSON numbers.
        if (*s != '.' && isdigit(*s)) {
            return "Invalid number: Expected decimal-point";
        }
    } else {
        // expect_size_t
        if (!isdigit(*s)) {
            return "Invalid number: Expected digits";
        }
        for (unsigned char c = *s; s < end; c = *++s) {
            if (c < '0' || '9' < c) {
                break;
            }
        }
    }

    // optional fractional part (after the dot)
    if (s < end && *s == '.') {
        text_.num_.has_dot_ = true;
        consume_char(*this); // must be followed by digits
        // consume the digits that follow
        const unsigned char first_c = *s;
        if (first_c < '0' || '9' < first_c) {
            return "Invalid number: expected decimals";
        }
        consume_char(*this, true); // may have more digits
        for (unsigned char c = *s; s < end; c = *++s) {
            if (c < '0' || '9' < c) {
                break;
            }
        }
    }

    // optional exponential part
    if (s < end && (*s == 'e' || *s == 'E')) {
        text_.num_.has_exp_ = true;
        consume_char(*this); // must be followed by +/- and/or digits
        if (*s == '-' || *s == '+') {
            consume_char(*this); // must be followed by digits
        }
        const unsigned char first_c = *s;
        if (first_c < '0' || '9' < first_c) {
            return "Invalid number: expected decimals";
        }
        consume_char(*this, true); // may have more digits
        for (unsigned char c = *s; s < end; c = *++s) {
            if (c < '0' || '9' < c) {
                break;
            }
        }
    }

    text_.endsAt(s);
    if (options_.ignore_trailing_content_ && frames_.empty()) {
        end = s; // force JSON_END on next next() call
    }
    return set_event(JSON_NUMBER);
}

void JSONPullParser::get_integer(int* i4_value, int64_t* i8_value, uint64_t* u8_value) {
    // Get the int part.
    const bool is_negative = text_.num_.is_negative_;
    const char* beg = is_negative? text_.beg_ + 1: text_.beg_;
    const char* tmp_end = text_.end_;
    if (text_.num_.has_dot_) {
        tmp_end = text_.find('.');
        BOOST_ASSERT(tmp_end != text_.end_);
    } else if (text_.num_.has_exp_) {
        tmp_end = text_.find('e');
        if (tmp_end == text_.end_) {
            tmp_end = text_.find('E');
        }
        BOOST_ASSERT(tmp_end != text_.end_);
    }

    const uint64_t int_part = parser_integer(beg, tmp_end);

    if (!text_.num_.has_dot_ && !text_.num_.has_exp_) {
        // naked integer value
        if (int_part <= (uint64_t)std::numeric_limits<int>::max()) {
            const int as_int32 = static_cast<int>(int_part);
            const int value = is_negative? -as_int32: as_int32;
            if (i4_value) { *i4_value = value; return; }
            if (i8_value) { *i8_value = value; return; } // widening
            if (u8_value) { verify(!is_negative, "Sign mismatch"); *u8_value = int_part; return; }

        } else if (is_negative && int_part == (1 + (uint64_t)std::numeric_limits<int>::max())) {
            // smallest int, i.e. -2,147,483,648 does not fit in absolute value
            // as an int, so we cannot cast int_part into as_int32 as above.
            const int value = std::numeric_limits<int>::min();
            if (i4_value) { *i4_value = value; return; }
            if (i8_value) { *i8_value = value; return; } // widening
            verify(false, "Cannot get negative integer as unsigned");

        } else if (int_part <= (uint64_t)std::numeric_limits<int64_t>::max()) {
            const int64_t as_int64 = static_cast<int64_t>(int_part);
            const int64_t value = is_negative? -as_int64: as_int64;
            if (i4_value) { verify(false, "Integer Overflow"); return; }
            if (i8_value) { *i8_value = value; return; }
            if (u8_value) { verify(!is_negative, "Sign mismatch"); *u8_value = int_part; return; }

        } else if (is_negative && int_part == (1 + (uint64_t)std::numeric_limits<int64_t>::max())) {
            // smallest int64, i.e. -9,223,372,036,854,775,808 does not fit in absolute value
            // as an int64, so we cannot cast int_part into as_int64 as above.
            const int64_t value = std::numeric_limits<int64_t>::min();
            if (i4_value) { verify(false, "Integer Overflow"); return; }
            if (i8_value) { *i8_value = value; return; }
            verify(false, "Cannot get negative integer as unsigned");

        } else if (!is_negative && int_part <= std::numeric_limits<uint64_t>::max()) {
            if (i4_value) { verify(false, "Integer Overflow"); return; }
            if (i8_value) { verify(false, "Integer Overflow"); return; }
            if (u8_value) { *u8_value = int_part; return; }

        } else {
            if (is_negative) {
                BOOST_ASSERT(int_part > (uint64_t)std::numeric_limits<int64_t>::max() + 1);
                verify(false, "Numeric overflow: < MIN_SSIZE_T");
            } else {
                // we already throw in parse_number(), so should never be reached
                verify(false, "Numeric overflow: > MAX_SIZE_T");
            }
        }
    }
}

// only verifies the codepoint, does not extract its value
bool JSONPullParser::consume_unicode() {
    BOOST_ASSERT(*s == 'u');
    verify((s + 4) < end, "Truncated Unicode Codepoint");
    verify(
        isxdigit(*++s) && isxdigit(*++s) &&
        isxdigit(*++s) && isxdigit(*++s),
        "Invalid Unicode Codepoint"
    );
    return text_.str_.has_unicode_ = true;
}

// positions \a s one-char past the closing double-quote
const char* JSONPullParser::expect_string(bool is_key) {
    BOOST_ASSERT(*s == '"');
    text_.reset(s);
    consume_char(*this);
    const size_t max_len = is_key?
        options_.max_objkey_byte_size_: options_.max_string_byte_size_;
    // Note: +1 below to account for terminating quote
    const char*const s_end = std::min<>(s + max_len + 1, end);
    for (unsigned char c = *s; s < s_end; c = *++s) {
        if (c > 127) {
            // part of a multi-byte UTF-8 encoded string.
            // TODO: Check valid multi-byte.
            text_.str_.not_ascii_ = true;
            continue;
        }
        switch (c) {
        case '"': {
            // Reached the end of the string.
            consume_char(*this, true);
            text_.endsAt(s);
            if (!is_key && options_.ignore_trailing_content_ && frames_.empty()) {
                end = s; // force JSON_END on next next() call
            }
            return set_event(is_key? JSON_OBJECT_KEY: JSON_STRING);
        }
        case '\\':
            if ((s + 1) >= s_end) {
                if (s_end < end) {
                    return "JSON maximum string length exceeded";
                } else {
                    return "Unexpected Partial Escape";
                }
            }
            switch (c = *++s) {
            case  'u': // encoded Unicode codepoint must be followed by 4 hex
                consume_unicode();
                break;

            case  '"': case '\\': case  '/':
                text_.str_.has_escape_ = true;
                break;

            case 'b': case 'f': case 'n': case 'r': case 't':
                text_.str_.has_escape_ = true;
                break;

            default: // only above characters are allowed after backslash
                return "Invalid String: Unexpected Escape";
            }
            break;

        case '/':
            // Even though forward-slash can be escaped, as per the spec,
            // (and JSONWriter always escape it), it is also valid to have an
            // unescaped one in an encoded JSON string. See Errata ID: 3159
            // from http://www.rfc-editor.org/errata_search.php?rfc=4627
            //return "Invalid String: Unescaped forward-slash";
            break;

        default:
            if (!isprint(c)) {
                // should have been encoded in range \u0000-\u001f
                return "Invalid String: Unencoded non-printable character";
            }
        }
    }
    if (s_end < end) {
        return "JSON maximum string length exceeded";
    }
    // reached the end without seeing the closing double-quote
    return "Invalid String: No closing double-quote";
}

const char* JSONPullParser::expect_array() {
    BOOST_ASSERT(*s == '[');
    verify(frames_.size() < options_.max_depth_, "JSON maximum depth exceeded");
    frames_.push_back(Frame('['));
    text_.reset(s).endsAt(s + 1);
    consume_char(*this);
    return set_event(JSON_ARRAY_BEGIN);
}

const char* JSONPullParser::expect_array_content(DynamicState& _) {
    Frame& frame = frames_.back();
    BOOST_ASSERT('[' == frame.type_);

    const bool peeking = _.is_peek(*this);

    verify(_.s < end, "Unexpected end-of-array");
    consume_whitespace(_);
    if (*_.s == ']') {
        if (!peeking) {
            frames_.pop_back();
        }
        _.text_.reset(_.s).endsAt(_.s + 1);
        consume_char(_, true);
        if (!peeking && options_.ignore_trailing_content_ && frames_.empty()) {
            end = _.s; // force JSON_END on next next() call
        }
        return _.set_event(JSON_ARRAY_END);
    }

    const bool first = (frame.count_ == 0);
    // If we expect a comma
    if (!first) {
        if (*_.s == ',') {
            // TODO: Syntax Mode
            //handler_(JSON_SYNTAX_ARRAY_COMMA, text_.reset(s).endsAt(s + 1), user_data_);
            consume_char(_);
        } else {
            return "Invalid array: missing comma separator";
        }
    }

    verify(
        frame.count_ < options_.max_array_size_,
        "JSON maximum array entry count exceeded"
    );
    if (!peeking) {
        ++frame.count_;
    }
    return expect_value(_);
}

const char* JSONPullParser::expect_object() {
    BOOST_ASSERT(*s == '{');
    verify(frames_.size() < options_.max_depth_, "JSON maximum depth exceeded");
    frames_.push_back(Frame('{'));
    text_.reset(s).endsAt(s + 1);
    consume_char(*this);
    return set_event(JSON_OBJECT_BEGIN);
}

const char* JSONPullParser::expect_object_content(DynamicState& _) {
    Frame& frame = frames_.back();
    BOOST_ASSERT('{' == frame.type_);

    const bool peeking = _.is_peek(*this);

    verify(_.s < end, "Unexpected end-of-object");
    consume_whitespace(_);
    if (*_.s == '}') {
        verify(frame.expect_key_ == true, "Unexpected end-of-object");
        if (!peeking) {
            frames_.pop_back();
        }
        _.text_.reset(_.s).endsAt(_.s + 1);
        consume_char(_, true);
        if (!peeking && options_.ignore_trailing_content_ && frames_.empty()) {
            end = _.s; // force JSON_END on next next() call
        }
        return _.set_event(JSON_OBJECT_END);
    }

    const bool first = (frame.count_ == 0);
    // If we expect a comma
    if (!first && frame.expect_key_) {
        if (*_.s == ',') {
            // TODO: Syntax Mode
            //handler_(JSON_SYNTAX_OBJECT_COMMA, text_.reset(s).endsAt(s + 1), user_data_);
            consume_char(_);
            consume_whitespace(_);
        } else {
            return "Invalid object: missing comma separator";
        }
    }

    if (frame.expect_key_) {
        verify(*_.s == '"', "Missing expected object key");
        if (peeking) {
            return _.set_event(JSON_OBJECT_KEY);
        }
        frame.expect_key_ = false;
        return expect_string(true);
    } else {
        verify(*_.s == ':', "Missing expected object key:value separator");
        _.text_.reset(_.s);
        consume_char(_);
        // TODO: Syntax Mode
        //handler_(JSON_SYNTAX_OBJECT_COLON, text_.endsAt(s), user_data_);

        verify(
            frame.count_ < options_.max_object_size_,
            "JSON maximum object key-value-pair count exceeded"
        );
        if (!peeking) {
            ++frame.count_;
            frame.expect_key_ = true;
        }
        return expect_value(_);
    }
}

const char* JSONPullParser::expect_value(DynamicState& _) {
    if (consume_whitespace(_)) {
        return "Unexpected end-of-value";
    }

    const bool peeking = _.is_peek(*this);

    switch (*_.s) {
    case '{':
        return peeking? _.set_event(JSON_OBJECT_BEGIN): expect_object();

    case '[':
        return peeking? _.set_event(JSON_ARRAY_BEGIN): expect_array();

    case '"':
        return peeking? _.set_event(JSON_STRING): expect_string(false);

    case '-': // case '+': leading plus not allowed by JSON spec
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return peeking? _.set_event(JSON_NUMBER): expect_number();

    case 't':
        return expect_literal(_, "true", 4, JSON_TRUE);
    case 'f':
        return expect_literal(_, "false", 5, JSON_FALSE);
    case 'n':
        return expect_literal(_, "null", 4, JSON_NULL);
    }

    return "Invalid JSON value: Unexpected character";
}

JSONParsingEventType JSONPullParser::next(DynamicState& _) {
    const char state = frames_.empty()? 0: frames_.back().type_;

    switch (state) {
    case  0 : return eof(_)? _.evt_ = JSON_END: // force evt_ to END, for current()
                     verify(_, expect_value(_));
    case '[': return verify(_, expect_array_content(_));
    case '{': return verify(_, expect_object_content(_));
    }

    verify(false, "Unknown error");
    return JSON_END; // unreachable
}

/*************************************************************************/

struct JSONReader::SavePointImpl {
    const JSONReader* owner_; //cppcheck-suppress unusedStructMember
    JSONPullParser parser_;
};

/*************************************************************************/

/*! \class JSONReader oes/core/utils/Json.h
 * \brief Reads (parses) a JSON document.
 *
 * JavaScript Object Notation (JSON) is a text format for the
 * serialization of structured data.  It is derived from the object
 * literals of JavaScript, as defined in the ECMAScript Programming
 * Language Standard, Third Edition.
 *
 * JSON can represent four primitive types (strings, numbers, booleans,
 * and null) and two structured types (objects and arrays).
 *
 * A string is a sequence of zero or more Unicode characters [UNICODE].
 *
 * An object is an unordered collection of zero or more name/value
 * pairs, where a name is a string and a value is a string, number,
 * boolean, null, object, or array.
 *
 * An array is an ordered sequence of zero or more values.
 *
 * JSONReader will \em incrementally parse the document, each time its next()
 * method is called, returning the type of JSON value seen, and providing
 * accessors to extract typed C/C++ values corresponding to those JSON values.
 *
 * JSONReader's API mimics JDK 7's javax.json.stream.JsonParser, except its
 * next() method returns JSON_END instead of having an has_next() method.
 *
 * JSONReader supports a limited number of options to customize its behavior
 * and in particular a set of limits to how large values can be.
 *
 * For example, the JSON document {"version":"1.2.3","values":[1,2.3,true,null,{}]}
 * will generate the following parsing events when passed to parse_json() below:
 * \code
 * void parse_json(const std::string& doc) {
 *     JSONReader json(doc.c_str(), doc.size());
 *     json.next()          // ==> JSON_OBJECT_BEGIN
 *     // 1st key-value pair
 *     json.next()          // ==>   JSON_OBJECT_KEY
 *     json.get_string()    // ==>   std::string("version")
 *     json.next()          // ==>   JSON_STRING
 *     json.get_string()    // ==>   std::string("1.2.3")
 *     // 2nd key-value pair
 *     json.next()          // ==>   JSON_OBJECT_KEY
 *     json.get_string()    // ==>   std::string("values")
 *     json.next()          // ==>   JSON_ARRAY_BEGIN
 *     // array content
 *     json.next()          // ==>     JSON_NUMBER
 *     json.is_integral()   // ==>     true
 *     json.get_int()       // ==>     1
 *     json.next()          // ==>     JSON_NUMBER
 *     json.is_integral()   // ==>     false
 *     json.get_int()       // ==>     2.3
 *     json.next()          // ==>     JSON_TRUE
 *     json.get_boolean()   // ==>     true
 *     json.next()          // ==>     JSON_NULL
 *     json.next()          // ==>     JSON_OBJECT_BEGIN
 *     json.next()          // ==>     JSON_OBJECT_END
 *
 *     json.next()          // ==>   JSON_ARRAY_END
 *     json.next()          // ==> JSON_OBJECT_END
 *     json.next()          // ==> JSON_END
 * }
 * \endcode
 *
 * JSONReader is often called a \em PULL or \em STREAMING parser, because
 * client code \em pulls events from the parser, as opposed to using
 * json_parse(const char*,size_t,JSONHandler&) which uses a \em PUSH parser
 * which calls back on a client-code-provided \em handler. A push parser
 * decides which client-code handler method to call (handle_number(int) vs
 * handle_number(uint64_t)), whereas control is reversed with JSONReader and
 * its pull-parsing model, the client-code decide how which number-based
 * method to call, and can even access the token() directly.
 *
 * Pull-parsing makes it easy to nest parsing code, by delegating parsing of
 * a JSON document piece (typically an object) to another method, thus writing
 * Recursive Decent Parsers with ease.
 *
 * \note This reader allows reading a single \em primitive value (either a
 *     number, a string, a boolean, or null) by itself, outside of an array
 *     or an object, as an extension, when JSONParserOptions::allow_scalar_
 *     is true (the default).
 *
 * \sa http://tools.ietf.org/html/rfc4627
 * \sa http://docs.oracle.com/javaee/7/api/javax/json/stream/JsonParser.html
 */

/*!
 * \brief Instantiates a new JSON reader (pull parser).
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document to read.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \param options the parsing options and limits.
 */
JSONReader::JSONReader(
    const char* json_utf8_text, size_t len,
    const JSONParserOptions& options
) : impl_(new JSONPullParser)
{
    impl_->uft8_ = json_utf8_text;
    impl_->len_ = len;

    impl_->s = json_utf8_text;
    impl_->end = json_utf8_text + len;

    impl_->options_ =  options;
}

//! \internal Instantiates nested parser
JSONReader::JSONReader(
    const std::shared_ptr<JSONPullParser>& outer_parser
) : impl_(new JSONPullParser)
{
    BOOST_ASSERT(outer_parser->s <= outer_parser->text_.end_);

    // Do a peek on outer_parser. Note that we are not doing switch (peek())
    // in JSONReader::scope() because we also need the token to advance the
    // nested parser to the start of the next value.
    JSONPullParserDynamicState state(*outer_parser); // truncation on purpose
    JSONParsingEventType next_evt = outer_parser->next(state);
    switch (next_evt) {
    case JSON_NULL:
    case JSON_TRUE:
    case JSON_FALSE:
    case JSON_STRING:
    case JSON_NUMBER:
    case JSON_OBJECT_BEGIN:
    case JSON_ARRAY_BEGIN:
        break;

    default:
        throw std::runtime_error(
            "JSONReader: must expect JSON value on scope()"
        );
    }

    impl_->uft8_ = state.text_.beg_;
    impl_->parent_ = outer_parser;
    impl_->len_ = (outer_parser->end - impl_->uft8_);

    impl_->s = impl_->uft8_;
    impl_->end = outer_parser->end;

    impl_->options_ = outer_parser->options_;
    impl_->options_.ignore_trailing_content_ = true;
    impl_->options_.max_depth_ -= outer_parser->frames_.size();
}

/*!
 * \brief Deletes this JSON reader.
 */
JSONReader::~JSONReader() {
    if (std::shared_ptr<JSONPullParser> parent = impl_->parent_.lock()) {
        // update parent's "partial" state with the one of the nested parser
        (JSONPullParserDynamicState&)(*parent) = (JSONPullParserDynamicState&)(*impl_);

        if (!impl_->frames_.empty()) { // most likely on error
            parent->frames_.insert(
                parent->frames_.end(),
                impl_->frames_.begin(), impl_->frames_.end()
            );
        } else if (!parent->frames_.empty()) {
            // FIXME: This "patches up" the frame a posteriori, and the fact
            // that it is necessary is not a good sign...
            JSONPullParser::Frame& frame = parent->frames_.back();
            switch (frame.type_) {
            case '{':
                frame.expect_key_ = true;
                // fall-thru
            case '[':
                ++frame.count_;
                break;

            default:
                OES_DEBUG_ASSERT(false);
            }
        }
    }
    impl_.reset();
}

/*!
 * \brief Tests the type of the token
 * \param expected expected token type
 * \param received received token type
 */
bool JSONReader::test_token_type(JSONParsingEventType expected, JSONParsingEventType received) {
    if (expected != received) {
        return false;
    }
    return true;
}

/*!
 * \brief Tests the token string
 * \param expected expected string
 * \param received received string
 */
bool JSONReader::test_token_string(const std::string& expected, const std::string& received) {
    if (expected != received) {
        return false;
    }
    return true;
}

/*!
 * \brief Gets the type for the next parsing state,
 *     without \em advancing this reader.
 *
 * This method behaves exactly as next() (i.e. can throw exceptions on JSON
 * syntax errors), except it does not modify the parsing state at all.
 *
 * \return JSON_END if positioned at the end of the document to parse;
 *     the token type (parsing event type) otherwise.
 * \throws std::runtime_error on invalid JSON syntax, or when a parsing
 *     limit is exceeded.
 *
 * \note peek() is similar to a \em 1-event savepoint, i.e. it is equivalent
 *     to creating a \em savepoint, calling next() and recording the result,
 *     then call rollback_to(savepoint).
 */
JSONParsingEventType JSONReader::peek() const {
    JSONPullParserDynamicState state(*impl_); // truncation on purpose
    return impl_->next(state);
}

/*!
 * \brief Gets the type for the next parsing state.
 *
 * JSONReader keeps track of parsing states to determine which valid parsing
 * state(s) can follow the previous one.
 *
 * \return JSON_END if positioned at the end of the document to parse;
 *     the token type (parsing event type) otherwise.
 * \throws std::runtime_error on invalid JSON syntax, or when a parsing
 *     limit is exceeded.
 */
JSONParsingEventType JSONReader::next() {
    return impl_->next(*impl_);
}

/*!
 * \brief Gets the type for the current parsing state.
 *
 * Unlike next(), current() does not \em advance to the next token, but instead
 * returns the same event than the previous next() call returned;
 *
 * \return what the previous call to next() returned. If previous next() threw
 *     a C++ exception, the result is undefined and should not be depended on.
 *
 * \sa next()
*/
JSONParsingEventType JSONReader::current() {
    return impl_->evt_;
}

/*!
 * \brief Creates a nested JSONReader, to \em isolate reading a \em child value.
 *
 * Parsing non-trivial JSON documents typically involves calling json_parse()
 * helper methods to delegate parsing subsets of a document, typically
 * corresponding to a given type. For example, parsing a color-map implemented
 * in C++ as a std::map<std::string, MyColor> where MyColor is a struct made of
 * the RGB components of the color can be represented in JSON as
 * \code
 * {
 *   "white": { "r": 1.0, "g": 1.0, "b": 1.0 }
 *   "cyan": { "r": 0.0, "g": 1.0, "b": 1.0 }
 *   ...
 * }
 * \endcode
 *
 * Instead of writing the JSON writing/parsing code for MyColor in the color-map
 * parsing code, one can delegate parsing the color to
 * json_parse(JSONReader& json, MyColor& color), to be reused by color-map and
 * any other client code that deals in MyColor. But when delegating such parsing
 * to other code, one runs the risk of the json_parse() method reading too many
 * JSON events/tokens (i.e. making too many next() calls), stepping out of the
 * JSON \b value being read. The scope() method creates a special \em nested
 * JSONReader instance, still connected to its \em parent (outer) JSONReader,
 * which won't allow the parsing code to \em overstep the JSON \em boundaries
 * of the value being read. (it returns JSON_END as soon as the value ends).
 * Once the called code returns, the nested parser must be deleted to resume
 * parsing on the outer/parent JSONReader, either explicitly via .reset():
 * \code
 * JSONReader json(...);
 * std::map<std::string, MyColor> colormap;
 * json.next(); // ==> JSON_OBJECT_BEGIN
 * while (json.peek() != JSON_OBJECT_END) {
 *     json.next(); // ==> JSON_OBJECT_KEY
 *     std::string name = json.get_key();
 *     MyColor c;
 *     JSONReader::Scope nested(json.scope());
 *     const bool ok = json_parse(*nested, c); // check ok
 *     nested.reset();
 * }
 * \endcode
 * or implicitly if used inline:
 * \code
 * ...
 * while (json.peek() != JSON_OBJECT_END) {
 *     ...
 *     MyColor c;
 *     const bool ok = json_parse(*json.scope(), c); // check ok
 * }
 * \endcode
 * (.scope() returns an std::shared which remains \em alive for the duration of
 * the statement, until the semicolon when it is implicitly deleted, \em closing
 * the nested scope/reader).
 *
 * A scope can only be created when expecting a JSON value, i.e. when reading
 * array values, or the value after an object key.
 *
 * \return A nested JSONReader.
 * \throw std::runtime_error when this JSONReader's is not expected a JSON
 *     value next.
 *
 * \warning a scope \b must be deleted before calling methods of the parent
 *     reader again
 */
JSONReader::Scope JSONReader::scope() {
    return JSONReader::Scope(new JSONReader(impl_));
}

/*!
 * \brief Takes a \em snapshot of the reader, to possibly go back to.
 *
 * This is similar to a SQL transaction savepoint, that you can rollback_to()
 * in case an error occurs after the savepoint was taken, to attempt or retry
 * to complete the \em transaction (JSON parsing in this case) successfully.
 *
 * \return the savepoint on success; null otherwise.
 */
JSONReader::SavePoint JSONReader::savepoint() {
    // Not sure whether we should enforce specific points in the
    // JSON document when savepoints can be taken or not...
    return SavePoint(new SavePointImpl { this, *impl_ });
}

/*!
 * \brief Rollbacks this parser to a previous saved state.
 *
 * Savepoints are used to recover from parsing failures, typically in nested
 * parsing code. One creates a JSONSavePoint before entering the nested parsing
 * code, and on failure of that code, rollback_to() the savepoint, and then
 * call skip_next() for example in an attempt to parse the invalid \em value
 * and go to the next value.
 *
 * \param savepoint the previously created savepoint, typically on the stack.
 * \return true on a successful rollback; false otherwise, if \sa savepoint
 *     was not created from this parser, or we can no longer go back to the
 *     savepoint because this parser advanced past the array or object the
 *     savepoint was created in.
 *
 * \sa peek(), an implicit \em 1-event savepoint.
 */
bool JSONReader::rollback_to(const SavePoint& savepoint) {
    OES_ASSERT(savepoint, return false;);
    OES_ASSERT(savepoint->owner_ == this, return false;);
    OES_ASSERT(savepoint->parser_.frames_.size() <= depth(), return false;);
    // TODO: check savepoint's frames_ are a prefix to this frames_
    *impl_ = savepoint->parser_;
    return true; // savepoint deleted here
}

/*!
 * \brief Skips the next JSON value.
 *
 * Convenience method equivalent to \code next(); skip_current() \endcode.
 * See skip_current() for details.
 *
 * \return the number of next() calls made to skip the value.
 * \note this method throws on any JSON syntax error, like next() does.
 */
size_t JSONReader::skip_next() {
    next();
    return skip_current();
}

/*!
 * \brief Skips the current JSON value.
 *
 * Scalar values do not call next(), and return one.
 *
 * Object/array values \b must be on their opening event (JSON_OBJECT_BEGIN
 * and JSON_ARRAY_BEGIN respectively), and are skipped in full, irrespective
 * of content (even nested content).
 *
 * \return the number of next() calls made to skip the value,
 *     plus one for the current event.
 * \throws if next() throws on syntax errors, or the current event is neither
 *     a scalar value (an object-key is not considered scalar), nor the beginning
 *     of an object or array.
 */
size_t JSONReader::skip_current() {
    // TODO: Syntax mode
    switch (current()) {
    case JSON_END:
        return 0;

    case JSON_NULL:
    case JSON_TRUE:
    case JSON_FALSE:
    case JSON_STRING:
    case JSON_NUMBER:
        return 1;

    case JSON_OBJECT_BEGIN: {
        const size_t curr_depth = depth();
        size_t count = 2;
        for (next(); depth() >= curr_depth; next()) ++count;
        impl_->verify_event(JSON_OBJECT_END);
        return count;
    }

    case JSON_ARRAY_BEGIN: {
        const size_t curr_depth = depth();
        size_t count = 2;
        for (next(); depth() >= curr_depth; next()) ++count;
        impl_->verify_event(JSON_ARRAY_END);
        return count;
    }

    default:
        impl_->verify(false, "Cannot skip partial JSON value");
        return 0;
    }
}

/*!
 * \brief Gets the current token.
 *
 * After JSONReader properly identified the token type, as returned by next(),
 * client code typically calls one of the get_*() methods, but can also access
 * the raw text for the token and interpret it as a integer or double or
 * boolean itself. This can be used for example to get an integer larger than
 * uint64_t, and store it in some \em BigNum class specific to the client code.
 *
 * \return the raw text of the current parsing state, i.e. token.
 *
 * \note invalid to call after next() threw an std::runtime_error
 *     or returned JSON_END.
 */
JSONToken JSONReader::token() {
    return impl_->text_; // truncation on purpose
}

/*!
 * \brief How many nested object or arrays did we parse so far.
 *
 * Increments each time we encounter a JSON_OBJECT_BEGIN or JSON_ARRAY_BEGIN
 * event; Similarly decrements on every JSON_OBJECT_END or JSON_ARRAY_END.
 *
 * \return the current object/array parsing depth.
 */
size_t JSONReader::depth() {
    return impl_->frames_.size();
}

/*!
 * \brief How many object key-value pair or array values read so far,
 *     at the current depth.
 *
 * Every time depth() changes value, count() also likely changes, since count()
 * is depth()-dependent. Resets to zero after JSON_OBJECT_BEGIN or
 * JSON_ARRAY_BEGIN, and restores the previous depth's count (+ 1) after
 * JSON_OBJECT_END or JSON_ARRAY_END.
 *
 * \return the number of \em values in the current object/array read so far.
 */
size_t JSONReader::count() {
    return impl_->frames_.empty()? (impl_->eof(*impl_)? 1: 0): impl_->frames_.back().count_;
}

/*!
 * \brief Determines whether the current token is an integral number.
 *
 * Note that JSONReader supports large numbers at parsing, but is limited
 * to C/C++ datatypes for returning numbers. Access token() directly, and
 * convert it to some \em BigNum class in client-code to overcome this
 * limitation.
 *
 * \return true if integral; false otherwise (i.e. floating-point).
 * \throws std::runtime_error when previous next() did not return JSON_NUMBER,
 *     or is_integral() is false.
 *
 * \note It is not an error to call get_float() or get_double() for integral
 *     values, but can result in roundoff approximations for large integers;
 *
 * \sa get_int()
 * \sa get_int64_t()
 * \sa get_uint64_t()
 */
bool JSONReader::is_integral() {
    impl_->verify_event(JSON_NUMBER);
    return !impl_->text_.num_.has_exp_ && !impl_->text_.num_.has_dot_;
}

/*!
 * \brief Gets the current integral number as a C/C++ int.
 *
 * \return the int value.
 * \throws std::runtime_error when previous next() did not return JSON_NUMBER,
 *     or is_integral() is false, or the integral values overflows an int.
 *
 * \sa is_integral()
 * \sa token()
 */
int JSONReader::get_int() {
    impl_->verify(is_integral(), "Not an integer");
    int value = 0;
    impl_->get_integer(&value, nullptr, nullptr);
    return value;
}

/*!
 * \brief Gets the current integral number as a C/C++ int64_t.
 *
 * \return the int64_t value.
 * \throws std::runtime_error when previous next() did not return JSON_NUMBER,
 *     or is_integral() is false, or the integral values overflows an int.
 *
 * \sa is_integral()
 * \sa token()
 */
int64_t JSONReader::get_int64_t() {
    impl_->verify(is_integral(), "Not an integer");
    int64_t value = 0;
    impl_->get_integer(nullptr, &value, nullptr);
    return value;
}

/*!
 * \brief Gets the current integral number as a C/C++ uint64_t.
 *
 * \return the uint64_t value.
 * \throws std::runtime_error when previous next() did not return JSON_NUMBER,
 *     or is_integral() is false, or the integral values overflows an uint64_t.
 *
 * \sa is_integral()
 * \sa token()
 */
uint64_t JSONReader::get_uint64_t() {
    impl_->verify(is_integral(), "Not an integer");
    uint64_t value = 0;
    impl_->get_integer(nullptr, nullptr, &value);
    return value;
}

/*!
 * \brief Gets the current number as a C/C++ float.
 *
 * \return the float value (possibly integral).
 * \throws std::runtime_error when previous next() did not return JSON_NUMBER;
 *     Or the conversion overflows a float, as per C++ I/O Stream semantic.
 *
 * \note Text-to-Floating-Point conversion uses C++ I/O Stream, but using
 *     std::istream::operator>>(float&). Client code can perform its own
 *     conversion by accessing the token() directly.
 *
 * \sa is_integral()
 * \sa token()
 */
float JSONReader::get_float() {
    impl_->verify_event(JSON_NUMBER);
    return parse_number<float>(impl_->text_.beg_, impl_->text_.end_);
}

/*!
 * \brief Gets the current number as a C/C++ double.
 *
 * \return the double value (possibly integral).
 * \throws std::runtime_error when previous next() did not return JSON_NUMBER;
 *     Or the conversion overflows a float, as per C++ I/O Stream semantic.
 *
 * \note Text-to-Floating-Point conversion uses C++ I/O Stream, but using
 *     std::istream::operator>>(double&). Client code can perform its own
 *     conversion by accessing the token() directly.
 *
 * \sa is_integral()
 * \sa token()
 */
double JSONReader::get_double() {
    impl_->verify_event(JSON_NUMBER);
    return parse_number<double>(impl_->text_.beg_, impl_->text_.end_);
}

/*!
 * \brief Gets the current string, possibly decoded.
 *
 * \return the string value (possibly an object key).
 * \throws std::runtime_error when previous next() did not return
 *     JSON_STRING or JSON_OBJECT_KEY.
 *
 * \warning neither next() or get_string(), in case of JSON_OBJECT_KEY,
 *     check the key's unicity within the current object. Client code
 *     must test this unicity itself, if required.
 *
 * \sa token()
 */
std::string JSONReader::get_string() {
    impl_->verify( // TODO: accept JSON_STRING only
        impl_->evt_ == JSON_STRING || impl_->evt_ == JSON_OBJECT_KEY,
        "Incompatible type requested"
    );
    std::string buffer;
    buffer.reserve(impl_->text_.size());
    impl_->text_.decode(buffer, true);
    return buffer;
}

/*!
 * \brief Gets the current string, accepting a null literal.
 *
 * Convenience method returning an empty string on a null literal,
 * and get_string() otherwise.
 *
 * \sa JSONWriter::string(const std::string&, bool)
 */
std::string JSONReader::get_string_or_null() {
    if (impl_->evt_ == JSON_NULL) {
        return std::string();
    }
    return get_string();
}

/*!
 * \brief Gets the current JSON_TRUE or JSON_FALSE as a C/C++ bool.
 *
 * Convience method, since the token type itself carries the value.
 *
 * \return true when next() returned JSON_TRUE;
 *     false when it returned JSON_FASLE.
 * \throws std::runtime_error when previous next()
 *     did not return JSON_TRUE or JSON_FALSE.
 *
 * \note true and false are not considered \em integral.
 *
 * \sa is_integral()
 */
bool JSONReader::get_boolean() {
    impl_->verify(
        impl_->evt_ == JSON_TRUE || impl_->evt_ == JSON_FALSE,
        "Incompatible type requested"
    );
    return (impl_->evt_ == JSON_TRUE)? true: false;
}

/*!
 * \brief Gets the current \em decoded object key.
 *
 * This method must return the key by value, in case the raw JSON text uses
 * escape sequences or unicode characters. For simple pure-ASCII keys, prefer
 * is_key() which can avoid the key copy when no decoding is necessary.
 *
 * \return the object key.
 * \throws std::runtime_error when previous next() did not return JSON_OBJECT_KEY.
 *
 * \warning neither next() or get_string(), in case of JSON_OBJECT_KEY,
 *     check the key's unicity within the current object. Client code
 *     must test this unicity itself, if required.
 *
 * \sa is_key()
 * \sa JSONToken::matches_unquote()
 */
std::string JSONReader::get_key() {
    impl_->verify_event(JSON_OBJECT_KEY);
    std::string buffer;
    buffer.reserve(impl_->text_.size());
    impl_->text_.decode(buffer, true);
    return buffer;
}

/*!
 * \brief Convenience method equivalent to is_key(key, strlen(key)).
 *
 * \param key the null-terminated key to test against.
 * \return true when \a key is the current object key; false otherwise.
 * \throws std::runtime_error when previous next() did not return JSON_OBJECT_KEY.
 */
bool JSONReader::is_key(const char* key) {
    return is_key(key, strlen(key));
}

/*!
 * \brief Tests whether the current object key matches a given string literal.
 *
 * For simple pure-ASCII keys, this method is faster than get_key() == key.
 *
 * \param key the key (not necessarily null-terminated) key to test against.
 * \param len the key len.
 * \return true when \a key is the current object key; false otherwise.
 * \throws std::runtime_error when previous next() did not return JSON_OBJECT_KEY.
 *
 * \sa get_key()
 * \sa JSONToken::matches_unquote()
 */
bool JSONReader::is_key(const char* key, size_t len) {
    impl_->verify_event(JSON_OBJECT_KEY);
    if (impl_->text_.str_.not_ascii_ ||
        impl_->text_.str_.has_escape_ ||
        impl_->text_.str_.has_unicode_
    ) {
        return get_key() == key;
    }
    return impl_->text_.matches_unquote(key, len);
}

/*************************************************************************/

/*!
 * \brief Validates a JSON document is well-formed.
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \return true if \a json_utf8_text was entirely consumed by this parser,
 *     with leading and trailing JSON whitespace ignored, and the text in
 *     between was a valid well-formed JSON document; false otherwise.
 *
 * \note Does not check number underflow/overflow, only that the string
 *     representation is consistent with a number.
 */
bool json_validate_syntax(const char* json_utf8_text, size_t len) {
    try {
        JSONSyntaxParser parser(do_nothing_handler);
        return 0 == parser.parse(json_utf8_text, len);
    } catch (const std::runtime_error&) {
        return false;
    }
}

/*************************************************************************/

namespace {

struct FormatHandler {
    std::ostream& os_;
    std::string key_sep_;
    std::vector<std::string> indents_; // FIXME: Use C++11 std::array instead
    std::string indentbuf_;
    std::string* curr_indent_;
    JSONParsingEventType prev_evt_;
    int level_;

    explicit FormatHandler(
        std::ostream& os,
        const std::string& indent = "\t",
        const std::string& key_sep = ": "
    ) : os_(os)
      , key_sep_(key_sep)
      , curr_indent_(nullptr)
      , prev_evt_(JSON_SYNTAX_WHITESPACE)
      , level_(0)
    {
        // check valid indent
        for (char c : indent) {
            if (!is_json_whitespace(c)) {
                throw std::runtime_error("Invalid JSON indent: not spaces");
            }
        }

        // check valid key/value separator
        bool hasColon = false;
        for (char c : key_sep) {
            if (c == ':') {
                if (hasColon) {
                    throw std::runtime_error("Invalid JSON key/value separator");
                }
                hasColon = true;
            } else if (!is_json_whitespace(c)) {
                throw std::runtime_error("Invalid JSON key/value separator");
            }
        }
        if (!hasColon) {
            throw std::runtime_error("Invalid JSON key/value separator");
        }

        // pre-compute some indents
        indents_.reserve(16);
        indents_.push_back("");
        indents_.push_back(indent);
        for (size_t i = 2; i < indents_.capacity(); ++i) {
            // FIXME: use C++11 emplace_back() instead of push_back()
            indents_.push_back(indents_[i - 1] + indent);
        }
        curr_indent_ = &indents_[0];
    }

    void update_indent() {
        if (level_ < static_cast<int>(indents_.size())) {
            curr_indent_ = &indents_[level_];
        } else {
            indentbuf_.clear();
            indentbuf_.reserve(level_ * indents_[1].size());
            size_t remaining = level_ + 1;
            for (; remaining >= indents_.size(); remaining -= indents_.size()) {
                indentbuf_.append(indents_.back());
            }
            if (remaining) {
                indentbuf_.append(indents_[remaining]);
            }
            curr_indent_ = &indentbuf_;
        }
    }

    void operator()(JSONParsingEventType evt, const JSONRawStringRef& text) {
        // adjust the level
        switch (evt) {
        case JSON_SYNTAX_WHITESPACE:
            return; // ignore existing whitespace

        case JSON_SYNTAX_OBJECT_COLON:
            os_ << key_sep_;
            return;

        case JSON_OBJECT_BEGIN:
        case JSON_ARRAY_BEGIN:
            if (prev_evt_ != JSON_SYNTAX_OBJECT_COLON) {
                os_ << *curr_indent_;
            }
            os_ << text << '\n';
            ++level_;
            update_indent();
            return;

        case JSON_OBJECT_END:
        case JSON_ARRAY_END:
            // FIXME: Special case empty array/object, avoiding indent?
            --level_;
            update_indent();
            if (prev_evt_ != JSON_OBJECT_BEGIN && prev_evt_ != JSON_ARRAY_BEGIN) {
                os_ << '\n';
            }
            os_ << *curr_indent_ << text;
            return;

        case JSON_SYNTAX_OBJECT_COMMA:
        case JSON_SYNTAX_ARRAY_COMMA:
            os_ << text << '\n';
            return;

        default:
            if (prev_evt_ != JSON_SYNTAX_OBJECT_COLON) {
                os_ << *curr_indent_;
            }
            os_ << text;
        }
    }

    static void handle_event(
        JSONParsingEventType evt, const JSONRawStringRef& text, void* user_data
    ) {
        BOOST_ASSERT(!text.empty());
        FormatHandler& self = *static_cast<FormatHandler*>(user_data);
        self(evt, text);
        if (evt != JSON_SYNTAX_WHITESPACE) {
            self.prev_evt_ = evt;
        }
    }

private:
    FormatHandler& operator=(const FormatHandler&);
};

} // namespace

/*!
 * \brief Formats (pretty-prints) a JSON document.
 *
 * All existing whitespace is ignored, replaced by new whitespace which indent
 * each value based on its array/object nesting level.
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document to format.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \param[out] os the output stream to write the whitespace-less equivalent document.
 * \param indent (Optional; default to tab) the \em unit indent whitespace string.
 * \param key_sep (Optional; default to colon-space) the object key-value separator.
 * \return true if \a json_utf8_text was entirely consumed by this parser,
 *     with leading and trailing JSON whitespace ignored, and the text in
 *     between was a valid well-formed JSON document; false otherwise.
 */
bool json_format(
    const char* json_utf8_text, size_t len, std::ostream& os,
    const std::string& indent, const std::string& key_sep
) {
    try {
        FormatHandler handler(os, indent, key_sep);
        return 0 == json_parse_syntax(
            json_utf8_text, len, &FormatHandler::handle_event, &handler
        );
    } catch (const std::runtime_error&) {
        return false;
    }
}

size_t json_format_multi_part(
    const char* json_utf8_text, size_t len, std::ostream& os,
    const std::string& indent, const std::string& key_sep
) {
    FormatHandler handler(os, indent, key_sep);
    const char* end = json_utf8_text + len;
    size_t remaining = len;
    size_t part_count = 0;
    do {
        OES_ASSERT(remaining <= len, return part_count;);
        const char* from = end - remaining;
        remaining = json_parse_syntax(
            from, remaining, &FormatHandler::handle_event, &handler
        );
        os << std::endl;
        ++part_count;
    } while (remaining > 0);
    return part_count;
}

/*!
 * \brief Formats (pretty-prints) a JSON document.
 *
 * All existing whitespace is ignored, replaced by new whitespace which indent
 * each value based on its array/object nesting level.
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document to format.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \param indent (Optional; default to tab) the \em unit indent whitespace string.
 * \param key_sep (Optional; default to colon-space) the object key-value separator.
 * \return the formatted document on success; an empty string otherwise.
 */
std::string json_format(
    const char* json_utf8_text, size_t len,
    const std::string& indent, const std::string& key_sep
) {
    std::ostringstream oss;
    return json_format(json_utf8_text, len, oss, indent, key_sep)? oss.str(): "";
}

/*************************************************************************/

/*!
 * \brief Removes all insignificant whitespace from a JSON document.
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \param[out] os the output stream to write the whitespace-less equivalent document.
 * \return true if \a json_utf8_text was entirely consumed by this parser,
 *     with leading and trailing JSON whitespace ignored, and the text in
 *     between was a valid well-formed JSON document; false otherwise.
 *
 * \note Resulting document is just one (possibly very long) line.
 */
bool json_remove_whitespace(
    const char* json_utf8_text, size_t len, std::ostream& os
) {
    try {
        JSONSyntaxParser parser(print_no_whitespace_handler, &os);
        return 0 == parser.parse(json_utf8_text, len);
    } catch (const std::runtime_error&) {
        return false;
    }
}

/*!
 * \brief Removes all insignificant whitespace from a JSON document.
 *
 * \param json_utf8_text UTF-8 encoded text of the JSON document.
 * \param len the byte-length of the text in \a json_utf8_text.
 * \return the whitespace-less document on success; an empty string otherwise.
 *
 * \note Resulting document is just one (possibly very long) line.
 */
std::string json_remove_whitespace(const char* json_utf8_text, size_t len) {
    std::ostringstream oss;
    return json_remove_whitespace(json_utf8_text, len, oss)? oss.str(): "";
}

/*************************************************************************/

/*!
 * \fn template <typename T> std::string to_json_text(const T&)
 * \brief Serializes a value to JSON text.
 *
 * Convenience function that expects a json_write(JSONWriter& json, const T& t)
 * overload to exist, and is equivalent to the following code snippet:
 * \code
 *    std::ostringstream oss;
 *    JSONWriter json(oss);
 *    json_write(json, t);
 *    return oss.str();
 * \endcode
 *
 * \tparam T the type to serialize to JSON thanks to json_write() overload
 * \param t the value or object to serialize.
 * \return the serialized JSON text.
 *
 * \note The json_write() overload for T must be either in the same namespace
 *     as T (to be found via Argument-Dependent-Lookup, also known as Koenig
 *     Lookup), or be declared in the oes::core namespace.
 *
 * \sa JSONWriter
 * \sa json_parse(const char*, size_t, JSONHandler&)
 */

/*!
 * \fn template<typename V> void json_write(JSONWriter&, const std::map<std::string, V>&)
 * \brief Writes a map of key/value pairs.
 *
 * Writes key/value pairs \p key_values to the JSON writer \p json as a JSON
 * object. This function expects a json_write(JSONWriter& json, const V&) to
 * write the map values. Overloads already exist for all JSON basic types.
 * (See notes in to_json_text()).
 *
 * The resulting document can be later parsed with json_parse().
 *
 * \tparam V type of the values in the map
 * \param json the JSON writer to write to.
 * \param key_values a map of key/value pairs
 *
 * \see json_parse(const char*, size_t, std::map<std::string, std::string>&, bool)
 */

/*!
 * \fn template<typename V> void json_write(JSONWriter&, const std::vector<V>&)
 *
 * \tparam V type of the values in the vector
 * \param json the JSON writer to write to
 * \param values a vector of values
 * \sa JSONWriter::values(const C&)
 */

/*!
 * \fn template<typename V> void json_write(JSONWriter&, const std::set<V>&)
 * \tparam V type of the values in the set
 * \param json the JSON writer to write to.
 * \param values a set of values
 * \sa JSONWriter::values(const C&)
 */

/*!
 * \fn template<typename V, std::size_t N> void json_write(JSONWriter& json, const boost::array<V, N>& values)
 * \tparam V type of the values in the array
 * \param json the JSON writer to write to.
 * \param values an array of values
 * \sa JSONWriter::values(const C&)
 */

/*************************************************************************/

} // namespace core
} // namespace oes
