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
#ifndef OES_COMMON_AVROUTILS_HPP
#define OES_COMMON_AVROUTILS_HPP

#include "AvroUtils.h"
#include "AvroBinary.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <avro/Specific.hh>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <vector>
#include <memory>
#include <optional>

//============================================================================

namespace avro {

template <size_t N> struct codec_traits<boost::array<uint8_t, N> > {
    static void encode(Encoder& e, const boost::array<uint8_t, N>& b) {
        e.encodeFixed(&b[0], N);
    }
    static void decode(Decoder& d, boost::array<uint8_t, N>& s) {
        std::vector<uint8_t> v(N);
        d.decodeFixed(N, v);
        std::copy(&v[0], &v[0] + N, &s[0]);
    }
};

} // avro

//============================================================================

namespace oes::common {

using my_buffer_ptr = std::shared_ptr<std::vector<uint8_t>>;

//============================================================================

template <typename T>
my_buffer_ptr avro_encode(const T& msg, const ::avro::EncoderPtr& enc) {
    // Warning: deprecated std::auto_ptr-to-std::unique_ptr conversion removed in C++17
    std::unique_ptr<::avro::OutputStream> out = ::avro::memoryOutputStream();
    enc->init(*out);
    msg.encode(*enc);
    enc->flush(); // calls ::avro::OutputStream::backup(), for minimum message
    return ::avro::snapshot(*out);
}

template <typename T>
void avro_decode(T& msg, const my_buffer_ptr& buf, const ::avro::DecoderPtr& dec) {
    MemoryInputStream2 in(buf->data(), buf->size());
    dec->init(in);
    msg.decode(*dec);
}

template <typename T>
my_buffer_ptr encode_binary(const T& msg) {
    return avro_encode(msg, ::avro::binaryEncoder());
}

template <typename T>
void decode_binary(T& msg, const my_buffer_ptr& buf) {
    return avro_decode(msg, buf, ::avro::binaryDecoder());
}

template <typename T>
my_buffer_ptr encode_json(const ::avro::ValidSchema& schema, const T& msg, bool pretty_print) {
    return avro_encode(
        msg, pretty_print? ::avro::jsonPrettyEncoder(schema): ::avro::jsonEncoder(schema)
    );
}

template <typename T>
void decode_json(const ::avro::ValidSchema& schema, T& msg, const my_buffer_ptr& buf) {
    return avro_decode(msg, buf, ::avro::jsonDecoder(schema));
}

template <typename T>
my_buffer_ptr encode_json(const T& msg, bool pretty_print) {
    const ::avro::ValidSchema& schema(T::schema());
    return avro_encode(
        msg, pretty_print? ::avro::jsonPrettyEncoder(schema): ::avro::jsonEncoder(schema)
    );
}

template <typename T>
void decode_json(T& msg, const my_buffer_ptr& buf) {
    const ::avro::ValidSchema& schema(T::schema());
    return avro_decode(msg, buf, ::avro::jsonDecoder(schema));
}

template <typename T>
std::string to_json(const T& msg, bool pretty_print = true) {
    my_buffer_ptr buf = encode_json(msg, pretty_print);
    return std::string(buf->begin(), buf->end());
}

template <typename T>
void from_json(T& msg, std::string& json_str) {
    StringAsAvroIStreamView in(json_str);
    const ::avro::ValidSchema& schema(T::schema());
    const ::avro::DecoderPtr& dec = ::avro::jsonDecoder(schema);
    dec->init(in);
    msg.decode(*dec);
}

//============================================================================

inline
int64_t encodedBytesOf(::avro::Encoder& e) {
    e.flush();
    return e.byteCount();
}

struct MultiPartCounts {
    // Where the count was written, and how many bytes it encoded to
    int64_t size_offset_ = 0;
    int64_t item_offset_ = 0;
    size_t count_bytes_ = 0;

