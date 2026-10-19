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

#include <random>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/eml/ServerFixture.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/LoggerTestTools.h>
#include <oes/testing/eml/ServerTesting.hpp>

#include <oes/core/utils/Guid.h>
#include <oes/core/utils/ScopedUtils.h>

#include <oes/postgresql/DbConnectionPool.h>
#include <oes/eml/openkv/ServerConfig.h>
#include <oes/eml/etp12/MsgUtils.h>
#include <oes/eml/openkv/ArrayProto.h>
#include <oes/eml/openkv/SpaceProto.h>
#include <oes/eml/openkv/StoreProto.h>
#include <oes/eml/openkv/TransProto.h>
#include <oes/eml/openkv/TypesProto.h>


namespace {

void assignUid(std::array<uint8_t, 16>& lhs, const oes::core::Guid& rhs) {
    std::memcpy(lhs.data(), rhs.begin(), oes::core::Guid::static_size());
}

}

namespace oes::testing::eml {

//============================================================================

//using namespace etp12;
using namespace openkv;
using namespace data12;

//============================================================================

OpenKVServerFixture::OpenKVServerFixture()
    : connection_pool_(std::make_shared<postgresql::DbConnectionPool>(100))
{
    const char* connection_string = std::getenv("POSTGRESQL_CONN_STRING");
    // TODO: unused ptr check
    BOOST_CHECK(connection_string!=nullptr);
    const std::string connection_str(connection_string);
    BOOST_CHECK(!connection_str.empty());

    const bool isRegistered = connection_pool_->registerDatabase("single", connection_str, true);
    BOOST_CHECK_MESSAGE(isRegistered, "Error: Single Database mode cannot connect");

    auto dbConnection = connection_pool_->tryAcquireConnection("single");
    admin_db_ = std::make_shared<OpenKVAdminDB>(dbConnection);

    bool schema_ok = false;
    if (!admin_db_->isSchemaCreated()) {
        schema_ok = admin_db_->createSchema();
    } else {
        schema_ok = true;
    }
    BOOST_REQUIRE(schema_ok);

    const std::shared_ptr<OpenKVSession> sess = std::make_shared<OpenKVSession>();
    sess->set_negotiated_max_msg_size(1ull<<31); // i.e. 2GB
    session_ = sess;
    session_->check_activities_ = false;
    session_->pg_bulk_size_ = 100;

    BOOST_REQUIRE_EQUAL(0, spaceCount());
}

OpenKVServerFixture::~OpenKVServerFixture() {
    // Rollback not completed ETP transaction
    if (session_ && session_->tx_ && session_->tx_->db_transaction_.getConnection()->transaction_status() == postgresql::TransactionStatus::eActive) {
        session_->tx_->db_transaction_.rollback();
    }

    for (const auto& space: spaces_) {
        clearDataSpace(space);
    }
    clearAdmin();
}

void OpenKVServerFixture::clearAdmin(){
    if( admin_db_ ){
        admin_db_->dropAdmin();
    }
}

void OpenKVServerFixture::clearDataSpace(const std::string& space_path) {
    if (!space_path.empty() )
    {
        std::string folder, name;
        std::string out_path, db_path, err_msg;
        const bool path_ok = Space::validatePath(space_path, out_path, db_path, folder, name, err_msg);
        if (path_ok)
        {
            const oes::postgresql::relational::PhysicalName phys(db_path, oes::postgresql::relational::IMPLICIT_LOWERCASE);
            admin_db_->handle()->execute(fmt::format("DROP SCHEMA IF EXISTS {} CASCADE", phys.quotedName()));
        }
        else {
            OES_WARNING("Unable to cleanup test space '{}': {}", space_path, err_msg);
        }
    }
}

bool OpenKVServerFixture::hasError(const EtpResponseStatus& rc) {
    return !rc.ok || rc.ex.m_error || !rc.ex.m_errors.empty();
}

std::string OpenKVServerFixture::errorOf(const EtpResponseStatus& rc) {
    if (rc.ex.m_error) {
        return rc.ex.m_error->m_message;
    }
    if (!rc.ex.m_errors.empty()) {
        return rc.ex.m_errors.begin()->second.m_message;
    }
    BOOST_FAIL("Expected error message");
    return {};
}

void OpenKVServerFixture::importEpc(
    const std::string& rel_path,
    const std::string& file_name,
    const std::string& space_path
) {
    const std::string& epc = oes::testing::resolveTestDataFile(
        rel_path, file_name, true
    );

    std::string db_path;
    if (!space_path.empty()) {
        std::string folder, name, path, err_msg;
        const bool path_ok = openkv::Space::validatePath(
            space_path,
            path, db_path, folder, name, err_msg // outputs
        );
        BOOST_REQUIRE(path_ok);
    } else {
        db_path = "EmrEtp12DefaultSpace.db";
    }

    const fs::path out_db = admin_folder_ / db_path;
    fs::create_directory(out_db.parent_path()); // OK if already exists

    fs::path hdf_path(epc);
    hdf_path.replace_extension(".h5");
    fs::file_status hdf_stat = fs::status(hdf_path);
    BOOST_REQUIRE(fs::is_regular_file(hdf_stat));
}

size_t OpenKVServerFixture::spaceCount(std::weak_ptr<OpenKVSession> sess) {
    auto get_space_rpc = makeRpc(&OpenKVDataspace::GetDataspaces, sess);
    if (!get_space_rpc.call()) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_space_rpc.rc_));
    }
    return get_space_rpc.res_.m_dataspaces.size();
}

