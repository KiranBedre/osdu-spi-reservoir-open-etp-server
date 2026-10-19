// ============================================================================
// Copyright 2019-2023 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <cstddef>
#include <type_traits>
#include <map>
#include <cctype>
#include <stdexcept>
#include <variant>
#include <algorithm>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/endian/conversion.hpp>
#include <libpq-fe.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Statement.h"
#include "Connection.h"
#include "SqlLogger.h"

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Guid.h>

#include "ErrorCodes.h"

//DBG
//#include <iostream>
//DBG

namespace {

// Oid of types are taken from toolworks\postgresql\...\...\include\server\catalog\pg_type_d.h
enum class PostgresType : int {
    BOOLOID = 16,
    BYTEAOID = 17,
    CHAROID = 18,
    NAMEOID = 19,
    INT8OID = 20,
    INT2OID = 21,
    INT2VECTOROID = 22,
    INT4OID = 23,
    REGPROCOID = 24,
    TEXTOID = 25,
    OIDOID = 26,
    TIDOID = 27,
    XIDOID = 28,
    CIDOID = 29,
    OIDVECTOROID = 30,
    JSONOID = 114,
    XMLOID = 142,
    PGNODETREEOID = 194,
    PGNDISTINCTOID = 3361,
    PGDEPENDENCIESOID = 3402,
    PGMCVLISTOID = 5017,
    PGDDLCOMMANDOID = 32,
    POINTOID = 600,
    LSEGOID = 601,
    PATHOID = 602,
    BOXOID = 603,
    POLYGONOID = 604,
    LINEOID = 628,
    FLOAT4OID = 700,
    FLOAT8OID = 701,
    UNKNOWNOID = 705,
    CIRCLEOID = 718,
    CASHOID = 790,
    MACADDROID = 829,
    INETOID = 869,
    CIDROID = 650,
    MACADDR8OID = 774,
    ACLITEMOID = 1033,
    BPCHAROID = 1042,
    VARCHAROID = 1043,
    DATEOID = 1082,
    TIMEOID = 1083,
    TIMESTAMPOID = 1114,
    TIMESTAMPTZOID = 1184,
    INTERVALOID = 1186,
    TIMETZOID = 1266,
    BITOID = 1560,
    VARBITOID = 1562,
    NUMERICOID = 1700,
    REFCURSOROID = 1790,
    REGPROCEDUREOID = 2202,
    REGOPEROID = 2203,
    REGOPERATOROID = 2204,
    REGCLASSOID = 2205,
    REGTYPEOID = 2206,
    REGROLEOID = 4096,
    REGNAMESPACEOID = 4089,
    UUIDOID = 2950,
    LSNOID = 3220,
    TSVECTOROID = 3614,
    GTSVECTOROID = 3642,
    TSQUERYOID = 3615,
    REGCONFIGOID = 3734,
    REGDICTIONARYOID = 3769,
    JSONBOID = 3802,
    JSONPATHOID = 4072,
    TXID_SNAPSHOTOID = 2970,
    INT4RANGEOID = 3904,
    NUMRANGEOID = 3906,
    TSRANGEOID = 3908,
    TSTZRANGEOID = 3910,
    DATERANGEOID = 3912,
    INT8RANGEOID = 3926,
    RECORDOID = 2249,
    RECORDARRAYOID = 2287,
    CSTRINGOID = 2275,
    ANYOID = 2276,
    ANYARRAYOID = 2277,
    VOIDOID = 2278,
    TRIGGEROID = 2279,
    EVTTRIGGEROID = 3838,
    LANGUAGE_HANDLEROID = 2280,
    INTERNALOID = 2281,
    OPAQUEOID = 2282,
    ANYELEMENTOID = 2283,
    ANYNONARRAYOID = 2776,
    ANYENUMOID = 3500,
    FDW_HANDLEROID = 3115,
    INDEX_AM_HANDLEROID = 325,
    TSM_HANDLEROID = 3310,
    TABLE_AM_HANDLEROID = 269,
    ANYRANGEOID = 3831,
    BOOLARRAYOID = 1000,
    BYTEAARRAYOID = 1001,
    CHARARRAYOID = 1002,
    NAMEARRAYOID = 1003,
    INT8ARRAYOID = 1016,
    INT2ARRAYOID = 1005,
    INT2VECTORARRAYOID = 1006,
    INT4ARRAYOID = 1007,
    REGPROCARRAYOID = 1008,
    TEXTARRAYOID = 1009,
    OIDARRAYOID = 1028,
    TIDARRAYOID = 1010,
    XIDARRAYOID = 1011,
    CIDARRAYOID = 1012,
    OIDVECTORARRAYOID = 1013,
    JSONARRAYOID = 199,
    XMLARRAYOID = 143,
    POINTARRAYOID = 1017,
    LSEGARRAYOID = 1018,
    PATHARRAYOID = 1019,
    BOXARRAYOID = 1020,
    POLYGONARRAYOID = 1027,
    LINEARRAYOID = 629,
    FLOAT4ARRAYOID = 1021,
    FLOAT8ARRAYOID = 1022,
    CIRCLEARRAYOID = 719,
    MONEYARRAYOID = 791,
    MACADDRARRAYOID = 1040,
    INETARRAYOID = 1041,
    CIDRARRAYOID = 651,
    MACADDR8ARRAYOID = 775,
    ACLITEMARRAYOID = 1034,
    BPCHARARRAYOID = 1014,
    VARCHARARRAYOID = 1015,
    DATEARRAYOID = 1182,
    TIMEARRAYOID = 1183,
    TIMESTAMPARRAYOID = 1115,
    TIMESTAMPTZARRAYOID = 1185,
    INTERVALARRAYOID = 1187,
    TIMETZARRAYOID = 1270,
    BITARRAYOID = 1561,
    VARBITARRAYOID = 1563,
    NUMERICARRAYOID = 1231,
    REFCURSORARRAYOID = 2201,
    REGPROCEDUREARRAYOID = 2207,
    REGOPERARRAYOID = 2208,
    REGOPERATORARRAYOID = 2209,
    REGCLASSARRAYOID = 2210,
    REGTYPEARRAYOID = 2211,
    REGROLEARRAYOID = 4097,
    REGNAMESPACEARRAYOID = 4090,
    UUIDARRAYOID = 2951,
    PG_LSNARRAYOID = 3221,
    TSVECTORARRAYOID = 3643,
    GTSVECTORARRAYOID = 3644,
    TSQUERYARRAYOID = 3645,
    REGCONFIGARRAYOID = 3735,
    REGDICTIONARYARRAYOID = 3770,
    JSONBARRAYOID = 3807,
    JSONPATHARRAYOID = 4073,
    TXID_SNAPSHOTARRAYOID = 2949,
    INT4RANGEARRAYOID = 3905,
    NUMRANGEARRAYOID = 3907,
    TSRANGEARRAYOID = 3909,
    TSTZRANGEARRAYOID = 3911,
    DATERANGEARRAYOID = 3913,
    INT8RANGEARRAYOID = 3927,
    CSTRINGARRAYOID = 1263
};

using ValHolder = std::variant<
    std::nullptr_t,
    int16_t,
    int32_t,
    int64_t,
    bool,
    float,
    double,
    std::string,
    std::vector<char>,
    oes::core::Guid,
    std::vector<int32_t>,
    std::vector<int64_t>,
    std::vector<oes::core::Guid>
>;

void prepareStatement(oes::postgresql::Connection_ptr connection, const std::string& stmtName, const std::string& sql)
{
    auto mt_guard = connection->make_multi_threads_guard();
    const oes::postgresql::PGresult_uptr res_prepare(PQprepare(connection->pq_connection(), stmtName.c_str(), sql.c_str(), 0, nullptr));
    const ExecStatusType st = PQresultStatus(res_prepare.get());
    if (st != PGRES_COMMAND_OK)
    {
        auto ex = oes::postgresql::PostgreSQLException::fromPGresult(res_prepare);
        if (ex.error_code == oes::postgresql::PG_42P05) {
            // Already prepared - leads to abort transaction (can't be ignored)
            OES_ERROR("Statement '{}' is already prepared", stmtName);
        }
        throw ex;
    }
}

oes::postgresql::PGresult_uptr executePrepared(oes::postgresql::Connection_ptr connection, const std::string& stmtName, const oes::postgresql::Connection::QueryParameters& params, bool binaryResult = false)
{
    oes::postgresql::SqlLogger log("PreparedStmt");

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
    // missing guard
    auto mt_guard = connection->make_multi_threads_guard();
    oes::postgresql::PGresult_uptr res_query(PQexecPrepared(
        connection->pq_connection(), stmtName.c_str(), nParams, paramValues.data(), paramLengths.data(), paramFormats.data(), binaryResult ? 1 : 0
    ));
    const ExecStatusType st = PQresultStatus(res_query.get());
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        log.logError(PQresultErrorMessage(res_query.get()));
        throw oes::postgresql::PostgreSQLException::fromPGresult(res_query);
    }
    return res_query;
}

} // namespace

