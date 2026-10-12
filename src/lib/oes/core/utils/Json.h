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

#ifndef OES_CORE_TYPES_CORE_JSON_H
#define OES_CORE_TYPES_CORE_JSON_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/std_fwd.h>

#include <map>
#include <memory>
#include <iosfwd>
#include <stdint.h>
#include <string>
#include <vector>

#include <oes/core/utils/VersionNumber.h>

namespace oes {
namespace core {

/****************************************************************************/

struct OES_CORE_UTILS_EXPORT JSONWriterOptions {
    // TODO: format option: indent, all-ascii, floating-point formatting, ...

    VersionNumber version_;

    bool omit_defaults_ = false;

    bool null_on_empty_ = false;
};

/****************************************************************************/

class OES_CORE_UTILS_EXPORT JSONWriter {
public:
    explicit JSONWriter(
        std::ostream& os, const JSONWriterOptions& options = JSONWriterOptions()
    );
#if (__cplusplus > 199711L) || (defined (_MSVC_LANG) && (_MSVC_LANG >= 201402L))
    ~JSONWriter() noexcept(false) ; // no meant to be extended/derived
#else
    ~JSONWriter() ; // no meant to be extended/derived
#endif

    bool ok() const;
    void reset();

    const JSONWriterOptions& options() const;

    JSONWriter& number(int value);
    JSONWriter& number(int64_t value);
    JSONWriter& number(uint64_t value);
    JSONWriter& number(unsigned int value);
#ifdef _WIN32
    JSONWriter& number(unsigned long value);
#endif
    JSONWriter& number(float value);
    JSONWriter& number(double value);
    JSONWriter& string(const std::string& value, bool null_on_empty = false);
    JSONWriter& json_bool(bool value);
    JSONWriter& json_null();
    JSONWriter& json_true();
    JSONWriter& json_false();

    JSONWriter& push_array();
    JSONWriter& pop_array();

    JSONWriter& push_object();
    JSONWriter& key(const std::string& key);
    JSONWriter& pop_object();

    template <typename V> JSONWriter& value(const V& value); // see ./json.hpp
    template <typename C> JSONWriter& values(const C& container);
    template <typename C> JSONWriter& size(const C& container);
    JSONWriter& size(size_t value);

    JSONWriter& whitespace(const std::string& ws, bool before_comma_colon);

private:
    JSONWriter& operator=(const JSONWriter&);
    JSONWriter(const JSONWriter&);

    void flush_pending_ws();
    void assert_expects_key();
    void assert_expects_value();
    template <typename T> JSONWriter& push_value(T t);

private:
    std::ostream& os_;
    std::vector<char> end_;
    std::vector<char> pending_ws_;
    JSONWriterOptions options_;
};

OES_FWD_DECL_SHARED_PTR(JSONWriter);

inline JSONWriter& JSONWriter::size(size_t value) {
    return number(value);
}

/****************************************************************************/

class OES_CORE_UTILS_EXPORT JSONHandler {
public:
    const bool return_code;

public:
    explicit JSONHandler(bool return_code = false);
    virtual ~JSONHandler();

    virtual bool handle_object_begin();
    virtual bool handle_object_key(const std::string& key);
    virtual bool handle_object_comma();
    virtual bool handle_object_end();

    virtual bool handle_array_begin();
    virtual bool handle_array_comma();
    virtual bool handle_array_end();

    virtual bool handle_number(int);
    virtual bool handle_number(int64_t);
    virtual bool handle_number(uint64_t);
    virtual bool handle_number(double value);
    virtual bool handle_string(const std::string& value);
    virtual bool handle_boolean(bool value);
    virtual bool handle_null();

    virtual bool handle_whitespace(const std::string& ws, bool before_comma_colon);

private: // no assignment
    JSONHandler& operator=(const JSONHandler&);
};

OES_FWD_DECL_SHARED_PTR(JSONHandler);

OES_CORE_UTILS_EXPORT JSONHandler_ptr json_handler_writer(std::ostream& os);
OES_CORE_UTILS_EXPORT JSONHandler_ptr json_handler_writer(const JSONWriter_ptr& writer);

/****************************************************************************/

struct OES_CORE_UTILS_EXPORT JSONToken {
    const char* beg_;
    const char* end_;

