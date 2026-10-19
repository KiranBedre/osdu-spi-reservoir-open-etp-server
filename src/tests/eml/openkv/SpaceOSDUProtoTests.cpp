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

BOOST_AUTO_TEST_SUITE_END() // SpaceProtoTests

//============================================================================

} // End anonymous namespace
