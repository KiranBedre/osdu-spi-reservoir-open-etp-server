// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
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
#include <unordered_set>
#include <fmt/format.h>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "SpaceDML.h"

#include <oes/common/epc/FilePart.h>
#include <oes/common/epc/ContentType.h>

#include "../../common/etp12/EmlUri.h"

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Statement.h>
#include <oes/postgresql/Transaction.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Otel.h>
#include <oes/core/utils/Logger.h>

using oes::core::Guid;

namespace {

// WARNING: Duplicated in ArrayProto.cpp
const uint64_t in_row_blob_threshold__ =     64*1024; // 64 KiB
const int out_of_row_blob_chunk_size__ = 1*1024*1024; //  1 MiB

// Ensure we are not splitting elements across chunks
static_assert(out_of_row_blob_chunk_size__ <= int(1e9), "<= 1GB");
static_assert(out_of_row_blob_chunk_size__ % 8 == 0, "multiple of 8");
static_assert(out_of_row_blob_chunk_size__ % 4 == 0, "multiple of 4");
static_assert(out_of_row_blob_chunk_size__ % 2 == 0, "multiple of 2");
static_assert(
    out_of_row_blob_chunk_size__ % in_row_blob_threshold__ == 0,
    "multiple of in-row blob threshold (64 KiB)"
);

bool is_ext_part_ref(const std::string& etp_type) {
    return boost::ends_with(etp_type, "EpcExternalPartReference");
}

void bindGuid(oes::postgresql::Statement& stmt, int idx, const Guid& guid) {
    if (guid.isValid()) {
        stmt.bind(idx, guid.begin(), static_cast<int>(Guid::static_size()));
    } else {
        stmt.bind(idx); // null
    }
}

} // namespace

namespace oes::eml::openkv {

//============================================================================

TypedUid stored_uid(const EmlUri& uri) {
    if (!uri.isObj()) {
        return {};
    }
    return { uri.to_canonical_type(), uri.guid_ };
}


//============================================================================

bool SqlBinRange::valid() const {
    if (first_chunk_ > last_chunk_) {
        return false;
    }
    if (front_off_ >= chunk_len_) {
        return false;
    }
    if (back_len_ <= 0) {
        return false;
    }
    if (back_len_ > chunk_len_) {
        return false;
    }
    return true;
}

bool SqlBinRange::defined() const {
    return chunk_len_ && back_len_ != -1;
}

bool SqlBinRange::is_single_chunk() const {
    return first_chunk_ == last_chunk_;
}

int SqlBinRange::full_chunks_begin() const {
    return (front_off_)? first_chunk_ + 1: first_chunk_;
}

int SqlBinRange::full_chunks_end() const {
    return (back_len_ == chunk_len_)? last_chunk_ + 1: last_chunk_;
}

bool SqlBinRange::has_front_partial_chunk() const {
    return front_off_;
}

bool SqlBinRange::has_back_partial_chunk() const {
    return !is_single_chunk() && (back_len_ != chunk_len_);
}

//============================================================================

bool SqlAryId::valid() const {
    return ary_id_ >=0 && !path_.empty() && xpa_id_ >= 0;
}

//============================================================================

// Checks various invariants of this class
bool SqlAry::valid() const {
    if (!defined()) {
        return false;
    }
    if (blob_len_ > 0x10000) {
        return false; // in-row blob larger than 64 KiB
    }
    if (chunk_len_ && ((chunk_len_ & 0xFFFF) != 0)) {
        //return false; // Multiple of 64 KiB (implies modulo 2, 4, 8 as well)
    }
    if (is_in_row() && !is_single_blob()) {
        return false; // is_in_row() implies is_single_blob()
    }
    if (!is_in_row() && is_single_blob() &&
        (!usize_ || !chunk_len_ || usize_ > chunk_len_)
    ) {
        return false; // is_single_blob() implies chunked and (obviously) a single chunk
    }
    if (!is_single_blob() && (!usize_ || !chunk_len_ || usize_ <= chunk_len_)) {
        return false; // not is_single_blob() implies several chunks (2 or more)
    }
    return true;
}

bool SqlAry::defined() const {
    return ary_id_ >= 0;
}

bool SqlAry::is_in_row() const {
    return (blob_len_ != -1) && !chunk_len_ && !usize_;
}

bool SqlAry::is_single_blob() const {
    return (blob_len_ != -1) || (usize_ && usize_ <= chunk_len_);
}

bool SqlAry::is_compressed() const {
    return usize_ && zcomp_ >= 0;
}

size_t SqlAry::size() const {
    return size(elem_size());
}

size_t SqlAry::elem_size() const {
    switch (type_) {
    case eAryOfVoid:    break;
    case eAryOfInt:     return 4;
    case eAryOfDouble:  return 8;
    case eAryOfFloat:   return 4;
    case eAryOfBytes:   return 1;
    case eAryOfBoolean: return 1;
    case eAryOfLong:    return 8;
    case eAryOfString:  break;
    // no default on purpose
    }
    assert(false);
    return 0;
}

size_t SqlAry::size(size_t elem_size) const {
    assert(elem_size == this->elem_size());

    size_t count = byte_size() / elem_size;
    assert(byte_size() == count * elem_size);
    return count;
}

size_t SqlAry::byte_size() const {
    return (blob_len_ != -1)? blob_len_: usize_;
}

int SqlAry::chunk_count() const {
    if (!usize_ || !chunk_len_) {
        assert(is_in_row());
        return 0;
    }
    assert(!is_in_row());
    const eml_int64 count = (usize_ + chunk_len_ - 1) / chunk_len_;
    assert(count > 0);
    assert(!is_single_blob() || count == 1);
    return int(count);
}

int SqlAry::chunk_index(eml_int64 off) const {
    return int(off / chunk_len_);
}

SqlBinRange SqlAry::chunk_range(int64_t off, int64_t len) const {
    assert(!is_in_row());
  //assert(!is_single_blob());

    SqlBinRange r;

    r.chunk_len_ = chunk_len_;

    const int64_t end = off + len;
    r.first_chunk_ = chunk_index(off);
    r.last_chunk_  = chunk_index(end - 1);

    const int64_t first_chunk_byte_off = 1ll * r.first_chunk_ * chunk_len_;
    const int64_t first_chunk_blob_off = off - first_chunk_byte_off;
    assert(0 <= first_chunk_blob_off);
    assert(     first_chunk_blob_off < chunk_len_);
    r.front_off_ = int(first_chunk_blob_off);

    const int64_t last_chunk_byte_off = 1ll * r.last_chunk_ * chunk_len_;
    const int64_t last_chunk_blob_len = end - last_chunk_byte_off;
    assert(0 < last_chunk_blob_len);
    assert(    last_chunk_blob_len <= chunk_len_);
    r.back_len_ = int(last_chunk_blob_len);

    if (r.front_off_ && r.is_single_chunk()) {
        assert(r.back_len_ > r.front_off_);
        r.back_len_ -= r.front_off_;
    }

    return r;
}

bool SqlAry::get_dimensions(std::vector<int64_t>& dims) const {
    dims.resize(rank_);
    switch (rank_) {
    case  4: dims[3] = dims_[3]; [[fallthrough]];
    case  3: dims[2] = dims_[2]; [[fallthrough]];
    case  2: dims[1] = dims_[1]; [[fallthrough]];
    case  1: dims[0] = dims_[0]; break;
    default: assert(false); return false;
    }
    return true;
}

SqlAry SqlAry::from(const HdfAry& ary, eml_int64 ary_id) {
    SqlAry sql_ary;

    sql_ary.ary_id_ = ary_id;
    sql_ary.type_ = ary.elem_type;

    sql_ary.rank_ = ary.rank;
    switch (ary.rank) {
    case  4: sql_ary.dims_[3] = ary.dims[3]; [[fallthrough]];
    case  3: sql_ary.dims_[2] = ary.dims[2]; [[fallthrough]];
    case  2: sql_ary.dims_[1] = ary.dims[1]; [[fallthrough]];
    case  1: sql_ary.dims_[0] = ary.dims[0]; break;
    default: assert(false);
    }

    sql_ary.usize_ = ary.uncompressed_size;
    sql_ary.chunk_len_ = out_of_row_blob_chunk_size__;

    // sql_ary.blob_len_ not known here

    return sql_ary;
}

//============================================================================
SpaceCommon::SpaceCommon(oes::postgresql::Connection_ptr eml_db)
    : db_(std::move(eml_db)) {
    
    prepare(sel_ary_, R"sql(
SELECT id,
       rank1,
       dim1,
       dim2,
       dim3,
       dim4,
       type,
       COALESCE(usize, 0),
       COALESCE(zcomp, -1),
       COALESCE(chunk_length, 0) AS chunk_len,
       COALESCE(LENGTH(value), CASE WHEN dim1 = 0 THEN 0 ELSE NULL END, -1) AS blob_len
  FROM ary
 WHERE epc_guid = $1
   AND path = $2
)sql"
    );

    prepare(bulk_sel_ary_, R"sql(
SELECT tgt.epc_guid, tgt.path, tgt.idx,
       COALESCE(a.id, -1),
       a.rank1, 
       a.dim1, a.dim2, a.dim3, a.dim4,
       a.type,
       COALESCE(a.usize, 0), COALESCE(a.zcomp, -1),
       COALESCE(a.chunk_length,0) AS chunk_len,
       COALESCE(LENGTH(a.value), CASE WHEN a.dim1=0 THEN 0 ELSE NULL END, -1) AS blob_len
  FROM UNNEST(
    $1::uuid[],
    $2::text[],
    $3::int[]
    ) AS tgt(epc_guid, path, idx)
  LEFT JOIN ary AS a
     ON a.epc_guid = tgt.epc_guid
     AND a.path = tgt.path
)sql");

    prepare(sel_ary_val_, R"sql(
SELECT value
  FROM ary
 WHERE id = $1
)sql"
    );

    prepare(sel_bin_val_, R"sql(
SELECT value
  FROM bin
 WHERE id = $1
)sql"
    );
    prepare(sel_bin_, R"sql(
SELECT id,
       LENGTH(value)
  FROM bin
 WHERE ary_id = $1
   AND idx = $2
)sql"
    );
}

void SpaceCommon::prepare(Statement_opt& stmt, std::string sql) const {
    boost::trim(sql);
    stmt.emplace(db_->prepare(std::move(sql)));
}

SqlAry SpaceCommon::select_ary(const oes::core::Guid& epc_guid, const std::string& path) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "SpaceReader." + std::string(__func__),
        {{"path", path}});

    auto& stmt = *sel_ary_;
    stmt.bind(1, epc_guid);
    bindTextNoCopy(stmt, 2, path);

    SqlAry ary;
    if (!stmt.step()) {
        stmt.reset();
        return ary;
    }

    ary.ary_id_ = stmt.getInt64(0);

    // FYI: NULLs implicitly converted to zeros. See type-conversion
    // table from https://www.sqlite.org/c3ref/column_blob.html
    ary.rank_ = stmt.getInt(1);
    ary.dims_[0] = stmt.getInt(2);
    ary.dims_[1] = stmt.getInt(3);
    ary.dims_[2] = stmt.getInt(4);
    ary.dims_[3] = stmt.getInt(5);

    switch (stmt.getInt(6)) {
    case  0: ary.type_ = eAryOfInt; break;
    case  1: ary.type_ = eAryOfDouble; break;
    case  2: ary.type_ = eAryOfFloat; break;
    case  3: ary.type_ = eAryOfBytes; break;
    case  4: ary.type_ = eAryOfBoolean; break;
    case  5: ary.type_ = eAryOfLong; break;
    case  6: ary.type_ = eAryOfString; break;
    }

    ary.usize_ = stmt.getInt64(7);
    ary.zcomp_ = stmt.getInt(8);

    ary.chunk_len_ = stmt.getInt(9);
    ary.blob_len_ = stmt.getInt(10); // -1 means not-a-blob; 0 means empty-blob

    assert(!stmt.step());
    stmt.reset();
    stmt.bind(2); // Just in case, clear non-transient text binding
    return ary;
}

std::vector<SqlAry> SpaceCommon::bulk_get_ary(
        const std::vector<oes::core::Guid>& epc_guids,
        const std::vector<std::string>& paths,
        const std::vector<int>& indexes,
        size_t bulk_size
    ) {
    
    assert(epc_guids.size() == paths.size());
    assert(paths.size() == indexes.size());

    if (paths.size() < bulk_size) {
        bulk_size = paths.size();
    }

    auto remaining = static_cast<long long>(paths.size());


    auto it_guid_beg = epc_guids.cbegin();
    auto it_path_beg = paths.cbegin();
    auto it_idx_beg = indexes.cbegin();

    std::vector<SqlAry> res_arr;
    while (remaining > 0) {
        if (remaining < static_cast<long long>(bulk_size)) { 
            bulk_size = static_cast<size_t>(remaining);
        }

        //!std::vector<std::string> path_bulk(it_path_beg, it_path_beg + bulk_size);
        //!std::vector<int> idx_bulk(it_idx_beg, it_idx_beg + bulk_size);

        //! bulk_sel_ary_->bind<std::string>(1, it_path_beg, it_path_beg + bulk_size);
        bulk_sel_ary_->bind<oes::core::Guid>(1, it_guid_beg, it_guid_beg + bulk_size);
        bulk_sel_ary_->bindTextArray(2, it_path_beg, it_path_beg + bulk_size);
        bulk_sel_ary_->bind<int>(3, it_idx_beg, it_idx_beg + bulk_size);

        size_t n_processed {0};
        while (bulk_sel_ary_->step()) {
            int col_idx = 0;
            SqlAry res;

            res.epc_guid_ = bulk_sel_ary_->getGuid(col_idx++); // tgt.epc_guid
            res.path_ = bulk_sel_ary_->getString(col_idx++); // tgt.path
            res.bulk_idx = bulk_sel_ary_->getInt(col_idx++); // tgt.idx

            res.ary_id_ = bulk_sel_ary_->getInt64(col_idx++);      // a.id
            res.rank_ = bulk_sel_ary_->getInt(col_idx++);           // a.rank1
            res.dims_[0] = bulk_sel_ary_->getInt64(col_idx++);      // a.dim1
            res.dims_[1] = bulk_sel_ary_->getInt64(col_idx++);      // a.dim2 
            res.dims_[2] = bulk_sel_ary_->getInt64(col_idx++);      // a.dim3
            res.dims_[3] = bulk_sel_ary_->getInt64(col_idx++);      // a.dim4
            res.type_ = static_cast<HdfAryType>(bulk_sel_ary_->getInt(col_idx++)); // a.type
            res.usize_ = bulk_sel_ary_->getInt64(col_idx++);        // usize
            res.zcomp_ = bulk_sel_ary_->getInt(col_idx++);          // zcomp
            res.chunk_len_ = bulk_sel_ary_->getInt(col_idx++);      // chunk_len
            res.blob_len_ = bulk_sel_ary_->getInt(col_idx++);       // blob_len

            res_arr.push_back(res);
            ++n_processed;
        }
        bulk_sel_ary_->reset();

        //!assert(n_processed == bulk_size);

        it_guid_beg += bulk_size;
        it_path_beg += bulk_size;
        it_idx_beg += bulk_size;
        remaining -= static_cast<decltype(remaining)>(bulk_size);
    }

    return res_arr;
}

void SpaceCommon::bindTextNoCopy(oes::postgresql::Statement& stmt, int index, const std::string& text) {
    stmt.bind(index,text);
}

//============================================================================

// WARNING: For now, internal caches are never reset, so to be safe,
// use a new instance per transaction.
SpaceReader::SpaceReader(
    oes::postgresql::Connection_ptr eml_db
) : SpaceCommon(std::move(eml_db))
{
    prepare(cnt_rels_, R"sql(
SELECT (SELECT COUNT(*)
          FROM rel
         WHERE obj_id = res.obj_id) AS targets,
       (SELECT COUNT(*)
          FROM rel
         WHERE dst_id = res.obj_id) AS sources
  FROM res
 WHERE obj_id = $1
)sql"
    );

    prepare(edg_rels_, R"sql(
SELECT rel.obj_id,
       rel.dst_id,
       xpa.th
  FROM rel
  JOIN xpa ON rel.xpa_id = xpa.id
 WHERE rel.obj_id = ANY($1)
   AND rel.dst_id = ANY($1)
)sql"
    );
    // is_in_row
    prepare(bulk_sel_ary_val_, R"sql(
SELECT tgt.idx, a.value
  FROM ary AS a
 CROSS JOIN UNNEST($1::bigint[], $2::int[]) AS tgt(id, idx)
 WHERE a.id = tgt.id
)sql"
    );
    prepare(get_ref_spc_, R"sql(
SELECT target_sp
  FROM admin.space_rel rel
  JOIN res ON res.space_rel_id = rel.id
  JOIN typ ON typ.id = res.typ_id
  JOIN uri ON uri.id = typ.uri_id
 WHERE res.guid = $1
   AND uri.ml || '.' || typ.xml = $2
   AND res.space_rel_id IS NOT NULL
)sql"
    );
    prepare(get_rel_spc_, R"sql(
SELECT tgt.etp_type,
       r.guid,
       r.space_rel_id
  FROM res AS r
  JOIN typ ON typ.id = r.typ_id
  JOIN uri ON uri.id = typ.uri_id
  JOIN UNNEST($1::uuid[], $2::text[]) AS tgt(guid, etp_type)
    ON r.guid = tgt.guid
   AND uri.ml || '.' || typ.xml = tgt.etp_type
 WHERE r.space_rel_id IS NOT NULL
)sql"
    );
    prepare(get_dst_spc_, R"sql(
SELECT target_sp
  FROM admin.space_rel
 WHERE id = $1::bigint
)sql"
    );
    
    // One row per target; obj.id is NULL when the object is not found.
    prepare(bulk_sel_obj_, R"sql(
SELECT tgt.typ_ml, tgt.guid, tgt.idx,
       obj.id,
       obj.xml, r.name, r.createdBy,
       COALESCE(r.createdAt, 0),
       COALESCE(r.updatedAt, 0),
       (EXTRACT(EPOCH FROM (r.storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (r.storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       r.space_rel_id
  FROM UNNEST(
            $1::text[],
            $2::uuid[],
            $3::int[]) AS tgt(typ_ml, guid, idx)
  LEFT JOIN (res AS r JOIN obj ON obj.id = r.obj_id)
    ON r.typ_id =
       (
        SELECT typ.id
          FROM typ
          JOIN uri ON uri.id = typ.uri_id
         WHERE uri.ml || '.' || typ.xml = tgt.typ_ml
       )
   AND r.guid = tgt.guid
)sql"
    );
    prepare(bulk_sel_bin_val_, R"sql(
SELECT tgt.idx,
       b.id,
       tgt.bin_idx,
       LENGTH(b.value),
       b.value
  FROM bin AS b
 CROSS JOIN UNNEST(
        $1::bigint[],
        $2::int[],
        $3::int[]) AS tgt(ary_id, bin_idx, idx)
 WHERE b.ary_id = tgt.ary_id
   AND b.idx = tgt.bin_idx
)sql"
    );
}

SpaceReader::~SpaceReader() {
    close_blob();
}

std::pair<int32_t, int32_t>
SpaceReader::count_dst_src_rels(eml_int64 obj_id) {
    auto& stmt = *cnt_rels_;

    stmt.bind(1, obj_id);

    bool has_row = stmt.step();
    OES_RELEASE_UNUSED(has_row);
    assert(has_row);

    int32_t targets = stmt.getInt(0);
    int32_t sources = stmt.getInt(1);

    has_row = stmt.step();
    assert(!has_row);

    stmt.reset();

    return { targets, sources };
}


/**
 * @brief Returns a vector of pairs of edge relationships between objects with the given ids.
 * 
 * @param obj_ids A vector of object ids to search for edge relationships.
 * @return std::vector<EdgeRel> A vector of pairs of edge relationships between objects with the given ids.
 */
std::vector<EdgeRel> SpaceReader::edge_rels(const std::vector<eml_int64>& obj_ids){
    auto& stmt = *edg_rels_;

    // Create a vector of strings from the vector of object ids
    std::vector<std::string> strings;
    for (auto& obj_id : obj_ids) {
        strings.push_back(std::to_string(obj_id));
    }

    // Create a new string concatenating the strings array elements separated by commas and surrounded by curly braces
    std::string array_str = "{";
    array_str += boost::algorithm::join(strings, ",");
    array_str += "}";

    stmt.bind(1, array_str);

    std::vector<EdgeRel> r;
    while (stmt.step()) {
        r.emplace_back(stmt.getInt64(0), stmt.getInt64(1), stmt.getString(2));
    }
    stmt.reset();

    return r;
}

bool SpaceReader::select_bin(
    eml_int64 ary_id, int idx,
    eml_int64& row_id, int& blob_len
) {
    assert(idx >= 0);
    if (cur_blob_.ary_id_ == ary_id && cur_blob_.idx_id_ == idx) {
        row_id = cur_blob_.bin_id_;
        blob_len = static_cast<int>(cur_blob_.pg_blob_.size());
        return true;
    }

    // select the rowid for the blob, but from the bin table
    auto& stmt = *sel_bin_;

    stmt.bind(1, ary_id);
    stmt.bind(2, idx);
    if (!stmt.step()) {
        stmt.reset();
        return false;
    }

    row_id = stmt.getInt64(0);  // i.e. bin.rowid
    blob_len = stmt.getInt(1);  // i.e. length(bin.value)

    assert(!stmt.step()); // since UNIQUE
    stmt.reset();

    cur_blob_.ary_id_ = ary_id;
    cur_blob_.idx_id_ = idx;
    cur_blob_.bin_id_ = row_id;

    return true;
}

const char* SpaceReader::read_ary(
    const SqlAry& ary, unsigned char* dst, int src_len, int64_t src_off
) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(std::string("SpaceReader.") + __func__);

    assert(ary.valid());

    if (ary.is_compressed()) {
        span.setError("Cannot incrementally read compressed array");
        return "Cannot incrementally read compressed array"; // For now
    }

    if (src_off < 0) {
        span.setError("Out-of-bounds incremental read");
        return "Out-of-bounds incremental read";
    }

    // array <= 64 KiB (i.e. in_row_blob_threshold__)
    // in-row (small) array, as a single blob in the ary table
    if (ary.is_in_row()) {
        if ((src_off + src_len) > ary.blob_len_) {
            span.setError("Out-of-bounds incremental read");
            return "Out-of-bounds incremental read";
        }
        auto& stmt = *sel_ary_val_;
        stmt.bind(1,ary.ary_id_);
        stmt.execute();
        int size=0;
        const void* val = stmt.getBlob(0, size);
        if(!val){
            stmt.reset();
            span.setError("Cannot read blob");
            return "Cannot read blob";
        }else{
            std::memcpy(dst, (char*)val + src_off, src_len);
            stmt.reset();
        }
        return nullptr;
    }

    // array > 1 MiB (i.e. out_of_row_blob_chunk_size__)
    // out-of-row array, but large enough to be in multiple chunks in the bin table
    if (!ary.is_single_blob()) {
        if ((src_off + src_len) > ary.usize_) {
            span.setError("Out-of-bounds incremental read");
            return "Out-of-bounds incremental read";
        }

        return read_bin(ary, dst, src_len, src_off);
    }

    // select the rowid for that one "bin blob"
    eml_int64 bin_row_id;
    int bin_blob_len;
    if (!select_bin(ary.ary_id_, 0, bin_row_id, bin_blob_len)) {
        std::memset(dst, 0, src_len); // assume "sparse" array (i.e. missing chunk)
        return nullptr;
    }

    if ((src_off + src_len) > bin_blob_len) {
        span.setError("Out-of-bounds incremental read");
        return "Out-of-bounds incremental read";
    }

    return readBinBlob(bin_row_id, dst, src_off, src_len);
}

