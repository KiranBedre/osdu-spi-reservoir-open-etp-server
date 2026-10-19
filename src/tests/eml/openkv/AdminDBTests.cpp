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

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/LoggerTestTools.h>

#include <oes/core/utils/Guid.h>

#include <oes/common/etp12/Messages.h>
#include <oes/eml/openkv/AdminDB.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <regex>

namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::testing;

using oes::core::Guid;
using openkv::Space;

//============================================================================

BOOST_AUTO_TEST_SUITE(SpaceTests)

BOOST_AUTO_TEST_CASE(test_equals) {
    //  cppcheck-suppress unassignedVariable
    Space empty, space1, space2, upper1;
    space1.xdata_json_ = "{\"a\":\"b\",\"c\":\"d\"}";
    space2.xdata_json_ = "{\t\"c\":\"d\", \"a\":\"b\"\n}";
    upper1.xdata_json_ = "{\"A\":\"b\",\"c\":\"d\"}";

    BOOST_CHECK(empty == empty);
    BOOST_CHECK(empty != space1);
    BOOST_CHECK(empty != space2);

    BOOST_CHECK(upper1 != space1);
    BOOST_CHECK(space1 != upper1);

    // JSON syntax doesn't matter, xdata is normalized for ==
    BOOST_CHECK(space1 == space2);
    BOOST_CHECK_EQUAL(space1 == space2, space1.equals(space2));
}

BOOST_AUTO_TEST_CASE(test_lessThan) {
    // cppcheck-suppress unassignedVariable
    Space empty, space1, space2, upper1;
    space1.xdata_json_ = "{\"a\":\"b\",\"c\":\"d\"}";
    space2.xdata_json_ = "{\t\"c\":\"d\", \"a\":\"b\"\n}";
    upper1.xdata_json_ = "{\"A\":\"b\",\"c\":\"d\"}";

    BOOST_CHECK(!(empty < empty));
    BOOST_CHECK(empty < space1);
    BOOST_CHECK(empty < space2);

    BOOST_CHECK(!(space1 < upper1));
    BOOST_CHECK(upper1 < space1);

    // JSON syntax doesn't matter, xdata is normalized for <
    BOOST_CHECK_EQUAL(false, space1 < space2);
    BOOST_CHECK_EQUAL(space1 < space2, space2 < space1);
    BOOST_CHECK_EQUAL(space1 < space2, space1.lessThan(space2));
}

BOOST_AUTO_TEST_CASE(test_xdata_xform) {
    Space space;

    auto map = space.xdataAsMap();
    BOOST_CHECK(map.empty());
    space.setXDdataFromMap(map);
    BOOST_CHECK_EQUAL("", space.xdata_json_);

    space.xdata_json_ = "{}";
    map = space.xdataAsMap();
    BOOST_CHECK(map.empty());
    space.setXDdataFromMap(map);
    BOOST_CHECK_EQUAL("", space.xdata_json_);

    space.xdata_json_ = "{\"a\":\"b\"}";
    map = space.xdataAsMap();
    BOOST_CHECK_EQUAL(size_t(1), map.size());
    space.setXDdataFromMap(map);
    BOOST_CHECK_EQUAL("{\"a\":\"b\"}", space.xdata_json_);

    space.xdata_json_ = "{\"a\":\"b\",\"c\":\"d\"}";
    map = space.xdataAsMap();
    BOOST_CHECK_EQUAL(size_t(2), map.size());
    space.setXDdataFromMap(map);
    BOOST_CHECK_EQUAL("{\"a\":\"b\",\"c\":\"d\"}", space.xdata_json_);

    space.xdata_json_ = "{\"c\":\"d\",\"a\":\"b\"}";
    map = space.xdataAsMap();
    BOOST_CHECK_EQUAL(size_t(2), map.size());
    space.setXDdataFromMap(map); // xdata reordered
    BOOST_CHECK_EQUAL("{\"a\":\"b\",\"c\":\"d\"}", space.xdata_json_);
}

