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

#include <map>
#include <set>
#include <cctype>
#include <sstream>
#include <algorithm>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <libpq-fe.h>
#include <pg_config_manual.h> // for NAMEDATALEN
#include <boost/crc.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/SqlLogger.h>

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/Otel.h>

#include <oes/postgresql/Format.hpp>
#include <oes/postgresql/ErrorCodes.h>
#include <oes/postgresql/PhysicalName.h>

using namespace oes::postgresql::connect_params;

namespace {
    /*!
    * \brief Converts a ParamKey to the corresponding libpq parameter name. An unknown value will return an empty string.
    * \param key the key to convert
    */
    std::string paramKeyToString(ParamKey key) {
        switch (key) {
        case eHOST:
            return "host";
        case eHOSTADDR:
            return "hostaddr";
        case ePORT:
            return "port";
        case eDBNAME:
            return "dbname";
        case eUSER:
            return "user";
        case ePASSWORD:
            return "password";
        case eCONNECT_TIMEOUT:
            return "connect_timeout";
        case eCLIENT_ENCODING:
            return "client_encoding";
        case eOPTIONS:
            return "options";
        case eAPPLICATION_NAME:
            return "application_name";
        case eFALLBACK_APPLICATION_NAME:
            return "fallback_application_name";
        case eKEEPALIVES:
            return "keepalives";
        case eKEPPEALIVES_IDLE:
            return "keppealives_idle";
        case eKEPPEALIVES_INTERVAL:
            return "keppealives_interval";
        case eKEPPEALIVES_COUNT:
            return "keppealives_count";
        case eTTY:
            return "tty";
        case eSSLMODE:
            return "sslmode";
        default:
            return "";
        }
    }

    void cleanupDynamicVals(std::vector<const char*>& cleanupVals) {
        std::for_each(
            cleanupVals.begin(),
            cleanupVals.end(),
            [](const char *c) { free((char*)c); });
    }

    using SqlLogger = oes::postgresql::SqlLogger;
}

namespace oes {
namespace postgresql {

/****************************************************************************/

size_t maxNameLength() {
    return NAMEDATALEN;
}

PGconn_uptr connectDb(const std::string& connectionString)
{
    auto conn = PGconn_uptr(PQconnectdb(connectionString.c_str()));
    if (PQstatus(conn.get()) != CONNECTION_OK) {
        const auto errMsg = PQerrorMessage(conn.get());
        throw PostgreSQLException(PG_08006/*=connection_failure*/, errMsg);
    }
    return conn;
}

std::string buildStatementName(std::string_view sql, StatementType stmtType)
{
    // Statement name generation must be deterministic, name should be unique based on input arguments
    std::string action = stmtType == StatementType::PreparedWithCursor ? "stmt_cursor" : "stmt";
    if (stmtType == StatementType::Prepared)
    {
        const auto startActionIt = std::find_if_not(sql.cbegin(), sql.cend(), [](auto ch) { return std::isspace(ch); });
        const auto endActionIt = std::find_if(startActionIt, sql.cend(), [](auto ch) { return std::isspace(ch); });
        if (startActionIt != sql.cend()) {
            action += "_" + std::string(startActionIt, endActionIt);
        }
    }

    return fmt::format("{}_{}", action, std::hash<std::string_view>()(sql));
}

/****************************************************************************/

/*!
 * \ingroup oes_postgresql
 * \class oes::oracle::PostgreSQLException oes/postgresql/Connection.h
 *
 * \brief Exception thrown when any libpq call returned an error.
 *
 * Derives from std::runtime_error, which allows to generically catch
 * runtime exceptions.
 */

/*!
* \brief Instantiates a new PostgreSQL exception from a string.
* \param msg the error message
*/
PostgreSQLException::PostgreSQLException(
    PGErrorCode err_code,
    const char* msg
) : std::runtime_error(msg), error_code(err_code)
{}

/*!
* \brief Instantiates a new PostgreSQL exception from a faulty PGresult.
* \param result the PostgreSQL result of the query that  failed.
*/
PostgreSQLException PostgreSQLException::fromPGresult(const PGresult_uptr& result) {
    // msg is deleted with PGresult
    const char* msg = PQresultErrorMessage(result.get());
    // TODO: more info with PQresultErrorField
    const char* state = PQresultErrorField(result.get(), PG_DIAG_SQLSTATE);
    PGErrorCode code = lookupErrorCodeByName(state);
    return PostgreSQLException(code, msg);
}

/****************************************************************************/

/*!
* \class Connection oes/postgresql/Connection.h
* \brief The link between a client and a PostgreSQL server.
* \note One connection cannot be used for perform concurrent commands from different threads.
* Use multiple connections if you need to run concurrent commands.
*/

/*!
 * \brief Takes ownership of the raw connection.
 *
 * \param conn the raw libpq connection which will now belong to Connection
 */
Connection::Connection(PGconn_uptr conn)
    : conn_(std::move(conn))
{
    // always capture messages, even when no handler was registered
    if (PQsetNoticeReceiver(conn_.get(), &notice_receiver, this)) {
        //OES_WARNING(oes::core::Format("Previous PQnoticeReceiver cancelled"));
    }

    // setting OS user and host for server-side use, as PostgreSQL does not do it.
    try {
        execute(fmt::format(
            "SELECT set_config('USERENV.OS_USER', '{}', false)",
            oes::core::System::getUserName()
        ));
        execute(fmt::format(
            "SELECT set_config('USERENV.HOST', '{}', false)",
            oes::core::System::getHostName()
        ));
    } catch(...) {} // intercept
}

/*!
 * \brief Open new connection based on connection string.
 *
 * \param connection_string Connection string with credentials
 */
Connection::Connection(const std::string& connection_string) :
    Connection(connectDb(connection_string))
{
}

/*!
 * \brief Returns the underlying PostgreSQL connection object.
 *
 * Will return a valid instance if Connection itself is valid.
 * \return The raw PGconn.
 */
PGconn* Connection::pq_connection() {
    return conn_.get();
}

/*!
 * \brief Returns if the connection was reset after temporary failure.
 *
 * \return true if the connection was reset; false otherwise.
*/
bool Connection::was_reset() const{
    return was_reset_;
}

/*!
 * \brief Sets the connection reset status.
 *
 * Sets the connection reset status to the given value and clears the list of prepared statements.
 * \param reset the new reset status.
 */
void Connection::set_reset(bool reset)
{
    was_reset_ = reset;
    statements_.clear();
}

/*!
 * \brief Executes a statement.
 *
 * Executes the given SQL statement.
 * Makes sense to use during application initialization or for rare operations, in any other cases prefer to use prepared statements.
 * \param stmt the SQL statement
 * \param silent if true, do not log the error (used for connection probe where an error can be expected and is acted upon)
 * \return the PGresult that can be read and parsed for the query result.
 * \throws PostgreSQLException if the result failed
 */
PGresult_uptr Connection::execute(const std::string& stmt, bool silent) const {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "Connection." + std::string(__func__),
        {{"stmt", stmt}});
    
