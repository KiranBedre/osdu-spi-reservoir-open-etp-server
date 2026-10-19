// ============================================================================
// Copyright 2019-2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2022-2023 EPAM Systems
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

#include <oes/common/AvroUtils.h>
#include <oes/eml/openkv/TransProto.h>
#include <oes/eml/openkv/SpaceProto.h>
#include <oes/eml/openkv/StoreProto.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

using oes::core::Guid;

//============================================================================

BOOST_FIXTURE_TEST_SUITE(TransProtoTests, oes::testing::eml::OpenKVServerFixture)

BOOST_AUTO_TEST_CASE(test_Transaction18_errors) {
    auto rpcBegin  = makeRpc(&OpenKVTransProto::StartTransaction);
    auto rpcCommit = makeRpc(&OpenKVTransProto::CommitTransaction);
    auto rpcRollbk = makeRpc(&OpenKVTransProto::RollbackTransaction);

    auto space = putSpace();

    rpcBegin.req_.m_readOnly = true;
    rpcBegin.req_.m_message = "Dummy Transaction";

    // Start a transaction with invalid URI (just with only space path). Should FAIL.
    // Supported EML URIs which includes path or UUID of dataspace:
    // - eml:///dataspace('GOM/Block2')
    // - eml:///dataspace('6fd3b725-bda2-4b21-af8d-579be7c3c109')
    rpcBegin.req_.m_dataspaceUris.emplace_back(space.m_path);
    rpcBegin.call();
    BOOST_REQUIRE(rpcBegin.rc_.ex.m_error);
    BOOST_CHECK_EQUAL(rpcBegin.rc_.ex.m_error->m_code, EINVALID_ARGUMENT);
    BOOST_CHECK_EQUAL(rpcBegin.rc_.ex.m_error->m_message, "Cannot start transaction, invalid dataspace URI(s): " + space.m_path);

    // Start a transaction with valid URI. Should SUCCEED.
    rpcBegin.req_.m_dataspaceUris = { space.m_uri };
    BOOST_REQUIRE(rpcBegin.call());
    Guid txUid = Guid::from(rpcBegin.res_.m_transactionUuid);
    BOOST_CHECK(txUid.isValid());

    // Try to start another transaction. Should FAIL.
    BOOST_REQUIRE(!rpcBegin.call());
    BOOST_REQUIRE(rpcBegin.rc_.ex.m_error);
    BOOST_CHECK_EQUAL(rpcBegin.rc_.ex.m_error->m_code, EMAX_TRANSACTIONS_EXCEEDED);
    BOOST_CHECK_EQUAL(rpcBegin.rc_.ex.m_error->m_message, "Transaction already active");

    // Try to commit transaction with INVALID uuid. Should FAIL.
    oes::common::assignUid(rpcCommit.req_.m_transactionUuid, Guid::make());
    BOOST_REQUIRE(rpcCommit.call());
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_successful, false);
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_failureReason, "Unknown transaction uuid");

    // Commit. Should SUCCEED.
    oes::common::assignUid(rpcCommit.req_.m_transactionUuid, txUid);
    BOOST_REQUIRE(rpcCommit.call());
    BOOST_CHECK(rpcCommit.res_.m_successful);
    BOOST_CHECK(rpcCommit.res_.m_failureReason.empty());

    // Try to commit a second time. Should FAIL.
    BOOST_REQUIRE(rpcCommit.call());
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_successful, false);
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_failureReason, "No active transaction");

    // Try to rollback, after the commit. Should FAIL.
    oes::common::assignUid(rpcRollbk.req_.m_transactionUuid, txUid);
    BOOST_REQUIRE(rpcRollbk.call());
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_successful, false);
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_failureReason, "No active transaction");

    // Start second transaction. Should SUCCEED.
    rpcBegin.req_.m_readOnly = false;
    rpcBegin.req_.m_message = "Another Transaction";
    BOOST_REQUIRE(rpcBegin.call());
    Guid txUid2;
    oes::common::assignUid(txUid2, rpcBegin.res_.m_transactionUuid);
    BOOST_CHECK(txUid2.isValid());
    BOOST_CHECK(txUid != txUid2);

    // Try rollback of 1st transaction. Should FAIL.
    BOOST_REQUIRE(rpcRollbk.call());
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_successful, false);
    BOOST_CHECK_EQUAL(rpcCommit.res_.m_failureReason, "No active transaction");

    // Rollback of 2nd transaction. Should SUCCEED.
    oes::common::assignUid(rpcRollbk.req_.m_transactionUuid, txUid2);
    BOOST_REQUIRE(rpcRollbk.call());

    // Try a PUT inside a read-only transaction. Should FAIL.
    rpcBegin.req_.m_readOnly = true;
    rpcBegin.req_.m_message = "3rd Transaction";
    BOOST_REQUIRE(rpcBegin.call());
    Guid txUid3;
    oes::common::assignUid(txUid3, rpcBegin.res_.m_transactionUuid);
    BOOST_CHECK(txUid3.isValid());
    BOOST_CHECK(txUid3 != txUid);
    BOOST_CHECK(txUid3 != txUid2);

    try {
        putSpace("GOM/Block2", std::weak_ptr<OpenKVSession>(), true);
        BOOST_FAIL("Expected proto12::EtpResponseStatus thrown");
    } catch (const proto12::EtpResponseStatus& rc) {
        BOOST_REQUIRE(rc.ex.m_error);
        const data12::ErrorInfo& err = *rc.ex.m_error;
        BOOST_CHECK_EQUAL(err.m_code, EREQUEST_DENIED);
        BOOST_CHECK_EQUAL(err.m_message, "Read-only Transaction");
    }

    oes::common::assignUid(rpcRollbk.req_.m_transactionUuid, txUid3);
    BOOST_REQUIRE(rpcRollbk.call());
}

