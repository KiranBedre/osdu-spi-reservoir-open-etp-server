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
#include <oes/eml/openkv/SpaceOSDUProto.h>
#include <oes/eml/openkv/StoreProto.h>
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
using oes::testing::eml::OpenKVServerFixture;

//============================================================================

std::vector<Guid> makeGuids(size_t n) {
    std::vector<Guid> guids;
    for (size_t i = 0; i < n; ++i) {
        guids.push_back(Guid::make());
    }
    return guids;
}

std::string crsUri(const data12::Object::Dataspace& space, const Guid& uid) {
    return EmlUri::parse(space.m_uri).child_type(
        oes::testing::eml::crs_mock().type_, EmlUri::eRESQML20
    ).child_obj(uid).to_string();
}

void putCrsCopies(OpenKVServerFixture& fx, const data12::Object::Dataspace& space, const std::vector<Guid>& guids) {
    auto put_rpc = fx.makeRpc(&OpenKVStore::PutDataObjects);
    for (const Guid& uid : guids) {
        const auto crs = oes::testing::eml::with_guid(oes::testing::eml::crs_mock(), uid);
        data12::Object::DataObject obj;
        obj.m_resource.m_uri = crsUri(space, uid);
        obj.m_resource.m_name = crs.name_;
        obj.m_format = "xml";
        obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
        put_rpc.req_.m_dataObjects.emplace(uid.toXmlString(), std::move(obj));
    }
    if (!put_rpc.call() || fx.hasError(put_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, fx.errorOf(put_rpc.rc_));
    }
}

void deleteCrsInTx(OpenKVServerFixture& fx, const data12::Object::Dataspace& space, const std::vector<Guid>& guids) {
    const Guid txUid = fx.startTx(space.m_uri, false);
    auto del_rpc = fx.makeRpc(&OpenKVStore::DeleteDataObjects);
    for (const Guid& uid : guids) {
        del_rpc.req_.m_uris.emplace(uid.toXmlString(), crsUri(space, uid));
    }
    if (!del_rpc.call() || fx.hasError(del_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, fx.errorOf(del_rpc.rc_));
    }
    BOOST_REQUIRE(fx.commitTx(txUid));
}

bool lockSpace(OpenKVServerFixture& fx, const data12::Object::Dataspace& space, bool lock) {
    auto lock_rpc = fx.makeRpc(&OpenKVDataspaceOSDU::LockDataspaces);
    lock_rpc.req_.m_uris.emplace(space.m_path, space.m_uri);
    lock_rpc.req_.m_lock = lock;
    return lock_rpc.call();
}

int64_t spaceRelCount(OpenKVServerFixture& fx, const data12::Object::Dataspace& space) {
    const Space info = fx.admin_db_->findSpace(space.m_path);
    return fx.admin_db_->handle()->executeInt64(fmt::format(
        "SELECT count(*) FROM admin.space_rel WHERE source_sp = '{}'", info.uid_
    ));
}

struct DerivedSpace {
    data12::Object::Dataspace source;
    data12::Object::Dataspace derived;
    std::vector<Guid> guids; // of the CRS imported by reference
};

DerivedSpace makeDerivedSpace(OpenKVServerFixture& fx, const std::string& folder, size_t n) {
    DerivedSpace d{ fx.putSpace(folder + "/Source"), {}, makeGuids(n) };
    putCrsCopies(fx, d.source, d.guids);
    BOOST_REQUIRE(lockSpace(fx, d.source, true));

    d.derived = fx.putSpace(folder + "/Derived");
    auto copy_rpc = fx.makeRpc(&OpenKVDataspaceOSDU::CopyDataspacesContent);
    copy_rpc.req_.m_dataspaces.emplace(d.source.m_path, d.source.m_uri);
    copy_rpc.req_.m_targetDataspace = d.derived.m_uri;
    BOOST_REQUIRE(copy_rpc.call());
    BOOST_REQUIRE_EQUAL(copy_rpc.res_.m_success.size(), 1u);
    BOOST_REQUIRE_EQUAL(spaceRelCount(fx, d.derived), 1);
    return d;
}

//============================================================================

BOOST_FIXTURE_TEST_SUITE(SpaceOSDUProtoTests, oes::testing::eml::OpenKVServerFixture)

BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_GetDataspaceInfo1) {
    auto get_rpc = makeRpc(&OpenKVDataspace::GetDataspaces);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);
    auto inf_rpc = makeRpc(&OpenKVDataspaceOSDU::GetDataspaceInfo);
    auto del_rpc = makeRpc(&OpenKVDataspace::DeleteDataspaces);

    // We start with no dataspaces
    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE(get_rpc.res_.m_dataspaces.empty());

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
    BOOST_REQUIRE(put_rpc.call());
    spaces_.push_back(space->m_path);

    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE_EQUAL(get_rpc.res_.m_dataspaces.size(), 1);

    std::map<std::string, std::string> uris;
    uris.emplace(space->m_path, space->m_uri);
    inf_rpc.req_.m_uris = uris;
    BOOST_REQUIRE(inf_rpc.call());
    BOOST_CHECK_EQUAL(inf_rpc.rc_.ex.m_errors.size(),0);

    data12::Object::Dataspace space2a = inf_rpc.res_.m_dataspaces["GOM/Block1"];
    BOOST_CHECK_EQUAL(space->m_uri, space2a.m_uri);
    BOOST_CHECK_EQUAL(space->m_path, space2a.m_path);
    BOOST_REQUIRE(space2a.m_storeLastWrite);

    // delete the dataspace
    const size_t initialSpaceCount = spaceCount();
    del_rpc.req_.m_uris.emplace(space->m_path, space->m_uri);
    BOOST_REQUIRE(del_rpc.call());
    BOOST_CHECK_EQUAL(initialSpaceCount - 1, spaceCount());

    BOOST_REQUIRE(inf_rpc.call());
    BOOST_CHECK_EQUAL(inf_rpc.rc_.ex.m_errors.size(),1); // 1 error
    BOOST_CHECK_EQUAL(inf_rpc.rc_.ex.m_errors.begin()->second.m_code, ENOT_FOUND);
    BOOST_CHECK(inf_rpc.rc_.ex.m_errors.begin()->second.m_message.find_first_of("Dataspace GOM/Block1 not found")!=std::string::npos);

}

BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_CopyDataspacesContent) {
    auto get_rpc = makeRpc(&OpenKVDataspace::GetDataspaces);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);
    auto get_obj_rpc = makeRpc(&OpenKVStore::GetDataObjects);

    // 2a) Add single Dataspace, with "Guid URI" (OpenKV extension)
    const std::string origin_uid = "Clone/Test1";
    auto space_origin = put_rpc.req_.m_dataspaces.emplace(
        origin_uid, data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_origin.second);
    data12::Object::Dataspace* space = &space_origin.first->second;
    const std::string origin = EmlUri::make_space(origin_uid).to_string();
    space->m_uri = origin;
    space->m_path = origin_uid;
    xdataEmplace(space->m_customData, "key", "value");
    BOOST_REQUIRE(put_rpc.call());
    spaces_.push_back(origin_uid);

    const auto& crs = oes::testing::eml::crs_mock();
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);
    const EmlUri space_uri = EmlUri::parse(origin);
    data12::Object::DataObject crs_obj;
    crs_obj.m_resource.m_uri = space_uri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_).to_string();
    crs_obj.m_resource.m_name = crs.name_;
    xdataEmplace(crs_obj.m_resource.m_customData, "key", "val");
    crs_obj.m_format = "xml";
    crs_obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
    put_store_rpc.req_.m_dataObjects.emplace(crs_obj.m_resource.m_name, std::move(crs_obj));
    BOOST_REQUIRE(put_store_rpc.call());

    const std::string origin_uid2 = "Clone/Test2";
    auto space_origin2 = put_rpc.req_.m_dataspaces.emplace(
        origin_uid2, data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_origin2.second);
    data12::Object::Dataspace* space2 = &space_origin2.first->second;
    const EmlUri space2_uri = EmlUri::make_space(origin_uid2);
    const std::string origin2 = space2_uri.to_string();
    space2->m_uri = origin2;
    space2->m_path = origin_uid2;
    xdataEmplace(space2->m_customData, "key", "value");
    BOOST_REQUIRE(put_rpc.call());
    spaces_.push_back(origin_uid2);
    const auto& hdf = oes::testing::eml::epc_ext_mock();
    data12::Object::DataObject hdf_obj;
    hdf_obj.m_resource.m_uri = space2_uri.child_type(hdf.type_, EmlUri::eRESQML20).child_obj(hdf.guid_).to_string();
    hdf_obj.m_resource.m_name = hdf.name_;
    xdataEmplace(hdf_obj.m_resource.m_customData, "key", "val");
    hdf_obj.m_format = "xml";
    hdf_obj.m_data.assign(hdf.xml_.begin(), hdf.xml_.end());
    put_store_rpc.req_.m_dataObjects.clear();
    put_store_rpc.req_.m_dataObjects.emplace(hdf_obj.m_resource.m_name, std::move(hdf_obj));
    BOOST_REQUIRE(put_store_rpc.call());


    // Request should contain 2 Dataspace
    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(get_rpc.res_.m_dataspaces.size(), 2);

    // Add Third Dataspace, with "Guid URI" (OpenKV extension)
    const std::string dest_uid = "Clone/Test3";
    auto space_dest = put_rpc.req_.m_dataspaces.emplace(
        dest_uid, data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_dest.second);
    data12::Object::Dataspace* space3 = &space_dest.first->second;
    EmlUri dest_uri = EmlUri::make_space(dest_uid);
    const std::string dest = dest_uri.to_string();
    space3->m_uri = dest;
    space3->m_path = dest_uid;
    xdataEmplace(space3->m_customData, "key", "value");
    BOOST_REQUIRE(put_rpc.call());

    // Request should contain 3 Dataspaces
    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(get_rpc.res_.m_dataspaces.size(), 3);

    // Request should contain Dataspace info
    auto copy_rpc = makeRpc(&OpenKVDataspaceOSDU::CopyDataspacesContent);
    BOOST_REQUIRE(!copy_rpc.call());
    BOOST_REQUIRE(copy_rpc.rc_.ex.m_error);
    BOOST_CHECK_EQUAL(copy_rpc.rc_.ex.m_error->m_code, EINVALID_ARGUMENT);
    BOOST_CHECK_EQUAL(copy_rpc.rc_.ex.m_error->m_message, "Nothing to add");
    BOOST_CHECK(copy_rpc.rc_.ex.m_errors.empty()); // no "array-errors"

    auto space_it2 = copy_rpc.req_.m_dataspaces.emplace(
        origin_uid, origin
    );
    BOOST_CHECK(space_it2.second);
    copy_rpc.req_.m_targetDataspace = dest;

    // Source dataspace must be locked
    BOOST_REQUIRE(copy_rpc.call());
    BOOST_CHECK_EQUAL(copy_rpc.rc_.ex.m_errors.size(),1); // 1 error
    BOOST_CHECK_EQUAL(copy_rpc.rc_.ex.m_errors.begin()->second.m_code, EINVALID_ARGUMENT);
    BOOST_CHECK(copy_rpc.rc_.ex.m_errors.begin()->second.m_message.find_first_of("not read-only")!=std::string::npos);

    // Lock source dataspace
    auto lock_rpc = makeRpc(&OpenKVDataspaceOSDU::LockDataspaces);
    lock_rpc.req_.m_uris.emplace(origin_uid, origin);
    lock_rpc.req_.m_lock = true;
    BOOST_REQUIRE(lock_rpc.call());

    BOOST_REQUIRE(copy_rpc.call());
    BOOST_CHECK_EQUAL(copy_rpc.res_.m_success.size(),1); // 1 success

    const std::string dest_obj_uri = dest_uri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_).to_string();
    get_obj_rpc.req_.m_uris.emplace(dest_obj_uri, dest_obj_uri);
    BOOST_REQUIRE(get_obj_rpc.call());
    BOOST_CHECK_EQUAL(get_obj_rpc.res_.m_dataObjects.size(), 1);

    // Unlock should fail since there is reference to the space
    lock_rpc.req_.m_lock = false;
    BOOST_REQUIRE(!lock_rpc.call());

    // Second call should with same source succeed and give same number of objects since duplicate is ignored
    BOOST_REQUIRE(copy_rpc.call());
    BOOST_CHECK_EQUAL(copy_rpc.res_.m_success.size(),1); // 1 success
    BOOST_REQUIRE(get_obj_rpc.call());
    BOOST_CHECK_EQUAL(get_obj_rpc.res_.m_dataObjects.size(), 1);

    // Third call should adding different source should succeed and add object
    lock_rpc.req_.m_uris.clear();
    lock_rpc.req_.m_uris.emplace(origin_uid2, origin2);
    lock_rpc.req_.m_lock = true;
    BOOST_REQUIRE(lock_rpc.call());
    copy_rpc.req_.m_dataspaces.emplace(
        origin_uid2, origin2
    );
    BOOST_REQUIRE(copy_rpc.call());
    BOOST_CHECK_EQUAL(copy_rpc.res_.m_success.size(),2); // 1 success
    const std::string dest_obj_uri2 = dest_uri.child_type(hdf.type_, EmlUri::eEML20).child_obj(hdf.guid_).to_string();
    get_obj_rpc.req_.m_uris.emplace(dest_obj_uri2, dest_obj_uri2);
    BOOST_REQUIRE(get_obj_rpc.call());
    BOOST_CHECK_EQUAL(get_obj_rpc.res_.m_dataObjects.size(), 2);

    // Add fourth Dataspace, and copy third into it
    const std::string dest2_uid = "Clone/Test4";
    auto space_dest2 = put_rpc.req_.m_dataspaces.emplace(
        dest2_uid, data12::Object::Dataspace{}
    );
    BOOST_CHECK(space_dest2.second);
    data12::Object::Dataspace* space4 = &space_dest2.first->second;
    EmlUri dest2_uri = EmlUri::make_space(dest2_uid);
    const std::string dest2 = dest2_uri.to_string();
    space4->m_uri = dest2;
    space4->m_path = dest2_uid;
    xdataEmplace(space4->m_customData, "key", "value");
    BOOST_REQUIRE(put_rpc.call());

    lock_rpc.req_.m_uris.clear();
    lock_rpc.req_.m_uris.emplace(dest_uid, dest);
    lock_rpc.req_.m_lock = true;
    BOOST_REQUIRE(lock_rpc.call());

    // Request should contain 4 Dataspaces
    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(get_rpc.res_.m_dataspaces.size(), 4);

    copy_rpc.req_.m_targetDataspace = dest2;
    copy_rpc.req_.m_dataspaces.clear();
    copy_rpc.req_.m_dataspaces.emplace(
        dest_uid, dest
    );
    BOOST_REQUIRE(copy_rpc.call());
    BOOST_CHECK_EQUAL(copy_rpc.res_.m_success.size(),1); // 1 success

    // Delete the destination dataspace
    auto del_rpc = makeRpc(&OpenKVDataspace::DeleteDataspaces);
    del_rpc.req_.m_uris.emplace(dest2, dest2);
    BOOST_REQUIRE(del_rpc.call());

    // Lock should now succeed since there is no reference to the spaces
    lock_rpc.req_.m_lock = false;
    BOOST_REQUIRE(lock_rpc.call());

    del_rpc.req_.m_uris.clear();
    del_rpc.req_.m_uris.emplace(dest_obj_uri, dest_obj_uri);
    BOOST_REQUIRE(del_rpc.call());

}

BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_LockDataspace) {
    auto del_rpc = makeRpc(&OpenKVDataspace::DeleteDataspaces);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);
    auto loc_rpc = makeRpc(&OpenKVDataspaceOSDU::LockDataspaces);

    // Use two separate Puts, to maximize changes of separate last-modified stamps
    data12::Object::Dataspace space1;
    space1.m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();
    space1.m_path = "Lock/Test1";
    put_rpc.req_.m_dataspaces.emplace(space1.m_path, space1);
    BOOST_REQUIRE(put_rpc.call());
    BOOST_CHECK_EQUAL(1, spaceCount());

    std::map<std::string, std::string> uris;
    uris.emplace(space1.m_path, space1.m_uri);
    loc_rpc.req_.m_uris = uris;
    loc_rpc.req_.m_lock = true;
    BOOST_REQUIRE(loc_rpc.call());


    del_rpc.req_.m_uris = uris;
    BOOST_REQUIRE(!del_rpc.call());
    BOOST_CHECK_EQUAL(1, spaceCount());

    loc_rpc.req_.m_lock = false;
    BOOST_REQUIRE(loc_rpc.call());

    BOOST_REQUIRE(del_rpc.call());
    BOOST_CHECK_EQUAL(0, spaceCount());
}

BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_CopyToDataspace){
    auto del_rpc = makeRpc(&OpenKVDataspace::DeleteDataspaces);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);
    auto cop_rpc = makeRpc(&OpenKVDataspaceOSDU::CopyToDataspace);
    auto loc_rpc = makeRpc(&OpenKVDataspaceOSDU::LockDataspaces);
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);
    auto get_obj_rpc = makeRpc(&OpenKVStore::GetDataObjects);

    data12::Object::Dataspace space1;
    space1.m_uri = EmlUri::make_space(Guid::make().toXmlString()).to_string();
    space1.m_path = "Copy/Test1";
    put_rpc.req_.m_dataspaces.emplace(space1.m_path, space1);
    spaces_.push_back(space1.m_path);

    data12::Object::Dataspace space2;
    EmlUri space2_uri = EmlUri::make_space(Guid::make().toXmlString());
    space2.m_uri = space2_uri.to_string();
    space2.m_path = "Copy/Test2";
    put_rpc.req_.m_dataspaces.emplace(space2.m_path, space2);
    BOOST_REQUIRE(put_rpc.call());
    BOOST_CHECK_EQUAL(2, spaceCount());

    std::map<std::string, std::string> uris;
    const auto& crs = oes::testing::eml::crs_mock();
    const EmlUri space_uri = EmlUri::parse(space1.m_uri);
    data12::Object::DataObject crs_obj;
    crs_obj.m_resource.m_uri = space_uri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_).to_string();
    uris.emplace(crs_obj.m_resource.m_uri, crs_obj.m_resource.m_uri);
    crs_obj.m_resource.m_name = crs.name_;
    xdataEmplace(crs_obj.m_resource.m_customData, "key", "val");
    crs_obj.m_format = "xml";
    crs_obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
    put_store_rpc.req_.m_dataObjects.emplace(crs_obj.m_resource.m_name, crs_obj);

    const auto& hdf = oes::testing::eml::epc_ext_mock();
    data12::Object::DataObject hdf_obj;
    hdf_obj.m_resource.m_uri = space_uri.child_type(hdf.type_, EmlUri::eEML20).child_obj(hdf.guid_).to_string();
    uris.emplace(hdf_obj.m_resource.m_uri, hdf_obj.m_resource.m_uri);
    hdf_obj.m_resource.m_name = hdf.name_;
    xdataEmplace(hdf_obj.m_resource.m_customData, "key", "val");
    hdf_obj.m_format = "xml";
    hdf_obj.m_data.assign(hdf.xml_.begin(), hdf.xml_.end());
    put_store_rpc.req_.m_dataObjects.emplace(hdf_obj.m_resource.m_name, std::move(hdf_obj));
    BOOST_REQUIRE(put_store_rpc.call());

    loc_rpc.req_.m_uris.emplace(space1.m_path, space1.m_uri);
    loc_rpc.req_.m_lock = true;
    BOOST_REQUIRE(loc_rpc.call());

    // Test single object copy
    cop_rpc.req_.m_uris = uris;
    cop_rpc.req_.m_dataspaceUri = space2.m_uri;
    BOOST_REQUIRE(cop_rpc.call());

    const std::string dest_obj_uri2 = space2_uri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_).to_string();
    get_obj_rpc.req_.m_uris.emplace(dest_obj_uri2, dest_obj_uri2);
    BOOST_REQUIRE(get_obj_rpc.call());
    BOOST_CHECK_EQUAL(get_obj_rpc.res_.m_dataObjects.size(), 1);
    int64_t write = get_obj_rpc.res_.m_dataObjects.begin()->second.m_resource.m_storeLastWrite ;

    // Overwrite first resource
    crs_obj.m_resource.m_uri = space2_uri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_).to_string();
    put_store_rpc.req_.m_dataObjects.clear();
    put_store_rpc.req_.m_dataObjects.emplace(crs_obj.m_resource.m_name, std::move(crs_obj));
    BOOST_REQUIRE(put_store_rpc.call());

    BOOST_REQUIRE(get_obj_rpc.call());
    BOOST_CHECK_EQUAL(get_obj_rpc.res_.m_dataObjects.size(), 1);
    int64_t write2 = get_obj_rpc.res_.m_dataObjects.begin()->second.m_resource.m_storeLastWrite ;

    // Check that it was overwritten
    BOOST_CHECK(write2 > write);

    del_rpc.req_.m_uris.clear();
    del_rpc.req_.m_uris.emplace(space1.m_path, space1.m_uri);
    BOOST_REQUIRE(!del_rpc.call());
    BOOST_CHECK_EQUAL(2, spaceCount());

    del_rpc.req_.m_uris.clear();
    del_rpc.req_.m_uris.emplace(space2.m_path, space2.m_uri);
    BOOST_REQUIRE(del_rpc.call());
    BOOST_CHECK_EQUAL(1, spaceCount());

    loc_rpc.req_.m_lock = false;
    BOOST_REQUIRE(loc_rpc.call());

}

