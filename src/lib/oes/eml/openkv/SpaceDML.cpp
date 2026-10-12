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

// WARNING: For now, internal caches are never reset, so to be safe,
// use a new instance per transaction.
SpaceReader::SpaceReader(
    oes::postgresql::Connection_ptr eml_db
) : db_(std::move(eml_db))
{
    prepare(cnt_rels_, R"sql(

select (select count(*) from rel where obj_id = res.obj_id) as targets,
       (select count(*) from rel where dst_id = res.obj_id) as sources
  from res
 where obj_id = $1

)sql"
    );

    prepare(edg_rels_, R"sql(
select rel.obj_id, rel.dst_id, xpa.th
  from rel
  join xpa on rel.xpa_id = xpa.id
  where rel.obj_id = ANY($1) and rel.dst_id = ANY($1)
)sql"
    );

    // Cannot use iif() yet (since 3.32). Use more verbose CASE instead for now.
    prepare(sel_ary_, R"sql(
select id, rank1, dim1, dim2, dim3, dim4, type,
       coalesce(usize, 0), coalesce(zcomp, -1),
       coalesce(chunk_length,0) as chunk_len,
       coalesce(length(value), case when dim1=0 then 0 else null end, -1) as blob_len
  from ary
  where path = $1
)sql"
    );

    prepare(sel_ary_val_, R"sql(
select value
  from ary
 where id = $1
)sql"
    );
    prepare(sel_bin_val_, R"sql(
select value
  from bin
 where id = $1
)sql"
    );
    prepare(sel_bin_, R"sql(
select id, length(value)
  from bin
 where ary_id = $1
   and idx = $2
)sql"
    );
    prepare(get_ref_spc_, R"sql(
SELECT target_sp FROM admin.space_rel rel
JOIN res ON res.space_rel_id = rel.id
WHERE res.guid = $1 AND res.space_rel_id IS NOT NULL;
)sql"
    );
    prepare(get_rel_spc_, R"sql(
SELECT guid, space_rel_id FROM res WHERE space_rel_id IS NOT NULL AND guid = ANY($1::uuid[]);
)sql"
    );
    prepare(get_dst_spc_, R"sql(
SELECT target_sp FROM admin.space_rel WHERE id = $1::bigint;
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

SqlAry SpaceReader::select_ary(const std::string& path) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "SpaceReader." + std::string(__func__),
        {{"path", path}});

    auto& stmt = *sel_ary_;
    bindTextNoCopy(stmt, 1, path);

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
    stmt.bind(1); // Just in case, clear non-transient text binding
    return ary;
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