namespace oes::postgresql {

class Statement::Impl {
public:
    Impl(const Connection_ptr& connection, std::string sql, std::string stmtName)
        : _connection(connection)
        , _sql(std::move(sql))
        , _statementName(std::move(stmtName))
    { }

    virtual ~Impl() {
        Impl::close();
    }

    [[nodiscard]] Connection_ptr getConnection() const
    {
        auto conn = _connection.lock();
        if (!conn) {
            throw std::runtime_error("Internal error: statement is invalid due to destroyed connection");
        }
        return conn;
    }

    virtual void prepare();
    virtual int64_t execute();
    virtual bool step();
    virtual void reset();
    virtual void close();

    void checkGetValue(int index) const
    {
        OES_RELEASE_UNUSED(index);
#ifdef OES_DEBUG_BUILD
        if (!_result) {
            throw std::runtime_error("Result is not defined");
        }
        if (index >= _numColumns || index < 0) {
            throw std::runtime_error("Index is out of range");
        }
#endif
    }

    [[nodiscard]] int64_t getAffectedRows() const
    {
        if (_result)
        {
            const char* affected_rows = PQcmdTuples(_result.get());
            return affected_rows == nullptr ? -1 : std::strtol(affected_rows, nullptr, 10);
        }
        return -1;
    }

    [[nodiscard]] std::string getTableName(Oid tableOid) const {
        // TODO may be changed later to get name by oid from pg_class table
        return std::to_string(tableOid);
    }

    template <typename T>
    std::enable_if_t<std::is_arithmetic<T>::value, bool>
        bind(int position, T value)
    {
        boost::endian::endian_reverse_inplace(value);
        _vh[position] = value;
        return true;
    }

    template <typename T>
    std::enable_if_t<!std::is_arithmetic<T>::value, bool>
        bind(int position, T value) {
        _vh[position] = value;
        return true;
    }

    bool bind(int position, bool value) {
        _vh[position] = value;
        return true;
    }

