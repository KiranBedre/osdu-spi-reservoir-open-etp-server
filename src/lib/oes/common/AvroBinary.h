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
#ifndef OES_COMMON_AVROBINARY_H
#define OES_COMMON_AVROBINARY_H

#include "export.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Stream.hh>
#include <avro/Encoder.hh>
#include <avro/Decoder.hh>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <vector>

namespace oes::common {

//============================================================================

class OES_COMMON_EXPORT AvroBinaryEncoder final : public ::avro::Encoder {
    std::vector<uint8_t> buf_;
    ::avro::OutputStream* p_os_ = nullptr;

public:
    explicit AvroBinaryEncoder(size_t capacity = 1024);

    void clear();
    size_t capacity() const;
    void reserve(size_t capacity);
    void encodingAsString(std::string& encoded) const;
    void encodingAsVector(std::vector<uint8_t>& encoded) const;
    void moveEncodingTo(std::vector<uint8_t>& encoded);

public: // Encoder interface
    void init(::avro::OutputStream& os) override;

    void flush() override;
    int64_t byteCount() const override;

    void encodeNull() override;
    void encodeBool(bool b) override;
    void encodeInt(int32_t i) override;
    void encodeLong(int64_t l) override;
    void encodeFloat(float f) override;
    void encodeDouble(double d) override;
    void encodeString(const std::string& s) override;
    void encodeBytes(const uint8_t* bytes, size_t len) override;
    void encodeFixed(const uint8_t* bytes, size_t len) override;

    void encodeEnum(size_t e) override;
    void arrayStart() override;
    void arrayEnd() override;
    void mapStart() override;
    void mapEnd() override;
    void setItemCount(size_t count) override;
    void startItem() override;
    void encodeUnionIndex(size_t e) override;
};

//============================================================================

class OES_COMMON_EXPORT AvroBinaryDecoder final : public ::avro::Decoder {
    ::avro::StreamReader in_;

public:
    void init(::avro::InputStream& is) override;

    void decodeNull() override;
    bool decodeBool() override;
    int32_t decodeInt() override;
    int64_t decodeLong() override;
    float decodeFloat() override;
    double decodeDouble() override;
    void decodeString(std::string& value) override;
    void decodeBytes(std::vector<uint8_t>& value) override;
    void decodeFixed(size_t n, std::vector<uint8_t>& value) override;

    size_t decodeEnum() override;
    size_t arrayStart() override;
    size_t arrayNext() override;
    size_t mapStart() override;
    size_t mapNext() override;
    size_t decodeUnionIndex() override;

    void skipString() override;
    void skipBytes() override;
    void skipFixed(size_t n) override;
    size_t skipArray() override;
    size_t skipMap() override;
    void drain() override;
};

//============================================================================

} // namespace oes::common

#endif // OES_COMMON_AVROBINARY_H
