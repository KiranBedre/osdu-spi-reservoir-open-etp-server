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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "ArrayProto.h"

#include "Repo.h"
#include "SpaceDML.h"
#include "Session.h"
#include "TransProto.h"
#include "AuditLog.h"

#include "../etp12/Server.hpp"
#include "../etp12/Extensions.h"
#include "../etp12/MultiPartReply.hpp"

#include <oes/common/epc/FilePart.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/common/HdfAry.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThreadUtils.hpp>
#include <oes/core/utils/Otel.h>
#include <oes/core/utils/String.h>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsHelper.h>
#include <oes/core/utils/MetricsNames.h>

#include <unordered_map>
#include <numeric>
#include <chrono>
#include <limits>

using namespace oes::eml::etp12;
using namespace oes::common::etp12::Datatypes;

using oes::core::Guid;
using oes::core::thisThreadId;

using oes::common::etp12::EmlUri;

using oes::common::etp12::Protocol::EtpResponseStatus;

namespace hdf = oes::common::hdf;
using oes::eml::openkv::SqlAry;
using oes::eml::openkv::SpaceReader;
using oes::eml::openkv::SpaceWriter;

//============================================================================

namespace {

using bytes_t = std::vector<uint8_t>;

// Overflow-safe multiplication of array dimensions.
// Returns the product of all values in `dims`, or -1 if any value is negative
// or if the multiplication would overflow int64_t.
inline int64_t checkedDimsProduct(const std::vector<int64_t>& dims) {
    int64_t product = 1;
    for (int64_t d : dims) {
        if (d < 0) return -1;
        if (d != 0 && product > std::numeric_limits<int64_t>::max() / d) return -1;
        product *= d;
    }
    return product;
}

template <typename TAry>
const decltype(TAry::m_values)& ary_as(const AnyArray_item_v& ary) {
    return std::get<TAry>(ary).m_values;
}

Guid getBlobUid(oes::postgresql::Statement& stmt, int pos, bool null_ok = false) {
    OES_UNUSED(null_ok);
    return stmt.getGuid(pos);
}

bool getAryRankAndDims(
    oes::postgresql::Statement& stmt, int pos,
    const std::string& path, // for errors only
    std::vector<int64_t>& m_dimensions
) {
    const int rank = stmt.getInt(pos + 0);
    m_dimensions.resize(rank);
    switch (rank) {
    case  4: m_dimensions[3] = stmt.getInt64(pos + 4); [[fallthrough]];
    case  3: m_dimensions[2] = stmt.getInt64(pos + 3); [[fallthrough]];
    case  2: m_dimensions[1] = stmt.getInt64(pos + 2); [[fallthrough]];
    case  1: m_dimensions[0] = stmt.getInt64(pos + 1);
        break;
    default: // Not supported
        OES_ERROR("Unexpected rank = {}: {}", rank, path);
        return false;
    }
    return true;
}

bool getAryType(hdf::HdfAryType type, Datatypes::AnyArrayType& m_arrayType) {
    using oes::eml::etp12::Datatypes::AnyArrayType;

    switch (type) {
    case hdf::HdfAryType::eAryOfInt:     m_arrayType = AnyArrayType::eArrayOfInt; break;
    case hdf::HdfAryType::eAryOfDouble:  m_arrayType = AnyArrayType::eArrayOfDouble; break;
    case hdf::HdfAryType::eAryOfFloat:   m_arrayType = AnyArrayType::eArrayOfFloat; break;
    case hdf::HdfAryType::eAryOfBytes:   m_arrayType = AnyArrayType::eBytes; break;
    case hdf::HdfAryType::eAryOfBoolean: m_arrayType = AnyArrayType::eArrayOfBoolean; break;
    case hdf::HdfAryType::eAryOfLong:    m_arrayType = AnyArrayType::eArrayOfLong; break;
    case hdf::HdfAryType::eAryOfString:  [[fallthrough]]; // Not used by RESQML
    case hdf::HdfAryType::eAryOfVoid:    [[fallthrough]]; // Impossible
    default: // Not supported
        return false;
    }
    return true;
}

bool getAryType(
    oes::postgresql::Statement& stmt, int pos,
    Datatypes::AnyArrayType& m_arrayType
) {
    const int value_type = stmt.getInt(pos);
    return getAryType(hdf::HdfAryType(value_type), m_arrayType);
}

inline
const char* readBlobPartially2D(
    SpaceReader& r, const SqlAry& sql_ary,
    unsigned char*const dst,
    const int span, const int src_len,
    const int64_t last_dim,
    const int64_t i0_beg, const int64_t i0_end,
    const int64_t last_beg
) {
    int64_t i = 0;
    for (int64_t i0 = i0_beg; i0 < i0_end; ++i0, ++i) {
        const int64_t dst_off = i * src_len; // bytes
        const int64_t src_off = (i0 * last_dim) + last_beg; // elements

        const char* err = r.read_ary(sql_ary, dst + dst_off, src_len, span * src_off);
        if (err) {
            return err;
        }
    }
    return nullptr;
}

inline
const char* readBlobPartially3D(
    SpaceReader& r, const SqlAry& sql_ary,
    unsigned char*const dst,
    const int span, const int src_len,
    const int64_t i1_dim, const int64_t last_dim,
    const int64_t i0_beg, const int64_t i0_end,
    const int64_t i1_beg, const int64_t i1_end,
    const int64_t last_beg
) {
    int64_t i = 0;
    for (int64_t i0 = i0_beg; i0 < i0_end; ++i0)
    for (int64_t i1 = i1_beg; i1 < i1_end; ++i1, ++i) {
        const int64_t dst_off = i * src_len; // bytes
        const int64_t src_off = (i0 * i1_dim + i1) * last_dim + last_beg; // elements

        const char* err = r.read_ary(sql_ary, dst + dst_off, src_len, span * src_off);
        if (err) {
            return err;
        }
    }
    return nullptr;
}

inline
const char* readBlobPartially4D(
    SpaceReader& r, const SqlAry& sql_ary,
    unsigned char*const dst,
    const int span, const int src_len,
    const int64_t i1_dim, const int64_t i2_dim, const int64_t last_dim,
    const int64_t i0_beg, const int64_t i0_end,
    const int64_t i1_beg, const int64_t i1_end,
    const int64_t i2_beg, const int64_t i2_end,
    const int64_t last_off
) {
    int64_t i = 0;
    for (int64_t i0 = i0_beg; i0 < i0_end; ++i0)
    for (int64_t i1 = i1_beg; i1 < i1_end; ++i1)
    for (int64_t i2 = i2_beg; i2 < i2_end; ++i2, ++i) {
        const int64_t dst_off = i * src_len; // bytes
        const int64_t src_off = ((i0 * i1_dim + i1) * i2_dim + i2) * last_dim + last_off; // elements

        const char* err = r.read_ary(sql_ary, dst + dst_off, src_len, span * src_off);
        if (err) {
            return err;
        }
    }
    return nullptr;
}

// dim[0] slowest, dim[rank-1] fastest
const char* readBlobPartially(
    SpaceReader& r, const SqlAry& sql_ary,
    void* p_data, int span, int rank,
    const int64_t* dim, const int64_t* off, const int64_t* len
) {
    assert(rank >= 0);
    assert(span > 0);
    assert(sql_ary.valid() && p_data && dim && off && len);

    // needed for pointer arithmetic
    unsigned char*const dst = static_cast<unsigned char*>(p_data);

    if (rank == 0) { // read the whole blob (valid when recursing only)
        return r.read_ary(sql_ary, dst, span, 0);
    }

    const int64_t last_dim = dim[rank - 1]; // aka fastest-dimension
    const int64_t last_len = len[rank - 1];

    if (last_dim == last_len) {
        // when the fastest dim is the same as the slab one, that's
        // equivalent to reading a lower-rank larger (contiguous) span.
        return readBlobPartially(
            r, sql_ary, p_data, int(span * last_dim), rank - 1, dim, off, len
        );
    }

    // We've already "aggregated" continuous spans, via the recursion above
    // Thus here, how much we read from the blob is fixed, whatever the rank
    const int src_len = int(span * last_len); // bytes

    switch (rank) {
    case  1: // read 1 continuous "chunk" of the blob
        return r.read_ary(sql_ary, dst, src_len, span * off[0]);

    case  2: // read N continuous "chunk" of the blob
        return readBlobPartially2D(
            r, sql_ary, dst, span,
            src_len, last_dim,
            off[0], off[0] + len[0],
            off[1]
        );

    case  3: // read N*M continuous "chunk" of the blob
        return readBlobPartially3D(
            r, sql_ary, dst, span,
            src_len, dim[1], last_dim,
            off[0], off[0] + len[0],
            off[1], off[1] + len[1],
            off[2]
        );

    case  4: // read N*M*L continuous "chunk" of the blob
        return readBlobPartially4D(
            r, sql_ary, dst, span,
            src_len, dim[1], dim[2], last_dim,
            off[0], off[0] + len[0],
            off[1], off[1] + len[1],
            off[2], off[2] + len[2],
            off[3]
        );
    }

    assert(false);
    return nullptr;
}

template <class T>
void readDataArray(
    SpaceReader& r, const SqlAry& sql_ary,
    DataArrayTypes::DataArray& ary
) {
    using vec_type = decltype(T::m_values);
    using val_type = typename vec_type::value_type;
    const size_t elem_size = sizeof(val_type);

    const size_t bytes = sql_ary.byte_size();
    const size_t count = sql_ary.size(elem_size);

    assert(bytes <= 2u*1024*1024*1024); // < 2GiB: Should read sub-arrays instead

    T array_of;
    vec_type& vec = array_of.m_values;
    vec.resize(count);
    auto* buf = reinterpret_cast<unsigned char*>(vec.data());
    r.read_ary(sql_ary, buf, int(bytes), 0);
    ary.m_data.m_item = std::move(array_of);
}

// AnyArray_item_u is not "regular", the vector is not encapsulated inside
// a struct like the others, but directly used. Must specialize for it.
// Warning: Dev-22 added ArrayOfBytes as a std::vector<std::vector<uint8_t>>
using ByteVec = std::vector<uint8_t>;

template <>
void readDataArray<ByteVec>(
    SpaceReader& r, const SqlAry& sql_ary,
    DataArrayTypes::DataArray& ary
) {
    const size_t bytes = sql_ary.byte_size();

    assert(bytes <= 2u*1024*1024*1024); // < 2GiB: Should read sub-arrays instead

    ByteVec array_of(bytes);
    r.read_ary(sql_ary, array_of.data(), int(bytes), 0);
    ary.m_data.m_item = std::move(array_of);
}

template <>
void readDataArray<ArrayOfBoolean>(
    SpaceReader& r, const SqlAry& sql_ary,
    DataArrayTypes::DataArray& ary
) {
    const size_t bytes = sql_ary.byte_size();

    assert(bytes <= 2u*1024*1024*1024); // < 2GiB: Should read sub-arrays instead

    // vector<bool> does not expose its internal buffer as a bool*,
    // because it uses 1-bit-per-bool internally, not 1-byte-per-bool.
    // We must first read into a temporary "real" bool-array, then copy
    std::vector<int8_t> bools(bytes);
    unsigned char* uchars = reinterpret_cast<unsigned char*>(bools.data());
    r.read_ary(sql_ary, uchars, int(bytes), 0);

    ArrayOfBoolean array_of;
    array_of.m_values.assign(bools.begin(), bools.end());
    ary.m_data.m_item = std::move(array_of);
}

template <class T>
const char* readDataSubarray(
    SpaceReader& r, const SqlAry& sql_ary,
    DataArrayTypes::DataArray& ary, // full dims on input, subarray dims on output
    const DataArrayTypes::GetDataSubarraysType& sub,
    int64_t total_size // in sub also, but already computed in caller
) {
    assert(total_size > 0);

    using vector_type = decltype(T::m_values);
    using value_type  = typename vector_type::value_type;
    const size_t elem_size = sizeof(value_type);

    T array_of;
    array_of.m_values.resize(size_t(total_size));
    value_type* p_data = array_of.m_values.data();

    const char* err = readBlobPartially(
        r, sql_ary, p_data, int(elem_size), int(ary.m_dimensions.size()),
        ary.m_dimensions.data(), sub.m_starts.data(), sub.m_counts.data()
    );
    if (!err) {
        ary.m_data.m_item = std::move(array_of);
        ary.m_dimensions = sub.m_counts;
    }
    return err;
}

template <> // not "regular". see above
const char* readDataSubarray<ByteVec>(
    SpaceReader& r, const SqlAry& sql_ary,
    DataArrayTypes::DataArray& ary, // full dims on input, subarray dims on output
    const DataArrayTypes::GetDataSubarraysType& sub,
    int64_t total_size // in sub also, but already computed in caller
) {
    assert(total_size > 0);

    ByteVec array_of(static_cast<size_t>(total_size), 0);

    const char* err = readBlobPartially(
        r, sql_ary, array_of.data(), 1, int(ary.m_dimensions.size()),
        ary.m_dimensions.data(), sub.m_starts.data(), sub.m_counts.data()
    );
    if (!err) {
        ary.m_data.m_item = std::move(array_of);
        ary.m_dimensions = sub.m_counts;
    }
    return err;
}

template <> // vector<bool> is "special", has no .data() member!
const char* readDataSubarray<ArrayOfBoolean>(
    SpaceReader& r, const SqlAry& sql_ary,
    DataArrayTypes::DataArray& ary, // full dims on input, subarray dims on output
    const DataArrayTypes::GetDataSubarraysType& sub,
    int64_t total_size // in sub also, but already computed in caller
) {
    assert(total_size > 0);

    ByteVec bytes(static_cast<size_t>(total_size), 0);
    uint8_t* p_data = bytes.data();

    const char* err = readBlobPartially(
        r, sql_ary, p_data, 1, int(ary.m_dimensions.size()),
        ary.m_dimensions.data(), sub.m_starts.data(), sub.m_counts.data()
    );
    if (!err) {
        ArrayOfBoolean array_of;
        array_of.m_values.assign(bytes.begin(), bytes.end());
        ary.m_data.m_item = std::move(array_of);
        ary.m_dimensions = sub.m_counts;
    }
    return err;
}

//============================================================================

inline
const char* writeAry1D(
    SpaceWriter& w, const SqlAry& ary,
    const void* sub_buf, size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 1);
    assert(sub_dims.size() >= 1);
    assert(sub_offs.size() >= 1);

    eml_uint64 off = elem_bytes * sub_offs[0];
    const int  len = int(elem_bytes * sub_dims[0]);

    return w.write_ary(ary, sub_buf, off, len);
}

const char* writeAry2D(
    SpaceWriter& w, const SqlAry& ary,
    const void* sub_buf, size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 2);
    assert(sub_dims.size() >= 2);
    assert(sub_offs.size() >= 2);
    assert(sub_dims[1] != ary.dims_[1]); // not the degenerate case

    const auto* sub_ubuf = static_cast<const unsigned char*>(sub_buf);

    const int64_t     n1 = ary.dims_[1];
    const int64_t sub_n0 = sub_dims[0];
    const int64_t sub_n1 = sub_dims[1];

    const int len = int(elem_bytes * sub_n1);

    for (int64_t i0 = sub_offs[0], sub_i0 = 0; sub_i0 < sub_n0; ++i0, ++sub_i0) {
        const int64_t i1 = sub_offs[1];

        eml_uint64 off = elem_bytes * (    i0 *     n1 + i1);
        int64_t    sub_off = elem_bytes * (sub_i0 * sub_n1     );

        if (const char* write_err = w.write_ary(ary, sub_ubuf + sub_off, off, len)) {
            return write_err;
        }
    }

    return nullptr;
}

const char* writeAry3D(
    SpaceWriter& w, const SqlAry& ary,
    const void* sub_buf, size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 3);
    assert(sub_dims.size() >= 3);
    assert(sub_offs.size() >= 3);
    assert(sub_dims[2] != ary.dims_[2]); // not the degenerate case

    const auto* sub_ubuf = static_cast<const unsigned char*>(sub_buf);

    const int64_t     n1 = ary.dims_[1];
    const int64_t     n2 = ary.dims_[2];
    const int64_t sub_n0 = sub_dims[0];
    const int64_t sub_n1 = sub_dims[1];
    const int64_t sub_n2 = sub_dims[2];

    const int len = int(elem_bytes * sub_n2);

    for (int64_t i0 = sub_offs[0], sub_i0 = 0; sub_i0 < sub_n0; ++i0, ++sub_i0)
    for (int64_t i1 = sub_offs[1], sub_i1 = 0; sub_i1 < sub_n1; ++i1, ++sub_i1) {
        const int64_t i2 = sub_offs[2];

        eml_uint64 off = elem_bytes * (    i0 * (n1*n2) +     i1 *     n2 + i2);
        int64_t    sub_off = elem_bytes * ((sub_i0 * sub_n1 + sub_i1) * sub_n2);

        if (const char* write_err = w.write_ary(ary, sub_ubuf + sub_off, off, len)) {
            return write_err;
        }
    }

    return nullptr;
}

const char* writeAry4D(
    SpaceWriter& w, const SqlAry& ary,
    const void* sub_buf, size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 4);
    assert(sub_dims.size() >= 4);
    assert(sub_offs.size() >= 4);
    assert(sub_dims[3] != ary.dims_[3]); // not the degenerate case

    const auto* sub_ubuf = static_cast<const unsigned char*>(sub_buf);

    const int64_t     n1 = ary.dims_[1];
    const int64_t     n2 = ary.dims_[2];
    const int64_t     n3 = ary.dims_[3];
    const int64_t sub_n0 = sub_dims[0];
    const int64_t sub_n1 = sub_dims[1];
    const int64_t sub_n2 = sub_dims[2];
    const int64_t sub_n3 = sub_dims[3];

    const int len = int(elem_bytes * sub_n3);

    for (int64_t i0 = sub_offs[0], sub_i0 = 0; sub_i0 < sub_n0; ++i0, ++sub_i0)
    for (int64_t i1 = sub_offs[1], sub_i1 = 0; sub_i1 < sub_n1; ++i1, ++sub_i1)
    for (int64_t i2 = sub_offs[2], sub_i2 = 0; sub_i2 < sub_n2; ++i2, ++sub_i2) {
        const int64_t i3 = sub_offs[3];

        eml_uint64 off = elem_bytes * (    i0 * (n1*n2*n3) +     i1 * (n2*n3) +     i2 *     n3 + i3);
        int64_t    sub_off = elem_bytes * (((sub_i0 * sub_n1 + sub_i1) * sub_n2 + sub_i2) * sub_n3);

        if (const char* write_err = w.write_ary(ary, sub_ubuf + sub_off, off, len)) {
            return write_err;
        }
    }

    return nullptr;
}

// === Suport for bulk insert
struct AryOffsetMeta {
    std::vector<int64_t> sub_off;
    std::vector<eml_uint64> off;
    int len;

    std::string error;
};

std::string check_ary_offset(
    const SqlAry& ary,
    eml_uint64 off,
    int len
) {
    if (ary.is_in_row()) {
        if ((eml_int64(off) + len) > ary.blob_len_) {
            return "Cannot increase array size via incremental write";
        }
    }

    if (!ary.is_single_blob()) {
        assert(eml_int64(off + len) <= ary.usize_);
    }

    return "";
}

AryOffsetMeta getAry1DOff(
    const SqlAry& ary,
    size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 1);
    assert(sub_dims.size() >= 1);
    assert(sub_offs.size() >= 1);

    AryOffsetMeta meta;

    meta.off.push_back(elem_bytes * sub_offs[0]);
    meta.sub_off.push_back(0);
    meta.len = int(elem_bytes * sub_dims[0]);
    meta.error = check_ary_offset(ary, meta.off.front(), meta.len);

    return meta;
}

AryOffsetMeta getAry2D_SubOff_Off(
    const SqlAry& ary,
    size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 2);
    assert(sub_dims.size() >= 2);
    assert(sub_offs.size() >= 2);
    assert(sub_dims[1] != ary.dims_[1]); // not the degenerate case

    const int64_t     n1 = ary.dims_[1];
    const int64_t sub_n0 = sub_dims[0];
    const int64_t sub_n1 = sub_dims[1];

    AryOffsetMeta ret;
    ret.off.reserve(sub_n0);
    ret.sub_off.reserve(sub_n0);

    const int len = int(elem_bytes * sub_n1);
    ret.len = len;

    for (int64_t i0 = sub_offs[0], sub_i0 = 0; sub_i0 < sub_n0; ++i0, ++sub_i0) {
        const int64_t i1 = sub_offs[1];

        const eml_uint64 off = elem_bytes * (i0 * n1 + i1);
        const int64_t sub_off =  elem_bytes * (sub_i0 * sub_n1);
        ret.off.push_back(off);
        ret.sub_off.push_back(sub_off);

        ret.error = check_ary_offset(ary, off, len);
        if (!ret.error.empty()) {
            return ret;
        }
    }

    return ret;
}

AryOffsetMeta getAry3D_SubOff_Off(
    const SqlAry& ary,
    size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 3);
    assert(sub_dims.size() >= 3);
    assert(sub_offs.size() >= 3);
    assert(sub_dims[2] != ary.dims_[2]); // not the degenerate case

    const int64_t     n1 = ary.dims_[1];
    const int64_t     n2 = ary.dims_[2];
    const int64_t sub_n0 = sub_dims[0];
    const int64_t sub_n1 = sub_dims[1];
    const int64_t sub_n2 = sub_dims[2];

    AryOffsetMeta ret;

    ret.off.reserve(sub_n0*sub_n1);
    ret.sub_off.reserve(sub_n0*sub_n1);

    const int len = int(elem_bytes * sub_n2);
    ret.len = len;

    for (int64_t i0 = sub_offs[0], sub_i0 = 0; sub_i0 < sub_n0; ++i0, ++sub_i0)
    for (int64_t i1 = sub_offs[1], sub_i1 = 0; sub_i1 < sub_n1; ++i1, ++sub_i1) {
        const int64_t i2 = sub_offs[2];


        const eml_uint64 off = elem_bytes * (i0 * (n1*n2) + i1 * n2 + i2);
        const int64_t sub_off = elem_bytes * ((sub_i0 * sub_n1 + sub_i1) * sub_n2);
        ret.off.push_back(off);
        ret.sub_off.push_back(sub_off);

        ret.error = check_ary_offset(ary, off, len);
        if (!ret.error.empty()) {
            return ret;
        }
    }

    return ret;
}