void SpaceReader::prepare(Statement_opt& stmt, std::string sql) const {
    boost::trim(sql);
    stmt.emplace(db_->prepare(std::move(sql)));
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


void SpaceReader::bindTextNoCopy(oes::postgresql::Statement& stmt, int index, const std::string& text) {
    stmt.bind(index,text);
}

EmlUri SpaceReader::containing_dataspace_uri(EmlUri uri){
    get_ref_spc_->bind(1, uri.guid_.begin(), int(Guid::static_size()));
    
    if(get_ref_spc_->step()){
        uri.dataspace_ = get_ref_spc_->getString(0);
    }
    get_ref_spc_->reset();
    
    return uri;
}

/*!
 * Get dataspace relation ids for the given guids, when not null
 * \param uids The vector of resource guids
 * \return A map of space relation guids and their corresponding dataspace ids
*/
std::map<Guid,eml_int64> SpaceReader::containing_dataspace_ids(const std::vector<Guid>& uids){
    
    // Decompose the vector of Guids into arrays of 50 elements
    std::vector<std::vector<Guid>> uid_array_slices;
    const int nb_element = 20;
    std::vector<Guid> array_slice1;
    for (size_t i = 0; i < uids.size(); ++i) {
        array_slice1.push_back(uids[i]);
        if (array_slice1.size() == nb_element || i == uids.size() - 1) {
            uid_array_slices.push_back(array_slice1);
            array_slice1.clear();
        }
    }

    // process individual arrays
    std::map<Guid,eml_int64> spc_ids;
    for (auto& array_slice : uid_array_slices) {
        if( array_slice.empty() ){
            continue;
        }

        auto stmt = *get_rel_spc_;
        try {
            stmt.bind(1,array_slice);
            while (stmt.step()) {
                spc_ids.insert({stmt.getGuid(0),stmt.getInt64(1)});
            }
            stmt.reset();
        } catch (const std::exception& e) {
            std::string uid_str;
            for(auto& sl : array_slice){
                uid_str += sl.toXmlString() + ", ";
            }
            OES_ERROR("Error while getting import space with guids [{}]: {}", uid_str, e.what());
            throw std::runtime_error(
                fmt::format("Error while getting import space with guids [{}]: {}", uid_str, e.what())
            );
        }
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
select guid, name, uri.ns, uri.pp, uri.ml, uri.vs, xml, version
, createdAt, updatedAt
, createdBy, updatedBy, xdata_json, xdata_avro
, space_rel_id
    from (
        with rescache as (
            select obj_id, 0 as level from res
                where res.typ_id = (
                    select typ.id from typ
                        join uri on uri.id = typ.uri_id
                        where (uri.ml || '.' || typ.xml) = $1
                )
                and res.guid = $2
                -- and res.version = $3
        )
        select obj_id, MIN(level) as level from (
            WITH RECURSIVE children_of(obj_id, level) as (
                select obj_id, 0 as level from rescache
                UNION
                select dst_id, level + 1 from rel
                    join children_of c on c.obj_id = rel.obj_id
                    where level < 10
            )
            select obj_id, level from children_of
        ) c group by obj_id
    ) as ids
    join res on res.obj_id = ids.obj_id
    join typ on typ.id = res.typ_id
    join uri on uri.id = typ.uri_id
    order by ids.level
)sql"
        );
    }
    EmlUri eml_uri = EmlUri::parse(uri);
    const std::string st = eml_uri.to_type();
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
 * Get the Relation information where the given URI is the source
*/
std::vector<ReferenceRel> SpaceReader::get_relations( const Guid& uuid )
{
    if(!sel_rels_){
        prepare(sel_rels_, R"sql(
select res1.guid, res1.version, res2.guid, res2.version, xpa.th, rel.cnt from res as res1
join rel as rel on rel.obj_id = res1.obj_id
join res as res2 on rel.dst_id = res2.obj_id
join xpa on rel.xpa_id = xpa.id
where res1.guid = $1
--and res1.version = $2
        )sql"
        );
    }
    sel_rels_->bind(1, uuid.begin(), int(Guid::static_size()));
    // sel_rels_->bind(2, version);
    std::vector<ReferenceRel> rels;
    while(sel_rels_->step()){
        ReferenceRel ref;
        int res_idx = 0;
        ref.src_guid = sel_rels_->getGuid(res_idx++);
        ref.src_version = sel_rels_->getString(res_idx++);
        ref.dst_guid = sel_rels_->getGuid(res_idx++);
        ref.dst_version = sel_rels_->getString(res_idx++);
        ref.xpath = sel_rels_->getString(res_idx++);
        ref.cnt = sel_rels_->getInt(res_idx++);
        rels.push_back(ref);
    }
    sel_rels_->reset();
    return rels;
}

//============================================================================