    template <typename T, std::enable_if_t<sizeof(T) % sizeof(uint32_t) == 0, int> = 0>
    void s_convertToCharVect(int index, PostgresType oid, const std::vector<T>& vect)
    {
        constexpr int header_size = 5;
        constexpr int elem_size_factor = sizeof(T) / sizeof(uint32_t);
        std::vector<uint32_t> arr(header_size + (/*num elements=*/vect.size() *
            (/*size indicator=*/1 + /*elem size in uint32=*/elem_size_factor)));
        arr[0] = boost::endian::native_to_big(1); // ndims
        arr[1] = 0; // has null
        arr[2] = boost::endian::native_to_big(static_cast<int>(oid)); // oid of uint64 type. Still needed if we have the type indicator in query ?
        arr[3] = boost::endian::native_to_big(static_cast<int>(vect.size())); // size dim 0
        arr[4] = boost::endian::native_to_big(1); // starting index of first dim

        const uint32_t elem_size = boost::endian::native_to_big(static_cast<uint32_t>(sizeof(T)));
        int cur_arr_ind = header_size;
        for (auto val : vect)
        {
            arr[cur_arr_ind] = elem_size;
            cur_arr_ind++;
            boost::endian::endian_reverse_inplace(val);
            std::memcpy(&arr[cur_arr_ind], &val, sizeof(T));
            cur_arr_ind += elem_size_factor;
        }
        const char* data = reinterpret_cast<char*>(arr.data());
        _vh[index].emplace<std::vector<char>>(data, data + arr.size() * sizeof(uint32_t));
    }

    void bind(const std::vector<ValHolder>& vh) {
        _vh.clear();
        for (size_t i = 0; i < vh.size(); ++i) {
            if (std::holds_alternative<std::vector<int32_t>>(vh[i])) {
                s_convertToCharVect(static_cast<int>(i + 1), PostgresType::INT4OID, std::get<std::vector<int32_t>>(vh[i]));
            } else if (std::holds_alternative<std::vector<int64_t>>(vh[i])) {
                const auto& val = std::get<std::vector<int64_t>>(vh[i]);
                s_convertToCharVect(static_cast<int>(i + 1), PostgresType::INT8OID, val);
            }
            else {
                _vh[static_cast<int>(i + 1)] = vh[i];
            }
        }
    }

    // Serialize array of T to array of uint32_t for postgres binary format
    template <typename T>
    static std::vector<uint32_t> s_convertArrayToIntVect(std::vector<T> value, size_t elem_size = sizeof(T))
    {
        constexpr int header_size = 5;
        const uint32_t elem_size_factor = static_cast<uint32_t>(elem_size) / sizeof(uint32_t);
        std::vector<uint32_t> ids( header_size + (/*num elements=*/value.size() * (/*size indicator=*/1 + elem_size_factor)));
        ids[0] = boost::endian::native_to_big(1); // ndims
        ids[1] = 0; // has null
        ids[2] = boost::endian::native_to_big(2950); // oid of uuid type. Still needed if we have the type indicator in query ?
        ids[3] = boost::endian::native_to_big(static_cast<int>(value.size())); // size dim 0
        ids[4] = boost::endian::native_to_big(1); // starting index of first dim
        uint32_t elem_size_bi = boost::endian::native_to_big(static_cast<int>(elem_size));
        for(size_t i = 0; i < value.size(); ++i){
            ids[5 + i * 5] = elem_size_bi; // size of uuid
            std::memcpy(&ids[6 + i * 5], value[i].begin(), elem_size);
        }
        return ids;
    }

    static void s_toPreparedQueryParameter(const ValHolder& vh, Connection::QueryParameter& qp)
    {
        std::visit([&qp](auto&& value)
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_arithmetic_v<T>)
            {
                qp.binary_format = true;
                qp.bytes_address = reinterpret_cast<const char*>(&value);
                qp.length = sizeof(T);
            }
            else if constexpr (std::is_same_v<T, std::vector<char>>)
            {
                qp.binary_format = true;
                qp.bytes_address = !value.empty() ? value.data() : nullptr;
                qp.length = static_cast<int>(value.size());
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                qp.binary_format = false;
                qp.bytes_address = value.c_str();
                qp.length = static_cast<int>(value.size());
            }
            else if constexpr (std::is_same_v<T, oes::core::Guid>)
            {
                qp.binary_format = true;
                qp.bytes_address = reinterpret_cast<const char*>(value.begin());
                qp.length = static_cast<int>(oes::core::Guid::static_size());
            }
            else if constexpr (std::is_same_v<T, std::vector<oes::core::Guid>>)
            {
                
                std::vector<uint32_t> ids = s_convertArrayToIntVect(value, oes::core::Guid::static_size());
                qp.binary_format = true;
                qp.length = static_cast<int>(sizeof(uint32_t) * ids.size());
                qp.bytes = std::move(ids);
            }
            else
            {
                qp.binary_format = false;
                qp.bytes_address = nullptr;
                qp.length = 0;
            }
        }, vh);
    }

    template<typename T>
    T get(int index) {
        checkGetValue(index);
        int num_bytes = PQgetlength(_result.get(), _currentRow, index);
        char* value = PQgetvalue(_result.get(), _currentRow, index);
        if (num_bytes > 0 && value) {
            OES_ASSERT(1 == PQfformat(_result.get(), index), return 0;);
            // Process PostgreSQL predefined types first
            Oid value_type_oid = PQftype(_result.get(), index);
            //std::cout << "Oid: " << value_type_oid << " num_bytes: " << num_bytes << "\n";
            switch (static_cast<PostgresType>(value_type_oid)) {
            case PostgresType::INT2OID:
            {
                int16_t t = boost::endian::big_to_native(*reinterpret_cast<int16_t*>(value));
                return T(t);
            }
            case PostgresType::BOOLOID:
                return static_cast<T>(*value);
            case PostgresType::INT4OID:
            case PostgresType::OIDOID:
            {
                int32_t t = boost::endian::big_to_native(*reinterpret_cast<int32_t*>(value));
                return T(t);
            }
            case PostgresType::INT8OID:
            case PostgresType::TIMESTAMPOID:
            case PostgresType::TIMESTAMPTZOID:
            {
                int64_t t = boost::endian::big_to_native(*reinterpret_cast<int64_t*>(value));
                return T(t);
            }
            case PostgresType::FLOAT4OID:
            {
                int32_t t = boost::endian::big_to_native(*reinterpret_cast<int32_t*>(value));
                float t1;
                std::memcpy(&t1, &t, sizeof(float));
                return T(t1);
            }
            case PostgresType::FLOAT8OID:
            {
                int64_t t = boost::endian::big_to_native(*reinterpret_cast<int64_t*>(value));
                double t1;
                std::memcpy(&t1, &t, sizeof(double));
                return T(t1);
            }
            default:
                OES_ASSERT(
                    false,
                    OES_ERROR(
                        "Failed to decode value from PostgreSQL."
                        " Type Oid: {}"
                        " num_bytes: {}"
                        " Requested type: {}"
                        , value_type_oid, num_bytes, typeid(T).name()
                    );

                return T(0);
                );
            } // end of switch for type_of_value
        }
        return T(0);
    }

