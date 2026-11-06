// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <chrono>
#include <cmath>
#include <functional>
#include <set>
#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <libpq-fe.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/Guid.h>

#include <oes/postgresql/DbConnectionPool.h>
#include <oes/postgresql/PhysicalName.h>

#include <oes/eml/etp12/MsgUtils.h>
#include <oes/eml/openkv/ArrayProto.h>
#include <oes/eml/openkv/DiscoProto.h>
#include <oes/eml/openkv/PGStream.h>
#include <oes/eml/openkv/SpaceOSDUProto.h>
#include <oes/eml/openkv/SpaceProto.h>
#include <oes/eml/openkv/Repo.h>
#include <oes/eml/openkv/SpaceDDL.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>


namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

using oes::core::Guid;

using oes::postgresql::relational::PhysicalName;

int64_t curr_usec() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto tp = time_point_cast<microseconds>(now);
    return tp.time_since_epoch().count();
};

int latestSpaceVersion() {
    int magic_cookie = 0;
    return space_schema_versions(magic_cookie).back().version;
}

//============================================================================

// Downgrade a schema version
    // and return success if the schema was downgraded to the expected version
    bool downgradeSpaceSchemaVersion(Connection_ptr conn, const std::string& dbFile, int expected_version)
    {
        const PhysicalName phys(dbFile, oes::postgresql::relational::IMPLICIT_LOWERCASE);

        // Update schema version
        const std::string vSql = fmt::format("SELECT obj_description({}::regnamespace, 'pg_namespace')",phys.singleQuotedName());
        const std::string str = conn->executeString(vSql);
        if( str.empty() ){
            return false;
        }
        std::string app_str = str.substr(0, str.find(':'));
        if(app_str.empty() ){
            return false;
        }
        const int app_id = std::stoi( app_str );

        std::string current_version_str = str.substr(str.find(':')+1);
        const int current_version = std::stoi( current_version_str );

        if (current_version > 17 && expected_version < 18) {
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
)sql", phys.quotedName()));
        }

        // Undo version 16 first: the new resources view uses res.space_rel_id, dropped below 10
        if (current_version > 15 && expected_version < 16) {
            const std::string sql = fmt::format(R"sql(
CREATE OR REPLACE VIEW {0}.resources AS
    SELECT obj_id, uri.ml ML, typ.xml AS type,
        res.guid AS guid, name, version,
        to_timestamp(createdAt/1e6) AS createdAtUTC, createdBy,
        to_timestamp(updatedAt/1e6) AS updatedAtUTC, updatedBy,
        storeCreated, storeUpdated,
        xdata_json, length(xdata_avro),
        (
        SELECT string_agg
                ( CASE rank1
                WHEN 1 THEN '1D - '||dim1||'x '
                WHEN 2 THEN '2D - '||dim1||' x '||dim2
                WHEN 3 THEN '3D - '||dim1||' x '||dim2||' x '||dim3
                WHEN 4 THEN '4D - '||dim1||' x '||dim2||' x '||dim3||' x '||dim4
                ELSE        'xD - Invalid dimensions'
                END
                ||' - '
                ||
                CASE type
                WHEN 0 THEN 'int'
                WHEN 1 THEN 'double'
                WHEN 2 THEN 'float'
                WHEN 3 THEN 'byte'
                WHEN 4 THEN 'bool'
                WHEN 5 THEN 'long'
                ELSE 'UNKNOWN'
                END
                ||' - '
                ||substr(path, position('/' IN path) + 38)
                , chr(10)
                )
            FROM {0}.ary
        WHERE obj_id = res.obj_id
        GROUP BY obj_id
        ) AS arrays
    FROM {0}.res
    JOIN {0}.typ ON res.typ_id = typ.id
    JOIN {0}.uri ON typ.uri_id = uri.id;
ALTER TABLE {0}.ary DROP COLUMN epc_guid;
ALTER TABLE {0}.ary ADD CONSTRAINT ary_path_key UNIQUE (path);
)sql", phys.quotedName());
            conn->execute(sql);
        }

        // Remove deleted resources table
        if(current_version>8 && expected_version<9){
            oes::eml::openkv::remove_deleted_resources_table(conn, phys.quotedName());
        }

        if(current_version>9 && expected_version<10){
            std::string sql = fmt::format("DROP TRIGGER res_space_rel_trigger ON {}.res;", phys.quotedName());
            sql += fmt::format("ALTER TABLE {}.res DROP COLUMN space_rel_id;", phys.quotedName());
            sql += fmt::format("ALTER TABLE {}.res ALTER COLUMN obj_id DROP DEFAULT;", phys.quotedName());
            sql += fmt::format("DROP SEQUENCE IF EXISTS {}.res_obj_id_seq;", phys.quotedName());
            sql += fmt::format("ALTER TABLE {}.res ALTER COLUMN guid SET NOT NULL;", phys.quotedName());
            sql += fmt::format("ALTER TABLE {}.obj DROP CONSTRAINT obj_id_fkey;", phys.quotedName());
            sql += fmt::format(
                "ALTER TABLE {}.res ADD FOREIGN KEY (obj_id) REFERENCES {}.obj(id) ON DELETE CASCADE;",
                phys.quotedName(), phys.quotedName()
            );
            sql += fmt::format("ALTER TABLE {}.rel DROP CONSTRAINT rel_obj_id_fkey;", phys.quotedName());
            sql += fmt::format("ALTER TABLE {}.rel DROP CONSTRAINT rel_dst_id_fkey;", phys.quotedName());
            sql += fmt::format(
                "ALTER TABLE {}.rel ADD FOREIGN KEY (obj_id) REFERENCES {}.obj(id) ON DELETE CASCADE;",
                phys.quotedName(), phys.quotedName()
            );
            sql += fmt::format(
                "ALTER TABLE {}.rel ADD FOREIGN KEY (dst_id) REFERENCES {}.obj(id);",
                phys.quotedName(), phys.quotedName()
            );
            conn->execute(sql);
            conn->set_current_schema(phys);
        }

        if (current_version > 12 && expected_version < 13){
            std::string sql = fmt::format("DROP TRIGGER IF EXISTS update_res_storeUpdated ON {}.res;", phys.quotedName());
            sql += fmt::format("DROP FUNCTION IF EXISTS {}.update_storeUpdated_column;", phys.quotedName());
            sql += fmt::format("DROP TRIGGER IF EXISTS res_space_rel_trigger ON {}.res;", phys.quotedName());
            sql += fmt::format("DROP FUNCTION IF EXISTS {}.remove_space_rel_if_no_res;", phys.quotedName());
            conn->execute(sql);
            conn->set_current_schema(phys);
        }
        if (current_version > 13 && expected_version < 14){
            std::string sql = fmt::format("ALTER TABLE {}.uri DROP CONSTRAINT IF EXISTS uri_ns_vs_key;", phys.quotedName());
            sql += fmt::format("ALTER TABLE {}.uri DROP COLUMN vs;", phys.quotedName());
            sql += fmt::format("ALTER TABLE {}.uri ADD CONSTRAINT uri_ns_key UNIQUE (ns);", phys.quotedName());
            conn->execute(sql);
            conn->set_current_schema(phys);
        }
        const std::string sql = fmt::format("COMMENT ON SCHEMA {} IS '{}:{}'", phys.quotedName(), app_id, expected_version);
        const auto schema_update = conn->execute(sql);
        if (!schema_update) {
            return false;
        }

        return true;
    }

