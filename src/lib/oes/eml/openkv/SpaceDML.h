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

#ifndef OES_EML_OPENKV_SPACEDML_H
#define OES_EML_OPENKV_SPACEDML_H

#include "../fwd.h"
#include "../export.h"

#include <oes/core/utils/Guid.h>
#include <oes/common/epc/FilePart.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Statement.h>
#include <oes/common/HdfAry.h>

typedef oes::common::hdf::hdf_int64 eml_int64;
typedef unsigned long long int eml_uint64;
typedef char db_blob;

#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>

namespace oes::eml::openkv {

using namespace oes::common::hdf;

//============================================================================

//! \em Natural-Key of a resource, per ETP (within a given ETP-Space)
using ResKey = std::pair<int, oes::core::Guid>; // type_id and uid

//! Same key, but with the ETP type (e.g. "resqml20.obj_MdDatum") instead of
//! its type_id, for the code paths which do not hold the typ table's ids.
using TypedUid = std::pair<std::string, oes::core::Guid>;

//! Identity of the resource a client-supplied object URI designates,
//! with its type in canonical form (see EmlUri::canonicalType()).
OES_EML_EXPORT TypedUid stored_uid(const EmlUri& uri);

//! A dataset is identified by its container and its path within it:
//! two EPC external parts may both hold the same path.
using AryKey = std::pair<oes::core::Guid, std::string>;

struct ObjRef {
    eml_int64 obj_id=-1;    //!< The obj containing the reference (\em child-row)
    eml_int64 xpa_id=-1;    //!< The XML path at which the reference was found
    ResKey res_key;         //!< The reference itself, via its type_id and uid.
                            //!< Must be "resolved" into type_id \b and obj_id

    auto as_tuple() const {
        // note .res_key 1st. See insert_uncompressed_obj() why.
        return std::tie(res_key, obj_id, xpa_id);
    }
};

inline
bool operator<(const ObjRef& lhs, const ObjRef& rhs) {
    return lhs.as_tuple() < rhs.as_tuple();
}

//============================================================================

struct DstRelId {
    eml_int64 obj_id;    //!< The obj containing the reference (\em child-row)
    eml_int64 rel_id;    //!< ROWID of the reference itself

    auto as_tuple() const {
        return std::tie(obj_id, rel_id);
    }
};

inline
bool operator<(const DstRelId& lhs, const DstRelId& rhs) {
    return lhs.as_tuple() < rhs.as_tuple();
}

using DstRelIds = std::vector<DstRelId>;

//============================================================================

struct OES_EML_EXPORT SqlBinRange {
    int chunk_len_ = 0;

    int first_chunk_ = 0;   //!< index of the first chunk/blob
    int last_chunk_ = 0;    //!< index of the last chunk/blob

    int front_off_ = 0;     //!< offset in the first chunk/blob
    int back_len_ = -1;     //!< length in the last chunk/blob

    bool valid() const;
    bool defined() const;
    bool is_single_chunk() const;
    bool has_front_partial_chunk() const;
    bool has_back_partial_chunk() const;

    int full_chunks_begin() const;
    int full_chunks_end() const;

    explicit operator bool() const {
        return defined();
    }

    auto as_tuple() const {
        return std::tie(chunk_len_, first_chunk_, last_chunk_, front_off_, back_len_);
    }
};

inline
bool operator==(const SqlBinRange& lhs, const SqlBinRange& rhs) {
    return lhs.as_tuple() == rhs.as_tuple();
}

//============================================================================

struct OES_EML_EXPORT SqlAry {
    eml_int64 ary_id_ = -1;
    oes::core::Guid epc_guid_; // NK, with path_
    std::string path_; // NK
    HdfAryType type_ = eAryOfVoid;

    int rank_ = 0;
    eml_int64 dims_[4] = { 0 };
    eml_int64 usize_ = 0;
    int chunk_len_ = 0; // when value is integer, = value
    int blob_len_ = -1; // when value is blob, = length(value), -1 otherwise
    int zcomp_ = -1;

    size_t bulk_idx = 0;

