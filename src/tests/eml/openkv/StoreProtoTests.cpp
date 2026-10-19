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

#include <oes/testing/eml/ServerFixture.h>

#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/Guid.h>

#include <oes/eml/etp12/MsgUtils.h>
#include <oes/testing/eml/ServerTesting.hpp>

#include <oes/eml/openkv/DiscoProto.h>
#include <oes/eml/openkv/StoreProto.h>
#include <oes/eml/openkv/AdminDB.h>
#include <oes/eml/openkv/SpaceDML.h>

#include <cmath>

namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

using oes::core::Guid;

//============================================================================

BOOST_FIXTURE_TEST_SUITE(StoreProtoTests, oes::testing::eml::OpenKVServerFixture)

BOOST_AUTO_TEST_CASE(test_Store4_PutDataObjects) {
    auto get_disco_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);

    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // ETP space should be empty, initially
    get_disco_rpc.req_.m_context.m_uri = space.m_uri;
    BOOST_REQUIRE(get_disco_rpc.call());
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 0);

    auto make_obj_uri = [&space_uri](const std::string& resqml_type, const Guid& uid) {
        return space_uri.child_type(resqml_type, EmlUri::eRESQML20).child_obj(uid);
    };

    const auto& crs = oes::testing::eml::crs_mock();
    const EmlUri crs_uri = make_obj_uri(crs.type_, crs.guid_);
    data12::Object::DataObject crs_obj;
    crs_obj.m_resource.m_uri = crs_uri.to_string();
    crs_obj.m_resource.m_name = crs.name_;

    XDataMapBuilder xmb{crs_obj.m_resource.m_customData};
    xmb.emplace("key", "val");
    xmb.emplace("text", "String Value");
    xmb.emplace("bool", true);
    xmb.emplace("real", 1.23);
    xmb.emplace("long", int64_t(1) << 45);
    xmb.emplace("float", NAN);
    xmb.emplace("roles", std::vector<std::string>{ "admin", "data-mgr", "user" });
    xmb.emplace("bbox", std::vector<double>{ 1.1, 1.2, 9.9, 10.1 });

    crs_obj.m_format = "xml";
    crs_obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
    constexpr auto crsPartId = "crs";
    put_store_rpc.req_.m_dataObjects.emplace(crsPartId, crs_obj);

    BOOST_REQUIRE(put_store_rpc.call());
    BOOST_REQUIRE_EQUAL(put_store_rpc.res_.m_success.size(), 1);
    const auto& crsPartResponseIt = put_store_rpc.res_.m_success.find(crsPartId);
    BOOST_REQUIRE(crsPartResponseIt != put_store_rpc.res_.m_success.cend());

    // Check Resource was correctly extracted from DataObject
    BOOST_REQUIRE(get_disco_rpc.call()); // should see 1 res now
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 1);
    BOOST_CHECK_EQUAL(get_disco_rpc.res_.m_resources[0].m_name, crs.name_);
    BOOST_CHECK_EQUAL(
        get_disco_rpc.res_.m_resources[0].m_customData.size(),
        crs_obj.m_resource.m_customData.size() + 2 // OpenKV adds "creator" and "created" key
    );
    get_disco_rpc.res_.m_resources[0].m_customData.erase("creator");
    get_disco_rpc.res_.m_resources[0].m_customData.erase("created");
    BOOST_CHECK(
        get_disco_rpc.res_.m_resources[0].m_customData ==
        crs_obj.m_resource.m_customData
    );

    const auto& datum = oes::testing::eml::datum_mock();
    const EmlUri datum_uri = make_obj_uri(datum.type_, datum.guid_);
    data12::Object::DataObject datum_obj;
    datum_obj.m_resource.m_uri = datum_uri.to_string();
    datum_obj.m_resource.m_name = datum.name_;
    xdataEmplace(datum_obj.m_resource.m_customData, "key2", "val2");
    datum_obj.m_resource.m_lastChanged = 1465810594; // 2016-06-13T09:36:34Z
    datum_obj.m_format = "xml";
    datum_obj.m_data.assign(datum.xml_.begin(), datum.xml_.end());
    put_store_rpc.req_.m_dataObjects.clear();
    constexpr auto datumPartId = "datum";
    put_store_rpc.req_.m_dataObjects.emplace(datumPartId, datum_obj);

    BOOST_REQUIRE(put_store_rpc.call());
    BOOST_REQUIRE_EQUAL(put_store_rpc.res_.m_success.size(), 1);
    const auto& datumPartResponseIt = put_store_rpc.res_.m_success.find(datumPartId);
    BOOST_REQUIRE(datumPartResponseIt != put_store_rpc.res_.m_success.cend());

    BOOST_REQUIRE(get_disco_rpc.call()); // should see 2 res now
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 2);

    // Check references are correctly extracted too
    get_disco_rpc.req_.m_scope = Object::ContextScopeKind::eTargets;
    get_disco_rpc.req_.m_context.m_uri = datum_uri.to_string();
    get_disco_rpc.req_.m_context.m_depth = 1;
    get_disco_rpc.req_.m_context.m_dataObjectTypes.clear();

    // Datum obj references CRS obj, so should be visible
    BOOST_REQUIRE(get_disco_rpc.call()); // should see 1 res now
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 1);
    BOOST_CHECK_EQUAL(get_disco_rpc.res_.m_resources[0].m_name, crs.name_);

    get_disco_rpc.req_.m_context.m_includeSecondarySources = true;
    BOOST_REQUIRE(get_disco_rpc.call()); // should see 2 res now
    BOOST_CHECK_EQUAL(get_disco_rpc.res_.m_resources.size(), 2);
}

