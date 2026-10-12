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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/Guid.h>

#include <oes/postgresql/PhysicalName.h>

#include <oes/eml/etp12/MsgUtils.h>
#include <oes/eml/openkv/DiscoProto.h>
#include <oes/eml/openkv/SpaceProto.h>
#include <oes/eml/openkv/Repo.h>
#include <oes/eml/openkv/SpaceDDL.h>


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

BOOST_FIXTURE_TEST_SUITE(SpaceProtoTests, oes::testing::eml::OpenKVServerFixture)

// Test that downgrade the DB schema, and check that it is propertly upgraded when trying to access it
// This test will be used to test the upgrade path from schema version 8 to current version
BOOST_AUTO_TEST_CASE(test_schema_upgrade) {

    auto space = putSpace("DataSpaceTest/UpgradeSchema");

    Space space3 = admin_db_->findSpace(space.m_path);
    int first_version = oes::eml::openkv::OpenKVRepo::getSchemaVersion(admin_db_->handle(), space3.dbfile_);
    BOOST_CHECK_EQUAL(first_version,15);

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
    BOOST_CHECK_EQUAL(schema_version,15);

    clearDataSpace(space3.dbfile_);
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