data12::Object::Dataspace OpenKVServerFixture::putSpace(
    const std::string& path,
    std::weak_ptr<OpenKVSession> sess,
    bool throw_expected_failure
) {
    auto put_space_rpc = makeRpc(&OpenKVDataspace::PutDataspaces, sess);

    const size_t before = spaceCount();

    data12::Object::Dataspace space;
    eml_db_filename_ = path;
    EmlUri space_uri = EmlUri::make_space(eml_db_filename_);
    space.m_uri = space_uri.to_string();
    space.m_path = path;
    spaces_.push_back(space.m_path);

    // GMT: Thursday, April 18, 2019 2:45:55 AM
    space.m_storeLastWrite = 1555555555; // secs since EPOCH
    xdataEmplace(space.m_customData, "key", "value");

    put_space_rpc.req_.m_dataspaces.emplace(space.m_path, space);
    if (throw_expected_failure) {
        if (!put_space_rpc.call()) {
            throw put_space_rpc.rc_;
        }
    } else {
        if (!put_space_rpc.call()) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(put_space_rpc.rc_));
        }
    }

    BOOST_REQUIRE_EQUAL(before + 1, spaceCount());

    return space;
}

bool OpenKVServerFixture::deleteSpace(const std::string& path, std::weak_ptr<OpenKVSession> sess, bool throw_expected_failure)
{
    auto delete_space_rpc = makeRpc(&OpenKVDataspace::DeleteDataspaces, sess);
    delete_space_rpc.req_.m_uris.emplace(path, EmlUri::make_space(path).to_string());
    if (throw_expected_failure) {
        if (!delete_space_rpc.call()) {
            throw delete_space_rpc.rc_;
        }
    }
    else {
        if (!delete_space_rpc.call()) {
            return false;
        }
    }
    return true;
}

Guid OpenKVServerFixture::startTx(std::string space_uri, bool readOnly, std::weak_ptr<OpenKVSession> sess, bool failure_expected)
{
    return startTx(std::vector { std::move(space_uri) }, readOnly, sess, failure_expected);
}