using oes::testing::eml::OpenKVServerFixture;
using oes::testing::eml::EmlObjMock;

// traj_mock() without its interpretation, its CRS and datum, and its arrays at epc_ext_mock()
EmlObjMock putTraj20(OpenKVServerFixture& fx, const data12::Object::Dataspace& space) {
    EmlObjMock traj = oes::testing::eml::traj_mock(); // copy
    EmlObjMock::erase_tag("resqml:RepresentedInterpretation", traj.xml_);

    const Guid txUid = fx.startTx(space.m_uri, false);
    BOOST_REQUIRE(txUid.isValid());
    fx.putCrsObj(space);
    fx.putDatumObj(space);
    fx.putMockObj(space, traj);
    for (const auto& ary : traj.arrays_) {
        fx.putMockAry(space, traj, ary);
    }
    BOOST_REQUIRE(fx.commitTx(txUid));
    return traj;
}

// CopyDataspaceFromRemote without the network: dump on one connection, load on another.
// Returns the consumer's error, empty on success; \p tamper may alter each part in transit.
std::string copySpace(
    OpenKVServerFixture& fx, const Space& source, bool has_ref, const Space& target,
    const std::string& source_version, const std::string& target_version,
    const std::function<void(TableData&)>& tamper = nullptr, int project_to = 0
) {
    PGStreamer streamer(
        source, fx.connection_pool_->tryAcquireConnection("single"), has_ref, size_t(1) << 24, source_version, project_to
    );
    PGConsumer consumer(target, fx.connection_pool_->tryAcquireConnection("single"), target_version);

    SpaceStreamStatus status = SpaceStreamStatus::eOngoing;
    while (status == SpaceStreamStatus::eOngoing) {
        TableData td;
        status = streamer.stream(td);
        if (status == SpaceStreamStatus::eFailed) {
            return "the streamer failed at table " + td.m_tableName;
        }
        td.m_copyStatus = static_cast<uint8_t>(status);
        if (tamper) {
            tamper(td);
        }
        if (consumer.consume(td) == SpaceStreamStatus::eFailed) {
            return consumer.error();
        }
    }
    // As the handler does: a copy from an older source is upgraded like any space
    auto repo = std::make_shared<OpenKVRepo>(fx.session_, fx.connection_pool_, oes::testing::TestConfig::make());
    ErrorInfo err;
    if (!repo->getSpaceConnection(target, err)) {
        return "cannot upgrade the copy: " + err.m_message;
    }
    return consumer.finish() ? std::string() : consumer.error();
}

