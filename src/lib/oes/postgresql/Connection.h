// ============================================================================
// Copyright 2018-2023 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef oes_postgresql_connection_h
#define oes_postgresql_connection_h

#include <oes/postgresql/export.h>
#include <oes/postgresql/fwd.h>
#include <oes/postgresql/PhysicalName.h>
#include <oes/postgresql/Transaction.h>
#include <oes/postgresql/Statement.h>

#include <oes/core/utils/EnumDecl.h>
#include <oes/core/utils/VersionNumber.h>
#include <oes/core/utils/ScopedUtils.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <postgres_ext.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <functional>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <optional>
#include <unordered_map>

namespace oes {
namespace postgresql {

using oes::core::VersionNumber;
using oes::postgresql::relational::PhysicalName;
using oes::postgresql::relational::PhysicalNameVec;

/****************************************************************************/
// Custom exception to throw when a connection status is not in good shape.
class OES_POSTGRESQL_EXPORT StatusConnectionException: public std::exception {
private:
    std::string msg_;
public:
    StatusConnectionException(const char* const msg): msg_(msg) {}

    const char * what() const noexcept override {return msg_.data();}

};

/****************************************************************************/

// See ErrorCodes.h for all possible PGErrorCode values
class OES_POSTGRESQL_EXPORT PostgreSQLException : public std::runtime_error {
public:
    PostgreSQLException(PGErrorCode err_code, const char* msg);
    ~PostgreSQLException() throw() override {}

    PGErrorCode error_code;

    PGErrorCode getErrorCode() const { return error_code; }

    static PostgreSQLException fromPGresult(const PGresult_uptr& result);

    PostgreSQLException& operator=(const PostgreSQLException&) = delete;
};

/****************************************************************************/

OES_POSTGRESQL_EXPORT
size_t maxNameLength();

/****************************************************************************/

enum NoticeSeverity : int {
    eNONE,
    eERROR, eFATAL, ePANIC, // errors
    eWARNING, eNOTICE, eDEBUG, eINFO, eLOG // notices
};

using NoticeHandler = std::function<
    void(const PGresult*, NoticeSeverity, const char* msg, const char* details)
>;

/****************************************************************************/

enum class TransactionStatus
{
    eIdle,     // no active transaction
    eActive,   // active transaction
    eOnError,  // error transaction
    eUnknown,  // unknown status
};

/****************************************************************************/

class OES_POSTGRESQL_EXPORT Connection final : public std::enable_shared_from_this<Connection> {
public:
    explicit Connection(PGconn_uptr conn);
    explicit Connection(const std::string& connection_string);

    struct QueryParameter {
        const char* bytes_address   = nullptr;
        int length                  = 0;
        bool binary_format          = false; // false: text, true: binary
        std::vector<uint32_t> bytes = {}; //when the initial type need to be transformed
    };
    using QueryParameters = std::vector<QueryParameter>;
    using QueryParameterTypes = std::vector<Oid>;

    PGresult_uptr execute(const std::string& stmt) const;
    PGresult_uptr execute(const std::string& stmt, const QueryParameters& params, QueryParameterTypes paramTypes = {}, bool binaryResult = false) const;

    int64_t     executeInt64( const std::string& stmt) const;
    int         executeInt(   const std::string& stmt) const;
    std::string executeString(const std::string& stmt) const;

    Statement prepare(std::string query, StatementType stmtType = StatementType::Prepared);
    Statement prepare(std::string query, std::string stmtName, StatementType stmtType = StatementType::Prepared);
    Statement getPrepared(const std::string& stmtName) const;
    std::optional<Statement> tryGetPrepared(const std::string& stmtName) const;

    std::string user_name() const;
    const PhysicalName& user_pname() const;

    PhysicalName db() const;
    std::string host() const;
    std::string port() const;
    std::string options() const;
    PhysicalName current_schema() const;
    PhysicalNameVec current_schemas() const;
    std::string search_path() const;
    bool set_current_schema(
        const PhysicalName& schema,
        bool add_extensions_schemas = true
    );
    bool set_current_schemas(
        const PhysicalNameVec& schema,
        bool add_extensions_schemas = true
    );