    explicit JSONToken(const char* begin = 0, const char* end = 0);

    bool empty() const;
    size_t size() const;
    bool is_quoted() const;
    const char* find(char c) const;
    bool matches(const char* text, size_t len) const;
    bool matches_unquote(const char* text, size_t len) const;

    std::string to_string(bool unquote = false) const;
    std::ostream& print(std::ostream& os) const;
};

inline std::ostream& operator<<(std::ostream& os, const JSONToken& token) {
    return token.print(os);
}

/****************************************************************************/

// TODO: Merge with JSONToken, collapsing union into opaque uint flags_
struct OES_CORE_UTILS_EXPORT JSONRawStringRef : public JSONToken {
    union {
        struct {
            bool is_negative_;
            bool has_dot_;
            bool has_exp_;
        } num_;

        struct {
            bool not_ascii_;
            bool has_escape_;
            bool has_unicode_;
        } str_;

        bool before_comma_colon_;
    };

    explicit JSONRawStringRef(const char* begin = 0, const char* end = 0);

    JSONRawStringRef& reset(const char* begin);
    JSONRawStringRef& endsAt(const char* end);

    void decode(std::string& buffer, bool unquote = false) const;
};

/****************************************************************************/

// TODO: Rename JSONTokenType
enum JSONParsingEventType {
    //! Special end-of-document token.
    JSON_END = 0,

    // Value tokens.
    JSON_NULL,
    JSON_TRUE,
    JSON_FALSE,
    JSON_STRING,
    JSON_NUMBER,

    JSON_OBJECT_BEGIN,
    JSON_OBJECT_KEY,
    JSON_OBJECT_END,

    JSON_ARRAY_BEGIN,
    JSON_ARRAY_END,

    // Syntax-only tokens (not emitted by JSONReader by default).
    JSON_SYNTAX_WHITESPACE,
    JSON_SYNTAX_OBJECT_COLON,
    JSON_SYNTAX_OBJECT_COMMA,
    JSON_SYNTAX_ARRAY_COMMA
};

typedef void (*JSONSyntaxHandler)(
    JSONParsingEventType evt, const JSONRawStringRef& text, void* handler_data
);

OES_CORE_UTILS_EXPORT size_t json_parse_syntax(
    const char* json_utf8_text, size_t len,
    JSONSyntaxHandler handler, void* handler_data
);

/****************************************************************************/

struct OES_CORE_UTILS_EXPORT JSONParserOptions {
    JSONParserOptions();

    bool allow_scalar_; // TODO
    bool syntax_mode_;  // TODO
    bool ignore_trailing_content_;

    // limits (to avoid Denial-Of-Service attacks)
    size_t max_depth_;
    size_t max_array_size_;
    size_t max_object_size_;
    size_t max_string_byte_size_;
    size_t max_objkey_byte_size_;
};

/****************************************************************************/

struct JSONPullParser;

class OES_CORE_UTILS_EXPORT JSONReader {
private:
    struct SavePointImpl;

public:
    using SavePoint = std::shared_ptr<SavePointImpl>;
    using Scope = std::shared_ptr<JSONReader>;

public:
    // TODO: Ctor taking JSONInput abstraction.
    // TODO: Resumable reader on invalid input.
    // TODO: Line:Column location of errors, and start of token.
    JSONReader(
        const char* json_utf8_text, size_t len,
        const JSONParserOptions& options = JSONParserOptions()
    );
    ~JSONReader();

    JSONParsingEventType peek() const;
    JSONParsingEventType next();
    JSONParsingEventType current();

    Scope scope();

    SavePoint savepoint();
    bool rollback_to(const SavePoint& savepoint);

    size_t skip_next();
    size_t skip_current();

    JSONToken token();
    size_t depth();
    size_t count();

    bool is_integral();

    int get_int();
    int64_t get_int64_t();
    uint64_t get_uint64_t();

    float get_float();
    double get_double();

