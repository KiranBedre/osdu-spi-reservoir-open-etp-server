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

#include <cassert>
#include <utility>
#include <thread>
#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Repo.h"
#include "AdminDB.h"
#include "Session.h"
#include "SpaceDDL.h"
#include "SpaceDML.h"

#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/System.h>
#include <oes/common/epc/FilePart.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/postgresql/Connection.h>
#include <oes/postgresql/DbConnectionPool.h>
#include <oes/osdu/OsduConnectionProvider.h>


namespace oes::eml::openkv
{
    /*!
     * \class OpenKVRepo oes/eml/openkv/Repo.h
     * \brief Aggregated repository for working with the database.
     * \ingroup oes_openkv
     * \details
     * Automatically reserving DB connection on first access (or use transactional connection from session).
     * The lifetime must be short as possible, for release DB connection for other pending requests.
     */

    using etp12::errorStatus;
    using ErrorCodes = etp12::Datatypes::ErrorCodes;

    namespace
    {
        // Maximum waiting time for a free DB connection
        constexpr std::chrono::seconds acquireConnectionTimeout(10);

        // Mutex to prevent concurrent partition refresh attempts
        std::mutex partition_refresh_mutex;
    }

    OpenKVRepo::OpenKVRepo(std::weak_ptr<OpenKVSession> session, DbConnectionPool_ptr dbConnectionPool, oes::eml::openETPServerConfig_ptr app_config)
        : session_(session)
        , dbConnectionPool_(std::move(dbConnectionPool))
        , server_config_(app_config)
    { }

    Connection_ptr OpenKVRepo::getDbConnection()
    {
        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            return nullptr;
        }
        if (!dbConnection_)
        {
            // Every user request temporary acquires individual DB connection, except transactions where the connection is reserved until it completes
            if (session->tx_)
            {
                // Use DB connection from active transaction
                dbConnection_ = session->tx_->db_transaction_.getConnection();
            }
            else
            {
                const std::string dbKey = resolveUserPartition();
                dbConnection_ = acquireDbConnection(dbKey);
            }
        }
        return dbConnection_;
    }

    OpenKVAdminDB_uptr& OpenKVRepo::getAdminDb()
    {
        if (!adminDb_) {
            adminDb_ = std::make_unique<OpenKVAdminDB>(getDbConnection());
        }
        if(dbConnection_ && dbConnection_->was_reset()){
            // Clear all cached statements
            adminDb_->reset_statements();
            OpenKVSession* session = session_.lock().get();
            if (session->tx_) {
                session->tx_->spaceUid_spaceReader_map_.clear();
                session->tx_->spaceUid_spaceWriter_map_.clear();
            }
            dbConnection_->set_reset(false);
            OES_INFO("AdminDB was reset after disconnection.");
        }

        return adminDb_;
    }

    SpaceReader_ptr OpenKVRepo::getSpaceReader(const Space& space, ErrorInfo& err)
    {
        Connection_ptr spaceDb = getSpaceConnection(space, err);
        if (!spaceDb) {
            return nullptr;
        }

        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            return nullptr;
        }

        if (session->tx_) {
            // There are no restrictions to READ dataspaces within a transaction.
            // Note: access can be limited for RO mode accordingly to specified list of dataspaces in the `StartTransaction` request,
            // but currently it won't give any performance benefits.
            auto& readers = session->tx_->spaceUid_spaceReader_map_;
            auto found = readers.find(space.uid_);
            if (found == readers.end()) {
                found = readers.emplace(space.uid_, std::make_shared<SpaceReader>(spaceDb)).first;
            }
            return found->second;
        }

        // return "standalone" and "throw-away" reader
        return std::make_shared<SpaceReader>(spaceDb);
    }

    SpaceWriter_ptr OpenKVRepo::getSpaceWriter(const Space& space, ErrorInfo& err)
    {
        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            return nullptr;
        }

        Connection_ptr spaceDb = getSpaceConnection(space, err);
        if (!spaceDb) {
            return nullptr;
        }

        if (session->tx_) {
            // Space writer shouldn't be requested in the RO mode (validation already exists in the OpenKVSession::txApi())
            assert(!session->tx_->ro_);

            if (!checkIsSpaceInvolvedInTheTransaction(space.uri()))
            {
                err.m_code = ErrorCodes::EREQUEST_DENIED;
                err.m_message = "Writing to dataspaces not specified in `StartTransaction` is not allowed";
                return nullptr;
            }

            auto& writers = session->tx_->spaceUid_spaceWriter_map_;
            auto found = writers.find(space.uid_);
            if (found == writers.end()) {
                found = writers.emplace(space.uid_, std::make_shared<SpaceWriter>(spaceDb)).first;
            }
            return found->second;
        }

        // return "standalone" and "throw-away" writer
        return std::make_shared<SpaceWriter>(spaceDb);
    }
