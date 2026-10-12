// ============================================================================
// Copyright 2022-2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
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

#pragma once
#include "../fwd.h"
#include "../export.h"
#include "../OpenETPServerConfig.h"
#include <oes/postgresql/PhysicalName.h>

namespace oes::eml::openkv
{
    struct Space;

    class OES_EML_EXPORT OpenKVRepo
    {
    public:
        OpenKVRepo(std::weak_ptr<OpenKVSession> session, DbConnectionPool_ptr dbConnectionPool, oes::eml::openETPServerConfig_ptr app_config);

        Connection_ptr getDbConnection();
        OpenKVAdminDB_uptr& getAdminDb();
        const oes::eml::openETPServerConfig_ptr& getServerConfig() const { return server_config_; }

        SpaceReader_ptr getSpaceReader(const Space& space, ErrorInfo& err);
        SpaceWriter_ptr getSpaceWriter(const Space& space, ErrorInfo& err);

        // ToDo: consider to extract these methods to separate class like SpaceManager (with lazy instantiation like as `adminDb_`)
        //Connection_ptr findSpaceByUri(const std::string& uri, bool expect_space_uri, EtpResponseStatus& rc);
        Space          findSpaceInfo(const std::string& uri_or_path);
        Connection_ptr getSpaceConnection(const Space& space, ErrorInfo& err);
        Connection_ptr switchCurrentSpace(const Space& space, ErrorInfo& err);
        bool checkSpaceSchemaVersion(const Space& space, ErrorInfo& err);
        bool upgradeSpaceSchemaVersion(const std::string& dbFile, ErrorInfo& err, int current_version, int expected_version);
        bool updateSpaceInfo(const Space& space);
        static int getSchemaVersion(const Connection_ptr& conn, const std::string& dbFile);
        [[nodiscard]] bool checkIsSpaceInvolvedInTheTransaction(const EmlUri& space) const;

    private:
        [[nodiscard]] std::string resolveUserPartition() const;
        [[nodiscard]] Connection_ptr acquireDbConnection(const std::string& dbKey) const;

        std::weak_ptr<OpenKVSession> session_;
        DbConnectionPool_ptr dbConnectionPool_;
        Connection_ptr dbConnection_;
        OpenKVAdminDB_uptr adminDb_;
        oes::eml::openETPServerConfig_ptr server_config_;
    };
} // namespace oes::eml::openkv
