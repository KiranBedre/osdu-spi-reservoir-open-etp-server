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

#include <utility>
#include <algorithm>
#include <cassert>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <libpq-fe.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/postgresql/DbConnectionPool.h>

#include <oes/core/utils/Logger.h>
#include <oes/postgresql/Connection.h>


/*! \file oes/postgresql/DbConnectionPool.h
 * \brief Database connections pool.
 * \ingroup oes_postgresql
 */

namespace oes::postgresql
{
    constexpr std::chrono::minutes MaxReservationTime(10);  // Will log a warning message when reservation time of DB connection is longer
    constexpr std::chrono::minutes HealthCheckInterval(3);

    // ============================================================================

    DbConnectionPool::ConnectionMetaData::ConnectionMetaData(Connection_ptr connection, std::string dbTag)
        : dbTag_(std::move(dbTag))
        , connection_(std::move(connection))
    { }

    bool DbConnectionPool::ConnectionMetaData::isFree() const {
        return connection_.use_count() == 1 && connection_->transaction_status() == TransactionStatus::eIdle;
    }

    // ============================================================================

    /*!
     * \class DbConnectionPool oes/postgresql/DbConnectionPool.h
     * \brief Database connection pool.
     * \ingroup oes_postgresql
     * \details
     * Supports registering multiple databases, acquire and auto release connections (managed via std::shared_ptr), periodical health check.
     */

    /*!
     * \brief Constructor of `DbConnectionPool` class.
     * \param connectionsLimit Maximum allowed number of connections to the database
     */
    DbConnectionPool::DbConnectionPool(size_t connectionsLimit)
        : connectionsLimit_(connectionsLimit)
    {
        assert(connectionsLimit > 0);
    }

    /*!
     * \brief Registers new database.
     * \param dbTag Human-readable database name, must be unique
     * \param connectionString Database connection string
     * \param verify If `true`, will try to establish connection to database before register
     * \return Returns `true` when successfully registered
     */
    bool DbConnectionPool::registerDatabase(const std::string& dbTag, const std::string& connectionString, bool verify) {
        PostgresConnectParams connectionParams;
        if (!PostgresConnectParams::tryParse(connectionString, connectionParams)) {
            return false;
        }

        return registerDatabase(dbTag, connectionParams, verify);
    }

    /*!
     * \brief Registers new database.
     * \param dbTag Human-readable database name, must be unique
     * \param connectionParams Database connection params
     * \param verify If `true`, will try to establish connection to database before register
     * \return Returns `true` when successfully registered
     */
    bool DbConnectionPool::registerDatabase(const std::string& dbTag, const PostgresConnectParams& connectionParams, bool verify)
    {
        std::lock_guard lock(mutex_);

        // Check existence
        if (isDatabaseRegistered(dbTag))
        {
            OES_ERROR("DbConnectionPool: Database '{}' is already registered!", dbTag);
            return false;
        }

        if (verify)
        {
            // Verify connection
            auto dbConnection = tryEstablishConnection(dbTag, connectionParams);
            if (dbConnection)
            {
                dbConnections_.emplace_back(std::move(dbConnection), dbTag);
                connectionParams_.emplace(std::move(dbTag), std::move(connectionParams));
                return true;
            }
        }
        else {
            // Register without establishing connection
            return connectionParams_.emplace(std::move(dbTag), std::move(connectionParams)).second;
        }

        OES_ERROR("DbConnectionPool: Could not register Database '{}'", dbTag);
        return false;
    }

    bool DbConnectionPool::isDatabaseRegistered(const std::string& dbTag) const {
        return connectionParams_.find(dbTag) != connectionParams_.cend();
    }

    bool DbConnectionPool::refreshDatabases(const std::map<std::string, PostgresConnectParams>& connectionInfos) {
        std::string dbTag;
        PostgresConnectParams connectionParams;
        bool newPartitionRegisteredFlag = false;
        for (const auto& partitionInfo : connectionInfos) {
            dbTag = partitionInfo.first;
            connectionParams = partitionInfo.second;

            if (!isDatabaseRegistered(dbTag)) {
                if (registerDatabase(dbTag, connectionParams, true)) {
                    newPartitionRegisteredFlag = true;
                }
            }
        }
        return newPartitionRegisteredFlag;
    }