AryOffsetMeta getAry4D_SubOff_Off(
    const SqlAry& ary,
    size_t elem_bytes,
    const std::vector<int64_t>& sub_dims,
    const std::vector<int64_t>& sub_offs
) {
    assert(ary.rank_ >= 4);
    assert(sub_dims.size() >= 4);
    assert(sub_offs.size() >= 4);
    assert(sub_dims[3] != ary.dims_[3]); // not the degenerate case

    const int64_t     n1 = ary.dims_[1];
    const int64_t     n2 = ary.dims_[2];
    const int64_t     n3 = ary.dims_[3];
    const int64_t sub_n0 = sub_dims[0];
    const int64_t sub_n1 = sub_dims[1];
    const int64_t sub_n2 = sub_dims[2];
    const int64_t sub_n3 = sub_dims[3];

    AryOffsetMeta ret;
    ret.off.reserve(sub_n0*sub_n1*sub_n2);
    ret.sub_off.reserve(sub_n0*sub_n1*sub_n2);

    const int len = int(elem_bytes * sub_n3);
    ret.len = len;

    for (int64_t i0 = sub_offs[0], sub_i0 = 0; sub_i0 < sub_n0; ++i0, ++sub_i0)
    for (int64_t i1 = sub_offs[1], sub_i1 = 0; sub_i1 < sub_n1; ++i1, ++sub_i1)
    for (int64_t i2 = sub_offs[2], sub_i2 = 0; sub_i2 < sub_n2; ++i2, ++sub_i2) {
        const int64_t i3 = sub_offs[3];

        const eml_uint64 off = elem_bytes * (i0 * (n1*n2*n3) + i1 * (n2*n3) + i2 * n3 + i3);
        const int64_t sub_off = elem_bytes * (((sub_i0 * sub_n1 + sub_i1) * sub_n2 + sub_i2) * sub_n3);
        ret.off.push_back(off);
        ret.sub_off.push_back(sub_off);

        ret.error = check_ary_offset(ary, off, len);
        if (!ret.error.empty()) {
            return ret;
        }
    }

    return ret;
}

//============================================================================

using AryIdent = Datatypes::DataArrayTypes::DataArrayIdentifier;
using AryData  = Datatypes::DataArrayTypes::DataArray;
using AryMeta  = Datatypes::DataArrayTypes::DataArrayMetadata;

EmlUri checkAryIdentIsValidAndNormalizePath(
    oes::eml::openkv::OpenKVRepo_ptr repo, AryIdent& ary_ident, MultiPartError& mp_err, const std::string& cid
) {
    EmlUri uri = EmlUri::parse(ary_ident.m_uri);

    oes::eml::openkv::Space space = repo->findSpaceInfo(uri.dataspace_);
    if(!space.isValid()) {
        mp_err.addError(cid, ENOT_FOUND, "Dataspace not found");
        return {}; // i.e. invalid EmlUri
    }

    if(space.uid_ != uri.dataspace_) {
        uri.dataspace_ = space.uid_;
    }

    if (!uri.guid_.isValid()) {
        mp_err.addError(cid, EINVALID_URI, "Invalid Object UID");
        return {}; // i.e. invalid EmlUri
    }

    if (ary_ident.m_pathInResource.empty()) {
        mp_err.addError(
            cid, EINVALID_URI,
            "Invalid empty pathInResource"
        );
        return {}; // i.e. invalid EmlUri
    }

    // Often queried w/o leading slash, so always store it w/o too
    if (ary_ident.m_pathInResource[0] == '/') {
        ary_ident.m_pathInResource.erase(0, 1);
    }

    return uri;
}

// ============================================================================
// In-memory equivalents of readBlobPartially* — same logic, memcpy instead of r.read_ary

inline
void readBlobPart2DFromMem(
    const unsigned char* src,
    unsigned char* const dst,
    const int span, const int src_len,
    const int64_t last_dim,
    const int64_t i0_beg, const int64_t i0_end,
    const int64_t last_beg
) {
    int64_t i = 0;
    for (int64_t i0 = i0_beg; i0 < i0_end; ++i0, ++i) {
        const int64_t dst_off = i * src_len;
        const int64_t src_off = (i0 * last_dim) + last_beg;
        std::memcpy(dst + dst_off, src + span * src_off, static_cast<size_t>(src_len));
    }
}

inline
void readBlobPart3DFromMem(
    const unsigned char* src,
    unsigned char* const dst,
    const int span, const int src_len,
    const int64_t i1_dim, const int64_t last_dim,
    const int64_t i0_beg, const int64_t i0_end,
    const int64_t i1_beg, const int64_t i1_end,
    const int64_t last_beg
) {
    int64_t i = 0;
    for (int64_t i0 = i0_beg; i0 < i0_end; ++i0)
    for (int64_t i1 = i1_beg; i1 < i1_end; ++i1, ++i) {
        const int64_t dst_off = i * src_len;
        const int64_t src_off = (i0 * i1_dim + i1) * last_dim + last_beg;
        std::memcpy(dst + dst_off, src + span * src_off, static_cast<size_t>(src_len));
    }
}

inline
void readBlobPart4DFromMem(
    const unsigned char* src,
    unsigned char* const dst,
    const int span, const int src_len,
    const int64_t i1_dim, const int64_t i2_dim, const int64_t last_dim,
    const int64_t i0_beg, const int64_t i0_end,
    const int64_t i1_beg, const int64_t i1_end,
    const int64_t i2_beg, const int64_t i2_end,
    const int64_t last_off
) {
    int64_t i = 0;
    for (int64_t i0 = i0_beg; i0 < i0_end; ++i0)
    for (int64_t i1 = i1_beg; i1 < i1_end; ++i1)
    for (int64_t i2 = i2_beg; i2 < i2_end; ++i2, ++i) {
        const int64_t dst_off = i * src_len;
        const int64_t src_off = ((i0 * i1_dim + i1) * i2_dim + i2) * last_dim + last_off;
        std::memcpy(dst + dst_off, src + span * src_off, static_cast<size_t>(src_len));
    }
}

// dim[0] slowest, dim[rank-1] fastest — mirrors readBlobPartially signature
void readBlobPartiallyFromMem(
    const unsigned char* src,
    void* p_data, int span, int rank,
    const int64_t* dim, const int64_t* off, const int64_t* len
) {
    assert(rank >= 0);
    assert(span > 0);
    assert(src && p_data && dim && off && len);

    unsigned char* const dst = static_cast<unsigned char*>(p_data);

    if (rank == 0) {
        std::memcpy(dst, src, static_cast<size_t>(span));
        return;
    }

    const int64_t last_dim = dim[rank - 1];
    const int64_t last_len = len[rank - 1];

    if (last_dim == last_len) {
        readBlobPartiallyFromMem(src, p_data, int(span * last_dim), rank - 1, dim, off, len);
        return;
    }

    const int src_len = int(span * last_len);

    switch (rank) {
    case 1:
        std::memcpy(dst, src + span * off[0], static_cast<size_t>(src_len));
        return;
    case 2:
        readBlobPart2DFromMem(src, dst, span, src_len, last_dim,
            off[0], off[0] + len[0], off[1]);
        return;
    case 3:
        readBlobPart3DFromMem(src, dst, span, src_len, dim[1], last_dim,
            off[0], off[0] + len[0], off[1], off[1] + len[1], off[2]);
        return;
    case 4:
        readBlobPart4DFromMem(src, dst, span, src_len, dim[1], dim[2], last_dim,
            off[0], off[0] + len[0], off[1], off[1] + len[1], off[2], off[2] + len[2], off[3]);
        return;
    }
    assert(false);
}

template <class T>
void readDataSubarrayFromMem(
    const std::vector<uint8_t>& src_bytes,
    DataArrayTypes::DataArray& ary,
    const DataArrayTypes::GetDataSubarraysType& sub,
    int64_t total_size
) {
    using vector_type = decltype(T::m_values);
    using value_type  = typename vector_type::value_type;
    const int elem_size = sizeof(value_type);

    T array_of;
    array_of.m_values.resize(size_t(total_size));
    value_type* p_data = array_of.m_values.data();

    readBlobPartiallyFromMem(
        src_bytes.data(), p_data, elem_size, int(ary.m_dimensions.size()),
        ary.m_dimensions.data(), sub.m_starts.data(), sub.m_counts.data()
    );
    ary.m_dimensions = sub.m_counts;
    ary.m_data.m_item = std::move(array_of);
}

template <>
void readDataSubarrayFromMem<ByteVec>(
    const std::vector<uint8_t>& src_bytes,
    DataArrayTypes::DataArray& ary,
    const DataArrayTypes::GetDataSubarraysType& sub,
    int64_t total_size
) {
    ByteVec array_of(static_cast<size_t>(total_size), 0);
    readBlobPartiallyFromMem(
        src_bytes.data(), array_of.data(), 1, int(ary.m_dimensions.size()),
        ary.m_dimensions.data(), sub.m_starts.data(), sub.m_counts.data()
    );
    ary.m_dimensions = sub.m_counts;
    ary.m_data.m_item = std::move(array_of);
}

template <>
void readDataSubarrayFromMem<ArrayOfBoolean>(
    const std::vector<uint8_t>& src_bytes,
    DataArrayTypes::DataArray& ary,
    const DataArrayTypes::GetDataSubarraysType& sub,
    int64_t total_size
) {
    ByteVec bytes_buf(static_cast<size_t>(total_size), 0);
    readBlobPartiallyFromMem(
        src_bytes.data(), bytes_buf.data(), 1, int(ary.m_dimensions.size()),
        ary.m_dimensions.data(), sub.m_starts.data(), sub.m_counts.data()
    );
    ArrayOfBoolean array_of;
    array_of.m_values.assign(bytes_buf.begin(), bytes_buf.end());
    ary.m_dimensions = sub.m_counts;
    ary.m_data.m_item = std::move(array_of);
}

} // namespace

namespace oes::eml::openkv {

//============================================================================

OpenKVDataArray::OpenKVDataArray(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

using cid_path_index_tuple_t = std::tuple<const std::string&, const std::string&, const std::string&>;
using c_cid_path_index_tuple_t = std::tuple<const std::string&, const std::string&, const std::string&>;
using ary_keys_indexes_t = std::tuple<std::vector<oes::core::Guid>, std::vector<std::string>, std::vector<int>>;

//! RESQML 2.0.1 arrays are stored under their external part, but reads may name their owner.
//! The owner's XML, not ary.obj_id, tells which external part: obj_id names one owner only.
class AryContainerResolver {
public:
    AryContainerResolver(SpaceReader& r, size_t bulk_size)
        : r_(r), bulk_size_(std::max<size_t>(bulk_size, 1))
    { }

    //! True for a RESQML 2.0.1 owner; external parts and EML 2.3 objects are their own container
    static bool needs_container_lookup(const EmlUri& uri) {
        return uri.isObj()
            && (uri.ml_ == EmlUri::eEML20 || uri.ml_ == EmlUri::eRESQML20)
            && !boost::ends_with(uri.obj_type_, "EpcExternalPartReference");
    }

    //! Fetches the XML of the RESQML 2.0.1 owners among uris not seen yet, in one query
    void prefetch(const std::vector<std::string>& uris) {
        std::vector<Guid> guids;
        std::vector<std::string> types;
        std::vector<int> indexes;
        for (const std::string& uri_str : uris) {
            const EmlUri uri = EmlUri::parse(uri_str);
            if (!needs_container_lookup(uri)) {
                continue;
            }
            const TypedUid key = stored_uid(uri);
            if (!owners_.try_emplace(key).second) {
                continue;
            }
            indexes.push_back(int(guids.size()));
            types.push_back(key.first);
            guids.push_back(key.second);
        }
        if (guids.empty()) {
            return;
        }
        for (BulkObj_t& obj : r_.bulk_get_obj(guids, types, indexes, bulk_size_)) {
            if (obj.found) {
                owners_[{ types.at(obj.idx), guids.at(obj.idx) }] = scan(std::move(obj.xml));
            }
        }
    }

    //! The guid the array is stored under; nil for a RESQML 2.0.1 owner not referencing path
    Guid container_of(const std::string& uri_str, const std::string& path) {
        const EmlUri uri = EmlUri::parse(uri_str);
        if (!needs_container_lookup(uri)) {
            return uri.guid_;
        }
        const TypedUid key = stored_uid(uri);
        auto it = owners_.find(key);
        if (it == owners_.end()) {
            prefetch({ uri_str });
            it = owners_.find(key);
        }
        const auto found = it->second.find(path);
        return (found == it->second.end()) ? Guid() : found->second;
    }

private:
    using PathToContainer = std::map<std::string, Guid>;

    static PathToContainer scan(std::string xml) {
        using oes::common::epc::FilePart;
        PathToContainer containers;
        auto [data, rc] = FilePart::scanData(std::move(xml), FilePart::eHdfs);
        if (rc != FilePart::eOK_DONE) {
            return containers;
        }
        for (auto& named_hdf : data.hdfs_) {
            std::string path = std::move(named_hdf.second.PathInHdfFile_);
            if (!path.empty() && path[0] == '/') {
                path.erase(0, 1);
            }
            const Guid proxy = Guid::parse(named_hdf.second.HdfProxy_.Uuid_);
            if (!path.empty() && proxy.isValid()) {
                containers.emplace(std::move(path), proxy);
            }
        }
        return containers;
    }

    SpaceReader& r_;
    const size_t bulk_size_;
    std::map<TypedUid, PathToContainer> owners_;
};

// Server policy, ETP allows both: the XML names the external part, and the owner may not be put yet
const char* const eml20_put_at_owner_msg = "RESQML 2.0.1 arrays are put at their EpcExternalPartReference";

//! Sorted by path, with the container each URI resolves to.
//! Entries whose container cannot be resolved are left out, their index added to unresolved.
ary_keys_indexes_t sorted_ary_keys_indexes(
    const std::vector<std::string>& paths, const std::vector<std::string>& uris,
    AryContainerResolver& containers, std::vector<int>& unresolved
) {
    containers.prefetch(uris);

    std::vector<int> order(paths.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&paths](int lhs, int rhs){
        return paths.at(lhs) < paths.at(rhs);
    });

    std::vector<oes::core::Guid> guids; guids.reserve(order.size());
    std::vector<std::string> sorted_paths; sorted_paths.reserve(order.size());
    std::vector<int> indexes; indexes.reserve(order.size());
    for (int i : order) {
        const Guid container = containers.container_of(uris.at(i), paths.at(i));
        if (!container.isValid()) {
            unresolved.push_back(i);
            continue;
        }
        guids.push_back(container);
        sorted_paths.push_back(paths.at(i));
        indexes.push_back(i);
    }
    return {std::move(guids), std::move(sorted_paths), std::move(indexes)};
}

struct ProcessBulkArrayHelper {
private:
    std::vector<std::string> client_ids_;
    std::vector<std::string> paths_;
    std::vector<std::string> uris_;

public:
    ProcessBulkArrayHelper() = default;
    
    size_t size() const { return client_ids_.size(); }
    
    cid_path_index_tuple_t emplace_back(const std::string& cid, const std::string& path_in_resource, const std::string& uri) {
        return std::tie(
            client_ids_.emplace_back(cid),
            paths_.emplace_back(path_in_resource),
            uris_.emplace_back(uri)
        );
    }
    
    c_cid_path_index_tuple_t c_at(size_t index) const {
        if (index < client_ids_.size()) {
            return std::tie(client_ids_.at(index), paths_.at(index), uris_.at(index));
        }
        throw std::runtime_error("ProcessBulkArrayHelper index out of scope");
    }
    
    std::vector<std::string>& client_ids() { return client_ids_; }
    std::vector<std::string>& paths() { return paths_; }

