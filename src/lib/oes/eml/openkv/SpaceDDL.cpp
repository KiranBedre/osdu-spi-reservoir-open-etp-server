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

#include <iostream>
#include <sstream>
#include <limits>
#include <stdexcept>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "SpaceDDL.h"

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/PhysicalName.h>
#include <oes/postgresql/Statement.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>


namespace oes::eml::openkv {

//============================================================================

namespace {

// The schema is pinned rather than taken from the search path, because
// admin.array_summary() runs whenever the view is queried, not when it is created.
// \param schema the bare (unquoted) schema name, also passed to array_summary() as a value.
std::string resources_view_ddl(const std::string& schema) {
    const char* sql = R"sql(
CREATE OR REPLACE VIEW "{0}".resources AS
    SELECT obj_id, uri.ml ML, typ.xml AS type,
        res.guid AS guid, name, version,
        to_timestamp(createdAt/1e6) AS createdAtUTC, createdBy,
        to_timestamp(updatedAt/1e6) AS updatedAtUTC, updatedBy,
        storeCreated, storeUpdated,
        xdata_json, length(xdata_avro),
        admin.array_summary(
            '{0}', res.obj_id, res.space_rel_id, uri.ml || '.' || typ.xml, res.guid
        ) AS arrays
    FROM "{0}".res AS res
    JOIN "{0}".typ AS typ ON res.typ_id = typ.id
    JOIN "{0}".uri AS uri ON typ.uri_id = uri.id;
)sql";
    return fmt::format(sql, schema);
}

// Removes admin.space_rel rows no res row uses any more: once per DELETE, per row when space_rel_id changes.
// admin.update_space_rels() disables res_space_rel_trigger by name. \p schema is the bare (unquoted) name.
std::string space_rel_triggers_ddl(const std::string& schema) {
    const char* sql = R"sql(
CREATE OR REPLACE FUNCTION "{0}".remove_space_rels_if_no_res() RETURNS TRIGGER AS $$
BEGIN
    EXECUTE format(
        'DELETE FROM admin.space_rel AS s
          WHERE s.id IN (SELECT space_rel_id FROM old_rows WHERE space_rel_id IS NOT NULL)
            AND NOT EXISTS (SELECT 1 FROM %I.res AS r WHERE r.space_rel_id = s.id)',
        TG_TABLE_SCHEMA);
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER res_space_rel_del_trigger
AFTER DELETE ON "{0}".res
REFERENCING OLD TABLE AS old_rows
FOR EACH STATEMENT
EXECUTE PROCEDURE "{0}".remove_space_rels_if_no_res();

CREATE TRIGGER res_space_rel_trigger
AFTER UPDATE OF space_rel_id ON "{0}".res
FOR EACH ROW
WHEN (OLD.space_rel_id IS NOT NULL AND OLD.space_rel_id IS DISTINCT FROM NEW.space_rel_id)
EXECUTE PROCEDURE "{0}".remove_space_rel_if_no_res();
)sql";
    return fmt::format(sql, schema);
}

// sel_ary_ids_ and the cascade from obj; res_null_guid_idx only holds rows nulled until their commit.
// \p schema is the bare (unquoted) name.
std::string lookup_indexes_ddl(const std::string& schema) {
    const char* sql = R"sql(
CREATE INDEX ary_obj_id_idx ON "{0}".ary (obj_id);
CREATE INDEX res_null_guid_idx ON "{0}".res (obj_id) WHERE guid IS NULL;
)sql";
    return fmt::format(sql, schema);
}

} // namespace

//============================================================================

