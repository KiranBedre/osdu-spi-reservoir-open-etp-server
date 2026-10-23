#include "PGStream.h"
#include "SpaceDDL.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <libpq-fe.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>
#include <oes/postgresql/Connection.h>
#include <oes/postgresql/PhysicalName.h>
#include <oes/common/etp12/Messages.h>

#include <array>
#include <cassert>
#include <cstring>
#include <iterator>
#include <map>
#include <set>


namespace oes::eml::openkv {

namespace {

// Migrations append columns, so both sides name them, in the same (sorted) order;
// sources before the version exchange copy whole tables, i.e. in physical order
std::string column_list(
    PGconn* conn, const std::string& schema, const std::string& table, bool physical_order = false,
    const std::vector<std::string>& excluded = {}
) {
    const std::string qualified = schema + "." + table;
    std::string excluded_array = "{";
    for (const auto& column : excluded) {
        excluded_array += (excluded_array.size() > 1 ? "," : "") + column;
    }
    excluded_array += "}";
    const char* params[2] = { qualified.c_str(), excluded_array.c_str() };
    const std::string query = fmt::format(
        "SELECT string_agg(quote_ident(attname), ', ' ORDER BY {}) FROM pg_attribute"
        " WHERE attrelid = $1::regclass AND attnum > 0 AND NOT attisdropped AND attname <> ALL($2::text[])",
        physical_order ? "attnum" : "attname"
    );
    PGresult* res = PQexecParams(
        conn, query.c_str(),
        2, nullptr, params, nullptr, nullptr, 0
    );
    std::string columns;
    if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1 && !PQgetisnull(res, 0, 0)) {
        columns = PQgetvalue(res, 0, 0);
    } else {
        OES_ERROR("Cannot list the columns of {}: {}", qualified, PQerrorMessage(conn));
    }
    PQclear(res);
    return columns;
}

// Tables a stream may carry, besides the "schema" and "admin" pseudo-tables
bool is_streamed_table(const std::string& table) {
    static const std::set<std::string> tables = {
        "uri", "xpa", "typ", "del_res", "res", "obj", "rel", "ary", "bin"
    };
    return tables.count(table) > 0;
}

// Field count of the first tuple of a binary COPY: -1 for an empty table, -2 if not a binary COPY
int copy_field_count(const std::vector<uint8_t>& data) {
    static const char signature[] = "PGCOPY\n\377\r\n"; // 11 bytes with its NUL
    if (data.size() < 19 || std::memcmp(data.data(), signature, 11) != 0) {
        return -2;
    }
    const size_t extension = (size_t(data[15]) << 24) | (size_t(data[16]) << 16) | (size_t(data[17]) << 8) | size_t(data[18]);
    const size_t pos = 19 + extension;
    if (data.size() < pos + 2) {
        return -2;
    }
    return int16_t((data[pos] << 8) | data[pos + 1]);
}

std::string version_number(const std::string& schema_version) {
    return schema_version.substr(schema_version.find(':') + 1);
}

} // namespace