    std::string get_string();
    std::string get_string_or_null();

    bool get_boolean();

    std::string get_key();
    bool is_key(const char* key);
    bool is_key(const char* key, size_t len);

    template <typename F> bool parse_array(F lambda); // see ./json.hpp
    template <typename F> bool try_parse_array(F lambda);

    template <typename V> bool try_parse(V& value);
    template <typename V> bool try_parse_member(V& value);

    static bool test_token_type(JSONParsingEventType expected, JSONParsingEventType received);
    static bool test_token_string(const std::string& expected, const std::string& received);

private: // no copy/assignment
    JSONReader& operator=(const JSONReader&) = delete;
    JSONReader(const JSONReader&) = delete;

    explicit JSONReader(const std::shared_ptr<JSONPullParser>& outer_parser);

private:
    std::shared_ptr<JSONPullParser> impl_;
};

/****************************************************************************/

// TODO: Rename JSONValueType
enum JSONType {
    JSON_TYPE_INVALID,
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY,
    JSON_TYPE_STRING,
    JSON_TYPE_NUMBER,
    JSON_TYPE_BOOLEAN,
    JSON_TYPE_ANY
};

OES_CORE_UTILS_EXPORT JSONType json_type_of(const std::string& json_expr);

OES_CORE_UTILS_EXPORT std::string json_type_string_of(JSONType type_value);

OES_CORE_UTILS_EXPORT JSONType json_type_value_of(const std::string& type_string);

/****************************************************************************/

OES_CORE_UTILS_EXPORT bool json_parse(
    const char* json_utf8_text, size_t len, JSONHandler& handler
);

OES_CORE_UTILS_EXPORT bool json_parse(
    const char* json_utf8_text, size_t len,
    std::map<std::string, std::string>& key_values,
    bool throw_on_duplicates = false,
    JSONType value_type = JSON_TYPE_ANY
);

OES_CORE_UTILS_EXPORT bool json_parse(
    const char* json_utf8_text, size_t len,
    std::vector<std::string>& values,
    JSONType value_type = JSON_TYPE_ANY
);

OES_CORE_UTILS_EXPORT bool json_validate_syntax(
    const char* json_utf8_text, size_t len
);

OES_CORE_UTILS_EXPORT bool json_format(
    const char* json_utf8_text, size_t len, std::ostream& os,
    const std::string& indent = "\t", const std::string& key_sep = ": "
);
OES_CORE_UTILS_EXPORT std::string json_format(
    const char* json_utf8_text, size_t len,
    const std::string& indent = "\t", const std::string& key_sep = ": "
);
OES_CORE_UTILS_EXPORT size_t json_format_multi_part(
    const char* json_utf8_text, size_t len, std::ostream& os,
    const std::string& indent = "\t", const std::string& key_sep = ": "
);

OES_CORE_UTILS_EXPORT bool json_remove_whitespace(
    const char* json_utf8_text, size_t len, std::ostream& os
);
OES_CORE_UTILS_EXPORT std::string json_remove_whitespace(
    const char* json_utf8_text, size_t len
);

/****************************************************************************/

inline void json_write(JSONWriter& json, bool value) {
    json.json_bool(value);
}
inline void json_write(JSONWriter& json, int value) {
    json.number(value);
}
inline void json_write(JSONWriter& json, uint64_t value) {
    json.number(value);
}
inline void json_write(JSONWriter& json, int64_t value) {
    json.number(value);
}
inline void json_write(JSONWriter& json, unsigned int value) {
    json.number(value);
}
#ifdef _WIN32
inline void json_write(JSONWriter& json, unsigned long value) {
    json.number(value);
}
#endif
inline void json_write(JSONWriter& json, float value) {
    json.number(value);
}
inline void json_write(JSONWriter& json, double value) {
    json.number(value);
}
inline void json_write(JSONWriter& json, const char* value) {
    if (value == nullptr) {
        json.json_null();
    } else {
        json.string(value);
    }
}
inline void json_write(JSONWriter& json, const std::string& value) {
    json.string(value);
}

/****************************************************************************/

} // namespace core
} // namespace oes

#endif