// copySpace() from a binary before the version exchange: no "schema" pseudo-table,
// whole tables in their physical column order, a part per row
std::string copySpaceFromLegacy(
    OpenKVServerFixture& fx, const Space& source, const Space& target, const std::string& target_version
) {
    const auto source_conn = fx.connection_pool_->tryAcquireConnection("single");
    PGconn* pg_conn = source_conn->pq_connection();
    const PhysicalName phys(source.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    PGConsumer consumer(target, fx.connection_pool_->tryAcquireConnection("single"), target_version);

    const std::vector<std::string> tables = { "uri", "xpa", "typ", "del_res", "res", "obj", "rel", "ary", "bin" };
    for (size_t i = 0; i < tables.size(); ++i) {
        const std::string cmd = fmt::format("COPY {}.{} TO STDOUT WITH (FORMAT BINARY)", phys.quotedName(), tables[i]);
        PGresult* res = PQexec(pg_conn, cmd.c_str());
        const bool copy_out = PQresultStatus(res) == PGRES_COPY_OUT;
        PQclear(res);
        BOOST_REQUIRE_MESSAGE(copy_out, cmd);

        std::string error;
        auto table_status = static_cast<uint8_t>(SpaceTableStreamStatus::eStart);
        char* buffer = nullptr;
        for (int len; (len = PQgetCopyData(pg_conn, &buffer, 0)) > 0;) {
            TableData td;
            td.m_tableName = tables[i];
            td.m_tableData.assign(buffer, buffer + len);
            PQfreemem(buffer);
            td.m_tableStatus = table_status | static_cast<uint8_t>(SpaceTableStreamStatus::eFinalChunk);
            td.m_copyStatus = static_cast<uint8_t>(SpaceStreamStatus::eOngoing);
            if (error.empty() && consumer.consume(td) == SpaceStreamStatus::eFailed) {
                error = consumer.error();
            }
            table_status = static_cast<uint8_t>(SpaceTableStreamStatus::eOngoing);
        }
        while (PGresult* end = PQgetResult(pg_conn)) {
            PQclear(end);
        }
        if (!error.empty()) {
            return error;
        }

        TableData done;
        done.m_tableName = tables[i];
        done.m_tableStatus = static_cast<uint8_t>(SpaceTableStreamStatus::eDone);
        done.m_copyStatus = static_cast<uint8_t>(
            i + 1 < tables.size() ? SpaceStreamStatus::eOngoing : SpaceStreamStatus::eDone
        );
        if (consumer.consume(done) == SpaceStreamStatus::eFailed) {
            return consumer.error();
        }
    }
    auto repo = std::make_shared<OpenKVRepo>(fx.session_, fx.connection_pool_, oes::testing::TestConfig::make());
    ErrorInfo err;
    if (!repo->getSpaceConnection(target, err)) {
        return "cannot upgrade the copy: " + err.m_message;
    }
    return consumer.finish() ? std::string() : consumer.error();
}

bool streamSpace(OpenKVServerFixture& fx, const Space& source, bool has_ref, const Space& target) {
    const auto conn = fx.admin_db_->handle();
    const std::string error = copySpace(
        fx, source, has_ref, target,
        spaceSchemaVersion(conn, source.dbfile_), spaceSchemaVersion(conn, target.dbfile_)
    );
    if (!error.empty()) {
        BOOST_TEST_MESSAGE("Streaming failed: " << error);
    }
    return error.empty();
}

int64_t rowCount(OpenKVServerFixture& fx, const Space& space, const std::string& table) {
    const PhysicalName phys(space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    return fx.admin_db_->handle()->executeInt64(fmt::format("SELECT count(*) FROM {}.{}", phys.quotedName(), table));
}

// Two spaces at the current version: a source holding a trajectory, and a new target
std::pair<Space, Space> streamSpaces(OpenKVServerFixture& fx, data12::Object::Dataspace& target) {
    const auto source = fx.putSpace("DataSpaceTest/StreamSource");
    putTraj20(fx, source);
    target = fx.putSpace("DataSpaceTest/StreamTarget");
    return { fx.admin_db_->findSpace(source.m_path), fx.admin_db_->findSpace(target.m_path) };
}

// Columns, constraints, indexes, triggers, views, functions and sequences of a space, one per line,
// with the schema name blanked; columns by name, not by position
std::set<std::string> catalogOf(OpenKVServerFixture& fx, const Space& space) {
    const PhysicalName phys(space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    const std::string text = fx.admin_db_->handle()->executeString(fmt::format(R"sql(
SELECT string_agg(line, E'\n') FROM (
    SELECT format('column %s.%s %s %s %s', table_name, column_name, data_type, is_nullable, column_default) AS line
      FROM information_schema.columns WHERE table_schema = {0}
    UNION ALL
    SELECT format('constraint %s %s %s', c.relname, con.conname, pg_get_constraintdef(con.oid))
      FROM pg_constraint con JOIN pg_class c ON c.oid = con.conrelid WHERE c.relnamespace = {0}::regnamespace
    UNION ALL
    SELECT format('index %s %s', indexname, indexdef) FROM pg_indexes WHERE schemaname = {0}
    UNION ALL
    SELECT format('trigger %s', pg_get_triggerdef(t.oid))
      FROM pg_trigger t JOIN pg_class c ON c.oid = t.tgrelid
     WHERE c.relnamespace = {0}::regnamespace AND NOT t.tgisinternal
    UNION ALL
    SELECT format('view %s %s', viewname, definition) FROM pg_views WHERE schemaname = {0}
    UNION ALL
    SELECT format('function %s %s', proname, prosrc) FROM pg_proc WHERE pronamespace = {0}::regnamespace
    UNION ALL
    SELECT format('sequence %s', sequencename) FROM pg_sequences WHERE schemaname = {0}
) AS catalog
)sql", phys.singleQuotedName()));

    std::set<std::string> lines;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);) {
        for (size_t pos = line.find(phys.sqlText()); pos != std::string::npos; pos = line.find(phys.sqlText(), pos)) {
            line.replace(pos, phys.sqlText().size(), "S");
        }
        lines.insert(line);
    }
    return lines;
}

std::string onlyIn(const std::set<std::string>& lhs, const std::set<std::string>& rhs) {
    std::string only;
    for (const auto& line : lhs) {
        if (rhs.count(line) == 0) {
            only += "\n  " + line;
        }
    }
    return only;
}

void checkStreamedTraj20(
    OpenKVServerFixture& fx, const data12::Object::Dataspace& target, const EmlObjMock& traj
) {
    BOOST_CHECK_EQUAL(4u, fx.objCount(target)); // CRS, datum, trajectory, external part

    const auto& hdf = oes::testing::eml::epc_ext_mock();
    const std::string ext_uri = EmlUri::make_obj(
        EmlUri::parse(target.m_uri).dataspace_, hdf.type_, hdf.guid_, EmlUri::eEML20
    ).to_string();

    auto get_rpc = fx.makeRpc(&OpenKVDataArray::GetDataArrays);
    for (const auto& mock : traj.arrays_) {
        DataArrayTypes::DataArrayIdentifier ident;
        ident.m_uri = ext_uri;
        ident.m_pathInResource = mock.path_;
        get_rpc.req_.m_dataArrays.emplace(mock.path_, ident);
    }
    if (!get_rpc.call() || fx.hasError(get_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, fx.errorOf(get_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(traj.arrays_.size(), get_rpc.res_.m_dataArrays.size());
    for (const auto& mock : traj.arrays_) {
        BOOST_CHECK(mock.equals(mock.path_, get_rpc.res_.m_dataArrays.at(mock.path_)));
    }
}

void migrateFrom15(OpenKVServerFixture& fx, const data12::Object::Dataspace& space) {
    const Space info = fx.admin_db_->findSpace(space.m_path);
    const auto conn = fx.admin_db_->handle();
    BOOST_REQUIRE(downgradeSpaceSchemaVersion(conn, info.dbfile_, 15));

    auto get_rpc = fx.makeRpc(&OpenKVDiscovery::GetResources);
    get_rpc.req_.m_context.m_uri = space.m_uri;
    get_rpc.req_.m_context.m_depth = 1;
    get_rpc.call();
    BOOST_REQUIRE_EQUAL(oes::eml::openkv::OpenKVRepo::getSchemaVersion(conn, info.dbfile_), latestSpaceVersion());

    // 16 appends the column, a new space has it fourth
    const PhysicalName phys(info.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    const std::string columns = conn->executeString(fmt::format(
        "SELECT string_agg(attname, ',' ORDER BY attnum) FROM pg_attribute"
        " WHERE attrelid = '{}.ary'::regclass AND attnum > 0 AND NOT attisdropped",
        phys.quotedName()
    ));
    BOOST_REQUIRE_MESSAGE(boost::ends_with(columns, ",epc_guid"), columns);
}

BOOST_FIXTURE_TEST_SUITE(SpaceProtoTests, oes::testing::eml::OpenKVServerFixture)

// Test that downgrade the DB schema, and check that it is propertly upgraded when trying to access it
// This test will be used to test the upgrade path from schema version 8 to current version
BOOST_AUTO_TEST_CASE(test_schema_upgrade) {

    auto space = putSpace("DataSpaceTest/UpgradeSchema");

    Space space3 = admin_db_->findSpace(space.m_path);
    int first_version = oes::eml::openkv::OpenKVRepo::getSchemaVersion(admin_db_->handle(), space3.dbfile_);
    BOOST_CHECK_EQUAL(first_version,latestSpaceVersion());

    // Downgrade the schema version to 8
    downgradeSpaceSchemaVersion(admin_db_->handle(), space3.dbfile_, 9);
    int downgraded_version = oes::eml::openkv::OpenKVRepo::getSchemaVersion(admin_db_->handle(), space3.dbfile_);
    BOOST_CHECK_EQUAL(downgraded_version,9);

    // Accessing the resources will check dataspace schema version and upgrade it to version 13
    auto get_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    get_rpc.req_.m_context.m_uri = space.m_uri;
    get_rpc.req_.m_context.m_depth = 1;
    get_rpc.call();

    // Check that space schema version is up to date
    int schema_version = oes::eml::openkv::OpenKVRepo::getSchemaVersion(admin_db_->handle(), space3.dbfile_);
    BOOST_CHECK_EQUAL(schema_version,latestSpaceVersion());

    const PhysicalName phys(space3.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    const std::string triggers = admin_db_->handle()->executeString(fmt::format(
        "SELECT string_agg(pg_get_triggerdef(oid), E'\\n' ORDER BY tgname) FROM pg_trigger"
        " WHERE tgrelid = '{}.res'::regclass AND NOT tgisinternal",
        phys.quotedName()
    ));
    BOOST_CHECK_MESSAGE(
        triggers.find("res_space_rel_del_trigger AFTER DELETE ON") != std::string::npos &&
        triggers.find("REFERENCING OLD TABLE AS old_rows FOR EACH STATEMENT") != std::string::npos &&
        triggers.find("res_space_rel_trigger AFTER UPDATE OF space_rel_id ON") != std::string::npos,
        triggers
    );
    const std::string indexes = admin_db_->handle()->executeString(fmt::format(
        "SELECT string_agg(indexname, ',' ORDER BY indexname) FROM pg_indexes WHERE schemaname = {}"
        " AND indexname IN ('ary_obj_id_idx', 'bin_pkey', 'res_null_guid_idx')",
        phys.singleQuotedName()
    ));
    BOOST_CHECK_EQUAL(indexes, "ary_obj_id_idx,bin_pkey,res_null_guid_idx");

    clearDataSpace(space3.dbfile_);
}

// PutDataspaces labels a clone with the latest version: a source nobody upgraded is upgraded first
BOOST_AUTO_TEST_CASE(test_clone_upgrades_source) {
    const auto source = putSpace("DataSpaceTest/CloneSource");
    const Space source_space = admin_db_->findSpace(source.m_path);
    const auto conn = admin_db_->handle();
    BOOST_REQUIRE(downgradeSpaceSchemaVersion(conn, source_space.dbfile_, 15));

    data12::Object::Dataspace clone;
    clone.m_path = "DataSpaceTest/Clone";
    clone.m_uri = EmlUri::make_space(clone.m_path).to_string();
    xdataEmplace(clone.m_customData, "fromDataspace", source.m_uri);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);
    put_rpc.req_.m_dataspaces.emplace(clone.m_path, clone);
    spaces_.push_back(clone.m_path);
    if (!put_rpc.call() || hasError(put_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_rpc.rc_));
    }

    const Space clone_space = admin_db_->findSpace(clone.m_path);
    const PhysicalName clone_phys(clone_space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, source_space.dbfile_), latestSpaceVersion());
    BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, clone_space.dbfile_), latestSpaceVersion());
    BOOST_CHECK_EQUAL(conn->executeInt64(fmt::format(
        "SELECT count(*) FROM information_schema.columns"
        " WHERE table_schema = {} AND table_name = 'ary' AND column_name = 'epc_guid'",
        clone_phys.singleQuotedName()
    )), 1);
    BOOST_CHECK_EQUAL(conn->executeInt64(fmt::format(
        "SELECT count(*) FROM pg_trigger"
        " WHERE tgrelid = '{}.res'::regclass AND tgname = 'res_space_rel_del_trigger'",
        clone_phys.quotedName()
    )), 1);
}

// Version 16 left a nil container on arrays of RESQML 2.2 objects whose <URI> names a file
BOOST_AUTO_TEST_CASE(test_schema_upgrade_17_resqml22_containers) {
    const auto space = putSpace("DataSpaceTest/UpgradeSchema17");
    const Space space3 = admin_db_->findSpace(space.m_path);
    const PhysicalName phys(space3.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    const auto conn = admin_db_->handle();

    const auto repaired = oes::testing::eml::prop22_mock(Guid::make());
    const auto clashing = oes::testing::eml::prop22_mock(Guid::make());

    const Guid txUid = startTx(space.m_uri, false);
    putGrid2dAry(space); // RESQML 2.0: its container is the external part, not its owner
    putCrsObj(space);
    putGrid2dObj(space);
    for (const auto* mock : { &repaired, &clashing }) {
        putMockObj(space, *mock);
        putResqml22Ary(space, *mock);
    }
    BOOST_REQUIRE(commitTx(txUid));

    const std::string nil = "00000000-0000-0000-0000-000000000000";
    conn->execute(fmt::format("UPDATE {}.ary SET epc_guid = '{}'", phys.quotedName(), nil));

    // Since put again under its container, as a client may have done after 16
    conn->execute(fmt::format(R"sql(
INSERT INTO {0}.ary (epc_guid, path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length)
SELECT '{1}', path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length
  FROM {0}.ary WHERE path = '{2}'
)sql", phys.quotedName(), clashing.guid_.toXmlString(), clashing.arrays_.front().path_.substr(1)));

    BOOST_REQUIRE(downgradeSpaceSchemaVersion(conn, space3.dbfile_, 16));

    auto get_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    get_rpc.req_.m_context.m_uri = space.m_uri;
    get_rpc.req_.m_context.m_depth = 1;
    get_rpc.call();
    BOOST_CHECK_EQUAL(oes::eml::openkv::OpenKVRepo::getSchemaVersion(conn, space3.dbfile_), latestSpaceVersion());

    auto count_of = [&](const std::string& guid) {
        return conn->executeInt64(fmt::format(
            "SELECT count(*) FROM {}.ary WHERE epc_guid = '{}'", phys.quotedName(), guid
        ));
    };
    BOOST_CHECK_EQUAL(count_of(repaired.guid_.toXmlString()), 1);
    BOOST_CHECK_EQUAL(count_of(clashing.guid_.toXmlString()), 1);
    BOOST_CHECK_EQUAL(count_of(nil), 2); // the grid2d array and the stale clashing row

    clearDataSpace(space3.dbfile_);
}

BOOST_AUTO_TEST_CASE(test_stream_space_migrated_from_15) {
    const auto source = putSpace("DataSpaceTest/StreamSource");
    const EmlObjMock traj = putTraj20(*this, source);
    migrateFrom15(*this, source);

    const Space source_space = admin_db_->findSpace(source.m_path);
    const auto conn = admin_db_->handle();
    const PhysicalName source_phys(source_space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);

    // As a future migration would: a column of another table moves last (usize is NULL here)
    conn->execute(fmt::format(
        "ALTER TABLE {0}.obj DROP COLUMN usize; ALTER TABLE {0}.obj ADD COLUMN usize INT;",
        source_phys.quotedName()
    ));

    const auto target = putSpace("DataSpaceTest/StreamTarget");
    const Space target_space = admin_db_->findSpace(target.m_path);
    BOOST_REQUIRE(!admin_db_->spaceIncludesReference(source_space));
    BOOST_REQUIRE(streamSpace(*this, source_space, false, target_space));

    const PhysicalName target_phys(target_space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    for (const char* table : { "uri", "xpa", "typ", "del_res", "res", "obj", "rel", "ary", "bin" }) {
        BOOST_TEST_CONTEXT(table) {
            const std::string count = "SELECT count(*) FROM {}.{}";
            BOOST_CHECK_EQUAL(
                conn->executeInt64(fmt::format(count, target_phys.quotedName(), table)),
                conn->executeInt64(fmt::format(count, source_phys.quotedName(), table))
            );
        }
    }
    checkStreamedTraj20(*this, target, traj);
}

BOOST_AUTO_TEST_CASE(test_stream_space_with_references) {
    const auto origin = putSpace("DataSpaceTest/StreamOrigin");
    const EmlObjMock traj = putTraj20(*this, origin);
    migrateFrom15(*this, origin);

    auto lock_rpc = makeRpc(&OpenKVDataspaceOSDU::LockDataspaces);
    lock_rpc.req_.m_uris.emplace(origin.m_path, origin.m_uri);
    lock_rpc.req_.m_lock = true;
    BOOST_REQUIRE(lock_rpc.call());

    const auto child = putSpace("DataSpaceTest/StreamChild");
    auto copy_rpc = makeRpc(&OpenKVDataspaceOSDU::CopyDataspacesContent);
    copy_rpc.req_.m_dataspaces.emplace(origin.m_path, origin.m_uri);
    copy_rpc.req_.m_targetDataspace = child.m_uri;
    BOOST_REQUIRE(copy_rpc.call());
    BOOST_REQUIRE_EQUAL(copy_rpc.res_.m_success.size(), 1u);

    const auto target = putSpace("DataSpaceTest/StreamTarget");
    const Space child_space = admin_db_->findSpace(child.m_path);
    const Space target_space = admin_db_->findSpace(target.m_path);
    BOOST_REQUIRE(admin_db_->spaceIncludesReference(child_space));
    BOOST_REQUIRE(streamSpace(*this, child_space, true, target_space));

    checkStreamedTraj20(*this, target, traj);
}

// COPY keeps the source ids: the target's sequences must be reset past them
BOOST_AUTO_TEST_CASE(test_stream_space_sequences) {
    data12::Object::Dataspace target;
    const auto [source_space, target_space] = streamSpaces(*this, target);
    BOOST_REQUIRE(streamSpace(*this, source_space, false, target_space));

    const auto prop = oes::testing::eml::prop22_mock(Guid::make());
    putMockObj(target, prop);
    putResqml22Ary(target, prop);
    BOOST_CHECK_EQUAL(5u, objCount(target));
    BOOST_CHECK_EQUAL(rowCount(*this, target_space, "ary"), rowCount(*this, source_space, "ary") + 1);
}

BOOST_AUTO_TEST_CASE(test_stream_space_version_mismatch) {
    data12::Object::Dataspace target;
    const auto [source_space, target_space] = streamSpaces(*this, target);
    const std::string version = spaceSchemaVersion(admin_db_->handle(), source_space.dbfile_);
    const std::string older = version.substr(0, version.find(':') + 1) + "16";

    const std::string error = copySpace(*this, source_space, false, target_space, version, older);
    const std::string expected = fmt::format("at schema {}, the target at 16", latestSpaceVersion());
    BOOST_CHECK_MESSAGE(error.find(expected) != std::string::npos, error);
    BOOST_CHECK_EQUAL(rowCount(*this, target_space, "res"), 0);
}

// A source before the version exchange is taken as schema 15: a newer binary without it is refused
BOOST_AUTO_TEST_CASE(test_stream_space_without_schema_version) {
    data12::Object::Dataspace target;
    const auto [source_space, target_space] = streamSpaces(*this, target);
    const std::string version = spaceSchemaVersion(admin_db_->handle(), target_space.dbfile_);

    const std::string error = copySpaceFromLegacy(*this, source_space, target_space, version);
    BOOST_CHECK_MESSAGE(
        error.find("Table ary: the source sends 15 fields per row, the target has 14 columns") != std::string::npos, error
    );
}

// A 15 binary sends its space, upgraded to 15 when opened, as it lies on disk
BOOST_AUTO_TEST_CASE(test_stream_space_from_legacy_source) {
    const auto source = putSpace("DataSpaceTest/StreamSource");
    const EmlObjMock traj = putTraj20(*this, source);
    const Space source_space = admin_db_->findSpace(source.m_path);
    const auto conn = admin_db_->handle();
    BOOST_REQUIRE(downgradeSpaceSchemaVersion(conn, source_space.dbfile_, 15));
    const auto target = putSpace("DataSpaceTest/StreamTarget");
    const Space target_space = admin_db_->findSpace(target.m_path);

    const std::string error = copySpaceFromLegacy(
        *this, source_space, target_space, spaceSchemaVersion(conn, target_space.dbfile_)
    );
    BOOST_REQUIRE_MESSAGE(error.empty(), error);
    BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, target_space.dbfile_), latestSpaceVersion());
    BOOST_CHECK_EQUAL(rowCount(*this, target_space, "ary"), rowCount(*this, source_space, "ary"));
    checkStreamedTraj20(*this, target, traj);
}

BOOST_AUTO_TEST_CASE(test_stream_space_column_mismatch) {
    data12::Object::Dataspace target;
    const auto [source_space, target_space] = streamSpaces(*this, target);
    const auto conn = admin_db_->handle();
    const PhysicalName target_phys(target_space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    conn->execute(fmt::format("ALTER TABLE {}.ary ADD COLUMN zz INT", target_phys.quotedName()));
    const std::string version = spaceSchemaVersion(conn, source_space.dbfile_);

    const std::string error = copySpace(*this, source_space, false, target_space, version, version);
    BOOST_CHECK_MESSAGE(error.find("Table ary: the source sends the columns") != std::string::npos, error);
    BOOST_CHECK_MESSAGE(error.find(", zz)") != std::string::npos, error);
}

// The field count of the first tuple is checked even when the column lists agree
BOOST_AUTO_TEST_CASE(test_stream_space_field_count) {
    data12::Object::Dataspace target;
    const auto [source_space, target_space] = streamSpaces(*this, target);
    const auto conn = admin_db_->handle();
    const PhysicalName target_phys(target_space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    conn->execute(fmt::format("ALTER TABLE {}.ary ADD COLUMN zz INT", target_phys.quotedName()));
    const std::string target_columns = conn->executeString(fmt::format(
        "SELECT string_agg(quote_ident(attname), ', ' ORDER BY attname) FROM pg_attribute"
        " WHERE attrelid = '{}.ary'::regclass AND attnum > 0 AND NOT attisdropped",
        target_phys.quotedName()
    ));
    const std::string version = spaceSchemaVersion(conn, source_space.dbfile_);

    // A source announcing the target's columns but sending its own rows
    const auto announce_target_columns = [&](TableData& td) {
        if (td.m_tableName != "schema") {
            return;
        }
        std::string text(td.m_tableData.begin(), td.m_tableData.end());
        const size_t begin = text.find("\nary:") + 5;
        text.replace(begin, text.find('\n', begin) - begin, target_columns);
        td.m_tableData.assign(text.begin(), text.end());
    };
    const std::string error = copySpace(*this, source_space, false, target_space, version, version, announce_target_columns);
    BOOST_CHECK_MESSAGE(error.find("Table ary: the source sends 15 fields per row, the target has 16 columns") != std::string::npos, error);
}

// Table names come from the source and end up in SQL: only the space tables are accepted
BOOST_AUTO_TEST_CASE(test_stream_space_unexpected_table) {
    data12::Object::Dataspace target;
    const auto [source_space, target_space] = streamSpaces(*this, target);
    const std::string version = spaceSchemaVersion(admin_db_->handle(), source_space.dbfile_);

    const auto rename_uri = [](TableData& td) {
        if (td.m_tableName == "uri") {
            td.m_tableName = "admin.spaces";
        }
    };
    const std::string error = copySpace(*this, source_space, false, target_space, version, version, rename_uri);
    BOOST_CHECK_MESSAGE(error.find("Unexpected table in the stream: admin.spaces") != std::string::npos, error);
    BOOST_CHECK_EQUAL(rowCount(*this, target_space, "res"), 0);
}

// A new space taken down to N, then upgraded, must look like a new space again
BOOST_AUTO_TEST_CASE(test_downgrade_empty_space_round_trip) {
    const auto conn = admin_db_->handle();
    const Space fresh = admin_db_->findSpace(putSpace("DataSpaceTest/Fresh").m_path);
    const auto expected = catalogOf(*this, fresh);
    BOOST_REQUIRE_GT(expected.size(), 100u);
    const int latest = OpenKVRepo::getSchemaVersion(conn, fresh.dbfile_);

    for (int version = 15; version < latest; ++version) {
        BOOST_TEST_CONTEXT("schema " << version) {
            const auto space = putSpace(fmt::format("DataSpaceTest/RoundTrip{}", version));
            const Space info = admin_db_->findSpace(space.m_path);
            downgrade_empty_space(conn, info.dbfile_, latest, version);
            BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, info.dbfile_), version);
            if (version < 16) {
                BOOST_CHECK(catalogOf(*this, info) != expected); // ary.epc_guid is gone
            }

            auto get_rpc = makeRpc(&OpenKVDiscovery::GetResources);
            get_rpc.req_.m_context.m_uri = space.m_uri;
            get_rpc.req_.m_context.m_depth = 1;
            get_rpc.call();
            BOOST_REQUIRE_EQUAL(OpenKVRepo::getSchemaVersion(conn, info.dbfile_), latest);

            const auto round_trip = catalogOf(*this, info);
            const std::string only_after = onlyIn(round_trip, expected);
            const std::string only_new = onlyIn(expected, round_trip);
            BOOST_CHECK_MESSAGE(only_after.empty(), "Only after the round trip:" << only_after);
            BOOST_CHECK_MESSAGE(only_new.empty(), "Only in a new space:" << only_new);
        }
    }
}

BOOST_AUTO_TEST_CASE(test_downgrade_empty_space_refusals) {
    const auto conn = admin_db_->handle();
    const auto space = putSpace("DataSpaceTest/NotEmpty");
    const Space info = admin_db_->findSpace(space.m_path);
    const int latest = OpenKVRepo::getSchemaVersion(conn, info.dbfile_);

    BOOST_CHECK_THROW(downgrade_empty_space(conn, info.dbfile_, latest, latest + 1), std::runtime_error);
    putCrsObj(space);
    BOOST_CHECK_THROW(downgrade_empty_space(conn, info.dbfile_, latest, 15), std::runtime_error);
    BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, info.dbfile_), latest);
}

// A source at 15 (a space nobody upgraded, as an older binary sends it): the target is taken down
// to 15, the rows copied, then 16 fills the containers from the XML as for any space it upgrades
BOOST_AUTO_TEST_CASE(test_stream_space_from_older_source) {
    data12::Object::Dataspace target;
    const auto source = putSpace("DataSpaceTest/StreamSource");
    const EmlObjMock traj = putTraj20(*this, source);
    const Space source_space = admin_db_->findSpace(source.m_path);
    const auto conn = admin_db_->handle();
    BOOST_REQUIRE(downgradeSpaceSchemaVersion(conn, source_space.dbfile_, 15));
    target = putSpace("DataSpaceTest/StreamTarget");
    const Space target_space = admin_db_->findSpace(target.m_path);

    const std::string error = copySpace(
        *this, source_space, false, target_space,
        spaceSchemaVersion(conn, source_space.dbfile_), spaceSchemaVersion(conn, target_space.dbfile_)
    );
    BOOST_REQUIRE_MESSAGE(error.empty(), error);
    BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, target_space.dbfile_), latestSpaceVersion());
    BOOST_CHECK_EQUAL(rowCount(*this, target_space, "ary"), rowCount(*this, source_space, "ary"));
    checkStreamedTraj20(*this, target, traj);
}