BOOST_AUTO_TEST_CASE(test_Transaction18_single_write_transaction) {
    std::shared_ptr<OpenKVSession> other_session = std::make_shared<OpenKVSession>();
    constexpr auto spacePath = "TRA/SingleWrite";
    const auto spaceUri = EmlUri::make_space(spacePath);
    const auto space = putSpace(spacePath);
    BOOST_CHECK(!space.m_uri.empty());
    const auto txUid = startTx(spaceUri.to_string(), false); // read-write

    const auto txUidRO = startTx(spaceUri.to_string(), true, other_session); // read-only
    BOOST_CHECK(txUidRO.isValid());
    const auto ok = rollbackTx(txUidRO, other_session);
    BOOST_CHECK(ok);

    
    const auto txUidRW = startTx(spaceUri.to_string(), false, other_session, true); // read-write
    BOOST_CHECK(!txUidRW.isValid());

    rollbackTx(txUid);
}

BOOST_AUTO_TEST_CASE(test_Transaction18_isolation_space_proto) {
    std::shared_ptr<OpenKVSession> other_session = std::make_shared<OpenKVSession>();

    // we start w/o any spaces
    BOOST_CHECK_EQUAL(0, spaceCount());
    BOOST_CHECK_EQUAL(0, spaceCount(other_session));

    constexpr auto spacePath = "GOM/Block2";
    const auto spaceUri = EmlUri::make_space(spacePath);
    const auto txUid = startTx(spaceUri.to_string(), false); // read-write
    const auto space = putSpace(spacePath);
    BOOST_CHECK(!space.m_uri.empty());

    // *before* commit, only the session creating it sees it.
    BOOST_CHECK_EQUAL(1, spaceCount());
    BOOST_CHECK_EQUAL(0, spaceCount(other_session));

    bool txOK1 = commitTx(txUid);
    BOOST_CHECK(txOK1);

    // *after* commit, both sessions see it.
    BOOST_CHECK_EQUAL(1, spaceCount());
    BOOST_CHECK_EQUAL(1, spaceCount(other_session));
}

BOOST_AUTO_TEST_CASE(test_Transaction18_isolation_multi_spaces)
{
    std::shared_ptr<OpenKVSession> other_session = std::make_shared<OpenKVSession>();
    constexpr auto space1Path = "GOM/Block1", space2Path = "GOM/Block2";
    // Passing empty list of dataspaces should allow to include all dataspaces into transaction
    const auto txUid = startTx(std::vector<std::string> { }, false); // read-write
    const auto space1 = putSpace(space1Path);
    const auto space2 = putSpace(space2Path);
    BOOST_CHECK(!space1.m_uri.empty() && !space2.m_uri.empty());

    // *before* commit, only the session creating it sees it.
    BOOST_CHECK_EQUAL(2, spaceCount());
    BOOST_CHECK_EQUAL(0, spaceCount(other_session));
    BOOST_REQUIRE(commitTx(txUid));

    // *after* commit, both sessions see it.
    BOOST_CHECK_EQUAL(2, spaceCount());
    BOOST_CHECK_EQUAL(2, spaceCount(other_session));
}

BOOST_AUTO_TEST_CASE(test_Transaction18_rollback_multi_spaces)
{
    constexpr auto space1Path = "GOM/Block1", space2Path = "GOM/Block2";
    const auto space1Uri = EmlUri::make_space(space1Path);
    const auto space2Uri = EmlUri::make_space(space2Path);

    // Only specified list of dataspaces should be included into transaction
    const auto txUid = startTx({ space1Uri.to_string(), space2Uri.to_string() }, false); // read-write
    BOOST_CHECK_EQUAL(putSpace(space1Path).m_path, space1Path);
    BOOST_CHECK_EQUAL(putSpace(space2Path).m_path, space2Path);
    // *before* rollback, created data spaces should be visible
    BOOST_CHECK_EQUAL(2, spaceCount());

    BOOST_REQUIRE(rollbackTx(txUid));
    BOOST_CHECK_EQUAL(0, spaceCount());
}