// WARNING: For now, internal caches are never reset, so to be safe,
// use a new instance per transaction.
SpaceWriter::SpaceWriter(
    oes::postgresql::Connection_ptr eml_db
) : db_(std::move(eml_db))
{
    // Prep all statements up-front, thus detecting schema issues early
    prepare(ins_uri_, "insert into uri (ns, pp, ml, vs) values ($1, $2, $3, $4) returning id");
    prepare(ins_typ_, "insert into typ (id, uri_id, xml) values (DEFAULT, $1, $2) returning id");
    prepare(ins_xpa_, "insert into xpa (th) values ($1) returning id");
    prepare(ins_obj_, "insert into obj (id, xml) values ($1, $2) returning id");
    prepare(upd_obj_, "update obj set xml = $1 where id = $2");
    prepare(del_null_rel_, "DELETE FROM rel USING res WHERE rel.obj_id = res.obj_id AND res.guid is NULL");
    prepare(del_null_res_, "DELETE FROM res WHERE guid is NULL");
    prepare(del_del_res_, "delete from del_res where guid = $1");
    prepare(del_rel_, "delete from rel where obj_id = $1");
    prepare(del_ary_, "delete from ary where id = $1");
    prepare(del_bin_, "delete from bin where ary_id = $1");
    prepare(sel_ary_id_, "select id from ary where path = $1");
    // statement to set guid null in res table
    prepare(null_res_uuid_, "update res set guid = null where obj_id = $1");
    prepare(add_del_res_, R"sql(INSERT INTO del_res(guid, typ_id, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, storeDeleted)
    SELECT guid, typ_id, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, now()
    FROM res WHERE obj_id = $1 AND guid IS NOT NULL ON CONFLICT ON CONSTRAINT del_res_guid_typ_id_key DO NOTHING
    )sql");
    prepare(batch_add_del_res_, R"sql(INSERT INTO del_res(guid, typ_id, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, storeDeleted)
    SELECT guid, typ_id, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, now()
    FROM res WHERE obj_id = ANY($1::bigint[]) AND guid IS NOT NULL ON CONFLICT ON CONSTRAINT del_res_guid_typ_id_key DO NOTHING
    )sql");
    prepare(upd_dst_id_, "update rel set dst_id = $1 where dst_id = $2");

    // Uses rel_by_dst non-UNIQUE index.
    prepare(sel_rel_by_dst_, "select id, obj_id from rel where dst_id = $1");
    prepare(sel_obj_id_, R"sql(

select obj_id
  from res
 where typ_id = $1
   and guid = $2

)sql"
    );

    prepare(sel_obj_id_xml_, R"sql(
select obj_id, xml
  from res
  join obj on id=res.obj_id
 where typ_id = $1
   and guid = $2
)sql"
    );

    // For julianday() calculation below, see
    // https://sqlite.org/forum/forumpost/8742abd5bb
    prepare(ins_res_, R"sql(

insert into res
( typ_id, guid, name, version
, createdAt, updatedAt
, createdBy, updatedBy, xdata_json, xdata_avro, space_rel_id
) values
( $1, $2, $3, $4
, $5, $6
, $7, $8, $9, $10, $11
) returning obj_id

)sql"
    );

    prepare(ins_res_no_ovwrt_, R"sql(
insert into res
( typ_id, guid, name, version
, createdAt, updatedAt
, createdBy, updatedBy, xdata_json, xdata_avro, space_rel_id
) values
( $1, $2, $3, $4
, $5, $6
, $7, $8, $9, $10, $11
) ON CONFLICT DO NOTHING RETURNING obj_id
)sql"
    );

    // Duplicates are common obj_WellboreMarkerFrameRepresentation with
    // xpath rsq:WellboreMarker/rsq:Interpretation, because can contain
    // many markers, which can have the same interpretations.
    prepare(ins_rel_, R"sql(

insert into rel
(obj_id, dst_id, xpa_id
) values
($1, $2, $3)
on conflict(obj_id, dst_id, xpa_id) -- UPSERT
do update set cnt = rel.cnt + 1

)sql"
    );

    prepare(ins_ary_, R"sql(
insert into ary
( obj_id, xpa_id, path, type
, rank1, dim1, dim2, dim3, dim4
, usize
, value, chunk_length
) values
( $1, $2, $3, $4
, $5, $6, $7, $8, $9
, $10
, $11, $12
) returning id

)sql"
    );

    prepare(ups_ary_, R"sql(
insert into ary
( obj_id, xpa_id, path, type
, rank1, dim1, dim2, dim3, dim4
, usize
, value, chunk_length
) values
( $1, $2, $3, $4
, $5, $6, $7, $8, $9
, $10
, $11, $12
)
on conflict(path) -- UPSERT
do update set
  obj_id = excluded.obj_id
, xpa_id = excluded.xpa_id
, type   = excluded.type
, rank1  = excluded.rank1
, dim1   = excluded.dim1
, dim2   = excluded.dim2
, dim3   = excluded.dim3
, dim4   = excluded.dim4
, usize  = excluded.usize
, value  = excluded.value
, chunk_length  = excluded.chunk_length
 returning id
)sql"
    );

    prepare(ins_bin_, R"sql(

insert into bin (ary_id, idx, value) values ($1, $2, $3)
    on conflict (ary_id, idx)
    do update set value  = excluded.value
    returning id
)sql"
    );
    prepare(upd_ary_, R"sql(

update ary
   set obj_id = $1,
       xpa_id = $2
 where id = $3

)sql"
    );

    prepare(res_check_, R"sql(
select from res where guid = $1
)sql"
    );

    // Cannot use iif() yet (since 3.32). Use more verbose CASE instead for now.
    prepare(sel_ary_, R"sql(
select id, rank1, dim1, dim2, dim3, dim4, type,
       coalesce(usize, 0), coalesce(zcomp, -1),
       coalesce(chunk_length,0) as chunk_len,
       coalesce(length(value), case when dim1=0 then 0 else null end, -1) as blob_len
  from ary
 where path = $1
)sql"
    );

    prepare(sel_ary_ids_, R"sql(

select id, path, xpa_id
  from ary
 where obj_id = $1
 order by path collate "C"

)sql"
    );

    prepare(sel_arr_obj_, R"sql(select id, obj_id, xpa_id from ary where path = $1)sql");

    prepare(sel_bin_, R"sql(
select id, length(value)
  from bin
 where ary_id = $1
   and idx = $2
    )sql");
    prepare(sel_ary_val_, R"sql(
select value
  from ary
  where id = $1
    )sql");
    prepare(sel_bin_val_, R"sql(
select value
  from bin
  where id = $1
    )sql");
    prepare(upd_ary_val_, R"sql(
update ary set value=$1
  where id = $2
    )sql");
    prepare(upd_bin_val_, R"sql(
update bin set value=$1
  where id = $2
    )sql");
    prepare(sel_rel_any_uuids_, R"sql(
select guid::text from (
    with rescache as (
        select obj_id from res where res.guid = ANY($1::uuid[])        
    )
    select obj_id, MIN(level) as level from (
        WITH RECURSIVE children_of(obj_id, level) as (
            select obj_id, 0 as level from rescache
            UNION
            select dst_id, level + 1 from rel
                join children_of c on c.obj_id = rel.obj_id
                where level < 10
        )
        select obj_id, level from children_of
    ) c group by obj_id
) as ids join res on res.obj_id = ids.obj_id
)sql");
    delete_objects_ = std::string(R"sql(
create temp table {} (
    typ_id INT,
    guid uuid,
    obj_id bigint 
) on commit drop;
with val as ( select typ_id, guid::uuid from (
	  values
		{}
	   ) s(typ_id, guid))
insert into {}
select 
	v.typ_id, v.guid, coalesce(r.obj_id, -1)::bigint as obj_id from val v
	left join res r on r.typ_id = v.typ_id and r.guid = v.guid;

-- Copy deleted res into del_res
insert into del_res
    (
     guid, typ_id, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json,
	 xdata_avro, storeCreated, storeUpdated, storeDeleted
    )
    select r.guid, r.typ_id, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json,
	       xdata_avro, storeCreated, storeUpdated, now()
    FROM res r
	join {} on r.obj_id = {}.obj_id
	where r.guid is not null and {}.obj_id!=(-1)
	on conflict on constraint del_res_guid_typ_id_key do nothing;

-- Nullify guid
update res r
	set guid = null
from {}
	where r.obj_id={}.obj_id and {}.obj_id!=(-1);

-- select the temp table content so we can give back objects that were deleted, and objects which were not found.
select obj_id, typ_id, guid  from {};
)sql");

    // Note that 0 and 1 are stored more efficiently in SQLite than >1,
    // and that uri_of() and typ_of() generate IDs starting at 1, thus
    // the insert logic below (i.e. force 0 and 1 when empty)

    // pre-load URIs, TYPs, XPAs. OBJs pre-load is postponed (lazy),
    // since can be much larger than the above ables, and may not be necessary.
    {
        oes::postgresql::Statement stmt = db_->prepare("select id, ns, pp, ml, vs from uri");
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
            }
            uri_map_.emplace(UriKey(ns,vs),  id);
        }
        stmt.reset();
    }
    {
        oes::postgresql::Statement stmt = db_->prepare("select id, uri_id, xml from typ");
        while (stmt.step()) {
            int id = static_cast<int>(stmt.getInt64(0));
            int uri_id = static_cast<int>(stmt.getInt64(1));
            std::string xml = stmt.getString(2);
            typ_map_.emplace(TypKey{ uri_id, std::move(xml) }, id);
        }
        stmt.reset();
    }
    {
        oes::postgresql::Statement stmt = db_->prepare("select id, th from xpa");
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
    oes::postgresql::Statement stmt = db_->prepare("select obj_id, typ_id, guid from res");
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
    auto stmt = db_->prepare("select typ_id, guid from res where obj_id = $1");

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
* Check if the given uuid exists in the resource table
* \param uuid The uuid to check
* \param in_transaction Whether the check is done in a transaction or not. If true, the uuid is checked at the end of the transaction
* \return true if the uuid exists, false otherwise
*/
bool SpaceWriter::check_ary_container(const oes::core::Guid& uuid, bool in_transaction){
    if(in_transaction){
        array_containers_.emplace(uuid);
        return true;
    }else{
        auto& stmt = *res_check_;
        stmt.bind(1, uuid);
        bool has_row = stmt.step();
        stmt.reset();
        return has_row;
    }
}

/*
* Check if all array containers used by the writer are present in the resource table.
* This needs to be called at the end of a transaction
* \return A vector of uuids of the missing array containers
*/
std::vector<std::string> SpaceWriter::check_pending_ary_containers(){
    std::vector<std::string> missing;
    auto& stmt = *res_check_;
    for(auto& uuid : array_containers_){
        stmt.bind(1, uuid);
        if(!stmt.step()){
            missing.push_back(uuid.toXmlString());
        }
        stmt.reset();
    }
    return missing;
}

void SpaceWriter::prepare(Statement_opt& stmt, std::string sql) const {
    boost::trim(sql);
    stmt.emplace(db_->prepare(std::move(sql)));
}

eml_int64 SpaceWriter::uri_of(
    const std::string& ns_uri,
    const std::string& ns_prefix,
    const std::string& etp_ml,
    const std::string& version
) {
    std::vector<std::string> va;
    boost::split(va, version, boost::algorithm::is_any_of("."));
    std::string v = version;
    if(va.size()>2){
        v = va[0]+"."+va[1];
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
 * Deletes the deleted resource with the given UUID from the database.
 * Used when re-importing a resource that was previously deleted.
 * 
 * @param uuid The UUID of the deleted resource to be deleted.
 */
void SpaceWriter::delete_deleted_res(const oes::core::Guid& uuid) {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("SpaceWriter." + std::string(__func__));
    auto& stmt = *del_del_res_;

    int idx = 0;
    bindGuid(stmt, ++idx, uuid);
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
        }
    }

    OES_ASSERT(uri_id != -1, return ref;);
    ref.res_key.first = typ_of(uri_id, qType[1]);

    ref.xpa_id = xpa_of(xpath);
    ref.res_key.second.reparse(r.Uuid_.c_str(), r.Uuid_.length(), false);

    ref.obj_id = obj_id;

    return ref;
}

bool SpaceWriter::insert_rel(const ObjRef& ref) {
    auto dst_found = res_map_.find(ref.res_key);
    if (dst_found == res_map_.end()) {
        if (fill_res_map()) {
            // entries added, so retry the lookup
            dst_found = res_map_.find(ref.res_key);
        }
        if (dst_found == res_map_.end()) {
           ++missing_refs_[ref];
            return false;
        }
    }
    const eml_int64 dst_id = dst_found->second;
    do_insert_rel(ref, dst_id);
    return true;
}

bool SpaceWriter::bulk_insert_rels(const std::vector<oes::eml::openkv::ObjRef>& refs) {
    bool missing = false;
    std::vector<struct RelToAdd> relations;
    for( const ObjRef& ref: refs ) {
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

    do_bulk_insert_rels(relations, 50);
    return missing;
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

namespace postgresql_utils {
int getInt( const oes::postgresql::PGresult_uptr& result, int row, int col) {
    int num_bytes = PQgetlength(result.get(), row, col);
    char* value = PQgetvalue(result.get(), row, col);
    if (num_bytes == 0 || !value) {
        return 0;
    }
    return std::stoi(value) ;
}

eml_int64 getInt64( const oes::postgresql::PGresult_uptr& result, int row, int col) {
    int num_bytes = PQgetlength(result.get(), row, col);
    char* value = PQgetvalue(result.get(), row, col);
    if (num_bytes == 0 || !value) {
        return 0;
    }
    return std::stoll(value) ;
}


oes::core::Guid getGuid( const oes::postgresql::PGresult_uptr& result, int row, int col) {
    int num_bytes = PQgetlength(result.get(), row, col);
    char* value = PQgetvalue(result.get(), row, col);
    if (num_bytes == 0 || !value) {
        return oes::core::Guid::invalid() ;
    }
    return oes::core::Guid::parse(value) ;
}
}

/*!
* delete all objects that match the given vector of (typ_id, uuid) pairs.
*/
std::vector<std::tuple<eml_int64, int, oes::core::Guid> > SpaceWriter::delete_objects(
    const std::vector<std::pair<int, oes::core::Guid> >& objects
)
{
    const std::string tablename = std::string("temporary_") + oes::core::Guid::make().toStringNoDash() ;
    std::vector<std::string> strings;
    for (auto& object : objects) {
        strings.push_back(
            std::string("(") +
            std::to_string(object.first) + // typ_id
            std::string(",'") +
            object.second.toXmlString() + // uuid
            std::string("')")
        );
    }
    std::string values = boost::algorithm::join(strings, ",");
    std::string sql = fmt::format(delete_objects_, tablename, values, tablename, tablename, tablename, tablename, tablename, tablename, tablename, tablename);
    auto sqlresult = db_->execute(std::move(sql));
    auto numRows = PQntuples(sqlresult.get());

    std::vector<std::tuple<eml_int64, int, oes::core::Guid> > result;
    for( auto i = 0 ; i< numRows ; i++) {
        const eml_int64 obj_id = postgresql_utils::getInt64(sqlresult, i, 0) ;
        const int typ_id = postgresql_utils::getInt(sqlresult, i, 1) ;
        const oes::core::Guid uid = postgresql_utils::getGuid(sqlresult, i, 2);

        if (obj_id != -1) {
            res_map_.erase(ResKey{ typ_id, uid });
        }
        result.emplace_back(std::make_tuple(obj_id, typ_id, uid));
    }
    sqlresult.reset();

    return result;
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
 * Build a bulk upsert query for inserting \param size elements.
 */
std::string build_bulk_rel_insert(int size) {
    std::string sql_first = R"sql(insert into rel(obj_id, dst_id, xpa_id) values ($1, $2, $3))sql";
    std::string sql_last = R"sql(,(${}, ${}, ${}) on conflict(obj_id, dst_id, xpa_id) do update set cnt = rel.cnt + 1)sql";
    //std::string sql_last = R"sql(,(${}, ${}, ${}) on conflict(obj_id, dst_id, xpa_id) do nothing)sql";
    std::string sql_value = R"sql(,(${}, ${}, ${}))sql";
    std::string sql ;
    for(auto i=0 ; i<size ; i++) {
        if( i==0) {
            sql += sql_first ;
            continue ;
        }
        if( i  == size-1 ) {
            sql += fmt::format(sql_last, 3*i+1, 3*i+2, 3*i+3);
            continue ;
        }
        sql += fmt::format(sql_value, 3*i+1, 3*i+2, 3*i+3);
    }
    return sql;
}

/*!
 * upsert relations in bulk groups.
 * \param relations the relations to insert.
 * \param bulk_size the number of elements to upsert in one query.
 *
 * Inserting in bulk allows to reduce the number of queries sent to the PostgreSQL server, greatly
 * improving the performances when a large number of elements need to be inserted. However increasing
 * the bulk size asymptotycally reaches a minimum very quickly, as expressed by those timings with 14000
 * relations:
 * - insert one by one: 35s
 * - bulk size 25: 1.8s
 * - bulk size 50: 1.5s
 * - bulk size 100: 1.3s
 * - bulk size 200: 1.2s
 * so the ideal bulk size is a tradeoff, high enough to get good performances, low enough to trigger
 * the mechanism as often as possible. Default is 50.
 */
void SpaceWriter::do_bulk_insert_rels(const std::vector<RelToAdd>& relations, int bulk_size) {
    const std::string sql = build_bulk_rel_insert(bulk_size);
    Statement_opt stmt;
    prepare(stmt, sql);
    auto &bulk = *stmt;

    // Prepared statement for single insert.
    auto& single = *ins_rel_;

    auto hash = [](const RelToAdd& r) { return r.obj_id + 1000 * r.dst_id + 1000000 * r.xpa_id; };
    auto equal = [](const RelToAdd& r1, const RelToAdd& r2) { return r1.obj_id==r2.obj_id && r1.dst_id==r2.dst_id && r1.xpa_id==r2.xpa_id; };

    // Number of bulk inserts to perform, and size of what is left to do.
    size_t steps = relations.size() / bulk_size;
    size_t left = relations.size() % bulk_size;
    for(size_t i = 0; i< steps ; i++ ) {
        const size_t start = i * bulk_size;

        // Per PostgreSQL documentation: a bulk upsert is fine, as long as the same row is not updated twice.
        // If that happens then a cardinality error is raised. So we make sure we do not have conflicting keys
        // in the bulk set.
        const std::vector<RelToAdd> sub_relations = {relations.begin() + start, relations.begin() + start + bulk_size};
        const std::unordered_set<RelToAdd, decltype(hash), decltype(equal)>
            unique_in_sub(sub_relations.begin(), sub_relations.end(), bulk_size/2, hash, equal);
        if( sub_relations.size() == unique_in_sub.size()) {
            // No dups in bulk group, insert in bulk.
            for( int j=0 ; j<bulk_size ; j++ ) {
                const size_t cur = (start + j);
                bulk.bind(3 * j + 1, relations[cur].obj_id);
                bulk.bind(3 * j + 2, relations[cur].dst_id);
                bulk.bind(3 * j + 3, relations[cur].xpa_id);
            }
            bulk.execute();
            bulk.reset();
        } else {
            // There are duplicates in the same bulk group, upsert one by one. In the worst case scenario
            // we theoretically could have all of our bulk statements containing duplicates, which would
            // defeat this optimization, but data consistency would still be maintained.Keeping the bulk
            // statement size low enough makes this less likely, ensuring we hit the optimized case more often.
            for( int j=0 ; j<bulk_size ; j++ ) {
                const size_t cur = (start + j);
                single.bind(1, relations[cur].obj_id);
                single.bind(2, relations[cur].dst_id);
                single.bind(3, relations[cur].xpa_id);
                single.execute();
                single.reset();
            }
        }
    }

    // Deal with what is left, again one by one. We could use a smaller bulk statement here as well, but
    // this will not buy us much if we keep the bulk statement size low enough.
    const size_t start = steps * bulk_size;
    for( size_t j=0 ; j<left ; j++ ) {
        const size_t cur = (start + j);
        single.bind(1, relations[cur].obj_id);
        single.bind(2, relations[cur].dst_id);
        single.bind(3, relations[cur].xpa_id);
        single.execute();
        single.reset();
    }
}

eml_int64 SpaceWriter::select_ary_id(const std::string& path) {
    auto& stmt = *sel_ary_id_;
    bindTextNoCopy(stmt, 1, path);

    if (!stmt.step()) {
        stmt.reset();
        return -1;
    }

    auto ary_id = stmt.getInt64(0);
    assert(!stmt.step());
    stmt.reset();
    stmt.bind(1); // Just in case, clear non-transient text binding
    return ary_id;
}

SqlAry SpaceWriter::select_ary(const std::string& path) {
    auto& stmt = *sel_ary_;
    bindTextNoCopy(stmt, 1, path);

    SqlAry ary;
    if (!stmt.step()) {
        stmt.reset();
        return ary;
    }

    ary.ary_id_ = stmt.getInt64(0);

    ary.rank_ = stmt.getInt(1);
    ary.dims_[0] = stmt.getInt(2);
    ary.dims_[1] = stmt.getInt(3);
    ary.dims_[2] = stmt.getInt(4);
    ary.dims_[3] = stmt.getInt(5);

    switch (stmt.getInt(6)) {
    case  0: ary.type_ = eAryOfInt     ; break;
    case  1: ary.type_ = eAryOfDouble  ; break;
    case  2: ary.type_ = eAryOfFloat   ; break;
    case  3: ary.type_ = eAryOfBytes   ; break;
    case  4: ary.type_ = eAryOfBoolean ; break;
    case  5: ary.type_ = eAryOfLong    ; break;
    case  6: ary.type_ = eAryOfString  ; break;
    }

    ary.usize_ = stmt.getInt64(7);
    ary.zcomp_ = stmt.getInt(8);

    ary.chunk_len_ = stmt.getInt(9);
    ary.blob_len_ = stmt.getInt(10); // -1 means not-a-blob; 0 means empty-blob

    assert(!stmt.step());
    stmt.reset();
    stmt.bind(1); // Just in case, clear non-transient text binding
    return ary;
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
 * Select the array containing object with the given path
*/
bool SpaceWriter::select_array_object(std::string& path, eml_int64& arr_id, eml_int64& obj_id, eml_int64& xpath_id){
    auto& stmt = *sel_arr_obj_;
    stmt.bind(1, path);
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
    // Below, we save both (obj_id, xpa_id), which in theory are UNIQUE (disabled now).
    // They are "redundant" with path, also UNIQUE. The 2-ints are more efficient, but
    // using them implies "rewriting" the XML to change the <PathInHdfFile> values, so
    // for now we take the "easy and pragmatic route". Also, when DataArrays are written
    // before DataObjects, we cannot know (obj_id, xpa_id) but still want to write the
    // arrays to the DB, connecting them to their objects (and xpas) once those come in later.

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
    bindTextNoCopy(stmt, ++idx, hdf.path); // NK (but see above)
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

void SpaceWriter::bindTextNoCopy(oes::postgresql::Statement& stmt, int index, const std::string& text) {
    stmt.bind(index,text);
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

std::vector<int64_t> SpaceWriter::insert_relations( const std::vector<ReferenceRel>& relations )
{
    if(!ins_rels_xpa_){
        prepare(ins_rels_xpa_,
            R"sql(
            INSERT INTO xpa(th)
            VALUES ($1)
            ON CONFLICT DO NOTHING;
            )sql"
        );
    }
    auto& stmt_xpa = *ins_rels_xpa_;
    for(auto& rel:relations){
        stmt_xpa.bind(1, rel.xpath);
        stmt_xpa.step();
        stmt_xpa.reset();
    }

    if(!ins_rels_from_uid_){
        prepare(ins_rels_from_uid_,
            R"sql(
            INSERT INTO rel(obj_id, dst_id, xpa_id, cnt)
            VALUES (
                (select obj_id from res where guid=$1 and version=$2),
                (select obj_id from res where guid=$3 and version=$4),
                (select id from xpa where th=$5), $6
            ) 
            ON CONFLICT DO NOTHING
            RETURNING id;
            )sql"
        );
    }

    auto& stmt = *ins_rels_from_uid_;

    std::vector<int64_t> ids;
    for(auto& rel:relations){
        int bind_idx = 0;
        stmt.bind(++bind_idx, rel.src_guid);
        stmt.bind(++bind_idx, rel.src_version);
        stmt.bind(++bind_idx, rel.dst_guid);
        stmt.bind(++bind_idx, rel.dst_version);
        stmt.bind(++bind_idx, rel.xpath);
        stmt.bind(++bind_idx, rel.cnt);
        stmt.step();
        ids.push_back(stmt.getInt64(0));
        stmt.reset();
    }
    return ids;
}

/*!
 * Get the Relation information where the given uuids is the source
*/
std::vector<std::string> SpaceWriter::get_related_uuids( const std::vector<oes::core::Guid>& uuids )
{
    sel_rel_any_uuids_->bind(1, uuids);
    // sel_rels_->bind(2, version);
    std::vector<std::string> related;
    while(sel_rel_any_uuids_->step()){
        related.push_back(sel_rel_any_uuids_->getString(0));
    }
    sel_rel_any_uuids_->reset();
    return related;
}

} // namespace oes::eml::openkv