const char* SpaceReader::read_bin(
    const SqlAry& ary, unsigned char* dst, int src_len, int64_t src_off
) {
    SqlBinRange range = ary.chunk_range(src_off, src_len);
    if (!range) {
        return "Invalid range";
    }

    assert(range.valid());

    const char* err = nullptr;
    const int chunk_len = range.chunk_len_; // shorter alias

    // Reads a given chunk, whole or in part, updating err in place
    auto read_chunk = [&](
        int chunk_idx, int blob_off, int read_len, int expected_blob_len
    ) -> bool {
        assert(read_len <= chunk_len);
        assert(expected_blob_len <= chunk_len);

        int blob_len;
        eml_int64 row_id;
        const bool chunk_found = select_bin(ary.ary_id_, chunk_idx, row_id, blob_len);
        if (!chunk_found) {
            // assume "sparse" array (i.e. missing chunk is OK)
            std::memset(dst, 0, read_len);
        } else {
#ifdef OES_DEBUG_BUILD
            if (expected_blob_len > 0) {
                assert(blob_len == expected_blob_len);
            }
            assert(int64_t(blob_off) + read_len <= blob_len);
#endif
            OES_RELEASE_UNUSED(expected_blob_len);
            err = readBinBlob(row_id, dst, blob_off, read_len);
        }
        return err == nullptr;
    };

    // reading within a single chunk
    if (range.is_single_chunk()) {
        read_chunk(range.first_chunk_, range.front_off_, range.back_len_, -1);
        return err;
    }

    // Because we deal with the single-chunk case above, we can assume
    // the first chunk (that we "partially" read) is a "full" chunk.
    if (range.has_front_partial_chunk()) {
        const int read_len = chunk_len - range.front_off_;
        if (!read_chunk(range.first_chunk_, range.front_off_, read_len, chunk_len)) {
            return err;
        }
        dst += read_len;
    }

    const int full_chunks_beg = range.full_chunks_begin();
    const int full_chunks_end = range.full_chunks_end();
    for (int chunk_idx = full_chunks_beg; chunk_idx < full_chunks_end; ++chunk_idx) {
        if (!read_chunk(chunk_idx, 0, chunk_len, chunk_len)) {
            return err;
        }
        dst += chunk_len;
    }

    if (range.has_back_partial_chunk()) {
        read_chunk(range.last_chunk_, 0, range.back_len_, -1);
    }

    return err;
}

bool SpaceReader::PgBytea::up_to_date(eml_int64 rowid, eml_int64 last_rowid) const {
    return (last_rowid == rowid && bin_id_ == rowid);
}

void SpaceReader::close_blob() {
    cur_blob_.ary_id_ = -1;
    cur_blob_.idx_id_ = -1;
    cur_blob_.bin_id_ = -1;
    cur_blob_.pg_blob_.clear();
}

const char* SpaceReader::readBinBlob(
    eml_int64 row_id, void* buf, int64_t off, int64_t len
) {
    return readBlob(row_id, cur_blob_, last_bin_rowid_, buf, off, len);
}

const char* SpaceReader::readBlob(
    eml_int64 row_id,
    PgBytea& pg_bytea, eml_int64& last_rowid,
    void* buf, int64_t off, int64_t len
) {
    assert((off + len) < (2ll<<31));
    if (!pg_bytea.up_to_date(row_id, last_rowid) || (off + len) > (int64_t)pg_bytea.pg_blob_.size()) {
        // Need to read a new row from the bin table
        pg_bytea.pg_blob_.clear();
        auto& stmt = *sel_bin_val_;
        stmt.bind(1, row_id);
        stmt.execute();
        int size=0;
        const void* val = stmt.getBlob(0, size);
        if(!val){
            stmt.reset();
            return "cannot read bytea value";
        }
        pg_bytea.pg_blob_.resize(size);
        std::memcpy(pg_bytea.pg_blob_.data(), val, size);
        stmt.reset();
        last_rowid = row_id;
        pg_bytea.bin_id_ = row_id;
    } else {
        // Still on the same bin row
        assert (!pg_bytea.pg_blob_.empty());
        assert (last_rowid == row_id);
    }
    assert((off + len) <= (int64_t)pg_bytea.pg_blob_.size());
    std::memcpy(buf, pg_bytea.pg_blob_.data()+off, len);

    return nullptr; // i.e. OK, no error
}

EmlUri SpaceReader::containing_dataspace_uri(EmlUri uri){
    const TypedUid uid = stored_uid(uri);
    if (uid.first.empty()) {
        return uri;
    }
    get_ref_spc_->bind(1, uid.second.begin(), int(Guid::static_size()));
    get_ref_spc_->bind(2, uid.first);
    
    if(get_ref_spc_->step()){
        uri.dataspace_ = get_ref_spc_->getString(0);
    }
    get_ref_spc_->reset();
    
    return uri;
}

/*!
 * Get dataspace relation ids for the given resources, when not null
 * \param uids The ETP type and guid of the resources
 * \return A map of resource identities, as stored, and their corresponding dataspace ids
*/
std::map<TypedUid,eml_int64> SpaceReader::containing_dataspace_ids(const std::vector<TypedUid>& uids, size_t bulk_size){
    std::map<TypedUid,eml_int64> spc_ids;
    if (uids.empty()) {
        return spc_ids;
    }
    std::vector<Guid> guids;
    std::vector<std::string> types;
    guids.reserve(uids.size());
    types.reserve(uids.size());
    for (const auto& [etp_type, guid] : uids) {
        types.push_back(etp_type);
        guids.push_back(guid);
    }

    if (guids.size() < bulk_size) {
        bulk_size = guids.size();
    }
    auto remaining = guids.size();
    auto it_guids_beg = guids.cbegin();
    auto it_types_beg = types.cbegin();

    auto stmt = *get_rel_spc_;
    try {
        while (remaining > 0) {
            if (remaining < bulk_size) {
                bulk_size = remaining;
            }
            stmt.bind<Guid>(1, it_guids_beg, it_guids_beg + bulk_size);
            stmt.bindTextArray(2, it_types_beg, it_types_beg + bulk_size);
            while (stmt.step()) {
                spc_ids.insert({{stmt.getString(0),stmt.getGuid(1)},stmt.getInt64(2)});
            }
            stmt.reset();

            it_guids_beg += bulk_size;
            it_types_beg += bulk_size;
            remaining -= bulk_size;
        }
    } catch (const std::exception& e) {
        std::string uid_str;
        for (const auto& [etp_type, guid] : uids) {
            uid_str += etp_type + '(' + guid.toXmlString() + "), ";
        }
        OES_ERROR("Error while getting import space of [{}]: {}", uid_str, e.what());
        throw std::runtime_error(
            fmt::format("Error while getting import space of [{}]: {}", uid_str, e.what())
        );
    }

    return spc_ids;
}

/*! 
 * Get the destination dataspace for the given space relation id
 * \param id The space relation id
 * \return The destination dataspace
*/
std::string SpaceReader::get_destination_dataspace(eml_int64 id){
    try{
        get_dst_spc_->bind(1, id);
        std::string spc = "";
        if(get_dst_spc_->step()){
            spc = get_dst_spc_->getString(0);
        }
        get_dst_spc_->reset();
        return spc;
    }catch(const std::exception& e){
        OES_ERROR("Error while getting destination space id {} : {}", id, e.what());
        return "";
    }
}

/*!
 * Get the Resource information for the given URI and its targets
 * \param uri The URI of the resource to get
 * \return A vector of ResourceRef object containing the resource information
*/
std::vector<ResourceRef> SpaceReader::get_dependencies( const std::string& uri ){
    if(uri.empty()){
        return {};
    }
    if(!sel_deps_){
        prepare(sel_deps_, R"sql(
SELECT guid,
       name,
       uri.ns,
       uri.pp,
       uri.ml,
       uri.vs,
       xml,
       version,
       createdAt,
       updatedAt,
       createdBy,
       updatedBy,
       xdata_json,
       xdata_avro,
       space_rel_id
  FROM (
        WITH rescache AS (
            SELECT obj_id,
                   0 AS level
              FROM res
             WHERE res.typ_id = (
                       SELECT typ.id
                         FROM typ
                         JOIN uri ON uri.id = typ.uri_id
                        WHERE (uri.ml || '.' || typ.xml) = $1
                )
                AND res.guid = $2
                -- and res.version = $3
        )
        SELECT obj_id,
               MIN(level) AS level
          FROM (
                WITH RECURSIVE children_of(obj_id, level) AS (
                    SELECT obj_id,
                           0 AS level
                      FROM rescache
                    UNION
                    SELECT dst_id,
                           level + 1
                      FROM rel
                      JOIN children_of c ON c.obj_id = rel.obj_id
                     WHERE level < 10
                )
                SELECT obj_id,
                       level
                  FROM children_of
               ) c
         GROUP BY obj_id
       ) AS ids
  JOIN res ON res.obj_id = ids.obj_id
  JOIN typ ON typ.id = res.typ_id
  JOIN uri ON uri.id = typ.uri_id
 ORDER BY ids.level
)sql"
        );
    }
    EmlUri eml_uri = EmlUri::parse(uri);
    const std::string st = eml_uri.to_canonical_type();
    sel_deps_->bind(1, st);
    sel_deps_->bind(2, eml_uri.guid_.begin(), int(Guid::static_size()));
    // sel_deps_->bind(3, eml_uri.version_);
    std::vector<ResourceRef> deps;
    while(sel_deps_->step()){
        ResourceRef ref;
        int res_idx = 0;
        ref.obj_id = 0;
        ref.guid = sel_deps_->getGuid(res_idx++);
        ref.name = sel_deps_->getString(res_idx++);
        ref.uri_namespace = sel_deps_->getString(res_idx++);
        ref.uri_prefix    = sel_deps_->getString(res_idx++);
        ref.uri_etp_ml    = sel_deps_->getString(res_idx++);
        ref.uri_ml_version = sel_deps_->getString(res_idx++);
        ref.xml_type      = sel_deps_->getString(res_idx++);
        ref.version       = sel_deps_->getString(res_idx++);
        ref.created       = sel_deps_->getInt64(res_idx++);
        ref.updated       = sel_deps_->getInt64(res_idx++);
        ref.created_by    = sel_deps_->getString(res_idx++);
        ref.updated_by    = sel_deps_->getString(res_idx++);
        ref.xdata_json    = sel_deps_->getString(res_idx++);
        ref.xdata_avro    = sel_deps_->getString(res_idx++);
        ref.space_rel_id  = sel_deps_->getInt64(res_idx++);
        deps.push_back(ref);
    }
    sel_deps_->reset();
    return deps;
}

/*!
 * Get the Relation information where the given resources are the source
*/
std::vector<ReferenceRel> SpaceReader::get_relations( const std::vector<TypedUid>& srcs, size_t bulk_size )
{
    if(!sel_rels_){
        prepare(sel_rels_, R"sql(
SELECT uri1.ml || '.' || typ1.xml, res1.guid, res1.version,
       uri2.ml || '.' || typ2.xml, res2.guid, res2.version, xpa.th, rel.cnt
  FROM UNNEST($1::uuid[], $2::text[]) AS src(guid, etp_type)
  JOIN typ AS typ1 ON true
  JOIN uri AS uri1 ON uri1.id = typ1.uri_id AND uri1.ml || '.' || typ1.xml = src.etp_type
  JOIN res AS res1 ON res1.typ_id = typ1.id AND res1.guid = src.guid
  JOIN rel AS rel ON rel.obj_id = res1.obj_id
  JOIN res AS res2 ON rel.dst_id = res2.obj_id
  JOIN typ AS typ2 ON typ2.id = res2.typ_id
  JOIN uri AS uri2 ON uri2.id = typ2.uri_id
  JOIN xpa ON rel.xpa_id = xpa.id
        )sql"
        );
    }
    std::vector<oes::core::Guid> guids;
    std::vector<std::string> types;
    guids.reserve(srcs.size());
    types.reserve(srcs.size());
    for (const auto& [etp_type, guid] : srcs) {
        guids.push_back(guid);
        types.push_back(etp_type);
    }

    std::vector<ReferenceRel> rels;
    const size_t chunk = std::max<size_t>(bulk_size, 1);
    for (size_t start = 0; start < srcs.size(); start += chunk) {
        const size_t end = std::min(start + chunk, srcs.size());
        sel_rels_->bind<oes::core::Guid>(1, guids.cbegin() + start, guids.cbegin() + end);
        sel_rels_->bindTextArray(2, types.cbegin() + start, types.cbegin() + end);
        while(sel_rels_->step()){
            ReferenceRel ref;
            int res_idx = 0;
            ref.src_type = sel_rels_->getString(res_idx++);
            ref.src_guid = sel_rels_->getGuid(res_idx++);
            ref.src_version = sel_rels_->getString(res_idx++);
            ref.dst_type = sel_rels_->getString(res_idx++);
            ref.dst_guid = sel_rels_->getGuid(res_idx++);
            ref.dst_version = sel_rels_->getString(res_idx++);
            ref.xpath = sel_rels_->getString(res_idx++);
            ref.cnt = sel_rels_->getInt(res_idx++);
            rels.push_back(ref);
        }
        sel_rels_->reset();
    }
    return rels;
}

oes::postgresql::Statement_ptr SpaceReader::get_get_object_stmt(oes::postgresql::Connection_ptr eml_db) {
    std::string sql = R"sql(
SELECT xml, name, createdBy,
       COALESCE(createdAt, 0),
       COALESCE(updatedAt, 0),
       (EXTRACT(EPOCH FROM (storeCreated AT TIME ZONE 'UTC'))*1000000)::bigint,
       (EXTRACT(EPOCH FROM (storeUpdated AT TIME ZONE 'UTC'))*1000000)::bigint,
       space_rel_id
  FROM res
  JOIN obj ON obj.id = res.obj_id
 WHERE typ_id =
       (
        SELECT typ.id
          FROM typ
          JOIN uri ON uri.id = typ.uri_id
         WHERE uri.ml || '.' || typ.xml = $1
       )
   AND guid = $2
)sql";
    boost::trim(sql);
    return std::make_shared<oes::postgresql::Statement>(eml_db->prepare(std::move(sql)));
}

std::vector<BulkObj_t> SpaceReader::bulk_get_obj(
    const std::vector<oes::core::Guid>& uids,
    const std::vector<std::string>& typ,
    const std::vector<int>& indexes,
    size_t bulk_size) {
    assert(uids.size() == typ.size());
    assert(uids.size() == indexes.size());

    if (uids.size() < bulk_size) {
        bulk_size = uids.size();
    }
    auto remaining = static_cast<long long>(uids.size());

    auto it_uid_beg = uids.cbegin();
    auto it_typ_beg = typ.cbegin();
    auto it_idx_beg = indexes.cbegin();

    std::vector<BulkObj_t> res_arr;
    while (remaining > 0) {
        if (remaining < static_cast<long long>(bulk_size)) { 
            bulk_size = static_cast<size_t>(remaining);
        }

        //!std::vector<std::string> typ_bulk(it_typ_beg, it_typ_beg + bulk_size);
        //!std::vector<oes::core::Guid> uid_bulk(it_uid_beg, it_uid_beg + bulk_size);
        //!std::vector<int> idx_bulk(it_idx_beg, it_idx_beg + bulk_size);

        //! bulk_sel_obj_->bind<std::string>(1, it_typ_beg, it_typ_beg + bulk_size);
        bulk_sel_obj_->bindTextArray(1, it_typ_beg, it_typ_beg + bulk_size);
        bulk_sel_obj_->bind<oes::core::Guid>(2, it_uid_beg, it_uid_beg + bulk_size);
        bulk_sel_obj_->bind<int>(3, it_idx_beg, it_idx_beg + bulk_size);

        size_t n_processed {0};
        while (bulk_sel_obj_->step()) {
            int res_idx = 0;
            BulkObj_t res;

            res.typ_ml_str = bulk_sel_obj_->getString(res_idx++); // tgt.typ_ml
            res.guid = bulk_sel_obj_->getGuid(res_idx++); // tgt.guid
            res.idx = static_cast<size_t>(bulk_sel_obj_->getInt64(res_idx++)); // tgt.typ_idx

            res.found = !bulk_sel_obj_->isNull(res_idx++); // obj.id
            if (res.found) {
                res.xml = bulk_sel_obj_->getString(res_idx++); // r.xml
                res.name = bulk_sel_obj_->getString(res_idx++); // r.name
                res.createdBy = bulk_sel_obj_->getString(res_idx++); // r.createdBy
                res.createdAt = bulk_sel_obj_->getInt64(res_idx++); // r.createdAt
                res.updatedAt = bulk_sel_obj_->getInt64(res_idx++); // r.updatedAt
                res.storeCreated = bulk_sel_obj_->getInt64(res_idx++); // r.storeCreated
                res.storeUpdated = bulk_sel_obj_->getInt64(res_idx++); // r.storeUpdated
                res.space_rel_id = bulk_sel_obj_->getInt64(res_idx++); // r.space_rel_id
            }

            res_arr.push_back(std::move(res));
            ++n_processed;
        }
        bulk_sel_obj_->reset();

        assert(bulk_size == n_processed);

        it_typ_beg += bulk_size;
        it_uid_beg += bulk_size;
        it_idx_beg += bulk_size;
        remaining -= static_cast<decltype(remaining)>(bulk_size);
    }

    return res_arr;
}