    bool valid() const;
    bool defined() const;
    bool is_in_row() const;
    bool is_single_blob() const;
    bool is_compressed() const;

    size_t size() const;
    size_t elem_size() const;
    size_t size(size_t elem_size) const;
    size_t byte_size() const;

    int chunk_count() const;
    int chunk_index(eml_int64 off) const;
    SqlBinRange chunk_range(int64_t off, int64_t len) const;

    bool get_dimensions(std::vector<int64_t>& dims) const;

    static SqlAry from(const HdfAry& ary, eml_int64 ary_id);
};

struct OES_EML_EXPORT SqlAryId {
    eml_int64 ary_id_ = -1;

    std::string path_;
    eml_int64 xpa_id_ = -1;

    bool valid() const;
};
using SqlAryIds = std::vector<SqlAryId>;

using EdgeRel = std::tuple<eml_int64, eml_int64, std::string>;

struct OES_EML_EXPORT ResourceRef {
    eml_int64 obj_id;
    oes::core::Guid guid;
    std::string name;
    std::string uri_namespace;
    std::string uri_prefix;
    std::string uri_etp_ml;
    std::string uri_ml_version;
    std::string xml_type;
    std::string version;
    eml_int64   created;
    eml_int64   updated;
    std::string created_by;
    std::string updated_by;
    std::string xdata_avro;
    std::string xdata_json;
    int64_t     space_rel_id;
};

struct OES_EML_EXPORT ReferenceRel {
    std::string src_type;   //!< ETP type, e.g. "resqml20.obj_MdDatum"
    oes::core::Guid src_guid;
    std::string src_version;
    std::string dst_type;
    oes::core::Guid dst_guid;
    std::string dst_version;
    std::string xpath;
    int         cnt;
};

//============================================================================

struct BulkObj_t {
    std::string typ_ml_str;
    oes::core::Guid guid;
    size_t idx;
    bool found = false;

    std::string xml;
    std::string name;
    std::string createdBy;
    int64_t createdAt;
    int64_t updatedAt;
    int64_t storeCreated;
    int64_t storeUpdated;
    int64_t space_rel_id;
};

using oes::common::hdf::HdfAryType;
struct BulkAry_t {
    std::string path;
    size_t idx; // original index in the request
    
    eml_int64 ary_id = -1;
    int rank = 0;
    eml_int64 dims[4] = { 0 };
    HdfAryType type = HdfAryType::eAryOfVoid;
    eml_int64 usize = 0;
    int chunk_len = 0;
    int blob_len = -1;
    int zcomp = -1;
};

struct AryVal_t {
    // int size = 0;
    // unsigned char* val = nullptr;
    int ary_sql_idx = -1;
    std::vector<uint8_t> bytes;
    std::string err;
};

struct BlobData_t {
    int ary_sql_idx = -1;
    eml_int64 bin_id = -1;
    int chunk_idx = 0;
    int blob_len = 0;
    std::vector<uint8_t> bytes;
};

class OES_EML_EXPORT SpaceCommon {
public:
    // * ARRAY
    // select single array
    SqlAry select_ary(const oes::core::Guid& epc_guid, const std::string& path);
    // select multiple arrays
    std::vector<SqlAry> bulk_get_ary(
        const std::vector<oes::core::Guid>& epc_guids,
        const std::vector<std::string>& paths,
        const std::vector<int>& indexes,
        size_t bulk_size
    );

protected:
    explicit SpaceCommon(oes::postgresql::Connection_ptr eml_db);

    using Statement_opt = std::optional<oes::postgresql::Statement>;
    void prepare(Statement_opt& stmt, std::string sql) const;

    void bindTextNoCopy(oes::postgresql::Statement& stmt, int index, const std::string& text);

    oes::postgresql::Connection_ptr db_;

    // ARY select array
    Statement_opt sel_ary_;
    Statement_opt bulk_sel_ary_;
    Statement_opt sel_ary_val_;
    