const std::vector<SchemaVersionInfo>& space_schema_versions(int& magic_cookie) {
    static const std::vector<SchemaVersionInfo> versions__{
        //    YYYY-MM-DD
        { 1, "2020-03-27", "DDe", "First creation" },
        { 2, "2020-04-08", "DDe", "Add objects and resources convenience views" },
        { 3, "2020-04-17", "DDe", "Limit arrays to 4D (drop ary.dimN, add ary.dim4)" },
        { 4, "2020-04-22", "DDe", "resources-view updated (datetime values now int-typed)" },
        { 5, "2020-07-31", "DDe", "Relax NOT NULL on same ary columns, for orphan arrays" },
        { 6, "2020-09-23", "DDe", "Add UNIQUE constraint on bin table" },
        { 7, "2020-10-26", "DDe", "Alter bin.idx column to NOT NULL" },
        { 8, "2020-12-22", "DDe", "XDataMap now on two columns (JSON and Avro binary)" },
        { 9, "2023-10-07", "LDe", "Add deleted object table" },
        {10, "2023-10-08", "LDe", "Add space reference in res" },
        {11, "2023-10-26", "LDe", "Fix obj_id indices"},
        {12, "2023-10-31", "LDe", "Remove delete_res trigger"},
        {13, "2023-11-19", "RCo", "Add triggers for ref imports if missing."},
        {14, "2023-12-14", "LDe", "Add MLs schema version"},
        {15, "2024-10-27", "RCo", "Add readable relations view."},
        {16, "2026-09-18", "RCo", "Key arrays on their container (ary.epc_guid) and path."},
        {17, "2026-10-12", "RCo", "Repair array containers of EML 2.3 objects left nil by version 16."},
        {18, "2026-10-22", "RCo", "Clean up admin.space_rel once per DELETE on res; index bin.id, ary.obj_id and nulled res rows."},
    };

    // 4c 74 4b 56 = "LtKV" (OpenKV w/o voyels)
    magic_cookie = 0x4c744b56;
    return versions__;
}

//============================================================================

void new_eml_db_prolog(const oes::postgresql::Connection_ptr& db, bool bulk_import) {
    int alt_magic_cookie = 0;
    const auto& alt_schema_history = space_schema_versions(alt_magic_cookie);
    std::ostringstream oss;
    oss << "SELECT set_config('application_name','" << alt_magic_cookie << "', false);";
    const std::string app_pragmas = oss.str();
    db->execute(app_pragmas.c_str());
    OES_UNUSED(bulk_import);

    // Main Tables: res, obj, rel, ary, bin
    //
    // res: Resource table, used by Discovery and Store protocol.
    // obj: DataObject table, used by Store protocol.
    // rel: Relation(ship) table, used by Discovery protocol.
    // ary: DataArray table, used by DataArray protocol.
    // bin: Binary-Data table, used by DataArray protocol.
    //
    // Other tables: typ, xpa
    // typ: Type table, to avoid storing XML type names all the time.
    // xpa: XPath table, annotating rel and ary table with which XML
    //      element contained the reference to a res or ary.
    //
    // res store's meta-data about entities, but not the XML,
    // which is stored in obj instead. This allows to keep the
    // res table much smaller, speeding up full-scans
    //
    // Similarly, the ary table will store data-arrays' meta-data,
    // and only small values, while larger values will be stored in
    // the bin table. Very large arrays may be split in several chunks.
    //
    // obj and res will be INTEGER PRIMARY KEY based, using the same ID
    // in both tables for the same entity (1 row in each, per entity).
    // Other tables (except typ) will reuse these same IDs, but with more
    // columns as part of the primary key.
    //
    // In both obj and bin table, columns will maintain compression info
    // even though this won't be used initially.
    //
    // SQLite values NULL, 0, and 1 are special, is that they can be stored
    // in a single byte (as part of the record header), while other integers
    // require 2 or more bytes. These values will thus be "abused", to for
    // example map to 1, 2, 3 (respectively).
    //
    // Date-times will be stored in integer UTC seconds, not strings.
    // (floating-point, as necessary, for subsecond accuracy?)
    //
    // Views will be provided, to provide more human-friendly representation
    // of the data, eventually.

    // TODO: Move all UNIQUE indexes out of table definitions,
    //       to create them post INSERTs.
    std::string schema;
    {
        oes::postgresql::Statement stmt = db->prepare("SELECT current_schema()");
        const bool has_row = stmt.step();
        OES_ASSERT(has_row, ;);
        schema = stmt.getString(0);
        stmt.reset();
    }
    const std::string resources_ddl = resources_view_ddl(schema);
    const std::string space_rel_ddl = space_rel_triggers_ddl(schema);

    const std::vector<const char*> ddls{
        // TRANSACTION is removed since we can only have one transaction per connection in PostgreSQL
        // and it may interfere with outside initiated transaction.
        R"sql(
CREATE TABLE hdr
( version INTEGER NOT NULL PRIMARY KEY
, created TEXT NOT NULL
, creator TEXT NOT NULL
, description TEXT NOT NULL
, CHECK(pg_typeof(version) = 'integer'::regtype AND version > 0)
, CHECK(pg_typeof(created) = 'text'::regtype AND length(created) = 10 AND
        date(created) IS NOT NULL)
, CHECK(pg_typeof(creator) = 'text'::regtype AND length(creator) = 3)
, CHECK(pg_typeof(description) = 'text'::regtype AND length(description) > 0)
);
        )sql",
        R"sql(
CREATE TABLE uri
( id BIGSERIAL PRIMARY KEY
, ns TEXT NOT NULL
, pp TEXT UNIQUE
, ml TEXT UNIQUE
, vs TEXT NOT NULL DEFAULT '2.0'
, UNIQUE(ns, vs)
);
        )sql",
        R"sql(