BOOST_AUTO_TEST_CASE(test_Store4_PutDataObjects_twice_no_rel_no_ary) {
    auto get_disco_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);

    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // ETP space should be empty, initially
    get_disco_rpc.req_.m_context.m_uri = space.m_uri;
    BOOST_REQUIRE(get_disco_rpc.call());
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 0);

    auto make_obj_uri = [&space_uri](const std::string& resqml_type, const Guid& uid) {
        return space_uri.child_type(resqml_type, EmlUri::eRESQML20).child_obj(uid);
    };

    const auto& crs = oes::testing::eml::crs_mock();
    const EmlUri crs_uri = make_obj_uri(crs.type_, crs.guid_);
    data12::Object::DataObject crs_obj;
    crs_obj.m_resource.m_uri = crs_uri.to_string();
    crs_obj.m_resource.m_name = crs.name_;
    xdataEmplace(crs_obj.m_resource.m_customData, "key", "val");
    crs_obj.m_format = "xml";
    crs_obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
    constexpr auto crsPartId = "crs";
    put_store_rpc.req_.m_dataObjects.emplace(crsPartId, crs_obj);

    // 1st time, does a SQL insert
    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }
    BOOST_CHECK_EQUAL(1u, objCount(space));

    // 2nd time, does a SQL update
    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(put_store_rpc.res_.m_success.size(), 1);
    const auto& crsPartResponseIt = put_store_rpc.res_.m_success.find(crsPartId);
    BOOST_REQUIRE(crsPartResponseIt != put_store_rpc.res_.m_success.cend());

    BOOST_CHECK_EQUAL(1u, objCount(space)); // still only 1
}