    // BIN
    Statement_opt sel_bin_;
    Statement_opt sel_bin_val_;
};

class OES_EML_EXPORT SpaceReader: public SpaceCommon {
public:
    explicit SpaceReader(oes::postgresql::Connection_ptr eml_db);
    ~SpaceReader();

    SpaceReader(SpaceReader&&) noexcept = default;
    SpaceReader& operator=(SpaceReader&&) = default;
    SpaceReader& operator=(const SpaceReader&) = delete;

    std::pair<int32_t, int32_t> count_dst_src_rels(eml_int64 obj_id);
    std::vector<EdgeRel> edge_rels(const std::vector<eml_int64>& obj_ids);

    const char* read_ary(
        const SqlAry& ary, unsigned char* dst, int src_len, int64_t src_off
    );

    void close_blob();

    oes::postgresql::Connection_ptr db() const { return db_; }

    EmlUri containing_dataspace_uri(EmlUri uri);
    std::map<TypedUid,eml_int64> containing_dataspace_ids(const std::vector<TypedUid>& uids, size_t bulk_size);
    std::string get_destination_dataspace(eml_int64 id);

    std::vector<ResourceRef> get_dependencies( const std::string& uri );
    std::vector<ReferenceRel> get_relations( const std::vector<TypedUid>& srcs, size_t bulk_size );

    static oes::postgresql::Statement_ptr get_get_object_stmt(oes::postgresql::Connection_ptr eml_db);
    std::vector<BulkObj_t> bulk_get_obj(
        const std::vector<oes::core::Guid>& uids,
        const std::vector<std::string>& typ,
        const std::vector<int>& indexes,
        size_t bulk_size);

    // std::vector<BulkAry_t> bulk_get_ary(
    //     const std::vector<std::string>& paths,
    //     const std::vector<int>& indexes,
    //     size_t bulk_size
    // );

    // in row
    std::vector<AryVal_t> bulk_get_ary_val(
        const std::vector<eml_int64>& ary_ids,
        const std::vector<int>& indexes,
        size_t bulk_size);

    // single blob
    // std::vector<BlobInfo_t> bulk_get_bin(
    //     const std::vector<eml_int64>& ary_ids,
    //     const std::vector<int>& bin_idx,
    //     const std::vector<int>& indexes,
    //     size_t bulk_size
    // );

    std::vector<BlobData_t> bulk_get_bin_val(
        const std::vector<eml_int64>& ary_ids,
        const std::vector<int>& bin_idx,
        const std::vector<int>& indexes,
        size_t bulk_size
    );


private:
    //! using Statement_opt = std::optional<oes::postgresql::Statement>;
    //! void prepare(Statement_opt& stmt, std::string sql) const;

    bool select_bin(
        eml_int64 ary_id, int idx,
        eml_int64& row_id, int& blob_len
    );
    const char* read_bin(
        const SqlAry& ary, unsigned char* dst, int src_len, int64_t src_off
    );

    const char* readBinBlob(eml_int64 row_id, void* buf, int64_t off, int64_t len);
    struct PgBytea {
        eml_int64 ary_id_ = -1;
        eml_int64 idx_id_ = -1;
        eml_int64 bin_id_ = -1;
        std::vector<char> pg_blob_;

        bool up_to_date(eml_int64 rowid, eml_int64 prev_rowid) const;
    };
    const char* readBlob(
        eml_int64 row_id,
        PgBytea& pp_blob, eml_int64& last_rowid,
        void* buf, int64_t off, int64_t len
    );
    //! void bindTextNoCopy(oes::postgresql::Statement& stmt, int index, const std::string& text);

private:
    //! oes::postgresql::Connection_ptr db_;

    Statement_opt cnt_rels_;
    Statement_opt edg_rels_;
    //! Statement_opt sel_ary_; ------> common
    //! Statement_opt sel_bin_;
    //! Statement_opt sel_ary_val_;
    Statement_opt bulk_sel_ary_val_;
    //! Statement_opt bulk_sel_ary_; ------> common
    //! Statement_opt sel_bin_val_;
    Statement_opt sel_deps_;
    Statement_opt sel_rels_;
    Statement_opt get_ref_spc_;
    Statement_opt get_rel_spc_;
    Statement_opt get_dst_spc_;
    Statement_opt bulk_sel_obj_;
    Statement_opt bulk_sel_bin_val_;
    // Statement_opt bulk_sel_bin_;
    PgBytea       cur_blob_;