    const size_t total_;    //!< Total elements in the collection
    size_t index_ = 0;      //!< Element index of the subset to encode
    size_t written_ = 0;    //!< Element count we (optimistially) encoded, typically remaining()
    size_t actual_ = 0;     //!< How many elements we actually encoded, w/o being oversize

    explicit MultiPartCounts(size_t size) : total_(size) {}

    bool done() const {
        return remaining() == 0;
    }

    size_t remaining() const {
        assert((index_ + written_) <= total_);
        return total_ - (index_ + actual_);
    }

    void nextMessage() {
        assert(!done());

        index_ += actual_;
        assert(index_ <= total_);

        size_offset_ = item_offset_ = 0;
        count_bytes_ = actual_ = written_ = 0;
    }

    void encodeSize(::avro::Encoder& e) {
        size_offset_ = encodedBytesOf(e);

        e.setItemCount(written_ = remaining());

        count_bytes_ = encodedBytesOf(e) - size_offset_;

        actual_ = 0;

        // more than 3 byte means count >= 1'048'576,
        // which should not be happening IMHO
        assert(1 <= count_bytes_ && count_bytes_ <= 3);
    }

    template <typename V>
    void encodeItem(::avro::Encoder& e, V&& v) {
        item_offset_ = encodedBytesOf(e);

        e.startItem(); // writes 0 byte (i.e. nothing) in binary encoder

        avro::encode(e, std::forward<V>(v));

        // encode above might throw, so counting fully written items only
        ++actual_;
    }

    void updateBody(std::string& msg) const {
        if (written_ == actual_) {
            // enc.arrayEnd() or enc.mapEnd() to "close" the collection,
            // even in this case, since encode_binary_multipart_col() never writes it.
            msg.push_back(0);
            return;
        }
        assert(actual_ < written_ && written_ <= total_);

        // First, truncate the message, to get rid of partially written "last" item
        assert(int64_t(msg.size()) >= item_offset_);
        msg.resize(item_offset_ + 1); // + 1 for 0 byte below
        msg.back() = 0; // i.e. enc.arrayEnd() or enc.mapEnd() to "close" the collection

        // Second, since we wrote fewer items, need to overwrite the count
        oes::common::AvroBinaryEncoder bin_enc;
        bin_enc.encodeLong(actual_);
        std::string new_encoded_count;
        bin_enc.encodingAsString(new_encoded_count);

        // Note: no need for any shifting of the body, if the two
        // sizes encoded to the same number of bytes, i.e. both are
        // on the same side of the thresholds 64, 8'192, 1'048'576, etc...
        msg.replace(size_offset_, count_bytes_, new_encoded_count);
    }
};

//============================================================================

// final and fully inlined, so hopefully completely optimized out
class OES_COMMON_EXPORT PassThruEncoder final : public ::avro::Encoder {
private:
    ::avro::EncoderPtr encoder_;

public:
    const bool binary_;

    std::optional<MultiPartCounts> split_counts_;
    std::function<bool(PassThruEncoder&)> split_encoder_;

    bool wasSplit() const {
        const bool has_counts = split_counts_.has_value();
        const bool was_split = has_counts && (
            split_counts_->actual_ != split_counts_->total_
        );
        assert(
            ( was_split &&  split_encoder_) ||
            (!was_split && !split_encoder_)
        );
        return was_split;
    }

public:
    PassThruEncoder(
        ::avro::EncoderPtr encoder,
        bool binary
    ) : encoder_(std::move(encoder))
      , binary_(binary)
    {
    }

    ~PassThruEncoder() override = default;

    PassThruEncoder(const PassThruEncoder&) = delete;
    PassThruEncoder& operator=(const PassThruEncoder&) = delete;

    PassThruEncoder(PassThruEncoder&&) = delete;
    PassThruEncoder& operator=(PassThruEncoder&&) = delete;

    int64_t encodedBytes() const {
        return encodedBytesOf(*encoder_);
    }

public: // Encoder interface
    void init(::avro::OutputStream& os) override {
        encoder_->init(os);
    }