BOOST_AUTO_TEST_CASE(test_stream_space_from_older_source_refusals) {
    data12::Object::Dataspace target;
    const auto [source_space, target_space] = streamSpaces(*this, target);
    const auto conn = admin_db_->handle();
    const std::string version = spaceSchemaVersion(conn, target_space.dbfile_);
    const std::string magic = version.substr(0, version.find(':') + 1);

    putCrsObj(target);
    const std::string error = copySpace(*this, source_space, false, target_space, magic + "15", version);
    BOOST_CHECK_MESSAGE(error.find("holding resources") != std::string::npos, error);
    BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, target_space.dbfile_), latestSpaceVersion());
}

// A target with the exchange, at an older version: the source sends its space in that layout
BOOST_AUTO_TEST_CASE(test_stream_space_to_older_target) {
    const auto source = putSpace("DataSpaceTest/StreamSource");
    const EmlObjMock traj = putTraj20(*this, source);
    const Space source_space = admin_db_->findSpace(source.m_path);
    const auto conn = admin_db_->handle();
    const std::string source_version = spaceSchemaVersion(conn, source_space.dbfile_);
    const std::string magic = source_version.substr(0, source_version.find(':') + 1);

    const int latest = latestSpaceVersion();
    for (int version = 15; version < latest; ++version) {
        BOOST_TEST_CONTEXT("schema " << version) {
            const auto target = putSpace(fmt::format("DataSpaceTest/StreamTarget{}", version));
            const Space target_space = admin_db_->findSpace(target.m_path);
            downgrade_empty_space(conn, target_space.dbfile_, latest, version);
            check_space_projection(conn, source_space.dbfile_, latest, version);

            const std::string target_version = magic + std::to_string(version);
            const std::string error = copySpace(
                *this, source_space, false, target_space, target_version, target_version, nullptr, version
            );
            BOOST_REQUIRE_MESSAGE(error.empty(), error);
            BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, target_space.dbfile_), latest);
            BOOST_CHECK_EQUAL(rowCount(*this, target_space, "ary"), rowCount(*this, source_space, "ary"));
            checkStreamedTraj20(*this, target, traj);
        }
    }
}