    SqlLogger log("Execute");
    
    auto mt_guard = make_multi_threads_guard();
    PGresult_uptr res_query(PQexec(conn_.get(), stmt.c_str()));
    ExecStatusType st = PQresultStatus(res_query.get());
    
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        if( !silent ) {
            log.logError(PQresultErrorMessage(res_query.get()));
        }
        throw PostgreSQLException::fromPGresult(res_query);
    }
    
    return res_query;
}

/*!
* \brief Executes a statement with parameters.
*
* Executes the given SQL statement with parameters.
* Makes sense to use during application initialization or for rare operations, in any other cases prefer to use prepared statements.
* \param stmt the SQL statement
* \param params the list of parameters bound to the statement
* \param paramTypes the types (given by their Oid) of the parameters to be bound (empty when the parameter is to be inferred)
* \param binaryResult pass `true` to obtain results in binary format, `false` for text format
* \return the PGresult that can be read and parsed for the query result.
* \throws PostgreSQLException if the result failed
*/
PGresult_uptr Connection::execute(const std::string& stmt, const QueryParameters& params, QueryParameterTypes paramTypes, bool binaryResult) const
{
    SqlLogger log("Execute(parameterized)");
    
    auto mt_guard = make_multi_threads_guard();

    const int nParams = static_cast<int>(params.size());
    std::vector<const char*> paramValues(nParams);
    std::vector<int> paramLengths(nParams);
    std::vector<int> paramFormats(nParams);
    
    for (int i = 0; i < nParams; ++i) {
        if(params[i].bytes.size() > 0){
            paramValues[i] = reinterpret_cast<const char*>(params[i].bytes.data());
            paramLengths[i] = static_cast<int>(sizeof(uint32_t) * params[i].bytes.size());
            paramFormats[i] = 1;
        }else{
            paramValues[i] = params[i].bytes_address;
            paramLengths[i] = params[i].length;
            paramFormats[i] = params[i].binary_format ? 1 : 0;
        }
    }

    const Oid* paramTypesPtr = paramTypes.empty() ? nullptr : paramTypes.data();
    PGresult_uptr res_query(PQexecParams(conn_.get(), stmt.c_str(), nParams, paramTypesPtr,
        paramValues.data(), paramLengths.data(), paramFormats.data(), binaryResult ? 1 : 0
    ));
    ExecStatusType st = PQresultStatus(res_query.get());
    
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        log.logError(PQresultErrorMessage(res_query.get()));
        throw PostgreSQLException::fromPGresult(res_query);
    }
    
    return res_query;
}

/*!
 * \brief Executes a statement and return single value as 64 bits int
 *
 * Executes the given SQL statement
 * \param stmt the SQL statement
 * \return the PGresult that can be read and parsed for the query result.
 * \throws PostgreSQLException if the result failed
 */
int64_t Connection::executeInt64(const std::string& stmt) const {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "Connection." + std::string(__func__),
        {{"stmt", stmt}});
    
    SqlLogger log("ExecuteInt64");
    
    auto mt_guard = make_multi_threads_guard();
    PGresult_uptr res(PQexec(conn_.get(), stmt.c_str()));
    ExecStatusType st = PQresultStatus(res.get());
    
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        log.logError(PQresultErrorMessage(res.get()));
        throw PostgreSQLException::fromPGresult(res);
    }
    const int ntuples = PQntuples(res.get());
    if (ntuples == 0) {
        log.logError("empty result set");
        throw PostgreSQLException(DXO_20980, "empty result set");
    }
    if (ntuples > 1) {
        log.logError("more than one result row returned");
        throw PostgreSQLException(DXO_20981, "more than one result row returned");
    }
    
    if (PQgetisnull(res.get(), 0, 0) == 1) {
        return 0;
    }
    
    const char* tmp_res = PQgetvalue(res.get(), 0, 0);
    size_t actual_len = PQgetlength(res.get(), 0, 0);
    size_t consumed_len = 0;
    int64_t int_res = std::stol(tmp_res, &consumed_len);
    assert(consumed_len == actual_len);
    OES_RELEASE_UNUSED(actual_len);
    return int_res;
}

