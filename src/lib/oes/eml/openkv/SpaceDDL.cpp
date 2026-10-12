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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "SpaceDDL.h"

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Statement.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>


namespace oes::eml::openkv {

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
    const std::vector<const char*> ddls{
        // TRANSACTION is removed since we can only have one transaction per connection in PostgreSQL
        // and it may interfere with outside initiated transaction.
        R"sql(
create table hdr
( version integer not null primary key
, created text not null
, creator text not null
, description text not null
, CHECK(pg_typeof(version) = 'integer'::regtype AND version > 0)
, CHECK(pg_typeof(created) = 'text'::regtype AND length(created) = 10 AND
        date(created) is not null)
, CHECK(pg_typeof(creator) = 'text'::regtype AND length(creator) = 3)
, CHECK(pg_typeof(description) = 'text'::regtype AND length(description) > 0)
);
        )sql",
        R"sql(
create table uri
( id bigserial primary key
, ns text not null
, pp text UNIQUE
, ml text UNIQUE
, vs text not null default '2.0'
, UNIQUE(ns, vs)
);
        )sql",
        R"sql(
create table typ
( id serial primary key
, uri_id bigint not null references uri(id)
, xml text -- XML element (local) name
, UNIQUE(uri_id, xml)
);
        )sql",
        R"sql(
create table xpa
( id bigserial primary key
, th text UNIQUE
);
        )sql",
        R"sql(
create table res
( obj_id bigserial primary key
, typ_id int                not null references typ(id)
, guid uuid null
, name text
, version text -- Omit? Untyped?
, createdAt bigint
, updatedAt bigint
, createdBy text
, updatedBy text
, xdata_json json
, xdata_avro bytea
, UNIQUE(typ_id, guid)
, storeCreated timestamp with time zone not null default now()
, storeUpdated timestamp with time zone not null default now()
, space_rel_id BIGINT REFERENCES admin.space_rel(id) NULL
);
-- Table of deleted resources
create table del_res
( guid   uuid not null
, typ_id int  not null references typ(id)
, name text
, version text -- Omit? Untyped?
, createdAt bigint
, updatedAt bigint
, createdBy text
, updatedBy text
, xdata_json json
, xdata_avro bytea
, UNIQUE(guid, typ_id)
, storeCreated timestamp with time zone not null default now()
, storeUpdated timestamp with time zone not null default now()
, storeDeleted timestamp with time zone not null default now()
);

-- Add trigger to update storeUpdated column
CREATE OR REPLACE FUNCTION update_storeUpdated_column()
    RETURNS TRIGGER AS $$
    BEGIN
        NEW.storeUpdated = now();
        RETURN NEW;
    END;
    $$ language 'plpgsql';
CREATE TRIGGER update_res_storeUpdated BEFORE UPDATE
    ON res FOR EACH ROW EXECUTE PROCEDURE 
    update_storeUpdated_column();

-- remove row from space_rel table if no res references it
CREATE OR REPLACE FUNCTION remove_space_rel_if_no_res() RETURNS TRIGGER AS $$
BEGIN
    EXECUTE format('DELETE FROM admin.space_rel WHERE id = $1 AND NOT EXISTS (SELECT 1 FROM %I.res WHERE space_rel_id = $2 limit 1)', TG_TABLE_SCHEMA)
	  USING OLD.space_rel_id, OLD.space_rel_id;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

-- add a trigger that invoke remove_space_rel_if_no_res when a row of res is edited or deleted
CREATE TRIGGER res_space_rel_trigger
AFTER DELETE OR UPDATE OF space_rel_id ON res
FOR EACH ROW
EXECUTE PROCEDURE remove_space_rel_if_no_res();
        )sql",
        R"sql(
create table rel
( id serial
, obj_id bigint not null references res(obj_id) on delete cascade
, dst_id bigint not null references res(obj_id)
, xpa_id bigint not null references xpa(id)
, cnt int default 1
, UNIQUE(obj_id, dst_id, xpa_id)
);
        )sql",
        R"sql(
-- table of xml objects
create table obj
( id bigserial primary key not null references res(obj_id) on delete cascade
, usize int -- uncompressed size (if compressed or chunked, null otherwise)
, zcomp int -- compressor (not compressed if null, 0 = ZLib)
, xml xml not null
);
        )sql",
        R"sql(
create table ary
( id bigserial primary key
, obj_id bigint references obj(id) on delete cascade
, xpa_id bigint references xpa(id)
, path  text not null UNIQUE
, type int not null
, rank1 int -- null means 1
, dim1 bigint not null
, dim2 bigint
, dim3 bigint
, dim4 bigint
, usize bigint
, zcomp bigint
, value bytea -- if small enough. Otherwise chunk-size
, chunk_length int
, CHECK((chunk_length = null) != (value = null))
--, UNIQUE(obj_id, xpa_id) FIXME: using path for now
)
        )sql",
        R"sql(
create table bin
( id bigserial
, ary_id bigint not null references ary(id) on delete cascade
, idx int not null check (idx >= 0)
, value bytea
, UNIQUE(ary_id, idx)
);
        )sql",
        R"sql(
