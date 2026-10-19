// ============================================================================
// Copyright 2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_COMMON_ETP12_ARRAYUTILS_H
#define OES_COMMON_ETP12_ARRAYUTILS_H

#include "../export.h"
#include "Messages.h"

#include <vector>

namespace oes::common::etp12 {

//============================================================================

using AryAny   = Datatypes::AnyArray;
using AryType  = Datatypes::AnyArrayType;
using AryUnion = Datatypes::AnyArray_item_v;
using AryEnum  = Datatypes::AnyArray_item_e;
using AryIdent = Datatypes::DataArrayTypes::DataArrayIdentifier;
using AryMeta  = Datatypes::DataArrayTypes::DataArrayMetadata;
using AryData  = Datatypes::DataArrayTypes::DataArray;

struct AryInfo {
    AryIdent ident_;
    AryMeta  meta_;
};

using AryInfoVec = std::vector<AryInfo>;

//============================================================================

// A non-owning view of the memory of an ETP array, backed by that array's own
// memory, EXCEPT for ArrayOfBoolean, where the memory is backed by an external
// vector, to work around the fact std::vector<bool> has no .data() accessor.
struct AryMemRef {
    const void* data_ = nullptr;
    AryType type_ = AryType::eArrayOfBoolean; // enum has no "null" value...
    size_t esize_ = 0;
    size_t size_ = 0; // element count (not its byte size)

    bool isValid() const { return data_ && esize_; }
    explicit operator bool() const { return isValid(); }

    bool empty() const { return size_ == 0; }
    size_t size() const { return size_; }
    size_t byteSize() const { return esize_ * size_; }

    const uint8_t* data()  const { return static_cast<const uint8_t*>(data_); }
    const uint8_t* begin() const { return data(); }
    const uint8_t* end()   const { return data() + byteSize(); }
};

OES_COMMON_EXPORT
AryMemRef aryMemoryOf(const AryUnion& ary, std::vector<int8_t>& bool_ary_mem);

inline
AryMemRef aryMemoryOf(const AryData& ary, std::vector<int8_t>& bool_ary_mem) {
    return aryMemoryOf(ary.m_data.m_item, bool_ary_mem);
}

//============================================================================

OES_COMMON_EXPORT
AryType aryTypeOf(const AryUnion& ary);

inline
AryType aryTypeOf(const AryMeta& meta) {
    return meta.m_transportArrayType; // for consistency with other help overloads
}

inline
AryType aryTypeOf(const AryData& ary) {
    return aryTypeOf(ary.m_data.m_item);
}

//============================================================================

// Poor man's version of reduce+multiplies w/o <numeric>+<functional> headers
template <typename T>
T productOf(const std::vector<T>& dims, size_t start, size_t end) {
    T product = 1;
    for (size_t i = start; i < end; i++) {
        product *= dims[i];
    }
    return product;
}

template <typename T>
T productOf(const std::vector<T>& dims, size_t start = 0) {
    return productOf(dims, start, dims.size());
}

//============================================================================

OES_COMMON_EXPORT
size_t arySizeOf(const AryUnion& ary);

inline
size_t arySizeOf(const AryMeta& meta) {
    return productOf(meta.m_dimensions);
}

inline
size_t arySizeOf(const AryData& ary) {
    return arySizeOf(ary.m_data.m_item);
}

//============================================================================

OES_COMMON_EXPORT
size_t byteSizeOf(AryType type);

OES_COMMON_EXPORT
size_t byteSizeOf(const AryUnion& ary);

inline
size_t byteSizeOf(const AryMeta& meta) {
    return byteSizeOf(meta.m_transportArrayType) * arySizeOf(meta);
}

inline
size_t byteSizeOf(const AryData& ary) {
    return byteSizeOf(ary.m_data.m_item);
}

//============================================================================

// Assumes 0 slowest, N-1 fastest
struct OES_COMMON_EXPORT AryDivision {
    size_t esize_ = 0;
    std::vector<int64_t> dims_;

    std::vector<int64_t> full_counts_;
    std::vector<int64_t> tail_counts_;

    std::vector<bool> are_tails_;
    std::vector<int64_t> offsets_; // in bytes
    std::vector<std::vector<int64_t>> starts_;

    static AryDivision make(
        std::vector<int64_t> dims, size_t esize, size_t max_byte_size
    );

    bool empty() const {
        return starts_.empty();
    }

    size_t size() const {
        return starts_.size();
    }

    template <typename V>
    int64_t initDataSubarraysType(size_t idx, V& start_vec, V& count_vec) const {
        const bool is_tail = are_tails_[idx];
        const auto& counts = is_tail? tail_counts_: full_counts_;
        const auto& starts = starts_[idx];

        // .assign() not op= to allow implicit conversions (int64_t -> hsize_t)
        start_vec.assign(starts.begin(), starts.end());
        count_vec.assign(counts.begin(), counts.end());
        return offsets_[idx];
    }
};

//============================================================================

} // namespace oes::common::etp12

#endif
