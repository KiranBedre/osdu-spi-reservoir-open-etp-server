// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include "AvroBinary.h"

#include <cstring>
#include <stdexcept>

/*!
 * \file oes/eml/AvroBinary.h
 * \brief Custom implementation of Avro binary serialization/deserialization.
 */

namespace oes::common {

//============================================================================

static inline uint8_t* encodeVarint(uint8_t* out, uint32_t val) {
    *out = val & 0x7F;
    while (val >>= 7) {
        *out |= 0x80; ++out;
        *out = (val & 0x7F);
    }
    return ++out;
}

static inline uint8_t* encodeVarint64(uint8_t* out, uint64_t val) {
    *out = val & 0x7F;
    while (val >>= 7) {
        *out |= 0x80; ++out;
        *out = (val & 0x7F);
    }
    return ++out;
}

//============================================================================

AvroBinaryEncoder::AvroBinaryEncoder(
    size_t capacity
) : buf_()
{
    buf_.reserve(capacity);
}

void AvroBinaryEncoder::clear() {
    buf_.clear();
}

size_t AvroBinaryEncoder::capacity() const {
    return buf_.capacity();
}

void AvroBinaryEncoder::reserve(size_t capacity) {
    buf_.reserve(capacity);
}

//! Gets copy of the encoding as a string
void AvroBinaryEncoder::encodingAsString(std::string& encoded) const {
    encoded.assign(buf_.begin(), buf_.end());
}

//! Gets copy of the encoding as a byte vector
void AvroBinaryEncoder::encodingAsVector(std::vector<uint8_t>& encoded) const {
    encoded.assign(buf_.begin(), buf_.end());
}

//! Move internal encoding byte vector to caller, and \a clear() this encoder
void AvroBinaryEncoder::moveEncodingTo(std::vector<uint8_t>& encoded) {
    buf_.swap(encoded);
    buf_.clear();
}

void AvroBinaryEncoder::init(::avro::OutputStream& os) {
    p_os_ = &os;
}

void AvroBinaryEncoder::flush() {
    // Copy our internal buffer into the output stream
    auto* cur = buf_.data();
    size_t remaining = buf_.size();
    while (remaining) {
        uint8_t* data;
        size_t len;
        if (!p_os_->next(&data, &len)) {
            assert(false);
            return; // or throw?
        }

        if (remaining > len) {
            std::memcpy(data, cur, len);
            remaining -= len;
            cur += len;
        } else {
            std::memcpy(data, cur, remaining);
            p_os_->backup(len - remaining);
            buf_.clear();
            return;
        }
    }
    assert(buf_.empty());
}

int64_t AvroBinaryEncoder::byteCount() const {
    return static_cast<int64_t>(buf_.size());
}

void AvroBinaryEncoder::encodeNull() {
    // nothing
}

void AvroBinaryEncoder::encodeBool(bool b) {
    buf_.emplace_back(b? 1: 0);
}

void AvroBinaryEncoder::encodeInt(int32_t i) {
    uint8_t in[5];
    uint32_t v((i << 1) ^ (i >> 31));
    uint8_t* out = encodeVarint(in, v);
    encodeFixed(in, out - in);
}

void AvroBinaryEncoder::encodeLong(int64_t l) {
    uint8_t in[10];
    uint64_t v((l << 1) ^ (l >> 63));
    uint8_t* out = encodeVarint64(in, v);
    encodeFixed(in, out - in);
}

void AvroBinaryEncoder::encodeFloat(float f) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&f);
    encodeFixed(p, sizeof(float));
}

void AvroBinaryEncoder::encodeDouble(double d) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&d);
    encodeFixed(p, sizeof(double));
}

void AvroBinaryEncoder::encodeString(const std::string& s) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(s.c_str());
    encodeBytes(p, s.size());
}

void AvroBinaryEncoder::encodeBytes(const uint8_t* bytes, size_t len) {
    encodeLong(len);
    encodeFixed(bytes, len);
}

void AvroBinaryEncoder::encodeFixed(const uint8_t* bytes, size_t len) {
    buf_.insert(buf_.end(), bytes, bytes + len);
}

void AvroBinaryEncoder::encodeEnum(size_t e) {
    encodeLong(e);
}

void AvroBinaryEncoder::arrayStart() {
    // nothing
}

void AvroBinaryEncoder::arrayEnd() {
    encodeLong(0);
}

void AvroBinaryEncoder::mapStart() {
    // nothing
}

void AvroBinaryEncoder::mapEnd() {
    encodeLong(0);
}