    ary_keys_indexes_t get_ary_keys_indexes(AryContainerResolver& containers, std::vector<int>& unresolved) const {
        return sorted_ary_keys_indexes(paths_, uris_, containers, unresolved);
    }
};

template<typename T>
void bytes_to_vector(DataArrayTypes::DataArray& dest_ary, std::vector<uint8_t>& source_bytes) {
    
    T ary_of;
    using vector_type = decltype(T::m_values);
    using value_type  = typename vector_type::value_type;
    auto& dest = ary_of.m_values;
    const size_t elem_size = sizeof(value_type);
    if (source_bytes.size() % elem_size != 0) {
        throw std::runtime_error("Byte size not aligned with target type size");
    }

    size_t element_count = source_bytes.size() / elem_size;
    dest.resize(element_count);

    std::memcpy(dest.data(), source_bytes.data(), source_bytes.size());
    
    source_bytes.clear();
    source_bytes.shrink_to_fit();

    dest_ary.m_data.m_item = std::move(ary_of);
}

template<>
void bytes_to_vector<ArrayOfBoolean>(DataArrayTypes::DataArray& dest_ary, std::vector<uint8_t>& source_bytes) {
    ArrayOfBoolean ary_of;
    std::vector<bool>& dest = ary_of.m_values;
    dest.resize(source_bytes.size());

    dest.assign(source_bytes.begin(), source_bytes.end());

    source_bytes.clear();
    source_bytes.shrink_to_fit();

    dest_ary.m_data.m_item = std::move(ary_of);
}

template<>
void bytes_to_vector<ByteVec>(DataArrayTypes::DataArray& dest_ary, std::vector<uint8_t>& source_bytes) {
    dest_ary.m_data.m_item = std::move(source_bytes);
}


using ReplyType = etp12::MultiPartReply<etp12::Protocol::IDataArray::GetDataArrays_res_t>;
void bulk_get_arr_value(
    std::vector<SqlAry>& bulk_arrays,
    const ProcessBulkArrayHelper& helper,
    SpaceReader& r,
    ReplyType& reply,
    const size_t bulk_size
    ) {

    const auto cbeg_it = bulk_arrays.begin();
    const auto cend_it = bulk_arrays.end();

    auto beg_it = bulk_arrays.begin();
    auto next_it = beg_it;
    auto end_it = beg_it + 1;
    
    // bool new_batch {true};
    size_t current_bytes {0};

    auto get_next_bytes = [&]() -> size_t {
        if (next_it == cend_it) {
            return 0;
        }

        auto& sql_ary = *next_it;
        auto tuple = helper.c_at(sql_ary.bulk_idx); // cid, path, uri
            
        const auto& cid = std::get<0>(tuple);
        const auto& path = std::get<1>(tuple);

        std::vector<int64_t> dims;
        if (!sql_ary.get_dimensions(dims)) {
            reply.addError(cid, EINVALID_STATE, fmt::format(
                "Unexpected array rank = {}: {}", sql_ary.rank_, path
            ));
        }

        const size_t ary_bytes = sql_ary.byte_size();
        if (ary_bytes >= reply.max_bytes_) {
            reply.addError(cid, ELIMIT_EXCEEDED, fmt::format(
                "Oversize array ({} > {} bytes): {}", ary_bytes, reply.max_bytes_, path
            ));
        }

        size_t temp_size = ary_bytes + 8;
        temp_size += cid.size() + 8;
        temp_size += dims.size()*8 + 8;

        return temp_size;
    };

    while (beg_it != cend_it) {
        const bool eoit = next_it == cend_it;
        const auto next_bytes = get_next_bytes();

        if (current_bytes + next_bytes > reply.max_bytes_ || eoit) {
            
            std::vector<eml_int64> ary_indexes;
            std::vector<int> req_indexes;

            for(auto it = beg_it; it != end_it; ++it) {
                
                ary_indexes.push_back(it->ary_id_);

                const size_t dest_ary_index = std::distance(cbeg_it, it);
                req_indexes.push_back(dest_ary_index); //? index???
            }

            auto res_arr = r.bulk_get_ary_val(ary_indexes, req_indexes, bulk_size);

            
            for (auto& res : res_arr) {
                // const auto dest_ary_index = res.ary_sql_idx;
                auto& sql_ary = bulk_arrays.at(res.ary_sql_idx);

                auto tuple = helper.c_at(sql_ary.bulk_idx); // cid, path, uri
            
                const auto& cid = std::get<0>(tuple);
                const auto& path = std::get<1>(tuple);
                //? const auto& uri = std::get<2>(tuple);

                if (!res.err.empty()) {
                    reply.addError(
                    cid, EINVALID_OBJECT,
                        fmt::format("Cannot read sub-array: {}", res.err)
                    );

                    continue;
                }

                auto it_ary = reply.addMapPart(cid);
                DataArrayTypes::DataArray& ary = it_ary.first->second;

                // Get dimentions
                sql_ary.get_dimensions(ary.m_dimensions);

                const auto ary_bytes_size = sql_ary.byte_size();
                auto size = ary_bytes_size + 8;
                size += cid.size() + 8;
                size += ary.m_dimensions.size()*8 + 8;

                reply.addBytes(size);

                assert(ary_bytes_size == res.bytes.size());

                switch (sql_ary.type_) {
                    case hdf::HdfAryType::eAryOfInt:
                        bytes_to_vector<ArrayOfInt>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfDouble:
                        bytes_to_vector<ArrayOfDouble>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfFloat:
                        bytes_to_vector<ArrayOfFloat>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfBytes:
                        bytes_to_vector<ByteVec>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfBoolean:
                        bytes_to_vector<ArrayOfBoolean>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfLong:
                        bytes_to_vector<ArrayOfLong>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfString:
                        [[fallthrough]];
                    case hdf::HdfAryType::eAryOfVoid:
                        [[fallthrough]];
                    default:
                        OES_ERROR("Unexpected array type = {}: {}", sql_ary.type_, path);
                        reply.eraseMapPart(cid, ary_bytes_size);
                        continue;
                }
            }

            if (!eoit) {
                reply.maybeProcess(next_bytes, true);
            }

            current_bytes = 0;
            beg_it = end_it;
            next_it = beg_it;
            if (end_it != cend_it) {
                end_it = beg_it + 1;
            }
        } else {
            current_bytes += next_bytes;
            next_it++;
            if (end_it < cend_it) {
                end_it++;  
            }
        }
    }
}


void bulk_get_arr_value_single_blob(
    std::vector<SqlAry>& bulk_arrays,
    const ProcessBulkArrayHelper& helper,
    SpaceReader& r,
    ReplyType& reply,
    const size_t bulk_size
    ) {

    
    const auto cbeg_it = bulk_arrays.begin();
    const auto cend_it = bulk_arrays.end();

    auto beg_it = bulk_arrays.begin();
    auto next_it = beg_it;
    auto end_it = beg_it + 1;
    
    size_t current_bytes {0};

    auto get_next_bytes = [&]() -> size_t {
        if (next_it == cend_it) {
            return 0;
        }

        auto& sql_ary = *next_it;
        auto tuple = helper.c_at(sql_ary.bulk_idx); // cid, path, uri
            
        const auto& cid = std::get<0>(tuple);
        const auto& path = std::get<1>(tuple);
        //!const auto& uri = std::get<2>(tuple);

        std::vector<int64_t> dims;
        if (!sql_ary.get_dimensions(dims)) {
            reply.addError(cid, EINVALID_STATE, fmt::format(
                "Unexpected array rank = {}: {}", sql_ary.rank_, path
            ));
        }

        const size_t ary_bytes = sql_ary.byte_size();
        if (ary_bytes >= reply.max_bytes_) {
            reply.addError(cid, ELIMIT_EXCEEDED, fmt::format(
                "Oversize array ({} > {} bytes): {}", ary_bytes, reply.max_bytes_, path
            ));
        }

        size_t temp_size = ary_bytes + 8;
        temp_size += cid.size() + 8;
        temp_size += dims.size()*8 + 8;

        return temp_size;
    };

    while (beg_it != cend_it) {

        const bool eoit = next_it == cend_it;
        const auto next_bytes = get_next_bytes();

        if (current_bytes + next_bytes > reply.max_bytes_ || eoit) {
            //todo close this batch
            // in row -> bulk_sel_ary_val_
            
            std::vector<eml_int64> ary_indexes;
            std::vector<int> req_indexes;

            for(auto it = beg_it; it != end_it; ++it) {
                
                ary_indexes.push_back(it->ary_id_);

                const size_t dest_ary_index = std::distance(cbeg_it, it);
                req_indexes.push_back(dest_ary_index); //? index???
            }

            std::vector<int> zeros(ary_indexes.size(), 0);
            auto res_arr = r.bulk_get_bin_val(ary_indexes, zeros, req_indexes, bulk_size);
            // auto res_arr = r.bulk_get_ary_val(ary_indexes, req_indexes, BULK_SIZE);

            // A missing bin row is a sparse (all-zero) array, as in SpaceReader::read_ary().
            std::vector<bool> has_blob(bulk_arrays.size(), false);
            for (const auto& res : res_arr) {
                has_blob.at(res.ary_sql_idx) = true;
            }
            for (int idx : req_indexes) {
                if (!has_blob.at(idx)) {
                    BlobData_t& blob = res_arr.emplace_back();
                    blob.ary_sql_idx = idx;
                    blob.bytes.assign(bulk_arrays.at(idx).byte_size(), 0);
                }
            }
            
            for (auto& res : res_arr) {
                // const auto dest_ary_index = res.ary_sql_idx;
                auto& sql_ary = bulk_arrays.at(res.ary_sql_idx);

                auto tuple = helper.c_at(sql_ary.bulk_idx); // cid, path, uri
            
                const auto& cid = std::get<0>(tuple);
                const auto& path = std::get<1>(tuple);
                //? const auto& uri = std::get<2>(tuple);

                auto it_ary = reply.addMapPart(cid);
                DataArrayTypes::DataArray& ary = it_ary.first->second;

                // Get dimentions
                sql_ary.get_dimensions(ary.m_dimensions);

                const auto ary_bytes_size = sql_ary.byte_size();
                auto size = ary_bytes_size + 8;
                size += cid.size() + 8;
                size += ary.m_dimensions.size()*8 + 8;

                reply.addBytes(size);

                assert(ary_bytes_size == res.bytes.size());

                switch (sql_ary.type_) {
                    case hdf::HdfAryType::eAryOfInt:
                        bytes_to_vector<ArrayOfInt>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfDouble:
                        bytes_to_vector<ArrayOfDouble>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfFloat:
                        bytes_to_vector<ArrayOfFloat>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfBytes:
                        bytes_to_vector<ByteVec>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfBoolean:
                        bytes_to_vector<ArrayOfBoolean>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfLong:
                        bytes_to_vector<ArrayOfLong>(ary, res.bytes);
                        break;
                    case hdf::HdfAryType::eAryOfString:
                        [[fallthrough]];
                    case hdf::HdfAryType::eAryOfVoid:
                        [[fallthrough]];
                    default:
                        OES_ERROR("Unexpected array type = {}: {}", sql_ary.type_, path);
                        reply.eraseMapPart(cid, ary_bytes_size);
                        continue;
                }
            }

            if (!eoit) {
                reply.maybeProcess(next_bytes, true);
            }

            current_bytes = 0;
            beg_it = end_it;
            next_it = beg_it;
            if (end_it != cend_it) {
                end_it = beg_it + 1;
            }
        } else {
            current_bytes += next_bytes;
            next_it++;
            if (end_it < cend_it) {
                end_it++;   
            }
        }
    }
}

void bulk_get_arr_value_mult_blob(
    std::vector<SqlAry>& bulk_arrays,
    const ProcessBulkArrayHelper& helper,
    SpaceReader& r,
    ReplyType& reply,
    const size_t bulk_size
    //! const ArrayValueBulkType type
    ) {

    const auto cbeg_it = bulk_arrays.begin();
    const auto cend_it = bulk_arrays.end();

    auto beg_it = bulk_arrays.begin();
    auto next_it = beg_it;
    auto end_it = beg_it + 1;

    size_t current_bytes {0};

    auto get_next_bytes = [&]() -> size_t {
        if (next_it == cend_it) {
            return 0;
        }

        auto& sql_ary = *next_it;
        auto tuple = helper.c_at(sql_ary.bulk_idx); // cid, path, uri

        const auto& cid = std::get<0>(tuple);
        const auto& path = std::get<1>(tuple);
        //!const auto& uri = std::get<2>(tuple);

        std::vector<int64_t> dims;
        if (!sql_ary.get_dimensions(dims)) {
            reply.addError(cid, EINVALID_STATE, fmt::format(
                "Unexpected array rank = {}: {}", sql_ary.rank_, path
            ));
        }

        const size_t ary_bytes = sql_ary.byte_size();
        if (ary_bytes >= reply.max_bytes_) {
            reply.addError(cid, ELIMIT_EXCEEDED, fmt::format(
                "Oversize array ({} > {} bytes): {}", ary_bytes, reply.max_bytes_, path
            ));
        }

        size_t temp_size = ary_bytes + 8;
        temp_size += cid.size() + 8;
        temp_size += dims.size()*8 + 8;

        return temp_size;
    };

    while (beg_it != cend_it) {
        const bool eoit = next_it == cend_it;
        const auto next_bytes = get_next_bytes();

        if (current_bytes + next_bytes > reply.max_bytes_ || eoit) {
            // Expand every (array, chunk) pair in this batch into flat vectors
            std::vector<eml_int64> all_ary_ids;
            std::vector<int> all_chunk_idxs;
            std::vector<int> all_req_idxs;

            for (auto jt = beg_it; jt != end_it; ++jt) {
                const int dest_ary_index = static_cast<int>(std::distance(cbeg_it, jt));
                const int n_chunks = jt->chunk_count();
                for (int c = 0; c < n_chunks; ++c) {
                    all_ary_ids.push_back(jt->ary_id_);
                    all_chunk_idxs.push_back(c);
                    all_req_idxs.push_back(dest_ary_index);
                }
            }

            auto chunk_results = r.bulk_get_bin_val(all_ary_ids, all_chunk_idxs, all_req_idxs, bulk_size);

            // Group returned chunks by dest_ary_index
            std::unordered_map<int, std::vector<BlobData_t*>> chunks_by_ary;
            for (auto& res : chunk_results) {
                chunks_by_ary[res.ary_sql_idx].push_back(&res);
            }
            for (auto& [idx, chunks] : chunks_by_ary) {
                std::sort(chunks.begin(), chunks.end(), [](const BlobData_t* a, const BlobData_t* b) {
                    return a->chunk_idx < b->chunk_idx;
                });
            }

            for (auto jt = beg_it; jt != end_it; ++jt) {
                auto& sql_ary = *jt;
                auto tuple = helper.c_at(sql_ary.bulk_idx); // cid, path, uri

                const auto& cid = std::get<0>(tuple);
                const auto& path = std::get<1>(tuple);

                const size_t ary_bytes = sql_ary.byte_size();

                auto it_ary = reply.addMapPart(cid);
                DataArrayTypes::DataArray& ary = it_ary.first->second;
                sql_ary.get_dimensions(ary.m_dimensions);

                auto size = ary_bytes + 8;
                size += cid.size() + 8;
                size += ary.m_dimensions.size()*8 + 8;
                reply.addBytes(size);

                // Reassemble: start with all-zero buffer (handles sparse/missing chunks)
                std::vector<uint8_t> complete_data(ary_bytes, 0);

                const int dest_ary_index = static_cast<int>(std::distance(cbeg_it, jt));
                auto ci = chunks_by_ary.find(dest_ary_index);
                if (ci != chunks_by_ary.end()) {
                    for (const BlobData_t* chunk : ci->second) {
                        const size_t offset = static_cast<size_t>(chunk->chunk_idx) * sql_ary.chunk_len_;
                        const size_t copy_size = std::min(chunk->bytes.size(), ary_bytes - offset);
                        std::memcpy(complete_data.data() + offset, chunk->bytes.data(), copy_size);
                    }
                }

                switch (sql_ary.type_) {
                    case hdf::HdfAryType::eAryOfInt:
                        bytes_to_vector<ArrayOfInt>(ary, complete_data);
                        break;
                    case hdf::HdfAryType::eAryOfDouble:
                        bytes_to_vector<ArrayOfDouble>(ary, complete_data);
                        break;
                    case hdf::HdfAryType::eAryOfFloat:
                        bytes_to_vector<ArrayOfFloat>(ary, complete_data);
                        break;
                    case hdf::HdfAryType::eAryOfBytes:
                        bytes_to_vector<ByteVec>(ary, complete_data);
                        break;
                    case hdf::HdfAryType::eAryOfBoolean:
                        bytes_to_vector<ArrayOfBoolean>(ary, complete_data);
                        break;
                    case hdf::HdfAryType::eAryOfLong:
                        bytes_to_vector<ArrayOfLong>(ary, complete_data);
                        break;
                    case hdf::HdfAryType::eAryOfString:
                        [[fallthrough]];
                    case hdf::HdfAryType::eAryOfVoid:
                        [[fallthrough]];
                    default:
                        OES_ERROR("Unexpected array type = {}: {}", sql_ary.type_, path);
                        reply.eraseMapPart(cid, ary_bytes);
                        continue;
                }
            }

            if (!eoit) {
                reply.maybeProcess(next_bytes, true);
            }

            current_bytes = 0;
            beg_it = end_it;
            next_it = beg_it;
            if (end_it != cend_it) {
                end_it = beg_it + 1;
            }
        } else {
            current_bytes += next_bytes;
            next_it++;
            if (end_it < cend_it) {
                end_it++;
            }
        }
    }
}

void OpenKVDataArray::GetDataArrays( // messageTypeId 2
    IRequest& ireq, GetDataArrays_req_t req,
    EtpResponseHandler<GetDataArrays_res_t> process_cb // messageTypeId 1
) try {
    char * use_bulk_p = std::getenv("USE_BULK_READ");
    std::string use_bulk;
    
    if (use_bulk_p) {
        use_bulk = std::string(use_bulk_p);
    } else {
        use_bulk = "true";
    }
    if (use_bulk == "true") {
        return GetDataArraysBULK(ireq, req, process_cb);
    }
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataArrays.size", req.m_dataArrays.size()}});
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_GET_ARRAYS_ERROR);
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataArrays.empty()) {
        span.setError("Empty URI list");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_GET_ARRAYS_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    struct ArrayEntry {
        const std::string* uri;  //cppcheck-suppress unusedStructMember
        const std::string* key;
        const std::string* path;
    };
    using ArrayEntries = std::vector<ArrayEntry>;
    std::map<std::string, ArrayEntries> init_space_arrays;

    for (auto& entry : req.m_dataArrays) {
        const std::string& cid = entry.first; // client ID
        AryIdent& ary_ident = entry.second; // ETP URI

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_ident, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        init_space_arrays[uri.dataspace_].emplace_back(
            ArrayEntry{ &ary_ident.m_uri, &cid, &ary_ident.m_pathInResource }
        );
    }

    // Follow dataspace references if needed
    std::map<std::string, ArrayEntries> per_space_arrays;
    std::map<std::string, std::string> initial_uris;
    for (auto& entry : init_space_arrays) {
        ArrayEntries& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);

        ErrorInfo err;
        if(!space.isValid()){
            err.m_code = ENOT_FOUND;
            err.m_message = fmt::format("Dataspace not found: {}", entry.first);
        }

        // The dataspace that matters is the one of the initial URI
        if (!space.isValid() || !checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }


        // Check that there is at least one row in admin.space_rel that has the dataspace as origin
        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            for(const ArrayEntry& nk : nks){
                const std::string& i_uri= *nk.uri;
                EmlUri uri = reader.containing_dataspace_uri(EmlUri::parse(i_uri));
                if(uri.to_string()!=i_uri){
                    const_cast<std::string&>(*nk.uri) = uri.to_string();
                    per_space_arrays[uri.dataspace_].emplace_back(nk);
                }else{
                    per_space_arrays[space.uid_].emplace_back(nk);
                }
            }
        }else{
            per_space_arrays[space.uid_] = std::move(nks);
        }
    }

    for (auto& entry : per_space_arrays) {
        ArrayEntries& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        auto reader = repo_->getSpaceReader(space, err);
        if (!reader) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        oes::eml::openkv::SpaceReader& r = *reader;

        // Sort by path (for better page-cache-locality)
        if (nks.size() > 1) {
            std::sort(nks.begin(), nks.end(), [](const auto& lhs, const auto& rhs) {
                return *lhs.path < *rhs.path;
            });
        }

        AryContainerResolver containers(r, 1);
        for (const ArrayEntry& nk : nks) {
            const std::string& path = *nk.path;
            assert(path[0] != '/'); // see above
            const Guid container = containers.container_of(*nk.uri, path);
            SqlAry sql_ary = container.isValid() ? r.select_ary(container, path) : SqlAry();

            if (!sql_ary.valid()) {
                reply.addError(*nk.key, ENOT_FOUND, path);
                continue;
            }

            std::vector<int64_t> dims;
            if (!sql_ary.get_dimensions(dims)) {
                reply.addError(*nk.key, EINVALID_STATE, fmt::format(
                    "Unexpected array rank = {}: {}", sql_ary.rank_, path
                ));
                continue;
            }

            const size_t ary_bytes = sql_ary.byte_size();
            if (ary_bytes >= reply.max_bytes_) {
                reply.addError(*nk.key, ELIMIT_EXCEEDED, fmt::format(
                    "Oversize array ({} > {} bytes): {}", ary_bytes, reply.max_bytes_, path
                ));
                continue;
            }

            size_t size = ary_bytes + 8;
            size += nk.key->size() + 8 ;
            size += dims.size()*8 + 8 ;
            reply.maybeProcess(size);

            auto it_ary = reply.addMapPart(*nk.key);
            DataArrayTypes::DataArray& ary = it_ary.first->second;
            ary.m_dimensions = std::move(dims);
            reply.addBytes(size);

            switch (sql_ary.type_) { // see EpcConv.cpp's acceptHdf5ObjInfo lambda
            case hdf::HdfAryType::eAryOfInt:     readDataArray<ArrayOfInt>    (r, sql_ary, ary); break;
            case hdf::HdfAryType::eAryOfDouble:  readDataArray<ArrayOfDouble> (r, sql_ary, ary); break;
            case hdf::HdfAryType::eAryOfFloat:   readDataArray<ArrayOfFloat>  (r, sql_ary, ary); break;
            case hdf::HdfAryType::eAryOfBytes:   readDataArray<ByteVec>       (r, sql_ary, ary); break;
            case hdf::HdfAryType::eAryOfBoolean: readDataArray<ArrayOfBoolean>(r, sql_ary, ary); break;
            case hdf::HdfAryType::eAryOfLong:    readDataArray<ArrayOfLong>   (r, sql_ary, ary); break;
            case hdf::HdfAryType::eAryOfString:  [[fallthrough]]; // Not used by RESQML
            case hdf::HdfAryType::eAryOfVoid:    [[fallthrough]]; // Impossible
            default: // Not supported
                OES_ERROR("Unexpected array type = {}: {}", sql_ary.type_, path);
                reply.eraseMapPart(it_ary.first, ary_bytes);
                continue;
            }
        }
    }
    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::ARRAY_OPS_COUNTER_GET_ARRAYS_SUCCESS,oes::core::metrics::ARRAY_LATENCY_GET_ARRAYS,start);
    reply.finalProcess(); 
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_GET_ARRAYS_ERROR);
    etp12::handleException(process_cb);
}