Guid OpenKVServerFixture::startTx(std::vector<std::string> space_uris, bool readOnly, std::weak_ptr<OpenKVSession> sess, bool failure_expected)
{
    auto rpcBegin = makeRpc(&OpenKVTransProto::StartTransaction, sess);

    static int tx_count__ = 0;
    rpcBegin.req_.m_readOnly = readOnly;
    rpcBegin.req_.m_message = fmt::format(
        "{} Transaction#{}",
        oes::testing::test_case_name(), ++tx_count__
    );
    rpcBegin.req_.m_dataspaceUris = std::move(space_uris);
    if (!rpcBegin.call()) {
        if (!failure_expected) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(rpcBegin.rc_));
        }else{
            return Guid();
        }
    }
    Guid txUid = Guid::from(rpcBegin.res_.m_transactionUuid);
    return txUid;
}

bool OpenKVServerFixture::commitTx(Guid txUid, std::weak_ptr<OpenKVSession> sess) {
    return commitTx(txUid, false, sess);
}

bool OpenKVServerFixture::commitTx(Guid txUid, bool failure_expected, std::weak_ptr<OpenKVSession> sess) {
    auto rpcCommit = makeRpc(&OpenKVTransProto::CommitTransaction, sess);
    assignUid(rpcCommit.req_.m_transactionUuid, txUid);
    if (!rpcCommit.call()) {
        if (!failure_expected) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(rpcCommit.rc_));
        }
    }
    return rpcCommit.res_.m_successful;
}

bool OpenKVServerFixture::rollbackTx(Guid txUid, std::weak_ptr<OpenKVSession> sess) {
    auto rpcRollback = makeRpc(&OpenKVTransProto::RollbackTransaction, sess);
    assignUid(rpcRollback.req_.m_transactionUuid, txUid);
    if (!rpcRollback.call()) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(rpcRollback.rc_));
    }
    return rpcRollback.res_.m_successful;
}

/*
** Will return a map similar to this:
std::map<std::string, int32_t> expected_counts{
    { "eml20.EpcExternalPartReference", 1 },
    { "resqml20.obj_LocalDepth3dCrs", 1 }, ...
};
*/
std::map<std::string, int32_t> OpenKVServerFixture::objCounts(const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess) {
    auto get_types_rpc = makeRpc(&OpenKVSupportedTypes::GetSupportedTypes, sess);
    get_types_rpc.req_.m_uri = space.m_uri;
    get_types_rpc.req_.m_returnEmptyTypes = false;
    get_types_rpc.req_.m_countObjects = true;
    get_types_rpc.req_.m_scope = Object::ContextScopeKind::eSelf;

    if (!get_types_rpc.call()) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_types_rpc.rc_));
    }

    std::map<std::string, int32_t> counts;
    for (const Object::SupportedType& supp_type : get_types_rpc.res_.m_supportedTypes) {
        BOOST_REQUIRE(supp_type.m_objectCount);
        const int32_t count = *supp_type.m_objectCount;
        BOOST_REQUIRE(count > 0);
        counts.emplace(supp_type.m_dataObjectType, count);
    }
    return counts;
}

size_t OpenKVServerFixture::objCount(const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess) {
    size_t total = 0;
    for (const auto& entry : objCounts(space, sess)) {
        total += size_t(entry.second);
    }
    return total;
}

// WARNING: Custom EXTENSION. Not official ETP 1.2
oes::common::etp12::AryInfoVec OpenKVServerFixture::allAryMeta(
    const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess
) {
    auto get_types_rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata, sess);

    get_types_rpc.req_ = DataArrayExt::to_list_arrays_req(EmlUri::parse(space.m_uri).dataspace_);

    if (!get_types_rpc.call()) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_types_rpc.rc_));
    }

    return DataArrayExt::move_to_info_vec(get_types_rpc.res_.m_arrayMetadata);
}

size_t OpenKVServerFixture::aryCount(const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess) {
    return allAryMeta(space, sess).size();
}