    VersionNumber client_version() const;
    VersionNumber server_version() const;
    VersionNumber protocol_version() const;
    std::string server_version_info() const;
    std::string get_parameter(const std::string& name) const;
    std::map<std::string, std::string> get_parameters() const;

    bool vacuum() noexcept;
    bool reset();

    bool set_notice_handler(NoticeHandler handler);

    std::string escapeLiteral(const std::string& to_escape);
    std::string escapeIdentifier(const std::string& to_escape);

    std::vector<std::string> registered_extensions() const;
    PhysicalNameVec registered_extension_schemas() const;
    bool register_extension(const std::string& ext_name, bool required = true);
    PhysicalName get_extension_schema(
        const std::string& ext_name, bool verbose_on_not_found = true
    );
    PhysicalName find_extension_schema(
        const std::string& ext_name, bool verbose_on_not_found = true
    );

    Transaction begin_transaction(bool read_only = false);
    TransactionStatus transaction_status() const;

    PGconn* pq_connection();

    bool was_reset() const;
    void set_reset(bool reset);

    core::ScopedFunctor make_multi_threads_guard() const;
    
private:
    static void notice_receiver(void* arg, const PGresult *res);

    PGconn_uptr conn_;
    NoticeHandler notice_handler_;
    mutable PhysicalName user_name_;

    PhysicalNameVec extension_schemas_;
    std::map<std::string, std::pair<PhysicalName, bool>> extensions_;
    mutable std::atomic_bool usage_flag = false;
    std::unordered_map<std::string, Statement> statements_;
    bool was_reset_ = false;
};

/*****************************************************************************/

namespace connect_params {

OES_DEFINE_ENUM(
    ParamKey, UNKNOWN_PARAM_KEY,
    (UNKNOWN_PARAM_KEY)
    (eHOST)
    (eHOSTADDR)
    (ePORT)
    (eDBNAME)
    (eUSER)
    (ePASSWORD)
    (eCONNECT_TIMEOUT)
    (eCLIENT_ENCODING)
    (eOPTIONS)
    (eAPPLICATION_NAME)
    (eFALLBACK_APPLICATION_NAME)
    (eKEEPALIVES)
    (eKEPPEALIVES_IDLE)
    (eKEPPEALIVES_INTERVAL)
    (eKEPPEALIVES_COUNT)
    (eTTY)
    (eSSLMODE)
);

} // namespace connect_params

struct OES_POSTGRESQL_EXPORT PostgresConnectParams {
    explicit PostgresConnectParams(size_t reserve = 6);

    static PostgresConnectParams parse(const std::string& conn_str, bool skip_vals_if_empty = false);
    static bool tryParse(const std::string& conn_str, PostgresConnectParams& result, bool skip_vals_if_empty = false) noexcept;

    const bool empty() const;

    void add(const connect_params::ParamKey key, const std::string& val, bool skip_if_empty = false);
    void add(const std::string& key, const std::string& val, bool skip_if_empty = false);
    void addOrUpdate(const connect_params::ParamKey key, const std::string& val, bool skip_if_empty = false);
    void addOrUpdate(const std::string& key, const std::string& val, bool skip_if_empty = false);
    void registerDynamicParam(const connect_params::ParamKey key, const std::function<std::string()>& fn);
    void registerDynamicParam(const std::string& key, const std::function<std::string()>& fn);

    PGconn* connectdbParams() const;
    Connection_ptr createConnection() const;

private:
    std::vector<std::string> keys_;
    std::vector<std::string> vals_;

    std::map<std::string, std::function<std::string()>> dynamicParams_;
};

/****************************************************************************/

class OES_POSTGRESQL_EXPORT ScopedSetCurrentSchema {
public:
    ScopedSetCurrentSchema(Connection& conn, const PhysicalName& schema);
    ~ScopedSetCurrentSchema();

    ScopedSetCurrentSchema& operator=(const ScopedSetCurrentSchema&) = delete;
    ScopedSetCurrentSchema(const ScopedSetCurrentSchema&) = delete;

private:
    Connection& conn_;
    PhysicalName old_schema_;
};

/****************************************************************************/

} // namespace postgresql
} // namespace oes

OES_DEFINE_ENUM_TRAITS(oes::postgresql::connect_params::ParamKey);

#endif