/*!
 * \brief Executes a statement and return single value as 32 bits int
 *
 * Executes the given SQL statement
 * \param stmt the SQL statement
 * \return the PGresult that can be read and parsed for the query result.
 * \throws PostgreSQLException if the result failed
 */
int Connection::executeInt(const std::string& stmt) const {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "Connection." + std::string(__func__),
        {{"stmt", stmt}});
    
    SqlLogger log("ExecuteInt");
    
    auto mt_guard = make_multi_threads_guard();
    PGresult_uptr res(PQexec(conn_.get(), stmt.c_str()));
    ExecStatusType st = PQresultStatus(res.get());
    
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        log.logError(PQresultErrorMessage(res.get()));
        throw PostgreSQLException::fromPGresult(res);
    }
    const int ntuples = PQntuples(res.get());
    if (ntuples == 0) {
        log.logError("empty result set");
        throw PostgreSQLException(DXO_20980, "empty result set");
    }
    if (ntuples > 1) {
        log.logError("more than one result row returned");
        throw PostgreSQLException(DXO_20981, "more than one result row returned");
    }
    
    if (PQgetisnull(res.get(), 0, 0) == 1) {
        return 0;
    }
    
    const char* tmp_res = PQgetvalue(res.get(), 0, 0);
    size_t actual_len = PQgetlength(res.get(), 0, 0);
    size_t consumed_len = 0;
    int int_res = std::stoi(tmp_res, &consumed_len);
    assert(consumed_len == actual_len);
    OES_RELEASE_UNUSED(actual_len);
    return int_res;
}

std::string Connection::executeString(const std::string& stmt) const {
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan("Connection." + std::string(__func__));
    span.setAttr("stmt", stmt);
    
    SqlLogger log("ExecuteString");
    
    auto mt_guard = make_multi_threads_guard();
    PGresult_uptr res(PQexec(conn_.get(), stmt.c_str()));
    ExecStatusType st = PQresultStatus(res.get());
    
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        log.logError(PQresultErrorMessage(res.get()));
        throw PostgreSQLException::fromPGresult(res);
    }
    const int ntuples = PQntuples(res.get());
    if (ntuples == 0) {
        log.logError("empty result set");
        throw PostgreSQLException(DXO_20980, "empty result set");
    }
    if (ntuples > 1) {
        log.logError("more than one result row returned");
        throw PostgreSQLException(DXO_20981, "more than one result row returned");
    }
    
    if (PQgetisnull(res.get(), 0, 0) == 1) {
        return std::string();
    }

    const char* temp_res = PQgetvalue(res.get(), 0, 0);
    const size_t actual_len = PQgetlength(res.get(), 0, 0);
    if (actual_len > 0 && temp_res) {
        return std::string(temp_res, actual_len);
    }
    return {};
}

/*!
* \brief Prepares a statement with an auto generated name.
*
* Prepares a SQL statement of the given query with an auto generated name
*
* \param query the SQL query
* \param stmtType statement type
* \throws PostgreSQLException if the prepare failed
*/
Statement Connection::prepare(std::string query, StatementType stmtType) {
    std::string stmtName = buildStatementName(query, stmtType);
    return prepare(std::move(query), std::move(stmtName), stmtType);
}

/*!
* \brief Prepares a statement.
*
* Prepares a SQL statement of the given \p query with the name \p stmtName.
* All prepared statements are caching and belong to the particular connection.
- Statement which is already prepared, cannot be overwritten by another SQL query with the same name.
* Statements with the same name shares internal state (for example, two instances will have the same binded arguments and result).
*
* \param query the SQL query
* \param stmtName the name of the prepared query
* \param stmtType statement type
* \return the `Statement` object (lightweight wrapper of internal statement implementation).
* \throws PostgreSQLException if the prepare failed
*/
Statement Connection::prepare(std::string query, std::string stmtName, StatementType stmtType)
{
    // cppcheck-suppress unreadVariable
    auto span = oes::core::OtelSpan(
        "Connection." + std::string(__func__),
        {{"query", query}});
    const auto stmtIt = statements_.find(stmtName);
    if (stmtIt != statements_.cend())
    {
        if (stmtIt->second.getSql() == query) {
            // Return cached statement
            return stmtIt->second;
        }
        // Statement is already prepared but with different SQL query (change name or use auto generated)
        throw std::runtime_error(fmt::format("Statement with name '{}' is already prepared with different SQL query", stmtName));
    }

    // May throw exception on preparing step
    Statement stmt(shared_from_this(), std::move(query), stmtName, stmtType);
    return statements_.emplace(std::move(stmtName), std::move(stmt)).first->second;
}

/*!
* \brief Gets a prepared statement.
* \param stmtName the name of the prepared statement
* \throws std::runtime_error if the statement does not exist
*/
Statement Connection::getPrepared(const std::string& stmtName) const
{
    const auto stmtIt = statements_.find(stmtName);
    if (stmtIt != statements_.cend()) {
        return stmtIt->second;
    }

    throw std::runtime_error(fmt::format("Statement with name '{}' does not exist", stmtName));
}

/*!
* \brief Tries to get a prepared statement.
* \param stmtName the name of the prepared statement
* \return the `Statement` object or `std::nullopt` when it does not exist.
*/
std::optional<Statement> Connection::tryGetPrepared(const std::string& stmtName) const
{
    const auto stmtIt = statements_.find(stmtName);
    return stmtIt == statements_.cend() ? std::nullopt : std::make_optional(stmtIt->second);
}

/*!
 * \return the name of the user that initiated the connection.
 */
std::string Connection::user_name() const {
    return std::string(PQuser(conn_.get()));
}

/*!
 * \return the name of the user that initiated the connection.
 */
