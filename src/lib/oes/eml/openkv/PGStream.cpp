#include "PGStream.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <libpq-fe.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>
#include <oes/postgresql/Connection.h>
#include <oes/common/etp12/Messages.h>

#include <array>
#include <cassert>
#include <iterator>


namespace oes::eml::openkv {

// Helper enum class SpaceTableStreamStatus functions
bool hasTableStreamStatus(const uint8_t raw_flags, const SpaceTableStreamStatus flag) {
    uint8_t status = static_cast<uint8_t>(flag);
    return (status & raw_flags) > 0;
}

void insertChunkFlag(uint8_t& flags) {
    flags |= static_cast<uint8_t>(SpaceTableStreamStatus::eChunk);
}

void insertFinalChunkFlag(uint8_t& flags) {
    flags |= static_cast<uint8_t>(SpaceTableStreamStatus::eFinalChunk);
}

// ========================================================================================================

// PGStream definition - base class definition
PGStream::PGStream(const Space& space, Connection_ptr&& conn)
    : m_space_(space),
      m_conn_(std::move(conn)) {}

PGStream::~PGStream() {}

// ========================================================================================================

// PGStreamer::Impl_ declaration and definition
class PGStreamer::Impl_ {
public:
    const Space& m_space;
    const std::string& m_schema;

    Impl_(const Space& space, Connection_ptr& conn, const bool has_ref, const size_t max_msg_size);
    ~Impl_();

    SpaceStreamStatus stream(TableData& td);
    
private:
    
    bool define_functions();
    bool call_requirements();
    bool clean();

    std::string prepare_cmd(const std::string& table);
    std::string get_admin_space_xdata();
    bool start_table_dump();
    bool end_table_dump();


private:
    PGconn* const pg_conn_;
    using tables_name_array_t = std::array<std::string, 10>;
    const static tables_name_array_t tables_;
    tables_name_array_t::const_iterator current_table_it_;
    oes::postgresql::Transaction tx_;

    char* buffer_ = nullptr;
    int len_ = 0;
    size_t offset_ = 0;
    
    size_t max_msg_size_ = 0;
    const bool has_ref;

