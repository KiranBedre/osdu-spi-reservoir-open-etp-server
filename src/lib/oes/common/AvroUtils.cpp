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

#include "AvroUtils.h"
#include "AvroUtils.hpp"

#include <oes/core/utils/Guid.h>

/*!
 * \file oes/eml/AvroUtils.h
 * \brief Utilities to work with the Apache Avro framework.
 */

namespace oes::common {

//============================================================================

void assignUid(std::array<uint8_t, 16>& lhs, const oes::core::Guid& rhs) {
    std::memcpy(lhs.data(), rhs.begin(), oes::core::Guid::static_size());
}

void assignUid(oes::core::Guid& lhs, const std::array<uint8_t, 16>& rhs) {
    const uint8_t* beg = &rhs[0];
    oes::core::Guid uid(beg, beg + 16);
    lhs = uid;
}

//============================================================================

void assignUid(boost::array<uint8_t, 16>& lhs, const oes::core::Guid& rhs) {
    std::memcpy(lhs.c_array(), rhs.begin(), oes::core::Guid::static_size());
}

// Guid::from() should be preferred IMHO, but for symmetry with above I guess
void assignUid(oes::core::Guid& lhs, const boost::array<uint8_t, 16>& rhs) {
    lhs = oes::core::Guid::from(rhs);
}

//============================================================================

// Part of official Avro 1.8.2 source distrib,
// but somehow absent from our Avro 1.8.2 in toolkits.

MemoryInputStream2::MemoryInputStream2(
    const uint8_t *data, size_t len
) : data_(data)
  , size_(len)
  , curLen_(0)
{
}

void MemoryInputStream2::init(const uint8_t* data, size_t len) {
    data_ = data;
    size_ = len;
    curLen_ = 0;
}

bool MemoryInputStream2::next(const uint8_t** data, size_t* len) {
    if (curLen_ == size_) {
        return false;
    }
    *data = &data_[curLen_];
    *len = size_ - curLen_;
    curLen_ = size_;
    return true;
}

void MemoryInputStream2::backup(size_t len) {
    curLen_ -= len;
}

void MemoryInputStream2::skip(size_t len) {
    if (len > (size_ - curLen_)) {
        len = size_ - curLen_;
    }
    curLen_ += len;
}

size_t MemoryInputStream2::byteCount() const {
    return curLen_;
}

//============================================================================

StringAsAvroIStreamView::StringAsAvroIStreamView(
    std::string& s // non-const to avoid temporaries
) : MemoryInputStream2(
        reinterpret_cast<const uint8_t*>(s.data()),
        s.size()
    )
{
}

void StringAsAvroIStreamView::init(std::string& s) {
    MemoryInputStream2::init(
        reinterpret_cast<const uint8_t*>(s.data()),
        s.size()
    );
}

void StringAsAvroIStreamView::reset() {
    MemoryInputStream2::init(nullptr, 0);
}

//============================================================================

StringAsAvroOStreamView::StringAsAvroOStreamView(
    std::string& buf, size_t chunkSize, size_t maxSize
) : buf_(buf)
  , chunkSize_(chunkSize)
  , maxSize_(maxSize)
{
    assert(chunkSize <= maxSize);

    buf.clear(); // start empty
}

bool StringAsAvroOStreamView::next(uint8_t** data, size_t* len) {
    assert(data && len);
    const size_t s = buf_.size();
    /*
    ** This breaks test_import_large_epc_max_msg_size with low 10K max-msg-size.
    ** Apparently the client sending more bytes than allowed does not "break"
    ** the WebSocketPP connection, like it does on the server-side.
    **
    ** Note that we used to test (s > maxSize) only, because we cannot
    ** know for sure yet whether the whole chunk will be used or not. This allowed
    ** to fail only when we were 100% sure the serialized message will be oversized.
    **
    ** But now that we encode_binary_multipart() (in Protocols.cpp), it's better to
    ** fail earlier, instead of risking a split message still being oversize.
    */
    if (s + chunkSize_ > maxSize_) {
        return false; // abort ASAP message too-big. Encoder throws AvroException("EOF reached")
    }
    buf_.resize(s + chunkSize_);
    char* chunk = &buf_[s];
    *data = reinterpret_cast<uint8_t*>(chunk);
    *len = chunkSize_;
    return true;
}

void StringAsAvroOStreamView::backup(size_t len) {
    const size_t s = buf_.size();
    if (len < s) {
        buf_.resize(s - len);
    } else {
        buf_.clear();
    }

    /*
    ** Does not seem to help, and in any case, WebSocketPP will
    ** copy the message to "mask and compress" it, based on size
    ** and not capacity, so little point in doing that.
    **
    ** Worse, now we flush() the encoder more often (which calls backup()),
    ** to know the intermediate offsets within messages, so shrink_to_fit()
    ** would force re-allocs later, and make things slower.
    **
    if (buf_.capacity() > 2*buf_.size()) {
        buf_.shrink_to_fit();
    }
    */
}

uint64_t StringAsAvroOStreamView::byteCount() const {
    return buf_.size();
}

void StringAsAvroOStreamView::flush() {
    // nothing to do
}

//============================================================================

static inline uint8_t* encodeVarint(uint8_t* out, uint32_t val) {
    *out = val & 0x7F;
    while (val >>= 7) {
        *out |= 0x80; ++out;
        *out = (val & 0x7F);
    }
    return ++out;
}

void encodeBinaryIntVector(::avro::Encoder& enc, const std::vector<int>& vec) {
    enc.arrayStart(); // see encodeBinaryFloatVector() below for comments

    if (!vec.empty()) {
        const int* v = vec.data();
        const size_t n = vec.size();

        enc.setItemCount(n);

        const size_t bits   = 3;
        const size_t batch  = 1 << bits;
        const size_t chunks = n >> bits;
        for (size_t i = 0; i < chunks; ++i, v += batch) {
            const uint32_t zigzag[batch] {
                uint32_t(v[0] << 1) ^ (v[0] >> 31),
                uint32_t(v[1] << 1) ^ (v[1] >> 31),
                uint32_t(v[2] << 1) ^ (v[2] >> 31),
                uint32_t(v[3] << 1) ^ (v[3] >> 31),
                uint32_t(v[4] << 1) ^ (v[4] >> 31),
                uint32_t(v[5] << 1) ^ (v[5] >> 31),
                uint32_t(v[6] << 1) ^ (v[6] >> 31),
                uint32_t(v[7] << 1) ^ (v[7] >> 31),
            };

            uint8_t varints[batch * 5];
            uint8_t* out = varints;
            out = encodeVarint(out, zigzag[0]);
            out = encodeVarint(out, zigzag[1]);
            out = encodeVarint(out, zigzag[2]);
            out = encodeVarint(out, zigzag[3]);
            out = encodeVarint(out, zigzag[4]);
            out = encodeVarint(out, zigzag[5]);
            out = encodeVarint(out, zigzag[6]);
            out = encodeVarint(out, zigzag[7]);

            enc.encodeFixed(varints, (out - varints));
        }

        if (const size_t tail = (n & (batch - 1))) {
            uint8_t varints[batch * 5];
            uint8_t* out = varints;

            for (size_t i = 0; i < tail; ++i) {
                out = encodeVarint(out, (v[i] << 1) ^ (v[i] >> 31));
            }

            enc.encodeFixed(varints, (out - varints));
        }
    }

    enc.arrayEnd();
}

//============================================================================

static inline uint8_t* encodeVarint64(uint8_t* out, uint64_t val) {
    *out = val & 0x7F;
    while (val >>= 7) {
        *out |= 0x80; ++out;
        *out = (val & 0x7F);
    }
    return ++out;
}

void encodeBinaryLongVector(::avro::Encoder& enc, const std::vector<int64_t>& vec) {
    enc.arrayStart(); // see encodeBinaryFloatVector() below for comments

    if (!vec.empty()) {
        const int64_t* v = vec.data();
        const size_t n = vec.size();

        enc.setItemCount(n);

        const size_t bits   = 3;
        const size_t batch  = 1 << bits;
        const size_t chunks = n >> bits;
        for (size_t i = 0; i < chunks; ++i, v += batch) {
            const uint64_t zigzag[batch] {
                uint64_t(v[0] << 1) ^ (v[0] >> 63),
                uint64_t(v[1] << 1) ^ (v[1] >> 63),
                uint64_t(v[2] << 1) ^ (v[2] >> 63),
                uint64_t(v[3] << 1) ^ (v[3] >> 63),
                uint64_t(v[4] << 1) ^ (v[4] >> 63),
                uint64_t(v[5] << 1) ^ (v[5] >> 63),
                uint64_t(v[6] << 1) ^ (v[6] >> 63),
                uint64_t(v[7] << 1) ^ (v[7] >> 63),
            };

            uint8_t varints[batch * 10];
            uint8_t* out = varints;
            out = encodeVarint64(out, zigzag[0]);
            out = encodeVarint64(out, zigzag[1]);
            out = encodeVarint64(out, zigzag[2]);
            out = encodeVarint64(out, zigzag[3]);
            out = encodeVarint64(out, zigzag[4]);
            out = encodeVarint64(out, zigzag[5]);
            out = encodeVarint64(out, zigzag[6]);
            out = encodeVarint64(out, zigzag[7]);

            enc.encodeFixed(varints, (out - varints));
        }

        if (const size_t tail = (n & (batch - 1))) {
            uint8_t varints[batch * 10];
            uint8_t* out = varints;

            for (size_t i = 0; i < tail; ++i) {
                out = encodeVarint64(out, (v[i] << 1) ^ (v[i] >> 63));
            }

            enc.encodeFixed(varints, (out - varints));
        }
    }

    enc.arrayEnd();
}

//============================================================================

void encodeBinaryFloatVector(::avro::Encoder& enc, const std::vector<float>& vec) {
    /*
    ** Original impl from Specific.hh, for reference
    **
template <typename T> struct codec_traits<std::vector<T> > {
    static void encode(Encoder& e, const std::vector<T>& b) {
        e.arrayStart();
        if (! b.empty()) {
            e.setItemCount(b.size());
            for (typename std::vector<T>::const_iterator it = b.begin();
                it != b.end(); ++it) {
                e.startItem();
                avro::encode(e, *it);
            }
        }
        e.arrayEnd();
    }
    */

    // Empty in BinaryEncoder. Lets keep it anyway, for symmetry
    enc.arrayStart();

    if (!vec.empty()) {
        enc.setItemCount(vec.size()); // zigzag + varint encoded

        // Here is the important change. We directly copy the raw data, since
        // startItem() is empty in BinaryEncoder, and encodeFloat() just blindly
        // copies 4 bytes at a time. We do not directly have access to the
        // internal out_.writeBytes() API, but that is exactly what encodeFixed()
        // does, so we abuse it a little, for the greater good!
        const uint8_t* p = reinterpret_cast<const uint8_t*>(vec.data());
        enc.encodeFixed(p, vec.size() * sizeof(float));
    }

    enc.arrayEnd();
}

void encodeBinaryDoubleVector(::avro::Encoder& enc, const std::vector<double>& vec) {
    enc.arrayStart(); // see encodeBinaryFloatVector() above for comments

    if (!vec.empty()) {
        enc.setItemCount(vec.size());

        const uint8_t* p = reinterpret_cast<const uint8_t*>(vec.data());
        enc.encodeFixed(p, vec.size() * sizeof(double));
    }

    enc.arrayEnd();
}

//============================================================================

} // namespace oes::oes
