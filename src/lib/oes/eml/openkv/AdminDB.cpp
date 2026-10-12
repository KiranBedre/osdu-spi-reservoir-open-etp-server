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

#include <iostream>
#include <filesystem>
#include <sstream>
#include <regex>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/trim.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "AdminDB.h"
#include "../etp12/MsgUtils.h"
#include <oes/common/etp12/ErrorCodes.h>
#include <oes/common/etp12/EmlUri.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/http/HTTP.h>
#include <oes/core/utils/Logger.h>

#include <oes/osdu/api/BaseClient.h>

namespace fs = std::filesystem;
using oes::postgresql::Statement;
using oes::core::Guid;
#include <oes/postgresql/Transaction.h>
#include <oes/postgresql/ErrorCodes.h>
#define POSTGRESQL_CONSTRAINT oes::postgresql::PG_23000

//============================================================================

namespace {

// 4C 69 4B 56 = "LiKV" in hex
const int magic_cookie__ = 0x4C694B56;

struct schema_version_info {
    int version;             //cppcheck-suppress unusedStructMember
    const char* created;     //cppcheck-suppress unusedStructMember
    const char* creator;     //cppcheck-suppress unusedStructMember
    const char* description; //cppcheck-suppress unusedStructMember
};
const std::vector<schema_version_info> schema_history__{
    //    YYYY-MM-DD
    { 1, "2020-01-22", "DDe", "First creation" },
    { 2, "2020-12-22", "DDe", "XDataMap now on two columns (JSON and Avro binary)" },
    { 3, "2023-09-08", "LDe", "Add space size" },
    { 4, "2023-09-21", "LDe", "Update clone function" },
    { 5, "2023-09-25", "LDe", "Add read only" },
    { 6, "2023-10-04", "LDe", "Add cross space reference" },
    { 7, "2023-11-15", "RCo",  "Add update space rels function" },
    { 8, "2023-11-16", "LDe", "Fix copy of serial obj_id" },
    { 9, "2023-11-17", "RCo",  "Fix copy of serials again" },
};

using oes::eml::openkv::Space;

using HTTP_ptr = std::shared_ptr<oes::core::HTTP>;
using HTTPFactory = std::function<HTTP_ptr()>;

void fetchSpace(Statement& stmt, Space& space) {
    space.uid_ = stmt.getString(0);
    space.path_ = stmt.getString(1);

    space.xdata_json_ = stmt.getString(2);

    int avro_size = 0;
    const auto* avro_bytes = static_cast<const uint8_t*>(
        stmt.getBlob(3, avro_size)
    );
    space.xdata_avro_.assign(avro_bytes, avro_bytes + avro_size);
    int64_t last_write = stmt.getInt64(5);
    if( last_write>0 ){
        space.last_write_ = last_write;
    }
    int64_t created = stmt.getInt64(6);
    space.created_ = created;

    space.dbfile_ = stmt.getString(4);
    space.size_str_ = stmt.getString(7);
    space.ro_       = stmt.getBoolean(8);
}

Space findSpaceBy(oes::postgresql::Statement& stmt, Space* alt_space = nullptr) {
    if (!stmt.step()) {
        stmt.reset();
        return {}; // not found
    }
    Space space;
    fetchSpace(stmt, space);
    if (alt_space) {
        if (stmt.step()) {
            fetchSpace(stmt, *alt_space);

            // more than two not possible
            // cppcheck-suppress assertWithSideEffect
            assert(!stmt.step());
        }
    } else {
        // cppcheck-suppress assertWithSideEffect
        assert(!stmt.step());
    }
    stmt.reset();
    return space;
}

void bindTextNoCopy(Statement& stmt, int index, const std::string& text) {
    stmt.bind(index,text);
}

void bindBlobNoCopy(Statement& stmt, int index, const std::vector<uint8_t>& blob) {
    if (blob.empty()) {
        stmt.bind(index); // NULL
        return;
    }
    int len = static_cast<int>(blob.size());
    stmt.bindBytea(index,(char*)(blob.data()),len);
}

} // namespace

namespace oes::eml::openkv {

//============================================================================

bool Space::isValid() const {
    // An empty uid_ or path_ is possible for the Default ETP 1.2 Dataspace
    return /*!uid_.empty() && !path_.empty() &&*/ !dbfile_.empty();
}

EmlUri Space::uri() const{
    return EmlUri::make_space(uid_);
}

bool Space::equals(const Space& rhs) const {
    const bool same =
        std::tie(uid_, path_, dbfile_) ==
        std::tie(rhs.uid_, rhs.path_, rhs.dbfile_)
    ;
    if (!same) {
        return false;
    }
    if (xdata_json_ == rhs.xdata_json_) {
        return true;
    }
    auto lhs_xdata_map = xdataAsMap();
    auto rhs_xdata_map = rhs.xdataAsMap();
    return lhs_xdata_map == rhs_xdata_map;
}

bool Space::lessThan(const Space& rhs) const {
    // FIXME: Natural key should be path most likely
    const bool same =
        std::tie(uid_, path_) <
        std::tie(rhs.uid_, rhs.path_)
    ;
    if (same) {
        return true;
    }

    // normalize xdata, for reliable comparison (order independent of the JSON)
    auto lhs_xdata_map = xdataAsMap();
    auto rhs_xdata_map = rhs.xdataAsMap();
    return lhs_xdata_map < rhs_xdata_map;
}

etp12::XDataMap Space::xdataAsMap() const {
    etp12::XDataMap xdata;
    const bool ok = etp12::xdataFromJsonAndAvroBinary(
        xdata, xdata_json_, xdata_avro_
    );
    OES_RELEASE_UNUSED(ok);
    assert(ok);

    return xdata;
}

void Space::setXDdataFromMap(const etp12::XDataMap& xdata) {
    std::tie(xdata_json_, xdata_avro_) = etp12::xdataToJsonAndAvroBinary(xdata);
}

// constant to represent 15mn in seconds, used when session is very long
const int64_t CACHE_TIMEOUT = 15 * 60; // 15mn in seconds

/**
 * \brief Check if the user read access to the dataspace is already cached
 * 
 * \return true if the user has read access to the dataspace, false otherwise
 *      if the cache is too old, it will return std::nullopt
 */
std::optional<bool> Space::canRead() const {
    if (read_access_cache_updated_ > 0) {
        const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        if (now - read_access_cache_updated_ < CACHE_TIMEOUT) {
            return read_access_;
        }
    }
    return std::nullopt;
}

/**
 * \brief Check if the user write access to the dataspace is already cached
 * 
 * \return true if the user has write access to the dataspace, false otherwise
 *     if the cache is too old, it will return std::nullopt
*/
std::optional<bool> Space::canWrite() const{
    if (write_access_cache_updated_ > 0) {
        const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        if (now - write_access_cache_updated_ < CACHE_TIMEOUT) {
            return write_access_;
        }
    }
    return std::nullopt;
}

/*!
 * \brief Convert \em logical user names/paths, to filesystem-friendly \em physical names/paths.
 *
 * The user-provided \a space_path argument may contain characters not accepted for
 * a file or folder on a filesystem, or a schema name of a RDBMS (i.e. a database),
 * or be longer than some backend-specific limit (30 chars in Oracle, or 63 in PostgreSQL).
 *
 * This method first checks constraints specific to OpenKV on the forms \a space_path
 * can take, and second performs a one-way mapping from \a space_path to \a db_path.
 *
 * Either \a space_path is of the form "folder/name", and \a input_epc_file is ignored,
 * or \a space_path is of the form "folder", and \a input_epc_file's trailing filename
 * is used, with any extension removed. The second form allows to name the ETP space
 * the same way as an imported EPC file, while the first form allows using a different
 * name than the EPC's. The first form is used for creating new empty ETP spaces too.
 *
 * \param space_path the ETP dataspace name
 * \param[out] path the ETP dataspace's \em logical path.
 *     Will always be \a name concatenated to \a folder, with a slash in between.
 *     Will be different from \a db_path, the \em physical path for the ETP dataspace.
 * \param[out] db_path The \em physical path for the ETP dataspace, compatible with postgresql schema requirements.
 * \param[out] folder() the folder-part of the \em logical \a path
 * \param[out] name the name-part of the \em logical \a path
 * \param[out] err_msg 
 * \return true on valid \a space_path; false otherwise.
 */
bool Space::validatePath(
    const std::string& space_path,
    std::string& path, std::string& db_path,
    std::string& folder, std::string& name,
    std::string& err_msg
) {
    if (space_path.empty()) {
        err_msg = "Empty";
        return false;
    }

    // space-path should be of the form folder/name, where
    // folder shouldn't contain any back-slashes, and at most a single slash,
    // If name is omitted, it will be "default".
    if (space_path.find('\\') != std::string::npos) {
        err_msg = "Cannot contain back-slashes";
        return false;
    }

    const size_t first_slash = space_path.find('/');
    const size_t last_slash = space_path.rfind('/');
    if (first_slash != last_slash) {
        assert(first_slash != std::string::npos);
        err_msg = "Cannot contain more than one forward-slash";
        return false;
    }

    // check that space-path contains only allowed symbols and has minimum length of 3
    if (space_path.size() < 3) {
        err_msg = "Dataspace path length should be >= 3 symbols";
        return false;
    }
    std::regex re(R"(^[A-Za-z0-9_/\-\.]{3,}$)");
    std::smatch m;
    if (!std::regex_match (space_path, m, re)) {
        std::regex neg_re(R"([^(A-Za-z0-9_/\-\.)])");
        std::string suggested_path = std::regex_replace(space_path, neg_re, "_");
        err_msg = R"(
Dataspace path contains unsupported symbols (supported expr: ^[A-Za-z0-9_/\-\.]{3,}$).
The correct path may be: )" + suggested_path;
        return false;
    }