// A 15 binary as target: no "schema" part, the 15 columns in physical order, here with references
BOOST_AUTO_TEST_CASE(test_stream_space_to_legacy_target) {
    const auto origin = putSpace("DataSpaceTest/StreamOrigin");
    const EmlObjMock traj = putTraj20(*this, origin);

    auto lock_rpc = makeRpc(&OpenKVDataspaceOSDU::LockDataspaces);
    lock_rpc.req_.m_uris.emplace(origin.m_path, origin.m_uri);
    lock_rpc.req_.m_lock = true;
    BOOST_REQUIRE(lock_rpc.call());

    const auto child = putSpace("DataSpaceTest/StreamChild");
    auto copy_rpc = makeRpc(&OpenKVDataspaceOSDU::CopyDataspacesContent);
    copy_rpc.req_.m_dataspaces.emplace(origin.m_path, origin.m_uri);
    copy_rpc.req_.m_targetDataspace = child.m_uri;
    BOOST_REQUIRE(copy_rpc.call());
    BOOST_REQUIRE_EQUAL(copy_rpc.res_.m_success.size(), 1u);

    const auto conn = admin_db_->handle();
    const auto target = putSpace("DataSpaceTest/StreamTarget");
    const Space child_space = admin_db_->findSpace(child.m_path);
    const Space target_space = admin_db_->findSpace(target.m_path);
    BOOST_REQUIRE(admin_db_->spaceIncludesReference(child_space));
    BOOST_REQUIRE(downgradeSpaceSchemaVersion(conn, target_space.dbfile_, 15));
    const std::string target_version = spaceSchemaVersion(conn, target_space.dbfile_);

    const std::string error = copySpace(*this, child_space, true, target_space, "", target_version, nullptr, 15);
    BOOST_REQUIRE_MESSAGE(error.empty(), error);
    BOOST_CHECK_EQUAL(OpenKVRepo::getSchemaVersion(conn, target_space.dbfile_), latestSpaceVersion());
    checkStreamedTraj20(*this, target, traj);
}