std::vector<AryVal_t> SpaceReader::bulk_get_ary_val(
    const std::vector<eml_int64>& ary_ids,
    const std::vector<int>& indexes,
    size_t bulk_size) {
    assert(ary_ids.size() == indexes.size());

    if (ary_ids.size() < bulk_size) {
        bulk_size = ary_ids.size();
    }

    auto remaining = static_cast<long long>(ary_ids.size());


    auto it_ary_ids_beg = ary_ids.cbegin();
    auto it_idx_beg = indexes.cbegin();

    std::vector<AryVal_t> res_arr;
    while (remaining > 0) {
        if (remaining < static_cast<long long>(bulk_size)) { 
            bulk_size = static_cast<size_t>(remaining);
        }

        //!std::vector<eml_int64> ary_idx_bulk(it_ary_ids_beg, it_ary_ids_beg + bulk_size);
        //!std::vector<int> idx_bulk(it_idx_beg, it_idx_beg + bulk_size);

        bulk_sel_ary_val_->bind<eml_int64>(1, it_ary_ids_beg, it_ary_ids_beg + bulk_size);
        bulk_sel_ary_val_->bind<int>(2, it_idx_beg, it_idx_beg + bulk_size);

        size_t n_processed {0};
        while (bulk_sel_ary_val_->step()) {
            int col_idx = 0;
            AryVal_t res;

            res.ary_sql_idx = bulk_sel_ary_val_->getInt(col_idx++);
                
            
            int size = 0;
            const void* temp_val = bulk_sel_ary_val_->getBlob(col_idx++, size);
            if (temp_val) {
                res.bytes.resize(size);
                std::memcpy(reinterpret_cast<unsigned char*>(res.bytes.data()), reinterpret_cast<const char*>(temp_val) /*+ src_off*/, size);
            } else {
                res.err = "Cannot read blob";
            }

            res_arr.push_back(res);
            ++n_processed;
        }
        bulk_sel_ary_val_->reset();

        assert(n_processed == bulk_size);

        it_ary_ids_beg += bulk_size;
        it_idx_beg += bulk_size;
        remaining -= static_cast<decltype(remaining)>(bulk_size);
    }

    return res_arr;
}


std::vector<BlobData_t> SpaceReader::bulk_get_bin_val(
        const std::vector<eml_int64>& ary_ids,
        const std::vector<int>& bin_idx,
        const std::vector<int>& indexes,
        size_t bulk_size
    ) {

    assert(ary_ids.size() == indexes.size());

    if (ary_ids.size() < bulk_size) {
        bulk_size = ary_ids.size();
    }

    auto remaining = static_cast<long long>(ary_ids.size());


    auto it_ary_ids_beg = ary_ids.cbegin();
    auto it_bin_idx_beg = bin_idx.cbegin();
    auto it_idx_beg = indexes.cbegin();

    std::vector<BlobData_t> res_arr;
    res_arr.reserve(ary_ids.size());

    while (remaining > 0) {
        if (remaining < static_cast<long long>(bulk_size)) { 
            bulk_size = static_cast<size_t>(remaining);
        }

        //!std::vector<eml_int64> ary_ids_bulk(it_ary_ids_beg, it_ary_ids_beg + bulk_size);
        //!std::vector<int> bin_idx_bulk(it_bin_idx_beg, it_bin_idx_beg + bulk_size);
        //!std::vector<int> idx_bulk(it_idx_beg, it_idx_beg + bulk_size);

        bulk_sel_bin_val_->bind<eml_int64>(1, it_ary_ids_beg, it_ary_ids_beg + bulk_size);
        bulk_sel_bin_val_->bind<int>(2, it_bin_idx_beg, it_bin_idx_beg + bulk_size);
        bulk_sel_bin_val_->bind<int>(3, it_idx_beg, it_idx_beg + bulk_size);

        size_t n_processed {0};
        while (bulk_sel_bin_val_->step()) {
            int col_idx = 0;
            BlobData_t res;

            res.ary_sql_idx = bulk_sel_bin_val_->getInt(col_idx++);
            res.bin_id = bulk_sel_bin_val_->getInt64(col_idx++);
            res.chunk_idx = bulk_sel_bin_val_->getInt(col_idx++);
            res.blob_len = bulk_sel_bin_val_->getInt(col_idx++);
            
            int size = 0;
            const void* temp_val = bulk_sel_bin_val_->getBlob(col_idx++, size);
            res.bytes.resize(size);
            if (temp_val) {
                std::memcpy(reinterpret_cast<unsigned char*>(res.bytes.data()), reinterpret_cast<const char*>(temp_val) /*+ src_off*/, size);
            } else { // sparse arrays
                std::memset(res.bytes.data(), 0, size); 
            }
            // assert(size == blob.blob_len);
            // res.bytes.reserve(size);
            // std::memcpy(reinterpret_cast<unsigned char*>(res.bytes.data()), reinterpret_cast<const char*>(temp_val) /*+ src_off*/, size);

            res_arr.push_back(std::move(res));
            ++n_processed;
        }
        bulk_sel_bin_val_->reset();

        // n_processed may be less than bulk_size for sparse arrays (missing chunks are OK)
        // OES_RELEASE_UNUSED(n_processed);
        //!assert(n_processed == bulk_size);

        it_ary_ids_beg += bulk_size;
        it_bin_idx_beg += bulk_size;
        it_idx_beg += bulk_size;
        remaining -= static_cast<decltype(remaining)>(bulk_size);
    }

    return res_arr;
}

//============================================================================

// WARNING: For now, internal caches are never reset, so to be safe,
// use a new instance per transaction.
SpaceWriter::SpaceWriter(
    oes::postgresql::Connection_ptr eml_db
) : SpaceCommon(std::move(eml_db))
{
    // Prep all statements up-front, thus detecting schema issues early
    prepare(ins_uri_, R"sql(
INSERT INTO uri (ns, pp, ml, vs)
VALUES ($1, $2, $3, $4)
RETURNING id
)sql");
    prepare(ins_typ_, R"sql(
INSERT INTO typ (id, uri_id, xml)
VALUES (DEFAULT, $1, $2)
RETURNING id
)sql");
    prepare(ins_xpa_, R"sql(
INSERT INTO xpa (th)
VALUES ($1)
RETURNING id
)sql");
    prepare(bulk_ins_xpa_, R"sql(
INSERT INTO xpa (th)
SELECT u.xpath
  FROM UNNEST(
    $1::text[]
  ) AS u(xpath)
RETURNING xpa.id, xpa.th
)sql");
    prepare(ins_obj_, R"sql(
INSERT INTO obj (id, xml)
VALUES ($1, $2)
RETURNING id
)sql");
    prepare(bulk_ins_obj_, R"sql(
INSERT INTO obj (id, xml)
SELECT 
    u.obj_id,
    u.xml::xml
  FROM UNNEST(
  $1::bigint[],
  $2::text[]
  ) AS u(obj_id, xml) 
RETURNING obj.id
)sql");
    prepare(upd_obj_, R"sql(
UPDATE obj
SET xml = $1
WHERE id = $2
)sql");
    prepare(del_null_rel_, R"sql(
DELETE FROM rel
 USING res
 WHERE rel.obj_id = res.obj_id
   AND res.guid IS NULL
)sql");
    prepare(del_null_res_, R"sql(
DELETE FROM res
 WHERE guid IS NULL
)sql");
    prepare(del_del_res_, R"sql(
DELETE FROM del_res
 WHERE guid = $1
   AND typ_id = (
        SELECT typ.id
          FROM typ
          JOIN uri ON uri.id = typ.uri_id
         WHERE uri.ml || '.' || typ.xml = $2
       )
)sql");
    prepare(del_rel_, R"sql(
DELETE FROM rel
 WHERE obj_id = $1
)sql");
    prepare(del_ary_, R"sql(
DELETE FROM ary
 WHERE id = $1
)sql");
    prepare(del_bin_, R"sql(
DELETE FROM bin
 WHERE ary_id = $1
)sql");
    prepare(sel_ary_id_, R"sql(
SELECT id
  FROM ary
 WHERE epc_guid = $1
   AND path = $2
)sql");
    // statement to set guid null in res table
    prepare(null_res_uuid_, R"sql(
UPDATE res
   SET guid = NULL
 WHERE obj_id = $1
)sql");
    // statement to null guids in bulk
    prepare(bulk_null_res_uuid_, R"sql(
UPDATE res
   SET guid = NULL
 WHERE obj_id = ANY($1::bigint[])
)sql");
    prepare(add_del_res_, R"sql(
INSERT INTO del_res (
    guid,
    typ_id,
    name,
    version,
    createdAt, updatedAt,
    createdBy, updatedBy,
    xdata_json,
    xdata_avro,
    storeCreated, storeUpdated, storeDeleted
    )
SELECT
    guid,
    typ_id,
    name,
    version,
    createdAt, updatedAt,
    createdBy, updatedBy,
    xdata_json,
    xdata_avro,
    storeCreated, storeUpdated, now()
  FROM res
 WHERE obj_id = $1
   AND guid IS NOT NULL ON CONFLICT ON CONSTRAINT del_res_guid_typ_id_key DO NOTHING
)sql");
    prepare(batch_add_del_res_, R"sql(
INSERT INTO del_res (
    guid,
    typ_id,
    name,
    version,
    createdAt, updatedAt,
    createdBy, updatedBy,
    xdata_json,
    xdata_avro,
    storeCreated, storeUpdated, storeDeleted
    )
SELECT
    guid,
    typ_id,
    name,
    version,
    createdAt, updatedAt,
    createdBy, updatedBy,
    xdata_json,
    xdata_avro,
    storeCreated, storeUpdated, now()
  FROM res
 WHERE obj_id = ANY($1::bigint[])
   AND guid IS NOT NULL ON CONFLICT ON CONSTRAINT del_res_guid_typ_id_key DO NOTHING
)sql");
    prepare(upd_dst_id_, R"sql(
UPDATE rel
SET dst_id = $1
WHERE dst_id = $2
)sql");
    prepare(bulk_upd_dst_id_, R"sql(
UPDATE rel AS r
   SET dst_id = u.new_id
  FROM UNNEST(
    $1::bigint[], -- new_id
    $2::bigint[]  -- old_id
  ) AS u(new_id, old_id)
WHERE u.old_id > -1 AND dst_id = u.old_id 
)sql");

    // Uses rel_by_dst non-UNIQUE index.
    prepare(sel_rel_by_dst_, R"sql(
SELECT id, obj_id
  FROM rel
 WHERE dst_id = $1
)sql");

    prepare(sel_obj_id_, R"sql(
SELECT obj_id
  FROM res
 WHERE typ_id = $1
   AND guid = $2
)sql"
    );
    
    prepare(bulk_sel_obj_id_, R"sql(
SELECT tgt.idx, r.obj_id
  FROM res AS r
  JOIN UNNEST(
    $1::int[],
    $2::uuid[],
    $3::int[]
    ) AS tgt(typ, guid, idx)
      ON r.typ_id = tgt.typ
        AND r.guid = tgt.guid
)sql"
    );

    prepare(sel_obj_id_xml_, R"sql(
SELECT obj_id,
       xml
  FROM res
  JOIN obj ON id = res.obj_id
 WHERE typ_id = $1
   AND guid = $2
)sql"
    );

    // For julianday() calculation below, see
    // https://sqlite.org/forum/forumpost/8742abd5bb
    prepare(ins_res_, R"sql(
INSERT INTO res (
    typ_id,
    guid,
    name,
    version,
    createdAt,
    updatedAt,
    createdBy,
    updatedBy,
    xdata_json,
    xdata_avro,
    space_rel_id
) VALUES (
    $1, $2, $3, $4,
    $5, $6,
    $7, $8, $9, $10, $11
) RETURNING obj_id
)sql"
    );

    prepare(ins_res_no_ovwrt_, R"sql(
INSERT INTO res (
    typ_id,
    guid,
    name,
    version,
    createdAt,
    updatedAt,
    createdBy,
    updatedBy,
    xdata_json,
    xdata_avro,
    space_rel_id
) VALUES (
    $1, $2, $3, $4,
    $5, $6,
    $7, $8, $9, $10, $11
) ON CONFLICT DO NOTHING 
RETURNING obj_id
)sql"
    );
    prepare(bulk_ins_res_, R"sql(
INSERT INTO res (
    typ_id,
    guid,
    name,
    version,
    createdAt,
    updatedAt,
    createdBy,
    updatedBy,
    xdata_json,
    xdata_avro,
    space_rel_id
) SELECT
    u.typ_id, -- $1
    u.guid, -- $2
    u.name, -- $3
    u.version, -- $4
    NULLIF(u.createdAt, -1), -- $5
    NULLIF(u.updatedAt, -1), -- $6
    u.createdBy, -- $7
    u.updatedBy, -- $8
    NULLIF(u.xdata_json::text, '')::json, -- $9
    NULLIF(u.xdata_avro, ''::bytea), -- $10
    NULLIF(u.space_rel_id, -1) -- $11
 FROM UNNEST(
    $1::int[], -- typ_id
    $2::uuid[], -- guid
    $3::text[], -- name
    $4::text[], -- version
    $5::bigint[], -- createdAt
    $6::bigint[], -- updatedAt
    $7::text[], -- createdBy
    $8::text[], -- updatedBy
    $9::text[], -- xdata_json
    $10::bytea[], -- xdata_avro
    $11::bigint[] -- space_rel_id
    --------------------$12::int[] -- bulk_idx
) AS u(typ_id, guid, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, space_rel_id)
 RETURNING res.obj_id, res.guid, res.typ_id
)sql"
    );

    // Duplicates are common obj_WellboreMarkerFrameRepresentation with
    // xpath rsq:WellboreMarker/rsq:Interpretation, because can contain
    // many markers, which can have the same interpretations.
    prepare(ins_rel_, R"sql(
INSERT INTO rel (
    obj_id,
    dst_id,
    xpa_id
) VALUES (
    $1, $2, $3
) ON CONFLICT (obj_id, dst_id, xpa_id) -- UPSERT
     DO UPDATE SET cnt = rel.cnt + 1
)sql"
    );

    // GROUP BY folds duplicates: an upsert cannot update the same row twice.
    prepare(bulk_ins_rel_, R"sql(
INSERT INTO rel (obj_id, dst_id, xpa_id, cnt)
SELECT u.obj_id, u.dst_id, u.xpa_id, count(*)
  FROM UNNEST(
    $1::bigint[],
    $2::bigint[],
    $3::bigint[]
  ) AS u(obj_id, dst_id, xpa_id)
 GROUP BY u.obj_id, u.dst_id, u.xpa_id
    ON CONFLICT (obj_id, dst_id, xpa_id)
    DO UPDATE SET cnt = rel.cnt + EXCLUDED.cnt
)sql"
    );

    prepare(ins_ary_, R"sql(
INSERT INTO ary (
    obj_id,
    xpa_id,
    epc_guid,
    path,
    type,
    rank1,
    dim1,
    dim2,
    dim3,
    dim4,
    usize,
    value,
    chunk_length
) VALUES (
    $1, $2, $3, $4, $5,
    $6, $7, $8, $9, $10,
    $11,
    $12, $13
) RETURNING id
)sql"
    );
    
    prepare(bulk_ins_ary_, R"sql(
INSERT INTO ary AS a (
    obj_id,
    xpa_id,
    epc_guid,
    path,
    type,
    rank1,
    dim1,
    dim2,
    dim3,
    dim4,
    usize,
    value,
    chunk_length
)
SELECT 
    NULLIF(u.obj_id, -1), NULLIF(u.xpa_id, -1),
    u.epc_guid,
    u.path,
    u.type,
    u.rank1,
    NULLIF(u.dim1, -1), NULLIF(u.dim2, -1), NULLIF(u.dim3, -1), NULLIF(u.dim4, -1),
    NULLIF(u.usize, -1),
    u.value,
    NULLIF(u.chunk_length, -1)
 FROM UNNEST(
    $1::bigint[], -- obj_id
    $2::bigint[], -- xpa_id
    $3::uuid[], -- epc_guid
    $4::text[], -- path
    $5::int[], -- type
    $6::int[], -- rank1
    $7::bigint[], -- dim1
    $8::bigint[], -- dim2
    $9::bigint[], -- dim3
    $10::bigint[], -- dim4
    $11::bigint[], -- usize
    $12::bytea[], -- value
    $13::int[] -- chunk_length
) AS u(obj_id, xpa_id, epc_guid, path, type, rank1, dim1, dim2, dim3, dim4, usize, value, chunk_length)
RETURNING a.id
)sql"
    );

    prepare(ups_ary_, R"sql(
INSERT INTO ary (
    obj_id,
    xpa_id,
    epc_guid,
    path,
    type,
    rank1,
    dim1,
    dim2,
    dim3,
    dim4,
    usize,
    value,
    chunk_length
) VALUES (
    $1, $2, $3, $4, $5,
    $6, $7, $8, $9, $10,
    $11,
    $12, $13
) ON CONFLICT (epc_guid, path) -- UPSERT
     DO UPDATE SET
         obj_id = excluded.obj_id,
         xpa_id = excluded.xpa_id,
         type = excluded.type,
         rank1 = excluded.rank1,
         dim1 = excluded.dim1,
         dim2 = excluded.dim2,
         dim3 = excluded.dim3,
         dim4 = excluded.dim4,
         usize = excluded.usize,
         value = excluded.value,
         chunk_length = excluded.chunk_length
    RETURNING id
)sql"
    );
    
    prepare(bulk_ups_ary_, R"sql(
INSERT INTO ary AS a(
    obj_id,
    xpa_id,
    epc_guid,
    path,
    type,
    rank1,
    dim1,
    dim2,
    dim3,
    dim4,
    usize,
    value,
    chunk_length
)
SELECT 
    NULLIF(u.obj_id, -1), NULLIF(u.xpa_id, -1),
    u.epc_guid,
    u.path,
    u.type,
    u.rank1,
    NULLIF(u.dim1, -1), NULLIF(u.dim2, -1), NULLIF(u.dim3, -1), NULLIF(u.dim4, -1),
    NULLIF(u.usize, -1),
    u.value,
    NULLIF(u.chunk_length, -1)
  FROM UNNEST(
    $1::bigint[], -- obj_id
    $2::bigint[], -- xpa_id
    $3::uuid[], -- epc_guid
    $4::text[], -- path
    $5::int[], -- type
    $6::int[], -- rank1
    $7::bigint[], -- dim1
    $8::bigint[], -- dim2
    $9::bigint[], -- dim3
    $10::bigint[], -- dim4
    $11::bigint[], -- usize
    $12::bytea[], -- value
    $13::int[] -- chunk_length
    ) AS u(obj_id, xpa_id, epc_guid, path, type, rank1, dim1, dim2, dim3, dim4, usize, value, chunk_length)
  ON CONFLICT (epc_guid, path) -- UPSERT
     DO UPDATE SET
         obj_id = EXCLUDED.obj_id,
         xpa_id = EXCLUDED.xpa_id,
         type = EXCLUDED.type,
         rank1 = EXCLUDED.rank1,
         dim1 = EXCLUDED.dim1,
         dim2 = EXCLUDED.dim2,
         dim3 = EXCLUDED.dim3,
         dim4 = EXCLUDED.dim4,
         usize = EXCLUDED.usize,
         value = EXCLUDED.value,
         chunk_length = EXCLUDED.chunk_length
RETURNING a.id, a.epc_guid, a.path
)sql"
    ); // bulk_ups_ary_

    prepare(ins_bin_, R"sql(
INSERT INTO bin (ary_id, idx, value)
VALUES ($1, $2, $3)
ON CONFLICT (ary_id, idx) DO UPDATE SET value = EXCLUDED.value
RETURNING id
)sql"
    );
    prepare(bulk_ins_bin_, R"sql(
INSERT INTO bin AS b(ary_id, idx, value)
SELECT 
    u.ary_id,
    u.idx,
    u.value
  FROM UNNEST(
    $1::bigint[], -- ary_id
    $2::int[], -- chunk_idx
    $3::bytea[] -- value
    ------------------------------$4::int[] -- req_idx for control
) AS u(ary_id, idx, value)
ON CONFLICT (ary_id, idx) DO UPDATE SET
    value = EXCLUDED.value
RETURNING b.id
)sql"
    );
    prepare(upd_ary_, R"sql(
UPDATE ary
   SET obj_id = $1,
       xpa_id = $2
 WHERE id = $3
)sql"
    );
    prepare(bulk_upd_ary_, R"sql(
UPDATE ary AS a
   SET obj_id = u.obj_id,
       xpa_id = u.xpa_id
  FROM UNNEST(
    $1::bigint[],
    $2::bigint[],
    $3::bigint[]
  ) AS u(obj_id, xpa_id, ary_id)
 WHERE id = u.ary_id
)sql"
    );
    /*
    UPDATE ary AS a
     SET value = u.value
    FROM UNNEST(
        $1::bigint[],
        $2::bytea[],
        $3::int[]
    ) AS u(id, value, req_idx)
 WHERE a.id = u.id
 RETURNING u.req_idx
)sql");
    */

    prepare(res_check_, R"sql(
SELECT FROM res
  JOIN typ ON typ.id = res.typ_id
  JOIN uri ON uri.id = typ.uri_id
 WHERE res.guid = $1
   AND uri.ml || '.' || typ.xml = $2
)sql"
    );
    prepare(bulk_res_check_, R"sql(
SELECT tgt.idx
  FROM res AS r
  JOIN typ ON typ.id = r.typ_id
  JOIN uri ON uri.id = typ.uri_id
  JOIN UNNEST(
    $1::uuid[],
    $2::text[],
    $3::int[]
    ) AS tgt(guid, etp_type, idx)
      ON r.guid = tgt.guid
     AND uri.ml || '.' || typ.xml = tgt.etp_type
)sql"
    );
    // Both spellings, as is_ext_part_ref(); no ML filter, older spaces may hold external parts under resqml20
    prepare(res_guid_check_, R"sql(
SELECT FROM res
  JOIN typ ON typ.id = res.typ_id
 WHERE res.guid = $1
   AND typ.xml LIKE '%EpcExternalPartReference'
)sql"
    );

    prepare(sel_ary_ids_, R"sql(
SELECT id,
       path,
       xpa_id
  FROM ary
 WHERE obj_id = $1
 ORDER BY path COLLATE "C"
)sql"
    );

    prepare(sel_arr_obj_, R"sql(
SELECT id,
       obj_id,
       xpa_id
  FROM ary
 WHERE epc_guid = $1
   AND path = $2
)sql");
    prepare(bulk_sel_ary_obj_, R"sql(
SELECT a.epc_guid, a.path, a.id, COALESCE(a.obj_id,-1), COALESCE(a.xpa_id,-1)
  FROM ary AS a
  JOIN UNNEST($1::uuid[], $2::text[]) AS tgt(epc_guid, path)
    ON a.epc_guid = tgt.epc_guid
   AND a.path = tgt.path
)sql");
    prepare(upd_ary_val_, R"sql(
UPDATE ary
   SET value = $1
 WHERE id = $2
)sql");
        prepare(bulk_upd_ary_val_, R"sql(
UPDATE ary AS a
     SET value = u.value
    FROM UNNEST(
        $1::bigint[],
        $2::bytea[],
        $3::int[]
    ) AS u(id, value, req_idx)
 WHERE a.id = u.id
 RETURNING u.req_idx
)sql");
    prepare(upd_bin_val_, R"sql(
UPDATE bin
   SET value = $1
 WHERE id = $2
)sql");
    prepare(sel_rel_any_uuids_, R"sql(
SELECT uri.ml || '.' || typ.xml,
       res.guid::text
  FROM (
        WITH rescache AS (
            SELECT res.obj_id
              FROM res
              JOIN typ ON typ.id = res.typ_id
              JOIN uri ON uri.id = typ.uri_id
              JOIN UNNEST($1::uuid[], $2::text[]) AS seed(guid, etp_type)
                ON seed.guid = res.guid
               AND seed.etp_type = uri.ml || '.' || typ.xml
        )
        SELECT obj_id,
               MIN(level) AS level
          FROM (
                WITH RECURSIVE children_of(obj_id, level) AS (
                    SELECT obj_id,
                           0 AS level
                      FROM rescache
                    UNION
                    SELECT dst_id,
                           level + 1
                      FROM rel
                      JOIN children_of c ON c.obj_id = rel.obj_id
                     WHERE level < 10
                )
                SELECT obj_id,
                       level
                  FROM children_of
               ) c
         GROUP BY obj_id
       ) AS ids
  JOIN res ON res.obj_id = ids.obj_id
  JOIN typ ON typ.id = res.typ_id
  JOIN uri ON uri.id = typ.uri_id
)sql");

    // Note that 0 and 1 are stored more efficiently in SQLite than >1,
    // and that uri_of() and typ_of() generate IDs starting at 1, thus
    // the insert logic below (i.e. force 0 and 1 when empty)

    // pre-load URIs, TYPs, XPAs. OBJs pre-load is postponed (lazy),
    // since can be much larger than the above ables, and may not be necessary.
    {
        oes::postgresql::Statement stmt = db_->prepare("SELECT id, ns, pp, ml, vs FROM uri");
        while (stmt.step()) {
            eml_int64 id = stmt.getInt64(0);
            std::string ns = stmt.getString(1);
            std::string pp = stmt.getString(2);
            std::string ml = stmt.getString(3);
            std::string vs = stmt.getString(4);
            if(ml=="resqml20"){
                rsq20_id_ = id;
            }else if(ml=="resqml22"){
                rsq22_id_ = id;
            }else if(ml=="eml20"){
                eml20_id_ = id;
            }else if(ml=="eml23"){
                eml23_id_ = id;
            }else if(ml=="witsml21"){
                wit21_id_ = id;
            }else if(ml=="prodml22"){
                prd22_id_ = id;
            }else if(ml=="prodml23"){
                prd23_id_ = id;
            }
            uri_map_.emplace(UriKey(ns,vs),  id);
        }
        stmt.reset();
    }
    {
        oes::postgresql::Statement stmt = db_->prepare("SELECT id, uri_id, xml FROM typ");
        while (stmt.step()) {
            int id = static_cast<int>(stmt.getInt64(0));
            int uri_id = static_cast<int>(stmt.getInt64(1));
            std::string xml = stmt.getString(2);
            typ_map_.emplace(TypKey{ uri_id, std::move(xml) }, id);
        }
        stmt.reset();
    }
    {
        oes::postgresql::Statement stmt = db_->prepare("SELECT id, th FROM xpa");
        while (stmt.step()) {
            int id = static_cast<int>(stmt.getInt64(0));
            std::string th = stmt.getString(1);
            xpa_map_.emplace(std::move(th), id);
        }
        stmt.reset();
    }

    const std::string RSQ("http://www.energistics.org/energyml/data/resqmlv2");
    const std::string EML("http://www.energistics.org/energyml/data/commonv2");
    const std::string WIT("http://www.energistics.org/energyml/data/witsmlv2");
    const std::string PRD("http://www.energistics.org/energyml/data/prodmlv2");

    // "Prime" uri_map_ with most often used URIs
    // pre-record special RESQML and EML URI IDs
    const bool empty = uri_map_.empty();
    if (empty) {
        rsq20_id_ = uri_of(RSQ, "rsq", "resqml20", "2.0");
        rsq22_id_ = uri_of(RSQ, "rsq22", "resqml22", "2.2");
        eml20_id_ = uri_of(EML, "eml", "eml20", "2.0");
        eml23_id_ = uri_of(EML, "eml23", "eml23", "2.3");
        wit21_id_ = uri_of(WIT, "wit21", "witsml21", "2.1");
        prd22_id_ = uri_of(PRD, "prd22", "prodml22", "2.2");
        prd23_id_ = uri_of(PRD, "prd23", "prodml23", "2.3");
    }
}