std::string spaceSchemaVersion(const Connection_ptr& conn, const std::string& dbfile) {
    const oes::postgresql::relational::PhysicalName phys(dbfile, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    return conn->executeString(
        fmt::format("SELECT obj_description('{}'::regnamespace, 'pg_namespace')", phys.sqlText())
    );
}

int schemaVersionNumber(const std::string& schema_version) {
    int magic_cookie = 0;
    space_schema_versions(magic_cookie);
    const std::string prefix = std::to_string(magic_cookie) + ':';
    if (schema_version.compare(0, prefix.size(), prefix) != 0) {
        return -1;
    }
    try {
        size_t used = 0;
        const int version = std::stoi(schema_version.substr(prefix.size()), &used);
        return prefix.size() + used == schema_version.size() ? version : -1;
    } catch (const std::exception&) {
        return -1;
    }
}

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

    Impl_(const Space& space, Connection_ptr& conn, const bool has_ref, const size_t max_msg_size, std::string schema_version, int project_to);
    ~Impl_();

    SpaceStreamStatus stream(TableData& td);
    
private:
    
    bool define_functions();
    bool call_requirements();
    bool clean();

    std::string prepare_cmd(const std::string& table);
    std::vector<std::string> excluded_columns(const std::string& table) const {
        return project_to_ > 0 ? columns_added_after(table, project_to_) : std::vector<std::string>();
    }
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
    std::string schema_data_; // the "schema" pseudo-table, until sent
    bool physical_order_ = false;
    const int project_to_;

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

PGStreamer::Impl_::Impl_(const Space& space, Connection_ptr& conn, const bool has_ref, const size_t max_msg_size, std::string schema_version, int project_to)
    : m_space(space),
      m_schema(m_space.dbfile_),
      pg_conn_(conn->pq_connection()),
      current_table_it_(PGStreamer::Impl_::tables_.cbegin()),
      tx_(conn->begin_transaction()),
      max_msg_size_(static_cast<size_t>(max_msg_size*0.9)),
      has_ref(has_ref),
      project_to_(project_to) {
    if (has_ref) {
        if (!define_functions()) {
            throw std::runtime_error("Can not prepare functions");
        }
    
        if (!call_requirements()) {
            throw std::runtime_error("Can not generate temporary tables");
        }
    }

    if (!schema_version.empty()) {
        schema_data_ = schema_version + '\n';
        for (const auto& table : tables_) {
            if (table == "admin") {
                continue;
            }
            const std::string columns = column_list(pg_conn_, m_schema, table, false, excluded_columns(table));
            if (columns.empty()) {
                throw std::runtime_error("cannot list the columns of table " + table);
            }
            schema_data_ += table + ':' + columns + '\n';
        }
    } else {
        physical_order_ = true;
        ++current_table_it_; // skips "admin": targets before the exchange overwrite their xdata with it instead of merging
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
        res_type text,
        dbfile text
    ) ON COMMIT DROP;

    EXECUTE FORMAT('
        INSERT INTO temp_parent_ref_guid_dbfile (dst_obj_id, res_guid, res_type, dbfile)
        SELECT dst_res.obj_id, dst_res.guid, dst_uri.ml || ''.'' || dst_typ.xml, s.dbfile
        FROM %I.res AS dst_res
        INNER JOIN %I.typ AS dst_typ ON dst_typ.id = dst_res.typ_id
        INNER JOIN %I.uri AS dst_uri ON dst_uri.id = dst_typ.uri_id
        INNER JOIN admin.space_rel AS sr ON dst_res.space_rel_id = sr.id
        INNER JOIN admin.spaces AS s ON sr.target_sp = s.path;',
        child_schema, child_schema, child_schema
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
            JOIN %I.typ AS orig_typ ON orig_typ.id = orig_res.typ_id
            JOIN %I.uri AS orig_uri ON orig_uri.id = orig_typ.uri_id
			WHERE parent.res_guid = orig_res.guid
              AND parent.res_type = orig_uri.ml || ''.'' || orig_typ.xml
              AND parent.dbfile = %L;',
            rec.dbfile,
            rec.dbfile,
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
        epc_guid uuid,
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
        INSERT INTO temp_ary_xpa_result(obj_id, xpa_th, epc_guid, path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length, orig_ary_id, orig_ary_schema)
        SELECT a.obj_id, x.th, a.epc_guid, a.path, a.type, a.rank1, a.dim1, a.dim2, a.dim3, a.dim4, a.usize, a.zcomp, a.value, a.chunk_length, null, null
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
            'INSERT INTO temp_ary_xpa_result(obj_id, xpa_th, epc_guid, path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length, orig_ary_id, orig_ary_schema)
            SELECT parent.dst_obj_id, x.th, oa.epc_guid, oa.path, oa.type, oa.rank1, oa.dim1, oa.dim2, oa.dim3, oa.dim4, oa.usize, oa.zcomp, oa.value, oa.chunk_length, oa.id, parent.dbfile
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
    const std::string columns = column_list(pg_conn_, m_schema, table, physical_order_, excluded_columns(table));
    if (columns.empty()) {
        throw std::runtime_error("cannot list the columns of table " + table);
    }

    if (!has_ref ||
        table == "uri" ||
        table == "typ" ||
        table == "del_res"
    ) {
        return fmt::format("COPY {}.{} ({}) TO STDOUT WITH (FORMAT BINARY)", m_schema, table, columns);
    }

    // Each source names its output columns after the table's
    std::string source;
    if (table == "res") {
        source = fmt::format(
            "SELECT obj_id, typ_id, guid, name, version, createdAt, updatedAt, createdBy, updatedBy,"
            " xdata_json, xdata_avro, storeCreated, storeUpdated, NULL AS space_rel_id FROM {}.res",
            m_schema
        );
    } else if (table == "obj") {
        source = "SELECT * FROM temp_obj_result";
    } else if (table == "xpa") {
        source = "SELECT * FROM temp_xpa_result";
    } else if (table == "rel") {
        source = fmt::format(R"sql(
    SELECT temp.id, temp.obj_id, temp.dst_id, nxpa.id AS xpa_id, temp.cnt FROM (
        SELECT rel.id, rel.obj_id, rel.dst_id, xpa.th, rel.cnt
        FROM {0}.rel as rel
        JOIN {0}.xpa as xpa ON xpa.id = rel.xpa_id
    ) AS temp
    JOIN temp_xpa_result as nxpa ON nxpa.th = temp.th
)sql", m_schema);
    } else if (table == "ary") {
        source = R"sql(
    SELECT t.id, t.obj_id, nxpa.id AS xpa_id, t.epc_guid, t.path, t.type, t.rank1, t.dim1, t.dim2, t.dim3, t.dim4, t.usize, t.zcomp, t.value, t.chunk_length
    FROM temp_ary_xpa_result as t
    JOIN temp_xpa_result as nxpa ON nxpa.th = t.xpa_th
)sql";
    } else if (table == "bin") {
        source = "SELECT * FROM temp_bin_result";
    } else {
        throw std::runtime_error("invalid table name");
    }

    return fmt::format("COPY (SELECT {} FROM ({}) AS s) TO STDOUT WITH (FORMAT BINARY)", columns, source);
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

    if (!schema_data_.empty()) {
        td.m_tableName = "schema";
        td.m_tableData.assign(schema_data_.begin(), schema_data_.end());
        td.m_tableStatus = static_cast<uint8_t>(SpaceTableStreamStatus::eDone);
        schema_data_.clear();
        return SpaceStreamStatus::eOngoing;
    }

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
PGStreamer::PGStreamer(const Space& space, Connection_ptr conn, const bool has_ref, const size_t max_msg_size, std::string schema_version, int project_to)
    : PGStream(space, std::move(conn)),
      impl_{std::make_unique<Impl_>(space, m_conn_, has_ref, max_msg_size, std::move(schema_version), project_to)} {}

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

    Impl_(const Space& space, Connection_ptr& conn, std::string schema_version);

    SpaceStreamStatus consume(TableData& td);
    bool finish();
    SpaceStreamStatus fail(std::string msg);

    std::string current_table;
    std::string error_;
private:
    Connection_ptr conn_;
    PGconn* const pg_conn_;

    std::vector<uint8_t> buffer_;

    const std::string schema_version_;
    bool schema_received_ = false;
    bool legacy_ = false; // the source sent no "schema" pseudo-table
    std::map<std::string, std::string> columns_; // received in the "schema" pseudo-table
    bool copy_active_ = false;
    bool first_row_pending_ = false;
    int expected_fields_ = 0;

    SpaceStreamStatus take_source_version(const std::string& version);
    SpaceStreamStatus read_schema(const TableData& td);
    bool clear_pg_conn();
};

PGConsumer::Impl_::Impl_(const Space& space, Connection_ptr& conn, std::string schema_version)
    : m_space(space),
      schema(space.dbfile_),
      conn_(conn),
      pg_conn_(conn->pq_connection()),
      schema_version_(std::move(schema_version)) {}

SpaceStreamStatus PGConsumer::Impl_::fail(std::string msg) {
    OES_ERROR("Dataspace stream into {}: {}", m_space.uid_, msg);
    if (copy_active_) {
        PQputCopyEnd(pg_conn_, msg.c_str());
        while (PGresult* res = PQgetResult(pg_conn_)) {
            PQclear(res);
        }
        copy_active_ = false;
    }
    buffer_.clear();
    error_ = std::move(msg);
    return SpaceStreamStatus::eFailed;
}

SpaceStreamStatus PGConsumer::Impl_::take_source_version(const std::string& version) {
    const int source = schemaVersionNumber(version);
    const int target = schemaVersionNumber(schema_version_);
    if (source < 0 || source > target) {
        return fail(fmt::format(
            "The source dataspace is at schema {}, the target at {}",
            version_number(version), version_number(schema_version_)
        ));
    }
    if (source < target) {
        // An older source: its rows go into its own layout, then the target's upgrade steps run on them
        try {
            downgrade_empty_space(conn_, schema, target, source);
        } catch (const std::exception& e) {
            return fail(fmt::format("Cannot take the target from schema {} down to {}: {}", target, source, e.what()));
        }
    }
    schema_received_ = true;
    return SpaceStreamStatus::eOngoing;
}

SpaceStreamStatus PGConsumer::Impl_::read_schema(const TableData& td) {
    std::istringstream lines(std::string(td.m_tableData.begin(), td.m_tableData.end()));
    std::string version;
    std::getline(lines, version);
    if (take_source_version(version) == SpaceStreamStatus::eFailed) {
        return SpaceStreamStatus::eFailed;
    }
    for (std::string line; std::getline(lines, line);) {
        const auto colon = line.find(':');
        if (colon != std::string::npos) {
            columns_[line.substr(0, colon)] = line.substr(colon + 1);
        }
    }
    return SpaceStreamStatus::eOngoing;
}

SpaceStreamStatus PGConsumer::Impl_::consume(TableData& td) {
    auto ret = SpaceStreamStatus::eOngoing;
    const auto res_table_raw_status = static_cast<uint8_t>(td.m_tableStatus);

    if (td.m_tableName == "schema") {
        return read_schema(td);
    }
    if (!schema_received_) {
        // Every server without the version exchange is a 15 binary
        legacy_ = true;
        int magic_cookie = 0;
        space_schema_versions(magic_cookie);
        if (take_source_version(fmt::format("{}:15", magic_cookie)) == SpaceStreamStatus::eFailed) {
            return SpaceStreamStatus::eFailed;
        }
    }

    if (hasTableStreamStatus(res_table_raw_status, SpaceTableStreamStatus::eStart)) {
        current_table = td.m_tableName;
        const auto received = columns_.find(current_table);
        if (!is_streamed_table(current_table) || (!legacy_ && received == columns_.end())) {
            return fail("Unexpected table in the stream: " + current_table);
        }
        const std::string columns = column_list(pg_conn_, schema, current_table, legacy_);
        if (columns.empty()) {
            return fail("Cannot list the columns of table " + current_table);
        }
        if (!legacy_ && columns != received->second) {
            return fail(fmt::format(
                "Table {}: the source sends the columns ({}), the target has ({})",
                current_table, received->second, columns
            ));
        }
        expected_fields_ = 1;
        for (size_t pos = columns.find(", "); pos != std::string::npos; pos = columns.find(", ", pos + 2)) {
            ++expected_fields_;
        }
        first_row_pending_ = true;

        const std::string cmd = fmt::format(
            "COPY {}.{} ({}) FROM STDIN WITH (FORMAT BINARY)", schema, current_table, columns
        );
        
        PGresult* result = PQexec(pg_conn_, cmd.c_str());
        const bool copy_in = PQresultStatus(result) == PGRES_COPY_IN;
        PQclear(result);
        if (!copy_in) {
            return fail(fmt::format("Copy in table {} failed: {}", current_table, PQerrorMessage(pg_conn_)));
        }
        copy_active_ = true;
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
            if (first_row_pending_) {
                first_row_pending_ = false;
                const int fields = copy_field_count(buffer_);
                if (fields == -2) {
                    return fail(fmt::format("Table {}: not a binary COPY stream", td.m_tableName));
                }
                if (fields >= 0 && fields != expected_fields_) {
                    return fail(fmt::format(
                        "Table {}: the source sends {} fields per row, the target has {} columns{}",
                        td.m_tableName, fields, expected_fields_,
                        legacy_ ? " (a source sending no schema version is taken as schema 15)" : ""
                    ));
                }
            }

            auto result = PQputCopyData(
                pg_conn_,
                reinterpret_cast<const char*>(buffer_.data()),
                static_cast<int>(buffer_.size())
            );
    
            buffer_.clear();

            if (result == 1) {
                return SpaceStreamStatus::eOngoing;
            } else {
                return fail(fmt::format("Copy into table {} failed: {}", td.m_tableName, PQerrorMessage(pg_conn_)));
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
                error_ = fmt::format("Could not insert xdata to space {}: {}", m_space.uid_, PQerrorMessage(pg_conn_));
                OES_ERROR("{}", error_);
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
            return fail("Can not finalize table copy since it is not active");
        }

        const std::string table = std::move(current_table);
        current_table.clear();
        copy_active_ = false;
        auto result = PQputCopyEnd(pg_conn_, nullptr);
        if (result != 1 || !clear_pg_conn()) {
            return fail(fmt::format("Copy into table {} failed: {}", table, PQerrorMessage(pg_conn_)));
        }
    }

    return ret;
}

bool PGConsumer::Impl_::finish() {
    const char* params[1] = { schema.c_str() };
    PGresult* res = PQexecParams(
        pg_conn_,
        R"sql(
SELECT format('SELECT setval(%L, COALESCE((SELECT max(%I) FROM %I.%I), 0) + 1, false)',
              seq, a.attname, n.nspname, c.relname)
  FROM pg_class AS c
  JOIN pg_namespace AS n ON n.oid = c.relnamespace
  JOIN pg_attribute AS a ON a.attrelid = c.oid AND a.attnum > 0 AND NOT a.attisdropped
 CROSS JOIN LATERAL pg_get_serial_sequence(format('%I.%I', n.nspname, c.relname), a.attname) AS seq
 WHERE n.nspname = lower($1) AND c.relkind = 'r' AND seq IS NOT NULL
)sql",
        1, nullptr, params, nullptr, nullptr, 0
    );
    std::vector<std::string> setvals;
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); ++i) {
            setvals.emplace_back(PQgetvalue(res, i, 0));
        }
    } else {
        error_ = fmt::format("Cannot list the sequences of {}: {}", schema, PQerrorMessage(pg_conn_));
    }
    PQclear(res);

    for (const auto& setval : setvals) {
        res = PQexec(pg_conn_, setval.c_str());
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            error_ = fmt::format("Cannot reset a sequence of {}: {}", schema, PQerrorMessage(pg_conn_));
        }
        PQclear(res);
    }

    if (!error_.empty()) {
        OES_ERROR("{}", error_);
        return false;
    }
    return true;
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
PGConsumer::PGConsumer(const Space& space, Connection_ptr conn, std::string schema_version)
    : PGStream(space, std::move(conn)),
      impl_{std::make_unique<PGConsumer::Impl_>(space, m_conn_, std::move(schema_version))} {}

PGConsumer::~PGConsumer() {}

bool PGConsumer::finish() {
    return impl_->finish();
}

const std::string& PGConsumer::error() const {
    return impl_->error_;
}

SpaceStreamStatus PGConsumer::consume(TableData& td) {
    if (td.m_copyStatus == static_cast<uint8_t>(SpaceStreamStatus::eFailed)) {
        return impl_->fail("The source server failed to stream the dataspace");
    }

    auto cp_status = static_cast<SpaceStreamStatus>(td.m_copyStatus);
    auto tb_status = impl_->consume(td);

    if (cp_status == SpaceStreamStatus::eDone && tb_status != SpaceStreamStatus::eFailed) {
        return cp_status;
    }
    return tb_status;
}

} // namespace oes::eml::openkv