BOOST_AUTO_TEST_CASE(test_Store4_PutDataObjects_twice_with_rel_no_ary) {
    auto get_disco_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);

    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // ETP space should be empty, initially
    get_disco_rpc.req_.m_context.m_uri = space.m_uri;
    BOOST_REQUIRE(get_disco_rpc.call());
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 0);

    auto make_obj_uri = [&space_uri](const std::string& resqml_type, const Guid& uid) {
        return space_uri.child_type(resqml_type, EmlUri::eRESQML20).child_obj(uid);
    };

    const auto& crs = oes::testing::eml::crs_mock();
    const EmlUri crs_uri = make_obj_uri(crs.type_, crs.guid_);
    data12::Object::DataObject crs_obj;
    crs_obj.m_resource.m_uri = crs_uri.to_string();
    crs_obj.m_resource.m_name = crs.name_;
    xdataEmplace(crs_obj.m_resource.m_customData, "key", "val");
    crs_obj.m_format = "xml";
    crs_obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
    put_store_rpc.req_.m_dataObjects.emplace("crs", crs_obj);

    const auto& datum = oes::testing::eml::datum_mock();
    const EmlUri datum_uri = make_obj_uri(datum.type_, datum.guid_);
    data12::Object::DataObject datum_obj;
    datum_obj.m_resource.m_uri = datum_uri.to_string();
    datum_obj.m_resource.m_name = datum.name_;
    xdataEmplace(datum_obj.m_resource.m_customData, "key2", "val2");
    datum_obj.m_resource.m_lastChanged = 1465810594; // 2016-06-13T09:36:34Z
    datum_obj.m_format = "xml";
    datum_obj.m_data.assign(datum.xml_.begin(), datum.xml_.end());
    constexpr auto datumPartId = "datum";
    put_store_rpc.req_.m_dataObjects.emplace(datumPartId, datum_obj);

    // 1st time, does a SQL insert
    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }
    BOOST_CHECK_EQUAL(2u, objCount(space));

    // 2nd time, does a SQL update.
    // We send only the datum this time, which has a ref
    put_store_rpc.req_.m_dataObjects.erase("crs");

    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(put_store_rpc.res_.m_success.size(), 1);
    const auto& datumPartResponseIt = put_store_rpc.res_.m_success.find(datumPartId);
    BOOST_REQUIRE(datumPartResponseIt != put_store_rpc.res_.m_success.cend());

    BOOST_CHECK_EQUAL(2u, objCount(space)); // still only 2
}