BOOST_AUTO_TEST_CASE(test_Transaction18_reject_put_not_specified_space)
{
    // Arrange
    constexpr auto space1Path = "GOM/Block1", space2Path = "GOM/Block2";
    const auto space1Uri = EmlUri::make_space(space1Path);
    const auto space2Uri = EmlUri::make_space(space2Path);
    const auto txUid = startTx(space1Uri.to_string(), false);

    // Act / Assert
    BOOST_CHECK_EQUAL(putSpace(space1Path).m_path, space1Path);
    // Spaces that are not specified in `StartTransaction` list should be rejected
    BOOST_CHECK_EXCEPTION(putSpace(space2Path, std::weak_ptr<OpenKVSession>(), true), proto12::EtpResponseStatus, [space2Path](const proto12::EtpResponseStatus& ex)
    {
        const auto errIt = ex.ex.m_errors.find(space2Path);
        BOOST_REQUIRE(errIt != ex.ex.m_errors.cend());
        BOOST_CHECK_EQUAL(errIt->second.m_code, EREQUEST_DENIED);
        BOOST_CHECK_EQUAL(errIt->second.m_message, "Writing to dataspaces not specified in `StartTransaction` is not allowed");
        return true;
    });
    BOOST_REQUIRE(commitTx(txUid));
    // Only one space should be created
    BOOST_CHECK_EQUAL(1, spaceCount());
}

BOOST_AUTO_TEST_CASE(test_Transaction18_reject_delete_not_specified_space)
{
    // Arrange
    constexpr auto space1Path = "GOM/Block1", space2Path = "GOM/Block2";
    const auto space1Uri = EmlUri::make_space(space1Path);
    const auto space2Uri = EmlUri::make_space(space2Path);
    BOOST_CHECK_EQUAL(putSpace(space1Path).m_path, space1Path);
    BOOST_CHECK_EQUAL(putSpace(space2Path).m_path, space2Path);

    // Act / Assert
    const auto txUid = startTx(space1Uri.to_string(), false);
    // Spaces that are not specified in `StartTransaction` list should be rejected

    BOOST_CHECK(deleteSpace(space1Path));
    BOOST_CHECK_EQUAL(1, spaceCount());
    BOOST_CHECK_EXCEPTION(deleteSpace(space2Path, std::weak_ptr<OpenKVSession>(), true), proto12::EtpResponseStatus, [space2Path](const proto12::EtpResponseStatus& ex)
    {
        const auto errIt = ex.ex.m_errors.find(space2Path);
        BOOST_REQUIRE(errIt != ex.ex.m_errors.cend());
        BOOST_CHECK_EQUAL(errIt->second.m_code, EREQUEST_DENIED);
        BOOST_CHECK_EQUAL(errIt->second.m_message, "Writing to dataspaces not specified in `StartTransaction` is not allowed");
        return true;
    });
    BOOST_REQUIRE(rollbackTx(txUid));
    // Only one of two spaces should be deleted
    BOOST_CHECK_EQUAL(2, spaceCount());
}

BOOST_AUTO_TEST_CASE(test_Transaction18_reject_write_to_not_specified_space)
{
    // Arrange
    constexpr auto space1Path = "GOM/Block1", space2Path = "GOM/Block2";
    const auto space1Uri = EmlUri::make_space(space1Path);
    const auto space2Uri = EmlUri::make_space(space2Path);
    BOOST_CHECK_EQUAL(putSpace(space1Path).m_path, space1Path);
    BOOST_CHECK_EQUAL(putSpace(space2Path).m_path, space2Path);

    auto putStoreObjectsRpc = makeRpc(&openkv::OpenKVStore::PutDataObjects);
    const auto& crs = oes::testing::eml::crs_mock();
    const EmlUri crsUri = space2Uri.child_type(crs.type_, EmlUri::eRESQML20).child_obj(crs.guid_);
    data12::Object::DataObject crsObj;
    crsObj.m_resource.m_uri = crsUri.to_string();
    crsObj.m_resource.m_name = crs.name_;
    crsObj.m_format = "xml";
    crsObj.m_data.assign(crs.xml_.cbegin(), crs.xml_.cend());
    putStoreObjectsRpc.req_.m_dataObjects.emplace(space2Path, std::move(crsObj));

    // Act
    startTx(space1Uri.to_string(), false);
    putStoreObjectsRpc.call();

    // Assert
    BOOST_CHECK(putStoreObjectsRpc.rc_.has_error());
    const auto errIt = putStoreObjectsRpc.rc_.ex.m_errors.find(space2Path);
    BOOST_CHECK_EQUAL(errIt->second.m_code, EREQUEST_DENIED);
    BOOST_CHECK_EQUAL(errIt->second.m_message, "Writing to dataspaces not specified in `StartTransaction` is not allowed");
}