public:
    Connection_wptr _connection;

    std::string _sql;
    std::string _statementName;

    PGresult_uptr _result;

    int _currentRow = -1;
    int _numRows = 0;
    int _numColumns = 0;

    std::map<int, ValHolder> _vh;
};

void Statement::Impl::prepare() {
    prepareStatement(getConnection(), _statementName, _sql);
}

int64_t Statement::Impl::execute()
{
    Connection::QueryParameters prepared_params;
    prepared_params.resize(_vh.size());
    int i = 0;
    for (auto& cvh : _vh) {
        s_toPreparedQueryParameter(cvh.second, prepared_params[i]);
        ++i;
    }

    try
    {
        _result = executePrepared(getConnection(), _statementName, prepared_params, true);
    }
    catch (const std::exception& ex)
    {
        OES_ERROR("Failed execution of statement '{}': {}", _statementName, ex.what());
        throw;
    }

    ExecStatusType st = PQresultStatus(_result.get());
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        throw PostgreSQLException::fromPGresult(_result);
    }

    _numRows = PQntuples(_result.get());
    _numColumns = PQnfields(_result.get());
    _currentRow = 0;
    return getAffectedRows();
}

bool Statement::Impl::step()
{
    if (_currentRow < 0) {
        return execute() > 0;
    }

    if (_currentRow < _numRows - 1) {
        _currentRow++;
        return true;
    }
    return false;
}

void Statement::Impl::reset() {
    _numRows = 0;
    _numColumns = 0;
    _currentRow = -1;
    _result.reset();
}

void Statement::Impl::close() {
    reset();
    _vh.clear();
}

//---------- CursorStatement --------------
class CursorImpl : public Statement::Impl {
public:
    CursorImpl(const Connection_ptr& connection, std::string sql, std::string stmtName);
    ~CursorImpl() override;

    bool createCursor();
    bool closeCursor();
    bool fetchData();

    static bool s_isSelectQuery(const std::string& sql);

    void prepare() override;
    int64_t execute() override;
    bool step() override;
    void reset() override;
    void close() override;

public:
    unsigned int _chunkSizeInRows = 1000;
    bool _cursorCreated = false;
};

CursorImpl::CursorImpl(const Connection_ptr& connection, std::string sql, std::string stmtName)
    : Statement::Impl(connection, std::move(sql), std::move(stmtName))
{ }

CursorImpl::~CursorImpl() {
    // Any actions after disconnected does not make sense
    if (_connection.lock())
    {
        try {
            closeCursor();
        }
        catch (const std::exception& e) {
            OES_ERROR("~CursorImpl: closeCursor: {}\n{}", e.what(), _sql);
        }
        catch (...) {
            OES_ERROR("~CursorImpl: closeCursor: Unknown exception\n{}", _sql);
        }
    }
}

bool CursorImpl::createCursor() {
    // Once cursor is declared, it cannot be declared again
    if (_cursorCreated) {
        return false;
    }

    Impl::execute();
    _cursorCreated = true;
    return true;
}

bool CursorImpl::closeCursor() {
    if (!_cursorCreated) {
        return false;
    }

    const std::string closeQuery = "CLOSE " + _statementName + ";";
    const auto result = getConnection()->execute(closeQuery);
    if (PGRES_COMMAND_OK != PQresultStatus(result.get())) {
        throw PostgreSQLException::fromPGresult(result);
    }
    _cursorCreated = false;
    return true;
}

bool CursorImpl::fetchData() {
    // reset query results
    _result.reset();
    _numRows = 0;
    _numColumns = 0;

    // create cursor if needed
    if (!_cursorCreated) {
        createCursor();
    }

    // run fetch data query
    const std::string fetchQuery = "FETCH " + std::to_string(_chunkSizeInRows) + " FROM " + _statementName + ";";
    _result = getConnection()->execute(fetchQuery);
    _numRows = PQntuples(_result.get());
    _numColumns = PQnfields(_result.get());
    if (PGRES_TUPLES_OK != PQresultStatus(_result.get())) {
        throw PostgreSQLException::fromPGresult(_result);
    }
    return _numRows > 0 && _numColumns > 0;
}