// res_space_rel_trigger: the relation to the source goes with the last imported resource
BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_space_rel_after_deletes) {
    const DerivedSpace d = makeDerivedSpace(*this, "SpaceRel", 10);

    const std::vector<Guid> local = makeGuids(2);
    putCrsCopies(*this, d.derived, local);
    deleteCrsInTx(*this, d.derived, local);
    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 1);

    deleteCrsInTx(*this, d.derived, std::vector<Guid>(d.guids.begin(), d.guids.begin() + 4));
    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 1);
    BOOST_CHECK(!lockSpace(*this, d.source, false));

    // A local object over an imported one
    putCrsCopies(*this, d.derived, { d.guids[4] });
    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 1);
    BOOST_CHECK(!lockSpace(*this, d.source, false));

    deleteCrsInTx(*this, d.derived, std::vector<Guid>(d.guids.begin() + 5, d.guids.end()));
    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 0);
    BOOST_CHECK(lockSpace(*this, d.source, false));
    BOOST_CHECK_EQUAL(objCount(d.derived), 1u);
}

// admin.update_space_rels gives the clone of a derived dataspace a relation of its own
BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_space_rel_after_clone) {
    const DerivedSpace d = makeDerivedSpace(*this, "SpaceRelClone", 3);

    data12::Object::Dataspace clone;
    clone.m_path = "SpaceRelClone/Clone";
    clone.m_uri = EmlUri::make_space(clone.m_path).to_string();
    xdataEmplace(clone.m_customData, "fromDataspace", d.derived.m_uri);
    auto put_rpc = makeRpc(&OpenKVDataspace::PutDataspaces);
    put_rpc.req_.m_dataspaces.emplace(clone.m_path, clone);
    spaces_.push_back(clone.m_path);
    if (!put_rpc.call() || hasError(put_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_rpc.rc_));
    }

    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 1);
    BOOST_CHECK_EQUAL(spaceRelCount(*this, clone), 1);
    const Space clone_info = admin_db_->findSpace(clone.m_path);
    const PhysicalName clone_phys(clone_info.dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    BOOST_CHECK_EQUAL(admin_db_->handle()->executeInt64(fmt::format(
        "SELECT count(*) FROM {}.res AS r JOIN admin.space_rel AS s ON s.id = r.space_rel_id"
        " WHERE s.source_sp = '{}' AND s.target_sp = '{}'",
        clone_phys.quotedName(), clone_info.uid_, admin_db_->findSpace(d.source.m_path).uid_
    )), 3);

    deleteCrsInTx(*this, clone, d.guids);
    BOOST_CHECK_EQUAL(spaceRelCount(*this, clone), 0);
    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 1);
    BOOST_CHECK(!lockSpace(*this, d.source, false));

    deleteCrsInTx(*this, d.derived, d.guids);
    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 0);
    BOOST_CHECK(lockSpace(*this, d.source, false));
}