void OpenKVDataArray::GetDataArraysBULK( // messageTypeId 2
    IRequest& ireq, GetDataArrays_req_t req,
    EtpResponseHandler<GetDataArrays_res_t> process_cb // messageTypeId 1
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataArrays.size", req.m_dataArrays.size()}});
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    const size_t BULK_SIZE = session->pg_bulk_size_;

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataArrays.empty()) {
        span.setError("Empty URI list");
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    std::map<std::string, ProcessBulkArrayHelper> init_space_arrays;

    for (auto& entry : req.m_dataArrays) {
        const std::string& cid = entry.first; // client ID
        AryIdent& ary_ident = entry.second; // ETP URI

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_ident, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        const auto& [it, _] = init_space_arrays.try_emplace(
            uri.dataspace_, ProcessBulkArrayHelper()
        );

        auto& [ds, h] = *it;
        h.emplace_back(cid, ary_ident.m_pathInResource, ary_ident.m_uri);
    }

    // Follow dataspace references if needed
    std::map<std::string, ProcessBulkArrayHelper> per_space_arrays;
    for (auto& entry : init_space_arrays) {
        ProcessBulkArrayHelper& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);

        ErrorInfo err;
        if(!space.isValid()){
            err.m_code = ENOT_FOUND;
            err.m_message = fmt::format("Dataspace not found: {}", entry.first);
        }

        // The dataspace that matters is the one of the initial URI
        if (!space.isValid() || !checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const auto& nk : nks.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const auto& nk : nks.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }


        // Check that there is at least one row in admin.space_rel that has the dataspace as origin
        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            for(size_t i {0}; i< nks.size(); ++i){
                auto tup = nks.c_at(i);
                const auto& cid = std::get<0>(tup);
                const auto& path = std::get<1>(tup);
                const auto& i_uri = std::get<2>(tup); // can belong to another dataspace

                
                EmlUri source_uri = reader.containing_dataspace_uri(EmlUri::parse(i_uri)); //TODO optimize it
                const auto& [it, _] = per_space_arrays.try_emplace(source_uri.dataspace_, ProcessBulkArrayHelper());
                auto& [ds, h] = *it;

                if(source_uri.to_string() != i_uri){

                    h.emplace_back(cid, path, source_uri.to_string());
                }else{
                    h.emplace_back(cid, path, i_uri);
                }
            }
        }else{
            per_space_arrays[space.uid_] = std::move(nks);
        }
    }

    for (auto& entry : per_space_arrays) {
        auto& bulk_helper = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const auto& nk : bulk_helper.client_ids()) {
                reply.addError(nk, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        auto reader = repo_->getSpaceReader(space, err);
        if (!reader) {
            for (const auto& nk : bulk_helper.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        oes::eml::openkv::SpaceReader& r = *reader;

        // Process arrays in bulk
        AryContainerResolver containers(r, BULK_SIZE);
        std::vector<int> unresolved;
        auto [epc_guids, paths, indexes] = bulk_helper.get_ary_keys_indexes(containers, unresolved);
        for (int idx : unresolved) {
            auto tuple = bulk_helper.c_at(size_t(idx));
            reply.addError(std::get<0>(tuple), ENOT_FOUND, std::get<1>(tuple));
        }
        auto bulk_arrays = r.bulk_get_ary(epc_guids, paths, indexes, BULK_SIZE);

        std::vector<SqlAry> sql_arrays_in_row;
        std::vector<SqlAry> sql_arrays_single_blob;
        std::vector<SqlAry> sql_arrays_mult_blob;

        for (const auto& sql_ary : bulk_arrays) {
            if (!sql_ary.valid()) {
                const size_t idx = sql_ary.bulk_idx;
                auto tuple = bulk_helper.c_at(idx); // cid, path, uri
            
                const auto& cid = std::get<0>(tuple);
                const auto& path = std::get<1>(tuple);
                
                reply.addError(cid, ENOT_FOUND, path);
                continue;
            }

            {
                auto tuple = bulk_helper.c_at(sql_ary.bulk_idx); // cid, path, uri
                const auto& cid = std::get<0>(tuple);
                const auto& path = std::get<1>(tuple);

                std::vector<int64_t> dims;
                if (!sql_ary.get_dimensions(dims)) {
                    reply.addError(cid, EINVALID_STATE, fmt::format(
                        "Unexpected array rank = {}: {}", sql_ary.rank_, path
                    ));
                    continue;
                }

                const size_t ary_bytes = sql_ary.byte_size();
                if (ary_bytes >= reply.max_bytes_) {
                    reply.addError(cid, ELIMIT_EXCEEDED, fmt::format(
                        "Oversize array ({} > {} bytes): {}", ary_bytes, reply.max_bytes_, path
                    ));
                    continue;
                }
            }

            if (sql_ary.is_in_row()) {
                sql_arrays_in_row.push_back(sql_ary);
            } else if (sql_ary.is_single_blob()) {
                sql_arrays_single_blob.push_back(sql_ary);
            } else {
                sql_arrays_mult_blob.push_back(sql_ary);
            }
        }

        bulk_get_arr_value(sql_arrays_in_row, bulk_helper, r, reply, BULK_SIZE);
        bulk_get_arr_value_single_blob(sql_arrays_single_blob, bulk_helper, r, reply, BULK_SIZE);
        bulk_get_arr_value_mult_blob(sql_arrays_mult_blob, bulk_helper, r, reply, BULK_SIZE);
    }

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

struct ProcessBulkSubArrayHelper {
private:
    std::vector<std::string> client_ids_;
    std::vector<std::string> paths_;
    std::vector<std::string> uris_;
    std::vector<DataArrayTypes::GetDataSubarraysType*> sub_requests_;

public:
    ProcessBulkSubArrayHelper() = default;
    
    size_t size() const { return client_ids_.size(); }
    
    void emplace_back(const std::string& cid, const std::string& path_in_resource, 
                        const std::string& uri, DataArrayTypes::GetDataSubarraysType* sub_req) {
        client_ids_.emplace_back(cid);
        paths_.emplace_back(path_in_resource);
        uris_.emplace_back(uri);
        sub_requests_.emplace_back(sub_req);
    }
    
    std::tuple<const std::string&, const std::string&, const std::string&, DataArrayTypes::GetDataSubarraysType*> 
    c_at(size_t index) const {
        if (index < client_ids_.size()) {
            return std::tie(client_ids_.at(index), paths_.at(index), uris_.at(index), sub_requests_.at(index));
        }
        throw std::runtime_error("ProcessBulkSubArrayHelper index out of scope");
    }
    
    std::vector<std::string>& client_ids() { return client_ids_; }
    std::vector<std::string>& paths() { return paths_; }

    ary_keys_indexes_t get_ary_keys_indexes(AryContainerResolver& containers, std::vector<int>& unresolved) const {
        return sorted_ary_keys_indexes(paths_, uris_, containers, unresolved);
    }
};

//============================================================================
// Bulk helpers for GetDataSubarrays

struct SqlSubAry {
    SqlAry sql_ary;
    DataArrayTypes::GetDataSubarraysType* sub = nullptr;
    int64_t total_size = 0;
    size_t sub_byte_size = 0;
};

using SubReplyType = etp12::MultiPartReply<etp12::Protocol::IDataArray::GetDataSubarrays_res_t>;

static void apply_subarray_result(
    SubReplyType& reply,
    const SqlSubAry& sub_ary,
    const ProcessBulkSubArrayHelper& helper,
    const std::vector<uint8_t>& src_bytes
) {
    auto tuple = helper.c_at(sub_ary.sql_ary.bulk_idx);
    const auto& cid  = std::get<0>(tuple);
    const auto& path = std::get<1>(tuple);

    const size_t size = sub_ary.sub_byte_size + 8 + cid.size() + 8;
    reply.maybeProcess(size);

    auto it_ary = reply.addMapPart(cid);
    DataArrayTypes::DataArray& ary = it_ary.first->second;
    reply.addBytes(size);
    sub_ary.sql_ary.get_dimensions(ary.m_dimensions);

    switch (sub_ary.sql_ary.type_) {
    case hdf::HdfAryType::eAryOfInt:
        readDataSubarrayFromMem<ArrayOfInt>    (src_bytes, ary, *sub_ary.sub, sub_ary.total_size); break;
    case hdf::HdfAryType::eAryOfDouble:
        readDataSubarrayFromMem<ArrayOfDouble> (src_bytes, ary, *sub_ary.sub, sub_ary.total_size); break;
    case hdf::HdfAryType::eAryOfFloat:
        readDataSubarrayFromMem<ArrayOfFloat>  (src_bytes, ary, *sub_ary.sub, sub_ary.total_size); break;
    case hdf::HdfAryType::eAryOfBytes:
        readDataSubarrayFromMem<ByteVec>       (src_bytes, ary, *sub_ary.sub, sub_ary.total_size); break;
    case hdf::HdfAryType::eAryOfBoolean:
        readDataSubarrayFromMem<ArrayOfBoolean>(src_bytes, ary, *sub_ary.sub, sub_ary.total_size); break;
    case hdf::HdfAryType::eAryOfLong:
        readDataSubarrayFromMem<ArrayOfLong>   (src_bytes, ary, *sub_ary.sub, sub_ary.total_size); break;
    case hdf::HdfAryType::eAryOfString: [[fallthrough]];
    case hdf::HdfAryType::eAryOfVoid:   [[fallthrough]];
    default:
        OES_ERROR("Unexpected array type = {}: {}", sub_ary.sql_ary.type_, path);
        reply.eraseMapPart(cid, sub_ary.sub_byte_size);
        return;
    }
}

void bulk_get_subarray_in_row(
    std::vector<SqlSubAry>& sub_arrays,
    const ProcessBulkSubArrayHelper& helper,
    SpaceReader& r,
    SubReplyType& reply,
    const size_t bulk_size
) {
    if (sub_arrays.empty()) return;

    std::vector<eml_int64> ary_ids;
    std::vector<int> req_idxs;
    ary_ids.reserve(sub_arrays.size());
    req_idxs.reserve(sub_arrays.size());
    for (int i = 0; i < static_cast<int>(sub_arrays.size()); ++i) {
        ary_ids.push_back(sub_arrays[i].sql_ary.ary_id_);
        req_idxs.push_back(i);
    }

    auto results = r.bulk_get_ary_val(ary_ids, req_idxs, bulk_size);

    for (auto& res : results) {
        if (!res.err.empty()) {
            auto tuple = helper.c_at(sub_arrays.at(res.ary_sql_idx).sql_ary.bulk_idx);
            const auto& cid  = std::get<0>(tuple);
            //!const auto& path = std::get<1>(tuple);

            reply.addError(
            cid, EINVALID_OBJECT,
                fmt::format("Cannot read sub-array: {}", res.err)
            );

            continue;
        }
        apply_subarray_result(reply, sub_arrays.at(res.ary_sql_idx), helper, res.bytes);
    }
}

void bulk_get_subarray_single_blob(
    std::vector<SqlSubAry>& sub_arrays,
    const ProcessBulkSubArrayHelper& helper,
    SpaceReader& r,
    SubReplyType& reply,
    const size_t bulk_size
) {
    if (sub_arrays.empty()) return;

    std::vector<eml_int64> ary_ids;
    std::vector<int> bin_idxs; // always 0 — single blob arrays have exactly one chunk
    std::vector<int> req_idxs;
    ary_ids.reserve(sub_arrays.size());
    bin_idxs.reserve(sub_arrays.size());
    req_idxs.reserve(sub_arrays.size());
    for (int i = 0; i < static_cast<int>(sub_arrays.size()); ++i) {
        ary_ids.push_back(sub_arrays[i].sql_ary.ary_id_);
        bin_idxs.push_back(0);
        req_idxs.push_back(i);
    }

    auto results = r.bulk_get_bin_val(ary_ids, bin_idxs, req_idxs, bulk_size);

    // A missing bin row is a sparse (all-zero) array, as in SpaceReader::read_ary().
    std::vector<bool> has_blob(sub_arrays.size(), false);
    for (const auto& res : results) {
        has_blob.at(res.ary_sql_idx) = true;
    }
    for (int i = 0; i < static_cast<int>(sub_arrays.size()); ++i) {
        if (!has_blob.at(i)) {
            BlobData_t& blob = results.emplace_back();
            blob.ary_sql_idx = i;
            blob.bytes.assign(sub_arrays[i].sql_ary.byte_size(), 0);
        }
    }

    for (auto& res : results) {
        apply_subarray_result(reply, sub_arrays.at(res.ary_sql_idx), helper, res.bytes);
    }
}

void bulk_get_subarray_mult_blob(
    std::vector<SqlSubAry>& sub_arrays,
    const ProcessBulkSubArrayHelper& helper,
    SpaceReader& r,
    SubReplyType& reply,
    const size_t bulk_size
) {
    if (sub_arrays.empty()) return;

    // Expand every (array, chunk) pair into flat vectors for a single bulk fetch
    std::vector<eml_int64> all_ary_ids;
    std::vector<int> all_chunk_idxs;
    std::vector<int> all_req_idxs;
    for (int i = 0; i < static_cast<int>(sub_arrays.size()); ++i) {
        const int n_chunks = sub_arrays[i].sql_ary.chunk_count();
        for (int c = 0; c < n_chunks; ++c) {
            all_ary_ids.push_back(sub_arrays[i].sql_ary.ary_id_);
            all_chunk_idxs.push_back(c);
            all_req_idxs.push_back(i);
        }
    }

    if (all_ary_ids.empty()) return;

    auto chunk_results = r.bulk_get_bin_val(all_ary_ids, all_chunk_idxs, all_req_idxs, bulk_size);

    // Group chunks by req index, sorted by chunk_idx
    std::unordered_map<int, std::vector<BlobData_t*>> chunks_by_ary;
    for (auto& chunk : chunk_results) {
        chunks_by_ary[chunk.ary_sql_idx].push_back(&chunk);
    }
    for (auto& [idx, chunks] : chunks_by_ary) {
        std::sort(chunks.begin(), chunks.end(), [](const BlobData_t* a, const BlobData_t* b) {
            return a->chunk_idx < b->chunk_idx;
        });
    }

    // Reassemble full array data per sub_ary, then apply in-memory subarray extraction
    for (int i = 0; i < static_cast<int>(sub_arrays.size()); ++i) {
        auto& sub_ary = sub_arrays[i];
        const size_t ary_bytes = sub_ary.sql_ary.byte_size();
        std::vector<uint8_t> full_data(ary_bytes, 0);

        auto ci = chunks_by_ary.find(i);
        if (ci != chunks_by_ary.end()) {
            for (const BlobData_t* chunk : ci->second) {
                const size_t offset = static_cast<size_t>(chunk->chunk_idx) * sub_ary.sql_ary.chunk_len_;
                const size_t copy_size = std::min(chunk->bytes.size(), ary_bytes - offset);
                std::memcpy(full_data.data() + offset, chunk->bytes.data(), copy_size);
            }
        }
        apply_subarray_result(reply, sub_ary, helper, full_data);
    }
}

void OpenKVDataArray::GetDataSubarrays( // messageTypeId 3
    IRequest& ireq, GetDataSubarrays_req_t req,
    EtpResponseHandler<GetDataSubarrays_res_t> process_cb // messageTypeId 8
) try {
    char * use_bulk_p = std::getenv("USE_BULK_READ");
    std::string use_bulk;
    
    if (use_bulk_p) {
        use_bulk = std::string(use_bulk_p);
    } else {
        use_bulk = "true";
    }
    if (use_bulk == "true") {
        return GetDataSubarraysBULK(ireq, req, process_cb);
    }
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataSubarrays.size", req.m_dataSubarrays.size()}});
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_GET_SUBARRAYS_ERROR);
        return;
    }
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataSubarrays.empty()) {
        span.setError("Empty URI list");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_GET_SUBARRAYS_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    struct ArrayEntry {
        const std::string* uri;
        const std::string* key;
        const DataArrayTypes::GetDataSubarraysType* sub;

        const std::string& path() const {
            return sub->m_uid.m_pathInResource;
        }
    };
    using ArrayEntries = std::vector<ArrayEntry>;
    std::map<std::string, ArrayEntries> init_space_arrays;

    for (auto& entry : req.m_dataSubarrays) {
        const std::string& cid = entry.first; // client ID
        DataArrayTypes::GetDataSubarraysType& sub = entry.second;
        AryIdent& ary_id = sub.m_uid; // ETP URI

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_id, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        if (sub.m_counts.size() != sub.m_starts.size()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: starts rank != counts rank");
            continue;
        }

        if (sub.m_counts.empty()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: Empty counts array");
            continue;
        }

        init_space_arrays[uri.dataspace_].emplace_back(
            ArrayEntry{ &ary_id.m_uri, &cid, &sub }
        );
    }

    // Follow dataspace references if needed
    std::map<std::string, ArrayEntries> per_space_arrays;
    std::map<std::string, std::string> initial_uris;
    for (auto& entry : init_space_arrays) {
        ArrayEntries& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        // The dataspace that matters is the one of the initial URI
        if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            for(const ArrayEntry& nk : nks){
                const std::string& i_uri= *nk.uri;;
                EmlUri uri = reader.containing_dataspace_uri(EmlUri::parse(i_uri));
                if(uri.to_string()!=i_uri){
                    const_cast<std::string&>(*nk.uri) = uri.to_string();
                    per_space_arrays[uri.dataspace_].emplace_back(nk);
                }else{
                    per_space_arrays[space.uid_].emplace_back(nk);
                }
            }
        }else{
            per_space_arrays[space.uid_] = std::move(nks);
        }
    }

    for (auto& entry : per_space_arrays) {
        ArrayEntries& nks = entry.second;

        Space space = repo_->findSpaceInfo(entry.first);

        ErrorInfo err;
        auto reader = repo_->getSpaceReader(space, err);
        if (!reader) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        oes::eml::openkv::SpaceReader& r = *reader;

        // Sort by path (for better page-cache-locality)
        if (nks.size() > 1) {
            std::sort(nks.begin(), nks.end(), [](const auto& lhs, const auto& rhs) {
                return lhs.path() < rhs.path();
            });
        }

        AryContainerResolver containers(r, 1);
        for (const ArrayEntry& nk : nks) {
            const std::string& cid = *nk.key;
            const std::string& path = nk.path();
            assert(path[0] != '/'); // checked/normalized above
            const Guid container = containers.container_of(*nk.uri, path);
            SqlAry sql_ary = container.isValid() ? r.select_ary(container, path) : SqlAry();

            if (!sql_ary.valid()) {
                reply.addError(cid, ENOT_FOUND, path);
                continue;
            }
            if (sql_ary.rank_ != int(nk.sub->m_counts.size())) {
                reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                    "Ranks differ: dimensions[{}] vs counts[{}]",
                    sql_ary.rank_, nk.sub->m_counts.size()
                ));
                continue;
            }

            if (!(1 <= sql_ary.rank_ && sql_ary.rank_ <= 4)) {
                OES_ERROR("Unexpected rank = {}: {}", sql_ary.rank_, path);
                continue;
            }

            int64_t total_size = 1;
            for (int r_idx = 0; r_idx < sql_ary.rank_; ++r_idx) {
                int64_t r_start = nk.sub->m_starts[r_idx];
                if (!(0 <= r_start && r_start <= sql_ary.dims_[r_idx])) {
                    reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                        "starts[{}] {} outside [0, dimensions[{}] {})",
                        r_idx, r_start, r_idx, sql_ary.dims_[r_idx]
                    ));
                }
                int64_t r_count = nk.sub->m_counts[r_idx];
                int64_t r_end = r_start + r_count;
                if (r_end > sql_ary.dims_[r_idx]) {
                    reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                        "(starts[{}] {} + counts[{}] {}) > dimensions[{}] {}",
                        r_idx, r_start, r_idx, r_count, r_idx, sql_ary.dims_[r_idx]
                    ));
                }
                if (r_count != 0 && total_size > std::numeric_limits<int64_t>::max() / r_count) {
                    reply.addError(cid, ELIMIT_EXCEEDED, "Sub-array counts overflow");
                    total_size = -1;
                    break;
                }
                total_size *= r_count;
            }
            if (total_size < 0) {
                continue;
            }

            const size_t byte_size = sql_ary.byte_size();
            const size_t sub_byte_size = total_size * sql_ary.elem_size();

            if (sub_byte_size >= reply.max_bytes_) {
                reply.addError(cid, ELIMIT_EXCEEDED, fmt::format(
                    "Oversize sub-array ({} > {} bytes): {}", sub_byte_size, reply.max_bytes_, path
                ));
                continue;
            }

            const size_t size = sub_byte_size+8 + cid.size()+8;
            reply.maybeProcess(size);


            auto it_ary = reply.addMapPart(cid);
            DataArrayTypes::DataArray& ary = it_ary.first->second;
            reply.addBytes(size);
            sql_ary.get_dimensions(ary.m_dimensions);

            if (!byte_size) {
                // Empty array. Kinda silly to request zero-length sub-array...
                auto& any_ary = ary.m_data.m_item;
                switch (sql_ary.type_) { // see EpcConv.cpp's acceptHdf5ObjInfo lambda
                case hdf::HdfAryType::eAryOfInt:     any_ary.emplace<ArrayOfInt>(); break;
                case hdf::HdfAryType::eAryOfDouble:  any_ary.emplace<ArrayOfDouble>(); break;
                case hdf::HdfAryType::eAryOfFloat:   any_ary.emplace<ArrayOfFloat>(); break;
                case hdf::HdfAryType::eAryOfBytes:   any_ary.emplace<bytes_t>(); break;
                case hdf::HdfAryType::eAryOfBoolean: any_ary.emplace<ArrayOfBoolean>(); break;
                case hdf::HdfAryType::eAryOfLong:    any_ary.emplace<ArrayOfLong>(); break;
                case hdf::HdfAryType::eAryOfString:  any_ary.emplace<ArrayOfString>(); break;
                case hdf::HdfAryType::eAryOfVoid:    [[fallthrough]]; // Impossible
                default: // Not supported
                    OES_ERROR("Unexpected array type = {}: {}", sql_ary.type_, path);
                    reply.eraseMapPart(it_ary.first, sub_byte_size);
                }
                continue;
            }

            const char* read_err = nullptr;
            switch (sql_ary.type_) { // see EpcConv.cpp's acceptHdf5ObjInfo lambda
            case hdf::HdfAryType::eAryOfInt:     read_err = readDataSubarray<ArrayOfInt>    (r, sql_ary, ary, *nk.sub, total_size); break;
            case hdf::HdfAryType::eAryOfDouble:  read_err = readDataSubarray<ArrayOfDouble> (r, sql_ary, ary, *nk.sub, total_size); break;
            case hdf::HdfAryType::eAryOfFloat:   read_err = readDataSubarray<ArrayOfFloat>  (r, sql_ary, ary, *nk.sub, total_size); break;
            case hdf::HdfAryType::eAryOfBytes:   read_err = readDataSubarray<ByteVec>       (r, sql_ary, ary, *nk.sub, total_size); break;
            case hdf::HdfAryType::eAryOfBoolean: read_err = readDataSubarray<ArrayOfBoolean>(r, sql_ary, ary, *nk.sub, total_size); break;
            case hdf::HdfAryType::eAryOfLong:    read_err = readDataSubarray<ArrayOfLong>   (r, sql_ary, ary, *nk.sub, total_size); break;
            case hdf::HdfAryType::eAryOfString:  [[fallthrough]]; // Not used by RESQML
            case hdf::HdfAryType::eAryOfVoid:    [[fallthrough]]; // Impossible
            default: // Not supported
                OES_ERROR("Unexpected array type = {}: {}", sql_ary.type_, path);
                reply.eraseMapPart(it_ary.first, sub_byte_size);
                continue;
            }

            if (read_err) {
                reply.addError(
                    cid, EINVALID_OBJECT,
                    fmt::format("Cannot read sub-array: {}", read_err)
                );
                reply.eraseMapPart(it_ary.first, sub_byte_size); // since already inserted
            }
        }
    }
    // On success - increment success counter and record latency 
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::ARRAY_OPS_COUNTER_GET_SUBARRAYS_SUCCESS,oes::core::metrics::ARRAY_LATENCY_GET_SUBARRAYS,start);
    reply.finalProcess();
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_GET_SUBARRAYS_ERROR);
    etp12::handleException(process_cb);
}


void OpenKVDataArray::GetDataSubarraysBULK( // messageTypeId 3
    IRequest& ireq, GetDataSubarrays_req_t req,
    EtpResponseHandler<GetDataSubarrays_res_t> process_cb // messageTypeId 8
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataSubarrays.size", req.m_dataSubarrays.size()}});
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    const size_t BULK_SIZE = session->pg_bulk_size_;

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataSubarrays.empty()) {
        span.setError("Empty URI list");
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    std::map<std::string, ProcessBulkSubArrayHelper> init_space_arrays;

    for (auto& entry : req.m_dataSubarrays) {
        const std::string& cid = entry.first; // client ID
        DataArrayTypes::GetDataSubarraysType& sub = entry.second;
        AryIdent& ary_id = sub.m_uid; // ETP URI

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_id, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        if (sub.m_counts.size() != sub.m_starts.size()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: starts rank != counts rank");
            continue;
        }

        if (sub.m_counts.empty()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: Empty counts array");
            continue;
        }

        const auto& [it, _] = init_space_arrays.try_emplace(
            uri.dataspace_, ProcessBulkSubArrayHelper()
        );

        auto& [ds, h] = *it;
        h.emplace_back(cid, ary_id.m_pathInResource, ary_id.m_uri, &sub);
    }

    // Follow dataspace references if needed
    std::map<std::string, ProcessBulkSubArrayHelper> per_space_arrays;
    for (auto& entry : init_space_arrays) {
        ProcessBulkSubArrayHelper& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);

        ErrorInfo err;
        if(!space.isValid()){
            err.m_code = ENOT_FOUND;
            err.m_message = fmt::format("Dataspace not found: {}", entry.first);
        }

        // The dataspace that matters is the one of the initial URI
        if (!space.isValid() || !checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const auto& nk : nks.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const auto& nk : nks.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        // Check that there is at least one row in admin.space_rel that has the dataspace as origin
        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            for(size_t i {0}; i< nks.size(); ++i){
                auto tup = nks.c_at(i);
                const auto& cid = std::get<0>(tup);
                const auto& path = std::get<1>(tup);
                const auto& i_uri = std::get<2>(tup); // can belong to another dataspace
                auto* sub_req = std::get<3>(tup);

                EmlUri source_uri = reader.containing_dataspace_uri(EmlUri::parse(i_uri)); //TODO optimize it
                const auto& [it, _] = per_space_arrays.try_emplace(source_uri.dataspace_, ProcessBulkSubArrayHelper());
                auto& [ds, h] = *it;
                if(source_uri.to_string() != i_uri){

                    h.emplace_back(cid, path, source_uri.to_string(), sub_req);
                }else{
                    h.emplace_back(cid, path, i_uri, sub_req);
                    // per_space_arrays[space.uid_].emplace_back(nk);
                }
            }
        }else{
            per_space_arrays[space.uid_] = std::move(nks);
        }
    }

    for (auto& entry : per_space_arrays) {
        auto& bulk_helper = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const auto& nk : bulk_helper.client_ids()) {
                reply.addError(nk, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        auto reader = repo_->getSpaceReader(space, err);
        if (!reader) {
            for (const auto& nk : bulk_helper.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        oes::eml::openkv::SpaceReader& r = *reader;

        // Process arrays in bulk

        AryContainerResolver containers(r, BULK_SIZE);
        std::vector<int> unresolved;
        auto [epc_guids, paths, indexes] = bulk_helper.get_ary_keys_indexes(containers, unresolved);
        for (int idx : unresolved) {
            auto tuple = bulk_helper.c_at(size_t(idx));
            reply.addError(std::get<0>(tuple), ENOT_FOUND, std::get<1>(tuple));
        }

        auto bulk_arrays = r.bulk_get_ary(epc_guids, paths, indexes, BULK_SIZE);

        std::vector<SqlSubAry> sub_arys_in_row;
        std::vector<SqlSubAry> sub_arys_single_blob;
        std::vector<SqlSubAry> sub_arys_mult_blob;

        for (auto& sql_ary : bulk_arrays) {
            const size_t idx = sql_ary.bulk_idx;
            auto tuple = bulk_helper.c_at(idx); // cid, path, uri, sub_req

            const auto& cid  = std::get<0>(tuple);
            const auto& path = std::get<1>(tuple);
            auto* sub = std::get<3>(tuple);

            if (!sql_ary.valid()) {
                reply.addError(cid, ENOT_FOUND, path);
                continue;
            }

            if (!sub) {
                reply.addError(cid, EINVALID_STATE, "Cannot find sub-array request");
                continue;
            }

            if (sql_ary.rank_ != int(sub->m_counts.size())) {
                reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                    "Ranks differ: dimensions[{}] vs counts[{}]",
                    sql_ary.rank_, sub->m_counts.size()
                ));
                continue;
            }

            if (!(1 <= sql_ary.rank_ && sql_ary.rank_ <= 4)) {
                OES_ERROR("Unexpected rank = {}: {}", sql_ary.rank_, path);
                continue;
            }

            int64_t total_size = 1;
            bool bounds_ok = true;
            for (int r_idx = 0; r_idx < sql_ary.rank_; ++r_idx) {
                int64_t r_start = sub->m_starts[r_idx];
                if (!(0 <= r_start && r_start <= sql_ary.dims_[r_idx])) {
                    reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                        "starts[{}] {} outside [0, dimensions[{}] {})",
                        r_idx, r_start, r_idx, sql_ary.dims_[r_idx]
                    ));
                    bounds_ok = false;
                    break;
                }
                int64_t r_count = sub->m_counts[r_idx];
                int64_t r_end = r_start + r_count;
                if (r_end > sql_ary.dims_[r_idx]) {
                    reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                        "(starts[{}] {} + counts[{}] {}) > dimensions[{}] {}",
                        r_idx, r_start, r_idx, r_count, r_idx, sql_ary.dims_[r_idx]
                    ));
                    bounds_ok = false;
                    break;
                }
                if (r_count != 0 && total_size > std::numeric_limits<int64_t>::max() / r_count) {
                    reply.addError(cid, ELIMIT_EXCEEDED, "Sub-array counts overflow");
                    bounds_ok = false;
                    break;
                }
                total_size *= r_count;
            }
            if (!bounds_ok) continue;

            const size_t byte_size    = sql_ary.byte_size();
            const size_t sub_byte_size = static_cast<size_t>(total_size) * sql_ary.elem_size();

            if (sub_byte_size >= reply.max_bytes_) {
                reply.addError(cid, ELIMIT_EXCEEDED, fmt::format(
                    "Oversize sub-array ({} > {} bytes): {}", sub_byte_size, reply.max_bytes_, path
                ));
                continue;
            }

            if (!byte_size) {
                // Empty array — handle inline without a blob fetch
                const size_t size = sub_byte_size + 8 + cid.size() + 8;
                reply.maybeProcess(size);
                auto it_ary = reply.addMapPart(cid);
                DataArrayTypes::DataArray& ary = it_ary.first->second;
                reply.addBytes(size);
                sql_ary.get_dimensions(ary.m_dimensions);

                auto& any_ary = ary.m_data.m_item;
                switch (sql_ary.type_) {
                case hdf::HdfAryType::eAryOfInt:     any_ary.emplace<ArrayOfInt>();     break;
                case hdf::HdfAryType::eAryOfDouble:  any_ary.emplace<ArrayOfDouble>();  break;
                case hdf::HdfAryType::eAryOfFloat:   any_ary.emplace<ArrayOfFloat>();   break;
                case hdf::HdfAryType::eAryOfBytes:   any_ary.emplace<bytes_t>();        break;
                case hdf::HdfAryType::eAryOfBoolean: any_ary.emplace<ArrayOfBoolean>(); break;
                case hdf::HdfAryType::eAryOfLong:    any_ary.emplace<ArrayOfLong>();    break;
                case hdf::HdfAryType::eAryOfString:  any_ary.emplace<ArrayOfString>();  break;
                case hdf::HdfAryType::eAryOfVoid:    [[fallthrough]];
                default:
                    OES_ERROR("Unexpected array type = {}: {}", sql_ary.type_, path);
                    reply.eraseMapPart(it_ary.first, sub_byte_size);
                }
                continue;
            }

            SqlSubAry sub_ary;
            sub_ary.sql_ary      = sql_ary;
            sub_ary.sub          = sub;
            sub_ary.total_size   = total_size;
            sub_ary.sub_byte_size = sub_byte_size;

            if (sql_ary.is_compressed()) {
                reply.addError(cid, EINVALID_OBJECT,
                    "Cannot read sub-array: Cannot incrementally read compressed array");
                continue;
            }

            if (sql_ary.is_in_row()) {
                sub_arys_in_row.push_back(std::move(sub_ary));
            } else if (sql_ary.is_single_blob()) {
                sub_arys_single_blob.push_back(std::move(sub_ary));
            } else {
                sub_arys_mult_blob.push_back(std::move(sub_ary));
            }
        }

        bulk_get_subarray_in_row(sub_arys_in_row,     bulk_helper, r, reply, BULK_SIZE);
        bulk_get_subarray_single_blob(sub_arys_single_blob, bulk_helper, r, reply, BULK_SIZE);
        bulk_get_subarray_mult_blob(sub_arys_mult_blob,  bulk_helper, r, reply, BULK_SIZE);
    }

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