SpaceWriter::~SpaceWriter() {
    close_blob();
}

/*!
 * Cache the resource information for the dataspace 
*/
bool SpaceWriter::fill_res_map() {
    if (filled_res_map_) {
        return false; // 1-time "pre-load"
    }
    const size_t before = res_map_.size();
    // Nulled rows (updated objects awaiting delete_null_res) would be cached under an invalid guid
    oes::postgresql::Statement stmt = db_->prepare("SELECT obj_id, typ_id, guid FROM res WHERE guid IS NOT NULL");
    while (stmt.step()) {
        eml_int64 obj_id = stmt.getInt64(0);
        int typ_id = static_cast<int>(stmt.getInt64(1));

        const Guid uid = stmt.getGuid(2);
        res_map_.emplace(ResKey{ typ_id, uid }, obj_id);
    }
    stmt.reset();
    filled_res_map_ = true;
    return res_map_.size() > before;
}

/*!
 * Return a user-friendly string representation of the reference information
 * \return A string containing the reference information
*/
std::string ReferenceInfo::to_string() const{
    return fmt::format("from {}({}) to {}({}) using {}", orig_type, orig_guid.toXmlString(), dest_type, dest_guid.toXmlString(), xpath);
}


/*!
* Get user-friendly reference information for the given references
* \return A vector of ReferenceInfo objects containing the references content as strings
*/
std::vector<ReferenceInfo> SpaceWriter::get_references_infos(){
    std::vector<ReferenceInfo> res_keys;
    if(missing_refs_.empty()){
        return res_keys;
    }
    auto stmt = db_->prepare("SELECT typ_id, guid FROM res WHERE obj_id = $1");

    // Create the reverse map of typ_map_
    std::map<eml_int64, TypKey> typ_map_rev;
    for(auto& [key, val] : typ_map_){
        typ_map_rev[val] = key;
    }

    // Create the reverse map of xpa_map_
    std::map<eml_int64, std::string> xpa_map_rev;
    for(auto& [key, val] : xpa_map_){
        xpa_map_rev[val] = key;
    }

    // Create the reverse map of res_map_
    std::map<eml_int64, ResKey> res_map_rev;
    for(auto& [key, val] : res_map_){
        res_map_rev[val] = key;
    }

    for (const auto& [ref, _] : missing_refs_) {
        eml_int64 dest_typ_id = ref.res_key.first;
        auto it = res_map_rev.find(ref.obj_id);
        if(it == res_map_rev.end()){
            continue;
        }
        ResKey orig_res = it->second;
        if (typ_map_rev.find(orig_res.first) != typ_map_rev.end() && 
            typ_map_rev.find(dest_typ_id) != typ_map_rev.end() &&
            xpa_map_rev.find(ref.xpa_id) != xpa_map_rev.end()){
            res_keys.push_back({typ_map_rev[orig_res.first].second, orig_res.second, typ_map_rev[dest_typ_id].second, ref.res_key.second, xpa_map_rev[ref.xpa_id]});
        }
    }
    return res_keys;
}

/*
* Check if the given object exists in the resource table
* \param container The ETP type and uuid of the object to check
* \param in_transaction Whether the check is done in a transaction or not. If true, the object is checked at the end of the transaction
* \return true if the object exists, false otherwise
*/
bool SpaceWriter::check_ary_container(const TypedUid& container, bool in_transaction){
    if(in_transaction){
        array_containers_.emplace(container);
        return true;
    }else{
        return has_ary_container(container);
    }
}

/*
* Clients before (type, guid) unicity name EpcExternalPartReference after its XML element,
* i.e. without the obj_ of its xsi:type, so it is matched by guid among external parts of either spelling.
*/
bool SpaceWriter::has_ary_container(const TypedUid& container){
    const bool by_guid = is_ext_part_ref(container.first);
    auto& stmt = by_guid ? *res_guid_check_ : *res_check_;
    stmt.bind(1, container.second);
    if (!by_guid) {
        stmt.bind(2, container.first);
    }
    const bool has_row = stmt.step();
    stmt.reset();
    return has_row;
}

/*
* Check if all array containers used by the writer are present in the resource table.
* This needs to be called at the end of a transaction
* \return A vector of uuids of the missing array containers
*/
std::vector<std::string> SpaceWriter::check_pending_ary_containers(){
    std::vector<std::string> missing;
    for(auto& [etp_type, uuid] : array_containers_){
        if(!has_ary_container({etp_type, uuid})){
            missing.push_back(etp_type + '(' + uuid.toXmlString() + ')');
        }
    }
    return missing;
}

/*!
 * Bulk-check containers (objects, identified by ETP type and uuid, that own arrays).
 * In transaction mode: accepts all containers unconditionally (deferred to commit).
 * In non-transaction mode: queries the DB for all containers at once.
 * Returns, for each index, whether the container is valid / accepted.
 */
std::vector<bool> SpaceWriter::bulk_check_ary_containers(
    const std::vector<std::string>& types,
    const std::vector<oes::core::Guid>& uuids,
    const std::vector<int>& indexes,
    size_t bulk_size,
    bool in_transaction)
{
    assert(types.size() == uuids.size());
    assert(uuids.size() == indexes.size());
    std::vector<bool> found_map(indexes.size(), false); // each indexes[i] --> found_map[indexes[i]]

    if (in_transaction) {
        // Record them all as pending; actual validation happens at commit time
        for (size_t i{0}; i < uuids.size(); i++) {
            array_containers_.emplace(types.at(i), uuids.at(i));
            found_map.at(indexes.at(i)) = true;
        }
        
        return found_map;
    }

    // External parts are matched by guid alone, see has_ary_container()
    std::map<oes::core::Guid, bool> ext_parts_found;
    std::vector<std::string> typed_types;
    std::vector<oes::core::Guid> typed_uuids;
    std::vector<int> typed_indexes;
    for (size_t i{0}; i < uuids.size(); i++) {
        if (is_ext_part_ref(types.at(i))) {
            auto [it, inserted] = ext_parts_found.emplace(uuids.at(i), false);
            if (inserted) {
                it->second = has_ary_container({types.at(i), uuids.at(i)});
            }
            found_map.at(indexes.at(i)) = it->second;
        } else {
            typed_types.push_back(types.at(i));
            typed_uuids.push_back(uuids.at(i));
            typed_indexes.push_back(indexes.at(i));
        }
    }

    if (typed_uuids.size() < bulk_size) {
        bulk_size = typed_uuids.size();
    }
    auto remaining = static_cast<long long>(typed_uuids.size());
    
    auto it_types_beg = typed_types.cbegin();
    auto it_uuids_beg = typed_uuids.cbegin();
    auto it_idx_beg = typed_indexes.cbegin();

    while (remaining > 0) {
        if (remaining < static_cast<long long>(bulk_size)) {
            bulk_size = static_cast<size_t>(remaining);
        }

        //!std::vector<oes::core::Guid> uuid_bulk(it_uuids_beg, it_uuids_beg + bulk_size);
        //!std::vector<int> idx_bulk(it_idx_beg, it_idx_beg + bulk_size);

        bulk_res_check_->bind<oes::core::Guid>(1, it_uuids_beg, it_uuids_beg + bulk_size);
        bulk_res_check_->bindTextArray(2, it_types_beg, it_types_beg + bulk_size);
        bulk_res_check_->bind<int>(3, it_idx_beg, it_idx_beg + bulk_size);
        
        size_t n_processed {0};
        while (bulk_res_check_->step()) {
            int res_idx = 0;
            auto idx = bulk_res_check_->getInt(res_idx++);

            found_map.at(idx) = true;
            ++n_processed;
        }
        bulk_res_check_->reset();
        
        it_types_beg += bulk_size;
        it_uuids_beg += bulk_size;
        it_idx_beg += bulk_size;
        remaining -= static_cast<long long>(bulk_size);
    } 

    return found_map;
}

/*!
 * Bulk-lookup arrays by (container, path).
 * Returns a map of (epc_guid, path) -> AryObjInfo for every array that exists in the ary table.
 */
std::map<AryKey, SpaceWriter::AryObjInfo> SpaceWriter::bulk_select_ary_objects(
    const std::vector<oes::core::Guid>& epc_guids,
    const std::vector<std::string>& paths,
    size_t bulk_size)
{
    assert(epc_guids.size() == paths.size());
    std::map<AryKey, AryObjInfo> result;
    if (paths.empty()) {
        return result;
    }

    if (paths.size() < bulk_size) {
        bulk_size = paths.size();
    }
    
    auto remaining = static_cast<long long>(paths.size());
    auto it_guid_beg = epc_guids.cbegin();
    auto it_path_beg = paths.cbegin();
    auto& stmt = *bulk_sel_ary_obj_;

    while (remaining > 0) {
        if (remaining < static_cast<long long>(bulk_size)) { 
            bulk_size = static_cast<size_t>(remaining);
        }

        //!std::vector<std::string> path_bulk(it_path_beg, it_path_beg + bulk_size);

        //! stmt.bind<std::string>(1, it_path_beg, it_path_beg + bulk_size);
        stmt.bind<oes::core::Guid>(1, it_guid_beg, it_guid_beg + bulk_size);
        stmt.bindTextArray(2, it_path_beg, it_path_beg + bulk_size);
        while (stmt.step()) {
            AryKey key{ stmt.getGuid(0), stmt.getString(1) };
            AryObjInfo info;
            info.ary_id  = stmt.getInt64(2);
            info.obj_id  = stmt.getInt64(3); // COALESCE'd to -1
            info.xpa_id  = stmt.getInt64(4); // COALESCE'd to -1
            // Treat 0 as invalid (same as select_array_object)
            if (info.obj_id  == 0) info.obj_id  = -1;
            if (info.xpa_id  == 0) info.xpa_id  = -1;
            result.emplace(std::move(key), info);
        }
        stmt.reset();

        remaining -= static_cast<long long>(bulk_size);
        it_guid_beg += bulk_size;
        it_path_beg += bulk_size;

    }

    return result;
}

eml_int64 SpaceWriter::uri_of(
    const std::string& ns_uri,
    const std::string& ns_prefix,
    const std::string& etp_ml,
    const std::string& version
) {
    // Before EML2.3 (i.e RESQML2.2, WITSML2.1, PRODML2.2), the schemaVersion syntax was not standardized,
    // and most implementers used a dot separated format such as "2.0.1" or "2.0".
    // EML2.3 and later, the schemaVersion syntax is standardized to be the first 2 digits of an ML version (no dot separation),
    // e.g. "23" for EML2.3, "21" for WITSML2.1, "22" for RESQML2.2, etc...
    assert((version.size() == 2 && isdigit(version[0]) && isdigit(version[1])) ||
        (version.size() > 2 && isdigit(version[0]) && version[1] == '.' && isdigit(version[2])));
    std::string v;
    if (version.size() > 2) {
        std::vector<std::string> va;
        boost::split(va, version, boost::algorithm::is_any_of("."));
        v = va.size() > 2 ? va[0] + "." + va[1] : version;
    }
    else {
        v = {version[0], '.', version[1]};
    }
    const auto found = uri_map_.find(UriKey(ns_uri,v));
    if (found != uri_map_.end()) {
        return found->second;
    }

    auto& stmt = *ins_uri_;
    bindTextNoCopy(stmt, 1, ns_uri);
    bindTextNoCopy(stmt, 2, ns_prefix);
    bindTextNoCopy(stmt, 3, etp_ml);
    bindTextNoCopy(stmt, 4, v);
    stmt.execute();

    const eml_int64 uri_id = stmt.getInt64(0);
    stmt.reset();
    auto rc = uri_map_.emplace(UriKey(ns_uri,version), uri_id);
    OES_RELEASE_UNUSED(rc);
    assert(rc.second);
    return uri_id;
}