// Targets before the exchange overwrite their xdata with the "admin" part instead of merging it
BOOST_AUTO_TEST_CASE(test_stream_space_xdata_to_legacy_target) {
    data12::Object::Dataspace target;
    auto [source_space, target_space] = streamSpaces(*this, target);
    source_space.xdata_json_ = R"({"origin":"source"})";
    const std::string version = spaceSchemaVersion(admin_db_->handle(), source_space.dbfile_);

    for (const std::string& announced : { version, std::string() }) {
        BOOST_TEST_CONTEXT("announced '" << announced << "'") {
            PGStreamer streamer(
                source_space, connection_pool_->tryAcquireConnection("single"), false, size_t(1) << 24, announced
            );
            int admin_parts = 0;
            SpaceStreamStatus status = SpaceStreamStatus::eOngoing;
            while (status == SpaceStreamStatus::eOngoing) {
                TableData td;
                status = streamer.stream(td);
                admin_parts += td.m_tableName == "admin" ? 1 : 0;
            }
            BOOST_CHECK(status == SpaceStreamStatus::eDone);
            BOOST_CHECK_EQUAL(admin_parts, announced.empty() ? 0 : 1);
        }
    }
}

// Two containers holding the same path cannot go to 15, which keys arrays by path
BOOST_AUTO_TEST_CASE(test_space_projection_refusal) {
    const auto source = putSpace("DataSpaceTest/StreamSource");
    putTraj20(*this, source);
    const Space source_space = admin_db_->findSpace(source.m_path);
    const auto conn = admin_db_->handle();
    const PhysicalName phys(source_space.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    conn->execute(fmt::format(R"sql(
INSERT INTO {0}.ary (epc_guid, path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length)
SELECT gen_random_uuid(), path, type, rank1, dim1, dim2, dim3, dim4, usize, zcomp, value, chunk_length
  FROM {0}.ary LIMIT 1
)sql", phys.quotedName()));

    BOOST_CHECK_NO_THROW(check_space_projection(conn, source_space.dbfile_, latestSpaceVersion(), 16));
    try {
        check_space_projection(conn, source_space.dbfile_, latestSpaceVersion(), 15);
        BOOST_ERROR("projected to 15");
    } catch (const std::runtime_error& e) {
        BOOST_CHECK_MESSAGE(std::string(e.what()).find("is in several containers") != std::string::npos, e.what());
    }
    BOOST_CHECK_THROW(check_space_projection(conn, source_space.dbfile_, 15, 16), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_Dataspace24_PutDataspaces3) {
    auto get_rpc = makeRpc(&OpenKVDataspace::GetDataspaces);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);

    // We start with no dataspaces
    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE(get_rpc.res_.m_dataspaces.empty());

    // 1) Request should contain at least 1 Dataspace
    BOOST_REQUIRE(!put_rpc.call());
    BOOST_REQUIRE(put_rpc.rc_.ex.m_error);
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_error->m_code, EINVALID_ARGUMENT);
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_error->m_message, "Nothing to add");
    BOOST_CHECK(put_rpc.rc_.ex.m_errors.empty()); // no "array-errors"

    // 2a) Add single Dataspace, with "Guid URI" (OpenKV extension)
    auto space_it = put_rpc.req_.m_dataspaces.emplace(
        "GOM/Block1", data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_it.second);
    data12::Object::Dataspace* space = &space_it.first->second;
    const std::string uid = Guid::make().toXmlString();
    space->m_uri = EmlUri::make_space(uid).to_string();
    space->m_path = space_it.first->first;
    xdataEmplace(space->m_customData, "key", "value");
    space->m_customData["locked"].m_item = false;

    int64_t before = curr_usec();
    BOOST_REQUIRE(put_rpc.call());
    int64_t after = curr_usec();
    spaces_.push_back(space->m_path);

    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE_EQUAL(get_rpc.res_.m_dataspaces.size(), 1);

    data12::Object::Dataspace space2a = get_rpc.res_.m_dataspaces[0];
    BOOST_CHECK_EQUAL(space->m_uri, space2a.m_uri);
    BOOST_CHECK_EQUAL(space->m_path, space2a.m_path);
    BOOST_REQUIRE(space2a.m_storeLastWrite);
    {
#ifdef WIN32
        // Expand allowed interval for StoreLastWrite time. Fixes test stability on some environments (the reason is unclear, probably due time differences in threads)
        before -= std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::seconds(10)).count();
        after += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::seconds(10)).count();