bool CursorImpl::s_isSelectQuery(const std::string& sql) {
    constexpr std::string_view select("SELECT");
    if (sql.size() < select.size()) {
        return false;
    }

    const auto begin_sql_it = std::find_if_not(sql.cbegin(), sql.cend(), &isspace);
    if (std::distance(begin_sql_it, sql.end()) < static_cast<ptrdiff_t>(select.size())) {
        return false;
    }
    const bool ret = std::equal(select.begin(), select.end(), begin_sql_it, [](const char& c1, const char& c2) {
        return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
    });
    return ret;
}

void CursorImpl::prepare() {
    if (!s_isSelectQuery(_sql)) {
        throw std::runtime_error("In the CURSOR mode, only SELECT queries are allowed");
    }

    // Prepare statement with declare cursor query
    const std::string declareQuery = "DECLARE " + _statementName + " BINARY CURSOR WITH HOLD FOR " + _sql + ";";
    prepareStatement(getConnection(), _statementName, declareQuery);
}

int64_t CursorImpl::execute()
{
    if (!fetchData()) {
        return 0;
    }
    _currentRow = 0;
    return getAffectedRows();
}

bool CursorImpl::step() {
    // if has data just increment to the next and return
    if (_currentRow + 1 < _numRows) {
        _currentRow++;
        return true;
    }
    return execute() > 0;
}

void CursorImpl::reset() {
    closeCursor();
    Impl::reset();
}

void CursorImpl::close() {
    closeCursor();
    Impl::close();
}

/*!
 * \class Statement Statement.h
 * \brief SQL Statement
 * \ingroup
 * \details
 * Class \c Statement is responsible for the preparation of SQL statement for
 * an SQL query, its execution and access to the result set
 */

/*!
 * \brief Prepares statement from the query string, which must contain a single SQL command.
 * \param[in] connection PostgreSQL connection
 * \param[in] sql SQL command
 * \param[in] name statement name
 * \param[in] stmtType statement type
 */
Statement::Statement(const Connection_ptr& connection, std::string sql, std::string name, StatementType stmtType)
{
    if (sql.empty()) {
        throw std::runtime_error("Cannot prepare statement - SQL query is empty");
    }
    if (name.empty()) {
        throw std::runtime_error("Cannot prepare statement - name is empty");
    }

    switch (stmtType)
    {
    case StatementType::Prepared:
        _impl = std::make_shared<Impl>(connection, std::move(sql), std::move(name));
        break;
    case StatementType::PreparedWithCursor:
        _impl = std::make_shared<CursorImpl>(connection, std::move(sql), std::move(name));
        break;
    default:
        throw std::runtime_error("Cannot prepare statement - invalid type");
    }

    _impl->prepare();
}

/*!
* \brief Returns name of prepared statement.
*/
const std::string& Statement::getName() const {
    return _impl->_statementName;
}

/*!
* \brief Returns SQL query.
*/
const std::string& Statement::getSql() const {
    return _impl->_sql;
}

/*!
 * \brief Binds null values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 */
void Statement::bind(int index) {
    _impl->bind(index, nullptr);
}
void Statement::bind(int index, int16_t value) {
    _impl->bind(index, value);
}


/*!
 * \brief Binds integer values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value Integer value bound to the column \a index.
 */
void Statement::bind(int index, int value) {
    _impl->bind(index, value);
}

/*!
 * \brief Binds 64-bits integer values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value  64-bits integer value bound to the column \a index.
 */
void Statement::bind(int index, int64_t value) {
    _impl->bind(index, value);
}

#if defined(__GNUC__) && (__WORDSIZE == 64)
void Statement::bind(int index, long long int value) {
    _impl->bind(index, (int64_t)value);
}
#endif

/*!
 * \brief Binds double values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value Double value bound to the column \a index.
 */
void Statement::bind(int index, double value) {
    _impl->bind(index, value);
}

void Statement::bind(int index, bool value) {
    _impl->bind(index, value);
}

void Statement::bind(int index, float value) {
    _impl->bind(index, value);
}

void Statement::bind(int index, const oes::core::Guid& value) {
    _impl->bind(index, value);
}

/*!
 * \brief Binds string values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value String value bound to the column \a index.
 * \param len Length of the string \a value.
 */
void Statement::bind(int index, const char* value, int len) {
    if (nullptr != value) {
        std::string s = len > 0 ? std::string(value) : std::string("");
       _impl->bind(index, std::move(s));
    }
    else {
        _impl->bind(index, nullptr);
    }
}

/*!
 * \brief Binds arrays of bytes to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value pointer to an array of bytes bound to the column \a index.
 * \param len Length of \a value.
 */
void Statement::bindBytea(int index, char* value, size_t len) {
    _impl->_vh[index].emplace<std::vector<char>>(value, value + len);
}

void Statement::bindBytea(int index, const std::vector<const char*>& values, const std::vector<size_t>& lens) {
    // PostgreSQL binary bytea[] format:
    //   Header (5 x int32 big-endian): ndims, has_null, element_oid, dim_size, lower_bound
    //   Per element: int32 big-endian byte-length, then raw bytes
    auto write_int32 = [](std::vector<char>& buf, int32_t val) {
        val = boost::endian::native_to_big(val);
        const char* p = reinterpret_cast<const char*>(&val);
        buf.insert(buf.end(), p, p + sizeof(int32_t));
    };

    std::vector<char> buf;
    const int32_t n = static_cast<int32_t>(values.size());

    write_int32(buf, 1);                                                    // ndims
    write_int32(buf, 0);                                                    // has_null
    write_int32(buf, static_cast<int32_t>(PostgresType::BYTEAOID));         // element OID
    write_int32(buf, n);                                                    // dimension size
    write_int32(buf, 1);                                                    // lower bound

    for (int32_t i = 0; i < n; ++i) {
        write_int32(buf, static_cast<int32_t>(lens[i]));
        buf.insert(buf.end(), values[i], values[i] + lens[i]);
    }

    _impl->_vh[index].emplace<std::vector<char>>(std::move(buf));
}

