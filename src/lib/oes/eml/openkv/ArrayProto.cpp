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

#include <oes/common/etp12/EmlUri.h>
#include <oes/common/HdfAry.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThreadUtils.hpp>
#include <oes/core/utils/Otel.h>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsHelper.h>
#include <oes/core/utils/MetricsNames.h>
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
        int64_t    sub_off = elem_bytes * (sub_i0 * sub_n1  + sub_i1 * sub_n2);

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
        int64_t    sub_off = elem_bytes * (sub_i0 * sub_n1     + sub_i1 * sub_n2  + sub_i2 * sub_n3);

        if (const char* write_err = w.write_ary(ary, sub_ubuf + sub_off, off, len)) {
            return write_err;
        }
    }

    return nullptr;
}

//============================================================================

bool isEpcExternalPartReference(const EmlUri& uri) {
    if (!uri.isObj()) {
        return false;
    }
    if (uri.ml_!=EmlUri::eRESQML20 && uri.ml_ != EmlUri::eEML20 && uri.ml_ != EmlUri::eEML23) {
        return false;
    }
    const bool starts_with_obj_underscore =
        (0 == std::strncmp("obj_", uri.obj_type_.c_str(), 4))
    ;
    if (starts_with_obj_underscore) {
        if (uri.obj_type_ != "obj_EpcExternalPartReference") {
            return false;
        }
    } else if (uri.obj_type_ != "EpcExternalPartReference") {
        return false;
    }
    return true;
}

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

} // namespace

namespace oes::eml::openkv {

//============================================================================

OpenKVDataArray::OpenKVDataArray(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

void OpenKVDataArray::GetDataArrays( // messageTypeId 2
    IRequest& ireq, GetDataArrays_req_t req,
    EtpResponseHandler<GetDataArrays_res_t> process_cb // messageTypeId 1
) try {
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

        for (const ArrayEntry& nk : nks) {
            const std::string& path = *nk.path;
            assert(path[0] != '/'); // see above
            SqlAry sql_ary = r.select_ary(path);

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

void OpenKVDataArray::GetDataSubarrays( // messageTypeId 3
    IRequest& ireq, GetDataSubarrays_req_t req,
    EtpResponseHandler<GetDataSubarrays_res_t> process_cb // messageTypeId 8
) try {
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

        for (const ArrayEntry& nk : nks) {
            const std::string& cid = *nk.key;
            const std::string& path = nk.path();
            assert(path[0] != '/'); // checked/normalized above
            SqlAry sql_ary = r.select_ary(path);

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

void OpenKVDataArray::PutDataArrays( // messageTypeId 4
    IRequest& ireq, PutDataArrays_req_t req,
    EtpResponseHandler<PutDataArrays_res_t> process_cb // messageTypeId 10
) try {
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

    // Check if we're in an ETP transaction (Protocol 18) for audit logging
    bool is_etp_transaction = (session->tx_ != nullptr);
    std::string commit_status = is_etp_transaction ? "COMMIT_PENDING" : "COMMITTED";

    if (req.m_dataArrays.empty()) {
        span.setError("Nothing to add");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);

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
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
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
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
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
            if(!w.check_ary_container(uri.guid_, tx!=nullptr)){
                reply.addError(cid, ENOT_FOUND, fmt::format(
                    "Containing object {} not found", uri.guid_.toXmlString()
                ));
                continue;
            }
            const AryData& ary_data = ary_entry.req_entry_->second.m_array;
            const VecInfo& vec_info = ary_entry.vec_info_;

            // Warning: req's original path values are moved-from (stolen here).
            openkv::HdfRef ref{ -1, -1, std::move(ary_ident.m_pathInResource) };
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
                auto ary_found = tx->missing_arys_.find(ref.path);
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
                ary_was_found = w.select_array_object(ref.path, array_id, ref.obj_id, ref.xpa_id);
            }

            eml_int64 ary_id = w.upsert_ary(ref, ary);

            if (!ary_was_found) {
                // Keep track of orphaned array rowids, indexed by path,
                // to update obj_id and xpa_id once their owner objects
                // are added later, in the same TX.
                if (tx!=nullptr) {
                    const auto orphan_added = tx->orphan_arys_.emplace(
                        std::move(ref.path), OpenKVTransaction::OrphanAry{ ary_id }
                    );
                    OES_ASSERT(orphan_added.second, ;);
                }
            }

            // LDe's code does that: see on_put_data_arrays() in DataArrayStoreImpl.hpp
            // Seems useless to me, but his DX ETP 1.2 client expects it, so must do it too???
            reply.addMapPart(cid); // FIXME: What to put in Map's entry value string?
        }
    }

    if (reply.errorCount() == req.m_dataArrays.size()) {
        // all entries failed to put (insert or update), bail out
        assert(0 == reply.partCount()); // cppcheck-suppress assertWithSideEffect

        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
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

        return reply.finalProcess();
    }

    // Audit log: Log success for successfully inserted/updated arrays
    size_t success_count = req.m_dataArrays.size() - reply.errorCount();
    if (success_count > 0) {
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
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR);
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

    etp12::handleException(process_cb);
}

void OpenKVDataArray::PutDataSubarrays( // messageTypeId 5
    IRequest& ireq, PutDataSubarrays_req_t req,
    EtpResponseHandler<PutDataSubarrays_res_t> process_cb // messageTypeId 11
) try {
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

    // Check if we're in an ETP transaction (Protocol 18) for audit logging
    bool is_etp_transaction = (session->tx_ != nullptr);
    std::string commit_status = is_etp_transaction ? "COMMIT_PENDING" : "COMMITTED";

    if (req.m_dataSubarrays.empty()) {
        span.setError("Nothing to add");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);

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
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);

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
            SqlAry sql_ary = w.select_ary(ary_path);
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
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);

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

        return reply.finalProcess();
    }