const PhysicalName& Connection::user_pname() const {
    if (user_name_.empty()) {
        user_name_ = PhysicalName::parse(user_name(), oes::postgresql::relational::NONE_CASE_SENSITIVE);
    }
    return user_name_;
}

/*!
 * \return the current database the user is connected to.
 * \note wraps PQdb() call
 */
PhysicalName Connection::db() const {
    return PhysicalName::asIs(PQdb(conn_.get()));
}

/*!
 * \return the current server host.
 * \note wraps PQhost() call
 */
std::string Connection::host() const {
    return std::string(PQhost(conn_.get()));
}

/*!
 * \return the port used to connect to server host.
 * \note wraps PQport() call
 */
std::string Connection::port() const {
    return std::string(PQport(conn_.get()));
}

/*!
 * \return the options used by the connection.
 * \note wraps PQoptions() call
 */
std::string Connection::options() const {
    return std::string(PQoptions(conn_.get()));
}

/*!
 * \return the current schema the connection is using.
 */
PhysicalName Connection::current_schema() const {
    PGresult_uptr res = execute("select current_schema()");
    if (PQntuples(res.get()) != 1) {
        throw PostgreSQLException(PG_42P15, "No current schema");
    }
    std::string schema_name = PQgetvalue(res.get(), 0, 0);
    std::string quoted_schema_name = schema_name.empty()? schema_name : "\"" + schema_name + "\"";
    return PhysicalName::parse(quoted_schema_name, oes::postgresql::relational::IMPLICIT_LOWERCASE);
}

/*!
 * \return the current schema the connection is using.
 */
PhysicalNameVec Connection::current_schemas() const {
    PGresult_uptr res = execute("select unnest(current_schemas(false))");
    int nt = PQntuples(res.get());
    if (nt == 0) {
        throw PostgreSQLException(PG_42P15, "No current schemas");
    }
    PhysicalNameVec schemas;
    schemas.reserve(nt);
    for (int i = 0; i < nt; ++i) {
        std::string schema_name = PQgetvalue(res.get(), i, 0);
        std::string quoted_schema_name = schema_name.empty() ? schema_name : "\"" + schema_name + "\"";
        schemas.push_back(PhysicalName::parse(quoted_schema_name, oes::postgresql::relational::IMPLICIT_LOWERCASE));
    }
    return schemas;
}

/*!
 * \brief Gets this connection's search-path as a \em pretty string.
 *
 * \return the search path.
 *
 * \note Prefer current_schemas() for programmatic access.
 */
std::string Connection::search_path() const {
    bool first = true;
    std::ostringstream oss;
    PhysicalNameVec schemas = Connection::current_schemas();
    for (const PhysicalName& schema : schemas) {
        if (first) {
            first = false;
        } else {
            oss << ", ";
        }
        oss << schema.quotedName();
    }
    return oss.str();
}

/*!
 * \brief Changes the current PostgreSQL schema.
 *
 * Sets the current PostgreSQL connection's search-path.
 *
 * \param schema the new schema to be first in the search-path.
 * \param add_extensions_schemas (defaults to true), if true, implicitly
 *     append all schemas to find extensions registered with this connection.
 * \return true if the current schema was changed; false otherwise.
 *
 * \sa registered_extension_schemas()
 */
bool Connection::set_current_schema(
    const PhysicalName& schema, bool add_extensions_schemas
) {
    OES_ASSERT(schema.isValid(), return false;);
    if (add_extensions_schemas && !extension_schemas_.empty()) {
        return set_current_schemas({ schema }, true);
    }
    // FIXME: SQL injection
    if (!exec_ddl(*this, "SET search_path TO {}", schema.quotedName())) {
        return false;
    }
    // As 'SET search_path TO' is a simple config setup, it can never fail.
    // Another round-trip is needed to make sure the schema has been changed.
    return (schema == current_schema());
}

/*!
 * \brief Changes this PostgreSQL connection's search-path.
 *
 * Sets the current PostgreSQL connection's search-path.
 *
 * \param schemas the new schemas to be first in the search-path.
 * \param add_extensions_schemas (defaults to true), if true, implicitly
 *     append all schemas to find extensions registered with this connection.
 * \return true if the search-path was changed; false otherwise.
 *
 * \sa registered_extension_schemas()
 */
bool Connection::set_current_schemas(
    const PhysicalNameVec& schemas, bool add_extensions_schemas
) {
    std::set<PhysicalName> unique_schemas;
    std::vector<std::string> schema_names;
    PhysicalNameVec expected_search_path;

    auto append_schemas = [&](const PhysicalNameVec& to_append) {
        for (const auto& schema : to_append) {
            if (!schema.isValid()) {
                continue; // FIXME: Why??? Some tests fail if asserted instead
            }
            auto unique = unique_schemas.insert(schema).second;
            if (unique) {
                expected_search_path.emplace_back(schema);
                // FIXME: SQL injection
                schema_names.emplace_back(schema.quotedName());
            }
        }
    };

    append_schemas(schemas);

    if (add_extensions_schemas && !extension_schemas_.empty()) {
        append_schemas(extension_schemas_);
    }

    std::string new_search_path = oes::core::StringUtils::join(
        schema_names, ", ", true
    );
    if (!exec_ddl(*this, "SET search_path TO {}", new_search_path)) {
        return false;
    }

    // As 'SET search_path TO' is a simple config setup, it can never fail.
    // Another round-trip is needed to make sure the schema has been changed.
    return (expected_search_path == current_schemas());
}

/**
 * \brief Performs garbage collection on the database.
 * \return true when database has successfully optimized; false otherwise.
 * \details VACUUM database, cannot be executed inside a transaction block.
 */