BOOST_AUTO_TEST_CASE(test_validatePath) {
    std::string space_path; // inputs
    std::string path, db_path, folder, name, err_msg; // outputs

    err_msg.clear();
    space_path = "foo/bar";
    bool ok = Space::validatePath(
        space_path,
        path, db_path, folder, name, err_msg
    );
    BOOST_CHECK_EQUAL(ok, true);
    BOOST_CHECK_EQUAL(path, "foo/bar");
    BOOST_CHECK(boost::starts_with(db_path,"sfoo_bar"));
    BOOST_CHECK_EQUAL(folder, "foo");
    BOOST_CHECK_EQUAL(name, "bar");
    BOOST_CHECK_EQUAL(err_msg, "");
    // check path with invalid symbols like space and '%'
    err_msg.clear();
    space_path = "6991677e-1532-4641-abe5-277e202c40fb/! @#$%^&:scenario__1";
    ok = Space::validatePath(
        space_path, path, db_path, folder, name, err_msg
    );
    BOOST_CHECK_EQUAL(ok, false);
    const std::string expected_err_msg = 
        R"(
Dataspace path contains unsupported symbols (supported expr: ^[A-Za-z0-9_/\-\.]{3,}$).
The correct path may be: )" + 
        std::string("6991677e-1532-4641-abe5-277e202c40fb/_________scenario__1");
    space_path = "6991677e-1532-4641-abe5-277e202c40fb/_________scenario__1";
    BOOST_CHECK_EQUAL(err_msg, expected_err_msg);
    // check that suggested path is correct
    ok = Space::validatePath(
        space_path,
        path, db_path, folder, name, err_msg
    );
    BOOST_CHECK_EQUAL(ok, true);
    std::regex re(R"(^[A-Za-z0-9_]{3,64}$)");
    std::smatch m;
    BOOST_CHECK(std::regex_match (db_path, m, re));
}

struct SpaceCacheUpdater: Space{
    SpaceCacheUpdater() = default;
    explicit SpaceCacheUpdater(const Space& space): Space(space) {}
    void setReadUpdateTime(int64_t read_access_cache_updated) const {
        read_access_cache_updated_ = read_access_cache_updated;
    }
    void setWriteUpdateTime(int64_t write_access_cache_updated) const {
        write_access_cache_updated_ = write_access_cache_updated;
    }
};

BOOST_AUTO_TEST_CASE(test_read_access_cache){
    Space space;
    BOOST_CHECK_EQUAL(space.canRead().has_value(), false);
    space.setReadAccess(true);
    BOOST_CHECK_EQUAL(space.canRead().has_value(), true);
    BOOST_CHECK_EQUAL(space.canRead().value(), true);
    space.setReadAccess(false);
    BOOST_CHECK_EQUAL(space.canRead().value(), false);
    space.setReadAccess(true);
    BOOST_CHECK_EQUAL(space.canRead().value(), true);

    SpaceCacheUpdater space_cache_updater(space);

    const int64_t fourteen_minutes_ago = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() - 14 * 60;
    space_cache_updater.setReadUpdateTime(fourteen_minutes_ago);
    BOOST_CHECK_EQUAL(space_cache_updater.canRead().has_value(), true);

    const int64_t sixteen_minutes_ago = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() - 16 * 60;
    space_cache_updater.setReadUpdateTime(sixteen_minutes_ago);
    BOOST_CHECK_EQUAL(space_cache_updater.canRead().has_value(), false);
}

BOOST_AUTO_TEST_CASE(test_write_access_cache){
    Space space;
    BOOST_CHECK_EQUAL(space.canWrite().has_value(), false);
    space.setWriteAccess(true);
    BOOST_CHECK_EQUAL(space.canWrite().has_value(), true);
    BOOST_CHECK_EQUAL(space.canWrite().value(), true);
    space.setWriteAccess(false);
    BOOST_CHECK_EQUAL(space.canWrite().value(), false);
    space.setWriteAccess(true);
    BOOST_CHECK_EQUAL(space.canWrite().value(), true);

    SpaceCacheUpdater space_cache_updater(space);

    const int64_t fourteen_minutes_ago = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() - 14 * 60;
    space_cache_updater.setWriteUpdateTime(fourteen_minutes_ago);
    BOOST_CHECK_EQUAL(space_cache_updater.canWrite().has_value(), true);

    const int64_t sixteen_minutes_ago = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() - 16 * 60;
    space_cache_updater.setWriteUpdateTime(sixteen_minutes_ago);
    BOOST_CHECK_EQUAL(space_cache_updater.canWrite().has_value(), false);
}

BOOST_AUTO_TEST_SUITE_END() // SpaceTests

//============================================================================

struct AdminDBFixture {
    AdminDBFixture() {
        const char* connection_string = std::getenv("POSTGRESQL_CONN_STRING");
        BOOST_CHECK(connection_string!=nullptr);
        const std::string mdb_file(connection_string);
        BOOST_CHECK(!mdb_file.empty());
        db_filename_ = mdb_file;
        conn_ = makeConn(db_filename_);
    }

    ~AdminDBFixture() {
        conn_->execute("DROP SCHEMA IF EXISTS admin CASCADE");
    }