void OpenKVDataArray::PutDataArrays( // messageTypeId 4
    IRequest& ireq, PutDataArrays_req_t req,
    EtpResponseHandler<PutDataArrays_res_t> process_cb // messageTypeId 10
) try {
    char * use_bulk_p = std::getenv("USE_BULK_WRITE");
    std::string use_bulk;
    
    if (use_bulk_p) {
        use_bulk = std::string(use_bulk_p);
    } else {
        use_bulk = "true";
    }
    if (use_bulk == "true") {
        return PutDataArraysBULK(ireq, req, process_cb);
    }
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataArrays.size", req.m_dataArrays.size()}});
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataArrays.empty()) {
        span.setError("Nothing to add");
        
        // Audit log: Empty request is a validation failure
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_ARRAYS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_CREATE,
            "unknown",
            "Empty array list"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to add");
    }

    // First, check entries, and sort them by space
    struct VecInfo {
        size_t size_ = 0;
        size_t sizeof_ = 0;
        const void* data_ = nullptr;
        hdf::HdfAryType type_ = hdf::HdfAryType::eAryOfVoid;
    };
    auto get_vec_info = [](const auto& vec, openkv::HdfAryType type) {
        return VecInfo{
            vec.size(),
            sizeof(*vec.data()),
            vec.data(),
            type
        };
    };

    using Entry = std::map<std::string, Datatypes::DataArrayTypes::PutDataArraysType>::value_type;
    struct AryEntry { Entry* req_entry_; VecInfo vec_info_; };
    using Entries = std::vector<AryEntry>;
    std::map<std::string, Entries> entries_by_space;
    for (auto& entry : req.m_dataArrays) {
        const std::string& cid = entry.first; // client ID
        AryIdent& ary_ident = entry.second.m_uid;
        const AryData& ary_data = entry.second.m_array;

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_ident, reply, cid);
        if (!uri.isValid()) {
            continue;
        }
        if (AryContainerResolver::needs_container_lookup(uri)) {
            reply.addError(cid, EINVALID_ARGUMENT, eml20_put_at_owner_msg);
            continue;
        }

        const auto& dims = ary_data.m_dimensions;
        if (dims.empty()) {
            reply.addError(cid, EINVALID_OBJECT, "No array dimensions");
            continue;
        }
        if (dims.size() > 4) {
            reply.addError(cid, ELIMIT_EXCEEDED, fmt::format("Array rank > 4: {}", dims.size()));
            continue;
        }
        const int64_t full_dim = checkedDimsProduct(dims);
        if (full_dim < 0) {
            reply.addError(cid, ELIMIT_EXCEEDED, "Array dimensions overflow or non-positive");
            continue;
        }

        VecInfo vec_info;
        const AnyArray_item_v& any_ary = ary_data.m_data.m_item;

        switch (any_ary.index()) {
        case size_t(AnyArray_item_e::eArrayOfBoolean):
            vec_info = { // Cannot use get_vec_info() directly, no vector<bool>::data()
                ary_as<ArrayOfBoolean>(any_ary).size(),
                1, nullptr, hdf::HdfAryType::eAryOfBoolean // pointer assign later, too early here
            };
            break;
        case size_t(AnyArray_item_e::eArrayOfInt):
            vec_info = get_vec_info(ary_as<ArrayOfInt>(any_ary), hdf::HdfAryType::eAryOfInt);
            break;
        case size_t(AnyArray_item_e::eArrayOfLong):
            vec_info = get_vec_info(ary_as<ArrayOfLong>(any_ary), hdf::HdfAryType::eAryOfLong);
            break;
        case size_t(AnyArray_item_e::eArrayOfFloat):
            vec_info = get_vec_info(ary_as<ArrayOfFloat>(any_ary), hdf::HdfAryType::eAryOfFloat);
            break;
        case size_t(AnyArray_item_e::eArrayOfDouble):
            vec_info = get_vec_info(ary_as<ArrayOfDouble>(any_ary), hdf::HdfAryType::eAryOfDouble);
            break;
        case size_t(AnyArray_item_e::eArrayOfString): // FIXME
            vec_info = get_vec_info(ary_as<ArrayOfString>(any_ary), hdf::HdfAryType::eAryOfString);
            vec_info.sizeof_ = 0; // Override. std::string is "special"
            break;
        case size_t(AnyArray_item_e::eBytes):
            vec_info = get_vec_info(std::get<bytes_t>(any_ary), hdf::HdfAryType::eAryOfBytes);
            break;
        } // no default on purpose

        // Warning: Array may be empty
        if (int64_t(vec_info.size_) != full_dim) {
            reply.addError(cid, EINVALID_OBJECT, fmt::format(
                "Array data - dimensions mismatch: {} != {}",
                vec_info.size_, full_dim
            ));
            continue;
        }

        entries_by_space[uri.dataspace_].emplace_back(AryEntry{ &entry, vec_info });
    }

    if (entries_by_space.empty() ||
        (reply.errorCount() == req.m_dataArrays.size())
    ) {
        // all entries are invalid, bail out now
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: All arrays failed validation
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_ARRAYS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_CREATE,
            "unknown",
            "All arrays failed validation"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
        return reply.finalProcess();
    }

    // Handle multi-spaces in parallel? Needs parallel transactions then?
    for (auto& entry : entries_by_space) {
        Entries& ary_entries = entry.second;

        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            
            // Audit log: Authorization failure for this dataspace
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_ARRAYS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                space.uri().to_string(),
                err.m_message
            );
            
            continue;
        }

        std::vector<EmlUri> txSpaceUris;
        std::string txSpaceUrisStr;
        txSpaceUris.emplace_back(EmlUri::make_space(space.uid_)); // To replace by space.uri()
        txSpaceUrisStr = space.uid_ ;
        // Create a local transaction if no transaction is ongoing. Record the session Id in case it goes away in the meantime.
        auto sessionId = session->id() ;
        LocalTransactionWriteRaii<OpenKVDataArray> local_tx(*this, txSpaceUrisStr, txSpaceUris);

        // If this is a local transaction this might have taken enough time for the client to give up.
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR(
                "SVR Thread({}) Session({}): Session has been forcefully terminated while waiting for transaction, {} tries over {}s",
                oes::core::thisThreadId(), sessionId, local_tx.numTries(), local_tx.tryDuration()
            );
            throw std::runtime_error("SVR Session has been forcefully terminated while waiting for transaction.");
        }

        if(!local_tx.ready()){
            // This can only fail if this is a local transaction.
            
            // Audit log: Transaction timeout
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_ARRAYS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                space.uid_,
                fmt::format("Failed to get transaction after {} tries over {}s", local_tx.numTries(), local_tx.tryDuration()/1000)
            );
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
            return reply.fatalError(
                EMAX_TRANSACTIONS_EXCEEDED, fmt::format("SVR Thread({}) Session({}) Local Tx: Failed to get transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000)
            );
        } else if(local_tx.isLocal()){
            OES_INFO(
                "SVR Thread({}) Session({}) Local Tx: Got transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000
            );
        }

        auto writer = repo_->getSpaceWriter(space, err);
        if (!writer) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue; // next space
        }
        
        const auto tx = this->session()->tx_;

        std::vector<int8_t> bools;
        oes::eml::openkv::SpaceWriter& w = *writer;
        for (AryEntry& ary_entry : ary_entries) {
            const std::string& cid = ary_entry.req_entry_->first; // client ID
            AryIdent& ary_ident = ary_entry.req_entry_->second.m_uid;
            // Check that the containing object is in the resource table
            const EmlUri uri = EmlUri::parse(ary_ident.m_uri);
            if(!w.check_ary_container(stored_uid(uri), tx!=nullptr)){
                reply.addError(cid, ENOT_FOUND, fmt::format(
                    "Array Container object {}({}) not found", uri.to_type(), uri.guid_.toXmlString()
                ));
                continue;
            }
            const AryData& ary_data = ary_entry.req_entry_->second.m_array;
            const VecInfo& vec_info = ary_entry.vec_info_;

            // Warning: req's original path values are moved-from (stolen here).
            openkv::HdfRef ref{ -1, -1, std::move(ary_ident.m_pathInResource), uri.guid_ };
            assert(!ref.path.empty() && ref.path[0] != '/'); // normalized above

            openkv::HdfAry ary;
            ary.rank = static_cast<int>(ary_data.m_dimensions.size());
            assert(ary.rank <= 4); // already checked above
            switch (ary.rank) {
            case 4: ary.dims[3] = static_cast<uint64_t>(ary_data.m_dimensions[3]); [[fallthrough]];
            case 3: ary.dims[2] = static_cast<uint64_t>(ary_data.m_dimensions[2]); [[fallthrough]];
            case 2: ary.dims[1] = static_cast<uint64_t>(ary_data.m_dimensions[1]); [[fallthrough]];
            case 1: ary.dims[0] = static_cast<uint64_t>(ary_data.m_dimensions[0]); break;
            }
            ary.uncompressed_size = (vec_info.size_ * vec_info.sizeof_);
            ary.storage_size = ary.uncompressed_size; // compression still a TODO
            ary.refd_data = vec_info.data_;
            ary.elem_type = vec_info.type_;

            // "Fixup" the data pointer of "boolean" arrays
            if (vec_info.type_ == hdf::HdfAryType::eAryOfBoolean) {
                assert(ary.data() == nullptr);
                const auto& bits = ary_as<ArrayOfBoolean>(ary_data.m_data.m_item);
                bools.assign(bits.begin(), bits.end());
                ary.refd_data = bools.data();
            }

            bool ary_was_found = false;
            if (tx!=nullptr) {
                auto ary_found = tx->missing_arys_.find({ref.epc_guid, ref.path});
                if (ary_found != tx->missing_arys_.end()) {
                    ref.obj_id = ary_found->second.obj_id;
                    ref.xpa_id = ary_found->second.xpa_id;
                    tx->missing_arys_.erase(ary_found); // no longer missing
                    ary_was_found = true;
                }
            }

            if(!ary_was_found) {
                // In a transaction, we may want to overwrite an existing array
                // so this should be applied also in the transactional context
                eml_int64 array_id = -1;
                ary_was_found = w.select_array_object(ref.epc_guid, ref.path, array_id, ref.obj_id, ref.xpa_id);
            }

            eml_int64 ary_id = w.upsert_ary(ref, ary);

            if (!ary_was_found) {
                // Keep track of orphaned array rowids, indexed by path,
                // to update obj_id and xpa_id once their owner objects
                // are added later, in the same TX.
                if (tx!=nullptr) {
                    const auto orphan_added = tx->orphan_arys_.emplace(
                        OpenKVTransaction::AryKey{ ref.epc_guid, std::move(ref.path) },
                        OpenKVTransaction::OrphanAry{ ary_id }
                    );
                    // A duplicate in the same request maps to the same row
                    OES_ASSERT(orphan_added.second || orphan_added.first->second.ary_id == ary_id, ;);
                }
            }

            // LDe's code does that: see on_put_data_arrays() in DataArrayStoreImpl.hpp
            // Seems useless to me, but his DX ETP 1.2 client expects it, so must do it too???
            reply.addMapPart(cid); // FIXME: What to put in Map's entry value string?
        }
        if (local_tx.isLocal()) {
            const std::vector<std::string> missing = w.check_pending_ary_containers();
            if (!missing.empty()) {
                oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
                return reply.fatalError(EINVALID_STATE, fmt::format(
                    "Missing array container(s): {}", oes::core::StringUtils::join(missing, ", ")
                )); // local_tx rolls back
            }
        }
        local_tx.commit();
    }

    if (reply.errorCount() == req.m_dataArrays.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: All arrays failed
        if (!entries_by_space.empty()) {
            auto first_space = entries_by_space.begin()->first;
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_ARRAYS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                first_space,
                "All arrays failed to put"
            );
        }
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
        return reply.finalProcess();
    }

    // Audit log: Log success for successfully inserted/updated arrays
    const size_t success_count = req.m_dataArrays.size() - reply.errorCount();
    if (success_count > 0) {
        const std::string commit_status = (session->tx_ != nullptr) ? "COMMIT_PENDING" : "COMMITTED";
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_ARRAYS,
            AuditLogConstants::RESULT_SUCCESS,
            AuditLogConstants::ACTION_CREATE,
            "",
            fmt::format("{} array(s) successfully put, commit_status: {}", success_count, commit_status)
        );
    }

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_SUCCESS,oes::core::metrics::ARRAY_LATENCY_PUT_ARRAYS,start);

    return reply.finalProcess();
} catch (...) {
    // Audit log: Log exception/failure
    try {
        if (auto session = session_.lock()) {
            AuditLog::log(
                *repo_->getServerConfig(),
                session.get(),
                AuditLogConstants::OP_PUT_DATA_ARRAYS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_CREATE,
                "unknown",
                "Exception occurred during PutDataArrays"
            );
        }
    } catch (...) {
        // Ignore audit log errors
    }
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
    etp12::handleException(process_cb);
}