int SpaceWriter::typ_of(eml_int64 uri_id, const std::string& xml_type) {
    if (uri_id == rsq20_id_ && boost::ends_with(xml_type, "EpcExternalPartReference")) {
        // FIXUP namespace, otherwise end up with a mix, since
        // EpcExternalPartReference often incorrectly put in RESQML ns.
        uri_id = eml20_id_;
    }
    TypKey typ_key(uri_id, xml_type);
    const auto found = typ_map_.find(typ_key);
    if (found != typ_map_.end()) {
        return found->second;
    }

    auto& stmt = *ins_typ_;
    stmt.bind(1, int64_t(typ_key.first));
    bindTextNoCopy(stmt, 2, typ_key.second);
    stmt.execute();
    const int typ_id = stmt.getInt(0);
    stmt.reset();

    stmt.bind(2); // clear non-transient bindings
    auto rc = typ_map_.emplace(typ_key, typ_id);
    OES_RELEASE_UNUSED(rc);
    assert(rc.second);
    return typ_id;
}

eml_int64 SpaceWriter::xpa_of(const std::string& xpath) {
    const auto found = xpa_map_.find(xpath);
    if (found != xpa_map_.end()) {
        return found->second;
    }

    auto& stmt = *ins_xpa_;
    bindTextNoCopy(stmt, 1, xpath);
    stmt.execute();
    const eml_int64 xpa_id = stmt.getInt64(0);
    stmt.reset();

    stmt.bind(1); // clear non-transient bindings
    auto rc = xpa_map_.emplace(xpath, xpa_id);
    OES_RELEASE_UNUSED(rc);
    assert(rc.second);
    return xpa_id;
}

std::unordered_map<int, eml_int64> SpaceWriter::xpa_of(
        const std::vector<std::string>& xpath,
        const std::vector<int>& indexes,
        size_t bulk_size
    ) {

    assert(xpath.size() == indexes.size());

    // Insert each uncached xpath once: xpa.th is UNIQUE
    std::vector<std::string> xpath_to_add;
    std::unordered_set<std::string> seen;
    for (const auto& p : xpath) {
        if (xpa_map_.find(p) == xpa_map_.end() && seen.insert(p).second) {
            xpath_to_add.push_back(p);
        }
    }

    if (xpath_to_add.size() < bulk_size) {
        bulk_size = xpath_to_add.size();
    }

    auto remaining = xpath_to_add.size();

    auto it_xpath = xpath_to_add.cbegin();
    while(remaining > 0) {
        if (remaining < bulk_size) {
            bulk_size = remaining;
        }

        bulk_ins_xpa_->bindTextArray(1, it_xpath, it_xpath + bulk_size);

        while (bulk_ins_xpa_->step()) {
            auto xpa_id = bulk_ins_xpa_->getInt64(0);
            auto xpa_xpath = bulk_ins_xpa_->getString(1);
            xpa_map_.emplace(std::move(xpa_xpath), xpa_id);
        }
        bulk_ins_xpa_->reset();

        it_xpath += bulk_size;
        remaining -= bulk_size;
    }

    std::unordered_map<int, eml_int64> map;
    for (size_t i{0}; i < xpath.size(); i++) {
        map.emplace(indexes.at(i), xpa_map_.at(xpath.at(i)));
    }

    return map;
}


eml_int64 SpaceWriter::select_obj_id(
    int typ_id, const oes::core::Guid& uid
) {
    ResKey obj_key(typ_id, uid);
    auto obj_found = res_map_.find(obj_key);
    if (obj_found != res_map_.end()) {
        return obj_found->second; // in-cache
    }

    auto& stmt = *sel_obj_id_;

    stmt.bind(1, typ_id);
    stmt.bind(2,uid);

    if (!stmt.step()) {
        // not found
        stmt.reset();
        return -1;
    }

    eml_int64 obj_id = stmt.getInt64(0);
    assert(obj_id >= 0);
    stmt.reset();

    auto rc = res_map_.emplace(obj_key, obj_id);
    OES_RELEASE_UNUSED(rc);
    assert(rc.second);

    return obj_id;
}

std::unordered_map<int, eml_int64> SpaceWriter::bulk_select_obj_ids(
    const std::vector<int>& typ_ids,
    const std::vector<oes::core::Guid>& guids,
    const std::vector<int>& indexes,
    size_t bulk_size) {
    
    assert(typ_ids.size() == guids.size());
    assert(typ_ids.size() == indexes.size());

    // Same semantics as select_obj_id(): cache first, then DB for misses, caching DB hits.
    std::unordered_map<int, eml_int64> res;
    std::vector<int> miss_typ_ids;
    std::vector<oes::core::Guid> miss_guids;
    std::vector<int> miss_pos; // bound as tgt.idx, position in the miss_* vectors
    std::vector<int> miss_indexes;
    for (size_t i{0}; i < indexes.size(); i++) {
        auto found = res_map_.find(ResKey{ typ_ids.at(i), guids.at(i) });
        if (found != res_map_.end()) {
            res.emplace(indexes.at(i), found->second);
            continue;
        }
        res.emplace(indexes.at(i), -1);
        miss_pos.push_back(static_cast<int>(miss_typ_ids.size()));
        miss_typ_ids.push_back(typ_ids.at(i));
        miss_guids.push_back(guids.at(i));
        miss_indexes.push_back(indexes.at(i));
    }
    
    if (miss_typ_ids.size() < bulk_size) {
        bulk_size = miss_typ_ids.size();
    }
    auto remaining = static_cast<long long>(miss_typ_ids.size());
    
    auto it_typ_ids_beg = miss_typ_ids.cbegin();
    auto it_guids_beg = miss_guids.cbegin();
    auto it_idx_beg = miss_pos.cbegin();

    while (remaining > 0) {
        if (remaining < static_cast<long long>(bulk_size)) {
            bulk_size = remaining;
        }

        bulk_sel_obj_id_->bind<int>(1, it_typ_ids_beg, it_typ_ids_beg +  bulk_size);
        bulk_sel_obj_id_->bind<oes::core::Guid>(2, it_guids_beg, it_guids_beg +  bulk_size);
        bulk_sel_obj_id_->bind<int>(3, it_idx_beg, it_idx_beg +  bulk_size);

        while(bulk_sel_obj_id_->step()) {
            int res_idx = 0;
            const auto pos = static_cast<size_t>(bulk_sel_obj_id_->getInt(res_idx++));
            eml_int64 id = bulk_sel_obj_id_->getInt64(res_idx++);

            res.at(miss_indexes.at(pos)) = id;
            res_map_.emplace(ResKey{ miss_typ_ids.at(pos), miss_guids.at(pos) }, id);
        }
        bulk_sel_obj_id_->reset();

        it_typ_ids_beg += bulk_size;
        it_guids_beg += bulk_size;
        it_idx_beg += bulk_size;

        remaining -= bulk_size;
    }

    return res;

}

std::pair<eml_int64, std::string> SpaceWriter::select_obj_id_xml(
    int typ_id, const oes::core::Guid& uid
) {
    auto& stmt = *sel_obj_id_xml_;

    stmt.bind(1, typ_id);
    stmt.bind(2,uid);

    if (!stmt.step()) {
        // not found
        stmt.reset();
        return {-1, ""};
    }

    eml_int64 obj_id = stmt.getInt64(0);
    const std::string xml = stmt.getString(1);
    assert(obj_id >= 0);
    stmt.reset();

    return {obj_id, xml};
}

eml_int64 SpaceWriter::insert_uncompressed_obj(
    eml_int64 obj_id, int typ_id, const oes::core::Guid& uid, const std::string& xml
) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "SpaceWriter." + std::string(__func__),
         {{"uuid", uid.toXmlString()}});
    ResKey obj_key(typ_id, uid);
    assert(res_map_.find(obj_key) != res_map_.end());

    auto& stmt = *ins_obj_;
    stmt.bind(1, obj_id);
    bindTextNoCopy(stmt, 2, xml);
    stmt.execute();
    stmt.reset();

    stmt.bind(1); // clear non-transient bindings
    const bool found = res_map_.find(obj_key)!=res_map_.end();

    // In case the just-inserted object was a missing reference
    // for one-or-more previous objects, add those refs (i.e. "catch-up")
    if (found && !missing_refs_.empty()) {
        // cppcheck-suppress unreadVariable
        auto rels_span = oes::core::OtelSpan(std::string("SpaceWriter.") + __func__ + ".insertRels");
        ObjRef ref{ 0, 0, obj_key };
        auto ref_iter = missing_refs_.lower_bound(ref);
        int64_t count=0;
        while (
            ref_iter != missing_refs_.end() &&
            ref_iter->first.res_key == obj_key
        ) {
            // There can be several refs, for the same obj_id and xpa_id.
            // We count them in the table, so must insert several time.
            // Could do it in single statement via different SQL...
            // (no code reuse then though)
            const int ref_count = ref_iter->second;
            for (int i = 0; i < ref_count; ++i) {
                // Does an UPSERT, so first is an INSERT with cnt = 1
                // (default value), next ones are UPDATEs with ++cnt
                do_insert_rel(ref_iter->first, obj_id);
                count++;
            }
            ref_iter = missing_refs_.erase(ref_iter);
        }
        rels_span.setAttr("MissingCount", std::to_string(count));
    } else {
        assert(found);
    }

    // TODO: Figure out if we need to update orphan_rels_ too.
    // The new re-added object may have change obj_id

    return obj_id;
}

bool SpaceWriter::update_uncompressed_obj(
    eml_int64 obj_id, const std::string& xml
) {
    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(std::string("SpaceWriter.") + __func__);
    auto& stmt = *upd_obj_;
    bindTextNoCopy(stmt, 1, xml);
    stmt.bind(2, obj_id);
    stmt.execute();
    int64_t updated_row = stmt.getAffectedRows();
    stmt.reset();
    return updated_row == 1;
}

eml_int64 SpaceWriter::insert_res(
    int typ_id,
    const oes::common::epc::xml::AbstractCitedDataObject& cited,
    const std::string& xdata_json, const std::vector<uint8_t>& xdata_avro
) {
    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"uuid", cited.uuid_}});
    Guid uuid = Guid::parse(cited.uuid_);

    auto& stmt = *ins_res_;

    std::chrono::microseconds creation_usec(0);
    if (!cited.Creation_.empty()) {
        const bool creation_ok = oes::core::usecSinceEpochFromExtendedIsoDateTime(
            cited.Creation_, creation_usec
        );
        assert(creation_ok);
        OES_RELEASE_UNUSED(creation_ok);
    }

    std::chrono::microseconds last_update_usec(0);
    if (!cited.LastUpdate_.empty()) {
        const bool last_update_ok = oes::core::usecSinceEpochFromExtendedIsoDateTime(
            cited.LastUpdate_, last_update_usec
        );
        assert(last_update_ok);
        OES_RELEASE_UNUSED(last_update_ok);
    }
    //// HEREEEE
    int idx = 0;
    stmt.bind(++idx, typ_id);
    bindGuid(stmt, ++idx, uuid);
    bindTextNoCopy(stmt, ++idx, cited.Title_);
    bindTextNoCopy(stmt, ++idx, cited.VersionString_); // Record??? Seems useless
    if (auto usec = creation_usec.count()) {
        stmt.bind(++idx, eml_int64(usec));
    } else {
        stmt.bind(++idx); // i.e. null
    }
    if (auto usec = last_update_usec.count()) {
        stmt.bind(++idx, eml_int64(usec));
    } else {
        stmt.bind(++idx); // i.e. null
    }
    bindTextNoCopy(stmt, ++idx, cited.Originator_);
    bindTextNoCopy(stmt, ++idx, cited.Editor_);

    if (xdata_json.empty()) {
        stmt.bind(++idx);
    } else {
        bindTextNoCopy(stmt, ++idx, xdata_json);
    }

    if (xdata_avro.empty()) {
        stmt.bind(++idx);
    } else {
        int len = static_cast<int>(xdata_avro.size());
        bindBlobNoCopy(stmt, ++idx, xdata_avro.data(), len);
    }

    stmt.bind(++idx);

    stmt.execute();
    const eml_int64 id = stmt.getInt64(0);
    stmt.reset();
    res_map_.emplace(ResKey{ typ_id, uuid }, id);
    return id;
}

/*!
 * Delete the resource with NULL guid from the database
*/
int64_t SpaceWriter::delete_null_res() {
    auto& stmt1 = *del_null_rel_;
    stmt1.execute();
    stmt1.getAffectedRows();
    stmt1.reset();
    auto& stmt2 = *del_null_res_;
    stmt2.execute();
    auto deleted = stmt2.getAffectedRows();
    stmt2.reset();
    return deleted;
}

/**
 * Deletes the deleted resource with the given type and UUID from the database.
 * Used when re-importing a resource that was previously deleted.
 * 
 * @param uid The ETP type and UUID of the deleted resource to be deleted.
 */
void SpaceWriter::delete_deleted_res(const TypedUid& uid) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("SpaceWriter." + std::string(__func__));
    auto& stmt = *del_del_res_;

    int idx = 0;
    bindGuid(stmt, ++idx, uid.second);
    bindTextNoCopy(stmt, ++idx, uid.first);
    stmt.execute();
    stmt.reset();
}

auto SpaceWriter::to_obj_ref(
    eml_int64 obj_id, const std::string& xpath, const oes::common::epc::xml::DataObjectReference& r
) -> ObjRef
{
    ObjRef ref;


    std::vector<std::string> qType;
    boost::split(qType, r.QualifiedType_, boost::algorithm::is_any_of("."));

    eml_int64 uri_id = -1;
    if(qType.size()==2){
        if (qType[0] == "resqml22") {
            uri_id = resqmlv22_uri_id();
        } else if (qType[0] == "resqml20") {
            uri_id = resqmlv20_uri_id();
        } else if (qType[0] == "eml23") {
            uri_id = commonv23_uri_id();
        } else if (qType[0] == "eml20") {
            uri_id = commonv20_uri_id();
        } else if (qType[0] == "witsml21") {
            uri_id = witsmlv21_uri_id();
        } else if (qType[0] == "prodml22") {
            uri_id = prodmlv22_uri_id();
        } else if (qType[0] == "prodml23") {
            uri_id = prodmlv23_uri_id();
        }
    }

    OES_ASSERT(uri_id != -1, return ref;);
    ref.res_key.first = typ_of(uri_id, qType[1]);

    ref.xpa_id = xpa_of(xpath);
    if (!ref.res_key.second.reparse(r.Uuid_.c_str(), r.Uuid_.length(), false)) {
        ref.res_key.second.reset(); // a failed reparse() may leave it partially written
    }
    if (!ref.res_key.second.isValid()) {
        OES_WARNING(
            "Ignoring reference with invalid UUID '{}' from object {} at {}",
            r.Uuid_, obj_id, xpath
        );
    }

    ref.obj_id = obj_id;

    return ref;
}

bool SpaceWriter::bulk_insert_rels(const std::vector<oes::eml::openkv::ObjRef>& refs, size_t bulk_size) {
    bool missing = false;
    std::vector<struct RelToAdd> relations;
    for( const ObjRef& ref: refs ) {
        if (!ref.res_key.second.isValid()) {
            continue; // warned in to_obj_ref()
        }
        auto dst_found = res_map_.find(ref.res_key);
        if (dst_found == res_map_.end()) {
            if (fill_res_map()) {
                // entries added, so retry the lookup
                dst_found = res_map_.find(ref.res_key);
            }
            if (dst_found == res_map_.end()) {
                ++missing_refs_[ref];
                missing = true;
                continue;
            }
        }
        const eml_int64 dst_id = dst_found->second;
        struct RelToAdd rel(ref.obj_id, ref.xpa_id, dst_id);
        relations.push_back(rel);
    }

    do_bulk_insert_rels(relations, bulk_size);
    return missing;
}

/*!
 * Bulk-null GUIDs for a batch of existing objects (update path).
 * Returns a vector of old obj_ids in the same order as type_uids (-1 if not found).
 */
std::vector<eml_int64> SpaceWriter::bulk_null_res_guids(
    const std::vector<int>& typ_ids,
    const std::vector<oes::core::Guid> guids,
    const std::vector<int> indexes,
    size_t bulk_size) {

    assert(typ_ids.size() == guids.size());
    std::vector<eml_int64> old_ids;
    old_ids.reserve(typ_ids.size());

    auto existing_obj_ids = bulk_select_obj_ids(typ_ids, guids, indexes, bulk_size);

    for (int idx : indexes) {
        old_ids.push_back(existing_obj_ids.at(idx));
    }

    if (old_ids.empty()) {
        return old_ids;
    }

    // Copy existing rows to del_res in batch
    auto& stmt_del = *batch_add_del_res_;
    stmt_del.bind(1, old_ids);
    stmt_del.execute();
    stmt_del.reset();

    // Set guid = NULL in batch
    auto& stmt_null = *bulk_null_res_uuid_;
    stmt_null.bind(1, old_ids);
    stmt_null.execute();
    stmt_null.reset();

    // Like null_res_guid(): any cached entry for these keys is now invalid.
    for (size_t i = 0; i < typ_ids.size(); ++i) {
        res_map_.erase(ResKey{ typ_ids.at(i), guids.at(i) });
    }

    return old_ids;
}

/*!
 * Bulk-insert a batch of res rows and return their new obj_ids in input order.
 * Also updates res_map_. xdata_avro is always NULL in this path.
 * 
 * std::vector<eml_int64> SpaceWriter::bulk_insert_res(
    const std::vector<ResData>& data,
    size_t bulk_size) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"count", std::to_string(data.size())}});

    if (data.empty()) return {};

    std::vector<eml_int64> result(data.size(), -1);


    std::vector<int> typ_ids;
    std::vector<Guid> guids;
    std::vector<std::string> title;
    std::vector<std::string> version;
    std::vector<eml_int64> createdAt; 
    std::vector<eml_int64> updatedAt;
    std::vector<std::string> originators;
    std::vector<std::string> editors;
    std::vector<std::string> xdata;
    
    std::vector<std::string> avro;
    std::vector<std::vector<uint8_t>> avro_lens;

    // std::vector<nu>

    for (const auto& d :data) {
        const auto& cited = d.cited;
        Guid uuid = Guid::parse(cited.uuid_);

        std::chrono::microseconds creation_usec(0);
        if (!cited.Creation_.empty()) {
            const bool creation_ok = oes::core::usecSinceEpochFromExtendedIsoDateTime(
                cited.Creation_, creation_usec
            );
            assert(creation_ok);
            OES_RELEASE_UNUSED(creation_ok);
        }

        std::chrono::microseconds last_update_usec(0);
        if (!cited.LastUpdate_.empty()) {
            const bool last_update_ok = oes::core::usecSinceEpochFromExtendedIsoDateTime(
                cited.LastUpdate_, last_update_usec
            );
            assert(last_update_ok);
            OES_RELEASE_UNUSED(last_update_ok);
        }

        typ_ids.push_back(d.typ_id);
        guids.push_back(uuid);
        title.push_back(cited.Title_);
        version.push_back(cited.VersionString_);

    }
 * 
 */