// Logs the time only (--log_level=message), to compare before and after the statement-level trigger
BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_space_rel_delete_timing) {
    const size_t n = 3000;
    const DerivedSpace d = makeDerivedSpace(*this, "SpaceRelTiming", n);

    const auto start = std::chrono::steady_clock::now();
    deleteCrsInTx(*this, d.derived, d.guids);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();
    BOOST_TEST_MESSAGE(fmt::format("Deleted {} imported objects in one transaction in {} ms", n, ms));
    BOOST_CHECK_EQUAL(spaceRelCount(*this, d.derived), 0);
}

// Imported objects looked up in one batch and in several, and a guid shared with a local object
BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_GetDataObjects_imported) {
    const DerivedSpace d = makeDerivedSpace(*this, "GetImported", 25);

    const auto ext = oes::testing::eml::with_guid(oes::testing::eml::epc_ext_mock(), d.guids.front());
    const auto ext_obj = putMockObj(d.derived, ext);

    for (const size_t bulk_size : { size_t(100), size_t(7) }) {
        BOOST_TEST_CONTEXT("bulk size " << bulk_size) {
            session_->pg_bulk_size_ = bulk_size;
            auto get_rpc = makeRpc(&OpenKVStore::GetDataObjects);
            for (const Guid& uid : d.guids) {
                get_rpc.req_.m_uris.emplace(uid.toXmlString(), crsUri(d.derived, uid));
            }
            get_rpc.req_.m_uris.emplace("ext", ext_obj.m_resource.m_uri);
            if (!get_rpc.call() || hasError(get_rpc.rc_)) {
                BOOST_REQUIRE_MESSAGE(false, errorOf(get_rpc.rc_));
            }

            const auto& objs = get_rpc.res_.m_dataObjects;
            BOOST_REQUIRE_EQUAL(objs.size(), d.guids.size() + 1);
            for (const Guid& uid : d.guids) {
                const auto& data = objs.at(uid.toXmlString()).m_data;
                const std::string xml(data.begin(), data.end());
                BOOST_CHECK_NE(xml.find("obj_LocalDepth3dCrs"), std::string::npos);
                BOOST_CHECK_NE(xml.find(uid.toXmlString()), std::string::npos);
            }
            const auto& ext_data = objs.at("ext").m_data;
            BOOST_CHECK_NE(std::string(ext_data.begin(), ext_data.end()).find("obj_EpcExternalPartReference"), std::string::npos);
        }
    }
    session_->pg_bulk_size_ = 100;
}