void AvroBinaryEncoder::setItemCount(size_t count) {
    if (count == 0) {
        throw std::runtime_error("Count cannot be zero");
    }
    encodeLong(count);
}

void AvroBinaryEncoder::startItem() {
    // nothing
}

void AvroBinaryEncoder::encodeUnionIndex(size_t e) {
    encodeLong(e);
}

//============================================================================

void AvroBinaryDecoder::init(::avro::InputStream& is) {
    in_.reset(is);
}

void AvroBinaryDecoder::decodeNull() {
    // nothing
}

bool AvroBinaryDecoder::decodeBool() {
    switch (in_.read()) {
    case  0: return false;
    case  1: return true;
    default: throw std::runtime_error("Invalid value for bool");
    }
}

int32_t AvroBinaryDecoder::decodeInt() {
    uint32_t encoded = 0;
    int shift = 0;
    uint8_t u;
    do {
        if (shift >= 64) {
            throw std::runtime_error("Invalid Avro varint");
        }
        u = in_.read();
        encoded |= static_cast<uint32_t>(u & 0x7f) << shift;
        shift += 7;
    } while (u & 0x80);

    return static_cast<int32_t>(((encoded >> 1) ^
         -(static_cast<int32_t>(  encoded) & 1)));
}

int64_t AvroBinaryDecoder::decodeLong() {
    uint64_t encoded = 0;
    int shift = 0;
    uint8_t u;
    do {
        if (shift >= 64) {
            throw std::runtime_error("Invalid Avro varint");
        }
        u = in_.read();
        encoded |= static_cast<uint64_t>(u & 0x7f) << shift;
        shift += 7;
    } while (u & 0x80);

    return static_cast<int64_t>(((encoded >> 1) ^
         -(static_cast<int64_t>(  encoded) & 1)));
}

static inline
void readInto(::avro::StreamReader& in, void* dest, size_t len) {
    auto bytes = static_cast<uint8_t*>(dest);
    in.readBytes(bytes, len);
}

float AvroBinaryDecoder::decodeFloat() {
    float f;
    readInto(in_, &f, sizeof(f));
    return f;
}

double AvroBinaryDecoder::decodeDouble() {
    double d;
    readInto(in_, &d, sizeof(d));
    return d;
}

void AvroBinaryDecoder::decodeString(std::string& value) {
    if (auto len = decodeLong()) {
        value.resize(len);
        readInto(in_, value.data(), value.size());
    } else {
        value.clear();
    }
}

void AvroBinaryDecoder::decodeBytes(std::vector<uint8_t>& value) {
    if (auto len = decodeLong()) {
        value.resize(len);
        in_.readBytes(value.data(), value.size());
    } else {
        value.clear();
    }
}

void AvroBinaryDecoder::decodeFixed(size_t n, std::vector<uint8_t>& value) {
    if (n) {
        value.resize(n);
        in_.readBytes(value.data(), n);
    } else {
        value.clear();
    }
}

size_t AvroBinaryDecoder::decodeEnum() {
    return decodeLong();
}

size_t AvroBinaryDecoder::arrayStart() {
    return decodeLong(); // Warning: Not "symmetrical" with Encoder::arrayStart()
}

size_t AvroBinaryDecoder::arrayNext() {
    return decodeLong(); // Warning: Not "symmetrical" with Encoder::startItem()
}

size_t AvroBinaryDecoder::mapStart() {
    return decodeLong(); // Warning: Not "symmetrical" with Encoder::mapStart()
}

size_t AvroBinaryDecoder::mapNext() {
    return decodeLong(); // Warning: Not "symmetrical" with Encoder::startItem()
}

size_t AvroBinaryDecoder::decodeUnionIndex() {
    return decodeLong();
}

// Not sure where the skip methods are used...

void AvroBinaryDecoder::skipString() {
    if (auto len = decodeLong()) {
        in_.skipBytes(len);
    }
}

void AvroBinaryDecoder::skipBytes() {
    if (auto len = decodeLong()) {
        in_.skipBytes(len);
    }
}

void AvroBinaryDecoder::skipFixed(size_t n) {
    in_.skipBytes(n);
}

size_t AvroBinaryDecoder::skipArray() {
    throw std::runtime_error("skipArray() not supported"); // TOO weird...
}

size_t AvroBinaryDecoder::skipMap() {
    throw std::runtime_error("skipMap() not supported");
}

void AvroBinaryDecoder::drain() {
    in_.drain(false);
}

//============================================================================

} // namespace oes::common