    if (first_slash == std::string::npos) {
        folder = space_path;
        name = "default";
    } else {
        if ((first_slash + 1) >= space_path.size()) {
            err_msg = "Cannot end with forward-slash";
            return false;
        }
        folder = space_path.substr(0, first_slash);
        name = space_path.substr(first_slash + 1);
    }

    path = fmt::format("{}/{}", folder, name);

    const Guid guid = Guid::make_deterministic(Guid::dns_namespace(), path);
    db_path = std::regex_replace(path, std::regex(R"([\W])"), "_",  std::regex_constants::match_any);
    // Postgres schema name maximum size is 63 and should start with a letter
    db_path = "s" + db_path.substr(0,30) + guid.toStringNoDash();
    std::transform(db_path.begin(), db_path.end(), db_path.begin(), ::tolower );
    return true;
}

//============================================================================

OpenKVAdminDB::OpenKVAdminDB(Connection_ptr conn)
    : conn_(std::move(conn))
{ }

int OpenKVAdminDB::app_id() const{
    return magic_cookie__;
}

/*!
 * \brief Reset the cached statements
 */
void OpenKVAdminDB::reset_statements(){
    select_one_user_.reset();
    insert_user_.reset();
    select_user_names_.reset();

    select_space_by_uid_.reset();
    select_space_by_path_.reset();
    select_space_by_either_.reset();
    select_spaces_.reset();
    select_spaces_after_.reset();
    insert_space_.reset();
    delete_space_.reset();
    lock_space_.reset();
    select_space_ro_.reset();
    update_space_info_.reset();
    space_is_referenced_.reset();

    space_includes_reference_.reset();
    create_space_reference_.reset();
    check_space_reference_.reset();
    delete_space_reference_.reset();
}

bool OpenKVAdminDB::isSchemaCreated() const {
    std::string tables = conn_->executeString(R"sql(
select string_agg(name,',')
  from (
    select tablename as name
        from pg_catalog.pg_tables
    where tablename in ('header_schema_history','help','spaces','users')
    order by name
  ) as name
    )sql");

    return tables == "header_schema_history,help,spaces,users";
}

bool OpenKVAdminDB::isSchemaUpToDate(bool version_only, int* current, int* latest) const {
    Statement stmt = conn_->prepare("SELECT schema_name FROM information_schema.schemata WHERE schema_name = 'admin'");
    if (stmt.execute() != 1){
        return false;
    }
    stmt.reset();
    const std::string str = conn_->executeString("SELECT obj_description('admin'::regnamespace, 'pg_namespace')");
    if( str.empty() ){
        return false;
    }
    std::string app_str = str.substr(0, str.find(':'));
    if(app_str.empty() ){
        return false;
    }
    const int app_id = std::stoi( app_str );
    if (app_id != magic_cookie__) {
        return false;
    }

    if( latest ){
        *latest = schema_history__.back().version;
    }

    std::string user_v_str = str.substr(str.find(':')+1);
    const int user_v = std::stoi( user_v_str );
    if( current ){
        *current = user_v;
    }

    if (user_v != schema_history__.back().version) {
        return false;
    }

    if (version_only) {
        return true;
    }

    // TODO: Deep introspection

    return true;
}

/**
 * \brief Upgrade the OpenKV admin schema from \a start_version to \a target_version.
 * 
 * \param start_version the current version of the OpenKV admin schema
 * \param target_version the target version of the OpenKV admin schema
 * \return true if the upgrade was successful, false otherwise
*/
bool OpenKVAdminDB::upgradeSchema(int start_version, int target_version)
{
    bool upgraded = false;
    std::ostringstream oss_version;
    oss_version << "COMMENT ON SCHEMA admin is '" << magic_cookie__ << ":" << schema_history__.back().version << "';";
    const std::string version_pragmas = oss_version.str();
    try {
         oes::postgresql::Transaction tx(conn_);
        if(start_version<3 && target_version>=3){
         conn_->execute(R"sql(
    ALTER TABLE admin.spaces 
    ADD COLUMN size text not null default '0 kB',
    ADD COLUMN last_write timestamp with time zone default now();
    CREATE OR REPLACE FUNCTION admin.get_space_size(text) RETURNS text
    VOLATILE STRICT
    LANGUAGE plpgsql AS $$
    DECLARE
        size text := '';
    BEGIN
        size := pg_size_pretty(sum(pg_total_relation_size(schemaname::text || '.' || tablename::text))) from pg_tables where schemaname = lower($1);
        IF (size is null)
        THEN
            RETURN '0 kB';
        ELSE
            RETURN size;
        END IF;
    END;
    $$;
    )sql");
        }
        if(start_version<5 && target_version>=5){
            conn_->execute(R"sql(
    ALTER TABLE admin.spaces 
    ADD COLUMN IF NOT EXISTS ro boolean default false;
                )sql");
        }
        if(start_version<6 && target_version>=6){
            conn_->execute(R"sql(
    CREATE TABLE IF NOT EXISTS admin.space_rel (
        id SERIAL PRIMARY KEY,
        source_sp TEXT REFERENCES admin.spaces(uid) ON DELETE CASCADE,
        target_sp TEXT REFERENCES admin.spaces(uid),
        CONSTRAINT unique_origin_target_pair UNIQUE (source_sp, target_sp)
    );
            )sql");
        }
        if(start_version<9 && target_version>=4){ // Update clone function is required for version 4 and above
            update_clone_schema_function(conn_);
        }
        conn_->execute(version_pragmas.c_str());
        tx.commit();
        upgraded = true;
    } catch (const oes::postgresql::PostgreSQLException& ex) {
        OES_ERROR("Unable to upgrade OpenKV admin schema from version {} to {}: {}", start_version, target_version, ex.what());
        return false;
    }
    const std::vector<Space> spaces = getSpaces();
    for (const Space& space : spaces) {
        updateSpaceInfo(space);
    }

    return upgraded;
}