void Statement::bindByteaArr(int index, const std::vector<const char*>& values, const std::vector<size_t>& lens) {
    // PostgreSQL binary bytea[] format:
    //   Header (5 x int32 big-endian): ndims, has_null, element_oid, dim_size, lower_bound
    //   Per element: int32 big-endian byte-length, then raw bytes
    auto write_int32 = [](std::vector<char>& buf, int32_t val) {
        val = boost::endian::native_to_big(val);
        const char* p = reinterpret_cast<const char*>(&val);
        buf.insert(buf.end(), p, p + sizeof(int32_t));
    };

    std::vector<char> buf;
    const auto n = values.size();

    // A null element pointer maps to a SQL NULL element (length -1). The header
    // must advertise has_null accordingly.
    int32_t has_null = 0;
    for (size_t i = 0; i < n; ++i) {
        if (values.at(i) == nullptr) {
            has_null = 1;
            break;
        }
    }

    write_int32(buf, 1);                                                    // ndims
    write_int32(buf, has_null);                                             // has_null
    write_int32(buf, static_cast<int32_t>(PostgresType::BYTEAOID));         // element OID
    write_int32(buf, static_cast<int64_t>(n));                                                    // dimension size
    write_int32(buf, 1);                                                    // lower bound

    for (size_t i = 0; i < n; ++i) {
        const auto& p = values.at(i);
        const auto& p_size = lens.at(i);

        if (p == nullptr) {
            if (p_size == 0) {
                write_int32(buf, -1);                                           // NULL element
                continue;
            } else { // p_size > 0 -> zeros
                std::vector<char> zeros(p_size, 0);
                write_int32(buf, static_cast<int32_t>(p_size));
                buf.insert(buf.end(), zeros.data(), zeros.data() + p_size);
            }
        } else { // p != nullptr
            if (p_size >= 0) {
                write_int32(buf, static_cast<int32_t>(p_size));
                buf.insert(buf.end(), p, p + p_size);
            // } else if (p_size == 0) {
            //     write_int32(buf, 1);
            //     std::vector<char> zero(1, 0);
            //     buf.insert(buf.end(), zero.data(), zero.data() + 1);
            } else {
                assert(false);
            }
        }


    }

    _impl->_vh[index].emplace<std::vector<char>>(std::move(buf));
}

/*!
 * \brief Binds an array of byte arrays as a bytea[] parameter to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param bin_arr Array of byte arrays; each inner vector becomes one bytea element of the array.
 *
 * Intended for bulk binary inserts using a `$N::bytea[]` parameter. This serializes the
 * value into PostgreSQL's binary array wire format (as consumed by the server's array_recv),
 * with all integers in network (big-endian) byte order:
 *   Header:  int32 ndim, int32 has_null flag, int32 element type OID,
 *            int32 dimension size, int32 lower bound
 *   Element: int32 byte-length (>= 0; never -1 since we emit no NULLs) followed by raw bytes.
 */
void Statement::bindByteaArray(int index, const std::vector<std::vector<uint8_t>>& bin_arr) {
    auto append_int32_be = [](std::vector<char>& buf, int32_t val) {
        val = boost::endian::native_to_big(val);
        const char* p = reinterpret_cast<const char*>(&val);
        buf.insert(buf.end(), p, p + sizeof(int32_t));
    };

    // Pre-compute total size: 5 header int32s + per element (int32 length + payload bytes).
    size_t total_size = 5 * sizeof(int32_t);
    for (const auto& elem : bin_arr) {
        total_size += sizeof(int32_t) + elem.size();
    }

    std::vector<char> buf;
    buf.reserve(total_size);

    const int32_t num_elems = static_cast<int32_t>(bin_arr.size());
    append_int32_be(buf, 1);                                            // ndim (1-D array)
    append_int32_be(buf, 0);                                            // has_null flag (no NULL elements)
    append_int32_be(buf, static_cast<int32_t>(PostgresType::BYTEAOID)); // element type OID (bytea = 17)
    append_int32_be(buf, num_elems);                                    // size of dimension 0
    append_int32_be(buf, 1);                                            // lower bound (arrays are 1-based)

    for (const auto& elem : bin_arr) {
        append_int32_be(buf, static_cast<int32_t>(elem.size()));        // element byte-length
        if (!elem.empty()) {                                       // empty -> zero-length bytea (''), never a NULL element
            const char* data = reinterpret_cast<const char*>(elem.data());
            buf.insert(buf.end(), data, data + elem.size());            // raw element bytes
        }
    }

    _impl->_vh[index].emplace<std::vector<char>>(std::move(buf));
}


void Statement::bindTextArray(int index, std::vector<std::string>::const_iterator value_beg, std::vector<std::string>::const_iterator value_end) {
    auto append_int32_be = [](std::vector<char>& buf, int32_t val) {
        val = boost::endian::native_to_big(val);
        const char* p = reinterpret_cast<const char*>(&val);
        buf.insert(buf.end(), p, p + sizeof(int32_t));
    };

    // Pre-compute total size: 5 header int32s + per element (int32 length + payload bytes).
    size_t total_size = 5 * sizeof(int32_t);
    for (auto it = value_beg; it != value_end; ++it) {
        total_size += sizeof(int32_t) + it->size();
    }

    std::vector<char> buf;
    buf.reserve(total_size);

    const int32_t num_elems = static_cast<int32_t>(std::distance(value_beg, value_end));
    append_int32_be(buf, 1);                                           // ndim (1-D array)
    append_int32_be(buf, 0);                                           // has_null flag (no NULL elements)
    append_int32_be(buf, static_cast<int32_t>(PostgresType::TEXTOID)); // element type OID (text = 25)
    append_int32_be(buf, num_elems);                                  // size of dimension 0
    append_int32_be(buf, 1);                                          // lower bound (arrays are 1-based)

    for (auto it = value_beg; it != value_end; ++it) {
        append_int32_be(buf, static_cast<int32_t>(it->size()));       // element byte-length
        if (!it->empty()) {                                           // empty -> zero-length text (''), never a NULL element
            buf.insert(buf.end(), it->data(), it->data() + it->size()); // raw element bytes
        }
    }

    _impl->_vh[index].emplace<std::vector<char>>(std::move(buf));
}