BOOST_AUTO_TEST_CASE(test_Store4_PutDataObjects_twice_with_rel_and_ary) {
    auto get_disco_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);

    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // ETP space should be empty, initially
    get_disco_rpc.req_.m_context.m_uri = space.m_uri;
    BOOST_REQUIRE(get_disco_rpc.call());
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 0);

    auto make_obj_uri = [&space_uri](const std::string& resqml_type, const Guid& uid) {
        return space_uri.child_type(resqml_type, EmlUri::eRESQML20).child_obj(uid);
    };

    const Guid txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());

    // Needs CRS and DATUM refs of TRAJ for TX-less PutDataObjects to succeed
    const auto& crs = oes::testing::eml::crs_mock();
    const EmlUri crs_uri = make_obj_uri(crs.type_, crs.guid_);
    data12::Object::DataObject crs_obj;
    crs_obj.m_resource.m_uri = crs_uri.to_string();
    crs_obj.m_resource.m_name = crs.name_;
    xdataEmplace(crs_obj.m_resource.m_customData, "key", "val");
    crs_obj.m_format = "xml";
    crs_obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
    put_store_rpc.req_.m_dataObjects.emplace("crs", crs_obj);

    const auto& datum = oes::testing::eml::datum_mock();
    const EmlUri datum_uri = make_obj_uri(datum.type_, datum.guid_);
    data12::Object::DataObject datum_obj;
    datum_obj.m_resource.m_uri = datum_uri.to_string();
    datum_obj.m_resource.m_name = datum.name_;
    xdataEmplace(datum_obj.m_resource.m_customData, "key", "val");
    datum_obj.m_format = "xml";
    datum_obj.m_data.assign(datum.xml_.begin(), datum.xml_.end());
    put_store_rpc.req_.m_dataObjects.emplace("datum", datum_obj);

    const auto& traj = oes::testing::eml::traj_mock();
    const EmlUri traj_uri = make_obj_uri(traj.type_, traj.guid_);
    data12::Object::DataObject traj_obj;
    traj_obj.m_resource.m_uri = traj_uri.to_string();
    traj_obj.m_resource.m_name = traj.name_;
    xdataEmplace(traj_obj.m_resource.m_customData, "key", "val");
    traj_obj.m_format = "xml";
    std::string traj_xml = traj.xml_;
    oes::testing::eml::EmlObjMock::erase_tag("resqml:RepresentedInterpretation", traj_xml);
    traj_obj.m_data.assign(traj_xml.begin(), traj_xml.end());
    put_store_rpc.req_.m_dataObjects.emplace("traj", traj_obj);

    // 1st time, does a SQL insert
    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(put_store_rpc.res_.m_success.size(), 3);
    BOOST_CHECK_EQUAL(3u, objCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    for (const auto& ary : traj.arrays_) {
        putMockAry(space, epc_uri_str, ary);
    }

    bool txOK = commitTx(txUid);
    BOOST_REQUIRE(txOK);

    BOOST_CHECK_EQUAL(3u, aryCount(space));

    // Find and remove the <tangentVectors> sub-tree in the XML
    oes::testing::eml::EmlObjMock::erase_tag("resqml:TangentVectors", traj_xml);
    traj_obj.m_data.assign(traj_xml.begin(), traj_xml.end());
    put_store_rpc.req_.m_dataObjects["traj"].m_data.assign(
        traj_xml.begin(), traj_xml.end()
    );

    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }
    BOOST_CHECK_EQUAL(4u, objCount(space)); // still 4

    // Arrays still referenced by the new XML are re-linked; only TangentVectors is dropped
    const auto arys = allAryMeta(space);
    BOOST_CHECK_EQUAL(2u, arys.size());
    for (const auto& ary : arys) {
        BOOST_CHECK(ary.ident_.m_pathInResource.find("tangentVectors") == std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(test_Store4_get_related_uuids) {
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    const Guid txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());
    const auto& crs = oes::testing::eml::crs_mock();
    const auto& datum = oes::testing::eml::datum_mock();
    putMockObj(space, crs);
    putMockObj(space, datum);
    BOOST_REQUIRE(commitTx(txUid));

    auto stored = [&space_uri](const oes::testing::eml::EmlObjMock& mock) {
        return stored_uid(space_uri.child_type(mock.type_, EmlUri::eRESQML20).child_obj(mock.guid_));
    };

    auto repo = std::make_shared<OpenKVRepo>(session_, connection_pool_, oes::testing::TestConfig::make());
    ErrorInfo err;
    auto writer = repo->getSpaceWriter(repo->findSpaceInfo(space_uri.dataspace_), err);
    BOOST_REQUIRE_MESSAGE(writer, err.m_message);

    // Characters that need escaping in an array literal must not break the query
    const auto related = writer->get_related_uuids({
        stored(datum),
        { "not,a \"type{}\\", Guid::make() },
    });

    // The datum references the CRS
    const std::set<TypedUid> got(related.begin(), related.end());
    BOOST_CHECK(got == (std::set<TypedUid>{ stored(datum), stored(crs) }));
}

// Older clients spell the external part after its XML element, without obj_
BOOST_AUTO_TEST_CASE(test_Store4_ary_container_EpcExternalPartReference_old_spelling) {
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    const auto& hdf = oes::testing::eml::epc_ext_mock(); // stored as eml20.obj_EpcExternalPartReference
    putMockObj(space, hdf);
    const auto& crs = oes::testing::eml::crs_mock();
    putMockObj(space, crs);

    auto repo = std::make_shared<OpenKVRepo>(session_, connection_pool_, oes::testing::TestConfig::make());
    ErrorInfo err;
    auto writer = repo->getSpaceWriter(repo->findSpaceInfo(space_uri.dataspace_), err);
    BOOST_REQUIRE_MESSAGE(writer, err.m_message);

    const TypedUid old_ext{ "eml20.EpcExternalPartReference", hdf.guid_ };
    BOOST_CHECK(writer->check_ary_container(old_ext, false));
    const TypedUid wrong_crs{ "resqml20.LocalDepth3dCrs", crs.guid_ }; // other types stay typed
    BOOST_CHECK(!writer->check_ary_container(wrong_crs, false));

    const auto found = writer->bulk_check_ary_containers(
        { old_ext.first, old_ext.first, wrong_crs.first },
        { old_ext.second, old_ext.second, wrong_crs.second },
        { 0, 1, 2 }, 10, false
    );
    BOOST_CHECK(found == (std::vector<bool>{ true, true, false }));

    writer->check_ary_container(old_ext, true);
    BOOST_CHECK(writer->check_pending_ary_containers().empty());
    writer->check_ary_container({ "eml20.EpcExternalPartReference", Guid::make() }, true);
    BOOST_CHECK_EQUAL(writer->check_pending_ary_containers().size(), 1u);
}

// A RESQML 2.2 array belongs to the object holding it, even when <URI> names a file as in an EPC
BOOST_AUTO_TEST_CASE(test_Store4_resqml22_file_name_uri) {
    const auto space = putSpace();

    const auto obj_first = oes::testing::eml::prop22_mock(Guid::make());
    const auto ary_first = oes::testing::eml::prop22_mock(Guid::make());
    const auto no_tx = oes::testing::eml::prop22_mock(Guid::make());

    // A wrong container leaves a missing or orphan array, and the commit fails
    const Guid txUid = startTx(space.m_uri, false);
    putMockObj(space, obj_first);
    putResqml22Ary(space, obj_first);
    putResqml22Ary(space, ary_first);
    putMockObj(space, ary_first);
    BOOST_REQUIRE(commitTx(txUid));

    // Without a transaction, putting the object again links the array put after it
    putMockObj(space, no_tx);
    putResqml22Ary(space, no_tx);
    putMockObj(space, no_tx);

    // Linked arrays are listed under their owner, unlinked ones under an external part
    std::set<Guid> owners;
    for (const auto& ary : allAryMeta(space)) {
        const EmlUri owner = EmlUri::parse(ary.ident_.m_uri);
        BOOST_CHECK_EQUAL(owner.obj_type_, "DiscreteProperty");
        BOOST_CHECK_EQUAL(int(owner.ml_), int(EmlUri::eRESQML22));
        owners.insert(owner.guid_);
    }
    BOOST_CHECK(owners == (std::set<Guid>{ obj_first.guid_, ary_first.guid_, no_tx.guid_ }));
}

BOOST_AUTO_TEST_CASE(test_Store4_PutDataObjects_in_multiple_spaces)
{
    // Arrange
    const auto space1 = putSpace("ut_test/space1");
    const auto space2 = putSpace("ut_test/space2");
    const auto& crs = oes::testing::eml::crs_mock();
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);
    for (const auto& space : { space1, space2 })
    {
        const auto space_uri = EmlUri::parse(space.m_uri);
        data12::Object::DataObject crs_obj;
        crs_obj.m_resource.m_uri = space_uri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_).to_string();
        crs_obj.m_resource.m_name = crs.name_;
        xdataEmplace(crs_obj.m_resource.m_customData, "key", "val");
        crs_obj.m_format = "xml";
        crs_obj.m_data.assign(crs.xml_.begin(), crs.xml_.end());
        put_store_rpc.req_.m_dataObjects.emplace(space.m_path, crs_obj);
    }

    // Act
    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }

    // Assert
    BOOST_REQUIRE_EQUAL(put_store_rpc.res_.m_success.size(), 2);
    BOOST_REQUIRE_EQUAL(objCount(space1), 1);
    BOOST_REQUIRE_EQUAL(objCount(space2), 1);
}