    void flush() override {
        encoder_->flush();
    }

    int64_t byteCount() const override {
        return encoder_->byteCount();
    }

    void encodeNull() override {
        encoder_->flush();
    }

    void encodeBool(bool b) override {
        encoder_->encodeBool(b);
    }

    void encodeInt(int32_t i) override {
        encoder_->encodeInt(i);
    }

    void encodeLong(int64_t l) override {
        encoder_->encodeLong(l);
    }

    void encodeFloat(float f) override {
        encoder_->encodeFloat(f);
    }

    void encodeDouble(double d) override {
        encoder_->encodeDouble(d);
    }

    void encodeString(const std::string& s) override {
        encoder_->encodeString(s);
    }

    void encodeBytes(const uint8_t* bytes, size_t len) override {
        encoder_->encodeBytes(bytes, len);
    }

    void encodeFixed(const uint8_t* bytes, size_t len) override {
        encoder_->encodeFixed(bytes, len);
    }

    void encodeEnum(size_t e) override {
        encoder_->encodeEnum(e);
    }

    void arrayStart() override {
        encoder_->arrayStart();
    }

    void arrayEnd() override {
        encoder_->arrayEnd();
    }

    void mapStart() override {
        encoder_->mapStart();
    }

    void mapEnd() override {
        encoder_->mapEnd();
    }

    void setItemCount(size_t count) override {
        encoder_->setItemCount(count);
    }

    void startItem() override {
        encoder_->startItem();
    }

    void encodeUnionIndex(size_t e) override {
        encoder_->encodeUnionIndex(e);
    }
};

//============================================================================

OES_COMMON_EXPORT
void encodeBinaryIntVector(::avro::Encoder& enc, const std::vector<int>& vec);

OES_COMMON_EXPORT
void encodeBinaryLongVector(::avro::Encoder& enc, const std::vector<int64_t>& vec);

OES_COMMON_EXPORT
void encodeBinaryFloatVector(::avro::Encoder& enc, const std::vector<float>& vec);

OES_COMMON_EXPORT
void encodeBinaryDoubleVector(::avro::Encoder& enc, const std::vector<double>& vec);

//============================================================================

} // namespace oes::common

//============================================================================

namespace avro {

template <> inline
void encode(Encoder& enc, const std::vector<int>& vec) {
    auto* my_enc = dynamic_cast<::oes::common::PassThruEncoder*>(&enc);
    if (!my_enc || !my_enc->binary_) {
        codec_traits<std::vector<int>>::encode(enc, vec);
    } else {
        ::oes::common::encodeBinaryIntVector(enc, vec);
    }
}

template <> inline
void encode(Encoder& enc, const std::vector<int64_t>& vec) {
    auto* my_enc = dynamic_cast<::oes::common::PassThruEncoder*>(&enc);
    if (!my_enc || !my_enc->binary_) {
        codec_traits<std::vector<int64_t>>::encode(enc, vec);
    } else {
        ::oes::common::encodeBinaryLongVector(enc, vec);
    }
}

template <> inline
void encode(Encoder& enc, const std::vector<float>& vec) {
    auto* my_enc = dynamic_cast<::oes::common::PassThruEncoder*>(&enc);
    if (!my_enc || !my_enc->binary_) {
        codec_traits<std::vector<float>>::encode(enc, vec);
    } else {
        ::oes::common::encodeBinaryFloatVector(enc, vec);
    }
}

template <> inline
void encode(Encoder& enc, const std::vector<double>& vec) {
    auto* my_enc = dynamic_cast<::oes::common::PassThruEncoder*>(&enc);
    if (!my_enc || !my_enc->binary_) {
        codec_traits<std::vector<double>>::encode(enc, vec);
    } else {
        ::oes::common::encodeBinaryDoubleVector(enc, vec);
    }
}

} // avro

#endif // OES_COMMON_AVROUTILS_HPP
