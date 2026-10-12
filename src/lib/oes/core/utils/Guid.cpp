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

#include <oes/core/utils/Guid.h>

#include <oes/core/utils/Assert.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/functional/hash.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <memory>
#include <random>
#include <thread>

/*! \file oes/core/utils/Guid.h
 * \brief Defines Guid class and associated free functions.
 */

/* TRANSLATOR oes::core::Guid */

namespace {

static const unsigned char zeros__[16] = {0}; //< array of 16 zero bytes.

/*!
 * \brief Converts a \em nibble into its (lowercase) hexadecimal encoding.
 *     hexadecimal letter encodes.
 * \param nibble half a byte, i.e. 4 bits, encoding a single hexadecimal letter.
 * \return the hexadecimal letter ([0-9a-f]) for \a nibble in range [0, 15].
 * \precondition 0 <= nibble < 16
 */
static char to_char(unsigned char nibble) {
    static const char hex[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
   return hex[nibble];
}

/*!
 * \brief Converts an hexadecimal letter to its decimal equivalent.
 *
 * \param c the hexadecimal letter
 * \return the \em nibble decimal in range [0, 15]
 *
 * \precondition isxdigit(c) == true
 */
static inline int to_nible(int c) {
    return
        (c <= '9')? c - '0':
        (c <= 'F')? c - 'A' + 10:
                    c - 'a' + 10;
}

/*!
 * \brief Converts a pair of hexadecimal letters into a [0, 255] byte.
 *
 * \param high the hexadecimal letter encoding the upper 4 bits.
 * \param low the hexadecimal letter encoding the lower 4 bits.
 * \return the byte in range [0, 255].
 *
 * \note this method is equivalent to using sscanf("%02x", &byte);
 * \precondition (isxdigit(low) && isxdigit(high)) == true
 */
static inline unsigned char to_ubyte(int high, int low) {
    return static_cast<unsigned char>(
        (to_nible(high) << 4) + // the high 4-bits of the byte
         to_nible(low)          // the low  4-bits of the byte
    );
}

/*!
 * \brief Converts a group of 4 hexadecimal letters into 2 bytes.
 *
 * \param c1 the 1st hexadecimal letter
 * \param c2 the 2nd hexadecimal letter
 * \param c2 the 3rd hexadecimal letter
 * \param c2 the 4th hexadecimal letter
 * \param[out] b1 the byte encoded by \a c1 and \a c2
 * \param[out] b2 the byte encoded by \a c3 and \a c4
 * \return true on success (valid hexadecimal letters); false otherwise.
 *
 * \note Equivalent to b1 = to_ubyte(c1, c2); b2 = to_ubyte(c3, c4);
 */
static inline bool init_group_no_dash(
    int c1, int c2, int c3, int c4, unsigned char& b1, unsigned char& b2
) {
    if (!isxdigit(c1) || !isxdigit(c2) || !isxdigit(c3) || !isxdigit(c4)) {
        return false;
    }
    b1 = to_ubyte(c1, c2);
    b2 = to_ubyte(c3, c4);
    return true;
}


/*!
 * \brief Scans an optionally dash-separated hexadecimal
 *     string into a 16-bytes unique id.
 *
 * dashes [0]  [1]  [2]  [3]
 * 00000000 0000 0000 0000 000000000000 DASH-less format
 * 00000000-0000-0000-0000-000000000000 XML UID format
 *
 * \param s0 pointer to the first character of the string to scan.
 * \param end pointer to the one-past-last character of the string to scan.
 * \param[out] r pointer to first byte of the 16-bytes uid to initialize.
 * \param[out] dashes array of 4 bool indicating which of the 4 possible
 *     dashes have been seen.
 * \return number of dashes seen if the conversion was successful; -1 otherwise.
 */
static inline int init_with_dashes(
    const char*const s0, const char*const end,
    unsigned char r[16], bool dashes[4]
) {
    // no dashes allowed before or in-between first two groups,
    // so we can use fixed-offsets into constant s0 pointer.
    if (!init_group_no_dash(s0[0], s0[1], s0[2], s0[3], r[0], r[1]) ||
        !init_group_no_dash(s0[4], s0[5], s0[6], s0[7], r[2], r[3])
    ) {
        return -1;
    }

    // these 4 statements are correlated, so cannot be reordered.
    // but after them, all pointers are constants with fixed offsets,
    // so optimizer should have opportunities to reorder/parallelize.
    const char*const s1 = (s0[ 8] == '-')? s0 + 1: s0;
    const char*const s2 = (s1[12] == '-')? s1 + 1: s1;
    const char*const s3 = (s2[16] == '-')? s2 + 1: s2;
    const char*const s4 = (s3[20] == '-')? s3 + 1: s3;

    // at this point, we have seen all possible/accepted dashes. So we can
    // check the string length against its expected length for a valid uid.
    if (s4 + 20 + 12 != end) {
        return -1;
    }

    // notice each call uses its own sX (possibly offset) pointer (s1, s2, and s3)
    if (!init_group_no_dash(s1[ 8], s1[ 9], s1[10], s1[11], r[ 4], r[ 5]) ||
        !init_group_no_dash(s2[12], s2[13], s2[14], s2[15], r[ 6], r[ 7]) ||
        !init_group_no_dash(s3[16], s3[17], s3[18], s3[19], r[ 8], r[ 9])
    ) {
        return -1;
    }

    // no more dashes allowed from this point on,
    // so we can use fixed offsets into constant (but possibly offset) s4 pointer.
    if (!init_group_no_dash(s4[20], s4[21], s4[22], s4[23], r[10], r[11]) ||
        !init_group_no_dash(s4[24], s4[25], s4[26], s4[27], r[12], r[13]) ||
        !init_group_no_dash(s4[28], s4[29], s4[30], s4[31], r[14], r[15])
    ) {
        return -1;
    }

    // report which dashes we have seen.
    dashes[0] = (s1 != s0);
    dashes[1] = (s2 != s1);
    dashes[2] = (s3 != s2);
    dashes[3] = (s4 != s3);

    // and how many dashes we have seen.
    return static_cast<int>(s4 - s0);
}

/*!
 * \brief Scans an hexadecimal string into a 16-bytes unique id.
 *
 * \param s pointer to the 32-hex char string to scan.
 * \param[out] r pointer to first byte of the 16-bytes uid to initialize.
 * \return true if the conversion was successful; false otherwise.
 */
static inline bool init_no_dash(
    const char*const s, unsigned char r[16]
) {
    return
        init_group_no_dash(s[ 0], s[ 1], s[ 2], s[ 3], r[ 0], r[ 1]) &&
        init_group_no_dash(s[ 4], s[ 5], s[ 6], s[ 7], r[ 2], r[ 3]) &&
        init_group_no_dash(s[ 8], s[ 9], s[10], s[11], r[ 4], r[ 5]) &&
        init_group_no_dash(s[12], s[13], s[14], s[15], r[ 6], r[ 7]) &&
        init_group_no_dash(s[16], s[17], s[18], s[19], r[ 8], r[ 9]) &&
        init_group_no_dash(s[20], s[21], s[22], s[23], r[10], r[11]) &&
        init_group_no_dash(s[24], s[25], s[26], s[27], r[12], r[13]) &&
        init_group_no_dash(s[28], s[29], s[30], s[31], r[14], r[15]);
}

/*!
 * \brief Parses an hexadecimal string into a 16-bytes array.
 *
 * \param s pointer to the 32-hex char (or 36 with dashes) string to scan.
 * \param len length of \a s string.
 * \param[out] r pointer to first byte of the 16-bytes uid to initialize.
 * \return true if the parsing was successful; false otherwise.
 */
static inline bool parse_guid(
    const char*const s, size_t len, bool lax, unsigned char r[16]
) {
    int expected_dashes = 0;
    switch (len) {
    case 36: ++expected_dashes; [[fallthrough]];
    case 35: ++expected_dashes; [[fallthrough]];
    case 34: ++expected_dashes; [[fallthrough]];
    case 33: ++expected_dashes;
        if (!lax && expected_dashes != 4) {
            return false;
        } else {
            bool dashes[4];
            return (expected_dashes == init_with_dashes(s, s + len, r, dashes));
        }
        [[fallthrough]];
    case 32:
        return init_no_dash(s, r); // try dash-less format
    case  0:
        std::memset(r, 0, 16);
        return true;
    }

    return false;
}

/*!
 * \brief Gets a random generator.
 * \return the random generator.
 */
static std::mt19937* randomGenerator() {
    static thread_local std::unique_ptr<std::mt19937> tls;
    std::mt19937* sran = tls.get();
    if (sran == nullptr) {
        sran = new std::mt19937;
        // Use random_device result as seed
        std::random_device rd;
        sran->seed(rd());
        tls.reset(sran);
    }
    return sran;
}

} // namespace

namespace oes {
namespace core {

/*! \class Guid oes/core/utils/Guid.h
 * \brief %Value class for a Globally Unique Identifier.
 * \ingroup oes_core_utils
 *
 * A Globally Unique Identifier (Guid for short) is a randomly generated
 * 128-bit integer.
 *
 * This class stores this \em integer using a compact representation (16 bytes)
 * and allows conversions to (toStringNoDash() and toXmlString()) and from
 * (parse()) hexadecimal strings typically used display these guids as text.
 *
 * A zero-initialized Guid is considered \em invalid. A default-constructed
 * Guid is always invalid, as is Guid::invalid() as well. Test a Guid's
 * validity using isValid().
 *
 * Use the make_guid() factory method to random-initialize a Guid.
 * The Guid generator is guaranteed to provide a good random distribution of
 * Guids, with virtually no chance of collisions.
 *
 * Guid is a simple \em value class with trivial copy-construction,
 * assignment, and destruction; and is suitable as a key in ordered or
 * hashed STL-compliant associative containers.
 *
 * \sa http://en.wikipedia.org/wiki/Globally_unique_identifier
 */

/*!
 * \typedef unsigned char Guid::value_type
 * \brief STL-Compliant typedef for the \em element type of a Guid \em container.
 */

/*!
 * \typedef unsigned char& Guid::reference
 * \brief STL-Compliant typedef for a reference to an \em element of a Guid
 *     \em container.
 */

/*!
 * \typedef unsigned char const& Guid::const_reference
 * \brief STL-Compliant typedef for a reference to a const \em element of a
 *     Guid \em container.
 */

/*!
 * \typedef unsigned char* Guid::iterator
 * \brief STL-Compliant typedef for the iterator type of a Guid \em container.
 */

/*!
 * \typedef unsigned char const* Guid::const_iterator
 * \brief STL-Compliant typedef for the const_iterator type of a Guid \em container.
 */

/*!
 * \typedef size_t Guid::size_type
 * \brief STL-Compliant typedef for the element type of a Guid \em container.
 * \brief STL-Compliant typedef for the size-type a Guid \em container.
 */

/*!
 * \brief Creates a new \em invalid Guid.
 *
 * \note This constructor yields an invalid Guid, equivalent to Guid::invalid(),
 *     but it is often more expressive, and therefore recommended, to prefer
 *     using Guid::invalid() explicitly when passed as a argument.
 *
 * \sa isValid()
 * \sa invalid()
 */
Guid::Guid() {
    reset();
}

/*!
 * \brief Generates and returns a new unique identifier.
 * \return random Guid.
 *
 * \sa make_deterministic(const std::string&);
 */
Guid Guid::make() {
    /*
       Commented out usage of default constructor of random_generator
       and go through another constructor for not relying on
       memory left uninitialized intentionally. This was causing false
       positives in memory checking tool such as valgrind.
       See http://www.boost.org/doc/libs/1_46_1/libs/uuid/uuid.html
       for additional information.
    */

    static thread_local std::unique_ptr<boost::uuids::basic_random_generator<std::mt19937>> tls;
    boost::uuids::basic_random_generator<std::mt19937>* gen = tls.get();
    if (gen == nullptr) {
        gen = new boost::uuids::basic_random_generator<std::mt19937> (randomGenerator());
        tls.reset (gen);
    }

    boost::uuids::uuid uid = (*gen)();
    return Guid::from(uid);
}

/*!
 * \brief Creates a new Guid by parsing the input \a uid string.
 *
 * The string has to be in one of the following format:
 * - 00000000000000000000000000000000 (no dash)
 * - 00000000-0000-0000-0000-000000000000 (xml convention)
 * (where 0 represents an hexadecimal digit, [0-1a-fA-F]).
 *
 * \param uid the hexadecimal string to parse.
 *
 * \note Empty strings are considered valid and silently yield an invalid Guid.
 *
 * \warning If the \a uid string cannot be parsed, this triggers the OES_ASSERT
 *     which can throw an exception or abort() in debug builds, and yields an
 *     invalid Guid in release ones. To silently accept unparse-able strings,
 *     prefer the Guid::parse() factory method.
 *
 * \sa parse()
 * \sa isValid()
 */
Guid::Guid(const String& uid) {
    const bool parse_ok = parse_guid(uid.data(), uid.length(), false, guid_);
    OES_ASSERT(parse_ok, reset(););
}

/*!
 * \brief Creates a new Guid by parsing the input \a uid string.
 *
 * The string has to be in one of the following format:
 * - 00000000000000000000000000000000 (no dash)
 * - 00000000-0000-0000-0000-000000000000 (xml convention)
 * (where 0 represents an hexadecimal digit, [0-1a-fA-F]).
 *
 * \param uid the hexadecimal string to parse.
 *
 * \note If the \a uid string cannot be parsed, this method just silently
 *     yields an invalid Guid.
 *
 * \sa isValid()
 * \sa Guid::Guid(const String&)
 */
Guid Guid::parse(const String& uid) {
    Guid guid;
    if (!parse_guid(uid.data(), uid.length(), false, guid.guid_)) {
        guid.reset();
    }
    return guid;
}

/*!
 * \brief Assigns this Guid by parsing the input C-style \a str.
 *
 * The string has to be in one of the following format (if \a lax is false):
 * - 00000000000000000000000000000000 (no dash)
 * - 00000000-0000-0000-0000-000000000000 (xml convention)
 * (where 0 represents an hexadecimal digit, [0-1a-fA-F]).
 *
 * When \a lax is true, any dash from the xml convention can be omitted,
 * provided they remain in their respective relative position. This notably
 * allows parsing the 3-dashes format:
 * - 00000000-00000000-0000-000000000000
 *
 * \param str the Guid string to parse.
 * \param len the length of the hexadecimal (and optional dashes) Guid string.
 * \param lax whether to allow optional dashes or not.
 * \return true on a successful parse; false otherwise, leaving this Guid in a
 *     partially parsed state. Caller should reset() this Guid if it matters.
 *
 * \note Guid::parse() does not allow to distinguish an invalid-format guid,
 *     from a valid-format \em invalid Guid (i.e. the \em null zero-only Guid).
 *     While this method does.
 *
 * \note This method avoids creating a throw-away std::string, just to parse it,
 *     unlike Guid::parse(). Especially since Guids exceed std::string's SBO's
 *     Small Buffer Optimization threshold, so always results in heap allocation.
 *
 * \warning On unsuccessful parses, this Guid may have been partially updated.
 *     Caller should take care to explicitly reset() this Guid, if it matters.
 *
 * \sa isValid()
 * \sa Guid::parse(const String&)
 */
bool Guid::reparse(const char* str, size_t len, bool lax) {
    return parse_guid(str, len, lax, guid_);
}

/*!
 * \brief Creates a copy of another \em foreign Guid.
 *
 * This constructor allows to efficiently convert another byte-array-based
 * Guid-like type into an instance of this Guid type, without going through
 * an intermediate string representation.
 *
 * Both \a begin and \a end should \em random const iterators to bytes
 * (unsigned char), and the \a distance between them must be exactly 16.
 *
 * \note this method is typically not used directly, but via the Guid::from()
 *     template helper.
 *
 * \param begin start of byte array from foreign guid type.
 * \param end end of byte array started at \a end.
 */
Guid::Guid(const_iterator begin, const_iterator end) {
    OES_ASSERT(end - begin == 16, reset(); return;);
    std::copy(begin, begin + size(), guid_);
}

/*!
 * \brief Gets the static invalid Guid.
 * \return the const invalid guid.
 */
const Guid& Guid::invalid() {
    static const Guid invalid_uid(zeros__, zeros__ + 16);
    return invalid_uid;
}

/*!
 * \brief Namespace for a fully-qualified domain name.
 * \return RFC 4122 Appendic C defined known value.
 */
Guid Guid::dns_namespace() {
    static const unsigned char bytes[] = {
        0x6b, 0xa7, 0xb8, 0x10, 0x9d, 0xad, 0x11, 0xd1,
        0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
    };
    return Guid(bytes, bytes + 16);
}

/*!
 * \brief Namespace for a URL.
 * \return RFC 4122 Appendic C defined known value.
 */
Guid Guid::url_namespace() {
    static const unsigned char bytes[] = {
        0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
        0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
    };
    return Guid(bytes, bytes + 16);
}

/*!
 * \brief Namespace for an ISO OID.
 * \return RFC 4122 Appendic C defined known value.
 */
Guid Guid::oid_namespace() {
    static const unsigned char bytes[] = {
        0x6b, 0xa7, 0xb8, 0x12, 0x9d, 0xad, 0x11, 0xd1,
        0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
    };
    return Guid(bytes, bytes + 16);
}

/*!
 * \brief Namespace for an X.500 DN (in DER or a text output format).
 * \return RFC 4122 Appendic C defined known value.
 */
Guid Guid::x500_namespace() {
    static const unsigned char bytes[] = {
        0x6b, 0xa7, 0xb8, 0x14, 0x9d, 0xad, 0x11, 0xd1,
        0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
    };
    return Guid(bytes, bytes + 16);
}

/*!
 * \brief Default \em named Guid namespace specific to Open ETP Server.
 * \return Fixed (arbitrary) value defining the \em Open ETP Server namespace.
 */
Guid Guid::oes_namespace() {
    // SHA-1 key f3c92614d98d1383c5b23d4e70ea197ac5ee51b0
    static const unsigned char bytes[] = {
        0xf3, 0xc9, 0x26, 0x14, 0xd9, 0x8d, 0x13, 0x83,
        0xc5, 0xb2, 0x3d, 0x4e, 0x70, 0xea, 0x19, 0x7a
    };
    return Guid(bytes, bytes + 16);
}

/*!
 * \fn Guid Guid::make_deterministic(const std::string& arbitrary_text)
 * \brief Convenience method defaulting the namespace to oes_namespace().
 * \param arbitrary_text the text to compute the Guid from.
 * \return A \em name-based non-random Guid, based on this calls input.
 * \note the same input always yield the same Guid value.
 */

/*!
 * \brief Generates and returns a new unique identifier, deterministically,
 *     based on the given namespace and text.
 * \param namespace_guid the \em namespace (seed) for the generation.
 * \param arbitrary_text the text to compute the Guid from.
 * \return A \em name-based non-random Guid, based on this calls input.
 * \note the same input always yield the same Guid value.
 */
Guid Guid::make_deterministic(const Guid& namespace_guid, const std::string& arbitrary_text) {
    boost::uuids::uuid namespace_uuid;
    std::copy(namespace_guid.begin(), namespace_guid.end(), namespace_uuid.data);
    boost::uuids::name_generator gen(namespace_uuid);
    boost::uuids::uuid result(gen(arbitrary_text));
    return Guid::from(result);
}

/*!
 * \brief Convenience method taking the namespace as text.
 * \param namespace_text the \em namespace (seed) for the generation, which can
 *     be either a valid string-representation of a Guid, in which case it is parsed
 *     as a Guid and used as-is for the namespace, or arbitrary text, in which case
 *     that text is first converted to a Guid itself, using make_deterministic() and
 *     the default oes_namespace(), to be used as the namespace.
 * \param arbitrary_text the text to compute the Guid from.
 * \return A \em name-based non-random Guid, based on this calls input.
 * \note the same input always yield the same Guid value.
 */
Guid Guid::make_deterministic(const std::string& namespace_text, const std::string& arbitrary_text) {
    Guid ns = Guid::parse(namespace_text);
    if (!ns.isValid()) {
        ns = make_deterministic(namespace_text);
    }
    return make_deterministic(ns, arbitrary_text);
}

/*!
 * \fn bool Guid::isValid() const
 * \brief Determines whether this Guid is valid.
 * \return true if this Guid is valid; false otherwise.
 */

/*!
 * \fn Guid::iterator Guid::begin()
 * \brief Gets STL-compliant range-starting iterator to internal byte array.
 * \return the iterator to start of 16-bytes long array.
 */

/*!
 * \fn Guid::const_iterator Guid::begin() const
 * \brief Gets STL-compliant range-starting iterator to internal byte array.
 * \return the const_iterator to start of 16-bytes long array.
 */

/*!
 * \fn Guid::iterator Guid::end()
 * \brief Gets STL-compliant range-ending iterator to internal byte array.
 * \return the iterator to the end (one-past last byte) of 16-bytes long array.
 */

/*!
 * \fn Guid::const_iterator Guid::end() const
 * \brief Gets STL-compliant range-ending iterator to internal byte array.
 * \return the iterator to the end (one-past last byte) of 16-bytes long array.
 */

/*!
 * \fn Guid::size_type Guid::size() const
 * \brief Gets the length of the byte array internal to this Guid.
 * \return Always 16.
 */

/*!
 * \fn void Guid::reset()
 * \brief Resets this Guid to invalid.
 * \sa isValid()
 */

/*!
 * \brief Converts this Guid to a dash-less hexadecimal string.
 *
 * Examples:
 * - 00000000000000000000000000000000 (for invalid Guid instances)
 * - 6f2fe04a3ea7490fa7d7469329bf3045 (for a given valid Guid)
 *
 * \return a 32 character long hexadecimal string.
 */
String Guid::toStringNoDash() const {
    const char characters[] = { // 32 hex chars
        to_char(guid_[ 0] >> 4), to_char(guid_[ 0] & 0xf),
        to_char(guid_[ 1] >> 4), to_char(guid_[ 1] & 0xf),
        to_char(guid_[ 2] >> 4), to_char(guid_[ 2] & 0xf),
        to_char(guid_[ 3] >> 4), to_char(guid_[ 3] & 0xf),
        to_char(guid_[ 4] >> 4), to_char(guid_[ 4] & 0xf),
        to_char(guid_[ 5] >> 4), to_char(guid_[ 5] & 0xf),
        to_char(guid_[ 6] >> 4), to_char(guid_[ 6] & 0xf),
        to_char(guid_[ 7] >> 4), to_char(guid_[ 7] & 0xf),
        to_char(guid_[ 8] >> 4), to_char(guid_[ 8] & 0xf),
        to_char(guid_[ 9] >> 4), to_char(guid_[ 9] & 0xf),
        to_char(guid_[10] >> 4), to_char(guid_[10] & 0xf),
        to_char(guid_[11] >> 4), to_char(guid_[11] & 0xf),
        to_char(guid_[12] >> 4), to_char(guid_[12] & 0xf),
        to_char(guid_[13] >> 4), to_char(guid_[13] & 0xf),
        to_char(guid_[14] >> 4), to_char(guid_[14] & 0xf),
        to_char(guid_[15] >> 4), to_char(guid_[15] & 0xf)
    };
    String string_guid(characters, characters + 32);
    return string_guid;
}

/*!
 * \brief Converts this Guid to a dash-separated hexadecimal string.
 *
 * This particular representation is common to many XML schemas, and is used
 * as the \em standard representation for the streaming operator.
 *
 * Examples:
 * - 00000000-0000-0000-0000-000000000000 (for invalid Guid instances)
 * - 6f2fe04a-3ea7-490f-a7d7-469329bf3045 (for a given valid Guid)
 *
 * \return a 36 character long hexadecimal string.
 */
String Guid::toXmlString() const {
    static const char dash ('-');
    const char characters[] = { // 32 hex chars + 4 dash separators
        to_char(guid_[ 0] >> 4), to_char(guid_[ 0] & 0xf),
        to_char(guid_[ 1] >> 4), to_char(guid_[ 1] & 0xf),
        to_char(guid_[ 2] >> 4), to_char(guid_[ 2] & 0xf),
        to_char(guid_[ 3] >> 4), to_char(guid_[ 3] & 0xf),
        dash, // 8
        to_char(guid_[ 4] >> 4), to_char(guid_[ 4] & 0xf),
        to_char(guid_[ 5] >> 4), to_char(guid_[ 5] & 0xf),
        dash, // 13
        to_char(guid_[ 6] >> 4), to_char(guid_[ 6] & 0xf),
        to_char(guid_[ 7] >> 4), to_char(guid_[ 7] & 0xf),
        dash, // 18
        to_char(guid_[ 8] >> 4), to_char(guid_[ 8] & 0xf),
        to_char(guid_[ 9] >> 4), to_char(guid_[ 9] & 0xf),
        dash, // 23
        to_char(guid_[10] >> 4), to_char(guid_[10] & 0xf),
        to_char(guid_[11] >> 4), to_char(guid_[11] & 0xf),
        to_char(guid_[12] >> 4), to_char(guid_[12] & 0xf),
        to_char(guid_[13] >> 4), to_char(guid_[13] & 0xf),
        to_char(guid_[14] >> 4), to_char(guid_[14] & 0xf),
        to_char(guid_[15] >> 4), to_char(guid_[15] & 0xf)
    };

    std::string string_guid(characters, characters + 36);
    return string_guid;
}

/*!
 * \brief Swaps the contents of two Guids.
 * \param lhs the left-hand-side Guid to swap.
 * \param rhs the right-hand-side Guid to swap.
 */
void swap(oes::core::Guid& lhs, oes::core::Guid& rhs) {
    std::swap_ranges(lhs.begin(), lhs.end(), rhs.begin());
}

/*!
 * \brief Gets the hash value of this Guid, for hashing containers.
 * \param uid the Guid to get the hash of.
 * \return the hash-value.
 */
std::size_t hash_value(const oes::core::Guid& uid) {
    // Marginally faster, but makes hash endianness dependent.
    //return *reinterpret_cast<const std::size_t*>(uid.begin());
    return boost::hash_range(uid.begin(), uid.end());
}

/*! \fn template <typename T> Guid Guid::from(const T& guid)
 * \brief Creates a Guid as a copy of another \em foreign Guid.
 * \tparam T foreign Guid-like type with begin()/end() methods to
 *     extract the range to the 6-byte array storing the foreign Guid.
 * \param guid the foreign guid to copy.
 * \return the guid.
 */

/*! \fn Guid make_guid()
 * \brief Makes (generates) a randomly initialized Guid.
 *
 * Helper allowing to write make_guid() instead of Guid::make(),
 * for increased code readability.
 *
 * This method can also be used to fill a container with random guids:
 * \code
 * std::vector<Guid> guids(1000);
 * std::generate(guids.begin(), guids.end(), &make_guid);
 * \endcode
 *
 * \return valid Guid.
 */

/*! \fn int Guid::compare(const Guid&) const
 * \brief Compares this guid with another.
 *
 * The comparison is consistent with how std::memcmp behaves.
 *
 * \param other the other guid to compare against.
 * \return 0 if equal;
 *     negative integer if less-than other;
 *     positive integer otherwise.
 */

/*! \fn bool operator==(const Guid& lhs, const Guid& rhs)
 * \brief Tests two guids for equality.
 * \param lhs the left-hand-side operand.
 * \param rhs the right-hand-side operand.
 * \return true if \a lhs equals \a rhs; false otherwise.
 */

/*! \fn bool operator!=(const Guid& lhs, const Guid& rhs)
 * \brief Tests two guids for inequality.
 * \param lhs the left-hand-side operand.
 * \param rhs the right-hand-side operand.
 * \return true if \a lhs does \b not equal \a rhs; false otherwise.
 */

/*! \fn bool operator<(const Guid& lhs, const Guid& rhs)
 * \brief Tests whether one Guid is strictly lesser than another guid.
 *
 * Implements a strict weak ordering for Guid%s, suitable for STL-compliant
 * ordered containers.
 *
 * \param lhs the left-hand-side operand.
 * \param rhs the right-hand-side operand.
 * \return true if \a lhs is less than \a rhs, i.e. orders before it;
 *     false otherwise.
 */

/*! \fn bool operator>(const Guid& lhs, const Guid& rhs)
 * \brief Tests whether one Guid is strictly greater than another guid.
 * \param lhs the left-hand-side operand.
 * \param rhs the right-hand-side operand.
 * \return true if \a lhs is greater than \a rhs; false otherwise.
 */

/*! \fn bool operator<=(const Guid& lhs, const Guid& rhs)
 * \brief Tests whether one Guid is lesser than or equal another guid.
 * \param lhs the left-hand-side operand.
 * \param rhs the right-hand-side operand.
 * \return true if TODO; false otherwise.
 * \return true if \a lhs is lesser than or equal to \a rhs; false otherwise.
 */

/*! \fn bool operator>=(const Guid& lhs, const Guid& rhs)
 * \brief Tests whether one Guid is greater than or equal another guid.
 * \param lhs the left-hand-side operand.
 * \param rhs the right-hand-side operand.
 * \return true if TODO; false otherwise.
 * \return true if \a lhs is greater than or equal to \a rhs; false otherwise.
 */

/*! \fn std::ostream& operator<<(std::ostream& os, const Guid& uid)
 * \brief Outputs a Guid to a stream, using the dash-separated format.
 * \param[in, out] os the stream to output to.
 * \param uid the Guid to output.
 * \return the input \a os, for call chaining.
 * \sa Guid::toXmlString()
 */

} // namespace core
} // namespace oes