bool Connection::vacuum() noexcept
{
    try {
        return exec_ddl(*this, "VACUUM");
    }
    catch (const std::exception& ex) {
        OES_ERROR(ex.what());
    }
    catch (...) {
        OES_ERROR("Unknown error while performing database cleanup (garbage collection)");
    }
    return false;
}

/*!
 * \brief Resets the communication channel to the server.
 */
bool Connection::reset() {
    auto mt_guard = make_multi_threads_guard();
    PQreset(conn_.get());
    return true;
}

/*!
 * \brief Resets the communication channel to the server.
 * \param handler the new handler to get notification messages. Should not be null.
 * \return true if successfully set, when no handler previously existed; false otherwise.
 */
bool Connection::set_notice_handler(NoticeHandler handler) {
    if (!notice_handler_ && handler) {
        notice_handler_ = std::move(handler);
        return true;
    }
    return false; // already set
}

//! \internal
void Connection::notice_receiver(void* arg, const PGresult* res) {
    OES_ASSERT(arg && res, return;);
    Connection* self = reinterpret_cast<Connection*>(arg);
    if (!self->notice_handler_) {
        return; // avoid default handler printing to stderr
    }

    switch (PQresultStatus(res)) {
    case PGRES_NONFATAL_ERROR:
        break;
    default:
        return; // not a notice...
    }

    char* severity = PQresultErrorField(res, PG_DIAG_SEVERITY_NONLOCALIZED); // post 9.6 only
    if (!severity || *severity == '\0') {
        severity = PQresultErrorField(res, PG_DIAG_SEVERITY); // pre 9.6
        if (!severity || *severity == '\0') {
            return; // ignore it...
        }
    }

    NoticeSeverity sev = eNONE;

    switch (*severity) {
    case 'D': if (0 == strcmp(severity, "DEBUG"))   sev = eDEBUG; break;
    case 'E': if (0 == strcmp(severity, "ERROR"))   sev = eERROR; break;
    case 'F': if (0 == strcmp(severity, "FATAL"))   sev = eFATAL; break;
    case 'I': if (0 == strcmp(severity, "INFO"))    sev = eINFO; break;
    case 'L': if (0 == strcmp(severity, "LOG"))     sev = eLOG; break;
    case 'N': if (0 == strcmp(severity, "NOTICE"))  sev = eNOTICE; break;
    case 'P': if (0 == strcmp(severity, "PANIC"))   sev = ePANIC; break;
    case 'W': if (0 == strcmp(severity, "WARNING")) sev = eWARNING; break;
    }

    OES_ASSERT(sev != eNONE, return;);

    char* primary = PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY);
    char* detail = PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL);

    /*
    ** Other "things" one can get from PGresult
    **
    char* hint = PQresultErrorField(res, PG_DIAG_MESSAGE_HINT);
    char* schema = PQresultErrorField(res, PG_DIAG_SCHEMA_NAME);
    char* table = PQresultErrorField(res, PG_DIAG_TABLE_NAME);
    char* column = PQresultErrorField(res, PG_DIAG_COLUMN_NAME);
    char* message = PQresultErrorMessage(res);
    // enum PGVerbosity { PQERRORS_TERSE, PQERRORS_DEFAULT, PQERRORS_VERBOSE }
    // enum PGContextVisibility { PQSHOW_CONTEXT_NEVER, PQSHOW_CONTEXT_ERRORS, PQSHOW_CONTEXT_ALWAYS }
    char* verbose = PQresultVerboseErrorMessage(res, PQERRORS_VERBOSE, PQSHOW_CONTEXT_ALWAYS);
    */

    try {
        self->notice_handler_(res, sev, primary, detail);
    } catch (...) {
        // called from C-code / libPQ, cannot propagate C++ exceptions out
    }
}

/*!
* \brief escape a string as literal.
*
* Single quotes will be added around the returned string.
* \return a copy of the escaped string \p to_escape with surrounding quotes.
*/
std::string Connection::escapeLiteral(const std::string& to_escape) {
    char* esc_chr = PQescapeLiteral(pq_connection(), to_escape.c_str(), to_escape.size());
    std::string esc_str(esc_chr);
    PQfreemem(esc_chr);
    return esc_str;
}

/*!
* \brief escape a string for use as an SQL identifier.
*
* Double quotes will be added around the returned string.
* \return a copy of the escaped string \p to_escape with surrounding quotes.
*/
std::string Connection::escapeIdentifier(const std::string& to_escape) {
    char* esc_chr = PQescapeIdentifier(pq_connection(), to_escape.c_str(), to_escape.size());
    std::string esc_str(esc_chr);
    PQfreemem(esc_chr);
    return esc_str;
}

/*!
 * \brief Gets the list of successfully registered extensions.
 *
 * \return registered extension. Can be empty.
 */
std::vector<std::string> Connection::registered_extensions() const {
    std::vector<std::string> exts;
    for (const auto& entry : extensions_) {
        if (entry.second.second) {
            exts.emplace_back(entry.first);
        }
    }
    return exts;
}

/*!
 * \brief Gets the list of schemas containing all successfully registered extensions.
 *
 * These schema(s) are implicitly added on calls to set_current_schema() and
 * set_current_schemas() by default.
 *
 * \return deduplicated list of extension schemas. Can be empty.
 */
PhysicalNameVec Connection::registered_extension_schemas() const {
    return extension_schemas_;
}

/*!
 * \brief Finds and registers a PostgreSQL extension with this connection.
 *
 * Schema(s) of successfully registered extensions are implicitly added on calls
 * to set_current_schema() and set_current_schemas() by default, and returned by
 * registered_extension_schemas().
 *
 * \param ext_name extension name. Case-sensitive (typically lowercase).
 * \param required (defaults to true) If true, sets verbose_on_not_found to
 *     true. Therefore not finding a non-required extension is silent.
 * \return true when found and registered (or already registered); false otherwise.
 */
