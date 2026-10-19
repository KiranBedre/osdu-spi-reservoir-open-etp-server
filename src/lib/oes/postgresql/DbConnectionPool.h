// ============================================================================
// Copyright 2012-2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <map>

#include "export.h"
#include "fwd.h"
#include "Connection.h"

namespace oes::postgresql
{
    class OES_POSTGRESQL_EXPORT DbConnectionPool
    {
    public:
        static constexpr size_t defaultConnectionsLimit = 90;  // PostgreSql server has 100 connections by default, leaving a few for sysadmins
        using ConnectionParams = std::unordered_map<std::string, PostgresConnectParams>;

        explicit DbConnectionPool(size_t connectionsLimit = defaultConnectionsLimit);

        DbConnectionPool(const DbConnectionPool&) = delete;
        DbConnectionPool& operator=(const DbConnectionPool&) = delete;

        bool registerDatabase(const std::string& dbTag, const std::string& connectionString, bool verify);
        bool registerDatabase(const std::string& dbTag, const PostgresConnectParams& connectionParams, bool verify);
        bool isDatabaseRegistered(const std::string& dbTag) const;
        bool refreshDatabases(const std::map<std::string, PostgresConnectParams>& connectionInfos);
        const ConnectionParams& getRegisteredDatabases() const;

        Connection_ptr tryAcquireConnection(const std::string& dbTag, int64_t correlationId = 0);

        bool checkConnectionsStatus() const;

    private:
        struct ConnectionMetaData
        {
            ConnectionMetaData() = default;
            explicit ConnectionMetaData(Connection_ptr connection, std::string dbTag);

            [[nodiscard]] bool isFree() const;

            std::string dbTag_;
            Connection_ptr connection_;
            std::chrono::steady_clock::time_point lastAcquireTime_ = std::chrono::steady_clock::now();
            int64_t correlationId_ = 0;
            bool checkTransactionStatus_ = false;
        };

        using DbConnections = std::vector<ConnectionMetaData>;

        [[nodiscard]] size_t getTotalConnectionsCount() const;
        [[nodiscard]] size_t getBusyConnectionsCount() const;
        [[nodiscard]] ConnectionMetaData* findFreeConnection(const std::string& dbTag);
        [[nodiscard]] const PostgresConnectParams* getConnectionParams(const std::string& dbTag) const;
        [[nodiscard]] Connection_ptr tryEstablishConnection(const std::string& dbTag, const PostgresConnectParams& connectionParams) const;
        [[nodiscard]] ConnectionMetaData* tryCreateNewDbConnection(const std::string& dbTag);
        [[nodiscard]] bool tryRemoveMostUnusedConnection();
        bool checkConnectionStatus(const ConnectionMetaData& c) const;
        void checkConnectionsHealth();

        mutable std::mutex mutex_;
        ConnectionParams connectionParams_;
        DbConnections dbConnections_;
        std::chrono::steady_clock::time_point lastHealthCheck_ = std::chrono::steady_clock::now();
        const size_t connectionsLimit_;
    };
} //namespace oes::postgresql