CREATE TABLE typ
( id SERIAL PRIMARY KEY
, uri_id BIGINT NOT NULL REFERENCES uri(id)
, xml TEXT -- XML element (local) name
, UNIQUE(uri_id, xml)
);
        )sql",
        R"sql(
CREATE TABLE xpa
( id BIGSERIAL PRIMARY KEY
, th TEXT UNIQUE
);
        )sql",
        R"sql(
CREATE TABLE res
( obj_id BIGSERIAL PRIMARY KEY
, typ_id INT                NOT NULL REFERENCES typ(id)
, guid UUID NULL
, name TEXT
, version TEXT -- Omit? Untyped?
, createdAt BIGINT
, updatedAt BIGINT
, createdBy TEXT
, updatedBy TEXT
, xdata_json JSON
, xdata_avro BYTEA
, UNIQUE(typ_id, guid)
, storeCreated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
, storeUpdated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
, space_rel_id BIGINT REFERENCES admin.space_rel(id) NULL
);
-- Table of deleted resources
CREATE TABLE del_res
( guid   UUID NOT NULL
, typ_id INT  NOT NULL REFERENCES typ(id)
, name TEXT
, version TEXT -- Omit? Untyped?
, createdAt BIGINT
, updatedAt BIGINT
, createdBy TEXT
, updatedBy TEXT
, xdata_json JSON
, xdata_avro BYTEA
, UNIQUE(guid, typ_id)
, storeCreated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
, storeUpdated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
, storeDeleted TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
);

-- Add trigger to update storeUpdated column
CREATE OR REPLACE FUNCTION update_storeUpdated_column()
    RETURNS TRIGGER AS $$
    BEGIN
        NEW.storeUpdated = NOW();
        RETURN NEW;
    END;
    $$ LANGUAGE 'plpgsql';
CREATE TRIGGER update_res_storeUpdated BEFORE UPDATE
    ON res FOR EACH ROW EXECUTE PROCEDURE 
    update_storeUpdated_column();

-- remove row from space_rel table if no res references it
CREATE OR REPLACE FUNCTION remove_space_rel_if_no_res() RETURNS TRIGGER AS $$
BEGIN
    EXECUTE format('DELETE FROM admin.space_rel WHERE id = $1 AND NOT EXISTS (SELECT 1 FROM %I.res WHERE space_rel_id = $2 LIMIT 1)', TG_TABLE_SCHEMA)
	  USING OLD.space_rel_id, OLD.space_rel_id;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;
        )sql",
        space_rel_ddl.c_str(),
        R"sql(
CREATE TABLE rel
( id SERIAL
, obj_id BIGINT NOT NULL REFERENCES res(obj_id) ON DELETE CASCADE
, dst_id BIGINT NOT NULL REFERENCES res(obj_id)
, xpa_id BIGINT NOT NULL REFERENCES xpa(id)
, cnt INT DEFAULT 1
, UNIQUE(obj_id, dst_id, xpa_id)
);
        )sql",
        R"sql(
-- table of xml objects
CREATE TABLE obj
( id BIGSERIAL PRIMARY KEY NOT NULL REFERENCES res(obj_id) ON DELETE CASCADE
, usize INT -- uncompressed size (if compressed or chunked, null otherwise)
, zcomp INT -- compressor (not compressed if null, 0 = ZLib)
, xml XML NOT NULL
);
        )sql",
        R"sql(
CREATE TABLE ary
( id BIGSERIAL PRIMARY KEY
, obj_id BIGINT REFERENCES obj(id) ON DELETE CASCADE
, xpa_id BIGINT REFERENCES xpa(id)
, epc_guid UUID NOT NULL
, path  TEXT NOT NULL
, type INT NOT NULL
, rank1 INT -- null means 1
, dim1 BIGINT NOT NULL
, dim2 BIGINT
, dim3 BIGINT
, dim4 BIGINT
, usize BIGINT
, zcomp BIGINT
, value BYTEA -- if small enough. Otherwise chunk-size
, chunk_length INT
, CHECK((chunk_length = NULL) != (value = NULL))
-- Two EPC external parts may hold the same path, so the container is part of the key
, UNIQUE(epc_guid, path)
)
        )sql",
        R"sql(