bool Connection::register_extension(const std::string& ext_name, bool required) {
    PhysicalName schema;

    auto ext_found = extensions_.find(ext_name);
    if (ext_found != extensions_.end()) {
        // known extension
        if (ext_found->second.second) {
            // already registered,
            // which implies in extension_schemas_ too
            return true;
        } else {
            // make it registered,
            // but must still decide whether to add to extension_schemas_
            ext_found->second.second = true;
            schema = ext_found->second.first;
        }
    }

    if (!schema.isValid()) {
        // verbose only if required
        schema = find_extension_schema(ext_name, required);
        if (schema.isValid()) {
            extensions_.emplace(ext_name, std::make_pair(schema, true));
        } else {
            return false; // extension not found
        }
    }

    for (const auto& ext_schema : extension_schemas_) {
        if (ext_schema == schema) {
            return true; // already there
        }
    }

    extension_schemas_.emplace_back(schema);
    return true;
}

/*!
 * \brief Wrapper around find_extension_schema(), adding memoization.
 *
 * \param ext_name extension name. Case-sensitive (typically lowercase).
 * \param verbose_on_not_found (defaults to true) Whether to issue an error
 *     when \a ext_name is not found (i.e. not installed in this database).
 * \return if extension was found (in DB the 1st time, in cache later); false otherwise.
 */
PhysicalName Connection::get_extension_schema(
    const std::string& ext_name, bool verbose_on_not_found
) {
    auto ext_found = extensions_.find(ext_name);
    if (ext_found != extensions_.end()) {
        return ext_found->second.first;
    }

    auto schema = find_extension_schema(ext_name, verbose_on_not_found);
    if (!schema.isValid()) {
        return PhysicalName::invalid();
    }

    extensions_.emplace(ext_name, std::make_pair(schema, false));
    return schema;
}

/*!
 * \brief Finds the schema where a given extension was installed into.
 *
 * Most extensions are installed in the Public schema, but can be installed elsewhere.
 *
 * \param ext_name extension name. Case-sensitive (typically lowercase).
 * \param verbose_on_not_found (defaults to true) Whether to issue an error
 *     when \a ext_name is not found (i.e. not installed in this database).
 * \return if extension was found; false otherwise.
 *
 * \note Prefer get_extension_schema(), which unlike this method, caches the results.
 */
PhysicalName Connection::find_extension_schema(
    const std::string& ext_name, bool verbose_on_not_found
) {
    std::string sql = fmt::format(R"sql(
SELECT n.nspname
  FROM pg_namespace n
  JOIN pg_extension e ON e.extnamespace = n.oid
 WHERE e.extname = '{}'
)sql",
        ext_name // FIXME: SQL Injection. Do proper binding
    );

    PGresult_uptr res = execute(sql);
    const int ntuples = PQntuples(res.get());
    if (ntuples == 0) {
        if (verbose_on_not_found) {
            OES_ERROR(
                    "Extension '{}' not found. Please add it on this database:\n"
                    "CREATE EXTENSION \"{}\";"
                , ext_name, ext_name
            );
        }
        return PhysicalName::invalid();
    }

    OES_ASSERT(ntuples == 1, return PhysicalName::invalid(););
    OES_ASSERT(PQgetisnull(res.get(), 0, 0) != 1, return PhysicalName::invalid(););

    const char* schema = PQgetvalue(res.get(), 0, 0);
    OES_ASSERT(schema, return PhysicalName::invalid(););

    return PhysicalName::parse(schema, oes::postgresql::relational::IMPLICIT_LOWERCASE);
}

/*!
 * \brief Begins new transaction
 * \param read_only if `true`, transaction will be read only
 * \return Transaction object
 */
Transaction Connection::begin_transaction(bool read_only)
{
    return Transaction(shared_from_this(), read_only);
}

/*!
 * Indicates the current status of transactions
 * As postgres support only one transaction per connection
 * It allows to know if starting a new transaction is possible (eTRANSACTIONIDLE)
 */
TransactionStatus Connection::transaction_status() const{
    auto mt_guard = make_multi_threads_guard();
    const PGTransactionStatusType type = PQtransactionStatus(conn_.get());
    switch(type){
        case PQTRANS_IDLE:
            [[fallthrough]];
        case PQTRANS_ACTIVE:
            return TransactionStatus::eIdle;
        case PQTRANS_INTRANS:
            return TransactionStatus::eActive;
        case PQTRANS_INERROR:
            return TransactionStatus::eOnError;
        case PQTRANS_UNKNOWN:
           return TransactionStatus::eUnknown;
    }
    return TransactionStatus::eUnknown;
}

VersionNumber Connection::client_version() const {
    int v = PQlibVersion();
    int major = v / 10000;
    v -= (major * 10000);
    int minor = v / 100;
    v -= (minor * 100);
    int patch = v;
    return VersionNumber(major, minor, patch);
}

VersionNumber Connection::server_version() const {
    int v = PQserverVersion(conn_.get());
    int major = v / 10000;
    v -= (major * 10000);
    int minor = v / 100;
    v -= (minor * 100);
    int patch = v;
    return VersionNumber(major, minor, patch);
}

VersionNumber Connection::protocol_version() const {
    int v = PQprotocolVersion(conn_.get());
    return VersionNumber(v, 0, 0);
}