std::unordered_map<size_t, eml_int64> SpaceWriter::bulk_insert_res(
    std::vector<ResData>&& data,
    const std::vector<size_t> index_map,
    size_t bulk_size) {
    assert(data.size() == index_map.size());

    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"count", std::to_string(data.size())}});

    if (data.empty()) {
        return {};
    }
    std::unordered_map<std::string, size_t> ins_map;
    for (size_t i{0}; i < data.size(); i++) {
        auto& d = data.at(i);
        oes::core::Guid guid(d.cited.uuid_);
        const bool added = ins_map.emplace(guid.toStringNoDash() + "_" + std::to_string(d.typ_id), i).second;
        assert(added && "bulk_insert_res: duplicate (typ_id, guid), collapse duplicates before calling");
        OES_RELEASE_UNUSED(added);
    }
    
    std::unordered_map<size_t, eml_int64> result;


    std::vector<int> typs_id; typs_id.reserve(bulk_size);
    std::vector<oes::core::Guid> guids; guids.reserve(bulk_size);
    std::vector<std::string> names; names.reserve(bulk_size);
    std::vector<std::string> version; version.reserve(bulk_size);
    std::vector<eml_int64> created_at; created_at.reserve(bulk_size);
    std::vector<eml_int64> last_update; last_update.reserve(bulk_size);
    std::vector<std::string> created_by; created_by.reserve(bulk_size);
    std::vector<std::string> updated_by; updated_by.reserve(bulk_size);
    std::vector<std::string> xdata_json; xdata_json.reserve(bulk_size);
    std::vector<std::vector<uint8_t>> xdata_avro; xdata_avro.reserve(bulk_size);
    std::vector<eml_int64> space_rel_ids; space_rel_ids.reserve(bulk_size);

    size_t start { 0 };

    size_t remaining = data.size();
    while (remaining > 0) {
        if (remaining < bulk_size) {
            bulk_size = remaining;
        }

        for (size_t i{start}; i < start + bulk_size; i++ ) {
            auto& d = data.at(i);
            typs_id.push_back(d.typ_id);
            guids.push_back(oes::core::Guid(d.cited.uuid_)); // same of cited.uuid_??
            names.push_back(std::move(d.cited.Title_));
            version.push_back(std::move(d.cited.VersionString_));

            std::chrono::microseconds creation_usec(0);
            if (!d.cited.Creation_.empty()) {
                const bool creation_ok = oes::core::usecSinceEpochFromExtendedIsoDateTime(
                    d.cited.Creation_, creation_usec
                );
                assert(creation_ok);
                OES_RELEASE_UNUSED(creation_ok);
            }
            if (auto usec = static_cast<eml_int64>(creation_usec.count())) {

                created_at.push_back(usec);
            } else {
                created_at.push_back(-1);
            }

            std::chrono::microseconds last_update_usec(0);
            if (!d.cited.LastUpdate_.empty()) {
                const bool last_update_ok = oes::core::usecSinceEpochFromExtendedIsoDateTime(
                    d.cited.LastUpdate_, last_update_usec
                );
                assert(last_update_ok);
                OES_RELEASE_UNUSED(last_update_ok);
            }
            if (auto usec = static_cast<eml_int64>(last_update_usec.count())) {

                last_update.push_back(usec);
            } else {
                last_update.push_back(-1);
            }
            
            created_by.push_back(std::move(d.cited.Originator_));
            updated_by.push_back(std::move(d.cited.Editor_));
            xdata_json.push_back(std::move(d.xdata_json)); // null
            
            xdata_avro.push_back(std::move(d.xdata_avro));
            
            space_rel_ids.push_back(-1);

        }
        
        auto& stmt = *bulk_ins_res_;
        int idx {1};
        stmt.bind<int>(idx++, typs_id);
        stmt.bind<oes::core::Guid>(idx++, guids.cbegin(), guids.cend());
        //! stmt.bind<std::string>(idx++, names);
        //! stmt.bind<std::string>(idx++, version);
        stmt.bindTextArray(idx++, names);
        stmt.bindTextArray(idx++, version);
        stmt.bind<eml_int64>(idx++, created_at);
        stmt.bind<eml_int64>(idx++, last_update);
        stmt.bindTextArray(idx++, created_by);
        stmt.bindTextArray(idx++, updated_by);
        stmt.bindTextArray(idx++, xdata_json);
        stmt.bindByteaArray(idx++, xdata_avro);
        stmt.bind<eml_int64>(idx++, space_rel_ids);

        while(stmt.step()) {
            auto id = stmt.getInt64(0);
            auto guid = stmt.getGuid(1);
            auto typ_id = stmt.getInt(2);

            auto key = guid.toStringNoDash() + "_" + std::to_string(typ_id);
            auto index = ins_map.at(key);

            result.emplace(index_map.at(index), id);
            res_map_.emplace(ResKey{ typ_id, guid }, id);
        }
        stmt.reset();

        typs_id.clear();
        guids.clear();
        names.clear();
        version.clear();
        created_at.clear();
        last_update.clear();
        created_by.clear();
        updated_by.clear();
        xdata_json.clear();
        xdata_avro.clear();
        space_rel_ids.clear();

        start += bulk_size;
        remaining -= bulk_size;
    }

    return result;
}

void SpaceWriter::bulk_insert_uncompressed_objs(
        const std::vector<eml_int64>& obj_ids,
        const std::vector<std::string>& xmls,
        const std::vector<int>& indexes,
        size_t bulk_size) {
    assert(obj_ids.size() == xmls.size());
    assert(obj_ids.size() == indexes.size());
        
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "SpaceWriter." + std::string(__func__),
         {{"count", obj_ids.size()}});
    std::unordered_map<int, eml_int64> res;

    std::unordered_map<eml_int64, int> objIds_bIndex_map;
    for(size_t i{0}; i < xmls.size(); i++) {
        objIds_bIndex_map[obj_ids.at(i)] = indexes.at(i);
    }
    
    if (obj_ids.size() < bulk_size) {
        bulk_size = obj_ids.size();
    }

    auto it_obj_ids = obj_ids.cbegin();
    auto it_xmls = xmls.cbegin();
    //!auto it_indexes = indexes.cbegin();

    auto remaining = obj_ids.size();
    while(remaining > 0) {
        //!std::vector<eml_int64> bulk_obj_ids(it_obj_ids, it_obj_ids + bulk_size);
        //!std::vector<std::string> bulk_xmls(it_xmls, it_xmls + bulk_size);
        //!std::vector<int> bulk_indexes(it_indexes, it_indexes + bulk_size);
        if (remaining < bulk_size) {
            bulk_size = remaining;
        }

        bulk_ins_obj_->bind<eml_int64>(1, it_obj_ids, it_obj_ids + bulk_size);
        bulk_ins_obj_->bindTextArray(2, it_xmls, it_xmls + bulk_size);
        //!bulk_ins_obj_->bind<int>(3, it_indexes, it_indexes + bulk_size);

        while (bulk_ins_obj_->step()) {
            auto obj_id = bulk_ins_obj_->getInt64(0);

            res[objIds_bIndex_map.at(obj_id)] = obj_id;
        }
        bulk_ins_obj_->reset();

        it_obj_ids += bulk_size;
        it_xmls += bulk_size;
        //!it_indexes += bulk_size;
        remaining -= bulk_size;
    }

    //TODO return map
}


void SpaceWriter::bulk_insert_missing_refs(
    const std::vector<eml_int64>& obj_ids,
    const std::vector<eml_int64>& typ_ids,
    const std::vector<oes::core::Guid>& guids,
    size_t bulk_size) {
    assert(typ_ids.size() == guids.size());
    
    std::vector<RelToAdd> rels;
    
    for (size_t i {0}; i < typ_ids.size(); i++) {
        ResKey obj_key(typ_ids.at(i), guids.at(i));
        const auto obj_id = obj_ids.at(i);
        
        assert(res_map_.find(obj_key) != res_map_.end());
        if (missing_refs_.empty()) {
            break; //?
        }
        
        ObjRef ref {0, 0, obj_key};
        auto ref_iter = missing_refs_.lower_bound(ref);

        while (
            ref_iter != missing_refs_.end() &&
            ref_iter->first.res_key == obj_key
        ) {
            // There can be several refs, for the same obj_id and xpa_id.
            // We count them in the table, so must insert several time.
            // Could do it in single statement via different SQL...
            // (no code reuse then though)
            const int ref_count = ref_iter->second;
            for (int i = 0; i < ref_count; ++i) {
                // Does an UPSERT, so first is an INSERT with cnt = 1
                // (default value), next ones are UPDATEs with ++cnt
                //! do_insert_rel(ref_iter->first, obj_id);
                RelToAdd to_add(ref_iter->first.obj_id, ref_iter->first.xpa_id, obj_id);

                rels.push_back(to_add);
            }
            ref_iter = missing_refs_.erase(ref_iter);
        }

    }

    do_bulk_insert_rels(rels, bulk_size);
}

/*!
* Nulls the guid of the resource with the given UUID.
*/
bool SpaceWriter::null_res_guid(int typ_id, const oes::core::Guid& uid)
{
    eml_int64 obj_id = select_obj_id(typ_id, uid);
    if(obj_id<0){
        return false;
    }
    auto& stmt1= *add_del_res_;
    stmt1.bind(1, obj_id);
    stmt1.execute();
    stmt1.reset();
    auto& stmt = *null_res_uuid_;
    stmt.bind(1, obj_id);
    stmt.execute();
    int64_t updated_row = stmt.getAffectedRows();
    stmt.reset();
    res_map_.erase(ResKey{ typ_id, uid });
    return updated_row == 1;
}

/*!
 * Updates the destination ID of the relations with the new ID.
*/
void SpaceWriter::update_dst_id(
    int64_t old_dst_id, int64_t new_dst_id
) {
    auto& stmt = *upd_dst_id_;
    stmt.bind(1, new_dst_id);
    stmt.bind(2, old_dst_id);
    stmt.execute();
    stmt.reset();
}

void SpaceWriter::bulk_update_dst_id(
    const std::vector<eml_int64>& old_ids,
    const std::vector<eml_int64>& new_ids,
    size_t bulk_size) {
    assert(old_ids.size() == new_ids.size());

    if (old_ids.size() < bulk_size) {
        bulk_size = old_ids.size();
    }

    long long remaning = static_cast<long long>(old_ids.size());
    auto it_old_ids_beg = old_ids.cbegin();
    auto it_new_ids_beg = new_ids.cbegin();
    while (remaning > 0) {
        if (remaning < static_cast<long long>(bulk_size)) {
            bulk_size = static_cast<size_t>(remaning);
        }

        bulk_upd_dst_id_->bind<eml_int64>(1, it_new_ids_beg, it_new_ids_beg + bulk_size);
        bulk_upd_dst_id_->bind<eml_int64>(2, it_old_ids_beg, it_old_ids_beg + bulk_size);
        bulk_upd_dst_id_->execute();
        bulk_upd_dst_id_->reset();

        it_new_ids_beg += bulk_size;
        it_old_ids_beg += bulk_size;
        remaning -= bulk_size;
    }
}

DstRelIds SpaceWriter::select_rels_by_dst(eml_int64 dst_id) {
    auto& stmt = *sel_rel_by_dst_;

    stmt.bind(1, dst_id);

    DstRelIds rel_ids;
    while (stmt.step()) {
        DstRelId rel_id;
        rel_id.rel_id = stmt.getInt64(0);
        rel_id.obj_id = stmt.getInt64(1);
        rel_ids.emplace_back(rel_id);
    }
    stmt.reset();

    return rel_ids;
}

void SpaceWriter::do_insert_rel(const ObjRef& ref, eml_int64 dst_id) {
    auto& stmt = *ins_rel_;

    stmt.bind(1, ref.obj_id);
    stmt.bind(2, dst_id);
    stmt.bind(3, ref.xpa_id);
    stmt.execute();
    stmt.reset();
}

/*!
 * upsert relations in bulk groups.
 * \param relations the relations to insert, duplicates included (each one adds 1 to rel.cnt).
 * \param bulk_size the number of elements to upsert in one query.
 */
void SpaceWriter::do_bulk_insert_rels(const std::vector<RelToAdd>& relations, int bulk_size) {
    std::vector<eml_int64> obj_ids;
    std::vector<eml_int64> dst_ids;
    std::vector<eml_int64> xpa_ids;
    obj_ids.reserve(relations.size());
    dst_ids.reserve(relations.size());
    xpa_ids.reserve(relations.size());
    for (const RelToAdd& r : relations) {
        obj_ids.push_back(r.obj_id);
        dst_ids.push_back(r.dst_id);
        xpa_ids.push_back(r.xpa_id);
    }

    auto& stmt = *bulk_ins_rel_;
    const size_t chunk = static_cast<size_t>(std::max(bulk_size, 1));
    for (size_t start = 0; start < relations.size(); start += chunk) {
        const size_t end = std::min(start + chunk, relations.size());
        stmt.bind<eml_int64>(1, obj_ids.cbegin() + start, obj_ids.cbegin() + end);
        stmt.bind<eml_int64>(2, dst_ids.cbegin() + start, dst_ids.cbegin() + end);
        stmt.bind<eml_int64>(3, xpa_ids.cbegin() + start, xpa_ids.cbegin() + end);
        stmt.execute();
        stmt.reset();
    }
}

eml_int64 SpaceWriter::select_ary_id(const oes::core::Guid& epc_guid, const std::string& path) {
    auto& stmt = *sel_ary_id_;
    stmt.bind(1, epc_guid);
    bindTextNoCopy(stmt, 2, path);

    if (!stmt.step()) {
        stmt.reset();
        return -1;
    }

    auto ary_id = stmt.getInt64(0);
    assert(!stmt.step());
    stmt.reset();
    stmt.bind(2); // Just in case, clear non-transient text binding
    return ary_id;
}

SqlAryIds SpaceWriter::select_ary_ids(eml_int64 obj_id) {
    auto& stmt = *sel_ary_ids_;
    stmt.bind(1, obj_id);

    SqlAryIds ids;
    while (stmt.step()) {
        SqlAryId& id = ids.emplace_back();
        id.ary_id_ = stmt.getInt64(0);
        id.path_ = stmt.getString(1);
        id.xpa_id_ = stmt.getInt64(2);
    }
    stmt.reset();
    return ids;
}

/*!
 * Select the array containing object with the given container and path
*/
bool SpaceWriter::select_array_object(const oes::core::Guid& epc_guid, std::string& path, eml_int64& arr_id, eml_int64& obj_id, eml_int64& xpath_id){
    auto& stmt = *sel_arr_obj_;
    stmt.bind(1, epc_guid);
    stmt.bind(2, path);
    if (!stmt.step()) {
        stmt.reset();
        return false;
    }
    arr_id = stmt.getInt64(0);
    obj_id = stmt.getInt64(1);
    xpath_id = stmt.getInt64(2);
    stmt.reset();
    if(obj_id == 0 || xpath_id == 0){
        obj_id = -1;
        xpath_id = -1;
        return false;
    }
    return true;
}

int64_t SpaceWriter::do_insert_or_upsert_ary(
    oes::postgresql::Statement& stmt,
    const HdfRef& hdf, const HdfAry& ary,
    bool& in_row_blob, const void*& to_be_chunked
) {
    // Below, we save both (obj_id, xpa_id), which are only a candidate key when the
    // space holds a single HDF file, and even then only if the xpath discriminates
    // repeated patches. The key is (epc_guid, path), mirroring the identity a dataset
    // has in the EPC itself: two external parts may hold the same path. The 2-ints
    // would be more efficient, but using them implies "rewriting" the XML to change
    // the <PathInHdfFile> values, so we take the "easy and pragmatic route". Also, when
    // DataArrays are written before DataObjects, we cannot know (obj_id, xpa_id) but
    // still want to write the arrays to the DB, connecting them to their objects
    // (and xpas) once those come in later.
    assert(hdf.epc_guid.isValid());

    int idx = 0;
    if (hdf.obj_id > 0) {
        stmt.bind(++idx, hdf.obj_id);
    } else {
        stmt.bind(++idx); // NULL
    }
    if (hdf.xpa_id > 0) {
        stmt.bind(++idx, hdf.xpa_id);
    } else {
        stmt.bind(++idx); // NULL
    }
    stmt.bind(++idx, hdf.epc_guid);
    bindTextNoCopy(stmt, ++idx, hdf.path); // NK, together with epc_guid (but see above)
    stmt.bind(++idx, ary.elem_type);
    stmt.bind(++idx, ary.rank);
    for (int i = 0; i < ary.rank; ++i) {
        stmt.bind(++idx, eml_int64(ary.dims[i]));
    }
    for (int i = ary.rank; i < 4; ++i) {
        stmt.bind(++idx); // NULL
    }

    // The blob is small enough, that we store it directly in ary table, not bin table
    in_row_blob = (ary.uncompressed_size <= in_row_blob_threshold__);

    // Note: ary.storage_size not used yet, compression remains a TODO
    // Because we cannot easily know the array's byte-size, unless it is stored "in-row",
    // we re-use the usize (i.e. uncompressed-size) column to store that size, when "out-of-row"
    // Warning: During Direct-Import, the actual HDF5 storage_size can be larger than
    //          uncompressed_size. So we no longer use != but < in compare them below.
    //          usize must be null when the blob is "in-row" (asserted invariant elsewhere).
    if (!in_row_blob || (ary.storage_size > 0 && ary.storage_size < ary.uncompressed_size)) {
        stmt.bind(++idx, eml_int64(ary.uncompressed_size));
    } else {
        stmt.bind(++idx); // NULL
    }

    to_be_chunked = nullptr;
    int blob_idx = 0;

    if (const void* p = ary.data()) {
        if (in_row_blob) {
            // the data is "all there" in "one piece", *AND* small enough that
            // we insert it all "in-row", to ensure the ary table stays small.
            stmt.bindBytea(++idx,(char*)p,ary.uncompressed_size);
            stmt.bind(++idx);
            blob_idx = idx;
        } else {
            // the data is "all there" in "one piece", *BUT* big enough that
            // we store it in the bin table instead, in chunks of 1MiB.
            stmt.bind(++idx);

            // Write the chunk-size, to have it per-blob, just in case
            stmt.bind(++idx, out_of_row_blob_chunk_size__);

            to_be_chunked = p;
        }
    } else {
        if (in_row_blob) {
            // should be rare, but why not
            bindZeroBlob(stmt, ++idx, int(ary.uncompressed_size));

            // For the chunk_length column
            stmt.bind(++idx);
        } else {
            stmt.bind(++idx);

            // Write the chunk-size, to have it per-blob, just in case
            stmt.bind(++idx, out_of_row_blob_chunk_size__);
        }
    }

    /*!
    ** Insert zcomp (modify ins_ary_ SQL) when compression no longer a TODO
    if (compression != zlib) {
    stmt.bind(++idx, zcomp);
    } else {
    stmt.bind(++idx); // NULL
    }
    */
    stmt.execute();
    const int64_t id = stmt.getInt64(0);
    stmt.reset();

    // Just in case, clear non-transient text or blob bindings
    // Use sqlite3_clear_bindings() instead?
    stmt.bind(3);
    if (blob_idx) {
        stmt.bind(blob_idx);
    }

    return id;
}

eml_int64 SpaceWriter::insert_ary(const HdfRef& hdf, const HdfAry& ary) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"path", hdf.path}}
    );
    auto& stmt = *ins_ary_;

    bool in_row_blob;
    const void* to_be_chunked;
    eml_int64 ary_id = do_insert_or_upsert_ary(stmt, hdf, ary, in_row_blob, to_be_chunked);

    span.setAttr("useBin", in_row_blob ? "false" : "true");
    if (!in_row_blob) {
        const SqlAry sql_ary = SqlAry::from(ary, ary_id);
        insert_bin(sql_ary, to_be_chunked, sql_ary.usize_);
    }

    return ary_id;
}

eml_int64 SpaceWriter::upsert_ary(const HdfRef& hdf, const HdfAry& ary) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"path", hdf.path}}
    );
    auto& stmt = *ups_ary_;

    bool in_row_blob;
    const void* to_be_chunked;
    eml_int64 ary_id = do_insert_or_upsert_ary(stmt, hdf, ary, in_row_blob, to_be_chunked);

    assert(ary_id != -1);

    span.setAttr("useBin", in_row_blob ? "false" : "true");
    if (!in_row_blob) {
        const SqlAry sql_ary = SqlAry::from(ary, ary_id);
        insert_bin(sql_ary, to_be_chunked, sql_ary.usize_);
    }

    return ary_id;
}