    const DbConnectionPool::ConnectionParams& DbConnectionPool::getRegisteredDatabases() const {
        return connectionParams_;
    }

    /*!
     * \brief Acquires a free database connection.
     * \param dbTag Hman-readable database name, must be unique
     * \param correlationId Some unique identifier of current request (just for logging/debug purpose)
     * \return Returns shared pointer to database connection, returns `nullptr` when no free
     */
    Connection_ptr DbConnectionPool::tryAcquireConnection(const std::string& dbTag, int64_t correlationId)
    {
        std::lock_guard lock(mutex_);

        // Check if needed to do a health check
        if (lastHealthCheck_ + HealthCheckInterval <= std::chrono::steady_clock::now()) {
            checkConnectionsHealth();
        }

        OES_DEBUG("DbConnectionPool: Try to acquire DB connection for database: {}", dbTag);

        // Try to find a free connection
        ConnectionMetaData* connectionMeta = findFreeConnection(dbTag);
        if (!connectionMeta)
        {
            // Establish new connection (if possible)
            if (getTotalConnectionsCount() < connectionsLimit_ || tryRemoveMostUnusedConnection()) {
                connectionMeta = tryCreateNewDbConnection(dbTag);
            }
        }

        if (connectionMeta)
        {
            // Update metadata
            connectionMeta->correlationId_ = correlationId;
            connectionMeta->lastAcquireTime_ = std::chrono::steady_clock::now();
            connectionMeta->checkTransactionStatus_ = true;

            // Creating a copy of connection will increase counter of shared_ptr and make it reserved
            Connection_ptr dbConnection = connectionMeta->connection_;
            return dbConnection;
        }

        return nullptr;
    }

    size_t DbConnectionPool::getTotalConnectionsCount() const
    {
        return dbConnections_.size();
    }

    size_t DbConnectionPool::getBusyConnectionsCount() const
    {
        return std::count_if(dbConnections_.cbegin(), dbConnections_.cend(), [](const ConnectionMetaData& meta) {
            return !meta.isFree();
        });
    }

    DbConnectionPool::ConnectionMetaData* DbConnectionPool::findFreeConnection(const std::string& dbTag)
    {
        const auto dbConnIt = std::find_if(dbConnections_.begin(), dbConnections_.end(), [&dbTag](const auto& meta) {
            return meta.dbTag_ == dbTag && meta.isFree();
        });
        if (dbConnIt != dbConnections_.cend()) {
            return &*dbConnIt;
        }

        return nullptr;
    }

    const PostgresConnectParams* DbConnectionPool::getConnectionParams(const std::string& dbTag) const
    {
        const auto it = connectionParams_.find(dbTag);
        if (it == connectionParams_.cend()) {
            return nullptr;
        }
        return &it->second;
    }

    Connection_ptr DbConnectionPool::tryEstablishConnection(const std::string& dbTag, const PostgresConnectParams& connectionParams) const
    {
        try
        {
            OES_DEBUG("DbConnectionPool: Establish new connection to database: {}", dbTag);
            return connectionParams.createConnection();
        }
        catch (const std::exception& ex) {
            OES_ERROR("DbConnectionPool: {}, {}", dbTag, ex.what());
        }
        catch (...) {
            OES_ERROR("DbConnectionPool: Unknown error when establishing a connection to database: {}", dbTag);
        }

        OES_DEBUG("DbConnectionPool: Current active DB connection(s): {}, max {}", dbConnections_.size(), connectionsLimit_);
        return nullptr;
    }

    DbConnectionPool::ConnectionMetaData* DbConnectionPool::tryCreateNewDbConnection(const std::string& dbTag)
    {
        const auto connectionParams = getConnectionParams(dbTag);
        if (connectionParams)
        {
            auto dbConnection = tryEstablishConnection(dbTag, *connectionParams);
            if (dbConnection) {
                return &dbConnections_.emplace_back(std::move(dbConnection), dbTag);
            }
        }
        else {
            OES_ERROR("DbConnectionPool: Database with tag '{}' is not registered", dbTag);
        }

        return nullptr;
    }