    // Audit log: Log success for successfully updated subarrays
    size_t success_count = req.m_dataSubarrays.size() - reply.errorCount();
    if (success_count > 0) {
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
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR);
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

    etp12::handleException(process_cb);
}

void OpenKVDataArray::GetDataArrayMetadata( // messageTypeId 6
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

        for (const ArrayEntry& nk : nks) {
            const std::string& cid = *nk.key;
            const std::string& path = *nk.path; // TODO: drop leading / before
            SqlAry sql_ary = r.select_ary((path[0] == '/')? path.substr(1): path);

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
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "Empty array list");
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
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "All array validations failed");
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

            // Warning: req's original path values are moved-from (stolen here).
            openkv::HdfRef ref{ -1, -1, std::move(ary_ident.m_pathInResource) };
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
                auto ary_found = tx->missing_arys_.find(ref.path);
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
                ary_was_found = w.select_array_object(ref.path, array_id, ref.obj_id, ref.xpa_id);
            }

            eml_int64 ary_id = w.upsert_ary(ref, ary);

            if (!ary_was_found) {
                // Keep track of orphaned array rowids, indexed by path,
                // to update obj_id and xpa_id once their owner objects
                // are added later, in the same TX.
                if (auto tx = api.tx_) {
                    const auto orphan_added = tx->orphan_arys_.emplace(
                        std::move(ref.path), OpenKVTransaction::OrphanAry{ ary_id }
                    );
                    OES_ASSERT(orphan_added.second, ;);
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
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "All uninitialized arrays failed during processing");
        return reply.finalProcess();
    }

    // Log success for successfully put uninitialized arrays
    size_t success_count = req.m_dataArrays.size() - reply.errorCount();
    if (success_count > 0) {
        std::string commit_status = (session->tx_ != nullptr) ? "COMMIT_PENDING" : "COMMITTED";
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_SUCCESS, AuditLogConstants::ACTION_CREATE, "", fmt::format("{} array(s) successfully put, commit_status: {}", success_count, commit_status));
    }
    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_SUCCESS,oes::core::metrics::ARRAY_LATENCY_PUT_UNINITIALIZED_ARRAYS,start);

    return reply.finalProcess();
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR);
    OpenKVSession* session = session_.lock().get();
    if (session != nullptr) {
        try {
            AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_CREATE, "unknown", "Exception occurred during PutUninitializedDataArrays");
        } catch (...) {
            // Ignore audit log errors
        }
    }
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
       typ.xml, res.guid
  from ary
  left join res on ary.obj_id = res.obj_id
  left join typ on res.typ_id = typ.id

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

        // After PutDataArrays, but before PutDataObjects adding their "owners"
        // xml and uid are null. Assign them to the "HDF entity" instead, since
        // otherwise they are "orphans".
        std::string xml = stmt.getString(7);
        Guid uid = getBlobUid(stmt, 8, /*null_ok*/true);

        if (xml.empty()) {
            xml = "obj_EpcExternalPartReference";
        }
        if (!uid.isValid()) {
            uid = fake_uid;
        }

        AryIdent ident; // FIXME: Get ML from DB
        ident.m_uri = EmlUri::make_obj(space.uid_, xml, uid, EmlUri::eRESQML20).to_string();
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