    SpaceTableStreamStatus status_ = SpaceTableStreamStatus::eStart;
    
};

const PGStreamer::Impl_::tables_name_array_t PGStreamer::Impl_::tables_ = {
    "admin",
    "uri", "xpa",
    "typ",
    "del_res", "res",
    "obj", "rel",
    "ary",
    "bin"
};

PGStreamer::Impl_::Impl_(const Space& space, Connection_ptr& conn, const bool has_ref, const size_t max_msg_size)
    : m_space(space),
      m_schema(m_space.dbfile_),
      pg_conn_(conn->pq_connection()),
      current_table_it_(PGStreamer::Impl_::tables_.cbegin()),
      tx_(conn->begin_transaction()),
      max_msg_size_(static_cast<size_t>(max_msg_size*0.9)),
      has_ref(has_ref) {
    if (has_ref) {
        if (!define_functions()) {
            throw std::runtime_error("Can not prepare functions");
        }
    
        if (!call_requirements()) {
            throw std::runtime_error("Can not generate temporary tables");
        }
    }

}

PGStreamer::Impl_::~Impl_() {
    // if (!clean()) {
    //     OES_ERROR("Could not clean PGstreamer correctly");
    // }

    // In any case: rollback
    tx_.rollback();
}

bool PGStreamer::Impl_::define_functions() {
    bool ok = true;
    const std::string functions_stmt = R"sql(
-- Create helper temp_parent_ref_guid_dbfile helper table
CREATE OR REPLACE FUNCTION make_temp_parent_ref_guid_dbfile(child_schema text)
RETURNS void AS $$
DECLARE
    rec RECORD;
    sql TEXT;
BEGIN
    CREATE TEMPORARY TABLE  temp_parent_ref_guid_dbfile(
		dst_obj_id bigint,
		orig_obj_id bigint,
        res_guid uuid,
        dbfile text
    ) ON COMMIT DROP;

    EXECUTE FORMAT('
        INSERT INTO temp_parent_ref_guid_dbfile (dst_obj_id, res_guid, dbfile)
        SELECT dst_res.obj_id, dst_res.guid, s.dbfile
        FROM %I.res AS dst_res
        INNER JOIN admin.space_rel AS sr ON dst_res.space_rel_id = sr.id
        INNER JOIN admin.spaces AS s ON sr.target_sp = s.path;',
        child_schema
    );
	
	FOR rec IN (
        SELECT dbfile
        FROM (
            SELECT DISTINCT dbfile
            FROM temp_parent_ref_guid_dbfile -- or SELECT dbfile from parent_obj_map GROUP BY dbfile
        )
    ) LOOP
        sql := FORMAT(
            'UPDATE temp_parent_ref_guid_dbfile AS parent
			SET orig_obj_id = orig_res.obj_id
            FROM %I.res AS orig_res
			WHERE parent.res_guid = orig_res.guid AND parent.dbfile = %L;',
            rec.dbfile,
            rec.dbfile
        );

        EXECUTE sql;
    END LOOP;
END;
$$ LANGUAGE plpgsql;

-- Create deferenced object table
CREATE OR REPLACE FUNCTION make_temp_obj_result(child_schema text)
RETURNS void AS $$

DECLARE
    rec RECORD;
    sql TEXT;
BEGIN

    -- Get the original objects
    CREATE TEMPORARY TABLE temp_obj_result (
        id bigint,
        usize integer,
        zcomp integer,
        xml xml
    ) ON COMMIT DROP;

    FOR rec IN (
        SELECT dbfile
        FROM (
            SELECT DISTINCT dbfile
            FROM temp_parent_ref_guid_dbfile -- or SELECT dbfile from parent_obj_map GROUP BY dbfile
        )
    ) LOOP
        sql := FORMAT(
            'INSERT INTO temp_obj_result(id, usize, zcomp, xml)
            SELECT parent.dst_obj_id, orig_obj.usize, orig_obj.zcomp, orig_obj.xml
            FROM %I.obj AS orig_obj
            JOIN temp_parent_ref_guid_dbfile AS parent ON parent.orig_obj_id = orig_obj.id
            WHERE parent.dbfile = %L;',
            rec.dbfile,
            rec.dbfile
        );

        EXECUTE sql;
    END LOOP;

    -- non-referenced objects (space_rel_id IS NULL)
    EXECUTE FORMAT('
        INSERT INTO temp_obj_result(id, usize, zcomp, xml)
        SELECT id, usize, zcomp, xml
        FROM %I.obj AS o
        JOIN %I.res AS sr ON sr.obj_id=o.id
        WHERE sr.space_rel_id IS NULL;',
        child_schema,
        child_schema,
        child_schema,
        child_schema
    );
END;
$$ LANGUAGE plpgsql;

-- Create unified xpa table (merges all xpa tables, ignore duplicates)
CREATE OR REPLACE FUNCTION make_temp_xpa_result(child_schema text)
RETURNS TABLE(id bigint, th text) AS $$

DECLARE
    rec RECORD;
    sql TEXT;
BEGIN


    -- Get the original objects
    CREATE TEMPORARY TABLE temp_xpa_result (
        id bigserial primary key,
        th text
    ) ON COMMIT DROP;

    EXECUTE FORMAT('
        INSERT INTO temp_xpa_result(th)
        SELECT th
        FROM %I.xpa;',
        child_schema
    );

    -- insert missing parent's xpa
    FOR rec IN (
        SELECT dbfile
        FROM (
            SELECT DISTINCT dbfile
            FROM temp_parent_ref_guid_dbfile -- or SELECT dbfile from parent_obj_map GROUP BY dbfile
        )
    ) LOOP
        sql := FORMAT(
            'INSERT INTO temp_xpa_result(th)
            SELECT orig_xpa.th
            FROM %I.xpa AS orig_xpa
            WHERE NOT EXISTS (
                SELECT th FROM temp_xpa_result as r WHERE r.th = orig_xpa.th
            );',
            rec.dbfile
        );

        EXECUTE sql;
    END LOOP;

    RETURN QUERY
    SELECT * FROM temp_xpa_result;
END;
$$ LANGUAGE plpgsql;

-- Create unifies ARY-XPA-th helper table.
CREATE OR REPLACE FUNCTION make_temp_ary_xpa_result(child_schema text)
RETURNS VOID AS $$
DECLARE
    rec RECORD;
    sql TEXT;
BEGIN
    CREATE TEMPORARY TABLE temp_ary_xpa_result (
        id bigserial PRIMARY KEY,
        obj_id bigint,
        --xpa_id bigint, -- removed for now
        xpa_th text, -- XPA ADDED
        path text,
        type integer,
        rank1 integer,
        dim1 bigint,
        dim2 bigint,
        dim3 bigint,
        dim4 bigint,
        usize bigint,
        zcomp bigint,
        value bytea,
        chunk_length integer,
        orig_ary_id bigint,
        orig_ary_schema text

    ) ON COMMIT DROP;

    EXECUTE FORMAT('
        INSERT INTO temp_ary_xpa_result(obj_id, xpa_th, path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length, orig_ary_id, orig_ary_schema)
        SELECT a.obj_id, x.th, a.path, a.type, a.rank1, a.dim1, a.dim2, a.dim3, a.dim4, a.usize, a.zcomp, a.value, a.chunk_length, null, null
        FROM %I.ary AS a
        JOIN %I.xpa AS x ON a.xpa_id = x.id;',
        child_schema,
        child_schema
    );

    FOR rec IN (
        SELECT dbfile
        FROM (
            SELECT DISTINCT dbfile
            FROM temp_parent_ref_guid_dbfile -- or SELECT dbfile from parent_obj_map GROUP BY dbfile
        )
    ) LOOP
        sql := FORMAT(
            'INSERT INTO temp_ary_xpa_result(obj_id, xpa_th, path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length, orig_ary_id, orig_ary_schema)
            SELECT parent.dst_obj_id, x.th, oa.path, oa.type, oa.rank1, oa.dim1, oa.dim2, oa.dim3, oa.dim4, oa.usize, oa.zcomp, oa.value, oa.chunk_length, oa.id, parent.dbfile
            FROM %I.ary AS oa
            JOIN temp_parent_ref_guid_dbfile AS parent ON oa.obj_id = parent.orig_obj_id
            LEFT JOIN %I.xpa AS x ON oa.xpa_id = x.id
            WHERE parent.dbfile = %L;',
            rec.dbfile,
            rec.dbfile,
            rec.dbfile
        );

        EXECUTE sql;

    END LOOP;

END;
$$ LANGUAGE plpgsql;

-- Create bin result table
CREATE OR REPLACE FUNCTION make_temp_bin_result(child_schema text)
RETURNS VOID AS $$
DECLARE
    rec RECORD;
    sql TEXT;
BEGIN
    CREATE TEMPORARY TABLE temp_bin_result (
        id bigserial PRIMARY KEY,
        ary_id bigint,
        idx integer,
        value bytea
    ) ON COMMIT DROP;

    -- first add child ary
    EXECUTE FORMAT('
        INSERT INTO temp_bin_result(ary_id, idx, value)
        SELECT b.ary_id, b.idx, b.value
        FROM %I.bin AS b;',
        child_schema
    );

    FOR rec IN (
        SELECT dbfile
        FROM (
            SELECT DISTINCT dbfile
            FROM temp_parent_ref_guid_dbfile -- or SELECT dbfile from parent_obj_map GROUP BY dbfile
        )
    ) LOOP
        sql := FORMAT(
            'INSERT INTO temp_bin_result(ary_id, idx, value)
            SELECT ary.id, b.idx, b.value
            FROM %I.bin AS b
            JOIN temp_ary_xpa_result AS ary ON b.ary_id = ary.orig_ary_id
            WHERE ary.orig_ary_schema = %L;',
            rec.dbfile,
            rec.dbfile
        );

        EXECUTE sql;

    END LOOP;

END;
$$ LANGUAGE plpgsql;

)sql";
    PGresult* res_query(PQexec(pg_conn_, functions_stmt.c_str()));
    ExecStatusType st = PQresultStatus(res_query);
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        std::string msg (PQresultErrorMessage(res_query));
        msg += " | " + std::string(PQresultErrorField(res_query, PG_DIAG_SQLSTATE));
        OES_ERROR(msg);
        ok = false;
    }
    PQclear(res_query);
    

    return ok;
}