CREATE TABLE bin
( id BIGSERIAL PRIMARY KEY
, ary_id BIGINT NOT NULL REFERENCES ary(id) ON DELETE CASCADE
, idx INT NOT NULL CHECK (idx >= 0)
, value BYTEA
, UNIQUE(ary_id, idx)
);
        )sql",
        resources_ddl.c_str(),
        R"sql(
CREATE VIEW obj_IjkGridRepresentation AS
SELECT id
  ,admin.eml_xpath(xml,'//eml:Title/text()') AS title
  ,admin.eml_xpath(xml,'//@uuid') AS uuid 
  ,admin.eml_xpath(xml,'//eml:Originator/text()') AS originator
  ,admin.eml_xpath(xml,'//eml:Creation/text()')::timestamptz AS creation
  ,split_part(admin.eml_xpath(xml,'//@xsi:type'),':',2) AS type
  ,admin.eml_xpath(xml,'//eml:Editor/text()') AS editor
  ,admin.eml_xpath(xml,'//resqml2:Ni/text()')::int AS ni 
  ,admin.eml_xpath(xml,'//resqml2:Ni/text()')::int AS nj 
  ,admin.eml_xpath(xml,'//resqml2:Nk/text()')::int AS nk
  ,admin.eml_xpath(xml,'//resqml2:RepresentedInterpretation/eml:UUID/text()') AS interpretation
  ,xml
  FROM obj
 WHERE split_part(admin.eml_xpath(xml,'//@xsi:type'),':',2) LIKE 'obj_IjkGridRepresentation';
        )sql",
        R"sql(

CREATE VIEW objects AS
SELECT r.*, obj.xml
  FROM resources r
  JOIN obj ON r.obj_id = obj.id

        )sql",
        R"sql(

CREATE VIEW relations AS
SELECT
	srctyp.xml AS src_type,
	src.guid AS src_guid,
	src.name AS src_name,
	dsttyp.xml AS dst_type,
	dst.guid AS dst_guid,
	dst.name AS dst_name,
	xpa.th,
	cnt
FROM rel
JOIN res src ON src.obj_id=rel.obj_id
JOIN res dst ON dst.obj_id=rel.dst_id
JOIN xpa ON rel.xpa_id=xpa.id
JOIN typ srctyp ON src.typ_id=srctyp.id
JOIN typ dsttyp ON dst.typ_id=dsttyp.id
)sql"
    };

    for (const char* ddl : ddls) {
        try {
            db->execute(ddl);
        } catch (const oes::postgresql::PostgreSQLException& ex) {
            OES_ERROR(
                "Creating schema: {}\n"
                "\twhile executing statement:\n"
                "{}\n",
                ex.what(), ddl
            );
            /*re*/throw;
        }
    }

    // Capture schema history information. Using latest format of course.
    oes::postgresql::Statement stmt = db->prepare(
        "INSERT INTO hdr"
        "( version, created, creator, description"
        ") VALUES ($1, $2, $3, $4)"
    );

    for (const auto& v : alt_schema_history) {
        stmt.bind(1, v.version);
        stmt.bind(2, v.created, 10);
        stmt.bind(3, v.creator, 3);
        stmt.bind(4, v.description, static_cast<int>(strnlen(v.description, std::numeric_limits<int>::max())));
        stmt.execute();
        stmt.reset();
    }
}

//============================================================================

void new_eml_db_epilog(const oes::postgresql::Connection_ptr& db, bool vacuum, bool analyze) {
    OES_UNUSED(vacuum);

    // After vacuum, but before analyze.
    // See https://sqlite.org/forum/forumpost/62fb63a29c
    // That (non-UNIQUE) index is necessary for efficiently finding "eSources"
    // in the Discovery.GetResources protocol. Finding eTargets uses the PK.
    // TODO: Move all UNIQUE indexes here too. See DRH post link above too.
    db->execute("CREATE INDEX rel_by_dst ON rel(dst_id)");
    db->execute(lookup_indexes_ddl(db->executeString("SELECT current_schema()")));
    if (analyze) {
        db->execute("ANALYZE");
    }
}

//============================================================================