void OpenKVDataArray::PutDataArraysBULK( // messageTypeId 4
    IRequest& ireq, PutDataArrays_req_t req,
    EtpResponseHandler<PutDataArrays_res_t> process_cb // messageTypeId 10
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataArrays.size", req.m_dataArrays.size()}});
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    const size_t BULK_SIZE = session->pg_bulk_size_;

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataArrays.empty()) {
        span.setError("Nothing to add");
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to add");
    }

    // First, check entries, and sort them by space
    struct VecInfo {
        size_t size_ = 0;
        size_t sizeof_ = 0;
        const void* data_ = nullptr;
        hdf::HdfAryType type_ = hdf::HdfAryType::eAryOfVoid;
    };
    auto get_vec_info = [](const auto& vec, openkv::HdfAryType type) {
        return VecInfo{
            vec.size(),
            sizeof(*vec.data()),
            vec.data(),
            type
        };
    };

    // Split the req arrays by dataspace
    using Entry = std::map<std::string, Datatypes::DataArrayTypes::PutDataArraysType>::value_type;
    struct AryEntry { Entry* req_entry_; VecInfo vec_info_; };
    using Entries = std::vector<AryEntry>;
    std::map<std::string, Entries> entries_by_space;
    for (auto& entry : req.m_dataArrays) {
        const std::string& cid = entry.first; // client ID
        AryIdent& ary_ident = entry.second.m_uid;
        const AryData& ary_data = entry.second.m_array;

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_ident, reply, cid);
        if (!uri.isValid()) {
            continue;
        }
        if (AryContainerResolver::needs_container_lookup(uri)) {
            reply.addError(cid, EINVALID_ARGUMENT, eml20_put_at_owner_msg);
            continue;
        }

        int64_t full_dim = 1;
        const auto& dims = ary_data.m_dimensions;
        switch (size_t rank = dims.size()) {
        case 4: full_dim *= dims[3]; [[fallthrough]];
        case 3: full_dim *= dims[2]; [[fallthrough]];
        case 2: full_dim *= dims[1]; [[fallthrough]];
        case 1: full_dim *= dims[0]; break;
        case 0:
            reply.addError(cid, EINVALID_OBJECT, "No array dimensions");
            continue;
        default:
            reply.addError(cid, ELIMIT_EXCEEDED, fmt::format("Array rank > 4: {}", rank));
            continue;
        }

        VecInfo vec_info;
        const AnyArray_item_v& any_ary = ary_data.m_data.m_item;

        switch (any_ary.index()) {
        case size_t(AnyArray_item_e::eArrayOfBoolean):
            vec_info = { // Cannot use get_vec_info() directly, no vector<bool>::data()
                ary_as<ArrayOfBoolean>(any_ary).size(),
                1, nullptr, hdf::HdfAryType::eAryOfBoolean // pointer assign later, too early here
            };
            break;
        case size_t(AnyArray_item_e::eArrayOfInt):
            vec_info = get_vec_info(ary_as<ArrayOfInt>(any_ary), hdf::HdfAryType::eAryOfInt);
            break;
        case size_t(AnyArray_item_e::eArrayOfLong):
            vec_info = get_vec_info(ary_as<ArrayOfLong>(any_ary), hdf::HdfAryType::eAryOfLong);
            break;
        case size_t(AnyArray_item_e::eArrayOfFloat):
            vec_info = get_vec_info(ary_as<ArrayOfFloat>(any_ary), hdf::HdfAryType::eAryOfFloat);
            break;
        case size_t(AnyArray_item_e::eArrayOfDouble):
            vec_info = get_vec_info(ary_as<ArrayOfDouble>(any_ary), hdf::HdfAryType::eAryOfDouble);
            break;
        case size_t(AnyArray_item_e::eArrayOfString): // FIXME
            vec_info = get_vec_info(ary_as<ArrayOfString>(any_ary), hdf::HdfAryType::eAryOfString);
            vec_info.sizeof_ = 0; // Override. std::string is "special"
            break;
        case size_t(AnyArray_item_e::eBytes):
            vec_info = get_vec_info(std::get<bytes_t>(any_ary), hdf::HdfAryType::eAryOfBytes);
            break;
        } // no default on purpose

        // Warning: Array may be empty
        if (int64_t(vec_info.size_) != full_dim) {
            reply.addError(cid, EINVALID_OBJECT, fmt::format(
                "Array data - dimensions mismatch: {} != {}",
                vec_info.size_, full_dim
            ));
            continue;
        }

        entries_by_space[uri.dataspace_].emplace_back(AryEntry{ &entry, vec_info });
    }

    if (entries_by_space.empty() ||
        (reply.errorCount() == req.m_dataArrays.size())
    ) {
        // all entries are invalid, bail out now
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        return reply.finalProcess();
    }

    // Handle multi-spaces in parallel? Needs parallel transactions then?
    for (auto& entry : entries_by_space) {
        Entries& ary_entries = entry.second;

        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue;
        }

        std::vector<EmlUri> txSpaceUris;
        std::string txSpaceUrisStr;
        txSpaceUris.emplace_back(EmlUri::make_space(space.uid_)); // To replace by space.uri()
        txSpaceUrisStr = space.uid_ ;
        // Create a local transaction if no transaction is ongoing. Record the session Id in case it goes away in the meantime.
        auto sessionId = session->id() ;
        LocalTransactionWriteRaii<OpenKVDataArray> local_tx(*this, txSpaceUrisStr, txSpaceUris);

        // If this is a local transaction this might have taken enough time for the client to give up.
        session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR(
                "SVR Thread({}) Session({}): Session has been forcefully terminated while waiting for transaction, {} tries over {}s",
                oes::core::thisThreadId(), sessionId, local_tx.numTries(), local_tx.tryDuration()
            );
            throw std::runtime_error("SVR Session has been forcefully terminated while waiting for transaction.");
        }

        if(!local_tx.ready()){
            // This can only fail if this is a local transaction.
            return reply.fatalError(
                EMAX_TRANSACTIONS_EXCEEDED, fmt::format("SVR Thread({}) Session({}) Local Tx: Failed to get transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000)
            );
        } else if(local_tx.isLocal()){
            OES_INFO(
                "SVR Thread({}) Session({}) Local Tx: Got transaction for {} after {} tries over {}s",
                oes::core::thisThreadId(), session->id(), txSpaceUrisStr, local_tx.numTries(), local_tx.tryDuration()/1000
            );
        }

        auto writer = repo_->getSpaceWriter(space, err);
        if (!writer) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue; // next space
        }
        
        const auto tx = this->session()->tx_;

        oes::eml::openkv::SpaceWriter& w = *writer;

        // --- Bulk pre-fetch: containers + existing array objects ---
        std::vector<std::string>      all_types;
        std::vector<oes::core::Guid>  all_uuids;
        std::vector<std::string>      all_paths;
        std::vector<int> entries_indexes;

        all_types.reserve(ary_entries.size());
        all_uuids.reserve(ary_entries.size());
        all_paths.reserve(ary_entries.size());
        entries_indexes.reserve(ary_entries.size());
        for (int i{0}; i < static_cast<int>(ary_entries.size()); i++) {
            const auto& ae = ary_entries.at(i);
            auto [etp_type, guid] = stored_uid(EmlUri::parse(ae.req_entry_->second.m_uid.m_uri));
            all_types.push_back(std::move(etp_type));
            all_uuids.push_back(guid);
            all_paths.push_back(ae.req_entry_->second.m_uid.m_pathInResource);
            entries_indexes.push_back(i);
        }

        // 1. Bulk-check containers
        auto found_containers_map = w.bulk_check_ary_containers(all_types, all_uuids, entries_indexes, BULK_SIZE, tx != nullptr);

        // 2. Bulk-lookup existing arrays
        //    Paths already in tx->missing_arys_ will be skipped per-entry below.
        auto ary_obj_map = w.bulk_select_ary_objects(all_uuids, all_paths, BULK_SIZE);

        // 3. Prepare arrays to be inserted
        const size_t found_count = static_cast<size_t>(std::accumulate(found_containers_map.cbegin(), found_containers_map.cend(), 0));
        std::vector<HdfAry> arrays; arrays.reserve(found_count);
        std::vector<HdfRef> refs; refs.reserve(found_count);
        entries_indexes.clear(); // reuse the entries_indexes vector
        std::unordered_map<size_t, bool> is_orphan_map;
        // Owner resolved for the first occurrence of each key, reused by later duplicates
        std::map<AryKey, std::pair<OpenKVTransaction::OrphanAry, bool>> resolved_owners;

        for (size_t i{0}; i < ary_entries.size(); i++) {
            auto& ary_entry = ary_entries.at(i);

            const std::string& cid = ary_entry.req_entry_->first; // client ID
            AryIdent& ary_ident = ary_entry.req_entry_->second.m_uid;
            const EmlUri uri = EmlUri::parse(ary_ident.m_uri);

            // Check that the containing object is in the resource table
            if (!found_containers_map.at(i)) {
                reply.addError(cid, ENOT_FOUND, fmt::format(
                    "Array Container object {}({}) not found", uri.to_type(), uri.guid_.toXmlString()
                ));
                continue;
            }
            
            const AryData& ary_data = ary_entry.req_entry_->second.m_array;
            const VecInfo& vec_info = ary_entry.vec_info_;

            // Warning: req's original path values are moved-from (stolen here).
            openkv::HdfRef ref{ -1, -1, ary_ident.m_pathInResource, uri.guid_ }; // copy, not move
            assert(!ref.path.empty() && ref.path[0] != '/'); // normalized above

            openkv::HdfAry ary;
            ary.rank = static_cast<int>(ary_data.m_dimensions.size());
            assert(ary.rank <= 4); // already checked above
            switch (ary.rank) {
            case 4: ary.dims[3] = static_cast<uint64_t>(ary_data.m_dimensions[3]); [[fallthrough]];
            case 3: ary.dims[2] = static_cast<uint64_t>(ary_data.m_dimensions[2]); [[fallthrough]];
            case 2: ary.dims[1] = static_cast<uint64_t>(ary_data.m_dimensions[1]); [[fallthrough]];
            case 1: ary.dims[0] = static_cast<uint64_t>(ary_data.m_dimensions[0]); break;
            }
            ary.uncompressed_size = (vec_info.size_ * vec_info.sizeof_);
            ary.storage_size = ary.uncompressed_size; // compression still a TODO
            ary.refd_data = vec_info.data_;
            ary.elem_type = vec_info.type_;

            // "Fixup" the data pointer of "boolean" arrays
            if (vec_info.type_ == hdf::HdfAryType::eAryOfBoolean) {
                assert(ary.data() == nullptr);
                const auto& bits = ary_as<ArrayOfBoolean>(ary_data.m_data.m_item);
                if (!bits.empty()) {
                    // The write happens after the loop, so each array needs its own copy
                    ary.owned_data = new unsigned char[bits.size()];
                    std::copy(bits.begin(), bits.end(), ary.owned_data);
                }
            }

            bool ary_was_found = false;
            if (auto prev = resolved_owners.find({ref.epc_guid, ref.path}); prev != resolved_owners.end()) {
                // missing_arys_ was consumed, and ary_obj_map predates the first write
                ref.obj_id = prev->second.first.obj_id;
                ref.xpa_id = prev->second.first.xpa_id;
                ary_was_found = prev->second.second;
            } else {
                if (tx!=nullptr) { // in transaction
                    auto ary_found = tx->missing_arys_.find({ref.epc_guid, ref.path});
                    if (ary_found != tx->missing_arys_.end()) {
                        ref.obj_id = ary_found->second.obj_id;
                        ref.xpa_id = ary_found->second.xpa_id;
                        tx->missing_arys_.erase(ary_found); // no longer missing
                        ary_was_found = true;
                    }
                }

                if(!ary_was_found) {
                    // Use the bulk-pre-fetched map (one query for all arrays)
                    auto it = ary_obj_map.find({ref.epc_guid, ref.path});
                    if (it != ary_obj_map.end()) {
                        ref.obj_id  = it->second.obj_id;
                        ref.xpa_id  = it->second.xpa_id;
                        ary_was_found = (ref.obj_id > 0 && ref.xpa_id > 0);
                    }
                }

                resolved_owners.emplace(
                    AryKey{ ref.epc_guid, ref.path },
                    std::make_pair(OpenKVTransaction::OrphanAry{ -1, ref.obj_id, ref.xpa_id }, ary_was_found)
                );
            }

            // if (!ary_was_found) {
            //     orphans_to_add.push_back(i);
            // }
            // auto [item, _] = is_orphan_map.emplace(i, false);
            // item->second = !ary_was_found;
            is_orphan_map.emplace(i, !ary_was_found);

            arrays.push_back(std::move(ary));
            refs.push_back(ref);
            entries_indexes.push_back(static_cast<int>(i)); //? index different of refs.index
        }

        // 5. Actually push the arrays to postgresql
        auto res_map = w.bulk_upsert_ary(refs, arrays, entries_indexes, BULK_SIZE);

        // 6. Send the response
        for (auto& [ary_entry_idx, ary_id] : res_map) {
            const auto& entry = ary_entries.at(ary_entry_idx);
            const auto& cid = entry.req_entry_->first;
            auto& ary_ident = entry.req_entry_->second.m_uid;
            
            reply.addMapPart(cid);

            // Keep track of orphaned array rowids, indexed by (container, path),
            // to update obj_id and xpa_id once their owner objects
            // are added later, in the same TX.
            if (tx != nullptr && is_orphan_map.at(ary_entry_idx)) {
                const auto orphan_added = tx->orphan_arys_.emplace(
                    OpenKVTransaction::AryKey{
                        all_uuids.at(ary_entry_idx), std::move(ary_ident.m_pathInResource)
                    },
                    OpenKVTransaction::OrphanAry{ ary_id }
                );
                // A duplicate in the same request maps to the same row
                OES_ASSERT(orphan_added.second || orphan_added.first->second.ary_id == ary_id, ;);
            }
        }
        if (local_tx.isLocal()) {
            const std::vector<std::string> missing = w.check_pending_ary_containers();
            if (!missing.empty()) {
                return reply.fatalError(EINVALID_STATE, fmt::format(
                    "Missing array container(s): {}", oes::core::StringUtils::join(missing, ", ")
                )); // local_tx rolls back
            }
        }
        local_tx.commit();
    }

    if (reply.errorCount() == req.m_dataArrays.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        return reply.finalProcess();
    }

    return reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

void OpenKVDataArray::PutDataSubarrays( // messageTypeId 5
    IRequest& ireq, PutDataSubarrays_req_t req,
    EtpResponseHandler<PutDataSubarrays_res_t> process_cb // messageTypeId 11
) try {
    char * use_bulk_p = std::getenv("USE_BULK_WRITE");
    std::string use_bulk;
    
    if (use_bulk_p) {
        use_bulk = std::string(use_bulk_p);
    } else {
        use_bulk = "true";
    }
    if (use_bulk == "true") {
        return PutDataSubarraysBULK(ireq, req, process_cb);
    }
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataSubarrays.size", req.m_dataSubarrays.size()}});
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);
        return;
    }

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataSubarrays.empty()) {
        span.setError("Nothing to add");
        
        // Audit log: Empty request is a validation failure
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_SUBARRAYS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_UPDATE,
            "unknown",
            "Empty subarray list"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to add");
    }

    using SubAry = Datatypes::DataArrayTypes::PutDataSubarraysType;

    // First, check entries, and sort them by space
    struct VecInfo {
        size_t size_ = 0;
        size_t sizeof_ = 0;
        const void* data_ = nullptr;
        openkv::HdfAryType type_ = hdf::HdfAryType::eAryOfVoid;
    };
    auto get_vec_info = [](const auto& vec, openkv::HdfAryType type) {
        return VecInfo{
            vec.size(),
            sizeof(*vec.data()),
            vec.data(),
            type
        };
    };

    using Entry = std::map<std::string, SubAry>::value_type;
    struct AryEntry { Entry* req_entry_; VecInfo vec_info_; };
    using Entries = std::vector<AryEntry>;
    std::map<std::string, Entries> entries_by_space;
    for (auto& entry : req.m_dataSubarrays) {
        const std::string& cid = entry.first; // client ID
        SubAry& sub_ary = entry.second;
        AryIdent& ary_ident = sub_ary.m_uid;
        const AnyArray& ary_data  = sub_ary.m_data;

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_ident, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        if (sub_ary.m_starts.size() != sub_ary.m_counts.size()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: starts rank != counts rank");
            continue;
        }
        if (sub_ary.m_counts.empty()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: Empty counts array");
            continue;
        }
        if (AryContainerResolver::needs_container_lookup(uri)) {
            reply.addError(cid, EINVALID_ARGUMENT, eml20_put_at_owner_msg);
            continue;
        }

        if (sub_ary.m_counts.size() > 4) {
            reply.addError(cid, ELIMIT_EXCEEDED, "Invalid sub-array: rank > 4");
            continue;
        }

        const int64_t full_count = checkedDimsProduct(sub_ary.m_counts);
        if (full_count < 0) {
            reply.addError(cid, ELIMIT_EXCEEDED, "Invalid sub-array: counts overflow or non-positive");
            continue;
        }

        VecInfo vec_info;
        const AnyArray_item_v& any_ary = ary_data.m_item;

        switch (any_ary.index()) {
        case size_t(AnyArray_item_e::eArrayOfBoolean):
            vec_info = { // Cannot use get_vec_info() directly, no vector<bool>::data()
                ary_as<ArrayOfBoolean>(any_ary).size(),
                1, nullptr, hdf::HdfAryType::eAryOfBoolean // pointer assign later, too early here
            };
            break;
        case size_t(AnyArray_item_e::eArrayOfInt):
            vec_info = get_vec_info(ary_as<ArrayOfInt>(any_ary), hdf::HdfAryType::eAryOfInt);
            break;
        case size_t(AnyArray_item_e::eArrayOfLong):
            vec_info = get_vec_info(ary_as<ArrayOfLong>(any_ary), hdf::HdfAryType::eAryOfLong);
            break;
        case size_t(AnyArray_item_e::eArrayOfFloat):
            vec_info = get_vec_info(ary_as<ArrayOfFloat>(any_ary), hdf::HdfAryType::eAryOfFloat);
            break;
        case size_t(AnyArray_item_e::eArrayOfDouble):
            vec_info = get_vec_info(ary_as<ArrayOfDouble>(any_ary), hdf::HdfAryType::eAryOfDouble);
            break;
        case size_t(AnyArray_item_e::eArrayOfString): // FIXME
            vec_info = get_vec_info(ary_as<ArrayOfString>(any_ary), hdf::HdfAryType::eAryOfString);
            vec_info.sizeof_ = 0; // Override. std::string is "special"
            break;
        case size_t(AnyArray_item_e::eBytes):
            vec_info = get_vec_info(std::get<bytes_t>(any_ary), hdf::HdfAryType::eAryOfBytes);
            break;
        } // no default on purpose

        if (int64_t(vec_info.size_) != full_count) {
            reply.addError(cid, EINVALID_OBJECT, fmt::format(
                "Sub-array data - counts mismatch: {} != {}",
                vec_info.size_, full_count
            ));
            continue;
        }

        entries_by_space[uri.dataspace_].emplace_back(AryEntry{ &entry, vec_info });
    }

    if (entries_by_space.empty() ||
        (reply.errorCount() == req.m_dataSubarrays.size())
    ) {
        // all entries are invalid, bail out now
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: All subarrays failed validation
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_SUBARRAYS,
            AuditLogConstants::RESULT_FAILURE,
            AuditLogConstants::ACTION_UPDATE,
            "unknown",
            "All subarrays failed validation"
        );
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);
        return reply.finalProcess();
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    // Handle multi-spaces in parallel? Needs parallel transactions then?
    for (auto& entry : entries_by_space) {
        Entries& ary_entries = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            
            // Audit log: Authorization failure for this dataspace
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_SUBARRAYS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_UPDATE,
                space.uri().to_string(),
                err.m_message
            );
            
            continue;
        }

        auto writer = repo_->getSpaceWriter(space, err);
        if (!writer) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue; // next space
        }

        openkv::SpaceWriter& w = *writer;
        for (AryEntry& ary_entry : ary_entries) {
            // TODO: Keep track of ranges written, for overlaps and unwritten ranges
            const std::string& cid = ary_entry.req_entry_->first;
            const AryIdent& ary_ident = ary_entry.req_entry_->second.m_uid;
            const std::string& ary_path = ary_ident.m_pathInResource;

            // Lookup existing array, we need its dimensions (and type)
            assert(ary_path[0] != '/'); // checked/normalized above
            SqlAry sql_ary = w.select_ary(EmlUri::parse(ary_ident.m_uri).guid_, ary_path);
            if (!sql_ary.defined()) {
                reply.addError(cid, ENOT_FOUND, ary_path);
                continue;
            }

            DataArrayTypes::DataArrayMetadata ary;
            if (!sql_ary.get_dimensions(ary.m_dimensions)) {
                reply.addError(cid, EINVALID_STATE, ary_path);
                continue;
            }

            const auto& sub_dims = ary_entry.req_entry_->second.m_counts;
            const auto& sub_offs = ary_entry.req_entry_->second.m_starts;

            const size_t rank = ary.m_dimensions.size();
            if (sub_dims.size() != rank) {
                reply.addError(cid, EINVALID_ARGUMENT, "Unexpected sub-array rank");
                continue;
            }

            // Validate starts and counts are within array dimensions.
            // Without this, negative starts cause heap corruption (CWE-787).
            {
                bool bounds_ok = true;
                for (size_t r_idx = 0; r_idx < rank; ++r_idx) {
                    int64_t r_start = sub_offs[r_idx];
                    int64_t r_count = sub_dims[r_idx];
                    int64_t r_dim   = ary.m_dimensions[r_idx];
                    if (r_start < 0 || r_start >= r_dim) {
                        reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                            "starts[{}] {} outside [0, dimensions[{}] {})",
                            r_idx, r_start, r_idx, r_dim
                        ));
                        bounds_ok = false;
                        break;
                    }
                    // Use subtraction to avoid signed overflow in (r_start + r_count).
                    // Safe because r_start >= 0 and r_start < r_dim (validated above).
                    const int64_t remaining = r_dim - r_start;
                    if (r_count <= 0 || r_count > remaining) {
                        reply.addError(cid, EINVALID_ARGUMENT, fmt::format(
                            "(starts[{}] {} + counts[{}] {}) > dimensions[{}] {}",
                            r_idx, r_start, r_idx, r_count, r_idx, r_dim
                        ));
                        bounds_ok = false;
                        break;
                    }
                }
                if (!bounds_ok) {
                    continue;
                }
            }

            if (ary_entry.vec_info_.type_ != sql_ary.type_) {
                reply.addError(cid, EINVALID_ARGUMENT, "Unexpected sub-array type");
                continue;
            }

            const void* buf = ary_entry.vec_info_.data_;
            size_t elem_bytes = ary_entry.vec_info_.sizeof_;
            const char* write_err = nullptr;

            // vector<bool> does not expose its internal buffer as a const bool*,
            // because it uses 1-bit-per-bool internally, not 1-byte-per-bool.
            std::vector<int8_t> bools;
            if (!buf) {
                assert(ary_entry.vec_info_.sizeof_ == 1);
                assert(ary_entry.vec_info_.type_ == openkv::eAryOfBoolean);
                const auto& bits = ary_as<ArrayOfBoolean>(ary_entry.req_entry_->second.m_data.m_item);
                bools.assign(bits.begin(), bits.end());
                buf = bools.data();
            }

            switch (rank) {
            case  4:
                if (sql_ary.dims_[3] != sub_dims[3]) {
                    write_err = writeAry4D(
                        w, sql_ary, buf, elem_bytes, sub_dims, sub_offs
                    );
                    break;
                }
                // degenerates to 3D case
                elem_bytes *= sql_ary.dims_[3];
                [[fallthrough]];

            case  3:
                if (sql_ary.dims_[2] != sub_dims[2]) {
                    write_err = writeAry3D(
                        w, sql_ary, buf, elem_bytes, sub_dims, sub_offs
                    );
                    break;
                }
                // degenerates to 2D case
                elem_bytes *= sql_ary.dims_[2];
                [[fallthrough]];

            case  2:
                if (sql_ary.dims_[1] != sub_dims[1]) {
                    write_err = writeAry2D(
                        w, sql_ary, buf, elem_bytes, sub_dims, sub_offs
                    );
                    break;
                }
                // degenerates to 1D case
                elem_bytes *= sql_ary.dims_[1];
                [[fallthrough]];

            case  1:
                // Note that when ary_dims[0] == sub_dims[0]
                // that's a "full write", i.e. could have been a bind
                write_err = writeAry1D(
                    w, sql_ary, buf, elem_bytes, sub_dims, sub_offs
                );
                break;
            default:
                reply.addError(cid, EINVALID_ARGUMENT, "Unsupported array rank");
                continue;
            }

            if (write_err) {
                reply.addError(cid, EINVALID_STATE, write_err);
                continue;
            }

            reply.addMapPart(cid); // FIXME: See also PutDataArrays
        }
    }

    if (reply.errorCount() == req.m_dataSubarrays.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        
        // Audit log: All subarrays failed
        if (!entries_by_space.empty()) {
            auto first_space = entries_by_space.begin()->first;
            AuditLog::log(
                *repo_->getServerConfig(),
                session,
                AuditLogConstants::OP_PUT_DATA_SUBARRAYS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_UPDATE,
                first_space,
                "All subarrays failed to update"
            );
        }
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);
        return reply.finalProcess();
    }

    // Audit log: Log success for successfully updated subarrays
    const size_t success_count = req.m_dataSubarrays.size() - reply.errorCount();
    if (success_count > 0) {
        const std::string commit_status = (session->tx_ != nullptr) ? "COMMIT_PENDING" : "COMMITTED";
        AuditLog::log(
            *repo_->getServerConfig(),
            session,
            AuditLogConstants::OP_PUT_DATA_SUBARRAYS,
            AuditLogConstants::RESULT_SUCCESS,
            AuditLogConstants::ACTION_UPDATE,
            "",
            fmt::format("{} subarray(s) successfully updated, commit_status: {}", success_count, commit_status)
        );
    }

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_SUCCESS,oes::core::metrics::ARRAY_LATENCY_PUT_SUBARRAYS,start);

    return reply.finalProcess();
} catch (...) {
    // Audit log: Log exception/failure
    try {
        if (auto session = session_.lock()) {
            AuditLog::log(
                *repo_->getServerConfig(),
                session.get(),
                AuditLogConstants::OP_PUT_DATA_SUBARRAYS,
                AuditLogConstants::RESULT_FAILURE,
                AuditLogConstants::ACTION_UPDATE,
                "unknown",
                "Exception occurred during PutDataSubarrays"
            );
        }
    } catch (...) {
        // Ignore audit log errors
    }
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);
    etp12::handleException(process_cb);
}