bool PGStreamer::Impl_::call_requirements() {
    const std::array<std::string, 5> rec_functions = {
        "make_temp_parent_ref_guid_dbfile",
        "make_temp_obj_result",
        "make_temp_xpa_result",
        "make_temp_ary_xpa_result",
        "make_temp_bin_result"
    };

    bool ok = true;
    std::string cmd;
    for (const auto& f: rec_functions) {
        cmd = fmt::format("SELECT {}('{}');", f, m_schema);
        PGresult* res = PQexec(pg_conn_, cmd.c_str());
        auto st = PQresultStatus(res);
        if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
            std::string msg (PQresultErrorMessage(res));
            // TODO: more info with PQresultErrorField
            msg += " | " + std::string(PQresultErrorField(res, PG_DIAG_SQLSTATE));
            OES_ERROR("Failed to select {}: {} - {} - {}", f, static_cast<int>(st), cmd, msg);
            ok = false;
        }
        PQclear(res);

        if (!ok) {
            break;
        }
    }

    return ok;
}

bool PGStreamer::Impl_::clean() {
    if (!has_ref) { // nothing to clean
        return true;
    }
    const std::array<std::string, 10> to_clean = {
        "make_temp_parent_ref_guid_dbfile",
        "make_temp_obj_result",
        "make_temp_xpa_result",
        "make_temp_ary_xpa_result",
        "make_temp_bin_result",
        "temp_parent_ref_guid_dbfile",
        "temp_obj_result",
        "temp_xpa_result",
        "temp_ary_xpa_result",
        "temp_bin_result"
    };

    bool ok = true;
    std::string cmd;
    for (const auto& c: to_clean) {
        if (c.at(0) == 'm') {
            cmd = fmt::format("DROP FUNCTION {};", c);
        } else {
            cmd = fmt::format("DROP TABLE {};", c);
        }
        PGresult* res = PQexec(pg_conn_, cmd.c_str());
        auto st = PQresultStatus(res);
        if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
            std::string msg (PQresultErrorMessage(res));
            // TODO: more info with PQresultErrorField
            msg += " | " + std::string(PQresultErrorField(res, PG_DIAG_SQLSTATE));
            OES_ERROR("Failed to drop {}: {} - {} - {}", c, static_cast<int>(st), cmd, msg);
            ok = false;
        }
        PQclear(res);
    }

    return ok;
}