std::unordered_map<int, eml_int64> SpaceWriter::bulk_upsert_ary(
        const std::vector<HdfRef>& refs,
        const std::vector<HdfAry>& arys,
        const std::vector<int>& indexes,
        size_t bulk_size) {
    std::unordered_map<int, eml_int64> res_map;

    auto& stmt = *bulk_ups_ary_;
    auto upsert_res = bulk_do_insert_or_upsert_ary(
        stmt,
        refs,
        arys,
        indexes,
        bulk_size
    );

    // std::vector<int> insert_bin_req_idx__single_blob;
    // std::vector<int> insert_bin_req_idx__multiple_blob;
    std::vector<int> insert_bin_req_idx;
    for (auto& ures : upsert_res) {
        const auto req_idx = ures.first;
        auto& ary_res = ures.second;

        auto [item, _] = res_map.emplace(req_idx, -1);
        

        if (ary_res.in_row || ary_res.skip_bins) {
            item->second = ary_res.ary_idx;
            continue;
        }

        const SqlAry sql_ary = SqlAry::from(arys.at(req_idx), ary_res.ary_idx);

        SqlBinRange range = sql_ary.chunk_range(0, sql_ary.usize_);
        if (!range) {
            continue;
        }
        assert(range.valid());
        ures.second.blob_size = sql_ary.usize_;
        ures.second.is_single_chunk = range.is_single_chunk();

        if (ures.second.is_single_chunk) {
            // insert_bin_req_idx__single_blob.push_back(req_idx);
            insert_bin_req_idx.push_back(req_idx);
            continue;
        }

        if (!ary_res.to_be_chunked) {
            // On PutUninitializedDataArrays, for a multi-chunk array
            // we do not pre-create zero blobs. That's faster, especially
            // since a single messages could be initializing VERY LARGE
            // arrays, and creating multi-GB zero arrays would be slow,
            // forcing to use a large timeout, despite using a small max-msg-size
            // The read/get code must be able to cope with that, though, of course.
            continue;
        }


        ures.second.chunk.chunk_len = sql_ary.chunk_len_;
        ures.second.chunk.chunk_idx_beg = range.full_chunks_begin();
        ures.second.chunk.chunk_idx_end = range.full_chunks_end();
        if (range.has_back_partial_chunk()) {
            ures.second.chunk.last_chunk_idx = range.last_chunk_;
            ures.second.chunk.back_len = range.back_len_;
        }
        
        // insert_bin_req_idx__multiple_blob.push_back(req_idx);
        insert_bin_req_idx.push_back(req_idx);
    }

    /*auto inserted_bin_status =*/ bulk_insert_bin(
        upsert_res,
        insert_bin_req_idx,
        bulk_size
    );

    for (auto& res_idx : insert_bin_req_idx) {
        //! const auto& res_idx = r.first;

        auto [item, _] = res_map.emplace(res_idx, -1);
        item->second = upsert_res.at(res_idx).ary_idx;
        
    }

    return res_map;
}


// Returns the map of req_index_map(refs, arrays) -> 
std::unordered_map<int, SpaceWriter::BulkInsertAryRes> SpaceWriter::bulk_do_insert_or_upsert_ary(
    oes::postgresql::Statement& stmt,
    const std::vector<HdfRef>& refs,
    const std::vector<HdfAry>& arys,
    const std::vector<int>& indexes,
    size_t bulk_size) {
    
    assert(refs.size() == arys.size());
    assert(refs.size() == indexes.size());

    // Request index of the latest occurrence of each key sent through the bulk statement
    std::map<AryKey, int> last_bulk_index;

    std::unordered_map<int, BulkInsertAryRes> res;

    if (refs.size() < bulk_size) {
        bulk_size = refs.size();
    }
    
    std::vector<eml_int64> obj_id; obj_id.reserve(bulk_size);
    std::vector<eml_int64> xpa_id; xpa_id.reserve(bulk_size);
    std::vector<oes::core::Guid> epc_guid; epc_guid.reserve(bulk_size);
    std::vector<std::string> path; path.reserve(bulk_size);
    std::vector<int> type; type.reserve(bulk_size);
    std::vector<int> rank; rank.reserve(bulk_size);
    std::vector<eml_int64> dim1; dim1.reserve(bulk_size);
    std::vector<eml_int64> dim2; dim2.reserve(bulk_size);
    std::vector<eml_int64> dim3; dim3.reserve(bulk_size);
    std::vector<eml_int64> dim4; dim4.reserve(bulk_size);
    std::vector<eml_int64> usize; usize.reserve(bulk_size);
    
    std::vector<const char*> value; value.reserve(bulk_size); // bytea
    std::vector<size_t> value_len__; value_len__.reserve(bulk_size);

    std::vector<int> chunk_length; chunk_length.reserve(bulk_size);
    //!std::vector<int> idx; idx.reserve(bulk_size);
    
    auto remaining = static_cast<long>(refs.size());
    size_t offset{0};
    while (remaining > 0) {
        if (static_cast<size_t>(remaining) < bulk_size) {
            bulk_size = static_cast<size_t>(remaining);
        }

        size_t zeros_size = 0;
        for(size_t j{0}; j < bulk_size; j++) {
            const size_t i = j + offset;
            const auto& ary = arys.at(i);
            if (!ary.data() && // nullptr
                ary.uncompressed_size <= in_row_blob_threshold__ && // in_row
                zeros_size < ary.uncompressed_size) { // nullptr and in_row
                zeros_size = ary.uncompressed_size;
            }
        }

        std::vector<char> zeros(zeros_size > 0 ? zeros_size : 1, static_cast<char>(0));

        size_t real_bulk_size = bulk_size;
        std::map<AryKey, int> batch_indexes; // keys in this batch -> request index
        std::vector<size_t> deferred;        // duplicates of a key already in this batch

        // build the flat arrays
        for(size_t j{0}; j < bulk_size; j++) {
            const size_t i = j + offset;
            const auto& ary = arys.at(i);
            const auto& ref = refs.at(i);
            assert(ref.epc_guid.isValid());

            AryKey key{ ref.epc_guid, ref.path };
            if (batch_indexes.count(key)) {
                // PostgreSQL rejects an upsert touching the same row twice in one statement
                deferred.push_back(i);
                --real_bulk_size;
                continue;
            }
            if (auto prev = last_bulk_index.find(key); prev != last_bulk_index.end()) {
                res.at(prev->second).skip_bins = true;
            }
            last_bulk_index[key] = indexes.at(i);
            batch_indexes.emplace(std::move(key), indexes.at(i));

            auto [item, _] = res.emplace(indexes.at(i), BulkInsertAryRes{});
            auto& ary_res = item->second;
            
            obj_id.push_back(ref.obj_id > 0 ? ref.obj_id : -1); // NULLIF
            xpa_id.push_back(ref.xpa_id > 0 ? ref.xpa_id : -1); // NULLIF
            epc_guid.push_back(ref.epc_guid);
            path.push_back(ref.path);
            type.push_back(ary.elem_type);
            rank.push_back(ary.rank);
            dim1.push_back( ary.rank >= 1 ? ary.dims[0] : -1); // NULLIF
            dim2.push_back( ary.rank >= 2 ? ary.dims[1] : -1); // NULLIF
            dim3.push_back( ary.rank >= 3 ? ary.dims[2] : -1); // NULLIF
            dim4.push_back( ary.rank >= 4 ? ary.dims[3] : -1); // NULLIF

            const bool in_row_blob = ary.uncompressed_size <= in_row_blob_threshold__;
            ary_res.in_row =  in_row_blob;
            const bool usize_too_big = (ary.storage_size > 0 && ary.storage_size < ary.uncompressed_size);
            usize.push_back(static_cast<eml_int64>(
                (!in_row_blob || usize_too_big)
                ? ary.uncompressed_size
                : -1
            ));

            //?void* to_be_chunked = nullptr;
            //?int blob_idx = 0;
            if (const void* p = ary.data()) { // not nullptr
                if (in_row_blob) {
                    value.push_back(reinterpret_cast<const char*>(p));
                    value_len__.push_back(ary.uncompressed_size);


                    chunk_length.push_back(-1); // null
                    //? stmt.bindBytea(++idx,(char*)p,ary.uncompressed_size);
                    //? stmt.bind(++idx);
                    //? blob_idx = idx;
                } else {
                    // the data is "all there" in "one piece", *BUT* big enough that
                    // we store it in the bin table instead, in chunks of 1MiB.
                    //?stmt.bind(++idx);
                    value.push_back(nullptr); // null
                    value_len__.push_back(0);


                    chunk_length.push_back(out_of_row_blob_chunk_size__);

                    // Write the chunk-size, to have it per-blob, just in case
                    //?stmt.bind(++idx, out_of_row_blob_chunk_size__);

                    //TODO to_be_chunked = p;
                    ary_res.to_be_chunked = p;
                }
            } else {
                if (in_row_blob) {
                    // should be rare, but why not
                    //! assert(ary.uncompressed_size > 0);
                    // std::vector<char> zeros(static_cast<size_t>(ary.uncompressed_size), static_cast<char>(0));
                    // zeros_matrix.push_back(std::move(zeros));
                    //?bindZeroBlob(stmt, ++idx, int(ary.uncompressed_size));
                    if (ary.uncompressed_size > 0) {
                        value.push_back(zeros.data());
                        value_len__.push_back(ary.uncompressed_size);
                    } else {
                        value.push_back(nullptr);
                        value_len__.push_back(0);
                    }
                    
                    
                    chunk_length.push_back(-1); // null

                    // For the chunk_length column
                    //?stmt.bind(++idx);
                } else {
                    //?stmt.bind(++idx);
                    value.push_back(nullptr); // null
                    value_len__.push_back(0);

                    chunk_length.push_back(out_of_row_blob_chunk_size__);
                    // Write the chunk-size, to have it per-blob, just in case
                    //?stmt.bind(++idx, out_of_row_blob_chunk_size__);
                }
            }

        }
        
        int row_idx{0};
        stmt.bind(++row_idx, obj_id);
        stmt.bind(++row_idx, xpa_id);
        stmt.bind(++row_idx, epc_guid);
        stmt.bind(++row_idx, path);
        stmt.bind(++row_idx, type);
        stmt.bind(++row_idx, rank);
        stmt.bind(++row_idx, dim1);
        stmt.bind(++row_idx, dim2);
        stmt.bind(++row_idx, dim3);
        stmt.bind(++row_idx, dim4);
        stmt.bind(++row_idx, usize);
        stmt.bindByteaArr(++row_idx, value, value_len__);
        stmt.bind(++row_idx, chunk_length);
        //!stmt.bind(++row_idx, indexes);

        size_t n_processed{0};
        while (stmt.step()) {
            //! const auto ary_index = bulk_ins_ary_->getInt64(0);
            //!const auto index = stmt.getInt(1);
            const auto index = batch_indexes.at({stmt.getGuid(1), stmt.getString(2)});

            auto [item, _] = res.emplace(index, BulkInsertAryRes{});
            auto& ary_res = item->second;
            ary_res.ary_idx = stmt.getInt64(0);

            n_processed++;
        }

        assert(n_processed == real_bulk_size);
        OES_RELEASE_UNUSED(real_bulk_size);
        // Clean up
        stmt.reset();

        // Later duplicates, one at a time and in request order, so the last one wins
        for (const size_t i : deferred) {
            const auto& ary = arys.at(i);
            const auto& ref = refs.at(i);
            auto& ary_res = res[indexes.at(i)];
            ary_res.ary_idx = upsert_ary(ref, ary);
            ary_res.in_row = ary.uncompressed_size <= in_row_blob_threshold__;
            ary_res.skip_bins = true; // upsert_ary() wrote them
            res.at(last_bulk_index.at({ref.epc_guid, ref.path})).skip_bins = true;
        }

        obj_id.clear();
        xpa_id.clear();
        epc_guid.clear();
        path.clear();
        type.clear();
        rank.clear();
        dim1.clear();
        dim2.clear();
        dim3.clear();
        dim4.clear();
        usize.clear();
        value.clear();
        value_len__.clear();
        chunk_length.clear();
        //!idx.clear();

        remaining -= static_cast<long>(bulk_size);
        offset += bulk_size;
    }

    return res;
}

void SpaceWriter::bulk_insert_bin(
    std::unordered_map<int, BulkInsertAryRes>& to_add,
    const std::vector<int>& multi_blob_idx,
    size_t bulk_size
) {
    // struct EmlInt64_Int_Hash {
    //     std::size_t operator() (const std::pair<eml_int64, int>& p) {
    //         auto h1 = std::hash<eml_int64>{}(p.first);
    //         auto h2 = std::hash<int>{}(p.second);
    //         return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    //     }
    // };
    // std::unordered_map<std::pair<eml_int64, int>, int, EmlInt64_Int_Hash> AryIdIdx_indexes_map;
    // for(const auto& item : to_add) {
    //     AryIdIdx_indexes_map[item.second.]
    // }

    //!std::unordered_map<int, bool> res_map;
    auto& stmt = *bulk_ins_bin_;

    //!std::vector<int> idxs; idxs.reserve(bulk_size);
    std::vector<eml_int64> ary_idx; ary_idx.reserve(bulk_size);
    std::vector<int> blob_idx; blob_idx.reserve(bulk_size);
    std::vector<const char*> values; values.reserve(bulk_size);
    std::vector<size_t> lens; lens.reserve(bulk_size);
    
    auto remaining = static_cast<long>(multi_blob_idx.size());
    size_t offset{0};
    size_t check{0};
    while (remaining > 0) {
        if (static_cast<size_t>(remaining) < bulk_size) {
            bulk_size = static_cast<size_t>(remaining);
        }
        
        check = 0;
        for (size_t j{0}; j < bulk_size; j++) {
            const size_t i = j + offset;
            const auto& idx = multi_blob_idx.at(i);

            auto& iary = to_add.at(idx);
            if (iary.is_single_chunk) {
                //!idxs.push_back(idx);
                ary_idx.push_back(iary.ary_idx);
                blob_idx.push_back(0); // only one
                values.push_back(reinterpret_cast<const char*>(iary.to_be_chunked));
                lens.push_back(iary.blob_size);

                check += 1;
            } else  {

                for (int i = iary.chunk.chunk_idx_beg; i < iary.chunk.chunk_idx_end; i++) {
                    //!idxs.push_back(idx);
                    ary_idx.push_back(iary.ary_idx);
                    blob_idx.push_back(i);
                    values.push_back(reinterpret_cast<const char*>(iary.to_be_chunked));
                    lens.push_back(iary.chunk.chunk_len);
                    iary.to_be_chunked = static_cast<const char*>(iary.to_be_chunked) + iary.chunk.chunk_len;
                    check += 1;
                }
    
                if (iary.chunk.last_chunk_idx > -1) {
                    assert(iary.chunk.back_len > 0);
                    //!idxs.push_back(idx);
                    ary_idx.push_back(iary.ary_idx);
                    blob_idx.push_back(iary.chunk.last_chunk_idx);
                    values.push_back(reinterpret_cast<const char*>(iary.to_be_chunked));
                    lens.push_back(iary.chunk.back_len);
                    iary.to_be_chunked = static_cast<const char*>(iary.to_be_chunked) + iary.chunk.back_len;

                    check += 1;
                }
            }
        }

        int row_idx{0};
        stmt.bind(++row_idx, ary_idx);
        stmt.bind(++row_idx, blob_idx);
        stmt.bindBytea(++row_idx, values, lens);
        //!stmt.bind(++row_idx, idxs);

        size_t n_processed{0};
        while (stmt.step()) {
            const auto bin_id = stmt.getInt64(0);
            //!const auto idx = stmt.getInt(1);

            OES_UNUSED(bin_id);

            //! auto [item, _] = res_map.emplace(idx, true);
            //! item->second = true;
            //!res_map.emplace(idx, true);

            n_processed++;
        }
        assert(n_processed == check);

        stmt.reset();

        //~idxs.clear();
        ary_idx.clear();
        blob_idx.clear();
        values.clear();
        lens.clear();

        remaining -= static_cast<long>(bulk_size);
        offset += bulk_size;
    }

    //!return res_map;
}

void SpaceWriter::bulk_update_ary_values(
    const std::vector<eml_int64>& ary_ids,
    const std::vector<const char*>& values,
    const std::vector<size_t>& lens,
    size_t bulk_size
) {
    assert(ary_ids.size() == values.size());
    assert(ary_ids.size() == lens.size());

    if (ary_ids.empty()) {
        return;
    }

    if (ary_ids.size() < bulk_size) {
        bulk_size = ary_ids.size();
    }

    auto& stmt = *bulk_upd_ary_val_;
    auto remaining = static_cast<int>(ary_ids.size());
    size_t offset = 0;

    while (remaining > 0) {
        if (remaining < static_cast<int>(bulk_size)) {
            bulk_size = static_cast<size_t>(remaining);
        }
        //TODO pass iterator
        std::vector<eml_int64> ary_ids_bulk(ary_ids.begin() + offset, ary_ids.begin() + offset + bulk_size);
        std::vector<const char*> values_bulk(values.begin() + offset, values.begin() + offset + bulk_size);
        std::vector<size_t> lens_bulk(lens.begin() + offset, lens.begin() + offset + bulk_size);
        std::vector<int> req_idxs; req_idxs.reserve(bulk_size);
        for (size_t i = 0; i < bulk_size; ++i) {
            req_idxs.push_back(static_cast<int>(i));
        }

        int row_idx = 0;
        stmt.bind(++row_idx, ary_ids_bulk);
        stmt.bindBytea(++row_idx, values_bulk, lens_bulk);
        stmt.bind(++row_idx, req_idxs);

        size_t n_processed = 0;
        while (stmt.step()) {
            OES_UNUSED(stmt.getInt(0));
            ++n_processed;
        }
        assert(n_processed == bulk_size);
        stmt.reset();

        remaining -= static_cast<int>(bulk_size);
        offset += bulk_size;
    }
}

void SpaceWriter::bulk_upsert_bin_values(
    const std::vector<eml_int64>& ary_ids,
    const std::vector<int>& chunk_idxs,
    const std::vector<const char*>& values,
    const std::vector<size_t>& lens,
    size_t bulk_size
) {
    assert(ary_ids.size() == chunk_idxs.size());
    assert(ary_ids.size() == values.size());
    assert(ary_ids.size() == lens.size());

    if (ary_ids.empty()) {
        return;
    }

    if (ary_ids.size() < bulk_size) {
        bulk_size = ary_ids.size();
    }

    auto& stmt = *bulk_ins_bin_;
    auto remaining = static_cast<int>(ary_ids.size());
    size_t offset = 0;

    while (remaining > 0) {
        if (remaining < static_cast<int>(bulk_size)) {
            bulk_size = static_cast<size_t>(remaining);
        }
        //TODO pass iterator
        std::vector<eml_int64> ary_ids_bulk(ary_ids.begin() + offset, ary_ids.begin() + offset + bulk_size);
        std::vector<int> chunk_idxs_bulk(chunk_idxs.begin() + offset, chunk_idxs.begin() + offset + bulk_size);
        std::vector<const char*> values_bulk(values.begin() + offset, values.begin() + offset + bulk_size);
        std::vector<size_t> lens_bulk(lens.begin() + offset, lens.begin() + offset + bulk_size);
        //! std::vector<int> req_idxs; req_idxs.reserve(bulk_size);
        //! for (size_t i = 0; i < bulk_size; ++i) {
        //!     req_idxs.push_back(static_cast<int>(i));
        //! }

        int row_idx = 0;
        stmt.bind(++row_idx, ary_ids_bulk);
        stmt.bind(++row_idx, chunk_idxs_bulk);
        stmt.bindBytea(++row_idx, values_bulk, lens_bulk);
        //!stmt.bind(++row_idx, req_idxs);

        size_t n_processed = 0;
        while (stmt.step()) {
            OES_UNUSED(stmt.getInt64(0));
            // OES_UNUSED(stmt.getInt(1));
            ++n_processed;
        }
        assert(n_processed == bulk_size);
        stmt.reset();

        remaining -= static_cast<int>(bulk_size);
        offset += bulk_size;
    }
}

bool SpaceWriter::update_ary_ids(
    eml_int64 ary_id, eml_int64 obj_id, eml_int64 xpa_id
) {
    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(std::string("SpaceWriter.") + __func__);
    auto& stmt = *upd_ary_;

    stmt.bind(1, obj_id);
    stmt.bind(2, xpa_id);
    stmt.bind(3, ary_id);
    stmt.execute();
    int64_t updated_rows = stmt.getAffectedRows();
    stmt.reset();
    return updated_rows == 1;
}