data12::Object::DataObject OpenKVServerFixture::putMockObj(
    const data12::Object::Dataspace& space,
    const oes::testing::eml::EmlObjMock& mock,
    std::weak_ptr<OpenKVSession> sess
) {
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects, sess);

    EmlUri space_uri = EmlUri::parse(space.m_uri);
    auto make_obj_uri = [&space_uri](const std::string& resqml_type, const Guid& uid) {
        return space_uri.child_type(resqml_type, EmlUri::eRESQML20).child_obj(uid);
    };

    const EmlUri obj_uri = make_obj_uri(mock.type_, mock.guid_);
    data12::Object::DataObject obj;
    obj.m_resource.m_uri = obj_uri.to_string();
    obj.m_resource.m_name = mock.name_;
    xdataEmplace(obj.m_resource.m_customData, "key", "val");
    obj.m_format = "xml";
    obj.m_data.assign(mock.xml_.begin(), mock.xml_.end());
    put_store_rpc.req_.m_dataObjects.emplace("mock", obj);

    if (!put_store_rpc.call() || hasError(put_store_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_store_rpc.rc_));
    }
    return obj;
}

//! \warning Can be called only once per space
data12::Object::DataObject OpenKVServerFixture::putCrsObj(
    const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess
) {
    return putMockObj(space, oes::testing::eml::crs_mock(), sess);
}

//! \warning Can be called only once per space
//! \warning Requires putCrs() first (in same space), to avoid dangling relation
data12::Object::DataObject OpenKVServerFixture::putDatumObj(
    const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess
) {
    return putMockObj(space, oes::testing::eml::datum_mock(), sess);
}

data12::Object::DataObject OpenKVServerFixture::putGrid2dObj(
    const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess
) {
    return putMockObj(space, oes::testing::eml::grid2d_mock(), sess);
}

data12::DataArrayTypes::PutDataArraysType OpenKVServerFixture::putMockAry(
    const data12::Object::Dataspace& space,
    const std::string& owner_uri,
    const oes::testing::eml::EmlAryMock& mock_ary,
    std::weak_ptr<OpenKVSession> sess
) {
    BOOST_REQUIRE(mock_ary.init_);

    auto put_array_rpc = makeRpc(&OpenKVDataArray::PutDataArrays, sess);

    data12::DataArrayTypes::PutDataArraysType ary;
    if (isExtPartRefUri(owner_uri)) {
        ary.m_uid.m_uri = owner_uri; // the "owner" of the data must be an ExtPartRef
    } else {
        EmlUri owner = EmlUri::parse(owner_uri);
        BOOST_REQUIRE(owner.isObj());

        // Creating HDF container
        const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
        putMockObj(space, mock_hdf);
        const EmlUri epc_uri = EmlUri::make_obj(
            owner.dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
        );
        ary.m_uid.m_uri = epc_uri.to_string();
    }
    ary.m_uid.m_pathInResource = mock_ary.path_;
    ary.m_array.m_dimensions = mock_ary.dims_;
    mock_ary.init_(ary.m_array.m_data.m_item);
    put_array_rpc.req_.m_dataArrays.emplace("ary#1", ary);

    if (!put_array_rpc.call() || hasError(put_array_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_array_rpc.rc_));
    }
    return ary;
}

