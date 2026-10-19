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
#ifndef OES_COMMON_AVROUTILS_H
#define OES_COMMON_AVROUTILS_H

#include "export.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/Stream.hh>
#include <boost/array.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <array>
#include <string>

namespace oes::core { class Guid; } // fwd-decl

namespace oes::common {

//============================================================================

OES_COMMON_EXPORT
void assignUid(std::array<uint8_t, 16>& lhs, const oes::core::Guid& rhs);

OES_COMMON_EXPORT
void assignUid(oes::core::Guid& lhs, const std::array<uint8_t, 16>& rhs);

//============================================================================

OES_COMMON_EXPORT
void assignUid(boost::array<uint8_t, 16>& lhs, const oes::core::Guid& rhs);

OES_COMMON_EXPORT
void assignUid(oes::core::Guid& lhs, const boost::array<uint8_t, 16>& rhs);

//============================================================================

class OES_COMMON_EXPORT MemoryInputStream2 : public ::avro::InputStream {
    const uint8_t* data_;
    size_t size_;
    size_t curLen_;

public:
    explicit MemoryInputStream2(const uint8_t* data = nullptr, size_t len = 0);

    void init(const uint8_t* data, size_t len);

public: // ::avro::InputStream interface
    bool next(const uint8_t** data, size_t* len) override;
    void backup(size_t len) override;
    void skip(size_t len) override;
    size_t byteCount() const override;
};

//============================================================================

struct OES_COMMON_EXPORT StringAsAvroIStreamView : MemoryInputStream2 {
    StringAsAvroIStreamView() = default;
    explicit StringAsAvroIStreamView(
        std::string& s // non-const to avoid temporaries
    );

    void init(std::string& s); // non-const to avoid temporaries
    void reset();
};

//============================================================================

class OES_COMMON_EXPORT StringAsAvroOStreamView : public ::avro::OutputStream {
    std::string& buf_;
    const size_t chunkSize_;
    const size_t maxSize_;

public:
    StringAsAvroOStreamView(std::string& buf, size_t chunkSize, size_t maxSize);
    ~StringAsAvroOStreamView() override = default;

    StringAsAvroOStreamView(StringAsAvroOStreamView&&) = delete;
    StringAsAvroOStreamView& operator=(StringAsAvroOStreamView&&) = delete;

    StringAsAvroOStreamView(const StringAsAvroOStreamView&) = delete;
    StringAsAvroOStreamView& operator=(const StringAsAvroOStreamView&) = delete;

    // avro::OutputStream interface
    bool next(uint8_t** data, size_t* len) override;
    void backup(size_t len) override;
    uint64_t byteCount() const override;
    void flush() override;
};

//============================================================================

} // namespace oes::common

#endif // OES_COMMON_AVROUTILS_H