std::string PGStreamer::Impl_::prepare_cmd(const std::string& table) {
    if (!has_ref || 
        table == "uri" ||
        table == "typ" ||
        table == "del_res"
    ) {
        return fmt::format("COPY {}.{} TO STDOUT WITH (FORMAT BINARY)", m_schema, table);
    } else if (table == "res") {
        std::string sql = R"sql(
COPY (
    SELECT obj_id, typ_id, guid, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, NULL AS space_rel_id FROM {}.res
) TO STDOUT WITH (FORMAT BINARY);
)sql";
        return std::string(fmt::format(sql, m_schema));
    } else if (table == "obj") {
        std::string sql = R"sql(
COPY (
    SELECT * FROM temp_obj_result
) TO STDOUT WITH (FORMAT BINARY);
)sql";
        return sql;
    } else if (table == "xpa") {
        std::string sql = R"sql(
COPY (
    SELECT * FROM temp_xpa_result
) TO STDOUT WITH (FORMAT BINARY);
)sql";
        return sql;
    } else if (table == "rel") {
        std::string sql = R"sql(
COPY (
    SELECT temp.id, temp.obj_id, temp.dst_id, nxpa.id, temp.cnt FROM (
        SELECT rel.id, rel.obj_id, rel.dst_id, xpa.th, rel.cnt
        FROM {0}.rel as rel
        JOIN {0}.xpa as xpa ON xpa.id = rel.xpa_id
    ) AS temp
    JOIN temp_xpa_result as nxpa ON nxpa.th = temp.th
) TO STDOUT WITH (FORMAT BINARY);
)sql";
        return std::string(fmt::format(sql, m_schema));
    } else if (table == "ary") {
        std::string sql = R"sql(
COPY (
    SELECT t.id, t.obj_id, nxpa.id AS xpa_id, t.path, t.type, t.rank1, t.dim1, t.dim2, t.dim3, t.dim4, t.usize, t.zcomp, t.value, t.chunk_length
    FROM temp_ary_xpa_result as t
    JOIN temp_xpa_result as nxpa ON nxpa.th = t.xpa_th
) TO STDOUT WITH (FORMAT BINARY);
)sql";
        return sql;
    } else if (table == "bin") {
        std::string sql = R"sql(
COPY (
    SELECT * FROM temp_bin_result
) TO STDOUT WITH (FORMAT BINARY);
)sql";
        return sql;
    }

    throw std::runtime_error("invalid table name");
}

