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

#include "ArrayUtils.h"

#include <cassert>
#include <cmath>

/*!
 * \file oes/common/etp12/ArrayUtils.h
 * \brief Utilities around ETP arrays and sub-arrays.
 */

namespace {

template <typename TAry>
const decltype(TAry::m_values)& ary_as(const oes::common::etp12::AryUnion& ary) {
    return std::get<TAry>(ary).m_values;
}

} // namespace

namespace oes::common::etp12 {

using namespace oes::common::etp12::Datatypes;

using bytes = std::vector<uint8_t>;

//============================================================================

AryType aryTypeOf(const AryUnion& ary) {
    switch (ary.index()) {
    case size_t(AryEnum::eArrayOfBoolean):return AryType::eArrayOfBoolean;
    case size_t(AryEnum::eArrayOfInt):    return AryType::eArrayOfInt;
    case size_t(AryEnum::eArrayOfLong):   return AryType::eArrayOfLong;
    case size_t(AryEnum::eArrayOfFloat):  return AryType::eArrayOfFloat;
    case size_t(AryEnum::eArrayOfDouble): return AryType::eArrayOfDouble;
    case size_t(AryEnum::eArrayOfString): return AryType::eArrayOfString;
    case size_t(AryEnum::eBytes):         return AryType::eBytes;
    } // no default on purpose

    assert(false);
    return AryType::eArrayOfBoolean;
}

//============================================================================

size_t arySizeOf(const AryUnion& ary) {
    switch (ary.index()) {
    case size_t(AryEnum::eArrayOfBoolean):return ary_as<ArrayOfBoolean>(ary).size();
    case size_t(AryEnum::eArrayOfInt):    return ary_as<ArrayOfInt>(ary).size();
    case size_t(AryEnum::eArrayOfLong):   return ary_as<ArrayOfLong>(ary).size();
    case size_t(AryEnum::eArrayOfFloat):  return ary_as<ArrayOfFloat>(ary).size();
    case size_t(AryEnum::eArrayOfDouble): return ary_as<ArrayOfDouble>(ary).size();
    case size_t(AryEnum::eArrayOfString): return ary_as<ArrayOfString>(ary).size();
    case size_t(AryEnum::eBytes):         return std::get<bytes>(ary).size();
    } // no default on purpose

    assert(false);
    return 0;
}

//============================================================================

size_t byteSizeOf(AryType type) {
    switch (type) {
    case AryType::eArrayOfBoolean: return 1;
    case AryType::eArrayOfInt:     return 4;
    case AryType::eArrayOfLong:    return 8;
    case AryType::eArrayOfFloat:   return 4;
    case AryType::eArrayOfDouble:  return 8;
    case AryType::eArrayOfString:  break;
    case AryType::eBytes:          return 1;
    } // no default on purpose

    assert(false);
    return 0;
}

size_t byteSizeOf(const AryUnion& ary) {
    switch (ary.index()) {
    case size_t(AryEnum::eArrayOfBoolean):return sizeof(bool)     * ary_as<ArrayOfBoolean>(ary).size();
    case size_t(AryEnum::eArrayOfInt):    return sizeof(int32_t)  * ary_as<ArrayOfInt>(ary).size();
    case size_t(AryEnum::eArrayOfLong):   return sizeof(int64_t)  * ary_as<ArrayOfLong>(ary).size();
    case size_t(AryEnum::eArrayOfFloat):  return sizeof(float)    * ary_as<ArrayOfFloat>(ary).size();
    case size_t(AryEnum::eArrayOfDouble): return sizeof(double)   * ary_as<ArrayOfDouble>(ary).size();
    case size_t(AryEnum::eArrayOfString): break; // TODO
    case size_t(AryEnum::eBytes):         return sizeof(uint8_t)  * std::get<bytes>(ary).size();
    } // no default on purpose

    assert(false);
    return 0;
}

//============================================================================

template <typename T>
AryMemRef aryMemOf(AryType type, const std::vector<T>& vec) {
    AryMemRef mem;
    mem.esize_ = sizeof(T);
    mem.data_ = vec.data();
    mem.size_ = vec.size();
    mem.type_ = type;
    return mem;
}

inline
AryMemRef aryMemOfBool(const AryUnion& ary, std::vector<int8_t>& bool_ary_mem) {
    // vector<bool> is "special". Copy to "regular" vector of int8_t
    const auto& bits = ary_as<ArrayOfBoolean>(ary);
    bool_ary_mem.assign(bits.begin(), bits.end());
    return aryMemOf(AryType::eArrayOfBoolean, bool_ary_mem);
}

AryMemRef aryMemoryOf(const AryUnion& ary, std::vector<int8_t>& bool_ary_mem) {
    switch (ary.index()) {
    case size_t(AryEnum::eArrayOfBoolean):return aryMemOfBool(ary, bool_ary_mem);
    case size_t(AryEnum::eArrayOfInt):    return aryMemOf(AryType::eArrayOfInt, ary_as<ArrayOfInt>(ary));
    case size_t(AryEnum::eArrayOfLong):   return aryMemOf(AryType::eArrayOfLong, ary_as<ArrayOfLong>(ary));
    case size_t(AryEnum::eArrayOfFloat):  return aryMemOf(AryType::eArrayOfFloat, ary_as<ArrayOfFloat>(ary));
    case size_t(AryEnum::eArrayOfDouble): return aryMemOf(AryType::eArrayOfDouble, ary_as<ArrayOfDouble>(ary));
    case size_t(AryEnum::eArrayOfString): break; // No can do. Non-continuous memory
    case size_t(AryEnum::eBytes):         return aryMemOf(AryType::eBytes, std::get<bytes>(ary));
    } // no default on purpose

    assert(false);
    return {};
}

//============================================================================

AryDivision AryDivision::make(
    std::vector<int64_t> dims, size_t esize, size_t max_byte_size
) {
    assert(!dims.empty());
    assert(0 < esize && esize <= 8);

    AryDivision div;
    div.dims_= dims;
    div.esize_ = esize;

    if ((productOf(dims) * esize) <= max_byte_size) {
        return div;
    }

    const int64_t max_msg_size_ = int64_t(max_byte_size);
    const size_t rank = dims.size();

    // Find the rank "below" which we can fetch all elements in a single message.
    // For that special "cutoff" rank, we typically still need to divide it in
    // several messages: that's our "inner" loop. "Above" that cutoff rank, we
    // will need to get each dim's index 1-by-1: that's our "outer" loop.
    // (Algo adapted from LD's one in SyncClientImpl::getDataArray())
    size_t cutoff_rank = 0;
    for (int64_t bytes = esize, i = rank - 1; i >= 0; --i) {
        bytes *= dims[i];
        cutoff_rank = i;
        if (bytes > max_msg_size_) {
            break;
        }
    }

    // Compute inner-loop constants
    const int64_t inner_block_bytes = esize * productOf(dims, cutoff_rank + 1);
    const int64_t inner_elem_count = int64_t(max_msg_size_ / inner_block_bytes); // per-message
    const int64_t inner_loop_count = int64_t(std::ceil(double(dims[cutoff_rank]) / inner_elem_count));
    int64_t inner_elem_count_last_iter = dims[cutoff_rank] % inner_elem_count;
    if (inner_elem_count_last_iter == 0) {
        inner_elem_count_last_iter = inner_elem_count;
    }

    // Compute outer-loop constants
    const int64_t outer_loop_count = productOf(dims, 0, cutoff_rank);
    std::vector<int64_t> counts(dims); // "inner" dims copied as-is
    for (size_t i = 0; i < cutoff_rank; ++i) {
        counts[i] = 1; // "outer" dims always 1
    }
    counts[cutoff_rank] = inner_elem_count; // != on each last inner loop iter
    const int64_t inner_max_count = productOf(counts);

    div.full_counts_ = counts;
    div.tail_counts_ = counts;
    div.tail_counts_[cutoff_rank] = inner_elem_count_last_iter;

    const size_t count = outer_loop_count * inner_loop_count;
    div.are_tails_.reserve(count);
    div.offsets_.reserve(count);
    div.starts_.reserve(count);

    std::vector<int64_t> starts(rank, 0);
    const int64_t inner_full_size = productOf(dims, cutoff_rank);
    for (int64_t outer_idx = 0; outer_idx < outer_loop_count; ++outer_idx) {
        // Compute outer-loop-iteration-specific constants,
        // i.e. start indices for dimensions "above" the cutoff_rank
        int64_t remaining = outer_idx;
        for (size_t i = 0; i < cutoff_rank; ++i) {
            int64_t lp = productOf(dims, i + 1, cutoff_rank);
            starts[i] = remaining / lp;
            remaining = remaining % lp;
        }

        // Create each message by decomposing along the dividing dimension
        for (int64_t inner_idx = 0; inner_idx < inner_loop_count; ++inner_idx) {
            starts[cutoff_rank] = inner_idx * inner_elem_count;

            div.are_tails_.emplace_back(inner_idx == inner_loop_count - 1);
            div.offsets_.emplace_back((outer_idx * inner_full_size) + (inner_idx * inner_max_count));
            div.starts_.emplace_back(starts);
        }
    }

    return div;
}

//============================================================================

} // namespace oes::common::etp12