create view resources as
    select obj_id, uri.ml ML, typ.xml as type,
        res.guid as guid, name, version,
        to_timestamp(createdAt/1e6) as createdAtUTC, createdBy,
        to_timestamp(updatedAt/1e6) as updatedAtUTC, updatedBy,
        storeCreated, storeUpdated,
        xdata_json, length(xdata_avro),
        (
        select string_agg
                ( case rank1
                when 1 then '1D - '||dim1||'x '
                when 2 then '2D - '||dim1||' x '||dim2
                when 3 then '3D - '||dim1||' x '||dim2||' x '||dim3
                when 4 then '4D - '||dim1||' x '||dim2||' x '||dim3||' x '||dim4
                else        'xD - Invalid dimensions'
                end
                ||' - '
                ||
                case type
                when 0 then 'int'
                when 1 then 'double'
                when 2 then 'float'
                when 3 then 'byte'
                when 4 then 'bool'
                when 5 then 'long'
                else 'UNKNOWN'
                end
                ||' - '
                ||substr(path, position('/' in path) + 38)
                , chr(10)
                )
            from ary
        where obj_id = res.obj_id
        group by obj_id
        ) as arrays
    from res
    join typ on res.typ_id = typ.id
    join uri on typ.uri_id = uri.id;
        )sql",
        R"sql(
create view obj_IjkGridRepresentation as
select id
  ,admin.eml_xpath(xml,'//eml:Title/text()') as title
  ,admin.eml_xpath(xml,'//@uuid') as uuid 
  ,admin.eml_xpath(xml,'//eml:Originator/text()') as originator
  ,admin.eml_xpath(xml,'//eml:Creation/text()')::timestamptz as creation
  ,split_part(admin.eml_xpath(xml,'//@xsi:type'),':',2) as type
  ,admin.eml_xpath(xml,'//eml:Editor/text()') as editor
  ,admin.eml_xpath(xml,'//resqml2:Ni/text()')::int as ni 
  ,admin.eml_xpath(xml,'//resqml2:Ni/text()')::int as nj 
  ,admin.eml_xpath(xml,'//resqml2:Nk/text()')::int as nk
  ,admin.eml_xpath(xml,'//resqml2:RepresentedInterpretation/eml:UUID/text()') as interpretation
  ,xml
  from obj
 where split_part(admin.eml_xpath(xml,'//@xsi:type'),':',2) like 'obj_IjkGridRepresentation';
        )sql",
        R"sql(

create view objects as
select r.*, obj.xml
  from resources r
  join obj on r.obj_id = obj.id

        )sql",
        R"sql(

create view relations as
select
	srctyp.xml as src_type,
	src.guid as src_guid,
	src.name as src_name,
	dsttyp.xml as dst_type,
	dst.guid as dst_guid,
	dst.name as dst_name,
	xpa.th,
	cnt
from rel
join res src on src.obj_id=rel.obj_id
join res dst on dst.obj_id=rel.dst_id
join xpa on rel.xpa_id=xpa.id
join typ srctyp on src.typ_id=srctyp.id
join typ dsttyp on dst.typ_id=dsttyp.id
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
        "insert into hdr"
        "( version, created, creator, description"
        ") values ($1, $2, $3, $4)"
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
        ( guid   uuid not null
        , typ_id int  not null references {}.typ(id)
        , name text
        , version text -- Omit? Untyped?
        , createdAt bigint
        , updatedAt bigint
        , createdBy text
        , updatedBy text
        , xdata_json json
        , xdata_avro bytea
        , UNIQUE(guid, typ_id)
        , storeCreated timestamp with time zone not null default now()
        , storeUpdated timestamp with time zone not null default now()
        , storeDeleted timestamp with time zone not null default now()
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
                NEW.storeUpdated = now();
                RETURN NEW;
            END;
            $$ language 'plpgsql';
        CREATE OR REPLACE TRIGGER update_res_storeUpdated BEFORE UPDATE
            ON {}.res FOR EACH ROW EXECUTE PROCEDURE 
            update_storeUpdated_column();

        -- remove row from space_rel table if no res references it
        CREATE OR REPLACE FUNCTION {}.remove_space_rel_if_no_res() RETURNS TRIGGER AS $$
        BEGIN
            EXECUTE format('DELETE FROM admin.space_rel WHERE id = $1 AND NOT EXISTS (SELECT 1 FROM %I.res WHERE space_rel_id = $2 limit 1)', TG_TABLE_SCHEMA)
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
        create or replace view {}.relations as
        select
            srctyp.xml as src_type,
            src.guid as src_guid,
            src.name as src_name,
            dsttyp.xml as dst_type,
            dst.guid as dst_guid,
            dst.name as dst_name,
            xpa.th,
            cnt
        from rel
        join res src on src.obj_id=rel.obj_id
        join res dst on dst.obj_id=rel.dst_id
        join xpa on rel.xpa_id=xpa.id
        join typ srctyp on src.typ_id=srctyp.id
        join typ dsttyp on dst.typ_id=dsttyp.id
    )sql";
    conn->execute(fmt::format(sql, schema));
}

//============================================================================

void new_empty_eml_db(const oes::postgresql::Connection_ptr& db) {
    new_eml_db_prolog(db, false);
    new_eml_db_epilog(db, false, false);
}

//============================================================================

} // namespace oes::eml::openkv