    db_blob* p_ary_blob_ = nullptr;
    db_blob* p_bin_blob_ = nullptr;
    eml_int64 last_ary_rowid_ = -1;
    eml_int64 last_bin_rowid_ = -1;
};

//============================================================================

struct OES_EML_EXPORT RelToAdd {
    RelToAdd(eml_int64 obj, eml_int64 xpa, eml_int64 dst) : obj_id(obj), xpa_id(xpa), dst_id(dst){};
    eml_int64 obj_id;
    eml_int64 xpa_id;
    eml_int64 dst_id;
};

struct OES_EML_EXPORT ResData {
    int typ_id = 0;
    oes::core::Guid guid;
    std::string name;
    std::string version;
    std::string creation;    // ISO datetime string, empty means NULL
    std::string last_update; // ISO datetime string, empty means NULL
    std::string created_by;
    std::string updated_by;
    std::string xdata_json;  // empty means NULL
    // xdata_avro always NULL in the bulk path; use insert_res() for binary xdata
    // space_rel_id always NULL in the bulk path

    oes::common::epc::xml::AbstractCitedDataObject cited;
    std::vector<uint8_t> xdata_avro;
};

struct OES_EML_EXPORT ReferenceInfo {
    std::string orig_type;
    oes::core::Guid orig_guid;
    std::string dest_type;
    oes::core::Guid dest_guid;
    std::string xpath;

    std::string to_string() const;
};
    

class OES_EML_EXPORT SpaceWriter: public SpaceCommon {
public:
    explicit SpaceWriter(oes::postgresql::Connection_ptr eml_db);
    ~SpaceWriter();

    SpaceWriter(SpaceWriter&&) noexcept = default;
    SpaceWriter& operator=(SpaceWriter&&) = default;
    SpaceWriter& operator=(const SpaceWriter&) = delete;

    eml_int64 resqmlv20_uri_id() const { return rsq20_id_; }
    eml_int64 resqmlv22_uri_id() const { return rsq22_id_; }
    eml_int64 commonv20_uri_id() const { return eml20_id_; }
    eml_int64 commonv23_uri_id() const { return eml23_id_; }
    eml_int64 witsmlv21_uri_id() const { return wit21_id_; }
    eml_int64 prodmlv22_uri_id() const { return prd22_id_; }
    eml_int64 prodmlv23_uri_id() const { return prd23_id_; }

    eml_int64 uri_of(
        const std::string& ns_uri,
        const std::string& ns_prefix,
        const std::string& etp_ml,
        const std::string& version
    );
    int typ_of(eml_int64 uri_id, const std::string& xml_type);
    eml_int64 xpa_of(const std::string& xpath);
    std::unordered_map<int, eml_int64> xpa_of(
        const std::vector<std::string>& xpath,
        const std::vector<int>& indexes,
        size_t bulk_size
    );

    int64_t delete_null_res();
    eml_int64 insert_res(
        int typ_id,
        const oes::common::epc::xml::AbstractCitedDataObject& cited,
        const std::string& xdata_json = {},
        const std::vector<uint8_t>& xdata_avro = {}
    );
    bool check_ary_container(const TypedUid& container, bool in_transaction = false);
    bool has_ary_container(const TypedUid& container);
    std::vector<std::string> check_pending_ary_containers();
    void delete_deleted_res(const TypedUid& uid);

    // Bulk-check containers by (ETP type, uuid): marks all as seen (in_transaction=true) or queries the DB.
    // Returns, for each index, whether the container was found (or accepted in transaction mode).
    std::vector<bool> bulk_check_ary_containers(
        const std::vector<std::string>& types,
        const std::vector<oes::core::Guid>& uuids,
        const std::vector<int>& indexes,
        size_t bulk_size,
        bool in_transaction = false);