// Function to create del_res table and associated trigger.
// This is used to keep track of deleted resources.
// The trigger is used to populate the del_res table.
// The del_res table is used to implement Discovery.GetDeletedResources.
void add_deleted_resources_table(const Connection_ptr& conn, const std::string &schema) {
    const char* sql = R"sql(
        CREATE TABLE IF NOT EXISTS {}.del_res
        ( guid   UUID NOT NULL
        , typ_id INT  NOT NULL REFERENCES {}.typ(id)
        , name TEXT
        , version TEXT -- Omit? Untyped?
        , createdAt BIGINT
        , updatedAt BIGINT
        , createdBy TEXT
        , updatedBy TEXT
        , xdata_json JSON
        , xdata_avro BYTEA
        , UNIQUE(guid, typ_id)
        , storeCreated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
        , storeUpdated TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
        , storeDeleted TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW()
        );
    )sql";
    conn->execute(fmt::format(sql, schema, schema));
}

// Remove the del_res table and associated trigger.
void remove_deleted_resources_table(const Connection_ptr& conn, const std::string &schema) {
    const char* sql = R"sql(
        DROP FUNCTION IF EXISTS {}.insert_deleted_row_to_del_res();
        DROP TABLE IF EXISTS {}.del_res;
    )sql";
    conn->execute(fmt::format(sql, schema, schema));
}

void add_import_by_ref_triggers(const Connection_ptr conn, const std::string &schema) {
    const std::string sql = R"sql(
        -- Add trigger to update storeUpdated column
        CREATE OR REPLACE FUNCTION {}.update_storeUpdated_column()
            RETURNS TRIGGER AS $$
            BEGIN
                NEW.storeUpdated = NOW();
                RETURN NEW;
            END;
            $$ LANGUAGE 'plpgsql';
        CREATE OR REPLACE TRIGGER update_res_storeUpdated BEFORE UPDATE
            ON {}.res FOR EACH ROW EXECUTE PROCEDURE 
            update_storeUpdated_column();

        -- remove row from space_rel table if no res references it
        CREATE OR REPLACE FUNCTION {}.remove_space_rel_if_no_res() RETURNS TRIGGER AS $$
        BEGIN
            EXECUTE format('DELETE FROM admin.space_rel WHERE id = $1 AND NOT EXISTS (SELECT 1 FROM %I.res WHERE space_rel_id = $2 LIMIT 1)', TG_TABLE_SCHEMA)
            USING OLD.space_rel_id, OLD.space_rel_id;
            RETURN OLD;
        END;
        $$ LANGUAGE plpgsql;

        -- add a trigger that invoke remove_space_rel_if_no_res when a row of res is edited or deleted
        CREATE OR REPLACE TRIGGER res_space_rel_trigger
        AFTER DELETE OR UPDATE OF space_rel_id ON {}.res
        FOR EACH ROW
        EXECUTE PROCEDURE remove_space_rel_if_no_res();
    )sql";
    conn->execute(fmt::format(sql, schema, schema, schema, schema));
}

// Create the relations view.
void add_relations_view(const Connection_ptr& conn, const std::string &schema) {
    const char* sql = R"sql(
        CREATE OR REPLACE VIEW {}.relations AS
        SELECT
            srctyp.xml AS src_type,
            src.guid AS src_guid,
            src.name AS src_name,
            dsttyp.xml AS dst_type,
            dst.guid AS dst_guid,
            dst.name AS dst_name,
            xpa.th,
            cnt
        FROM rel
        JOIN res src ON src.obj_id=rel.obj_id
        JOIN res dst ON dst.obj_id=rel.dst_id
        JOIN xpa ON rel.xpa_id=xpa.id
        JOIN typ srctyp ON src.typ_id=srctyp.id
        JOIN typ dsttyp ON dst.typ_id=dsttyp.id
    )sql";
    conn->execute(fmt::format(sql, schema));
}

// Replace the resources view with the one using admin.array_summary().
// Unlike the other helpers here, \p schema is the bare (unquoted) name.
void update_resources_view(const Connection_ptr& conn, const std::string &schema) {
    conn->execute(resources_view_ddl(schema));
}

// Version 18: the row-level trigger of 13 cleaned up admin.space_rel once per deleted res row.
// \p schema is the bare (unquoted) name.
void update_space_rel_triggers(const Connection_ptr& conn, const std::string &schema) {
    conn->execute(
        fmt::format("DROP TRIGGER IF EXISTS res_space_rel_trigger ON \"{}\".res;", schema)
        + space_rel_triggers_ddl(schema)
    );
}