BOOST_AUTO_TEST_CASE(test_Store4_GetDataObjects)
{
    // Arrange
    const auto space = putSpace();
    auto crsObj = putCrsObj(space);
    auto datumObj = putDatumObj(space);
    auto get_store_rpc = makeRpc(&OpenKVStore::GetDataObjects);
    constexpr auto crsPartId = "crs";
    get_store_rpc.req_.m_uris.emplace(crsPartId, crsObj.m_resource.m_uri);
    get_store_rpc.req_.m_uris.emplace("datum", datumObj.m_resource.m_uri);

    // Act
    if (!get_store_rpc.call() || hasError(get_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_store_rpc.rc_));
    }

    // Assert
    BOOST_REQUIRE_EQUAL(get_store_rpc.res_.m_dataObjects.size(), 2);
    const auto crsPartResponseIt = get_store_rpc.res_.m_dataObjects.find(crsPartId);    // Check content of 'crs' object
    BOOST_REQUIRE(crsPartResponseIt != get_store_rpc.res_.m_dataObjects.end());
    BOOST_REQUIRE_EQUAL(crsPartResponseIt->second.m_resource.m_name, crsObj.m_resource.m_name);
    BOOST_REQUIRE_EQUAL(crsPartResponseIt->second.m_resource.m_uri, crsObj.m_resource.m_uri);
    BOOST_REQUIRE_EQUAL(crsPartResponseIt->second.m_format, crsObj.m_format);
    BOOST_REQUIRE(!crsPartResponseIt->second.m_data.empty());
}

