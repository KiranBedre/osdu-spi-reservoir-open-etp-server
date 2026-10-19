// ============================================================================
// Copyright 2016-2021 Emerson Paradigm Holding LLC. All rights reserved.
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
#include <utility>

#include <oes/core/utils/TypeTraits.h>
#include <oes/core/utils/Assert.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/ErrorCodes.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <libpq-fe.h>
#include <boost/endian/conversion.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes {
namespace postgresql {

/****************************************************************************/

template <typename... Args>
void prep_sql(Connection& conn, std::string_view format_str, Args&&... args) {
    std::string sql = fmt::format(format_str, std::forward<Args>(args)...);
    conn.prepare(std::move(sql));
}

/****************************************************************************/

template <typename... Args>
bool exec_ddl(Connection& conn, std::string_view format_str, Args&&... args) {
    // FIXME: use streaming API to get the result of all instructions ?
    // Currently throws if an error is detected.
    const std::string sql = fmt::format(format_str, std::forward<Args>(args)...);
    PGresult_uptr res = conn.execute(sql);
    ExecStatusType st = PQresultStatus(res.get());
    // When calling stored functions that return void, they actually return a tuple with void in it...
    return st == PGRES_COMMAND_OK || st == PGRES_TUPLES_OK;
}

template <typename... Args>
int exec_scalar_select_int(Connection& conn, std::string_view format_str, Args&&... args) {
    const std::string sql = fmt::format(format_str, std::forward<Args>(args)...);
    PGresult_uptr res = conn.execute(sql);
    const int ntuples = PQntuples(res.get());
    if (ntuples == 0) {
        throw PostgreSQLException(DXO_20980, "empty result set");
    }
    if (ntuples > 1) {
        throw PostgreSQLException(DXO_20981, "more than one result row returned");
    }
    if (PQgetisnull(res.get(), 0, 0) == 1) {
        throw PostgreSQLException(DXO_20980, "empty result set");
    }
    // Should never be in binary format but in case of ...
    if (PQbinaryTuples(res.get()) == 1) {
        return boost::endian::big_to_native(*((uint32_t *)PQgetvalue(res.get(), 0, 0)));
    } else {
        const char* tmp_res = PQgetvalue(res.get(), 0, 0);
        size_t actual_len = PQgetlength(res.get(), 0, 0);
        size_t consumed_len = 0;
        int int_res = std::stoi(tmp_res, &consumed_len);
        assert(consumed_len == actual_len);
        OES_RELEASE_UNUSED(actual_len);
        return int_res;
    }
}

template <typename... Args>
bool exec_scalar_select_bool(Connection& conn, std::string_view format_str, Args&&... args) {
    std::string bool_str = exec_scalar_select_str(conn, format_str, std::forward<Args>(args)...);
    if (bool_str.size() == 1) {
        switch (bool_str[0]) {
        case 't':
        case '1': return true;
        case '0':
        case 'f': return false;
        }
    }
    assert(false);
    return false;
}

template <typename... Args>
std::string exec_scalar_select_str(Connection& conn, std::string_view format_str, Args&&... args) {
    std::string sql = fmt::format(format_str, std::forward<Args>(args)...);
    PGresult_uptr res = conn.execute(sql);
    const int ntuples = PQntuples(res.get());
    if (ntuples == 0) {
        throw PostgreSQLException(DXO_20980, "empty result set");
    }
    if (ntuples > 1) {
        throw PostgreSQLException(DXO_20981, "more than one result row returned");
    }
    if (PQgetisnull(res.get(), 0, 0) == 1) {
        return std::string();
    }
    const char* str = PQgetvalue(res.get(), 0, 0);
    return str == nullptr ? std::string() : std::string(str);
}

/****************************************************************************/

} // namespace oracle
} // namespace oes