bool SpaceWriter::bulk_update_ary_ids(
    const std::vector<eml_int64>& obj_ids,
    const std::vector<eml_int64>& xpa_ids,
    const std::vector<eml_int64>& ary_ids,
    size_t bulk_size
) {
    assert(ary_ids.size() == obj_ids.size());
    assert(ary_ids.size() == xpa_ids.size());

    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(std::string("SpaceWriter.") + __func__);


    auto remaining = static_cast<long>(ary_ids.size());
    if (static_cast<size_t>(remaining) < bulk_size) {
        bulk_size = static_cast<size_t>(remaining);
    }

    auto it_obj_ids_beg = obj_ids.cbegin();
    auto it_xpa_ids_beg = xpa_ids.cbegin();
    auto it_ary_ids_beg = ary_ids.cbegin();

    auto& stmt = *bulk_upd_ary_;
    int64_t updated_rows = 0;

    while (remaining > 0) {
        if (static_cast<size_t>(remaining) < bulk_size) {
            bulk_size = static_cast<size_t>(remaining);
        }
        
        //! std::vector<eml_int64> bulk_ary_ids(it_ary_ids_beg, it_ary_ids_beg + bulk_size);
        //! std::vector<eml_int64> bulk_obj_ids(it_obj_ids_beg, it_obj_ids_beg + bulk_size);
        //! std::vector<eml_int64> bulk_xpa_ids(it_xpa_ids_beg, it_xpa_ids_beg + bulk_size);
        
        stmt.bind<eml_int64>(1, it_obj_ids_beg, it_obj_ids_beg + bulk_size);
        stmt.bind<eml_int64>(2, it_xpa_ids_beg, it_xpa_ids_beg + bulk_size);
        stmt.bind<eml_int64>(3, it_ary_ids_beg, it_ary_ids_beg + bulk_size);
        stmt.execute();
        updated_rows += stmt.getAffectedRows();
        stmt.reset();

        it_obj_ids_beg += bulk_size;
        it_xpa_ids_beg += bulk_size;
        it_ary_ids_beg += bulk_size;

        remaining -= static_cast<long>(bulk_size);
    }


    return updated_rows == static_cast<int64_t>(ary_ids.size());
}

const char* SpaceWriter::write_ary(
    const SqlAry& ary, const void* buf, eml_uint64 off, int len
) {
    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"ary_id",std::to_string(ary.ary_id_)}, 
         {"len", len}}
    );
    /*
    ** Can exceed 1GB or even 2GB now that we shard blobs
    if (off > std::numeric_limits<int>::max()) {
        // FIXME: Use bin table, to split very large arrays in pieces
        return "Cannot incrementally write array >2GB";
    }
    */

    assert(ary.valid());

    if (ary.is_compressed()) {
        return "Cannot incrementally write compressed array"; // For now
    }

    // array <= 64 KiB (i.e. in_row_blob_threshold__)
    // in-row (small) array, as a single blob in the ary table
    if (ary.is_in_row()) {
        if ((eml_int64(off) + len) > ary.blob_len_) {
            return "Cannot increase array size via incremental write";
        }

        return writeAryBlob(ary.ary_id_, buf, off, len);
    }

    // array > 1 MiB (i.e. out_of_row_blob_chunk_size__)
    // out-of-row array, but large enough to be in multiple chunks in the bin table
    if (!ary.is_single_blob()) {
        assert(eml_int64(off + len) <= ary.usize_);

        return write_bin(ary, buf, off, len);
    }

    // 64 KiB < array <= 1 MiB
    // out-of-row array, but small enough to be a single chunk in the bin table

    // select the rowid for that one "bin blob" (i.e. WHERE idx is null, thus the -1)
    eml_int64 bin_row_id;
    eml_int64 bin_blob_len;
    if (!select_bin(ary.ary_id_, 0, bin_row_id, bin_blob_len)) {
        return "Cannot select array's only chunk";
    }

    if ((eml_int64(off) + len) > bin_blob_len) {
        return "Cannot increase chunk size via incremental write";
    }

    return writeBinBlob(bin_row_id, buf, off, len);
}

eml_int64 SpaceWriter::delete_ary(eml_int64 ary_id) {
    eml_int64 rows = 0;
    {
        auto& stmt = *del_ary_;
        stmt.bind(1, ary_id);
        stmt.execute();
        rows = stmt.getAffectedRows();
        stmt.reset();
    }

    return rows;
}

bool SpaceWriter::select_bin(
    eml_int64 ary_id, int idx,
    eml_int64& row_id, eml_int64& blob_len
) {
    assert(idx >= 0);

    // select the rowid for the blob, but from the bin table
    auto& stmt = *sel_bin_;

    stmt.bind(1, ary_id);
    stmt.bind(2, idx);
    if (!stmt.step()) {
        stmt.reset();
        return false;
    }

    row_id = stmt.getInt64(0);  // i.e. bin.rowid
    blob_len = stmt.getInt64(1);  // i.e. length(bin.value)

    assert(!stmt.step()); // since UNIQUE
    stmt.reset();

    return true;
}

const char* SpaceWriter::write_bin(
    const SqlAry& ary, const void* buf, eml_uint64 off, uint64_t len
) {
    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"ary_id",std::to_string(ary.ary_id_)},
         {"len", len}}
    );
    if (off == 0 && len == uint64_t(ary.usize_)) {
        insert_bin(ary, buf, len);
        return nullptr; // i.e. OK, no error
    }

    SqlBinRange range = ary.chunk_range(off, len);
    if (!range) {
        return "Invalid range";
    }

    assert(range.valid());

    auto& stmt = *ins_bin_;
    stmt.bind(1, ary.ary_id_);

    const char* err = nullptr;
    const int chunk_len = ary.chunk_len_; // shorter alias
    const uint8_t* src = static_cast<const uint8_t*>(buf);

    auto write_chunk = [&](
        int chunk_idx, int blob_off, int write_len, int expected_blob_len
    ) -> bool {
        const int64_t chunk_byte_off = 1ll * chunk_idx * chunk_len;
        assert(chunk_byte_off < ary.usize_);

        eml_int64 row_id;
        eml_int64 blob_len;
        const bool chunk_found = select_bin(ary.ary_id_, chunk_idx, row_id, blob_len);

        // If this is the last chunk, may need to be smaller
        const int64_t tail_len = ary.usize_ - chunk_byte_off;
        const eml_int64 needed_blob_len = std::min<eml_int64>(chunk_len, tail_len);

        // Create the chunk when missing, or replace a stale chunk of a different size
        if (!chunk_found || blob_len != needed_blob_len) {
            if (chunk_found) {
                OES_WARNING(
                    "Replacing stale bin chunk ary_id={} idx={} old_len={} new_len={}",
                    ary.ary_id_, chunk_idx, blob_len, needed_blob_len
                );
            }

            // TODO: Try copy'ing buf to in-mem chunk, and insert it,
            //       instead of inserting zero-blob then writing it.

            stmt.bind(1, ary.ary_id_);
            stmt.bind(2, chunk_idx);

            bindZeroBlob(stmt, 3, int(needed_blob_len));
            stmt.execute();
            row_id = stmt.getInt64(0);
            stmt.reset();
            blob_len = needed_blob_len;
        }

        if (expected_blob_len > 0) {
            assert(blob_len == expected_blob_len);
        }
        OES_RELEASE_UNUSED(expected_blob_len);

        // assert() is stripped in release builds; enforce the bound at runtime
        if (int64_t(blob_off) + write_len > blob_len) {
            OES_ERROR(
                "Sub-array write exceeds chunk bounds ary_id={} idx={} blob_off={} write_len={} blob_len={}",
                ary.ary_id_, chunk_idx, blob_off, write_len, blob_len
            );
            err = "Sub-array write exceeds chunk bounds";
            return false;
        }

        err = writeBinBlob(row_id, src, blob_off, write_len);

        return err == nullptr;
    };

    // writing within a single chunk
    if (range.is_single_chunk()) {
        write_chunk(range.first_chunk_, range.front_off_, range.back_len_, -1);
        return err;
    }

    // Because we deal with the single-chunk case above, we can assume
    // the first chunk (that we "partially" write) is a "full" chunk.
    if (range.has_front_partial_chunk()) {
        const int write_len = chunk_len - range.front_off_;
        if (!write_chunk(range.first_chunk_, range.front_off_, write_len, chunk_len)) {
            return err;
        }
        src += write_len;
    }

    const int full_chunks_beg = range.full_chunks_begin();
    const int full_chunks_end = range.full_chunks_end();
    for (int chunk_idx = full_chunks_beg; chunk_idx < full_chunks_end; ++chunk_idx) {
        if (!write_chunk(chunk_idx, 0, chunk_len, -1)) {
            return err;
        }
        src += chunk_len;
    }

    if (range.has_back_partial_chunk()) {
        if (!write_chunk(range.last_chunk_, 0, range.back_len_, -1)) {
            return err;
        }
    }

    stmt.bind(3); // Just in case, clear non-transient blob binding

    return err;
}

void SpaceWriter::insert_bin(
    const SqlAry& ary, const void* buf, uint64_t len
) {
    assert(eml_int64(len) == ary.usize_); // i.e. "whole"

    SqlBinRange range = ary.chunk_range(0, len);
    if (!range) {
        return;
    }

    assert(range.valid());
    auto& stmt = *ins_bin_;
    stmt.bind(1, ary.ary_id_);

    // faster code-path for common-case of array between 64 KiB and 1 MiB
    if (range.is_single_chunk()) {
        stmt.bind(2, 0); // 0 idx is either single chunk or first chunk
        if (buf) {
            bindBlobNoCopy(stmt, 3, buf, int(len));
        } else {
            bindZeroBlob(stmt, 3, int(len));
        }
        stmt.execute();
        stmt.reset();
        stmt.bind(3); // Just in case, clear non-transient blob binding
        return;
    }

    if (!buf) {
        // On PutUninitializedDataArrays, for a multi-chunk array
        // we do not pre-create zero blobs. That's faster, especially
        // since a single messages could be initializing VERY LARGE
        // arrays, and creating multi-GB zero arrays would be slow,
        // forcing to use a large timeout, despite using a small max-msg-size
        // The read/get code must be able to cope with that, though, of course.
        return;
    }

    const int chunk_len = ary.chunk_len_; // shorter alias
    const uint8_t* src = static_cast<const uint8_t*>(buf);

    // Loop on full chunks
    const int full_chunks_beg = range.full_chunks_begin();
    const int full_chunks_end = range.full_chunks_end();
    for (int chunk_idx = full_chunks_beg; chunk_idx < full_chunks_end; ++chunk_idx) {
        stmt.bind(1, ary.ary_id_);
        stmt.bind(2, chunk_idx);
        bindBlobNoCopy(stmt, 3, src, chunk_len);
        stmt.execute();
        stmt.reset();
        src += chunk_len;
    }

    if (range.has_back_partial_chunk()) {
        stmt.bind(1, ary.ary_id_);
        stmt.bind(2, range.last_chunk_);
        bindBlobNoCopy(stmt, 3, src, range.back_len_);
        stmt.execute();
        stmt.reset();
    }

    stmt.bind(3); // Just in case, clear non-transient blob binding
}

void SpaceWriter::close_blob() {
// TODO
}

const char* SpaceWriter::writeAryBlob(
    eml_int64 row_id, const void* buf, eml_uint64 off, uint64_t len
) {
    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(
        std::string("SpaceWriter.") + __func__,
        {{"row_id",std::to_string(row_id)}, {"len", len}}
    );
    auto& stmt = *sel_ary_val_;
    stmt.bind(1, row_id);
    stmt.execute();
    int size=0;
    const void* val = stmt.getBlob(0, size);
    if(!val){
        stmt.reset();
        return "Cannot read blob";
    }
    // Split compare to avoid unsigned overflow wrapping past the bound
    if (off > uint64_t(size) || len > uint64_t(size) - off) {
        OES_ERROR("Blob write out of bounds row_id={} off={} len={} size={}", row_id, off, len, size);
        stmt.reset();
        return "Blob write out of bounds";
    }
    std::memcpy((char*)val + off, buf, len);
    auto& stmt2 = *upd_ary_val_;
    stmt2.bind(1, val, size);
    stmt2.bind(2, row_id);
    stmt2.execute();
    stmt.reset();
    stmt2.reset();
    return nullptr;
}

const char* SpaceWriter::writeBinBlob(
    eml_int64 row_id, const void* buf, eml_uint64 off, uint64_t len
) {
    return writeBlob("bin", row_id, &p_bin_blob_, last_bin_rowid_, buf, off, len);
}

const char* SpaceWriter::writeBlob(
    const char* tab, eml_int64 row_id,
    db_blob** pp_blob, eml_int64& last_rowid,
    const void* buf, eml_uint64 off, uint64_t len
) {
    // cppcheck-suppress unreadVariable
    auto up_span = oes::core::OtelSpan(std::string("SpaceWriter.") + __func__, {{"row_id",std::to_string(row_id)}});
    auto& stmt = *sel_bin_val_;
    stmt.bind(1, row_id);
    stmt.execute();
    int size=0;
    const void* val = stmt.getBlob(0, size);
    if(!val){
        stmt.reset();
        return "Cannot read blob";
    }
    // Split compare to avoid unsigned overflow wrapping past the bound
    if (off > uint64_t(size) || len > uint64_t(size) - off) {
        OES_ERROR("Blob write out of bounds row_id={} off={} len={} size={}", row_id, off, len, size);
        stmt.reset();
        return "Blob write out of bounds";
    }
    std::memcpy((char*)val + off, buf, len);
    auto& stmt2 = *upd_bin_val_;
    stmt2.bind(1, val, size);
    stmt2.bind(2, row_id);
    stmt2.execute();
    stmt.reset();
    stmt2.reset();
    OES_UNUSED(tab);
    OES_UNUSED(pp_blob);
    OES_UNUSED(last_rowid);
    return nullptr; // i.e. OK, no error
}

void SpaceWriter::bindZeroBlob(oes::postgresql::Statement& stmt, int index, int len) {
    std::vector<char> empty_array;
    empty_array.resize(len);
    stmt.bindBytea(index,empty_array.data(),len);
}

void SpaceWriter::bindBlobNoCopy(oes::postgresql::Statement& stmt, int index, const void* buf, int len) {
    stmt.bind(index,buf,len);
}

/*!
 * \brief insert references to the resources table
* \param refs The references to insert
* \return The ids of the inserted references
*/
std::vector<int64_t> SpaceWriter::insert_references(const std::vector<ResourceRef> & refs){

    auto& stmt = *ins_res_no_ovwrt_;
    std::vector<int64_t> ids;
    for(auto& ref:refs){
        eml_int64 uri_id = uri_of(ref.uri_namespace,ref.uri_prefix,ref.uri_etp_ml,ref.uri_ml_version);
        int typ_id = typ_of(uri_id,ref.xml_type);
        if(typ_id<0){
            ids.push_back(0);
            continue;
        }
        ResKey obj_key(typ_id, ref.guid);
        auto obj_found = res_map_.find(obj_key);
        if (obj_found != res_map_.end()) {
            ids.push_back(0);
            continue;
        }

        int idx = 0;
        stmt.bind(++idx, typ_id);
        bindGuid(stmt, ++idx, ref.guid);
        bindTextNoCopy(stmt, ++idx, ref.name);
        bindTextNoCopy(stmt, ++idx, ref.version);
        if (ref.created) {
            stmt.bind(++idx, ref.created);
        } else {
            stmt.bind(++idx); // i.e. null
        }
        if (ref.updated) {
            stmt.bind(++idx, ref.updated);
        } else {
            stmt.bind(++idx); // i.e. null
        }
        bindTextNoCopy(stmt, ++idx, ref.created_by);
        bindTextNoCopy(stmt, ++idx, ref.updated_by);
        if (ref.xdata_json.empty()) {
            stmt.bind(++idx);
        } else {
            bindTextNoCopy(stmt, ++idx, ref.xdata_json);
        }
        if (ref.xdata_avro.empty()) {
            stmt.bind(++idx);
        } else {
            int len = static_cast<int>(ref.xdata_avro.size());
            bindBlobNoCopy(stmt, ++idx, ref.xdata_avro.data(), len);
        }
        if(ref.space_rel_id){
            stmt.bind(++idx, ref.space_rel_id);
        }else{
            stmt.bind(++idx);
        }
        stmt.execute();
        const int64_t id = stmt.getInt64(0);
        stmt.reset();
        res_map_.emplace(obj_key, id);
        ids.push_back(id);
    }
    return ids;
}

void SpaceWriter::insert_relations( const std::vector<ReferenceRel>& relations, size_t bulk_size )
{
    std::vector<std::string> xpaths;
    std::vector<int> xpath_indexes;
    xpaths.reserve(relations.size());
    xpath_indexes.reserve(relations.size());
    for (size_t i = 0; i < relations.size(); ++i) {
        xpaths.push_back(relations[i].xpath);
        xpath_indexes.push_back(static_cast<int>(i));
    }
    const auto xpa_info = xpa_of(xpaths, xpath_indexes, bulk_size);

    if(!ins_rels_from_uid_){
        // The source cnt is copied as is; a relation already there is left alone
        prepare(ins_rels_from_uid_,
            R"sql(
            INSERT INTO rel(obj_id, dst_id, xpa_id, cnt)
            SELECT s.obj_id, d.obj_id, r.xpa_id, r.cnt
              FROM UNNEST($1::text[], $2::uuid[], $3::text[], $4::text[], $5::uuid[], $6::text[],
                          $7::bigint[], $8::int[])
                   AS r(src_type, src_guid, src_version, dst_type, dst_guid, dst_version, xpa_id, cnt)
              JOIN typ ts ON true
              JOIN uri us ON us.id = ts.uri_id AND us.ml || '.' || ts.xml = r.src_type
              JOIN res s  ON s.typ_id = ts.id AND s.guid = r.src_guid AND s.version = r.src_version
              JOIN typ td ON true
              JOIN uri ud ON ud.id = td.uri_id AND ud.ml || '.' || td.xml = r.dst_type
              JOIN res d  ON d.typ_id = td.id AND d.guid = r.dst_guid AND d.version = r.dst_version
            ON CONFLICT DO NOTHING
            )sql"
        );
    }

    std::vector<std::string> src_types, src_versions, dst_types, dst_versions;
    std::vector<oes::core::Guid> src_guids, dst_guids;
    std::vector<eml_int64> xpa_ids;
    std::vector<int> cnts;
    for (size_t i = 0; i < relations.size(); ++i) {
        const ReferenceRel& rel = relations[i];
        src_types.push_back(rel.src_type);
        src_guids.push_back(rel.src_guid);
        src_versions.push_back(rel.src_version);
        dst_types.push_back(rel.dst_type);
        dst_guids.push_back(rel.dst_guid);
        dst_versions.push_back(rel.dst_version);
        xpa_ids.push_back(xpa_info.at(static_cast<int>(i)));
        cnts.push_back(rel.cnt);
    }

    auto& stmt = *ins_rels_from_uid_;
    const size_t chunk = std::max<size_t>(bulk_size, 1);
    for (size_t start = 0; start < relations.size(); start += chunk) {
        const size_t end = std::min(start + chunk, relations.size());
        stmt.bindTextArray(1, src_types.cbegin() + start, src_types.cbegin() + end);
        stmt.bind<oes::core::Guid>(2, src_guids.cbegin() + start, src_guids.cbegin() + end);
        stmt.bindTextArray(3, src_versions.cbegin() + start, src_versions.cbegin() + end);
        stmt.bindTextArray(4, dst_types.cbegin() + start, dst_types.cbegin() + end);
        stmt.bind<oes::core::Guid>(5, dst_guids.cbegin() + start, dst_guids.cbegin() + end);
        stmt.bindTextArray(6, dst_versions.cbegin() + start, dst_versions.cbegin() + end);
        stmt.bind<eml_int64>(7, xpa_ids.cbegin() + start, xpa_ids.cbegin() + end);
        stmt.bind<int>(8, cnts.cbegin() + start, cnts.cbegin() + end);
        stmt.execute();
        stmt.reset();
    }
}

/*!
 * Get the resources reachable from the given ones, themselves included
*/
std::vector<TypedUid> SpaceWriter::get_related_uuids( const std::vector<TypedUid>& uids )
{
    std::vector<oes::core::Guid> guids;
    std::vector<std::string> types;
    guids.reserve(uids.size());
    types.reserve(uids.size());
    for (const auto& [etp_type, guid] : uids) {
        guids.push_back(guid);
        types.push_back(etp_type);
    }

    sel_rel_any_uuids_->bind(1, guids);
    sel_rel_any_uuids_->bindTextArray(2, types);
    std::vector<TypedUid> related;
    while(sel_rel_any_uuids_->step()){
        related.emplace_back(
            sel_rel_any_uuids_->getString(0),
            oes::core::Guid::parse(sel_rel_any_uuids_->getString(1))
        );
    }
    sel_rel_any_uuids_->reset();
    return related;
}

} // namespace oes::eml::openkv