data12::DataArrayTypes::PutUninitializedDataArrayType OpenKVServerFixture::putMockAryInChunks(
    const std::string& owner_uri, int chunk_cnt,
    const oes::testing::eml::EmlAryMock& mock_ary,
    ChunkOrder chunk_order,
    ChunkChecker chunk_checker,
    std::weak_ptr<OpenKVSession> sess
) {
    BOOST_REQUIRE(mock_ary.init_);
    BOOST_REQUIRE(mock_ary.rank() == 1); // For now
    BOOST_REQUIRE(mock_ary.dims_[0] > chunk_cnt);
    BOOST_REQUIRE(mock_ary.type_ == Datatypes::AnyArrayType::eArrayOfDouble); // For now

    auto put_ini_rpc = makeRpc(&OpenKVDataArray::PutUninitializedDataArrays, sess);
    auto put_sub_rpc = makeRpc(&OpenKVDataArray::PutDataSubarrays, sess);

    // helper
    const std::vector<double>& values = std::get<ArrayOfDouble>(mock_ary.data()).m_values;
    auto set_subarray = [&values](AnyArray_item_v& sub_array, int64_t start, int64_t count) {
        ArrayOfDouble ary_of;
        const double* p_subvalues = &values[start];
        ary_of.m_values.assign(p_subvalues, p_subvalues + count);
        sub_array = std::move(ary_of);
    };

    // First declare it
    data12::DataArrayTypes::PutUninitializedDataArrayType ary;
    ary.m_uid.m_uri = owner_uri;
    ary.m_uid.m_pathInResource = mock_ary.path_;
    ary.m_metadata.m_dimensions = mock_ary.dims_;
    ary.m_metadata.m_transportArrayType = mock_ary.type_;
    BOOST_CHECK_EQUAL(int(mock_ary.type_), int(EmlAryMock::type_of(mock_ary.data())));

    put_ini_rpc.req_.m_dataArrays.emplace("ary#1", ary);
    if (!put_ini_rpc.call() || hasError(put_ini_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_ini_rpc.rc_));
    }

    // Compute indices of chunks
    const int64_t size = mock_ary.size();
    const int chunk_len = int((size + chunk_cnt - 1) / chunk_cnt);
    std::vector<int64_t> chunk_indices(chunk_cnt);
    for (int64_t i = 0; i < chunk_cnt; ++i) {
        chunk_indices[i] = i;
    }

    switch (chunk_order) {
    case ChunkOrder::eAscending:
        break;
    case ChunkOrder::eDescending:
        std::reverse(chunk_indices.begin(), chunk_indices.end());
        break;
    case ChunkOrder::eRandom: {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::shuffle(chunk_indices.begin(), chunk_indices.end(), mt);
        }
    }

    // then send it in chunks
    for (int64_t i : chunk_indices) {
        Datatypes::DataArrayTypes::PutDataSubarraysType part;
        part.m_uid = ary.m_uid;
        part.m_starts = { i * chunk_len };
        part.m_counts = { std::min<int64_t>(chunk_len, size - part.m_starts[0]) };
        set_subarray(part.m_data.m_item, part.m_starts[0], part.m_counts[0]);
        put_sub_rpc.req_.m_dataSubarrays.emplace("part#" + std::to_string(i), std::move(part));
        if (!put_sub_rpc.call() || hasError(put_sub_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(put_sub_rpc.rc_));
        }
        put_sub_rpc.req_.m_dataSubarrays.clear();

        if (chunk_checker) {
            chunk_checker(ary.m_uid, i == chunk_indices.back());
        }
    }

    return ary;
}

bool OpenKVServerFixture::fillAryInChunks(
    const AryIdent& ary_ident, int chunk_cnt,
    Datatypes::DataArrayTypes::DataArray& ary,
    ChunkOrder chunk_order,
    std::weak_ptr<OpenKVSession> sess
) {
    BOOST_REQUIRE(ary.m_dimensions.size() == 1); // For now
    BOOST_REQUIRE(ary.m_dimensions[0] > chunk_cnt);
    BOOST_REQUIRE(ary.m_data.m_item.index() == size_t(AnyArray_item_e::eArrayOfBoolean)); // i.e. "null"

    auto get_sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays, sess);

    // Compute indices of chunks
    const int64_t size = ary.m_dimensions[0];
    const int chunk_len = int((size + chunk_cnt - 1) / chunk_cnt);
    std::vector<int64_t> chunk_indices(chunk_cnt);
    for (int64_t i = 0; i < chunk_cnt; ++i) {
        chunk_indices[i] = i;
    }

    switch (chunk_order) {
    case ChunkOrder::eAscending:
        break;
    case ChunkOrder::eDescending:
        std::reverse(chunk_indices.begin(), chunk_indices.end());
        break;
    case ChunkOrder::eRandom: {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::shuffle(chunk_indices.begin(), chunk_indices.end(), mt);
        }
    }

    ArrayOfDouble ary_of;
    std::vector<double>& vec = ary_of.m_values;
    vec.resize(size);

    for (int64_t i : chunk_indices) {
        Datatypes::DataArrayTypes::GetDataSubarraysType part;
        part.m_uid = ary_ident;
        part.m_starts = { i * chunk_len };
        part.m_counts = { std::min<int64_t>(chunk_len, size - part.m_starts[0]) };

        get_sub_rpc.req_.m_dataSubarrays.emplace("part#" + std::to_string(i), part);
        if (!get_sub_rpc.call() || hasError(get_sub_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_sub_rpc.rc_));
        }
        BOOST_REQUIRE_EQUAL(1, get_sub_rpc.res_.m_dataSubarrays.size());
        const auto& sub = std::get<ArrayOfDouble>(
            get_sub_rpc.res_.m_dataSubarrays.begin()->second.m_data.m_item
        ).m_values;

        // Copy "chunk" into "whole" array
        std::copy(
            sub.begin(), sub.end(),
            vec.begin() + part.m_starts[0]
        );

        get_sub_rpc.req_.m_dataSubarrays.clear();
    }

    ary.m_data.m_item = std::move(ary_of);
    return true;
}