#endif
        const int64_t lastChanged = space2a.m_storeLastWrite;
        BOOST_CHECK_LE(before, lastChanged);
        BOOST_CHECK_LE(lastChanged, after);
    }
    // Remove "size" from space2a custom data, because it is set by OpenKV
    space2a.m_customData.erase("size");
    space2a.m_customData.erase("read-only");
    BOOST_CHECK(space->m_customData == space2a.m_customData);

    // 2b) Add single Dataspace, with "PATH URI" (Normal ETP behavior)
    put_rpc.req_.m_dataspaces.clear();
    space_it = put_rpc.req_.m_dataspaces.emplace(
        "GOM/Block2", data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_it.second);
    space = &space_it.first->second;
    space->m_path = space_it.first->first;
    space->m_uri = EmlUri::make_space(space->m_path).to_string(); // same as PATH

    BOOST_REQUIRE(put_rpc.call());

    spaces_.push_back(space->m_path);

    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE_EQUAL(get_rpc.res_.m_dataspaces.size(), 2);

    // OpenKV returns them path-sorted, but just in case
    if (!(get_rpc.res_.m_dataspaces[0].m_path < get_rpc.res_.m_dataspaces[1].m_path)) {
        std::swap(get_rpc.res_.m_dataspaces[0], get_rpc.res_.m_dataspaces[1]);
    }
    const data12::Object::Dataspace space2b = get_rpc.res_.m_dataspaces[1];
    BOOST_CHECK_EQUAL(space->m_uri, space2b.m_uri); // NOT a GUID URI
    BOOST_CHECK_EQUAL(space->m_path, space2b.m_path);
    BOOST_CHECK(space2b.m_storeLastWrite);

    // 3) Add Dataspace with a Path conflict: KO
    space->m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();

    BOOST_REQUIRE(!put_rpc.call());

    // We do not get the "scalar" error, but a "singular array-error" instead.
    BOOST_CHECK(!put_rpc.rc_.ex.m_error);
    BOOST_REQUIRE_EQUAL(put_rpc.rc_.ex.m_errors.size(), size_t(1));
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->first, space_it.first->first);
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->second.m_code, EINVALID_ARGUMENT);
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->second.m_message, "Space already exists: PATH conflict with GOM/Block2");

    // 4) Add Dataspace with a Guid conflict: KO
    put_rpc.req_.m_dataspaces.clear();
    space_it = put_rpc.req_.m_dataspaces.emplace(
        "GOM/Block3", data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_it.second);
    space = &space_it.first->second;
    space->m_uri = space2a.m_uri; // same GUID as existing ETP dataspace
    space->m_path = space_it.first->first;

    BOOST_REQUIRE(!put_rpc.call());

    BOOST_CHECK(!put_rpc.rc_.ex.m_error);
    BOOST_REQUIRE_EQUAL(put_rpc.rc_.ex.m_errors.size(), size_t(1));
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->first, space_it.first->first);
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->second.m_code, EINVALID_ARGUMENT);
    BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->second.m_message, "Space already exists: URI conflict with " + uid);

    // 5) Add Dataspace with invalid Path: KO
    auto try_invalid_path = [&](std::string bad_path, std::string expected_errmsg) {
        put_rpc.req_.m_dataspaces.clear();
        space_it = put_rpc.req_.m_dataspaces.emplace(
            bad_path, data12::Object::Dataspace{}
        );
        BOOST_CHECK(space_it.second);
        space = &space_it.first->second;
        space->m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();
        space->m_path = space_it.first->first;

        const bool put_rpc_ok = put_rpc.call();
        BOOST_REQUIRE(!put_rpc_ok);

        BOOST_CHECK(!put_rpc.rc_.ex.m_error);
        BOOST_REQUIRE_EQUAL(put_rpc.rc_.ex.m_errors.size(), size_t(1));
        BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->first, space_it.first->first);
        BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->second.m_code, EINVALID_ARGUMENT);
        BOOST_CHECK_EQUAL(put_rpc.rc_.ex.m_errors.begin()->second.m_message, fmt::format(
            "Invalid space path: '{}': {}", bad_path, expected_errmsg
        ));
    };
    try_invalid_path("", "Empty");
    try_invalid_path("GOM_Block3/", "Cannot end with forward-slash");
    try_invalid_path("GOM/Block3/", "Cannot contain more than one forward-slash");
    try_invalid_path("GOM\\Block3", "Cannot contain back-slashes");

    // 6) Add several (> 1) Dataspaces

    // 7) Check Partial Success: some Dataspace added, others not

    // GMT: Sunday, September 13, 2020 12:26:40 PM
    // 1600000000

    // 8) Add Dataspace with last-modified date *in the future*

    // GMT: Wednesday, May 18, 2033 3:33:20 AM
    // 2000000000
}