std::string Connection::server_version_info() const {
    auto mt_guard = make_multi_threads_guard();
    PGresult_uptr res(PQexec(conn_.get(), "select version()"));
    ExecStatusType st = PQresultStatus(res.get());
    OES_ASSERT(st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK, return {};);
    OES_ASSERT(PQntuples(res.get()) == 1, return {};);
    OES_ASSERT(PQgetisnull(res.get(), 0, 0) != 1, return {};);

    const char* info = PQgetvalue(res.get(), 0, 0);
    OES_ASSERT(info, return {};);

    return info;
}

std::string Connection::get_parameter(const std::string& name) const {
    OES_ASSERT(!name.empty(), return {};);
    const char* cstr = PQparameterStatus(conn_.get(), name.c_str());
    return cstr? std::string(cstr): std::string();
}

std::map<std::string, std::string> Connection::get_parameters() const {
    std::map<std::string, std::string> params;
    auto fetch_param = [&params, this](const char* param) {
        const char* cstr = PQparameterStatus(conn_.get(), param);
        if (cstr && *cstr) {
            params.emplace(param, cstr);
        }
    };
    fetch_param("server_version");
    fetch_param("server_encoding");
    fetch_param("client_encoding");
    fetch_param("application_name");
    fetch_param("is_superuser");
    fetch_param("session_authorization");
    return params;
}

/*!
* \brief Makes a guard for prevent concurrent usages of connection.
* \throws std::runtime_error when trying to use connection from multiple threads.
*/
core::ScopedFunctor Connection::make_multi_threads_guard() const
{
    if (usage_flag.exchange(true, std::memory_order_acq_rel) == false)
    {
        return core::ScopedFunctor([this]() {
            usage_flag = false;
        });
    }
    
    throw StatusConnectionException("Connection busy.");
}

/****************************************************************************/

/*!
 * \ingroup oes_postgresql
 * \class oes::oracle::PostgresConnectParams oes/postgresql/Connection.h
 *
 * \brief Helper to create a new PostgreSQL connection
 *
 * Gathers all needed variables of a connection and creates it with PQconnectdbParams()
 */

PostgresConnectParams::PostgresConnectParams(size_t reserve) {
    keys_.reserve(reserve);
    vals_.reserve(reserve);
}

/*!
* \brief Parses a PostgreSQL connection string. For accepted formats, see https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING
* \param connection_string the connection string to parse. If this is empty, returns an empty PostgresConnectParams object
* \param skip_vals_if_empty whether or not to track connection values that are not present in the connection string
* \return the parsed connection parameters
* \exception PostgreSQLException: thrown when the parse fails
*/
PostgresConnectParams PostgresConnectParams::parse(const std::string& connection_string, bool skip_vals_if_empty) {
    PostgresConnectParams result;
    if (connection_string.empty()) {
        return result;
    }

    char* error_message;
    auto conn_info = PQconninfoParse(connection_string.c_str(), &error_message);

    if (error_message) {
        OES_ERROR("Error parsing connection string: {}", error_message);
        throw PostgreSQLException(PG_42601/*=syntax_error*/, error_message);
    }

    if (conn_info == nullptr) {
        std::string message = "Out of memory trying to parse connection string";
        OES_ERROR(message);
        throw PostgreSQLException(PG_53200/*=out_of_memory*/, message.c_str());
    }

    // The array pointed to by conn_info is terminated by an entry with a null keyword
    for (auto iter = conn_info; iter != nullptr && iter->keyword != nullptr; ++iter) {
        if (skip_vals_if_empty && iter->val == nullptr) {
            continue;
        }

        result.keys_.push_back(std::string(iter->keyword));
        result.vals_.push_back(iter->val == nullptr
            ? std::string()
            : std::string(iter->val));
    }

    PQconninfoFree(conn_info);

    return result;
}

/*!
* \brief Attempts to parse a PostgreSQL connection string. For accepted formats, see https://www.postgresql.org/docs/current/libpq-connect.html#LIBPQ-CONNSTRING
* \param connection_string the connection string to parse. If this is empty, returns an empty PostgresConnectParams object
* \param result the variable that will contain the result if parsing is successful
* \param skip_vals_if_empty whether or not to track connection values that are not present in the connection string
* \return `true` if the parse was successful, in which case \a result will contain the parsed information. Otherwise, returns `false` and the value of \a result is undefined.
*/
bool PostgresConnectParams::tryParse(const std::string& conn_str, PostgresConnectParams& result, bool skip_vals_if_empty) noexcept {
    try {
        result = parse(conn_str, skip_vals_if_empty);
        return true;
    } catch (const PostgreSQLException& e) {
        // parse already logs these errors, no need for duplicate logging here.
        return false;
    } catch (const std::exception &e) {
        OES_ERROR("Unexpected exception during PostgresConnectParams::tryParse: {}", e.what());
        return false;
    } catch (...) {
        OES_ERROR("Unknown error during PostgresConnectParams::tryParse");
        return false;
    }
}

/*!
* \brief Adds a parameter to the connection information
* \param key the parameter key to add
* \param val the parameter value to add
* \param skip_if_empty if true and \a val is empty, no operation is performed
* \note If \a key is not a valid PostgreSQL parameter key, no operation is performed
*/
void PostgresConnectParams::add(const connect_params::ParamKey key, const std::string& val, bool skip_if_empty) {
    std::string key_str = paramKeyToString(key);
    if (key_str.empty()) {
        OES_ERROR("Unknown PostgreSQL connection option");
        return;
    }

    add(key_str, val, skip_if_empty);
}

/*!
* \brief Adds a parameter to the connection information
* \param key the parameter key to add
* \param val the parameter value to add
* \param skip_if_empty if true and \a val is empty, no operation is performed
*/
void PostgresConnectParams::add(const std::string& key, const std::string& val, bool skip_if_empty) {
    if (skip_if_empty && val.empty()) {
        return;
    }

    keys_.emplace_back(std::move(key));
    vals_.emplace_back(std::move(val));
}