void OpenKVDataArray::PutDataSubarraysBULK( // messageTypeId 5
    IRequest& ireq, PutDataSubarrays_req_t req,
    EtpResponseHandler<PutDataSubarrays_res_t> process_cb // messageTypeId 11
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataSubarrays.size", req.m_dataSubarrays.size()}});
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    const size_t BULK_SIZE = session->pg_bulk_size_;

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataSubarrays.empty()) {
        span.setError("Nothing to add");
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to add");
    }

    using SubAry = Datatypes::DataArrayTypes::PutDataSubarraysType;

    // First, check entries, and sort them by space
    struct VecInfo {
        size_t size_ = 0;
        size_t sizeof_ = 0;
        const void* data_ = nullptr;
        openkv::HdfAryType type_ = hdf::HdfAryType::eAryOfVoid;
    };
    auto get_vec_info = [](const auto& vec, openkv::HdfAryType type) {
        return VecInfo{
            vec.size(),
            sizeof(*vec.data()),
            vec.data(),
            type
        };
    };

    // Split the req arrays by dataspace
    using Entry = std::map<std::string, SubAry>::value_type;
    struct AryEntry { Entry* req_entry_; VecInfo vec_info_; };
    using Entries = std::vector<AryEntry>;
    std::map<std::string, Entries> entries_by_space;
    for (auto& entry : req.m_dataSubarrays) {
        const std::string& cid = entry.first; // client ID
        SubAry& sub_ary = entry.second;
        AryIdent& ary_ident = sub_ary.m_uid;
        const AnyArray& ary_data  = sub_ary.m_data;

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_ident, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        if (sub_ary.m_starts.size() != sub_ary.m_counts.size()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: starts rank != counts rank");
            continue;
        }
        if (sub_ary.m_counts.empty()) {
            reply.addError(cid, EINVALID_ARGUMENT, "Invalid sub-array: Empty counts array");
            continue;
        }
        if (AryContainerResolver::needs_container_lookup(uri)) {
            reply.addError(cid, EINVALID_ARGUMENT, eml20_put_at_owner_msg);
            continue;
        }

        int64_t full_count = sub_ary.m_counts[0];
        switch (sub_ary.m_counts.size()) {
        case 4: full_count *= sub_ary.m_counts[3]; [[fallthrough]];
        case 3: full_count *= sub_ary.m_counts[2]; [[fallthrough]];
        case 2: full_count *= sub_ary.m_counts[1]; break;
        case 1: break;
        default:
            reply.addError(cid, ELIMIT_EXCEEDED, "Invalid sub-array: rank > 4");
            continue;
        }

        VecInfo vec_info;
        const AnyArray_item_v& any_ary = ary_data.m_item;

        switch (any_ary.index()) {
        case size_t(AnyArray_item_e::eArrayOfBoolean):
            vec_info = { // Cannot use get_vec_info() directly, no vector<bool>::data()
                ary_as<ArrayOfBoolean>(any_ary).size(),
                1, nullptr, hdf::HdfAryType::eAryOfBoolean // pointer assign later, too early here
            };
            break;
        case size_t(AnyArray_item_e::eArrayOfInt):
            vec_info = get_vec_info(ary_as<ArrayOfInt>(any_ary), hdf::HdfAryType::eAryOfInt);
            break;
        case size_t(AnyArray_item_e::eArrayOfLong):
            vec_info = get_vec_info(ary_as<ArrayOfLong>(any_ary), hdf::HdfAryType::eAryOfLong);
            break;
        case size_t(AnyArray_item_e::eArrayOfFloat):
            vec_info = get_vec_info(ary_as<ArrayOfFloat>(any_ary), hdf::HdfAryType::eAryOfFloat);
            break;
        case size_t(AnyArray_item_e::eArrayOfDouble):
            vec_info = get_vec_info(ary_as<ArrayOfDouble>(any_ary), hdf::HdfAryType::eAryOfDouble);
            break;
        case size_t(AnyArray_item_e::eArrayOfString): // FIXME
            vec_info = get_vec_info(ary_as<ArrayOfString>(any_ary), hdf::HdfAryType::eAryOfString);
            vec_info.sizeof_ = 0; // Override. std::string is "special"
            break;
        case size_t(AnyArray_item_e::eBytes):
            vec_info = get_vec_info(std::get<bytes_t>(any_ary), hdf::HdfAryType::eAryOfBytes);
            break;
        } // no default on purpose

        if (int64_t(vec_info.size_) != full_count) {
            reply.addError(cid, EINVALID_OBJECT, fmt::format(
                "Sub-array data - counts mismatch: {} != {}",
                vec_info.size_, full_count
            ));
            continue;
        }

        entries_by_space[uri.dataspace_].emplace_back(AryEntry{ &entry, vec_info });
    }

    if (entries_by_space.empty() ||
        (reply.errorCount() == req.m_dataSubarrays.size())
    ) {
        // all entries are invalid, bail out now
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        return reply.finalProcess();
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    // Handle multi-spaces in parallel? Needs parallel transactions then?
    for (auto& entry : entries_by_space) {
        Entries& ary_entries = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue;
        }

        auto writer = repo_->getSpaceWriter(space, err);
        if (!writer) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue; // next space
        }

        auto reader = repo_->getSpaceReader(space, err);
        if (!reader) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue; // next space
        }

        openkv::SpaceWriter& w = *writer;
        openkv::SpaceReader& r = *reader;

        // std::vector<std::string> all_cids; all_cids.reserve(ary_entries.size());
        std::vector<oes::core::Guid> all_epc_guids; all_epc_guids.reserve(ary_entries.size());
        std::vector<std::string> all_ary_path; all_ary_path.reserve(ary_entries.size());
        std::vector<int> indexes(ary_entries.size(), 0);

        for(int i{0}; i < static_cast<int>(ary_entries.size()); i++) {
            const auto& ary_entry = ary_entries.at(i);
            //!const std::string& cid = ary_entry.req_entry_->first;
            const AryIdent& ary_ident = ary_entry.req_entry_->second.m_uid;
            const std::string& ary_path = ary_ident.m_pathInResource;

            all_epc_guids.push_back(EmlUri::parse(ary_ident.m_uri).guid_);
            all_ary_path.push_back(ary_path);
            indexes.at(i) = i;
        }
        auto sql_arys = w.bulk_get_ary(all_epc_guids, all_ary_path, indexes, BULK_SIZE);

        struct BufferToInsert {
            AryOffsetMeta meta;
            const unsigned char* buffer = nullptr;
            std::vector<uint8_t> owned_bytes;
        };

        std::unordered_map<int, BufferToInsert> sqlAryIndex_to_buffer_map;


        std::vector<int> in_rows;
        std::vector<int> single_blobs;
        std::vector<int> mult_blobs;

        auto get_offset_metadata = [](
            const SqlAry& ary,
            size_t elem_bytes,
            const std::vector<int64_t>& sub_dims,
            const std::vector<int64_t>& sub_offs,
            const size_t rank) -> AryOffsetMeta {
            
            AryOffsetMeta meta;
            switch (rank) {
            case  4:
                if (ary.dims_[3] != sub_dims[3]) {
                    meta = getAry4D_SubOff_Off(
                        ary, elem_bytes, sub_dims, sub_offs
                    );
                    break;
                }
                // degenerates to 3D case
                elem_bytes *= ary.dims_[3];
                [[fallthrough]];

            case  3:
                if (ary.dims_[2] != sub_dims[2]) {
                    meta = getAry3D_SubOff_Off(
                        ary, elem_bytes, sub_dims, sub_offs
                    );
                    break;
                }
                // degenerates to 2D case
                elem_bytes *= ary.dims_[2];
                [[fallthrough]];

            case  2:
                if (ary.dims_[1] != sub_dims[1]) {
                    meta = getAry2D_SubOff_Off(
                        ary, elem_bytes, sub_dims, sub_offs
                    );
                    break;
                }
                // degenerates to 1D case
                elem_bytes *= ary.dims_[1];
                [[fallthrough]];

            case  1:
                // Note that when ary_dims[0] == sub_dims[0]
                // that's a "full write", i.e. could have been a bind
                meta = getAry1DOff(
                        ary, elem_bytes, sub_dims, sub_offs
                    );
                break;
            default:
                meta.error = "Unsupported array rank";
                return meta;
            }
            assert(meta.sub_off.size() == meta.off.size());

            return meta;
        };

        for (size_t i{0}; i < sql_arys.size(); i++) {
            auto& sql_ary = sql_arys.at(i);
            const size_t idx = sql_ary.bulk_idx;
            const auto& ary_entry = ary_entries.at(idx);
            const AryIdent& ary_ident = ary_entry.req_entry_->second.m_uid;
            const std::string& ary_path = ary_ident.m_pathInResource;

            const std::string& cid = ary_entry.req_entry_->first;
            if (!sql_ary.defined()) {
                reply.addError(cid, ENOT_FOUND, ary_path);
                continue;
            }

            DataArrayTypes::DataArrayMetadata ary;
            if (!sql_ary.get_dimensions(ary.m_dimensions)) {
                reply.addError(cid, EINVALID_STATE, ary_path);
                continue;
            }

            if (sql_ary.is_compressed()) {
                reply.addError(cid, EINVALID_STATE, "Cannot incrementally write compressed array");
                continue;
            }

            const auto& sub_dims = ary_entry.req_entry_->second.m_counts;
            const auto& sub_offs = ary_entry.req_entry_->second.m_starts;

            const size_t rank = ary.m_dimensions.size();
            if (sub_dims.size() != rank) {
                reply.addError(cid, EINVALID_ARGUMENT, "Unexpected sub-array rank");
                continue;
            }

            if (ary_entry.vec_info_.type_ != sql_ary.type_) {
                reply.addError(cid, EINVALID_ARGUMENT, "Unexpected sub-array type");
                continue;
            }   

            auto [it, _] = sqlAryIndex_to_buffer_map.emplace(std::make_pair<int, BufferToInsert>(i,{}));

            BufferToInsert& buf = it->second;
            buf.buffer = static_cast<const unsigned char*>(ary_entry.vec_info_.data_);
            size_t elem_bytes = ary_entry.vec_info_.sizeof_;

            if (!buf.buffer) {
                // vector<bool> does not expose its internal buffer as a const bool*,
                // because it uses 1-bit-per-bool internally, not 1-byte-per-bool.
                assert(ary_entry.vec_info_.sizeof_ == 1);
                assert(ary_entry.vec_info_.type_ == openkv::eAryOfBoolean);
                const auto& bits = ary_as<ArrayOfBoolean>(ary_entry.req_entry_->second.m_data.m_item);
                buf.owned_bytes.assign(bits.begin(), bits.end());
                buf.buffer = reinterpret_cast<const unsigned char*>(buf.owned_bytes.data());
            }

            buf.meta = get_offset_metadata(sql_ary, elem_bytes, sub_dims, sub_offs, rank);
            if (!buf.meta.error.empty()) {
                reply.addError(cid, EINVALID_ARGUMENT, buf.meta.error);
                continue;
            }

            if (sql_ary.is_in_row()) {
                in_rows.push_back(i);
            } else if (sql_ary.is_single_blob()) {
                single_blobs.push_back(i);
            } else {
                mult_blobs.push_back(i);
            }
        }

        // Parts targeting the same array must patch one shared buffer, else only one update survives.
        std::unordered_map<eml_int64, int> owner_by_ary_id;
        for (const auto* list : {&in_rows, &single_blobs, &mult_blobs}) {
            for (int sqlAry_idx : *list) {
                owner_by_ary_id.emplace(sql_arys.at(sqlAry_idx).ary_id_, sqlAry_idx);
            }
        }
        auto owner_of = [&](int sqlAry_idx) {
            return owner_by_ary_id.at(sql_arys.at(sqlAry_idx).ary_id_);
        };

        auto patch_whole_blob = [](std::vector<uint8_t>& dst, const BufferToInsert& buf) {
            for (size_t sub_idx = 0; sub_idx < buf.meta.off.size(); ++sub_idx) {
                assert(buf.meta.off[sub_idx] + buf.meta.len <= dst.size());
                std::memcpy(
                    dst.data() + buf.meta.off[sub_idx],
                    buf.buffer + buf.meta.sub_off[sub_idx],
                    static_cast<size_t>(buf.meta.len)
                );
            }
        };

        auto chunk_byte_size = [](const SqlAry& sql_ary, int chunk_idx) -> size_t {
            const eml_int64 chunk_byte_off = 1ll * chunk_idx * sql_ary.chunk_len_;
            const eml_int64 remaining = sql_ary.usize_ - chunk_byte_off;
            assert(remaining > 0);
            return static_cast<size_t>(std::min<eml_int64>(sql_ary.chunk_len_, remaining));
        };

        if (!in_rows.empty()) {
            std::vector<eml_int64> ary_ids; ary_ids.reserve(in_rows.size());
            std::vector<int> req_idxs; req_idxs.reserve(in_rows.size());
            for (int sqlAry_idx : in_rows) {
                if (owner_of(sqlAry_idx) != sqlAry_idx) {
                    continue;
                }
                ary_ids.push_back(sql_arys.at(sqlAry_idx).ary_id_);
                req_idxs.push_back(sqlAry_idx);
            }

            auto results = r.bulk_get_ary_val(ary_ids, req_idxs, BULK_SIZE);
            std::unordered_map<int, std::vector<uint8_t>> bytes_by_sql_idx;
            for (auto& res : results) {
                bytes_by_sql_idx.emplace(res.ary_sql_idx, std::move(res.bytes));
            }

            std::vector<eml_int64> write_ids; write_ids.reserve(in_rows.size());
            std::vector<const char*> write_values; write_values.reserve(in_rows.size());
            std::vector<size_t> write_lens; write_lens.reserve(in_rows.size());
            std::vector<int> completed_idxs; completed_idxs.reserve(in_rows.size());
            std::vector<int> owner_idxs; owner_idxs.reserve(in_rows.size());

            for (int sqlAry_idx : in_rows) {
                auto buf_it = sqlAryIndex_to_buffer_map.find(sqlAry_idx);
                if (buf_it == sqlAryIndex_to_buffer_map.end()) {
                    continue;
                }

                const int owner_idx = owner_of(sqlAry_idx);
                const auto& sql_ary = sql_arys.at(owner_idx);
                auto& bytes = bytes_by_sql_idx.at(owner_idx);
                if (bytes.empty()) {
                    bytes.assign(sql_ary.byte_size(), 0);
                }

                patch_whole_blob(bytes, buf_it->second);

                if (owner_idx == sqlAry_idx) {
                    owner_idxs.push_back(owner_idx);
                }
                completed_idxs.push_back(sqlAry_idx);
            }

            for (int owner_idx : owner_idxs) {
                const auto& bytes = bytes_by_sql_idx.at(owner_idx);
                write_ids.push_back(sql_arys.at(owner_idx).ary_id_);
                write_values.push_back(reinterpret_cast<const char*>(bytes.data()));
                write_lens.push_back(bytes.size());
            }

            w.bulk_update_ary_values(write_ids, write_values, write_lens, BULK_SIZE);

            for (int sqlAry_idx : completed_idxs) {
                const auto& sql_ary = sql_arys.at(sqlAry_idx);
                const auto& ary_entry = ary_entries.at(sql_ary.bulk_idx);
                reply.addMapPart(ary_entry.req_entry_->first); // FIXME: See also PutDataArrays
            }
        }

        if (!single_blobs.empty()) {
            std::vector<eml_int64> ary_ids; ary_ids.reserve(single_blobs.size());
            std::vector<int> chunk_idxs(single_blobs.size(), 0);
            std::vector<int> req_idxs; req_idxs.reserve(single_blobs.size());
            for (int sqlAry_idx : single_blobs) {
                if (owner_of(sqlAry_idx) != sqlAry_idx) {
                    continue;
                }
                ary_ids.push_back(sql_arys.at(sqlAry_idx).ary_id_);
                req_idxs.push_back(sqlAry_idx);
            }
            chunk_idxs.resize(ary_ids.size());

            auto results = r.bulk_get_bin_val(ary_ids, chunk_idxs, req_idxs, BULK_SIZE);
            std::unordered_map<int, std::vector<uint8_t>> bytes_by_sql_idx;
            for (auto& res : results) {
                bytes_by_sql_idx.emplace(res.ary_sql_idx, std::move(res.bytes));
            }

            std::vector<eml_int64> write_ary_ids; write_ary_ids.reserve(single_blobs.size());
            std::vector<int> write_chunk_idxs; write_chunk_idxs.reserve(single_blobs.size());
            std::vector<const char*> write_values; write_values.reserve(single_blobs.size());
            std::vector<size_t> write_lens; write_lens.reserve(single_blobs.size());
            std::vector<int> completed_idxs; completed_idxs.reserve(single_blobs.size());
            std::vector<int> owner_idxs; owner_idxs.reserve(single_blobs.size());

            for (int sqlAry_idx : single_blobs) {
                auto buf_it = sqlAryIndex_to_buffer_map.find(sqlAry_idx);
                if (buf_it == sqlAryIndex_to_buffer_map.end()) {
                    continue;
                }

                const int owner_idx = owner_of(sqlAry_idx);
                const auto& sql_ary = sql_arys.at(owner_idx);
                auto& bytes = bytes_by_sql_idx[owner_idx];
                if (bytes.empty()) {
                    bytes.assign(sql_ary.byte_size(), 0);
                }

                patch_whole_blob(bytes, buf_it->second);

                if (owner_idx == sqlAry_idx) {
                    owner_idxs.push_back(owner_idx);
                }
                completed_idxs.push_back(sqlAry_idx);
            }

            for (int owner_idx : owner_idxs) {
                const auto& bytes = bytes_by_sql_idx.at(owner_idx);
                write_ary_ids.push_back(sql_arys.at(owner_idx).ary_id_);
                write_chunk_idxs.push_back(0);
                write_values.push_back(reinterpret_cast<const char*>(bytes.data()));
                write_lens.push_back(bytes.size());
            }

            w.bulk_upsert_bin_values(write_ary_ids, write_chunk_idxs, write_values, write_lens, BULK_SIZE);

            for (int sqlAry_idx : completed_idxs) {
                const auto& sql_ary = sql_arys.at(sqlAry_idx);
                const auto& ary_entry = ary_entries.at(sql_ary.bulk_idx);
                reply.addMapPart(ary_entry.req_entry_->first); // FIXME: See also PutDataArrays
            }
        }

        if (!mult_blobs.empty()) {
            std::map<std::pair<int, int>, std::vector<uint8_t>> chunk_bytes_by_key;
            std::vector<std::pair<int, int>> chunk_keys;
            std::vector<eml_int64> fetch_ary_ids;
            std::vector<int> fetch_chunk_idxs;
            std::vector<int> fetch_req_idxs;

            for (int sqlAry_idx : mult_blobs) {
                auto buf_it = sqlAryIndex_to_buffer_map.find(sqlAry_idx);
                if (buf_it == sqlAryIndex_to_buffer_map.end()) {
                    continue;
                }

                const auto& sql_ary = sql_arys.at(sqlAry_idx);
                const BufferToInsert& buf = buf_it->second;

                for (size_t sub_idx = 0; sub_idx < buf.meta.off.size(); ++sub_idx) {
                    const SqlBinRange range = sql_ary.chunk_range(static_cast<int64_t>(buf.meta.off[sub_idx]), buf.meta.len);
                    auto add_chunk = [&](int chunk_idx) {
                        const auto key = std::make_pair(owner_of(sqlAry_idx), chunk_idx);
                        if (chunk_bytes_by_key.emplace(key, std::vector<uint8_t>{}).second) {
                            fetch_ary_ids.push_back(sql_ary.ary_id_);
                            fetch_chunk_idxs.push_back(chunk_idx);
                            fetch_req_idxs.push_back(static_cast<int>(chunk_keys.size()));
                            chunk_keys.push_back(key);
                        }
                    };

                    if (range.is_single_chunk()) {
                        add_chunk(range.first_chunk_);
                        continue;
                    }

                    if (range.has_front_partial_chunk()) {
                        add_chunk(range.first_chunk_);
                    }
                    for (int chunk_idx = range.full_chunks_begin(); chunk_idx < range.full_chunks_end(); ++chunk_idx) {
                        add_chunk(chunk_idx);
                    }
                    if (range.has_back_partial_chunk()) {
                        add_chunk(range.last_chunk_);
                    }
                }
            }

            auto results = r.bulk_get_bin_val(fetch_ary_ids, fetch_chunk_idxs, fetch_req_idxs, BULK_SIZE);
            for (auto& res : results) {
                const auto& key = chunk_keys.at(res.ary_sql_idx);
                chunk_bytes_by_key[key] = std::move(res.bytes);
            }

            for (auto& [key, bytes] : chunk_bytes_by_key) {
                if (!bytes.empty()) {
                    continue;
                }

                const auto& sql_ary = sql_arys.at(key.first);
                bytes.assign(chunk_byte_size(sql_ary, key.second), 0);
            }

            for (int sqlAry_idx : mult_blobs) {
                auto buf_it = sqlAryIndex_to_buffer_map.find(sqlAry_idx);
                if (buf_it == sqlAryIndex_to_buffer_map.end()) {
                    continue;
                }

                const int owner_idx = owner_of(sqlAry_idx);
                const auto& sql_ary = sql_arys.at(owner_idx);
                const BufferToInsert& buf = buf_it->second;

                for (size_t sub_idx = 0; sub_idx < buf.meta.off.size(); ++sub_idx) {
                    const unsigned char* src = buf.buffer + buf.meta.sub_off[sub_idx];
                    const SqlBinRange range = sql_ary.chunk_range(static_cast<int64_t>(buf.meta.off[sub_idx]), buf.meta.len);

                    if (range.is_single_chunk()) {
                        auto& chunk = chunk_bytes_by_key.at(std::make_pair(owner_idx, range.first_chunk_));
                        std::memcpy(chunk.data() + range.front_off_, src, static_cast<size_t>(buf.meta.len));
                        continue;
                    }

                    if (range.has_front_partial_chunk()) {
                        const int write_len = sql_ary.chunk_len_ - range.front_off_;
                        auto& chunk = chunk_bytes_by_key.at(std::make_pair(owner_idx, range.first_chunk_));
                        std::memcpy(chunk.data() + range.front_off_, src, static_cast<size_t>(write_len));
                        src += write_len;
                    }

                    for (int chunk_idx = range.full_chunks_begin(); chunk_idx < range.full_chunks_end(); ++chunk_idx) {
                        auto& chunk = chunk_bytes_by_key.at(std::make_pair(owner_idx, chunk_idx));
                        std::memcpy(chunk.data(), src, static_cast<size_t>(sql_ary.chunk_len_));
                        src += sql_ary.chunk_len_;
                    }

                    if (range.has_back_partial_chunk()) {
                        auto& chunk = chunk_bytes_by_key.at(std::make_pair(owner_idx, range.last_chunk_));
                        std::memcpy(chunk.data(), src, static_cast<size_t>(range.back_len_));
                    }
                }
            }

            std::vector<eml_int64> write_ary_ids;
            std::vector<int> write_chunk_idxs;
            std::vector<const char*> write_values;
            std::vector<size_t> write_lens;
            write_ary_ids.reserve(chunk_bytes_by_key.size());
            write_chunk_idxs.reserve(chunk_bytes_by_key.size());
            write_values.reserve(chunk_bytes_by_key.size());
            write_lens.reserve(chunk_bytes_by_key.size());

            for (auto& [key, bytes] : chunk_bytes_by_key) {
                const auto& sql_ary = sql_arys.at(key.first);
                write_ary_ids.push_back(sql_ary.ary_id_);
                write_chunk_idxs.push_back(key.second);
                write_values.push_back(reinterpret_cast<const char*>(bytes.data()));
                write_lens.push_back(bytes.size());
            }

            w.bulk_upsert_bin_values(write_ary_ids, write_chunk_idxs, write_values, write_lens, BULK_SIZE);

            for (int sqlAry_idx : mult_blobs) {
                const auto& sql_ary = sql_arys.at(sqlAry_idx);
                const auto& ary_entry = ary_entries.at(sql_ary.bulk_idx);
                reply.addMapPart(ary_entry.req_entry_->first); // FIXME: See also PutDataArrays
            }
        }
    }

    if (reply.errorCount() == req.m_dataSubarrays.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        return reply.finalProcess();
    }

    return reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