/*
    Connection_ptr OpenKVRepo::findSpaceByUri(const std::string& uri, bool expect_space_uri, EtpResponseStatus& rc)
    {
        const EmlUri space_uri = EmlUri::parse(uri);
        if (!space_uri.isValid()) {
            rc = errorStatus(ErrorCodes::EINVALID_URI, "Invalid URI: " + uri);
            return nullptr;
        }

        if (expect_space_uri && space_uri.type_ != EmlUri::eDATA_SPACE) {
            rc = errorStatus(ErrorCodes::EINVALID_URI, "Expecting Dataspace URI: " + uri);
            return nullptr;
        }

        return findSpace(space_uri, rc);
    }

    Connection_ptr OpenKVRepo::findSpace(const EmlUri& etp_uri, EtpResponseStatus& rc)
    {
        OES_ASSERT(etp_uri.isValid(), ;);
        const std::string& uid_or_path = etp_uri.dataspace_;
        return findSpace(uid_or_path, rc);
    }

    Connection_ptr OpenKVRepo::findSpace(const std::string& uri_or_path, EtpResponseStatus& rc)
    {
        ErrorInfo err;
        if (auto conn = findSpace(uri_or_path, err)) {
            return conn;
        }
        rc.ok = false;
        rc.ex.m_error = std::move(err);
        return nullptr;
    }
*/
    Space OpenKVRepo::findSpaceInfo(const std::string& uri_or_path){
        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            return Space();
        }

        // Find space in the cache
        // ToDo: Think about cache between sessions, handle rollback transaction
        auto found = session->etp_space_by_path_.find(uri_or_path);
        if (found != session->etp_space_by_path_.end()) {
            const auto ro = getAdminDb()->spaceIsReadOnly(found->second) ;
            found->second.ro_ = ro ;
            return found->second;
        }
        const auto found_by_uid = session->etp_space_by_uid_.find(uri_or_path);
        if (found_by_uid != session->etp_space_by_uid_.end())
        {
            found = session->etp_space_by_path_.find(found_by_uid->second);
            OES_ASSERT(found != session->etp_space_by_path_.end(), return Space(););
            const auto ro = getAdminDb()->spaceIsReadOnly(found->second) ;
            found->second.ro_ = ro ;
            return found->second;
        }

        // Get space from DB
        return getAdminDb()->findSpace(uri_or_path);
    }

    Connection_ptr OpenKVRepo::getSpaceConnection(const Space& space, ErrorInfo& err)
    {
        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            return nullptr;
        }

        // Switch current schema
        auto dbConnection = switchCurrentSpace(space, err);
        if (dbConnection)
        {
            if (!checkSpaceSchemaVersion(space, err)) {
                return nullptr;
            }
        }
        return dbConnection;
    }

    Connection_ptr OpenKVRepo::switchCurrentSpace(const Space& space, ErrorInfo& err)
    {
        const postgresql::relational::PhysicalName phys(space.dbfile_, postgresql::relational::IMPLICIT_LOWERCASE);
        auto dbConnection = getDbConnection();
        const bool ok = dbConnection->set_current_schema(phys);
        if (!ok) {
            err.m_code = ErrorCodes::EINVALID_STATE;
            err.m_message = fmt::format("Invalid Dataspace schema: ", space.dbfile_);
        }
        return dbConnection;
    }

    /*!
    * Update space size and last modified time in DB
    */
    bool OpenKVRepo::updateSpaceInfo(const Space& space)
    {
        if (!space.isValid()) {
            return false;
        }
        return getAdminDb()->updateSpaceInfo(space);
    }

    /*!
    * Assign each array to the EPC external part its owner object points at.
    * Before schema 16 arrays were keyed by path alone, so the container was not stored.
    */
    void update_ary_containers(const Connection_ptr& conn)
    {
        using oes::common::epc::FilePart;

        std::vector<std::pair<std::string, std::string>> path_to_guid; // path, epc uuid
        {
            oes::postgresql::Statement sel = conn->prepare("select xml::text from obj");
            while (sel.step()) {
                auto [scan, scan_rc] = FilePart::scanData(sel.getString(0), FilePart::eHdfs);
                if (scan_rc != FilePart::eOK_DONE) {
                    continue; // unparseable XML cannot tell us anything
                }
                for (auto& named_hdf : scan.hdfs_) {
                    std::string path = std::move(named_hdf.second.PathInHdfFile_);
                    if (!path.empty() && path[0] == '/') {
                        path.erase(0, 1);
                    }
                    const std::string& uuid = named_hdf.second.HdfProxy_.Uuid_;
                    std::string guid = uuid.empty()
                        ? EmlUri::parse(named_hdf.second.HdfProxy_.EnergisticsUri_).guid_.toXmlString()
                        : uuid;
                    if (!path.empty() && oes::core::Guid::parse(guid).isValid()) {
                        path_to_guid.emplace_back(std::move(path), std::move(guid));
                    }
                }
            }
            sel.reset();
        }

        oes::postgresql::Statement upd = conn->prepare(
            "update ary set epc_guid = $1::uuid"
            " where path = $2 and epc_guid = '00000000-0000-0000-0000-000000000000'"
        );
        for (const auto& [path, guid] : path_to_guid) {
            upd.bind(1, guid);
            upd.bind(2, path);
            upd.execute();
            upd.reset();
        }
        OES_INFO("Assigned a container to {} array(s)", path_to_guid.size());
    }

    /*!
    * Version 16 left a nil container on arrays of EML 2.3 objects whose <URI> is a file name;
    * such an array belongs to the object itself.
    */
    void repair_eml23_ary_containers(const Connection_ptr& conn)
    {
        const int64_t repaired = conn->executeInt64(R"sql(
WITH upd AS (
    UPDATE ary a
       SET epc_guid = r.guid
      FROM res r
      JOIN typ t ON t.id = r.typ_id
      JOIN uri u ON u.id = t.uri_id
     WHERE a.obj_id = r.obj_id
       AND a.epc_guid = '00000000-0000-0000-0000-000000000000'
       AND r.guid IS NOT NULL
       AND u.ml NOT IN ('resqml20', 'eml20')
       AND NOT EXISTS (SELECT 1 FROM ary b WHERE b.epc_guid = r.guid AND b.path = a.path)
    RETURNING 1
)
SELECT count(*) FROM upd
        )sql");
        OES_INFO("Assigned their owner as container to {} EML 2.3 array(s)", repaired);
    }

    // Upgrade a schema version
    // and return success if the schema was upgraded to the expected version
    // Each step from 15 on must be mirrored in SpaceDDL: downgrade_empty_space, columns_added_after, check_space_projection
    bool OpenKVRepo::upgradeSpaceSchemaVersion(const std::string& dbFile, ErrorInfo& err, int current_version, int expected_version)
    {
        const postgresql::relational::PhysicalName phys(dbFile, postgresql::relational::IMPLICIT_LOWERCASE);

        // If current version is the latest, nothing to do
        if (current_version == expected_version) {
            return true;
        }

        if( current_version > expected_version ) {
            err.m_code = ErrorCodes::EINVALID_STATE;
            err.m_message = fmt::format("Invalid Dataspace schema: ", dbFile, " current version is greater than expected version");
            return false;
        }

        if(current_version< 9 && expected_version >= 9){
            add_deleted_resources_table(getDbConnection(), phys.quotedName());
        }

        if(current_version< 10 && expected_version >= 10){
            try{
                std::string sql = fmt::format("ALTER TABLE {}.res ADD COLUMN space_rel_id BIGINT REFERENCES admin.space_rel(id) NULL;", phys.quotedName());;
                sql += fmt::format("ALTER TABLE {}.res DROP CONSTRAINT res_obj_id_fkey;", phys.quotedName());
                sql += fmt::format("CREATE SEQUENCE {}.res_obj_id_seq;", phys.quotedName());
                // Move to bigserial
                sql += fmt::format("ALTER TABLE {}.res ALTER COLUMN obj_id SET NOT NULL;", phys.quotedName());
                sql += fmt::format("ALTER TABLE {}.res ALTER COLUMN obj_id SET DEFAULT nextval('{}.res_obj_id_seq');"
                    , phys.quotedName()
                    , phys.quotedName()
                );
                sql += fmt::format("ALTER SEQUENCE {}.res_obj_id_seq OWNED BY {}.res.obj_id;"
                    , phys.quotedName()
                    , phys.quotedName()
                );
                sql += fmt::format("ALTER TABLE {}.res ALTER COLUMN guid DROP NOT NULL;", phys.quotedName());
                sql += fmt::format(
                    "ALTER TABLE {}.obj ADD FOREIGN KEY (id) REFERENCES {}.res(obj_id) ON DELETE CASCADE;",
                    phys.quotedName(), phys.quotedName()
                );
                sql += fmt::format("ALTER TABLE {}.rel DROP CONSTRAINT rel_obj_id_fkey;", phys.quotedName());
                sql += fmt::format("ALTER TABLE {}.rel DROP CONSTRAINT rel_dst_id_fkey;", phys.quotedName());
                sql += fmt::format(
                    "ALTER TABLE {}.rel ADD FOREIGN KEY (obj_id) REFERENCES {}.res(obj_id) ON DELETE CASCADE;",
                    phys.quotedName(), phys.quotedName()
                );
                sql += fmt::format(
                    "ALTER TABLE {}.rel ADD FOREIGN KEY (dst_id) REFERENCES {}.res(obj_id);",
                    phys.quotedName(), phys.quotedName()
                );
                getDbConnection()->execute(sql);
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: ", dbFile, " from version 9 to 10");
                return false;
            }
        }

        if(current_version< 11 && expected_version >= 11){
            try{
                std::string sql = fmt::format("SELECT setval('{}.res_obj_id_seq', (SELECT MAX(obj_id) FROM {}.res));", phys.quotedName(), phys.quotedName());
                getDbConnection()->execute(sql);
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: ", dbFile, " from version 10 to 11");
                return false;
            }
        }

        if(current_version<12 && expected_version>=12){
            try{
                std::string sql = fmt::format("DROP TRIGGER IF EXISTS insert_deleted_row_to_del_res_trigger ON {}.res;", phys.quotedName());
                sql += fmt::format("DROP FUNCTION IF EXISTS {}.insert_deleted_row_to_del_res;", phys.quotedName());
                getDbConnection()->execute(sql);
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: ", dbFile, " from version 11 to 12");
                return false;
            }
        }

        if(current_version<13 && expected_version>=13){
            try{
                const std::string& schema = phys.quotedName() ;
                add_import_by_ref_triggers(getDbConnection(), schema) ;
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: ", dbFile, " from version 12 to 13");
                return false;
            }
        }

        if(current_version<14 && expected_version>=14){
            try{
                // Step 1: Drop the unique constraint on the ns column
                std::string sql = fmt::format("ALTER TABLE uri DROP CONSTRAINT IF EXISTS uri_ns_key;");

                // Step 2: Add the vs column
                sql += fmt::format("ALTER TABLE uri ADD COLUMN vs text NOT NULL DEFAULT '2.0';");

                // Step 3: Add a new unique constraint on ns and vs
                sql += fmt::format("ALTER TABLE uri ADD CONSTRAINT uri_ns_vs_key UNIQUE (ns, vs);");
                getDbConnection()->execute(sql);
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: ", dbFile, " from version 13 to 14");
                return false;
            }
        }

        if(current_version<15 && expected_version>=15){
            try{
                const std::string& schema = phys.quotedName() ;
                add_relations_view(getDbConnection(), schema) ;
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: ", dbFile, " from version 14 to 15");
                return false;
            }
        }
        if(current_version<16 && expected_version>=16){
            try{
                // The default only exists to fill the existing rows, update_ary_containers() replaces it
                std::string sql =
                    "ALTER TABLE ary ADD COLUMN IF NOT EXISTS epc_guid uuid NOT NULL"
                    " DEFAULT '00000000-0000-0000-0000-000000000000';"
                    "ALTER TABLE ary DROP CONSTRAINT IF EXISTS ary_path_key;"
                    "ALTER TABLE ary ADD CONSTRAINT ary_epc_guid_path_key UNIQUE (epc_guid, path);"
                ;
                getDbConnection()->execute(sql);
                update_ary_containers(getDbConnection());
                getDbConnection()->execute("ALTER TABLE ary ALTER COLUMN epc_guid DROP DEFAULT;");
                update_resources_view(getDbConnection(), phys.sqlText());
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: ", dbFile, " from version 15 to 16");
                return false;
            }
        }
        if(current_version<17 && expected_version>=17){
            try{
                repair_eml23_ary_containers(getDbConnection());
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: {} from version 16 to 17", dbFile);
                return false;
            }
        }
        if(current_version<18 && expected_version>=18){
            try{
                update_space_rel_triggers(getDbConnection(), phys.sqlText());
                add_lookup_indexes(getDbConnection(), phys.sqlText());
            }catch(std::exception&){
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format("Cannot upgrade schema: {} from version 17 to 18", dbFile);
                return false;
            }
        }
        // Update schema version
        int magic_cookie = 0;
        space_schema_versions(magic_cookie);
        const std::string sql = fmt::format("COMMENT ON SCHEMA {} IS '{}:{}'", phys.quotedName(), magic_cookie, expected_version);
        const auto schema_update = getDbConnection()->execute(sql);
        if (!schema_update) {
            err.m_code = ErrorCodes::EINVALID_STATE;
            err.m_message = fmt::format("Invalid Dataspace schema: ", dbFile, " schema version update failed");
            return false;
        }

        return true;
    }

    // Returns the schema version of a space
    int OpenKVRepo::getSchemaVersion(const Connection_ptr& conn, const std::string& dbFile)
    {
        const postgresql::relational::PhysicalName phys(dbFile, postgresql::relational::IMPLICIT_LOWERCASE);
        const std::string str = conn->executeString(
            fmt::format("SELECT obj_description('{}'::regnamespace, 'pg_namespace')", phys.sqlText())
        );
        if (str.empty()) {
            return 0;
        }
        const std::string user_v_str = str.substr(str.find(':') + 1);
        return std::stoi(user_v_str);
    }


    // Check the schema version and return success if the schema version is the latest
    // If the schema version is not the latest, attempt to upgrade the schema to the latest version
    // If the schema version is not the latest and the schema upgrade failed, return failure
    bool OpenKVRepo::checkSpaceSchemaVersion(const Space& space, ErrorInfo& err)
    {
        int magic_cookie = 0;
        const auto& schema_history = space_schema_versions(magic_cookie);
        const postgresql::relational::PhysicalName phys(space.dbfile_, postgresql::relational::IMPLICIT_LOWERCASE);
        const std::string str = getDbConnection()->executeString(
            fmt::format("SELECT obj_description('{}'::regnamespace, 'pg_namespace')", phys.sqlText())
        );
        if (str.empty()) {
            return true;
        }
        const std::string app_str = str.substr(0, str.find(':'));
        if (app_str.empty()) {
            return true;
        }
        const int app_id = std::stoi(app_str);
        if (magic_cookie != app_id) {
            err.m_code = ErrorCodes::EINVALID_STATE;
            err.m_message = "Invalid Dataspace schema: " + space.uid_;
            return false; // found, but invalid magic cookie
        }

        const int expected = schema_history.back().version;
        const std::string user_v_str = str.substr(str.find(':') + 1);
        const int version = std::stoi(user_v_str);
        if (expected != version) {
            // Attempt to upgrade
            upgradeSpaceSchemaVersion(space.dbfile_, err, version, expected);
            if(err.m_code != ErrorCodes::IS_OK) {
                return false;
            }

            // Check again after upgrade
            const int upgraded = getSchemaVersion( getDbConnection(), space.dbfile_);
            if(upgraded != expected) {
                err.m_code = ErrorCodes::EINVALID_STATE;
                err.m_message = fmt::format(
                    "Unexpected Dataspace schema: Got v{}; want v{}: {}",
                    upgraded, expected, space.uid_
                );
                return false; // found, but out-of-date
            }
        }

        return true;
    }

    /**
     * \brief Check if the requested dataspace is involved in the ETP transaction scope (based on the specified list in `StartTransaction`).
     * \param etp_uri EML URI of dataspace
     * \return true when dataspace is allowed; false otherwise.
     */
    bool OpenKVRepo::checkIsSpaceInvolvedInTheTransaction(const EmlUri& space_uri) const
    {
        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            OES_ERROR("SVR Session has been forcefully terminated.");
            return false;
        }

        // ETP spec: an empty LIST means all dataspaces
        if (!session->tx_ || session->tx_->dataspaceUris_.empty()) {
            return true;
        }

        auto& tx = *session->tx_;
        return std::find(tx.dataspaceUris_.cbegin(), tx.dataspaceUris_.cend(), space_uri) != tx.dataspaceUris_.cend();
    }

    std::string OpenKVRepo::resolveUserPartition() const
    {
        OpenKVSession* session = session_.lock().get();
        if(session == nullptr) {
            throw std::runtime_error("SVR Session has been forcefully terminated.");
        }

        std::string dbKey;
        const std::string dbm = server_config_->partitionMode();
        if (dbm.empty()) {
            throw std::runtime_error("Partition mode unknown");
        }

        if (dbm == "multiple") {
            dbKey = session->handshake_.data_partition_id_;
        }
        else {
            dbKey = "single";
        }

        // Check existence
        if (!dbConnectionPool_->isDatabaseRegistered(dbKey)) {
            // Try to dynamically register the partition by refreshing
            OES_INFO("Dynamic partition registration triggered for partition '{}'. Partition not found in registered databases.", dbKey);
            std::lock_guard<std::mutex> lock(partition_refresh_mutex);
            
            // Double-check after acquiring lock (another thread may have just refreshed)
            if (!dbConnectionPool_->isDatabaseRegistered(dbKey)) {
                OES_INFO("Partition '{}' not registered. Attempting to discover and register from OSDU.", dbKey);
                
                try {
                    // Discover all partitions from OSDU API
                    auto partitionInfo = oes::osdu::OsduConnectionInfoProvider::getOsduConnectionInfo(*server_config_);
                    
                    // Find the requested partition in the discovered list
                    auto it = partitionInfo.find(dbKey);
                    if (it == partitionInfo.end()) {
                        throw etp12::errorStatus(ErrorCodes::EINVALID_ARGUMENT, "Partition with name '" + dbKey + "' not found in OSDU configuration.");
                    }
                    
                    OES_INFO("Registering partition '{}'", dbKey);
                    dbConnectionPool_->refreshDatabases({{dbKey, it->second}});
                    
                    // Initialize database schema for the requested partition only
                    if (dbConnectionPool_->isDatabaseRegistered(dbKey)) {
                        try {
                            auto dbConnection = dbConnectionPool_->tryAcquireConnection(dbKey);
                            if (!dbConnection) {
                                OES_ERROR("Failed to acquire DB connection for partition '{}'.", dbKey);
                                throw etp12::errorStatus(ErrorCodes::EINVALID_ARGUMENT, "Unable to acquire connection for partition '" + dbKey + "'.");
                            }
                            
                            auto adminDb = std::make_unique<OpenKVAdminDB>(dbConnection);
                            
                            // Set application_name for this connection
                            std::ostringstream oss;
                            oss << "SELECT set_config('application_name','" << adminDb->app_id() << "', false);";
                            dbConnection->execute(oss.str().c_str());
                            
                            if (!adminDb->isSchemaCreated()) {
                                OES_INFO("Creating main schema for partition '{}'.", dbKey);
                                if (!adminDb->createSchema()) {
                                    OES_ERROR("Failed to create main schema for partition '{}'.", dbKey);
                                } else {
                                    OES_INFO("Successfully created main schema for partition '{}'.", dbKey);
                                }
                            } else {
                                // Check if schema needs upgrade
                                int current_v = 0;
                                int latest_v = 0;
                                adminDb->isSchemaUpToDate(true, &current_v, &latest_v);
                                if (current_v != latest_v) {
                                    OES_WARNING("Out-of-date main DB for partition '{}'. Current: {}, Latest: {}", dbKey, current_v, latest_v);
                                    if (!adminDb->upgradeSchema(current_v, latest_v)) {
                                        OES_ERROR("Cannot upgrade main DB for partition '{}' from version {} to {}.", dbKey, current_v, latest_v);
                                    } else {
                                        OES_INFO("Upgraded main DB for partition '{}' from version {} to {}.", dbKey, current_v, latest_v);
                                    }
                                } else {
                                    OES_INFO("Main schema already exists and up-to-date for partition '{}'.", dbKey);
                                }
                            }
                        } catch (const std::exception& ex) {
                            OES_ERROR("Failed to initialize schema for partition '{}': {}", dbKey, ex.what());
                        }
                    }
                } catch (const std::exception& ex) {
                    OES_ERROR("Failed to discover/register partition '{}': {}", dbKey, ex.what());
                    throw;
                }
                
                // Final validation: Ensure the requested partition is now registered
                if (!dbConnectionPool_->isDatabaseRegistered(dbKey)) {
                    throw etp12::errorStatus(ErrorCodes::EINVALID_ARGUMENT, "Partition with name '" + dbKey + "' registration failed.");
                }
            }
        }

        return dbKey;
    }

    Connection_ptr OpenKVRepo::acquireDbConnection(const std::string& dbKey) const
    {
        Connection_ptr dbConnection;
        const auto startTime = std::chrono::steady_clock::now();
        while (!dbConnection)
        {
            dbConnection = dbConnectionPool_->tryAcquireConnection(dbKey);
            if (dbConnection == nullptr)
            {
                // ToDo: use request timeout (for example, 70% from max timeout)
                if (std::chrono::steady_clock::now() - startTime > acquireConnectionTimeout) {
                    // Unable to acquire a DB connection within the allotted time
                    throw etp12::errorStatus(ErrorCodes::EBACKPRESSURE_LIMIT_EXCEEDED, "Requested partition '" + dbKey + "' is not available now due high load.\n");
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        return dbConnection;
    }
}