/*!
* \brief Updates a parameter in the connection information, or adds a new parameter if it doesn't exist
* \param key the parameter key to add or update
* \param val the parameter value to add or update
* \param skip_if_empty if true and \a val is empty, no operation is performed
* \note If \a key is not a valid PostgreSQL parameter key, no operation is performed
*/
void PostgresConnectParams::addOrUpdate(const connect_params::ParamKey key, const std::string& val, bool skip_if_empty) {
    std::string key_str = paramKeyToString(key);
    if (key_str.empty()) {
        OES_ERROR("Unknown PostgreSQL connection option");
        return;
    }

    addOrUpdate(key_str, val, skip_if_empty);
}

/*!
* \brief Updates a parameter in the connection information, or adds a new parameter if it doesn't exist
* \param key the parameter key to add or update
* \param val the parameter value to add or update
* \param skip_if_empty if true and \a val is empty, no operation is performed
*/
void PostgresConnectParams::addOrUpdate(const std::string& key, const std::string& val, bool skip_if_empty) {
    if (skip_if_empty && val.empty()) {
        return;
    }

    auto pos = std::find(keys_.begin(), keys_.end(), key);
    if (pos == keys_.end()) {
        keys_.emplace_back(std::move(key));
        vals_.emplace_back(std::move(val));
    } else {
        auto index = pos - keys_.begin();
        vals_[index] = std::move(val);
    }
}

/*!
* \brief Registers a dynamic parameter, which will be fetched when attempting to connect
* \param key the parameter key to add or update
* \param fn the function that will generate the parameter value
* \note If \a key is not a valid PostgreSQL parameter key, no operation is performed
* \note This enables parameters to be refreshed each time a new connection is established
*/
void PostgresConnectParams::registerDynamicParam(const connect_params::ParamKey key, const std::function<std::string()>& fn) {
    std::string key_str = paramKeyToString(key);
    if (key_str.empty()) {
        OES_ERROR("Unknown PostgreSQL connection option");
        return;
    }

    registerDynamicParam(key_str, fn);
}

/*!
* \brief Registers a dynamic parameter, which will be fetched when attempting to connect
* \param key the parameter key to add or update
* \param fn the function that will generate the parameter value
* \note If \a key is not a valid PostgreSQL parameter key, no operation is performed
*/
void PostgresConnectParams::registerDynamicParam(const std::string& key, const std::function<std::string()>& fn) {
    dynamicParams_.insert({key, fn});
}

/*!
* \brief Checks whether the params are empty
* \return `true` if this object contains no parameters; `false` otherwise
*/
const bool PostgresConnectParams::empty() const {
    return keys_.empty() && vals_.empty();
}

/*!
* \brief Connect to the database
* \return the PostgreSQL connection object
*/
PGconn* PostgresConnectParams::connectdbParams() const {
    const size_t n = keys_.size() + dynamicParams_.size();
    std::vector<const char*> pg_keys_(n + 1);
    std::vector<const char*> pg_vals_(n + 1);
    std::vector<const char*> cleanup_vals;

    try {
        size_t i = 0;
        for (; i < keys_.size(); ++i) {
            pg_keys_[i] = keys_[i].c_str();
            pg_vals_[i] = vals_[i].c_str();
        }

        // Note that these can override any values set in the above loop
        for (const auto& [key, fn] : dynamicParams_) {
            pg_keys_[i] = key.c_str();

            // Need to use strdup because the result will go out of scope before we use it.
            // This means they should be manually freed! Other pg_keys_ and pg_vals_ entires should not be cleaned
            // up the same way because they point to values in member variables that aren't going out of scope.
            pg_vals_[i] = strdup(fn().c_str());
            cleanup_vals.push_back(pg_vals_[i]);
            ++i;
        }

        auto result = PQconnectdbParams(pg_keys_.data(), pg_vals_.data(), 0);
        cleanupDynamicVals(cleanup_vals);
        return result;
    } catch (const std::exception& e) {
        OES_ERROR("Connection error: {}", e.what());
        cleanupDynamicVals(cleanup_vals);
        throw;
    } catch (...) {
        OES_ERROR("Unknown connection error");
        cleanupDynamicVals(cleanup_vals);
        throw;
    }
}

/*!
* \brief Connect to the database
* \return the connection object
*/
Connection_ptr PostgresConnectParams::createConnection() const {
    PGconn* conn = connectdbParams();
    oes::postgresql::PGconn_uptr c(conn);
    if (PQstatus(conn) != CONNECTION_OK) {
        throw PostgreSQLException(PG_08006/*=connection_failure*/, PQerrorMessage(conn));
    }
    return std::make_shared<Connection>(std::move(c));
}

/****************************************************************************/

/*!
 * \class ScopedSetCurrentSchema oes/postgresql/Connection.h
 * \brief RAII class that alter the current schema for a connection,
 *    until the instance goes out of scope.
 */

ScopedSetCurrentSchema::ScopedSetCurrentSchema(
    Connection& conn, const oes::postgresql::relational::PhysicalName& schema
) : conn_(conn)
, old_schema_("public")
{
    const oes::postgresql::relational::PhysicalName& old = conn.current_schema();
    if( old.isValid() ){
        old_schema_ = old;
    }
    conn.set_current_schema(schema);
}

ScopedSetCurrentSchema::~ScopedSetCurrentSchema() {
    conn_.set_current_schema(old_schema_);
}

/****************************************************************************/

} // namespace postgresql
} // namespace oes