    struct AryObjInfo {
        eml_int64 ary_id  = -1;
        eml_int64 obj_id  = -1;
        eml_int64 xpa_id  = -1;
    };
    // Bulk-lookup arrays by (container, path), returning {ary_id, obj_id, xpa_id} for each one found.
    std::map<AryKey, AryObjInfo> bulk_select_ary_objects(
        const std::vector<oes::core::Guid>& epc_guids,
        const std::vector<std::string>& paths,
        size_t bulk_size);

    eml_int64 select_obj_id(int typ_id, const oes::core::Guid& uid);
    std::unordered_map<int, eml_int64> bulk_select_obj_ids(
        const std::vector<int>& typ_ids,
        const std::vector<oes::core::Guid>& guids,
        const std::vector<int>& indexes,
        size_t bulk_size
    );
    std::pair<eml_int64, std::string> select_obj_id_xml(int typ_id, const oes::core::Guid& uid);
    eml_int64 insert_uncompressed_obj(
        eml_int64 obj_id, 
        int typ_id, const oes::core::Guid& uid, const std::string& xml
    );
    void bulk_insert_uncompressed_objs(
        const std::vector<eml_int64>& obj_ids,
        const std::vector<std::string>& xmls,
        const std::vector<int>& indexes,
        size_t bulk_size
    );
    void bulk_insert_missing_refs(
        const std::vector<eml_int64>& obj_ids,
        const std::vector<eml_int64>& typ_ids,
        const std::vector<oes::core::Guid>& guids,
        size_t bulk_size
    );
    bool update_uncompressed_obj(eml_int64 obj_id, const std::string& xml);

    // Since we store obj_id for the reference target too,
    // we must wait to have all of them to resolve res_key.
    ObjRef to_obj_ref(
        eml_int64 obj_id, const std::string& xpath, const oes::common::epc::xml::DataObjectReference& r
    );
    bool bulk_insert_rels(
        const std::vector<oes::eml::openkv::ObjRef> &refs,
        size_t bulk_size
    );
    
    bool null_res_guid(int typ_id, const oes::core::Guid& uid);
    std::vector<eml_int64> bulk_null_res_guids(
        const std::vector<int>& typ_ids,
        const std::vector<oes::core::Guid> guids,
        const std::vector<int> indexes,
        size_t bulk_size
    );
    // Bulk-null GUIDs for a batch of existing objects. Returns old obj_ids (-1 if not found),
    // in the same order as type_uids. Internally uses batch_add_del_res_ + bulk_null_res_uuid_.
    //!std::vector<eml_int64> bulk_null_res_guids(const std::vector<std::pair<int, oes::core::Guid>>& type_uids, size_t bulk_size);

    // Bulk-insert a batch of res rows. Returns new obj_ids in the same order as data.
    // Also updates res_map_. Note: xdata_avro is always NULL in this path.
    std::unordered_map<size_t, eml_int64> bulk_insert_res(std::vector<ResData>&& data, const std::vector<size_t> index_map, size_t bulk_size);

    void update_dst_id(int64_t old_id, int64_t new_id);
    void bulk_update_dst_id(
        const std::vector<eml_int64>& old_ids,
        const std::vector<eml_int64>& new_ids,
        size_t bulk_size);
    DstRelIds select_rels_by_dst(eml_int64 dst_id);

    eml_int64 select_ary_id(const oes::core::Guid& epc_guid, const std::string& path);
    SqlAryIds select_ary_ids(eml_int64 obj_id);
    bool select_array_object(const oes::core::Guid& epc_guid, std::string& path, eml_int64& arr_id, eml_int64& obj_id, eml_int64& xpath_id);
    eml_int64 insert_ary(
        const HdfRef& hdf,
        const HdfAry& ary);
    eml_int64 upsert_ary(
        const HdfRef& hdf,
        const HdfAry& ary);

    std::unordered_map<int, eml_int64> bulk_upsert_ary(
        const std::vector<HdfRef>& refs,
        const std::vector<HdfAry>& arys,
        const std::vector<int>& indexes,
        size_t bulk_size);
    