    static openkv::Connection_ptr makeConn(const std::string& db_filename) {
        return std::make_shared<oes::postgresql::Connection>(db_filename);
    }

    openkv::Connection_ptr newConn() const {
        return makeConn(db_filename_);
    }

    std::string db_filename_;
    openkv::Connection_ptr conn_;
};

//============================================================================

BOOST_FIXTURE_TEST_SUITE(AdminDBTests, AdminDBFixture)

BOOST_AUTO_TEST_CASE(test_createSchema) {
    openkv::OpenKVAdminDB master(conn_);
    BOOST_REQUIRE(master.createSchema());
}

BOOST_AUTO_TEST_CASE(test_isSchemaCreated) {
    openkv::OpenKVAdminDB master(conn_);
    BOOST_CHECK(!master.isSchemaCreated());
    BOOST_REQUIRE(master.createSchema());
    BOOST_CHECK(master.isSchemaCreated());
}

BOOST_AUTO_TEST_CASE(test_isSchemaUpToDate) {
    openkv::OpenKVAdminDB master(conn_);
    BOOST_CHECK(!master.isSchemaUpToDate(true));
    BOOST_REQUIRE(master.createSchema());
    BOOST_CHECK(master.isSchemaUpToDate(true));
    BOOST_CHECK(master.isSchemaUpToDate(false));
}

BOOST_AUTO_TEST_CASE(test_users) {
    openkv::OpenKVAdminDB master(conn_);
    BOOST_REQUIRE(master.createSchema());

    BOOST_CHECK_EQUAL(size_t(0), master.userCount());
    std::vector<std::string> expected_users;
    BOOST_CHECK(expected_users == master.getUserNames());

    expected_users.emplace_back("foo");
    BOOST_CHECK(master.addUser(expected_users.back(), "bar"));
    BOOST_CHECK_EQUAL(size_t(1), master.userCount());
    BOOST_CHECK(expected_users == master.getUserNames());
    BOOST_CHECK(master.userPasswordMatches("foo", "bar"));
    BOOST_CHECK(!master.userPasswordMatches("foo", "Bar")); // case sensitive on password
    BOOST_CHECK(!master.userPasswordMatches("foo", "baz"));
    BOOST_CHECK(!master.userPasswordMatches("FOO", "bar")); // case sensitive on name

    expected_users.emplace_back("zeta");
    BOOST_CHECK(master.addUser(expected_users.back(), "phi"));
    BOOST_CHECK_EQUAL(size_t(2), master.userCount());
    BOOST_CHECK(expected_users == master.getUserNames());
    BOOST_CHECK(master.userPasswordMatches("zeta", "phi"));

    expected_users.emplace_back("alpha");
    BOOST_CHECK(master.addUser(expected_users.back(), "beta", false));
    BOOST_CHECK_EQUAL(size_t(3), master.userCount());
    BOOST_CHECK(expected_users != master.getUserNames()); // always alpha-sorted
    std::sort(expected_users.begin(), expected_users.end());
    BOOST_CHECK(expected_users == master.getUserNames());
    BOOST_CHECK(master.userPasswordMatches("alpha", "beta"));

    // Cannot add the same user again
    {
        //oes::testing::ScopedExpectedMessage msg1(
        //    oes::core::Message::error,
        //    "Cannot add user zeta: UNIQUE constraint failed: users.name.*"
        //);
        BOOST_CHECK(!master.addUser(expected_users.back(), "beta", false));
    }
    BOOST_CHECK_EQUAL(size_t(3), master.userCount());
    BOOST_CHECK(expected_users == master.getUserNames());
}