void OpenKVDataArray::GetDataArrayMetadata( // messageTypeId 6
    IRequest& ireq, GetDataArrayMetadata_req_t req,
    EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
) try {
    char * use_bulk_p = std::getenv("USE_BULK_READ");
    std::string use_bulk;
    
    if (use_bulk_p) {
        use_bulk = std::string(use_bulk_p);
    } else {
        use_bulk = "true";
    }
    if (use_bulk == "true") {
        return GetDataArrayMetadataBULK(ireq, req, process_cb);
    }
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataArrays.size", req.m_dataArrays.size()}});
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataArrays.empty()) {
        span.setError("Empty URI list");
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    {   // WARNING: Custom extension. NOT part of official ETP 1.2
        std::string space_uid_or_path;
        if (etp12::DataArrayExt::is_list_arrays_req(req, space_uid_or_path)) {
            return ListDataArrayMetadata(space_uid_or_path, process_cb);
        }
    }

    struct ArrayEntry {
        const std::string* uri;  //cppcheck-suppress unusedStructMember
        const std::string* key;
        const std::string* path;
    };
    using ArrayEntries = std::vector<ArrayEntry>;

    std::map<std::string, ArrayEntries> init_space_arrays;
    for (auto& entry : req.m_dataArrays) {
        const std::string& cid = entry.first; // client ID
        AryIdent& ary_id = entry.second; // ETP URI

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_id, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        init_space_arrays[uri.dataspace_].emplace_back(
            ArrayEntry{ &ary_id.m_uri, &cid, &ary_id.m_pathInResource }
        );
    }
    
    // Follow dataspace references if needed
    std::map<std::string, ArrayEntries> per_space_arrays;
    for (auto& entry : init_space_arrays) {
        ArrayEntries& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        // The dataspace that matters is the one of the initial URI
        if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            for(const ArrayEntry& nk : nks){
                const std::string& i_uri= *nk.uri;
                EmlUri uri = reader.containing_dataspace_uri(EmlUri::parse(i_uri));
                if(uri.to_string()!=i_uri){
                    const_cast<std::string&>(*nk.uri) = uri.to_string();
                    per_space_arrays[uri.dataspace_].emplace_back(nk);
                }else{
                    per_space_arrays[space.uid_].emplace_back(nk);
                }
            }
        }else{
            per_space_arrays[space.uid_] = std::move(nks);
        }
    }

    for (auto& entry : per_space_arrays) {
        ArrayEntries& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        auto reader = repo_->getSpaceReader(space, err);
        if (!reader) {
            for (const ArrayEntry& nk : nks) {
                reply.addError(*nk.key, err);
            }
            continue;
        }

        oes::eml::openkv::SpaceReader& r = *reader;

        // Sort by path (for better page-cache-locality)
        if (nks.size() > 1) {
            std::sort(nks.begin(), nks.end(), [](const auto& lhs, const auto& rhs) {
                return *lhs.path < *rhs.path;
            });
        }

        AryContainerResolver containers(r, 1);
        for (const ArrayEntry& nk : nks) {
            const std::string& cid = *nk.key;
            const std::string& path = *nk.path; // TODO: drop leading / before
            const std::string key_path = (path[0] == '/')? path.substr(1): path;
            const Guid container = containers.container_of(*nk.uri, key_path);
            SqlAry sql_ary = container.isValid() ? r.select_ary(container, key_path) : SqlAry();

            if (!sql_ary.valid()) {
                reply.addError(cid, ENOT_FOUND, path);
                continue;
            }

            auto it_ary = reply.addMapPart(cid);
            DataArrayTypes::DataArrayMetadata& ary = it_ary.first->second;
            if (!sql_ary.get_dimensions(ary.m_dimensions)) {
                reply.addError(cid, EINVALID_STATE, fmt::format(
                    "Unexpected array rank = {}: {}", sql_ary.rank_, path
                ));
                continue;
            }

            if (!getAryType(sql_ary.type_, ary.m_transportArrayType)) {
                reply.addError(cid, EINVALID_STATE, fmt::format(
                    "Unexpected array type = {}: {}", sql_ary.type_, path
                ));
                continue;
            }
        }
    }

    return reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}


void OpenKVDataArray::GetDataArrayMetadataBULK( // messageTypeId 6
    IRequest& ireq, GetDataArrayMetadata_req_t req,
    EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKVDataArray.") + __func__,
                                    {{"req.dataArrays.size", req.m_dataArrays.size()}});
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    const size_t BULK_SIZE = session->pg_bulk_size_;

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataArrays.empty()) {
        span.setError("Empty URI list");
        return reply.fatalError(EINVALID_ARGUMENT, "Empty URI list");
    }

    {   // WARNING: Custom extension. NOT part of official ETP 1.2
        std::string space_uid_or_path;
        if (etp12::DataArrayExt::is_list_arrays_req(req, space_uid_or_path)) {
            return ListDataArrayMetadata(space_uid_or_path, process_cb);
        }
    }

    std::map<std::string, ProcessBulkArrayHelper> init_space_arrays;
    for (auto& entry : req.m_dataArrays) {
        const std::string& cid = entry.first; // client ID
        AryIdent& ary_id = entry.second; // ETP URI

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_id, reply, cid);
        if (!uri.isValid()) {
            continue;
        }

        const auto& [it, _] = init_space_arrays.try_emplace(
            uri.dataspace_, ProcessBulkArrayHelper()
        );

        auto& [ds, h] = *it;
        h.emplace_back(cid, ary_id.m_pathInResource, ary_id.m_uri);
    }
    
    // Follow dataspace references if needed
    std::map<std::string, ProcessBulkArrayHelper> per_space_arrays;
    for (auto& entry : init_space_arrays) {
        ProcessBulkArrayHelper& nks = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const auto& nk : nks.client_ids()) {
                reply.addError(nk, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        // The dataspace that matters is the one of the initial URI
        if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
            for (const auto& nk : nks.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
        if (!spacedb) {
            for (const auto& nk : nks.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        if(repo_->getAdminDb()->spaceIncludesReference(space)){
            SpaceReader reader(spacedb);
            for(size_t i {0}; i< nks.size(); ++i){
                auto tup = nks.c_at(i);
                const auto& cid = std::get<0>(tup);
                const auto& path = std::get<1>(tup);
                const auto& i_uri = std::get<2>(tup);

                EmlUri source_uri = reader.containing_dataspace_uri(EmlUri::parse(i_uri));
                const auto& [it, _] = per_space_arrays.try_emplace(source_uri.dataspace_, ProcessBulkArrayHelper());
                auto& [ds, h] = *it;
                if(source_uri.to_string() != i_uri){
                    h.emplace_back(cid, path, source_uri.to_string());
                }else{
                    h.emplace_back(cid, path, i_uri);
                }
            }
        }else{
            per_space_arrays[space.uid_] = std::move(nks);
        }
    }

    for (auto& entry : per_space_arrays) {
        auto& bulk_helper = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (const auto& nk : bulk_helper.client_ids()) {
                reply.addError(nk, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        auto reader = repo_->getSpaceReader(space, err);
        if (!reader) {
            for (const auto& nk : bulk_helper.client_ids()) {
                reply.addError(nk, err);
            }
            continue;
        }

        oes::eml::openkv::SpaceReader& r = *reader;

        // Process arrays in bulk

        AryContainerResolver containers(r, BULK_SIZE);
        std::vector<int> unresolved;
        auto [epc_guids, paths, indexes] = bulk_helper.get_ary_keys_indexes(containers, unresolved);
        for (int idx : unresolved) {
            auto tuple = bulk_helper.c_at(size_t(idx));
            reply.addError(std::get<0>(tuple), ENOT_FOUND, std::get<1>(tuple));
        }

        auto bulk_arrays = r.bulk_get_ary(epc_guids, paths, indexes, BULK_SIZE);

        for (auto& sql_ary : bulk_arrays) {
            const size_t idx = sql_ary.bulk_idx;
            auto tuple = bulk_helper.c_at(idx);
            
            const auto& cid = std::get<0>(tuple);
            const auto& path = std::get<1>(tuple);
            //const auto& uri = std::get<2>(tuple);

            assert(path == sql_ary.path_); //TODO remove

            if (!sql_ary.valid()) {
                reply.addError(cid, ENOT_FOUND, path);
                continue;
            }

            auto it_ary = reply.addMapPart(cid);
            DataArrayTypes::DataArrayMetadata& ary = it_ary.first->second;
            if (!sql_ary.get_dimensions(ary.m_dimensions)) {
                reply.addError(cid, EINVALID_STATE, fmt::format(
                    "Unexpected array rank = {}: {}", sql_ary.rank_, path
                ));
                continue;
            }

            if (!getAryType(sql_ary.type_, ary.m_transportArrayType)) {
                reply.addError(cid, EINVALID_STATE, fmt::format(
                    "Unexpected array type = {}: {}", sql_ary.type_, path
                ));
                continue;
            }
        }
    }

    return reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

void OpenKVDataArray::PutUninitializedDataArrays( // messageTypeId 9
    IRequest& ireq, PutUninitializedDataArrays_req_t req,
    EtpResponseHandler<PutUninitializedDataArrays_res_t> process_cb // messageTypeId 12
) try {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("OpenKBDataArray.") + __func__,
                                    {{"req.dataArrays.size", req.m_dataArrays.size()}});
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        span.setError("SVR Session pointer is nullptr (Session forcefully terminated).");
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
        return;
    }

    etp12::MultiPartReply reply(*session, process_cb);

    if (req.m_dataArrays.empty()) {
        span.setError("Nothing to add");
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "Empty array list");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
        return reply.fatalError(EINVALID_ARGUMENT, "Nothing to add");
    }

    // First, check entries, and sort them by space
    struct PartialVec {
        std::vector<int64_t> dims_;
        size_t size_ = 0; // i.e. dims_ product
        size_t sizeof_ = 0;
        openkv::HdfAryType type_ = hdf::HdfAryType::eAryOfVoid;
        // TODO: Ranges written so far
        // See https://stackoverflow.com/questions/28217179/iterating-boosticlinterval-set

        size_t rank() const { return dims_.size(); }
    };

    using Entry = std::map<std::string, Datatypes::DataArrayTypes::PutUninitializedDataArrayType>::value_type;
    struct AryEntry { Entry* req_entry_; PartialVec partial_vec_; };
    using Entries = std::vector<AryEntry>;
    std::map<std::string, Entries> entries_by_space;
    for (auto& entry : req.m_dataArrays) {
        const std::string& cid = entry.first; // client ID
        AryIdent& ary_ident = entry.second.m_uid;
        const AryMeta& ary_meta = entry.second.m_metadata;

        EmlUri uri = checkAryIdentIsValidAndNormalizePath(repo_, ary_ident, reply, cid);
        if (!uri.isValid()) {
            continue;
        }
        if (AryContainerResolver::needs_container_lookup(uri)) {
            reply.addError(cid, EINVALID_ARGUMENT, eml20_put_at_owner_msg);
            continue;
        }

        const auto& dims = ary_meta.m_dimensions;
        if (dims.empty()) {
            reply.addError(cid, EINVALID_OBJECT, "No array dimensions");
            continue;
        }
        if (dims.size() > 4) {
            reply.addError(cid, ELIMIT_EXCEEDED, fmt::format("Array rank > 4: {}", dims.size()));
            continue;
        }
        const int64_t full_dim = checkedDimsProduct(dims);
        if (full_dim < 0) {
            reply.addError(cid, ELIMIT_EXCEEDED, "Array dimensions overflow or non-positive");
            continue;
        }

        PartialVec partial_vec;
        partial_vec.dims_ = dims;
        partial_vec.size_ = full_dim;

        switch (ary_meta.m_transportArrayType) {
        case AnyArrayType::eArrayOfBoolean:
            partial_vec.type_ = hdf::HdfAryType::eAryOfBoolean;
            partial_vec.sizeof_ = 1;
            break;
        case AnyArrayType::eArrayOfInt:
            partial_vec.type_ = hdf::HdfAryType::eAryOfInt;
            partial_vec.sizeof_ = sizeof(decltype(data12::ArrayOfInt::m_values)::value_type);
            break;
        case AnyArrayType::eArrayOfLong:
            partial_vec.type_ = hdf::HdfAryType::eAryOfLong;
            partial_vec.sizeof_ = sizeof(decltype(data12::ArrayOfLong::m_values)::value_type);
            break;
        case AnyArrayType::eArrayOfFloat:
            partial_vec.type_ = hdf::HdfAryType::eAryOfFloat;
            partial_vec.sizeof_ = sizeof(decltype(data12::ArrayOfFloat::m_values)::value_type);
            break;
        case AnyArrayType::eArrayOfDouble:
            partial_vec.type_ = hdf::HdfAryType::eAryOfDouble;
            partial_vec.sizeof_ = sizeof(decltype(data12::ArrayOfDouble::m_values)::value_type);
            break;
        case AnyArrayType::eArrayOfString:
            partial_vec.type_ = hdf::HdfAryType::eAryOfString;
            partial_vec.sizeof_ = 0; // std::string is "special"
            break;
        case AnyArrayType::eBytes:
            partial_vec.type_ = hdf::HdfAryType::eAryOfBytes;
            partial_vec.sizeof_ = 1;
            break;
        } // no default on purpose

        entries_by_space[uri.dataspace_].emplace_back(AryEntry{ &entry, std::move(partial_vec) });
    }

    if (entries_by_space.empty() ||
        (reply.errorCount() == req.m_dataArrays.size())
    ) {
        // all entries are invalid, bail out now
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "All array validations failed");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
        return reply.finalProcess();
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    // Handle multi-spaces in parallel? Needs parallel transactions then?
    for (auto& entry : entries_by_space) {
        Entries& ary_entries = entry.second;
        Space space = repo_->findSpaceInfo(entry.first);
        if(!space.isValid()){
            for (AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, ENOT_FOUND, fmt::format("Dataspace not found: {}", entry.first));
            }
            continue;
        }

        ErrorInfo err;
        if (!checkDataspaceAccess(space, AccessType::CAN_WRITE, err)) {
            AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, space.uri().to_string(), err.m_message);
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue;
        }

        auto writer = repo_->getSpaceWriter(space, err);
        if (!writer) {
            for (const AryEntry& ary_entry : ary_entries) {
                reply.addError(ary_entry.req_entry_->first, err);
            }
            continue; // next space
        }

        oes::eml::openkv::SpaceWriter& w = *writer;
        for (AryEntry& ary_entry : ary_entries) {
            const std::string& cid = ary_entry.req_entry_->first; // client ID
            AryIdent& ary_ident = ary_entry.req_entry_->second.m_uid;
            const AryMeta& ary_meta = ary_entry.req_entry_->second.m_metadata;
            const PartialVec& partial_vec = ary_entry.partial_vec_;

            const EmlUri uri = EmlUri::parse(ary_ident.m_uri);
            if (!w.check_ary_container(stored_uid(uri), api.tx_ != nullptr)) {
                reply.addError(cid, ENOT_FOUND, fmt::format(
                    "Array Container object {}({}) not found", uri.to_type(), uri.guid_.toXmlString()
                ));
                continue;
            }

            // Warning: req's original path values are moved-from (stolen here).
            openkv::HdfRef ref{ -1, -1, std::move(ary_ident.m_pathInResource), uri.guid_ };
            assert(!ref.path.empty() && ref.path[0] != '/'); // normalized above

            openkv::HdfAry ary;
            ary.rank = static_cast<int>(ary_meta.m_dimensions.size());
            assert(ary.rank <= 4); // already checked above
            switch (ary.rank) {
            case 4: ary.dims[3] = static_cast<uint64_t>(ary_meta.m_dimensions[3]); [[fallthrough]];
            case 3: ary.dims[2] = static_cast<uint64_t>(ary_meta.m_dimensions[2]); [[fallthrough]];
            case 2: ary.dims[1] = static_cast<uint64_t>(ary_meta.m_dimensions[1]); [[fallthrough]];
            case 1: ary.dims[0] = static_cast<uint64_t>(ary_meta.m_dimensions[0]); break;
            }
            ary.uncompressed_size = (partial_vec.size_ * partial_vec.sizeof_);
            ary.storage_size = ary.uncompressed_size; // compression still a TODO
          //ary.refd_data = ary.owned_data = nullptr; in case it wasn't clear
            ary.elem_type = partial_vec.type_;

            bool ary_was_found = false;
            if (auto tx = api.tx_) {
                auto ary_found = tx->missing_arys_.find({ref.epc_guid, ref.path});
                if (ary_found != tx->missing_arys_.end()) {
                    ref.obj_id = ary_found->second.obj_id;
                    ref.xpa_id = ary_found->second.xpa_id;
                    tx->missing_arys_.erase(ary_found); // no longer missing
                    ary_was_found = true;
                }
            }

             if(!ary_was_found) {
                // In a transaction, we may want to overwrite an existing array
                // so this should be applied also in the transactional context
                eml_int64 array_id = -1;
                ary_was_found = w.select_array_object(ref.epc_guid, ref.path, array_id, ref.obj_id, ref.xpa_id);
            }

            eml_int64 ary_id = w.upsert_ary(ref, ary);

            if (!ary_was_found) {
                // Keep track of orphaned array rowids, indexed by path,
                // to update obj_id and xpa_id once their owner objects
                // are added later, in the same TX.
                if (auto tx = api.tx_) {
                    const auto orphan_added = tx->orphan_arys_.emplace(
                        OpenKVTransaction::AryKey{ ref.epc_guid, std::move(ref.path) },
                        OpenKVTransaction::OrphanAry{ ary_id }
                    );
                    // A duplicate in the same request maps to the same row
                    OES_ASSERT(orphan_added.second || orphan_added.first->second.ary_id == ary_id, ;);
                }
            }

            // LDe's code does that: see on_put_data_arrays() in DataArrayStoreImpl.hpp
            // Seems useless to me, but his DX ETP 1.2 client expects it, so must do it too???
            reply.addMapPart(cid); // FIXME
        }
    }

    if (reply.errorCount() == req.m_dataArrays.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "All uninitialized arrays failed during processing");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
        return reply.finalProcess();
    }

    // Log success for successfully put uninitialized arrays
    const size_t success_count = req.m_dataArrays.size() - reply.errorCount();
    if (success_count > 0) {
        const std::string commit_status = (session->tx_ != nullptr) ? "COMMIT_PENDING" : "COMMITTED";
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_SUCCESS, AuditLogConstants::ACTION_CREATE, "", fmt::format("{} array(s) successfully put, commit_status: {}", success_count, commit_status));
    }

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_SUCCESS,oes::core::metrics::ARRAY_LATENCY_PUT_UNINITIALIZED_ARRAYS,start);

    return reply.finalProcess();
} catch (...) {
    OpenKVSession* session = session_.lock().get();
    if (session != nullptr) {
        try {
            AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "Exception occurred during PutUninitializedDataArrays");
        } catch (...) {
            // Ignore audit log errors
        }
    }
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
    etp12::handleException(process_cb);
}

// WARNING: Custom extension. NOT part of official ETP 1.2
void OpenKVDataArray::ListDataArrayMetadata(
    const std::string& space_uid_or_path,
    EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb
) try {
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    etp12::MultiPartReply reply(*session, process_cb);

    Space space = repo_->findSpaceInfo(space_uid_or_path);
    if(!space.isValid()) {
        return reply.fatalError(ENOT_FOUND,fmt::format("Space {} not found", space_uid_or_path));
    }

    ErrorInfo err;
    if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        return reply.fatalError();
    }

    Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
    if (!spacedb) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        return reply.fatalError();
    }

    // TODO: Move to SpaceReader
    std::string sql = R"sql(

select rank1, dim1, dim2, dim3, dim4, type, path,
       typ.xml, res.guid, ary.epc_guid, uri.ml
  from ary
  left join res on ary.obj_id = res.obj_id
  left join typ on res.typ_id = typ.id
  left join uri on typ.uri_id = uri.id

    )sql";

    const Guid fake_uid = Guid::make();

    boost::trim(sql);
    oes::postgresql::Statement stmt = spacedb->prepare(std::move(sql));
    while (stmt.step()) {

        DataArrayTypes::DataArrayMetadata ary;

        // first, since used for error messages
        std::string path = stmt.getString(6);
        assert(!path.empty());

        if (!getAryRankAndDims(stmt, 0, path, ary.m_dimensions)) {
            continue;
        }

        if (!getAryType(stmt, 5, ary.m_transportArrayType)) {
            continue;
        }

        // Arrays are listed under their owner. Before PutDataObjects adds it, xml and uid
        // are null: report the container instead, since that is what the array is keyed on.
        const std::string xml = stmt.getString(7);

        AryIdent ident;
        if (xml.empty()) {
            Guid epc_guid = getBlobUid(stmt, 9, /*null_ok*/true);
            if (!epc_guid.isValid()) {
                epc_guid = fake_uid;
            }
            ident.m_uri = EmlUri::make_obj(space.uid_, "obj_EpcExternalPartReference", epc_guid, EmlUri::eEML20).to_string();
        } else {
            const std::string ml = stmt.getString(10);
            ident.m_uri = EmlUri::make_obj(
                space.uid_, xml, getBlobUid(stmt, 8, /*null_ok*/true), EmlUri::parseML(ml.data(), ml.size())
            ).to_string();
        }
        ident.m_pathInResource = path;

        std::string json_meta_key = etp12::DataArrayExt::to_json_meta_key(ident);
        size_t size = json_meta_key.size() + 8; //string size + 8 bytes for length
        size += ary.m_dimensions.size() * 8; // 8 bytes per int64_t
        size += ary.m_preferredSubarrayDimensions.size() * 8; // 8 bytes per int64_t
        size += 18; // 1 byte for transportArrayType, 1 byte for logicalArrayType,  8 bytes for storeLastWrite, 8 bytes for storeCreated
        reply.maybeProcess(size);
        reply.addMapPart(std::move(json_meta_key), std::move(ary));
        reply.addBytes(size);
    }
    stmt.reset();

    return reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

//============================================================================

} // namespace oes::eml::openkv