data12::DataArrayTypes::PutDataArraysType OpenKVServerFixture::putMockAry(
    const data12::Object::Dataspace& space,
    const oes::testing::eml::EmlObjMock& mock_obj,
    const oes::testing::eml::EmlAryMock& mock_ary,
    std::weak_ptr<OpenKVSession> sess
) {
    EmlUri space_uri = EmlUri::parse(space.m_uri);
    auto make_obj_uri = [&space_uri](const std::string& resqml_type, const Guid& uid) {
        return space_uri.child_type(resqml_type, EmlUri::eRESQML20).child_obj(uid);
    };

    const EmlUri grid2d_uri = make_obj_uri(mock_obj.type_, mock_obj.guid_);
    return putMockAry(space, grid2d_uri.to_string(), mock_ary, sess);
}

data12::DataArrayTypes::PutDataArraysType OpenKVServerFixture::putGrid2dAry(
    const data12::Object::Dataspace& space, std::weak_ptr<OpenKVSession> sess
) {
    const auto& grid2d = oes::testing::eml::grid2d_mock();
    BOOST_REQUIRE(!grid2d.arrays_.empty());
    const auto& zvalues = grid2d.arrays_[0];

    return putMockAry(space, grid2d, zvalues, sess);
}

data12::DataArrayTypes::PutDataArraysType OpenKVServerFixture::putResqml22Ary(
    const data12::Object::Dataspace& space,
    const oes::testing::eml::EmlObjMock& mock_obj,
    std::weak_ptr<OpenKVSession> sess
) {
    BOOST_REQUIRE(!mock_obj.arrays_.empty());
    const auto& mock_ary = mock_obj.arrays_.front();

    auto put_array_rpc = makeRpc(&OpenKVDataArray::PutDataArrays, sess);

    data12::DataArrayTypes::PutDataArraysType ary;
    ary.m_uid.m_uri = EmlUri::parse(space.m_uri)
        .child_type(mock_obj.type_, EmlUri::eRESQML22).child_obj(mock_obj.guid_).to_string();
    ary.m_uid.m_pathInResource = mock_ary.path_;
    ary.m_array.m_dimensions = mock_ary.dims_;
    mock_ary.init_(ary.m_array.m_data.m_item);
    put_array_rpc.req_.m_dataArrays.emplace("ary#1", ary);

    if (!put_array_rpc.call() || hasError(put_array_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_array_rpc.rc_));
    }
    return ary;
}

bool OpenKVServerFixture::isExtPartRefUri(const std::string& uri) const {
    // Note that we omit the obj_ prefix, to accept even a "not quite legal" one,
    // at least for RESQML 2.0.1. RESQML 2.2 seems to have dropped all obj_ prefixes.
    return uri.find("EpcExternalPartReference") != std::string::npos;
}

//============================================================================

} // namespace oes::testing::eml
