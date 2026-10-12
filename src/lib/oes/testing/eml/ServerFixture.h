#pragma once
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

#include "export.h"
#include "TestingMocks.h"
#include "ServerTesting.hpp"

#include <oes/common/etp12/Messages.h>
#include <oes/common/etp12/Protocols.h>

#include <oes/eml/fwd.h>
#include <oes/core/utils/fwd.h>

#include <oes/eml/openkv/AdminDB.h>
#include <oes/eml/openkv/Session.h>

#include <oes/eml/etp12/Extensions.h>

#include <map>
#include <string>
#include <filesystem>

namespace oes::testing::eml {

//============================================================================

namespace fs = std::filesystem;
namespace data12 = oes::common::etp12::Datatypes;
namespace proto12 = oes::common::etp12::Protocol;

using oes::core::Guid;
using namespace oes::eml;
using proto12::EtpResponseStatus;
using openkv::OpenKVSession;

using AryIdent = data12::DataArrayTypes::DataArrayIdentifier;

//============================================================================

struct OES_TESTING_EML_EXPORT OpenKVServerFixture {
    std::string admin_path_;
    fs::path admin_folder_;
    std::string dbfile_;
    openkv::Connection_ptr admin_conn_;
    openkv::OpenKVAdminDB_ptr admin_db_;
    openkv::DbConnectionPool_ptr connection_pool_;
    openkv::OpenKVSpaceResolver etp_space_resolver_;
    std::shared_ptr<OpenKVSession> session_;
    std::string eml_db_filename_;
    std::map<std::string, std::string> space_to_ext_uri_;

    std::vector<std::string> spaces_;

public:
    OpenKVServerFixture();

    ~OpenKVServerFixture();
    void clearAdmin();
    void clearDataSpace(const std::string& space_path);

    bool hasError(const EtpResponseStatus& rc);
    std::string errorOf(const EtpResponseStatus& rc);

    template <typename PROTO, typename REQ, typename RES, typename UDT = UserDataType>
    auto makeRpc(void (PROTO::* msg)(IRequest& ireq, REQ req, EtpResponseHandler<RES> processCb), std::weak_ptr<OpenKVSession> customSession = std::weak_ptr<OpenKVSession>(), UDT udt = UDT()) {
        std::weak_ptr<OpenKVSession> useSession = customSession.lock() ? customSession : session_;
        return TestRetRPC<PROTO, REQ, RES, UDT>(connection_pool_, useSession, msg, udt);
    }

    template <typename PROTO, typename REQ, typename UDT = UserDataType>
    auto makeRpc(void (PROTO::* msg)(IRequest& ireq, REQ req, EtpCompleteHandler doneCb), std::weak_ptr<OpenKVSession> customSession = std::weak_ptr<OpenKVSession>(), UDT udt = UDT()) {
        std::weak_ptr<OpenKVSession> useSession = customSession.lock() ? customSession : session_;
        return TestVoidRPC<PROTO, REQ, UDT>(connection_pool_, useSession, msg, udt);
    }

    void importEpc(
        const std::string& rel_path,
        const std::string& file_name,
        const std::string& space_path
    );

    // Spaces
    size_t spaceCount(std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>());
    data12::Object::Dataspace putSpace(
        const std::string& path = "GOM/Block2",
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>(),
        bool throw_expected_failure = false
    );
    bool deleteSpace(
        const std::string& path = "GOM/Block2",
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>(),
        bool throw_expected_failure = false
    );

    // Transactions
    Guid startTx(
        std::string space_uri, bool readOnly,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>(),
        bool failure_expected = false
    );
    Guid startTx(
        std::vector<std::string> space_uris, bool readOnly,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>(),
        bool failure_expected = false
    );
    bool commitTx(
        Guid txUid,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );
    bool commitTx(
        Guid txUid, bool failure_expected,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );
    bool rollbackTx(
        Guid txUid,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    // Objects
    std::map<std::string, int32_t> objCounts(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );
    size_t objCount(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    // Arrays (WARNING: Custom EXTENSION. Not official ETP 1.2)
    oes::common::etp12::AryInfoVec allAryMeta(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );
    size_t aryCount(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    // Mocks
    data12::Object::DataObject putMockObj(
        const data12::Object::Dataspace& space,
        const oes::testing::eml::EmlObjMock& mock,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );
    data12::Object::DataObject putCrsObj(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );
    data12::Object::DataObject putDatumObj(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );
    data12::Object::DataObject putGrid2dObj(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    data12::DataArrayTypes::PutDataArraysType putMockAry(
        const data12::Object::Dataspace& space,
        const std::string& owner_uri,
        const EmlAryMock& mock_ary,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    enum class ChunkOrder { eAscending, eDescending, eRandom };

    using ChunkChecker = std::function<void(
        const data12::DataArrayTypes::DataArrayIdentifier& ary_ident,
        bool last_chunk
    )>;

    data12::DataArrayTypes::PutUninitializedDataArrayType putMockAryInChunks(
        const std::string& owner_uri, int chunk_cnt,
        const EmlAryMock& mock_ary,
        ChunkOrder chunk_order = ChunkOrder::eAscending,
        ChunkChecker chunk_checker = nullptr,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    bool fillAryInChunks(
        const AryIdent& ary_ident, int chunk_cnt,
        data12::DataArrayTypes::DataArray& ary,
        ChunkOrder chunk_order = ChunkOrder::eAscending,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    data12::DataArrayTypes::PutDataArraysType putMockAry(
        const data12::Object::Dataspace& space,
        const EmlObjMock& mock_obj,
        const EmlAryMock& mock_ary,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    data12::DataArrayTypes::PutDataArraysType putGrid2dAry(
        const data12::Object::Dataspace& space,
        std::weak_ptr<OpenKVSession> sess = std::weak_ptr<OpenKVSession>()
    );

    bool isExtPartRefUri(const std::string& uri) const;
};

//============================================================================

} // namespace oes::testing::eml