bool OpenKVAdminDB::createSchema() {

    // TODO: WAL mode

    oes::postgresql::Transaction tx(conn_);
    // Page size in PostgreSQL seems to be always 8192
    // PostgreSQL enforce foreign keys constraint
    std::ostringstream oss_version;
    oss_version << "COMMENT ON SCHEMA admin is '" << magic_cookie__ << ":" << schema_history__.back().version << "';";
    const std::string version_pragmas = oss_version.str();
    const std::vector<const char*> ddls{
        R"sql(
create schema if not exists admin;
        )sql",
        version_pragmas.c_str(),
        R"sql(
create table if not exists admin.header_schema_history
( version integer not null primary key
, created text not null
, creator text not null
, description text not null
, CHECK(pg_typeof(version) = 'integer'::regtype AND version > 0)
, CHECK(pg_typeof(created) = 'text'::regtype AND length(created) = 10 AND date(created) is not null)
, CHECK(pg_typeof(creator) = 'text'::regtype AND length(creator) = 3)
, CHECK(pg_typeof(description) = 'text'::regtype AND length(description) > 0)
)
        )sql", 
        R"sql(
create table if not exists admin.help
( "table or view" text not null primary key
, summary text not null
, description text
)
        )sql",
        R"sql(
create table if not exists admin.spaces
( uid text not null PRIMARY KEY
, path text not null UNIQUE
, xdata_json json
, xdata_avro bytea
, dbfile text not null
, created timestamp with time zone not null default now()
, size text not null default '0 kB'
, last_write timestamp with time zone default now()
, ro boolean default false
);
        )sql",
        R"sql(
create table if not exists admin.users
( name text not null PRIMARY KEY
, password text CONSTRAINT users_ck1 CHECK(password is null OR pg_typeof(password) = 'text'::regtype)
, passhash bytea CONSTRAINT users_ck2 CHECK(passhash is null OR pg_typeof(passhash) = 'bytea'::regtype)
, hashtype text CONSTRAINT users_ck3 CHECK(hashtype is null OR pg_typeof(hashtype) = 'text'::regtype)
, CONSTRAINT users_ck4 CHECK(
    (password is not null AND passhash is null) OR
    (passhash is not null AND password is null)
    )
, CONSTRAINT users_ck5 CHECK(
    (passhash is null AND hashtype is null) OR
    (passhash is not null AND hashtype is not null)
    )
);
        )sql",
        R"sql(
-- Create new space_rel table to capture the relationship between spaces
CREATE TABLE if not exists admin.space_rel (
    id BIGSERIAL PRIMARY KEY,
    source_sp TEXT REFERENCES admin.spaces(uid) ON DELETE CASCADE,
    target_sp TEXT REFERENCES admin.spaces(uid),
    CONSTRAINT unique_origin_target_pair UNIQUE (source_sp, target_sp)
);
        )sql",
    };

    // TODO: groups, user-group membership, grants, etc...
    // Note that the spaces table is in the RESQML/ETP sense,
    // not the OpenDB sense.

    for (const char* ddl : ddls) {
        try {
            std::string sql(ddl);
            boost::trim(sql);
            Statement stmt = conn_->prepare(std::move(sql));
            const bool has_rows = stmt.step();
            OES_ASSERT(!has_rows, ;);
        } catch (const oes::postgresql::PostgreSQLException& ex) {
            OES_ERROR(
                "Unable to create OpenKV admin schema: {}\n"
                "\twhile executing statement:\n{}",
                ex.what(), ddl
            );
            tx.rollback();
            return false;
        }
    }
    try {
    conn_->execute(R"sql(
CREATE OR REPLACE FUNCTION admin.get_lo_size(oid) RETURNS bigint
VOLATILE STRICT
LANGUAGE 'plpgsql'
AS $$
DECLARE
    fd integer;
    sz bigint;
BEGIN
    -- The mode 40000 corresponds to the PostgreSQL LO mode INV_READ = 0x40000.
    fd := lo_open($1, x'40000'::int);
    -- Seek to the end.
    PERFORM lo_lseek(fd, 0, 2);
    -- Fetch the current file position
    sz := lo_tell(fd);
    -- Remember to close it, since the function may be called as part of a larger transaction.
    PERFORM lo_close(fd);
    -- Return the size.
    RETURN sz;
END;
$$;
--- Return the xml element of attribute corresponding to xpath in energitics xml document
CREATE OR REPLACE FUNCTION admin.eml_xpath(xml,text) RETURNS text
VOLATILE STRICT
LANGUAGE 'plpgsql'
AS $$
BEGIN
    RETURN ((xpath($2, $1, ARRAY[
        ARRAY['eml', 'http://www.energistics.org/energyml/data/commonv2'],
        ARRAY['resqml2', 'http://www.energistics.org/energyml/data/resqmlv2'],
        ARRAY['resqml', 'http://www.energistics.org/energyml/data/resqmlv2'],
        ARRAY['xsi', 'http://www.w3.org/2001/XMLSchema-instance']
    ]))[1])::text;
END;
$$;
--- Return the timestamp for last changed item in dataspace
CREATE OR REPLACE FUNCTION admin.get_space_last_write(text) RETURNS timestamp with time zone
    VOLATILE STRICT
    LANGUAGE plpgsql AS $$
    DECLARE
        latest timestamp with time zone := null;
    BEGIN
        IF (SELECT to_regnamespace($1)!=null)
        THEN 
        EXECUTE format('select coalesce(max(a.timestamp),b.created) FROM %s.obj a join admin.spaces b on b.dbfile=%s group by b.created',$1::regnamespace,quote_nullable($1))
        INTO latest;
        ELSE
        EXECUTE format('select created from admin.spaces where dbfile=%s group by created',quote_nullable($1))
        INTO latest;
        END IF;
        RETURN latest;
    END;
    $$;
--- Return the size of a dataspace
CREATE OR REPLACE FUNCTION admin.get_space_size(text) RETURNS text
    VOLATILE STRICT
    LANGUAGE plpgsql AS $$
    DECLARE
        size text := '';
    BEGIN
        size := pg_size_pretty(sum(pg_total_relation_size(schemaname::text || '.' || tablename::text))) from pg_tables where schemaname = lower($1);
        IF (size is null)
        THEN
            RETURN '0 kB';
        ELSE
            RETURN size;
        END IF;
    END;
    $$;
-- Create dataspace view
create or replace view admin.dataspaces as
    select uid, path, xdata_json, xdata_avro, dbfile, 
    admin.get_space_last_write(dbfile) as last_write,
    created,
    ro
    from admin.spaces ;
    )sql");
    update_clone_schema_function(conn_);
    } catch (const oes::postgresql::PostgreSQLException& ex) {
        OES_ERROR("Unable to create OpenKV functions and view: {}", ex.what());
        tx.rollback();
        return false;
    }
    tx.commit();
    return true;
}