void Statement::bindTextArray(int index, const std::vector<std::string>& value) {
    bindTextArray(index, value.cbegin(), value.cend());
}

/*!
 * \brief Binds arrays of bytes to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value pointer to an array of bytes bound to the column \a index.
 * \param len Length of \a value.
 */
void Statement::bind(int index, const void* value, int len) {
    const char* charPtr = static_cast<const char*>(value);
    _impl->_vh[index].emplace<std::vector<char>>(charPtr, charPtr + len);
}

/*!
 * \brief Binds string values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value String value bound to the column \a index.
 */
void Statement::bind(int index, const std::string& value) {
    _impl->_vh[index] = value;
}

/*!
 * \brief Binds array of uuid values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value Array of Guid values bound to the column \a index.
 */
void Statement::bind(int index, const std::vector<oes::core::Guid>& value) {
    _impl->_vh[index].emplace<std::vector<oes::core::Guid>>(value);
}

/*!
 * \brief Binds array of string or numeric values to a prepared statement.
 * \param index Index of a parameter to set. The leftmost parameter has an index of 1.
 * \param value Array of string or numeric values bound to the column \a index.
 */
template<typename T>
void Statement::bind(int index, const std::vector<T>& value) {
    std::string pg_array = "{";
    for (auto it = value.cbegin(); it != value.cend(); it++) {
        if (it != value.cbegin()) {
            pg_array += ",";
        }
        if constexpr (std::is_same_v<T, std::string>) {
            // Always double-quote and escape string elements. Unquoted elements
            // let commas, braces, whitespace, quotes and the literal NULL corrupt
            // the array (e.g. a Title "Seismic (Depth, 2018)" would be split into
            // two elements, causing an UNNEST length mismatch downstream).
            pg_array += '"';
            for (const char c : *it) {
                if (c == '\\' || c == '"') {
                    pg_array += '\\';
                }
                pg_array += c;
            }

            pg_array += '"';
        } else if constexpr (std::is_arithmetic_v<T>) {
            pg_array += std::to_string(*it);
        } else {
            static_assert(sizeof(T) == 0 /*always false*/, "Unsupported vector type");
        }
    }
    pg_array += "}";

    this->bind(index, pg_array);
}

template void Statement::bind(int index, const std::vector<int>& value);
template void Statement::bind(int index, const std::vector<int64_t>& value);
template void Statement::bind(int index, const std::vector<long long>& value);
template void Statement::bind(int index, const std::vector<std::string>& value);

template<typename T>
void Statement::bind(int index, typename std::vector<T>::const_iterator value_beg, typename std::vector<T>::const_iterator value_end) {
    std::string pg_array = "{";
    for (auto it = value_beg; it != value_end; it++) {
        if (it != value_beg) {
            pg_array += ",";
        }
        if constexpr (std::is_same_v<T, std::string>) {
            // Always double-quote and escape string elements (see note above).
            pg_array += '"';
            for (const char c : *it) {
                if (c == '\\' || c == '"') {
                    pg_array += '\\';
                }
                pg_array += c;
            }
            pg_array += '"';
        } else if constexpr (std::is_arithmetic_v<T>) {
            pg_array += std::to_string(*it);
        } else {
            static_assert(sizeof(T) == 0 /*always false*/, "Unsupported vector type");
        }
    }
    pg_array += "}";

    this->bind(index, pg_array);
}

template<>
void Statement::bind<oes::core::Guid>(int index, std::vector<oes::core::Guid>::const_iterator value_beg, std::vector<oes::core::Guid>::const_iterator value_end) {
    _impl->_vh[index].emplace<std::vector<oes::core::Guid>>(value_beg, value_end);
}

template
void Statement::bind<int>(int index, std::vector<int>::const_iterator value_beg, std::vector<int>::const_iterator value_end);
template
void Statement::bind<int64_t>(int index, std::vector<int64_t>::const_iterator value_beg, std::vector<int64_t>::const_iterator value_end);
template
void Statement::bind<long long>(int index, std::vector<long long>::const_iterator value_beg, std::vector<long long>::const_iterator value_end);
template
void Statement::bind<std::string>(int index, std::vector<std::string>::const_iterator value_beg, std::vector<std::string>::const_iterator value_end);


/*!
* \brief Executes a prepared statement.
* \return The number of affected rows.
*/
int64_t Statement::execute() {
    return _impl->execute();
}

/*!
 * \brief Evaluates the statement (fetch next row).
 * \return True if there are rows to be returned.
 */
bool Statement::step() {
    return _impl->step();
}

/*!
 * \brief Resets the statement back to its initial state to be re-evaluated
 * Any SQL statement variables that had values bound to them retain their values.
 */
void Statement::reset() {
    _impl->reset();
}

/*!
 * \brief Get the statement evaluation result as integer
 * \param index The index of the column for which information should be
 * returned. The leftmost column has the index 0.
 */
int Statement::getInt(int index) {
    return _impl->get<int>(index);
}