// Version 18: bin.id from its sequence is already unique and non-null. \p schema is the bare (unquoted) name.
void add_lookup_indexes(const Connection_ptr& conn, const std::string &schema) {
    conn->execute(
        fmt::format("ALTER TABLE \"{}\".bin ADD PRIMARY KEY (id);", schema)
        + lookup_indexes_ddl(schema)
    );
}

//============================================================================

// Each step undoes its up-step exactly, so the following upgrade recreates the same objects
void downgrade_empty_space(const Connection_ptr& conn, const std::string& dbfile, int from_version, int to_version) {
    int magic_cookie = 0;
    const int latest = space_schema_versions(magic_cookie).back().version;
    if (to_version > from_version || from_version > latest) {
        throw std::runtime_error(fmt::format(
            "Cannot take a space from schema {} down to {}", from_version, to_version
        ));
    }
    const oes::postgresql::relational::PhysicalName phys(dbfile, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    const std::string schema = phys.quotedName();
    if (conn->executeInt64(fmt::format("SELECT count(*) FROM (SELECT 1 FROM {}.res LIMIT 1) AS r", schema)) > 0) {
        throw std::runtime_error("Cannot take a space holding resources to an older schema");
    }

    // 18 -> 17: the row-level trigger, for DELETE as well, and no lookup indexes
    if (from_version >= 18 && to_version < 18) {
        conn->execute(fmt::format(R"sql(
DROP TRIGGER res_space_rel_del_trigger ON {0}.res;
DROP FUNCTION {0}.remove_space_rels_if_no_res();
DROP TRIGGER res_space_rel_trigger ON {0}.res;
CREATE TRIGGER res_space_rel_trigger
AFTER DELETE OR UPDATE OF space_rel_id ON {0}.res
FOR EACH ROW
EXECUTE PROCEDURE {0}.remove_space_rel_if_no_res();
DROP INDEX {0}.res_null_guid_idx;
DROP INDEX {0}.ary_obj_id_idx;
ALTER TABLE {0}.bin DROP CONSTRAINT bin_pkey;
)sql", schema));
    }

    // 17 -> 16: data only, nothing to undo

    // 16 -> 15: the path-only key. The resources view of 16 reads ary only through admin.array_summary(),
    // and the 16 up-step replaces it anyway
    if (from_version >= 16 && to_version < 16) {
        conn->execute(fmt::format(R"sql(
-- Also drops ary_epc_guid_path_key, as any constraint involving the column
ALTER TABLE {0}.ary DROP COLUMN epc_guid;
ALTER TABLE {0}.ary ADD CONSTRAINT ary_path_key UNIQUE (path);
)sql", schema));
    }

    conn->execute(fmt::format("COMMENT ON SCHEMA {} IS '{}:{}'", schema, magic_cookie, to_version));
}

std::vector<std::string> columns_added_after(const std::string& table, int to_version) {
    std::vector<std::string> columns;
    if (to_version < 16 && table == "ary") {
        columns.emplace_back("epc_guid");
    }
    return columns;
}

void check_space_projection(const Connection_ptr& conn, const std::string& dbfile, int from_version, int to_version) {
    int magic_cookie = 0;
    const int latest = space_schema_versions(magic_cookie).back().version;
    if (to_version > from_version || from_version > latest) {
        throw std::runtime_error(fmt::format(
            "Cannot project a space from schema {} down to {}", from_version, to_version
        ));
    }
    const oes::postgresql::relational::PhysicalName phys(dbfile, oes::postgresql::relational::IMPLICIT_LOWERCASE);

    // 18 -> 17: triggers and indexes only

    // 17 -> 16: the repaired containers stay valid at 16

    // 16 -> 15: one container per path
    if (from_version >= 16 && to_version < 16) {
        const std::string path = conn->executeString(fmt::format(
            "SELECT (SELECT path FROM {}.ary GROUP BY path HAVING count(*) > 1 LIMIT 1)", phys.quotedName()
        ));
        if (!path.empty()) {
            throw std::runtime_error(fmt::format(
                "Schema {} holds one array per path, the path {} is in several containers", to_version, path
            ));
        }
    }
}

//============================================================================

void new_empty_eml_db(const oes::postgresql::Connection_ptr& db) {
    new_eml_db_prolog(db, false);
    new_eml_db_epilog(db, false, false);
}

//============================================================================

} // namespace oes::eml::openkv