BOOST_AUTO_TEST_CASE(test_Dataspace24_PutDataspaces3_xdata) {
    auto get_rpc = makeRpc(&OpenKVDataspace::GetDataspaces);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);

    // We start with no dataspaces
    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE(get_rpc.res_.m_dataspaces.empty());

    auto space_it = put_rpc.req_.m_dataspaces.emplace(
        "GOM/Block1", data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_it.second);
    data12::Object::Dataspace* space = &space_it.first->second;
    space->m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();
    space->m_path = space_it.first->first;
    space->m_customData["locked"].m_item = false;

    XDataMapBuilder xmb{space->m_customData};
    xmb.emplace("text", "String Value");
    xmb.emplace("bool", true);
    xmb.emplace("real", 1.23);
    xmb.emplace("long", int64_t(1) << 45);
    xmb.emplace("float", NAN);
    xmb.emplace("roles", std::vector<std::string>{ "admin", "data-mgr", "user" });
    xmb.emplace("bbox", std::vector<double>{ 1.1, 1.2, 9.9, 10.1 });

    int64_t before = curr_usec();
    BOOST_REQUIRE(put_rpc.call());
    int64_t after = curr_usec();

    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE_EQUAL(get_rpc.res_.m_dataspaces.size(), 1);

    spaces_.push_back(space->m_path);

    data12::Object::Dataspace& space2a = get_rpc.res_.m_dataspaces[0];
    BOOST_CHECK_EQUAL(space->m_uri, space2a.m_uri);
    BOOST_CHECK_EQUAL(space->m_path, space2a.m_path);
    BOOST_REQUIRE(space2a.m_storeLastWrite);
    {
#ifdef WIN32
        // Expand allowed interval for StoreLastWrite time. Fixes test stability on some environments (the reason is unclear, probably due time differences in threads)
        before -= std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::seconds(10)).count();
        after += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::seconds(10)).count();
#endif
        const int64_t lastChanged = space2a.m_storeLastWrite;
        BOOST_CHECK_LE(before, lastChanged);
        BOOST_CHECK_LE(lastChanged, after);
    }

    space2a.m_customData.erase("size");
    space2a.m_customData.erase("read-only");
    BOOST_CHECK(space->m_customData == space2a.m_customData);
}

BOOST_AUTO_TEST_CASE(test_Dataspace24_GetDataspaces1_since) {
    auto get_rpc = makeRpc(&OpenKVDataspace::GetDataspaces);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);

    // We start with no dataspaces
    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE(get_rpc.res_.m_dataspaces.empty());

    // Use two separate Puts, to maximize changes of separate last-modified stamps
    data12::Object::Dataspace space1;
    space1.m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();
    space1.m_path = "GOM/Block1";
    put_rpc.req_.m_dataspaces.emplace(space1.m_path, space1);
    BOOST_REQUIRE(put_rpc.call());

    spaces_.push_back(space1.m_path);

    BOOST_CHECK_EQUAL(1, spaceCount());

    put_rpc.req_.m_dataspaces.clear();

    data12::Object::Dataspace space2, space3;
    space2.m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();
    space3.m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();
    space2.m_path = "GOM/Block2";
    space3.m_path = "GOM/Block3";
    put_rpc.req_.m_dataspaces.emplace(space2.m_path, space2);
    put_rpc.req_.m_dataspaces.emplace(space3.m_path, space3);
    BOOST_REQUIRE(put_rpc.call());
    spaces_.push_back(space2.m_path);
    spaces_.push_back(space3.m_path);

    BOOST_CHECK_EQUAL(3, spaceCount());

    auto sort_returned_spaces = [&get_rpc]() {
        std::sort(
            get_rpc.res_.m_dataspaces.begin(),
            get_rpc.res_.m_dataspaces.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.m_path < rhs.m_path;
            }
        );
    };

    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE_EQUAL(get_rpc.res_.m_dataspaces.size(), 3);
    sort_returned_spaces();
    BOOST_CHECK_EQUAL(space1.m_path, get_rpc.res_.m_dataspaces[0].m_path);
    BOOST_CHECK_EQUAL(space2.m_path, get_rpc.res_.m_dataspaces[1].m_path);
    BOOST_CHECK_EQUAL(space3.m_path, get_rpc.res_.m_dataspaces[2].m_path);
    const int64_t mod1 = get_rpc.res_.m_dataspaces[0].m_storeLastWrite;
    const int64_t mod2 = get_rpc.res_.m_dataspaces[1].m_storeLastWrite;
    const int64_t mod3 = get_rpc.res_.m_dataspaces[2].m_storeLastWrite;
    BOOST_CHECK_LE(mod1, mod2); // valid in a true async server?
    BOOST_CHECK_LE(mod1, mod3); // whether mod2 and mod3 have a <= relation is debatable

    get_rpc.req_.m_storeLastWriteFilter = mod1;
    BOOST_REQUIRE(get_rpc.call());
    // We should not get space1, not must be smaller than 3
    BOOST_CHECK_LT(get_rpc.res_.m_dataspaces.size(), 3);
    sort_returned_spaces();

    // But whether we get 2 or 1 or 0 spaces depends on whether the clock
    // resolution is fine enough for mod1 and mod2/mod3 to be different.
    if (mod1 < mod2 && mod1 < mod3) {
        BOOST_REQUIRE_EQUAL(get_rpc.res_.m_dataspaces.size(), 2);
        BOOST_CHECK_EQUAL(space2.m_path, get_rpc.res_.m_dataspaces[0].m_path);
        BOOST_CHECK_EQUAL(space3.m_path, get_rpc.res_.m_dataspaces[1].m_path);
        // lastChanged should be unchanged
        // Except currently, our conversion from file_clock to system_clock
        // does not appear consistent (got a 1usec difference during testing),
        // so allow a 100usec difference (1/10th of a millisecond).
        // FIXME once we have accurate (manually maintained) TX-based m_lastChanged for spaces.
        /*
SpaceProtoTests.cpp(284): error: in "SpaceProtoTests/test_Dataspace24_GetDataspaces1_since":
check mod3 == get_rpc.res_.m_dataspaces[1].m_lastChanged.get_long() has failed
[1610368676693922 !=
 1610368676693921]
        */
        BOOST_CHECK_LE(std::abs(mod2 - get_rpc.res_.m_dataspaces[0].m_storeLastWrite), 100);
        BOOST_CHECK_LE(std::abs(mod3 - get_rpc.res_.m_dataspaces[1].m_storeLastWrite), 100);
    } else if (mod1 == mod2 && mod1 == mod3) {
        BOOST_REQUIRE_EQUAL(get_rpc.res_.m_dataspaces.size(), 0);
    } else {
        BOOST_FAIL("Unexpected Dataspace lastChanged values");
    }

    /*
    ** I.e. we get one space, instead of none. Could be that same
    ** 1usec difference mentioned above. Lets also add 1/10th ms to avoid that.
SpaceProtoTests.cpp(304): error: in "SpaceProtoTests/test_Dataspace24_GetDataspaces1_since":
check get_rpc.res_.m_dataspaces.size() == 0 has failed [1 != 0]
    */
    const int64_t mod_max = std::max(mod1, std::max(mod2, mod3)) + 100;
    get_rpc.req_.m_storeLastWriteFilter = mod_max;
    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(get_rpc.res_.m_dataspaces.size(), 0);
    /*
    ** We no longer use the exact value above, so the > vs >= doesn't matter anymore...
    // Should use > not >=, thus no spaces should be returned
    // Unless we had that extension that returns deleted spaces too.
    */
}

BOOST_AUTO_TEST_SUITE_END() // SpaceProtoTests

//============================================================================

} // End anonymous namespace