/*!
 * \brief Get the statement evaluation result as a 64-bits integer
 * \param index The index of the column for which information should be
 * returned. The leftmost column has the index 0.
 */
int64_t Statement::getInt64(int index) {
    return _impl->get<int64_t>(index);
}

/*!
 * \brief Get the statement evaluation result as a double
 * \param index The index of the column for which information should be
 * returned. The leftmost column has the index 0.
 */
double Statement::getDouble(int index) {
    return _impl->get<double>(index);
}

float Statement::getFloat(int index) {
    return _impl->get<float>(index);
}

bool Statement::getBoolean(int index) {
    return _impl->get<bool>(index);
}

/*!
 * \brief Get the statement evaluation result as an array of char
 * \param index The index of the column for which information should be
 * returned. The leftmost column has the index 0.
 * \param[out] valSize size of the returned array
 * \return an array of char of size \a valSize
 */
const char* Statement::getString(int index, int& valSize) {
    _impl->checkGetValue(index);

    valSize = PQgetlength(_impl->_result.get(), _impl->_currentRow, index);
    const char* value = PQgetvalue(_impl->_result.get(), _impl->_currentRow, index);

    return value;
}

/*!
 * \brief Get the statement evaluation result as a string
 * \param index The index of the column for which information should be
 * returned. The leftmost column has the index 0.
 */
std::string Statement::getString(int index) {
    _impl->checkGetValue(index);

    const int num_bytes = PQgetlength(_impl->_result.get(), _impl->_currentRow, index);
    const char* value = PQgetvalue(_impl->_result.get(), _impl->_currentRow, index);
    if (num_bytes > 0 && value) {
        return std::string(value, static_cast<size_t>(num_bytes));
    }

    return {};
}

/*!
 * \brief Get the statement evaluation result as an array of bytes
 * \param index The index of the column for which information should be
 * returned. The leftmost column has the index 0.
 * \param[out] valSize size of the returned array of bytes
 * \return an array of bytes of size \a valSize
 */
const void* Statement::getBlob(int index, int& valSize) {
    _impl->checkGetValue(index);

    valSize = PQgetlength(_impl->_result.get(), _impl->_currentRow, index);
    const char* value = PQgetvalue(_impl->_result.get(), _impl->_currentRow, index);

    return value;
}

oes::core::Guid Statement::getGuid(int index) {
    _impl->checkGetValue(index);

    const int size = PQgetlength(_impl->_result.get(), _impl->_currentRow, index);
    if( size == 0){
        return oes::core::Guid::invalid();
    }
    const auto* value = reinterpret_cast<const unsigned char*>(PQgetvalue(_impl->_result.get(), _impl->_currentRow, index));
    oes::core::Guid ret(value, value + size);
    return ret;
}

/*!
 * \brief Returns type of a specified column
 * \param index The index of the column for which type should be returned. The
 * leftmost column of the result set has the index 0.
 * \return data type code
 */
uint32_t Statement::getColumnType(int index) const
{
    OES_RELEASE_UNUSED(index);
#ifdef OES_DEBUG_BUILD
    if (index >= _impl->_numColumns) {
        throw std::out_of_range("Index of column is out of range");
    }
#endif
    return PQftype(_impl->_result.get(), index);
}

/*!
 * \brief Returns column name
 * \param index The index of the column for which name should be returned. The
 * leftmost column of the result set has the index 0.
 */
const char* Statement::getColumnName(int index) {
#ifdef OES_DEBUG_BUILD
    if (index >= _impl->_numColumns) {
        throw std::out_of_range("Index of column is out of range");
    }
#endif
    return PQfname(_impl->_result.get(), index);
}

/*!
 * \brief Returns number of columns in a result set
 */
int Statement::getNumColumns() const {
    return _impl->_numColumns;
}

/*!
 * \brief Returns if value is NULL
 * \param index The index of the column for which name should be returned. The
 * leftmost column of the result set has the index 0.
 */
bool Statement::isNull(int index) {
#ifdef OES_DEBUG_BUILD
    if (index >= _impl->_numColumns) {
        throw std::out_of_range("Index of column is out of range");
    }
#endif
    return static_cast<bool>(PQgetisnull(_impl->_result.get(), _impl->_currentRow, index));
}

/*!
 * \brief Returns the number of affected row by the statement
 * \retval number of rows when successful, -1 else
 */
int64_t Statement::getAffectedRows() const{
    return _impl->getAffectedRows();
}

/*!
 * \brief Returns table name for a specified column
 * \param index The index of the column for which table name should be
 * returned. The leftmost column of the result set has the index 0.
 */
std::string Statement::getTableName(int index) const
{
#ifdef OES_DEBUG_BUILD
    if (index >= _impl->_numColumns) {
        //throw DatabaseException (DatabaseException::eCIND_OUT_OF_RANGE);
    }
#endif
    // unlike sqlite cannot get table name of queried column directly - only table's oid
    // oid then may be translated to name using pg_class system table
    Oid table_oid = PQftable(_impl->_result.get(), index);
    return _impl->getTableName(table_oid);
}

/*!
 * \brief Finalize the statement. Called in the destructor as well.
 * Must be called before Connection::close() otherwise connection could not be closed
 */
void Statement::close() {
    _impl->close();
}

void Statement::setChunkSizeInRows(unsigned int chunkSizeInRows) {
    OES_ASSERT(chunkSizeInRows > 0, return;);
    const auto cursor_impl = std::dynamic_pointer_cast<CursorImpl>(_impl);
    if (cursor_impl) {
        cursor_impl->_chunkSizeInRows = chunkSizeInRows;
    }
}

Statement::Statement(const Statement& stmt)
    : _impl(stmt._impl)
{
    reset();
}

} // namespace oes::postgresql