// Every relation of the space, as source>target@xpath=cnt, sorted
std::string relsText(OpenKVServerFixture& fx, const data12::Object::Dataspace& space) {
    const PhysicalName phys(fx.admin_db_->findSpace(space.m_path).dbfile_, oes::postgresql::relational::IMPLICIT_LOWERCASE);
    return fx.admin_db_->handle()->executeString(fmt::format(
        "SELECT string_agg(line, ',' ORDER BY line) FROM ("
        " SELECT s.guid || '>' || d.guid || '@' || x.th || '=' || r.cnt AS line"
        "   FROM {0}.rel AS r"
        "   JOIN {0}.res AS s ON s.obj_id = r.obj_id"
        "   JOIN {0}.res AS d ON d.obj_id = r.dst_id"
        "   JOIN {0}.xpa AS x ON x.id = r.xpa_id) AS l",
        phys.quotedName()
    ));
}

// Relations of the copied objects and of their shared dependencies, duplicates included; a second copy changes nothing
BOOST_AUTO_TEST_CASE(test_DataspaceOSDU2424_CopyToDataspace_relations) {
    for (const size_t bulk_size : { size_t(100), size_t(7) }) {
        BOOST_TEST_CONTEXT("bulk size " << bulk_size) {
            session_->pg_bulk_size_ = bulk_size;
            const std::string folder = fmt::format("CopyRels{}", bulk_size);
            const auto source = putSpace(folder + "/Source");
            const std::vector<Guid> crs_guids = makeGuids(3);
            putCrsCopies(*this, source, crs_guids);
            std::map<std::string, std::string> uris;
            for (const auto& datum : {
                oes::testing::eml::datum_referencing(crs_guids, 23),
                oes::testing::eml::datum_referencing({ crs_guids[0], crs_guids[1] }, 5)
            }) {
                uris.emplace(datum.guid_.toXmlString(), putMockObj(source, datum).m_resource.m_uri);
            }
            const std::string source_rels = relsText(*this, source);
            BOOST_REQUIRE_EQUAL(std::count(source_rels.begin(), source_rels.end(), ','), 4); // 5 rows
            BOOST_REQUIRE(lockSpace(*this, source, true));

            const auto target = putSpace(folder + "/Target");
            for (int copy = 1; copy <= 2; ++copy) {
                BOOST_TEST_CONTEXT("copy " << copy) {
                    auto cop_rpc = makeRpc(&OpenKVDataspaceOSDU::CopyToDataspace);
                    cop_rpc.req_.m_uris = uris;
                    cop_rpc.req_.m_dataspaceUri = target.m_uri;
                    if (!cop_rpc.call() || hasError(cop_rpc.rc_)) {
                        BOOST_REQUIRE_MESSAGE(false, errorOf(cop_rpc.rc_));
                    }
                    BOOST_CHECK_EQUAL(relsText(*this, target), source_rels);
                }
            }
        }
    }
    session_->pg_bulk_size_ = 100;
}

BOOST_AUTO_TEST_SUITE_END() // SpaceProtoTests

//============================================================================

} // End anonymous namespace