    bool DbConnectionPool::tryRemoveMostUnusedConnection()
    {
        // Find most unused DB connection (by last acquire time)
        auto unusedConnIt = dbConnections_.cend();
        for (auto it = dbConnections_.cbegin(); it != dbConnections_.cend(); ++it)
        {
            if (it->isFree())
            {
                if (unusedConnIt == dbConnections_.cend() || unusedConnIt->lastAcquireTime_ > it->lastAcquireTime_) {
                    unusedConnIt = it;
                }
            }
        }

        if (unusedConnIt != dbConnections_.cend())
        {
            dbConnections_.erase(unusedConnIt);
            return true;
        }

        return false;
    }

    void DbConnectionPool::checkConnectionsHealth()
    {
        OES_INFO("DbConnectionPool: Check health of database connections, total: {}, busy {}",
            getTotalConnectionsCount(), getBusyConnectionsCount());

        // Note: PostgreSql automatically recovers connections when server was temporary unavailable (no special action needed)
        const auto currentTime = std::chrono::steady_clock::now();
        for (auto it = dbConnections_.begin(); it != dbConnections_.end(); ++it)
        {
            if (it->connection_.use_count() == 1)
            {
                // Check for uncommitted transaction in the free connection (should never happen in the correctly written code)
                if (it->checkTransactionStatus_ && it->connection_->transaction_status() == TransactionStatus::eActive)
                {
                    OES_ERROR("DbConnectionPool: Found a free connection to database '{}' with an incomplete transaction, force rollback", it->dbTag_);
                    if (PQresultStatus(it->connection_->execute("ROLLBACK").get()) == PGRES_COMMAND_OK) {
                        it->checkTransactionStatus_ = false;
                    }
                }
            }
            // Check if the connection is busy for a long time
            else if (it->lastAcquireTime_ + MaxReservationTime <= currentTime)
            {
                const size_t busyTimeMinutes = std::chrono::round<std::chrono::minutes>(currentTime - it->lastAcquireTime_).count();
                OES_WARNING("DbConnectionPool: connection {} is busy for a long time (more than {} minutes)",
                    it->correlationId_, busyTimeMinutes);
            }
        }

        lastHealthCheck_ = currentTime;
    }

    /**
     * Checks the status of all database connections in the pool and attempts to reconnect if any of them are lost.
     * @return true if all connections are in good status, false otherwise.
     */
    bool DbConnectionPool::checkConnectionStatus() const{
        if(dbConnections_.empty()){
            OES_WARNING("DbConnectionPool: no connections to check");
            return true;
        }
        for (auto it = dbConnections_.begin(); it != dbConnections_.end(); ++it){

            try{
                // Check if the connection is involved in a transaction
                if (it->connection_->transaction_status() == TransactionStatus::eActive){
                    // A transaction is in progress, do not interfere
                    continue;
                }

                // Check that the connection is alive
                it->connection_->execute("SELECT 1");
            } catch(const PostgreSQLException&){
                OES_WARNING("DbConnectionPool: connection to '{}' was lost, attempting to reconnect", it->dbTag_);
                // Attempt to reconnect
                it->connection_->reset();
                try{
                    it->connection_->execute("SELECT 1");
                    it->connection_->set_reset(true);
                    OES_WARNING("DbConnectionPool: reconnection to '{}' successful", it->dbTag_);
                }catch(const std::exception&){
                    OES_ERROR("DbConnectionPool: reconnection to '{}' failed", it->dbTag_);
                    return false;
                }
            } catch (const std::exception& e) {
                // If any other exception, then we do not try to reconnect, because it is already connected, but busy!
                OES_WARNING("DbConnectionPool: connection to '{}' is busy. Exception: {}", it->dbTag_, e.what());
                continue;
            }
        }
        return true;
    }
} // namespace oes::postgresql