BOOST_AUTO_TEST_CASE(test_spaces) {
    openkv::OpenKVAdminDB master(conn_);
    BOOST_REQUIRE(master.createSchema());

    BOOST_CHECK_EQUAL(size_t(0), master.spaceCount());
    std::vector<Space> expected_spaces;
    BOOST_CHECK(expected_spaces == master.getSpaces());

    // Add 1st project (i.e. ETP space)
    Space ds1_prj1;
    ds1_prj1.uid_ = Guid::make().toXmlString();
    ds1_prj1.path_ = "DS1/PRJ1";
    ds1_prj1.dbfile_ = "DS1/Emr-ffaa-PRJ1.db";
    ds1_prj1.xdata_json_ = "{\"ds.title\":\"DS1\"}";

    std::vector<Space> new_spaces;
    new_spaces.emplace_back(ds1_prj1);

    BOOST_CHECK(master.addSpaces(new_spaces));

    // Not expecting ds1_prj1, because addSpaces() has side-effects on uid_ and dbfile_
    expected_spaces.emplace_back(new_spaces.back());

    BOOST_CHECK_EQUAL(size_t(1), master.spaceCount());
    BOOST_CHECK(expected_spaces == master.getSpaces());

    // Add 2nd (same "Dataspace" / "Folder")

    Space ds1_prj2;
    ds1_prj2.uid_ = Guid::make().toXmlString();
    ds1_prj2.path_ = "DS1/PRJ2";
    ds1_prj2.dbfile_ = "DS1/Emr-ffbb-PRJ2.db";
    ds1_prj2.xdata_json_ = "{\"ds.title\":\"DS1\"}";

    new_spaces.clear();
    new_spaces.emplace_back(ds1_prj2);

    BOOST_CHECK(master.addSpaces(new_spaces));
    expected_spaces.emplace_back(new_spaces.back());

    BOOST_CHECK_EQUAL(size_t(2), master.spaceCount());
    BOOST_CHECK(expected_spaces == master.getSpaces());

    // Add 3rd (new "Dataspace" / "Folder")

    Space ds2_prjA;
    ds2_prjA.uid_ = Guid::make().toXmlString();
    ds2_prjA.path_ = "DS2/PRJA";
    ds2_prjA.dbfile_ = "DS2/Emr-ffcc-PRJA.db";
    ds2_prjA.xdata_json_ = "{\"ds.title\":\"DS2\"}";

    new_spaces.clear();
    new_spaces.emplace_back(ds2_prjA);

    BOOST_CHECK(master.addSpaces(new_spaces));
    expected_spaces.emplace_back(new_spaces.back());

    BOOST_CHECK_EQUAL(size_t(3), master.spaceCount());
    BOOST_CHECK(expected_spaces == master.getSpaces());

    // Delete "2nd" project

    BOOST_CHECK(master.dropSpaces({ ds1_prj2.uid_ }));

    BOOST_CHECK_EQUAL(size_t(2), master.spaceCount());

    // Drop middle entry from expected_spaces
    BOOST_REQUIRE_EQUAL(size_t(3), expected_spaces.size());
    std::swap(expected_spaces[2], expected_spaces[1]);
    expected_spaces.resize(2);

    BOOST_CHECK(expected_spaces == master.getSpaces());

    // Dropping by-path is also allowed
    BOOST_CHECK(master.dropSpaces({ ds2_prjA.path_ }));
    expected_spaces.resize(1); // dropped one is the last / 2nd

    BOOST_CHECK(expected_spaces == master.getSpaces());
}

BOOST_AUTO_TEST_CASE(test_multi_session_access) {
    // Create AdminDB, with a single space
    openkv::OpenKVAdminDB master(conn_);
    BOOST_REQUIRE(master.createSchema());
    Space space;
    space.uid_ = Guid::make().toXmlString();
    space.path_ = "DS1/PRJ1";
    space.dbfile_ = "DS1/Emr-ffaa-PRJ1.db";
    space.xdata_json_ = "{\"ds.title\":\"DS1\"}";
    BOOST_CHECK(master.addSpaces({ space }));

    // Create separate connection, simulating a client that reads
    openkv::OpenKVAdminDB reader1(newConn());
    BOOST_CHECK_EQUAL(size_t(1), reader1.spaceCount());
    BOOST_CHECK_EQUAL(size_t(1), reader1.getSpaces().size());
    // Neither calls above was enough to lock the DB, must
    // be findSpace(), which was not properly reset()'ing its stmt
    Space space1a = reader1.findSpace(space.path_);
    BOOST_CHECK(space == space1a);

    // Now create yet another connection, but that tries to write this time,
    // simulating a client that does a direct-EPC-import.

    // Must lower default timeout of 30s, to have speedy test
    auto writer1_conn = newConn();
    openkv::OpenKVAdminDB writer1(writer1_conn);

    space.uid_ = Guid::make().toXmlString();
    space.path_ = "DS2/PRJ1";
    space.dbfile_ = "DS2/Emr-ffbb-PRJ1.db";
    space.xdata_json_ = "{\"ds.title\":\"DS2\"}";
    BOOST_CHECK(writer1.addSpaces({ space }));

    // Implicit (for now) transaction on addSpaces() just above, so we see it
    BOOST_CHECK_EQUAL(size_t(2), reader1.spaceCount());
    BOOST_CHECK_EQUAL(size_t(2), reader1.getSpaces().size());
    Space space1b = reader1.findSpace(space1a.path_);
    BOOST_CHECK(space1a == space1b);
}

BOOST_AUTO_TEST_SUITE_END() // AdminDBTests

//============================================================================

} // End anonymous namespace