    bool update_ary_ids(eml_int64 ary_id, eml_int64 obj_id, eml_int64 xpa_id);
    bool bulk_update_ary_ids(
        const std::vector<eml_int64>& obj_ids,
        const std::vector<eml_int64>& xpa_ids,
        const std::vector<eml_int64>& ary_ids,
        size_t bulk_size
    );
    const char* write_ary(const SqlAry& ary, const void* buf, eml_uint64 off, int len);
    void bulk_update_ary_values(
        const std::vector<eml_int64>& ary_ids,
        const std::vector<const char*>& values,
        const std::vector<size_t>& lens,
        size_t bulk_size);
    void bulk_upsert_bin_values(
        const std::vector<eml_int64>& ary_ids,
        const std::vector<int>& chunk_idxs,
        const std::vector<const char*>& values,
        const std::vector<size_t>& lens,
        size_t bulk_size);
    eml_int64 delete_ary(eml_int64 ary_id);
    void close_blob();

    size_t missing_reference_count() const {
        return missing_refs_.size();
    }

    const std::map<ObjRef, int>& missing_references() const {
        return missing_refs_;
    }

    size_t orphan_relation_count() const {
        return orphan_rels_.size();
    }

    std::vector<int64_t> insert_references( const std::vector<ResourceRef>& references );
    void insert_relations( const std::vector<ReferenceRel>& relations, size_t bulk_size );
    std::vector<TypedUid> get_related_uuids( const std::vector<TypedUid>& uids );

    std::vector<ReferenceInfo> get_references_infos();

private:
    // using Statement_opt = std::optional<oes::postgresql::Statement>;
    // void prepare(Statement_opt& stmt, std::string sql) const;

    bool fill_res_map();
    bool filled_res_map_ = false;

    void do_insert_rel(const ObjRef& ref, eml_int64 dst_id);
    void do_bulk_insert_rels(const std::vector<RelToAdd> &rels, int bulkSize = 50);

    int64_t do_insert_or_upsert_ary(
        oes::postgresql::Statement& stmt,
        const HdfRef& hdf, const HdfAry& ary,
        bool& in_row_blob, const void*& to_be_chunked
    );

    struct ChunkProps {
        int chunk_len = -1;

        int chunk_idx_beg = -1;
        int chunk_idx_end = -1;
        
        int last_chunk_idx = -1;
        int back_len = -1;
    };
    struct BulkInsertAryRes {
        eml_int64 ary_idx = -1;
        bool in_row = false;
        bool skip_bins = false; // bins already written, or superseded by a later duplicate

        
        const void* to_be_chunked = nullptr;
        uint64_t blob_size = 0;

        bool is_single_chunk = false;
        ChunkProps chunk; // use only if is_single_chunk = false;
    };
    std::unordered_map<int, BulkInsertAryRes> bulk_do_insert_or_upsert_ary(
        oes::postgresql::Statement& stmt,
        const std::vector<HdfRef>& refs,
        const std::vector<HdfAry>& arys,
        const std::vector<int>& indexes,
        size_t bulk_size);
    
    void bulk_insert_bin(
        std::unordered_map<int, BulkInsertAryRes>& to_add,
        const std::vector<int>& single_blob_idx,
        size_t bulk_size
    );

    bool select_bin(
        eml_int64 ary_id, int idx,
        eml_int64& row_id, eml_int64& blob_len
    );
    void insert_bin(const SqlAry& ary, const void* buf, uint64_t len);
    const char* write_bin(
        const SqlAry& ary, const void* buf, eml_uint64 off, uint64_t len
    );

    

    const char* writeAryBlob(
        eml_int64 row_id, const void* buf, eml_uint64 off, uint64_t len
    );
    const char* writeBinBlob(
        eml_int64 row_id, const void* buf, eml_uint64 off, uint64_t len
    );
    const char* writeBlob(
        const char* tab, eml_int64 row_id,
        db_blob** pp_blob, eml_int64& last_rowid,
        const void* buf, eml_uint64 off, uint64_t len
    );
    void bindZeroBlob(oes::postgresql::Statement& stmt, int index, int len);
    void bindBlobNoCopy(oes::postgresql::Statement& stmt, int index, const void* buf, int len);

private:

    int rsq_id_ = -1;
    int eml_id_ = -1;

    Statement_opt sel_all_uri_;
    Statement_opt sel_all_typ_;

    Statement_opt ins_uri_;
    Statement_opt ins_typ_;
    Statement_opt ins_xpa_;
    Statement_opt bulk_ins_xpa_;
    Statement_opt ins_res_;
    Statement_opt ins_res_no_ovwrt_;
    Statement_opt bulk_ins_res_;
    Statement_opt res_check_;
    Statement_opt res_guid_check_;
    Statement_opt bulk_res_check_; // SELECT guid FROM res WHERE guid = ANY($1::uuid[])
    Statement_opt del_null_res_;
    Statement_opt del_null_rel_;
    Statement_opt del_del_res_;
    Statement_opt sel_rel_by_dst_;
    Statement_opt ins_rel_;
    Statement_opt bulk_ins_rel_;
    Statement_opt del_rel_;
    Statement_opt ins_ary_;
    Statement_opt bulk_ins_ary_;
    Statement_opt ups_ary_;
    Statement_opt bulk_ups_ary_;
    Statement_opt ins_bin_;
    Statement_opt bulk_ins_bin_;
    Statement_opt ins_obj_;
    Statement_opt bulk_ins_obj_;
    Statement_opt upd_obj_;
    Statement_opt upd_ary_;
    Statement_opt bulk_upd_ary_;
    Statement_opt sel_obj_id_;
    Statement_opt bulk_sel_obj_id_;
    Statement_opt sel_obj_id_xml_;
    //! Statement_opt sel_ary_; --------> common
    //! Statement_opt bulk_sel_ary_; --------> common
    Statement_opt sel_ary_id_;
    Statement_opt sel_ary_ids_;
    Statement_opt del_ary_;
    //! Statement_opt sel_bin_;
    Statement_opt del_bin_;

    //! Statement_opt sel_ary_val_;
    Statement_opt bulk_upd_ary_val_;
    Statement_opt upd_ary_val_;
    //! Statement_opt sel_bin_val_;
    Statement_opt upd_bin_val_;

    Statement_opt null_res_uuid_;
    Statement_opt bulk_null_res_uuid_; // UPDATE res SET guid=NULL WHERE obj_id = ANY($1::bigint[])

    Statement_opt ins_rels_from_uid_;

    Statement_opt upd_dst_id_;
    Statement_opt bulk_upd_dst_id_;
    Statement_opt add_del_res_;
    Statement_opt batch_add_del_res_;

    Statement_opt sel_arr_obj_;
    Statement_opt bulk_sel_ary_obj_; // SELECT path, id, obj_id, xpa_id FROM ary WHERE path = ANY($1::text[])

    Statement_opt sel_rel_any_uuids_;

    db_blob* p_ary_blob_ = nullptr;
    db_blob* p_bin_blob_ = nullptr;
    eml_int64 last_ary_rowid_ = -1;
    eml_int64 last_bin_rowid_ = -1;

    using UriKey = std::pair<std::string,std::string>; // uri and version
    using TypKey = std::pair<eml_int64, std::string>; // uri_id and XML local name

    // TODO: Allow .find(std::string_view)
    // See https://stackoverflow.com/a/35525806/103724
    std::map<UriKey, eml_int64> uri_map_;
    std::map<TypKey, int> typ_map_;
    std::map<std::string, eml_int64> xpa_map_;
    std::map<ResKey, eml_int64> res_map_;

    std::map<ObjRef, int> missing_refs_;
    std::set<DstRelId> orphan_rels_;

    eml_int64 rsq20_id_;
    eml_int64 rsq22_id_;
    eml_int64 eml20_id_;
    eml_int64 eml23_id_;
    eml_int64 wit21_id_;
    eml_int64 prd22_id_;
    eml_int64 prd23_id_;
    std::set<TypedUid> array_containers_;
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_SPACEDML_H