std::string PGStreamer::Impl_::get_admin_space_xdata() {
    assert(*current_table_it_ == "admin");

    return m_space.xdata_json_;
}

bool PGStreamer::Impl_::start_table_dump() {
    const std::string cmd = prepare_cmd(*current_table_it_);
    PGresult* res = PQexec(pg_conn_, cmd.c_str());
    bool ok = true;
    if (PQresultStatus(res) != PGRES_COPY_OUT) {
        OES_ERROR("Failed to initialize copy out table {}", *current_table_it_);
        ok = false;
    }
    PQclear(res);
    return ok;
}

bool PGStreamer::Impl_::end_table_dump() {
    PGresult* res = nullptr;
    while ((res = PQgetResult(pg_conn_)) != nullptr) {
        PQclear(res);
    }

    return true;
}

SpaceStreamStatus PGStreamer::Impl_::stream(TableData& td) {

    // Must be always start OR ongoing
    assert(status_ != SpaceTableStreamStatus::eFailed);
    assert(status_ != SpaceTableStreamStatus::eDone);
    assert(current_table_it_ != tables_.cend());

    bool is_admin_table = *current_table_it_ == "admin";

    if (status_ == SpaceTableStreamStatus::eStart) {
        if (is_admin_table && len_ == 0) {
            auto xdata = get_admin_space_xdata();

            if (!xdata.empty()) {
                td.m_tableData.assign(
                    reinterpret_cast<uint8_t*>(xdata.data()),
                    reinterpret_cast<uint8_t*>(xdata.data()) + xdata.size()
                );

                len_ = -1; // done
            } else {
                current_table_it_++;
                is_admin_table = false;
            }
        }

        if (!is_admin_table) {
            start_table_dump();
        }
    }
    td.m_tableName = *current_table_it_;

    if (buffer_ == nullptr && len_ == 0) { // new batch
        len_ = PQgetCopyData(pg_conn_, &buffer_, 0);
    }
    
    if (len_ > 0) {
        size_t len_to_send;
        if (static_cast<size_t>(len_) > max_msg_size_) {
            len_to_send = max_msg_size_;
        } else {
            len_to_send = static_cast<size_t>(len_);
        }
        len_ -= static_cast<int>(len_to_send);
        
        td.m_tableData.assign(
            reinterpret_cast<uint8_t*>(buffer_) + offset_,
            reinterpret_cast<uint8_t*>(buffer_) + offset_ + len_to_send
        );
        auto temp_status = static_cast<uint8_t>(status_); // start or ongoing
        
        if (len_ > 0) {
            offset_ += len_to_send;
            // insertChunkFlag(temp_status);
        } else {
            len_ = 0;
            offset_ = 0;

            PQfreemem(buffer_);
            buffer_ = nullptr;

            insertFinalChunkFlag(temp_status);
        }
        
        td.m_tableStatus = temp_status;
        
        // update status
        status_ = SpaceTableStreamStatus::eOngoing;
        return SpaceStreamStatus::eOngoing;
    } else if (len_ == -1) {
        if (!is_admin_table && !end_table_dump()) {
            status_ = SpaceTableStreamStatus::eFailed;
        } else {
            status_ = SpaceTableStreamStatus::eDone;
            len_ = 0;
        }
    } else { // -2
        end_table_dump();
        status_ = SpaceTableStreamStatus::eFailed;
    }

    td.m_tableStatus = static_cast<uint8_t>(status_);
    if (status_ == SpaceTableStreamStatus::eFailed) {
        return SpaceStreamStatus::eFailed;
    }
    
    current_table_it_++;
    status_ = SpaceTableStreamStatus::eStart;
    
    if (current_table_it_ == tables_.cend()) {
        clean();
        return SpaceStreamStatus::eDone;
    }

    return SpaceStreamStatus::eOngoing;
}