BOOST_AUTO_TEST_CASE(test_Store4_GetDataObjects_from_multiple_spaces)
{
    // Arrange
    const auto space1 = putSpace("ut_test/space1");
    const auto space2 = putSpace("ut_test/space2");
    auto space1Obj = putCrsObj(space1);
    auto space2Obj = putCrsObj(space2);
    auto get_store_rpc = makeRpc(&OpenKVStore::GetDataObjects);
    get_store_rpc.req_.m_uris.emplace(space1.m_path, space1Obj.m_resource.m_uri);
    get_store_rpc.req_.m_uris.emplace(space2.m_path, space2Obj.m_resource.m_uri);

    // Act
    if (!get_store_rpc.call() || hasError(get_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_store_rpc.rc_));
    }

    // Assert
    BOOST_REQUIRE_EQUAL(get_store_rpc.res_.m_dataObjects.size(), 2);
    BOOST_REQUIRE_EQUAL(get_store_rpc.res_.m_dataObjects[space1.m_path].m_resource.m_uri, space1Obj.m_resource.m_uri);
    BOOST_REQUIRE_EQUAL(get_store_rpc.res_.m_dataObjects[space2.m_path].m_resource.m_uri, space2Obj.m_resource.m_uri);
}

BOOST_AUTO_TEST_CASE(test_Store4_DeleteDataObjects) {
    auto get_disco_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);
    auto del_store_rpc = makeRpc(&OpenKVStore::DeleteDataObjects);
    auto get_deleted_rpc = makeRpc(&OpenKVDiscovery::GetDeletedResources);

    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // ETP space should be empty, initially
    get_disco_rpc.req_.m_context.m_uri = space.m_uri;
    BOOST_REQUIRE(get_disco_rpc.call());
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 0);

    auto make_obj_uri = [&space_uri](const std::string& resqml_type, const Guid& uid) {
        return space_uri.child_type(resqml_type, EmlUri::eRESQML20).child_obj(uid);
    };

    Guid txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());

    // Needs CRS and DATUM refs of TRAJ for TX-less PutDataObjects to succeed
    putCrsObj(space);
    putDatumObj(space);

    const auto& traj = oes::testing::eml::traj_mock();
    const EmlUri traj_uri = make_obj_uri(traj.type_, traj.guid_);
    data12::Object::DataObject traj_obj;
    traj_obj.m_resource.m_uri = traj_uri.to_string();
    traj_obj.m_resource.m_name = traj.name_;
    xdataEmplace(traj_obj.m_resource.m_customData, "key", "val");
    traj_obj.m_format = "xml";
    std::string traj_xml = traj.xml_;
    oes::testing::eml::EmlObjMock::erase_tag("resqml:RepresentedInterpretation", traj_xml);
    traj_obj.m_data.assign(traj_xml.begin(), traj_xml.end());
    put_store_rpc.req_.m_dataObjects.emplace("traj", traj_obj);

    // 1st time, does a SQL insert
    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    for (const auto& ary : traj.arrays_) {
        putMockAry(space, epc_uri_str, ary);
    }

    // Now try to delete a "leaf", with arrays

    bool txOK = commitTx(txUid);
    BOOST_REQUIRE(txOK);

    BOOST_CHECK_EQUAL(4u, objCount(space));
    BOOST_CHECK_EQUAL(3u, aryCount(space));

    txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());

    del_store_rpc.req_.m_uris.emplace("traj", traj_uri.to_string());
    if (!del_store_rpc.call() || hasError(del_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(del_store_rpc.rc_));
    }
    else {
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris.size(), 1);
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris["traj"].m_values.size(), 1);
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris["traj"].m_values.front(), traj_uri.to_string());
    }

    txOK = commitTx(txUid);
    BOOST_REQUIRE(txOK);

    get_deleted_rpc.req_.m_dataspaceUri = space.m_uri;
    if (!get_deleted_rpc.call()) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_deleted_rpc.rc_));
    }
    BOOST_CHECK_EQUAL(1u,get_deleted_rpc.res_.m_deletedResources.size());
    BOOST_CHECK_EQUAL(traj_uri.to_string(),get_deleted_rpc.res_.m_deletedResources[0].m_uri);

    get_deleted_rpc.req_.m_dataObjectTypes.push_back("resqml20.obj_Wellbore");
    if (!get_deleted_rpc.call()) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_deleted_rpc.rc_));
    }
    BOOST_CHECK_EQUAL(0u,get_deleted_rpc.res_.m_deletedResources.size());

    get_deleted_rpc.req_.m_dataObjectTypes.clear();
    if (!get_deleted_rpc.call()) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_deleted_rpc.rc_));
    }
    BOOST_CHECK_EQUAL(1u,get_deleted_rpc.res_.m_deletedResources.size());

    BOOST_CHECK_EQUAL(3u, objCount(space));
    BOOST_CHECK_EQUAL(0u, aryCount(space)); // arrays deleted

    // Now try to delete a branch (no arrays). Should fail, since still used,
    // so would leave dangling references.

    
    const auto& crs = oes::testing::eml::crs_mock();
    const EmlUri crs_uri = make_obj_uri(crs.type_, crs.guid_);

    txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());

    const auto& datum = oes::testing::eml::datum_mock();
    const EmlUri datum_uri = make_obj_uri(datum.type_, datum.guid_);

    del_store_rpc.req_.m_uris.clear();
    del_store_rpc.req_.m_uris.emplace("1datum", datum_uri.to_string());
    del_store_rpc.req_.m_uris.emplace("2crs", crs_uri.to_string());
    if (!del_store_rpc.call() || hasError(del_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(del_store_rpc.rc_));
    }
    else {
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris.size(), 2);
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris["1datum"].m_values.size(), 1);
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris["1datum"].m_values.front(), datum_uri.to_string());
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris["2crs"].m_values.size(), 1);
        BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris["2crs"].m_values.front(), crs_uri.to_string());
    }

    txOK = commitTx(txUid);
    BOOST_REQUIRE(txOK);

    // space is empty now
    BOOST_CHECK_EQUAL(1u, objCount(space));
    BOOST_CHECK_EQUAL(0u, aryCount(space)); // arrays deleted
}