/*!
 * Add the latest version of the clone_schema function to the admin schema
*/
void OpenKVAdminDB::update_clone_schema_function(Connection_ptr conn) {
    conn->execute(R"sql(
-- Clone an entire schema and its content
CREATE OR REPLACE FUNCTION admin.clone_schema(source_schema text, dest_schema text) RETURNS void AS
$$
DECLARE
  object text;
  value bigint;
  buffer text;
  default_ text;
  sequence_ text;
  column_ text;
  fullcolumn text;
  fulltable text;
  mess text;
  constraint_name_ text;
  constraint_def_ text;
  object_table text;
  act_timing text;
  ev_manipulation text;
  act_statement text;
  act_orientation text;
  rout_type text;
  rout_data text;
  rout_definition text;
BEGIN
    -- Make sure that the active schema is NOT the source schema. Otherwise
    -- ALL queries to get foreign keys, sequences etc give us a short answer
    -- without the schema name and NONE of the replaces below ever work.
    SET SEARCH_PATH = 'admin';
    RAISE NOTICE 'Cloning schema from (%) to (%) (current schema: %).',  source_schema, dest_schema, current_schema();
    IF NOT EXISTS ( SELECT  *
                FROM    pg_catalog.pg_namespace
                WHERE   nspname = dest_schema )
    THEN
    BEGIN
        EXECUTE 'CREATE SCHEMA ' || dest_schema ;
        -- Create the sequences and reset their current value.
        FOR object, value IN
            select sequencename, last_value from pg_sequences where schemaname = source_schema
        LOOP
            buffer := dest_schema || '.' || object ;
            RAISE NOTICE 'Creating sequence % with current value %', buffer, value;
            EXECUTE 'CREATE SEQUENCE ' || buffer ;
            PERFORM setval( buffer, value, true );
        END LOOP;
        -- Create the tables.
        FOR object IN
            SELECT table_name::text FROM information_schema.TABLES WHERE table_schema = source_schema AND table_type='BASE TABLE'
        LOOP
            EXECUTE format('CREATE TABLE %1$I.%2$I (LIKE %3$I.%4$I INCLUDING CONSTRAINTS INCLUDING INDEXES INCLUDING DEFAULTS INCLUDING IDENTITY)',dest_schema,object,source_schema,object);
            FOR column_, default_ IN
              SELECT column_name::text, column_default::text FROM information_schema.COLUMNS
              WHERE table_schema = dest_schema AND table_name = object AND column_default LIKE 'nextval(%' || source_schema || '.%::regclass)'
            LOOP
              buffer := dest_schema || '.' || object ;
              RAISE NOTICE 'Replacing sequence reference % by % in column %', default_, REPLACE(default_, source_schema, dest_schema), buffer || '.' || column_;
              EXECUTE 'ALTER TABLE ' || buffer || ' ALTER COLUMN ' || column_ || ' SET DEFAULT ' || REPLACE(default_, source_schema, dest_schema);
            END LOOP;
        END LOOP;
        -- Create views
        FOR object IN
            SELECT table_name::text FROM information_schema.TABLES WHERE table_schema = source_schema AND table_type='VIEW' ORDER BY table_name::text DESC
        LOOP
            EXECUTE format('create view %1$I.%2$I as %3$s', dest_schema, object, replace(pg_get_viewdef(source_schema ||'.' || object),source_schema, dest_schema));
        END LOOP;

        -- Create trigger functions
        FOR rout_type, object, rout_data, rout_definition IN
            SELECT routine_type, routine_name, data_type, routine_definition FROM information_schema.routines WHERE specific_schema = source_schema
        LOOP
            EXECUTE format('CREATE OR REPLACE %1$s %2$s.%3$s() RETURNS %4$s AS $s$ %5$s $s$ language "plpgsql";', rout_type, dest_schema, object, rout_data, rout_definition);
        END LOOP;

        FOR object, act_timing, ev_manipulation, object_table, act_orientation, act_statement IN
        SELECT trigger_name, action_timing, event_manipulation, event_object_table, action_orientation, action_statement FROM information_schema.triggers where trigger_schema = source_schema
        LOOP
            EXECUTE format('CREATE OR REPLACE TRIGGER %1$s %2$s %3$s ON %4$s.%5$s FOR EACH %6$s %7$s;', object, act_timing, ev_manipulation, dest_schema, object_table, act_orientation, replace(act_statement,source_schema,dest_schema));
        END LOOP;

        -- reset the sequence table owner.
        FOR object, column_, sequence_ IN
            SELECT table_name::text, column_name, sequence_name from (
               SELECT t.oid::regclass AS table_name, a.attname AS column_name, s.relname AS sequence_name
                 FROM pg_class AS t
                 JOIN pg_attribute AS a ON a.attrelid = t.oid
                 JOIN pg_depend AS d ON d.refobjid = t.oid AND d.refobjsubid = a.attnum
                 JOIN pg_class AS s ON s.oid = d.objid
                where d.refobjsubid<>3
            ) as aaa WHERE table_name::text LIKE source_schema || '%.%'
        LOOP
            buffer := dest_schema || '.' || sequence_ ;
            RAISE NOTICE 'Replacing sequence % owner from % to %', buffer, object || '.' || column_, REPLACE(object, source_schema, dest_schema) || '.' || column_;
            EXECUTE format('ALTER SEQUENCE IF EXISTS %1$s OWNED BY %2$s.%3$s', buffer, REPLACE(object, source_schema, dest_schema), column_ );
        END LOOP;
        FOR object IN
        SELECT table_name::text FROM information_schema.TABLES WHERE table_schema = source_schema and table_type='BASE TABLE'
        LOOP
            EXECUTE format('INSERT INTO %1$I.%2$I SELECT * FROM %3$I.%4$I',dest_schema,object,source_schema,object);
        END LOOP;
        -- reiterate tables and create foreign keys
        FOR object IN
        SELECT table_name::text FROM information_schema.TABLES WHERE table_schema = source_schema and table_type='BASE TABLE'
        LOOP
            buffer := dest_schema || '.' || object ;
            -- create foreign keys
            FOR constraint_name_, constraint_def_ IN
            SELECT conname::text, pg_get_constraintdef(pg_constraint.oid) FROM pg_constraint INNER JOIN pg_class ON conrelid=pg_class.oid INNER JOIN pg_namespace ON pg_namespace.oid=pg_class.relnamespace WHERE contype='f' and relname=object and nspname=source_schema
            LOOP
            RAISE NOTICE 'Adding foreign key % from "%" to "%"', constraint_name_, constraint_def_, REPLACE(constraint_def_, source_schema||'.', dest_schema||'.');
            EXECUTE 'ALTER TABLE '|| buffer ||' ADD CONSTRAINT '|| constraint_name_ ||' '|| REPLACE(constraint_def_, source_schema||'.', dest_schema||'.');
            END LOOP;
        END LOOP;
    END;
    END IF;
END;
$$ LANGUAGE plpgsql VOLATILE;
        )sql");

        //Create a new postgres function that copies the resources and associated tables from one space to another
        //The target space can be empty or not.
        conn->execute(R"sql(
CREATE OR REPLACE FUNCTION admin.import_space_content(source_space text, dest_space text) RETURNS void AS
$$
DECLARE
  dest_schema TEXT := '';
  source_schema TEXT := '';
  rel_id BIGINT := 0;
  is_empty BOOLEAN := true;
BEGIN
  RAISE NOTICE 'Importing reference schema from % to %.',  source_space, dest_space;

  -- RAISE exception id source space is not read only or destination space is read only
  IF (SELECT ro FROM admin.spaces WHERE uid = source_space) = false
  THEN
    RAISE EXCEPTION 'Source space is not read only';
  END IF;
  IF (SELECT ro FROM admin.spaces WHERE uid = dest_space) = true
  THEN
    RAISE EXCEPTION 'Destination space is read only';
  END IF;

  SELECT lower(dbfile) from admin.spaces WHERE uid = source_space INTO source_schema;
  SELECT lower(dbfile) from admin.spaces WHERE uid = dest_space INTO dest_schema;
  EXECUTE format('SELECT NOT EXISTS (SELECT 1 FROM %I.uri limit 1)', dest_schema) INTO is_empty;
  EXECUTE format('INSERT INTO %I.uri SELECT * FROM %I.uri ON CONFLICT DO NOTHING', dest_schema, source_schema);
  EXECUTE format('SELECT setval(''%I.uri_id_seq'', (SELECT MAX(id) FROM %I.uri))', dest_schema, dest_schema);
  EXECUTE format('INSERT INTO admin.space_rel(source_sp,target_sp)
        SELECT %L, target_sp from admin.space_rel as rel1 WHERE source_sp=%L ON CONFLICT DO NOTHING',
        dest_space, source_space
  );
  SELECT id FROM admin.space_rel WHERE source_sp = dest_space AND target_sp = source_space INTO rel_id;
  IF rel_id IS NULL THEN
    INSERT INTO admin.space_rel(source_sp,target_sp) VALUES(dest_space, source_space) ON CONFLICT DO NOTHING RETURNING id INTO rel_id;
  END IF;
  IF is_empty THEN
    EXECUTE format('INSERT INTO %I.typ SELECT * FROM %I.typ', dest_schema, source_schema);
    EXECUTE format('SELECT setval(''%I.typ_id_seq'', (SELECT MAX(id) FROM %I.typ))', dest_schema, dest_schema);
    EXECUTE format('INSERT INTO %I.res(obj_id, typ_id, guid, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, space_rel_id)
		SELECT obj_id, typ_id, guid, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, 
        COALESCE(
            (SELECT id FROM admin.space_rel AS rela WHERE source_sp=%L AND target_sp = 
                (SELECT target_sp FROM admin.space_rel AS rel2 WHERE rel2.id = res1.space_rel_id)
            )
            ,%L)
         FROM %I.res as res1
        ', dest_schema, dest_space, rel_id, source_schema);
        -- COALESCE(space_rel_id,(SELECT id from admin.space_rel where source_sp=%L and target_sp=%L)::bigint) FROM %I.res', dest_schema, dest_space, source_space, source_schema);
    EXECUTE format('SELECT setval(''%I.res_obj_id_seq'', (SELECT MAX(obj_id) FROM %I.res))', dest_schema, dest_schema);
    EXECUTE format('INSERT INTO %I.xpa SELECT * FROM %I.xpa', dest_schema, source_schema);
    EXECUTE format('SELECT setval(''%I.xpa_id_seq'', (SELECT MAX(id) FROM %I.xpa))', dest_schema, dest_schema);
    EXECUTE format('INSERT INTO %I.rel SELECT * FROM %I.rel', dest_schema, source_schema);
    EXECUTE format('SELECT setval(''%I.rel_id_seq'', (SELECT MAX(id) FROM %I.rel))', dest_schema, dest_schema);
  ELSE
    RAISE NOTICE 'NOT EMPTY';
    EXECUTE format('INSERT INTO %I.uri SELECT * FROM %I.uri ON CONFLICT DO NOTHING;', dest_schema, source_schema);
    EXECUTE format('SELECT setval(''%I.uri_id_seq'', (SELECT MAX(id) FROM %I.uri))', dest_schema, dest_schema);
    EXECUTE format('INSERT INTO %I.typ(uri_id, xml) SELECT uri2.id, typ1.xml FROM %I.typ AS typ1 
                    JOIN %I.uri AS uri1 ON typ1.uri_id=uri1.id 
                    JOIN %I.uri AS uri2 ON uri2.ml=uri1.ml ON CONFLICT DO NOTHING;',
                    dest_schema, source_schema, source_schema, dest_schema);
    EXECUTE format('SELECT setval(''%I.typ_id_seq'', (SELECT MAX(id) FROM %I.typ))', dest_schema, dest_schema);
    EXECUTE format('INSERT INTO %I.res (typ_id, guid, name, version, createdAt, updatedAt, createdBy, updatedBy, xdata_json, xdata_avro, storeCreated, storeUpdated, space_rel_id)
        SELECT typ2.id, res1.guid, res1.name, res1.version, res1.createdAt, res1.updatedAt
        , res1.createdBy, res1.updatedBy, res1.xdata_json, res1.xdata_avro
        , res1.storeCreated, res1.storeUpdated,
        COALESCE(
            (SELECT id FROM admin.space_rel AS rela WHERE source_sp=%L AND target_sp = 
                (SELECT target_sp FROM admin.space_rel AS rel2 WHERE rel2.id = res1.space_rel_id)
            )
            ,%L)
        FROM %I.res AS res1
        join %I.typ AS typ1 ON typ1.id = res1.typ_id
        join %I.typ AS typ2 ON typ1.xml = typ2.xml
        join %I.uri AS uri1 ON typ1.uri_id=uri1.id
        join %I.uri AS uri2 ON uri1.ml = uri2.ml ON CONFLICT DO NOTHING;',
        dest_schema, dest_space, rel_id, source_schema, source_schema, dest_schema, source_schema, dest_schema);
    EXECUTE format('SELECT setval(''%I.res_obj_id_seq'', (SELECT MAX(obj_id) FROM %I.res))', dest_schema, dest_schema);
    EXECUTE format('INSERT INTO %I.xpa(th) SELECT th FROM %I.xpa ON CONFLICT DO NOTHING;',dest_schema, source_schema);
    EXECUTE format('SELECT setval(''%I.xpa_id_seq'', (SELECT MAX(id) FROM %I.xpa))', dest_schema, dest_schema);
    EXECUTE format('INSERT INTO %I.rel(obj_id, dst_id, xpa_id, cnt)
        SELECT res21.obj_id, res22.obj_id, xpa2.id, rel1.cnt from %I.rel as rel1
        JOIN %I.xpa as xpa1 on xpa1.id = rel1.xpa_id
        JOIN %I.xpa as xpa2 on xpa1.th = xpa2.th
        JOIN %I.res as res11 on res11.obj_id = rel1.obj_id
        JOIN %I.res as res12 on res12.obj_id = rel1.dst_id
        JOIN %I.res as res21 on res11.guid = res21.guid
        JOIN %I.res as res22 on res12.guid = res22.guid ON CONFLICT DO NOTHING;',
    dest_schema, source_schema, source_schema, dest_schema, source_schema, source_schema, dest_schema, dest_schema);
    EXECUTE format('SELECT setval(''%I.rel_id_seq'', (SELECT MAX(id) FROM %I.rel))', dest_schema, dest_schema);
  END IF;
  -- Remove unused space relationships
  EXECUTE format(
    'DELETE FROM admin.space_rel AS rel1 WHERE source_sp = %L AND NOT EXISTS (SELECT 1 FROM %I.res AS res1 WHERE res1.space_rel_id = rel1.id limit 1)',
     dest_space, dest_schema);
  --  Update size
  EXECUTE format(
    'UPDATE admin.spaces SET last_write = now(), size = admin.get_space_size(lower(dbfile)) WHERE uid = %L;',
    dest_space);
END;
$$
LANGUAGE plpgsql;
        )sql");
        
        //Create a new postgres function that updates the imported refs if a dataspace that was cloned contained any.
        conn->execute(R"sql(
CREATE OR REPLACE FUNCTION admin.update_space_rels(IN source_dataspace_uid text, IN dest_dataspace_uid text) RETURNS void  
AS $$
DECLARE
  dest_schema TEXT;
  original_rel_id BIGINT;
  new_rel_id BIGINT;
  source_space TEXT;
  target_space TEXT;
BEGIN
    -- Get dest schema.
    SELECT lower(dbfile) from admin.spaces WHERE uid = dest_dataspace_uid INTO dest_schema;

    -- disable the trigger that would attempt to remove the relation.
	EXECUTE 'ALTER TABLE ' || dest_schema ||'.res DISABLE TRIGGER res_space_rel_trigger' ;
    -- Loop over the existing relations.
    FOR original_rel_id in
    EXECUTE 'SELECT DISTINCT space_rel_id FROM ' || dest_schema ||'.res WHERE space_rel_id IS NOT NULL'
    LOOP
        RAISE NOTICE 'Found relation % in %', original_rel_id, dest_schema;
        SELECT source_sp, target_sp into source_space, target_space from admin.space_rel where id=original_rel_id;
        RAISE NOTICE 'Relation: %, Source: %, Target: %', original_rel_id, source_space, target_space;
        -- If source_space is source_dataspace_uid then replace it otherwise leave it as is.
        IF source_space = source_dataspace_uid THEN
            -- Add a relation between dest_dataspace_uid and the original target space.
            INSERT INTO admin.space_rel(source_sp,target_sp) VALUES (dest_dataspace_uid, target_space) RETURNING id into new_rel_id;
            RAISE NOTICE 'New relation id will be %', new_rel_id;
            EXECUTE 'UPDATE ' || dest_schema ||'.res SET space_rel_id=' || new_rel_id || ' WHERE space_rel_id=' || original_rel_id ;
        ELSE
            -- Relation is with another dataspace so is still valid.
            RAISE NOTICE 'Relation % left untouched.', original_rel_id;
		END IF;
    END LOOP;
    -- re-enable the trigger.
    EXECUTE 'ALTER TABLE ' || dest_schema ||'.res ENABLE TRIGGER res_space_rel_trigger' ;
END;
$$
LANGUAGE plpgsql;
        )sql");
}

size_t OpenKVAdminDB::spaceCount() const {
    return static_cast<size_t>(conn_->executeInt64("select count(*) from admin.spaces"));
}

std::vector<Space> OpenKVAdminDB::getSpaces(int64_t last_write_microseconds) {
    std::vector<Space> spaces;
    if(last_write_microseconds==0){
        if (!select_spaces_) {
            prepare(select_spaces_, R"sql(

    select uid, path, xdata_json, xdata_avro, dbfile,
        (EXTRACT(EPOCH FROM (last_write AT TIME ZONE 'UTC'))*1000000)::bigint,
        (EXTRACT(EPOCH FROM (created AT TIME ZONE 'UTC'))*1000000)::bigint,
        size,
        ro
    from admin.spaces
    order by path collate "C"

            )sql");
        }

        // 

        while (select_spaces_->step()) {
            Space& space = spaces.emplace_back();
            fetchSpace(*select_spaces_, space);
        }
        select_spaces_->reset();
    }else{
        if (!select_spaces_after_) {
            prepare(select_spaces_after_, R"sql(
    select uid, path, xdata_json, xdata_avro, dbfile,
        (EXTRACT(EPOCH FROM (last_write AT TIME ZONE 'UTC'))*1000000)::bigint,
        (EXTRACT(EPOCH FROM (created AT TIME ZONE 'UTC'))*1000000)::bigint,
        size,
        ro
    from admin.spaces
    WHERE (EXTRACT(EPOCH FROM last_write) * 1000000)::bigint > $1
    order by path collate "C"
            )sql");
        }
        select_spaces_after_->bind(1, last_write_microseconds);
        while (select_spaces_after_->step()) {
            Space& space = spaces.emplace_back();
            fetchSpace(*select_spaces_after_, space);
        }
        select_spaces_after_->reset();
    }

    return spaces;
}

Space OpenKVAdminDB::findSpace(const std::string& uid_or_path) {
    if (!select_space_by_either_) {
        prepare(select_space_by_either_, R"sql(

select uid, path, xdata_json, xdata_avro, dbfile,
    (EXTRACT(EPOCH FROM (last_write AT TIME ZONE 'UTC'))*1000000)::bigint,
    (EXTRACT(EPOCH FROM (created AT TIME ZONE 'UTC'))*1000000)::bigint,
    size,
    ro
  from admin.spaces
 where uid = $1
    or path = $2

        )sql");
    }

    select_space_by_either_->bind(1, uid_or_path);
    select_space_by_either_->bind(2, uid_or_path);

    Space alt_space;
    Space space = findSpaceBy(*select_space_by_either_, &alt_space);
    if (alt_space) {
        assert(space);
        if (alt_space.uid_ == uid_or_path) {
            assert(space.path_ == uid_or_path);
            return alt_space; // uid-match has precedence over path-match
        }
    }
    return space;
}

Space OpenKVAdminDB::findSpaceByUid(const std::string& uid) {
    if (!select_space_by_uid_) {
        prepare(select_space_by_uid_, R"sql(

select uid, path, xdata_json, xdata_avro, dbfile,
    (EXTRACT(EPOCH FROM (last_write AT TIME ZONE 'UTC'))*1000000)::bigint,
    (EXTRACT(EPOCH FROM (created AT TIME ZONE 'UTC'))*1000000)::bigint,
    size,
    ro
  from admin.spaces
 where uid = $1

        )sql");
    }

    select_space_by_uid_->bind(1, uid);
    return findSpaceBy(*select_space_by_uid_);
}

Space OpenKVAdminDB::findSpaceByPath(const std::string& path) {
    if (!select_space_by_path_) {
        prepare(select_space_by_path_, R"sql(

select uid, path, xdata_json, xdata_avro, dbfile,
    (EXTRACT(EPOCH FROM (last_write AT TIME ZONE 'UTC'))*1000000)::bigint,
    (EXTRACT(EPOCH FROM (created AT TIME ZONE 'UTC'))*1000000)::bigint,
    size,
    ro
  from admin.spaces
 where path = $1

        )sql");
    }

    select_space_by_path_->bind(1, path);
    return findSpaceBy(*select_space_by_path_);
}

bool OpenKVAdminDB::addSpaces(const std::vector<Space>& new_spaces) {
    if (!insert_space_) {
        prepare(insert_space_, R"sql(

insert into admin.spaces
( uid, path, xdata_json, xdata_avro, dbfile, size, last_write
) values ($1, $2, $3, $4, $5, admin.get_space_size($6::text), now())

        )sql");
    }

    int error_count = 0;
    for (Space space : new_spaces) {
        if (space.dbfile_.empty()) {
            ++error_count;
            break;
        }
        // For the special "Default" dataspace, of empty path,
        // we use an empty space uid as well, not a random uid.
        if (space.uid_.empty() && !space.path_.empty()) {
            // Use deterministic Guids instead?
            // Using server-instance random seed/namespace perhaps?
            space.uid_ = oes::core::Guid::make().toXmlString();
        }

        int idx = 0;
        insert_space_->bind(++idx, space.uid_);
        insert_space_->bind(++idx, space.path_);
        if(space.xdata_json_.empty()){
            insert_space_->bind(++idx);
        }else{
            bindTextNoCopy(*insert_space_, ++idx, space.xdata_json_);
        }
        bindBlobNoCopy(*insert_space_, ++idx, space.xdata_avro_);
        insert_space_->bind(++idx, space.dbfile_);
        insert_space_->bind(++idx, space.dbfile_);

        try {
            insert_space_->execute();
            insert_space_->reset();
        } catch (const oes::postgresql::PostgreSQLException& ex) {
            OES_ERROR(
                "Cannot add space {} {{{}}}: {}",
                space.path_, space.uid_, ex.what()
            );
            ++error_count;
            continue;
        }
    }

    return 0 == error_count;
}

bool OpenKVAdminDB::dropSpaces(const std::vector<std::string>& uids_or_paths) {
    if (!delete_space_) {
        prepare(delete_space_, R"sql(

delete from admin.spaces
 where uid = $1
    or path = $2
    and ro = false

        )sql");
    }

    int error_count = 0;
    for (const std::string& uid_or_path : uids_or_paths) {
        int idx = 0;
        delete_space_->bind(++idx, uid_or_path);
        delete_space_->bind(++idx, uid_or_path);

        // TODO: get rid of dbfile?

        try {
            const int64_t numChanges = delete_space_->execute();
            delete_space_->reset();
            if (numChanges != 1) {
                OES_ERROR(
                    "Cannot drop space {{{}}}: Uid not found or read only",
                    uid_or_path
                );
                ++error_count;
                continue;
            }
        } catch (const oes::postgresql::PostgreSQLException& ex) {
            OES_ERROR(
                "Cannot drop space {{{}}}: {}",
                uid_or_path, ex.what()
            );
            ++error_count;
            continue;
        }
    }

    return 0 == error_count;
}

/*!
 * \brief Check if a dataspace is referenced by another dataspace
 * \param target the dataspace to check
 * \return true if the dataspace is referenced, false otherwise
*/
bool OpenKVAdminDB::spaceIsReferenced(const Space& target){
    if(!space_is_referenced_){
        prepare(space_is_referenced_, R"sql(
select 1 from admin.space_rel where target_sp = $1 limit 1
        )sql");
    }
    space_is_referenced_->bind(1, target.uid_);
    space_is_referenced_->step();
    bool has_reference = !space_is_referenced_->isNull(0);
    space_is_referenced_->reset();
    return has_reference;
}

/**
 * \brief Lock or unlock a dataspace
 * 
 * \param uid_or_path the uid or path of the dataspace to lock or unlock
 * \param lock_space true to lock the dataspace, false to unlock it
 * \return true if the lock was successful, false otherwise
*/
ErrorInfo OpenKVAdminDB::lockSpace(const Space& space, bool lock_space) {
    if (!lock_space_) {
        // modify the ro column to the space table if the space is not referenced by another space
        prepare(lock_space_, R"sql(
            update admin.spaces as sp1 set ro = $1 where uid = $2
            and not (ro = true and exists (select 1 from admin.space_rel where target_sp = sp1.uid limit 1));
        )sql");
    }

    int idx = 0;
    lock_space_->bind(++idx, lock_space);
    lock_space_->bind(++idx, space.uid_);

    ErrorInfo err;
    try {
        const int64_t numChanges = lock_space_->execute();
        lock_space_->reset();
        if (numChanges != 1) {
            if(!lock_space && spaceIsReferenced(space)){
                err.m_code = oes::eml::etp12::Datatypes::ErrorCodes::EINVALID_ARGUMENT;
                err.m_message = fmt::format("Cannot unlock space {{{}}}: it is referenced by another space",space.uid_);
                return err;
            }
            err.m_code = oes::eml::etp12::Datatypes::ErrorCodes::EINVALID_ARGUMENT;
            err.m_message = fmt::format("Cannot lock space {{{}}}: Uid not found or read only",space.uid_);
            return err;
        }
    } catch (const oes::postgresql::PostgreSQLException& ex) {
        OES_ERROR(
            "Cannot lock space {{{}}}: {}",
            space.uid_, ex.what()
        );
        err.m_code = oes::eml::etp12::Datatypes::ErrorCodes::EINVALID_STATE;
        err.m_message = fmt::format("Cannot lock space {{{}}}", space.uid_);
        return err;
    }

    return err;
}

/*!
* \brief Check if a dataspace is read only
* \param uid_or_path the uid or path of the dataspace to check
* \return true if the dataspace is read only, false otherwise
*/
bool OpenKVAdminDB::spaceIsReadOnly(const Space& space) {
    if (!select_space_ro_) {
        prepare(select_space_ro_, R"sql(
select ro from admin.spaces where uid = $1
        )sql");
    }

    select_space_ro_->bind(1, space.uid_);

    bool ro = false;
    if (select_space_ro_->step()) {
        ro = select_space_ro_->getBoolean(0);
    }
    select_space_ro_->reset();

    return ro;
}

/*!
 * \brief Check if a dataspace contains any reference to another dataspace
 *
 * \param space the dataspace to check
*/
bool OpenKVAdminDB::spaceIncludesReference(const Space& space){
    if(!space_includes_reference_){
        prepare(space_includes_reference_, R"sql(select 1 from admin.space_rel where source_sp = $1 limit 1)sql");
    }
    space_includes_reference_->bind(1, space.uid_);
    space_includes_reference_->step();
    bool has_no_reference = space_includes_reference_->isNull(0);
    space_includes_reference_->reset();
    return !has_no_reference;
}

/*!
 * Update the last_write and size of the dataspace
 * 
 * \return true if the update was successful, false otherwise
*/
bool OpenKVAdminDB::updateSpaceInfo(const Space& space) {
    if (!update_space_info_) {
        prepare(update_space_info_, R"sql(
UPDATE admin.spaces SET last_write = now(), size = admin.get_space_size($1::text) WHERE uid = $2
        )sql");
    }

    int idx = 0;
    update_space_info_->bind(++idx, space.dbfile_);
    update_space_info_->bind(++idx, space.uid_);
    const int64_t numChanges = update_space_info_->execute();
    update_space_info_->reset();
    if (numChanges != 1) {
        OES_ERROR(
            "Unable to update space info: {}",
            space.uid_
        );
        return false;
    }
    return true;
}

/*!
* \brief Create a reference between two dataspace
* \param source the source dataspace
* \param target the target dataspace
* \return the id of the reference
*/
int64_t OpenKVAdminDB::createSpaceReference(const Space& source, const Space& target){
    if(!create_space_reference_){
        prepare(create_space_reference_, R"sql(
            INSERT INTO admin.space_rel(source_sp, target_sp) VALUES($1, $2) 
            ON CONFLICT DO NOTHING
            RETURNING id
        )sql");
    }
    create_space_reference_->bind(1, source.uid_);
    create_space_reference_->bind(2, target.uid_);
    create_space_reference_->step();
    int64_t id = create_space_reference_->getInt64(0);
    create_space_reference_->reset();
    return id;
}

/*!
* \brief Get the id of a reference between two dataspaces
* \param source the source dataspace
* \param target the target dataspace
* \return the id of the reference
*/
int64_t OpenKVAdminDB::getSpaceReference(const Space& source, const Space& target){
    if(!check_space_reference_){
        prepare(check_space_reference_, R"sql(
            SELECT id FROM admin.space_rel WHERE source_sp = $1 AND target_sp = $2
        )sql");
    }
    check_space_reference_->bind(1, source.uid_);
    check_space_reference_->bind(2, target.uid_);
    check_space_reference_->step();
    int64_t id = check_space_reference_->getInt64(0);
    check_space_reference_->reset();
    return id;
}

/*!
* \brief Delete a reference between two dataspaces
* \param source the source dataspace
* \param target the target dataspace
* \return true if the reference was deleted, false otherwise
*/
bool OpenKVAdminDB::deleteSpaceReference(const Space& source, const Space& target){
    if(!delete_space_reference_){
        prepare(delete_space_reference_, R"sql(
            DELETE FROM admin.space_rel WHERE source_sp = $1 AND target_sp = $2
        )sql");
    }
    try{
        delete_space_reference_->bind(1, source.uid_);
        delete_space_reference_->bind(2, target.uid_);
        delete_space_reference_->execute();
        delete_space_reference_->reset();
    }catch(const oes::postgresql::PostgreSQLException& ex){
        OES_ERROR("Unable to delete space reference between {} and {}: {}", source.uid_, target.uid_, ex.what());
        return false;
    }
    return true;
}

/** 
 * Import the content of multiple dataspaces into another dataspace
 * \param destination the destination dataspace
 * \param sources the list of dataspace to import
 * \return a list of errors that occurred during the import
*/
std::vector<ErrorInfo> OpenKVAdminDB::importSpaces(const Space& destination, const std::vector<Space>& sources ){
    std::string sql = R"sql(SELECT admin.import_space_content($1, $2))sql";
    Statement_opt stmt;
    prepare(stmt, std::move(sql));
    std::vector<ErrorInfo> errors;
    for(auto& ds : sources){
        stmt->bind(1, ds.uid_);
        stmt->bind(2, destination.uid_);
        ErrorInfo error;
        if(!stmt->step()){
            error.m_code = oes::eml::etp12::Datatypes::ErrorCodes::EINVALID_STATE;
            error.m_message = fmt::format("Cannot import space {}", ds.uid_);
        }
        errors.push_back(error);
        stmt->reset();
    }
    return errors;
}

void OpenKVAdminDB::dropAdmin(){
    if(conn_){
        conn_->execute("DROP SCHEMA IF EXISTS admin CASCADE");
    }
}

size_t OpenKVAdminDB::userCount() const {
    return static_cast<size_t>(conn_->executeInt64("select count(*) from admin.users"));
}

std::vector<std::string> OpenKVAdminDB::getUserNames() {
    if (!select_user_names_) {
        prepare(select_user_names_, "select name from admin.users");
    }

    std::vector<std::string> names;
    while (select_user_names_->step()) {
        names.emplace_back(select_user_names_->getString(0));
    }
    select_user_names_->reset();

    std::sort(names.begin(), names.end());
    return names;
}

bool OpenKVAdminDB::userPasswordMatches(
    const std::string& name, const std::string& password
) {
    if (!select_one_user_) {
        prepare(select_one_user_, R"sql(

select password, passhash, hashtype
  from admin.users
 where name = $1

        )sql");
    }

    int idx = 0;
    select_one_user_->bind(++idx, name);

    if (!select_one_user_->step()) {
        select_one_user_->reset();
        return false; // not found
    }

    idx = 0;
    int hashsize = 0;
    std::string db_password = select_one_user_->getString(idx++);
    const void* db_passhash = select_one_user_->getBlob(idx++, hashsize);
    std::string db_hashtype = select_one_user_->getString(idx++);

    // TODO: plug a more standard hash algorithm like SHA-1
    std::string guid_str = oes::core::Guid::make_deterministic(
        password
    ).toStringNoDash();

    std::string db_guid_str;
    if (hashsize) {
        // must be done before step() call below
        OES_ASSERT(db_hashtype == "oes_guid", return false;);
        OES_ASSERT(size_t(hashsize) == guid_str.size(), return false;);
        const char* cpasshash = static_cast<const char*>(db_passhash);
        db_guid_str = std::string(cpasshash,hashsize);
    }

    if (select_one_user_->step()) {
        select_one_user_->reset();
        return false; // expected single row only
    }
    select_one_user_->reset();

    if (!db_password.empty()) {
        return db_password == password;
    }
    return guid_str == db_guid_str;
}

bool OpenKVAdminDB::addUser(
    const std::string& name, const std::string& password, bool hash_password
) {
    if (!insert_user_) {
        prepare(insert_user_, R"sql(

insert into admin.users
( name, password, passhash, hashtype
) values ($1, $2, $3, $4)

        )sql");
    }

    int idx = 0;
    insert_user_->bind(++idx, name);
    if (hash_password) {
        // TODO: plug a more standard hash algorithm like SHA-1
        std::string guid_str = oes::core::Guid::make_deterministic(
            password
        ).toStringNoDash();
        insert_user_->bind(++idx); // i.e. null
        insert_user_->bind(++idx, guid_str);
        insert_user_->bind(++idx, std::string("oes_guid"));
    } else {
        insert_user_->bind(++idx, password);
        insert_user_->bind(++idx); // i.e. null
        insert_user_->bind(++idx); // i.e. null
    }

    try {
        insert_user_->step();
        insert_user_->reset();
    } catch (const oes::postgresql::PostgreSQLException& ex) {
        // The obvious error of adding the same user twice is "silent"
        std::string msg = ex.what();
        if ((ex.getErrorCode() != POSTGRESQL_CONSTRAINT) ||
            (msg.find("UNIQUE constraint failed") == std::string::npos)
        ) {
            OES_ERROR("Unable to add user {}: {}", name, ex.what());
        }
        return false;
    }

    return true;
}

void OpenKVAdminDB::prepare(Statement_opt& stmt, std::string sql) const {
    boost::trim(sql);
    stmt.emplace(conn_->prepare(std::move(sql)));
}

//============================================================================

} // nameapace oes::eml::openkv