// ========================================================================================================

// PGStreamer definition
PGStreamer::PGStreamer(const Space& space, Connection_ptr conn, const bool has_ref, const size_t max_msg_size)
    : PGStream(space, std::move(conn)),
      impl_{std::make_unique<Impl_>(space, m_conn_, has_ref, max_msg_size)} {}

PGStreamer::~PGStreamer() {}

SpaceStreamStatus PGStreamer::stream(TableData& td) {
    return impl_->stream(td);
}

// ========================================================================================================

// PGConsumer::Impl_ class
class PGConsumer::Impl_ {
public:
    const Space& m_space;
    const std::string& schema;

    Impl_(const Space& space, Connection_ptr& conn);

    SpaceStreamStatus consume(TableData& td);

    std::string current_table;
private:
    PGconn* const pg_conn_;

    std::vector<uint8_t> buffer_;

    bool clear_pg_conn();
};

PGConsumer::Impl_::Impl_(const Space& space, Connection_ptr& conn)
    : m_space(space),
      schema(space.dbfile_),
      pg_conn_(conn->pq_connection()) {}

SpaceStreamStatus PGConsumer::Impl_::consume(TableData& td) {
    auto ret = SpaceStreamStatus::eOngoing;
    const auto res_table_raw_status = static_cast<uint8_t>(td.m_tableStatus);

    if (hasTableStreamStatus(res_table_raw_status, SpaceTableStreamStatus::eStart)) {
        current_table = td.m_tableName;
        const std::string cmd = fmt::format("COPY {}.{} FROM STDIN WITH (FORMAT BINARY)", schema, current_table);
        
        PGresult* result = PQexec(pg_conn_, cmd.c_str());
        if (PQresultStatus(result) != PGRES_COPY_IN) {
            OES_ERROR("Copy in table {} failed: {}", current_table, PQerrorMessage(pg_conn_));
            ret = SpaceStreamStatus::eFailed;
        }
        PQclear(result);

        if (ret == SpaceStreamStatus::eFailed) {
            return ret;
        }
    } 

    if (hasTableStreamStatus(res_table_raw_status, SpaceTableStreamStatus::eStart) ||
        hasTableStreamStatus(res_table_raw_status, SpaceTableStreamStatus::eOngoing)) {
        assert(td.m_tableData.size() > 0);

        buffer_.insert(
            buffer_.end(),
            std::make_move_iterator(td.m_tableData.begin()),
            std::make_move_iterator(td.m_tableData.end())
        );

        if (hasTableStreamStatus(res_table_raw_status, SpaceTableStreamStatus::eFinalChunk)) {
            auto result = PQputCopyData(
                pg_conn_,
                reinterpret_cast<const char*>(buffer_.data()),
                static_cast<int>(buffer_.size())
            );
    
            buffer_.clear();

            if (result == 1) {
                return SpaceStreamStatus::eOngoing;
            } else {
                OES_ERROR("Can not initialize copy table for table {}", td.m_tableName);
                return SpaceStreamStatus::eFailed;
            }
        }

    } else if (hasTableStreamStatus(res_table_raw_status, SpaceTableStreamStatus::eDone)) {
        if (td.m_tableName == "admin") {
            std::string json(td.m_tableData.begin(), td.m_tableData.end());
            
            const char* params[2] = {
                json.c_str(),
                m_space.uid_.c_str()
            };
            
            PGresult* res = PQexecParams(
                pg_conn_,
                "UPDATE admin.spaces SET xdata_json = $1::jsonb WHERE uid = $2",
                2, nullptr, params, nullptr, nullptr, 0
            );

            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                OES_ERROR("Could not insert xdata to space {}", m_space.uid_);
                ret = SpaceStreamStatus::eFailed;
            }

            PQclear(res);

            if (!clear_pg_conn()) {
                OES_ERROR("Clear admin copy error: {}", PQerrorMessage(pg_conn_));
                ret = SpaceStreamStatus::eFailed;
            }

            return ret;
        }

        if (current_table.empty()) {
            OES_ERROR("Can not finalize table copy since it is not active");
            return SpaceStreamStatus::eFailed;
        }

        current_table.clear();
        auto result = PQputCopyEnd(pg_conn_, nullptr);
        if (result != 1) {
            ret = SpaceStreamStatus::eFailed;
            OES_ERROR("Can not end copy of table {}", current_table);
        }

        if (!clear_pg_conn()) {
            OES_ERROR("Copy end: {}", PQerrorMessage(pg_conn_));
            ret = SpaceStreamStatus::eFailed;
        }
    }

    return ret;
}

bool PGConsumer::Impl_::clear_pg_conn() {
    PGresult* result_p;
    while ((result_p = PQgetResult(pg_conn_)) != nullptr) {
        if (PQresultStatus(result_p) != PGRES_COMMAND_OK) {
            return false;
        }
        PQclear(result_p);
    }
    return true;
}

// ========================================================================================================

// PGConsumer definitions
PGConsumer::PGConsumer(const Space& space, Connection_ptr conn)
    : PGStream(space, std::move(conn)),
      impl_{std::make_unique<PGConsumer::Impl_>(space, m_conn_)} {}

PGConsumer::~PGConsumer() {}

SpaceStreamStatus PGConsumer::consume(TableData& td) {
    if (td.m_copyStatus == static_cast<uint8_t>(SpaceStreamStatus::eFailed)) {
        OES_ERROR("Space copy failed from remote server");
        return SpaceStreamStatus::eFailed;
    }

    auto cp_status = static_cast<SpaceStreamStatus>(td.m_copyStatus);
    auto tb_status = impl_->consume(td);

    if (cp_status == SpaceStreamStatus::eDone && tb_status != SpaceStreamStatus::eFailed) {
        return cp_status;
    }
    return tb_status;
}

} // namespace oes::eml::openkv