BOOST_AUTO_TEST_CASE(test_Store4_DeleteDataObjects_from_multiple_spaces)
{
    // Arrange
    const auto space1 = putSpace("ut_test/space1");
    const auto space2 = putSpace("ut_test/space2");
    auto space1Obj = putCrsObj(space1);
    auto space2Obj = putCrsObj(space2);
    auto del_store_rpc = makeRpc(&OpenKVStore::DeleteDataObjects);
    del_store_rpc.req_.m_uris.emplace(space1.m_path, space1Obj.m_resource.m_uri);
    del_store_rpc.req_.m_uris.emplace(space2.m_path, space2Obj.m_resource.m_uri);

    // Act
    if (!del_store_rpc.call() || hasError(del_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(del_store_rpc.rc_));
    }

    // Assert
    BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris.size(), 2);
    BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris[space1.m_path].m_values.size(), 1);
    BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris[space1.m_path].m_values.front(), space1Obj.m_resource.m_uri);
    BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris[space2.m_path].m_values.size(), 1);
    BOOST_REQUIRE_EQUAL(del_store_rpc.res_.m_deletedUris[space2.m_path].m_values.front(), space2Obj.m_resource.m_uri);
}

BOOST_AUTO_TEST_SUITE_END() // StoreProtoTests

//============================================================================

} // End anonymous namespace