BOOST_AUTO_TEST_CASE(test_Transaction18_isolation_store_proto) {
    std::shared_ptr<OpenKVSession> other_session = std::make_shared<OpenKVSession>();

    // we need a space to put into
    auto space = putSpace();

    // we start w/o any spaces
    BOOST_CHECK_EQUAL(0, objCount(space));
    BOOST_CHECK_EQUAL(0, objCount(space, other_session));

    auto txUid = startTx(space.m_uri, false); // read-write
    auto crs = putCrsObj(space);
    BOOST_CHECK(!crs.m_resource.m_uri.empty());

    // *before* commit, only the session creating it sees it.
    BOOST_CHECK_EQUAL(1, objCount(space));
    BOOST_CHECK_EQUAL(0, objCount(space, other_session));

    bool txOK1 = commitTx(txUid);
    BOOST_CHECK(txOK1);

    // *after* commit, both sessions see it.
    BOOST_CHECK_EQUAL(1, objCount(space));
    BOOST_CHECK_EQUAL(1, objCount(space, other_session));
}

// Note that this test also tests DataArray's GetDataArrayMetadata extension
// (i.e. ListDataArrayMetadata) as a side-effect, and the fact PutDataObjects
// AFTER PutDataArrays properly reconnects arrays to their owner objects.
BOOST_AUTO_TEST_CASE(test_Transaction18_isolation_array_proto) {
    std::shared_ptr<OpenKVSession> other_session = std::make_shared<OpenKVSession>();

    // we need a space to put into
    auto space = putSpace();

    // we start w/o any spaces
    BOOST_CHECK_EQUAL(0, aryCount(space));
    BOOST_CHECK_EQUAL(0, aryCount(space, other_session));

    auto txUid = startTx(space.m_uri, false); // read-write
    auto grid2d = putGrid2dAry(space);

    // *before* commit, only the session creating it sees it.
    BOOST_CHECK_EQUAL(1, aryCount(space));
    BOOST_CHECK_EQUAL(0, aryCount(space, other_session));

    // The array is an "Orphan", it's owner obj does not exist,
    // thus our extension associates it to a "generic" HDF5-file-like object
    oes::common::etp12::AryInfoVec arys = allAryMeta(space);
    BOOST_REQUIRE_EQUAL(1, arys.size());
    EmlUri ary0_owner = EmlUri::parse(arys[0].ident_.m_uri);
    BOOST_CHECK_EQUAL(ary0_owner.obj_type_, "obj_EpcExternalPartReference");
    if (grid2d.m_uid.m_pathInResource[0] == '/') {
        grid2d.m_uid.m_pathInResource.erase(0, 1); // always returns w/o / by OpenKV
    }
    BOOST_CHECK_EQUAL(arys[0].ident_.m_pathInResource, grid2d.m_uid.m_pathInResource);

    // Add the missing owner-obj of above's ary (in the same TX)
    // Must also add the CRS, since Grid2D has reference to it. Fails otherwise, with
    // Error: Relation not found: obj(1) xpa(1): 1c8bb605-c8e7-4da9-af4c-52e5a5e5147b
    auto crs_obj = putCrsObj(space);
    BOOST_CHECK(!crs_obj.m_resource.m_uri.empty());
    auto grid2d_obj = putGrid2dObj(space);
    BOOST_CHECK(!grid2d_obj.m_resource.m_uri.empty());

    // Now the array is properly associated to its owner
    arys = allAryMeta(space);
    BOOST_REQUIRE_EQUAL(1, arys.size());
    BOOST_CHECK_EQUAL(arys[0].ident_.m_uri, grid2d_obj.m_resource.m_uri);
    BOOST_CHECK_EQUAL(arys[0].ident_.m_pathInResource, grid2d.m_uid.m_pathInResource);

    bool txOK = commitTx(txUid);
    BOOST_CHECK(txOK);

    // *after* commit, both sessions see it.
    BOOST_CHECK_EQUAL(1, aryCount(space));

    // create std::weak_ptr from other_session
    std::weak_ptr<OpenKVSession> other_session_weak = other_session;
    BOOST_CHECK_EQUAL(1, aryCount(space, other_session_weak));
}

// TODO: Same as test_Transaction18_isolation_array_proto() above, but does not add
//       the owner-object in the same transaction. Should fail at commit time.

// TODO: Same as test_Transaction18_isolation_array_proto() above, but PUT the
//       owner-object BEFORE the array itself. Should be OK, but KO right now.

BOOST_AUTO_TEST_SUITE_END() // TransProtoTests

//============================================================================

} // End anonymous namespace
