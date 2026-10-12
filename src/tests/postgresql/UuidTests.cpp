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

#include <iostream>
#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/test/unit_test.hpp>
#include <libpq-fe.h>
#include <libpq/libpq-fs.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/postgresql/PostgreSQLTestsFixture.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Format.hpp>

#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/Json.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>


using namespace oes::core;
using namespace oes::postgresql::relational;
using namespace oes::postgresql;
using namespace oes::testing;
using namespace oes::testing::postgresql;
namespace endian = boost::endian;

namespace {

BOOST_FIXTURE_TEST_SUITE(UuidTests, PQLibTestsFixture)

BOOST_AUTO_TEST_CASE(test_insert) {
    Connection_ptr conn = connection();

    const std::string tname = "ut_uuid_test";

    BOOST_REQUIRE(exec_ddl(*conn, "DROP TABLE IF EXISTS {}", tname));

    BOOST_REQUIRE(exec_ddl(*conn, "CREATE TABLE {} (guid uuid NOT NULL, name varchar(256))", tname));

    Guid uid = Guid::make();
    std::string uid_str = uid.toXmlString();
    conn->execute(fmt::format("INSERT INTO {} VALUES ($1::uuid, $2)", tname), {
        Connection::QueryParameter{ reinterpret_cast<const char*>(uid.begin()), static_cast<int>(Guid::static_size()), /*as binary=*/true },
        Connection::QueryParameter{ "title1", 6, false },
        }, /*paramTypes*/{}, /*res as binary=*/false);

    PGresult_uptr res = conn->execute(fmt::format("SELECT guid::varchar FROM {}", tname));
    std::string res_uid_str(PQgetvalue(res.get(), 0, 0));
    // the form returned by PostgreSQL as text value is the same as Guid::toXmlString()
    BOOST_REQUIRE_EQUAL(uid_str, res_uid_str);

    BOOST_REQUIRE(exec_ddl(*conn, "DROP TABLE IF EXISTS {}", tname));
}

BOOST_AUTO_TEST_CASE(test_select) {
    Connection_ptr conn = connection();

    const std::string tname = "ut_uuid_test";

    BOOST_REQUIRE(exec_ddl(*conn, "DROP TABLE IF EXISTS {}", tname));

    // guid as primary key
    BOOST_REQUIRE(exec_ddl(*conn, "CREATE TABLE {} (guid uuid PRIMARY KEY, name varchar(256))", tname));

    Guid guid = Guid::make();
    std::string name = "title1";
    Connection::QueryParameters parameters = {
        Connection::QueryParameter{ reinterpret_cast<const char*>(guid.begin()), static_cast<int>(Guid::static_size()), /*as binary=*/true },
        Connection::QueryParameter{ name.c_str(), 6, false }
    };
    const auto insSql = fmt::format("INSERT INTO {} VALUES ($1::uuid, $2)", tname);
    conn->execute(insSql, parameters);
    guid = Guid::make();
    name = "another_title";
    parameters[1] = { name.c_str(), static_cast<int>(name.size()), false };
    conn->execute(insSql, parameters);

    {// select all
    PGresult_uptr res = conn->execute(fmt::format("SELECT guid, name FROM {}", tname));
    BOOST_REQUIRE_EQUAL(PQntuples(res.get()), 2);
    }

    {// select where
    PGresult_uptr res = conn->execute(fmt::format("SELECT guid, name FROM {} where guid = $1", tname), {
        { reinterpret_cast<const char*>(guid.begin()), static_cast<int>(Guid::static_size()), /*as binary=*/true }
        }, /*paramTypes*/{}, /*res as binary=*/true);
    BOOST_REQUIRE_EQUAL(PQntuples(res.get()), 1);
    unsigned char* bin_guid = reinterpret_cast<unsigned char*>(PQgetvalue(res.get(), 0, 0));
    int guid_size = PQgetlength(res.get(), 0, 0);
    BOOST_REQUIRE_EQUAL(Guid::static_size(), static_cast<size_t>(guid_size));
    Guid res_guid(bin_guid, bin_guid + guid_size);
    BOOST_REQUIRE_EQUAL(res_guid, guid);
    }

    const auto selectUuidsInArraySql = fmt::format("SELECT guid, name FROM {} where guid = any($1::uuid[])", tname);
    {// select in, by binding an array - uint32_t

    std::vector<uint32_t> ids(/*header size=*/5 + (/*num elements=*/2 * (/*size indicator=*/1 + /*elem size=*/4)));
    ids[0] = endian::native_to_big(1); // ndims
    ids[1] = 0; // has null
    ids[2] = endian::native_to_big(2950); // oid of uuid type. Still needed if we have the type indicator in query ?
    ids[3] = endian::native_to_big(2); // size dim 0
    ids[4] = endian::native_to_big(1); // starting index of first dim
    uint32_t elem_size = endian::native_to_big(static_cast<uint32_t>(Guid::static_size()));
    ids[5] = elem_size;
    std::memcpy(&ids[6], guid.begin(), Guid::static_size());
    // another guid, not in db
    Guid oguid = Guid::make();
    ids[10] = elem_size;
    std::memcpy(&ids[11], oguid.begin(), Guid::static_size());

    PGresult_uptr res = conn->execute(selectUuidsInArraySql, {
        { reinterpret_cast<const char*>(ids.data()), static_cast<int>(sizeof(uint32_t) * ids.size()), /*as binary=*/true }
    }, /*paramTypes*/{}, /*res as binary=*/true);
    BOOST_REQUIRE_EQUAL(PQntuples(res.get()), 1);
    unsigned char* bin_guid = reinterpret_cast<unsigned char*>(PQgetvalue(res.get(), 0, 0));
    int guid_size = PQgetlength(res.get(), 0, 0);
    BOOST_REQUIRE_EQUAL(Guid::static_size(), static_cast<size_t>(guid_size));
    Guid res_guid(bin_guid, bin_guid + guid_size);
    BOOST_REQUIRE_EQUAL(res_guid, guid);
    }

    {// select in, by binding an array - unsigned char

    std::vector<unsigned char> ids(/*header size=*/20 + (/*num elements=*/1 * (/*size indicator=*/4 + /*elem size=*/Guid::static_size())), 0);
    uint32_t ndim = endian::native_to_big(1);
    memcpy(ids.data(), &ndim, 4);
    ids[4] = 0; ids[5] = 0; ids[6] = 0; ids[7] = 0;
    uint32_t toid = endian::native_to_big(2950); // oid of uuid type. Still needed if we have the type indicator in query ?
    memcpy(ids.data()+8, &toid, 4);
    uint32_t s = endian::native_to_big(1); // size dim 0
    memcpy(ids.data()+12, &s, 4);
    uint32_t sindex = endian::native_to_big(1); // starting index of first dim
    memcpy(ids.data()+16, &sindex, 4);
    uint32_t elem_size = endian::native_to_big(static_cast<uint32_t>(Guid::static_size()));
    memcpy(ids.data()+20, &elem_size, 4);
    // unsigned char* val_pos = ids.data()+24;
    // val_pos = guid.begin(); // memcpy needed?
    memcpy(ids.data()+24, guid.begin(), 16);
    PGresult_uptr res = conn->execute(selectUuidsInArraySql, {
        { reinterpret_cast<const char*>(ids.data()), static_cast<int>(ids.size()), /*as binary=*/true }
    }, /*paramTypes*/{}, /*res as binary=*/true);
    BOOST_REQUIRE_EQUAL(PQntuples(res.get()), 1);
    unsigned char* bin_guid = reinterpret_cast<unsigned char*>(PQgetvalue(res.get(), 0, 0));
    int guid_size = PQgetlength(res.get(), 0, 0);
    BOOST_REQUIRE_EQUAL(Guid::static_size(), static_cast<size_t>(guid_size));
    Guid res_guid(bin_guid, bin_guid + guid_size);
    BOOST_REQUIRE_EQUAL(res_guid, guid);
    }

    {// select in, by binding an array - dedicated struct

    struct PGUidArray {
        uint32_t ndims = 1;
        uint32_t type_oid = 0;
        uint32_t has_null = 0;
        uint32_t start_index = 1;
        struct elem {
            uint32_t elem_size; // as big endian
            uint32_t data[4];
        };
        std::vector<elem> elems;

        std::vector<char> toQueryParameter() {
            size_t elems_size = 0;
            for (const auto& e : elems) {
                elems_size += endian::big_to_native(e.elem_size) + sizeof(uint32_t);
            }
            std::vector<char> arr(/*header size=*/20 + elems_size, 0);
            size_t offset = 0;
            uint32_t _ndims = endian::native_to_big(ndims);
            memcpy(arr.data() + offset, &_ndims, 4);
            offset += 4;
            uint32_t _has_null = endian::native_to_big(has_null); // cppcheck-suppress nullPointer
            memcpy(arr.data() + offset, &_has_null, 4);
            offset += 4;
            uint32_t _type_oid = endian::native_to_big(type_oid); // cppcheck-suppress nullPointer
            memcpy(arr.data() + offset, &_type_oid, 4);
            offset += 4;
            uint32_t _size = endian::native_to_big(static_cast<uint32_t>(elems.size()));
            memcpy(arr.data() + offset, &_size, 4);
            offset += 4;
            uint32_t _start_index = endian::native_to_big(start_index);
            memcpy(arr.data() + offset, &_start_index, 4);
            offset += 4;

            memcpy(arr.data() + offset, reinterpret_cast<char*>(elems.data()), elems_size);
            return arr;
        }
    };

    PGUidArray bind_arr;
    bind_arr.type_oid = 2950;
    bind_arr.elems.resize(1);
    bind_arr.elems[0].elem_size = endian::native_to_big(static_cast<uint32_t>(Guid::static_size()));
    memcpy(bind_arr.elems[0].data, guid.begin(), Guid::static_size());

    std::vector<char> arr = bind_arr.toQueryParameter();
    PGresult_uptr res = conn->execute(selectUuidsInArraySql, {
        { const_cast<const char*>(arr.data()), static_cast<int>(arr.size()), /*as binary=*/true }
    }, /*paramTypes*/{}, /*res as binary=*/true);
    BOOST_REQUIRE_EQUAL(PQntuples(res.get()), 1);
    unsigned char* bin_guid = reinterpret_cast<unsigned char*>(PQgetvalue(res.get(), 0, 0));
    int guid_size = PQgetlength(res.get(), 0, 0);
    BOOST_REQUIRE_EQUAL(Guid::static_size(), static_cast<size_t>(guid_size));
    Guid res_guid(bin_guid, bin_guid + guid_size);
    BOOST_REQUIRE_EQUAL(res_guid, guid);
    }

    {// select in
    std::vector<unsigned char> ids(/*header size=*/20 + (/*num elements=*/1 * (/*size indicator=*/4 + /*elem size=*/Guid::static_size())), 0);
    uint32_t ndim = endian::native_to_big(1);
    memcpy(ids.data(), &ndim, 4);
    ids[4] = 0; ids[5] = 0; ids[6] = 0; ids[7] = 0;
    uint32_t toid = endian::native_to_big(2950); // oid of uuid type. Still needed if we have the type indicator in query ?
    memcpy(ids.data() + 8, &toid, 4);
    uint32_t s = endian::native_to_big(1); // size dim 0
    memcpy(ids.data() + 12, &s, 4);
    uint32_t sindex = endian::native_to_big(1); // starting index of first dim
    memcpy(ids.data() + 16, &sindex, 4);
    uint32_t elem_size = endian::native_to_big(static_cast<uint32_t>(Guid::static_size()));
    memcpy(ids.data() + 20, &elem_size, 4);
    // unsigned char* val_pos = ids.data()+24;
    // val_pos = guid.begin(); // memcpy needed?
    memcpy(ids.data() + 24, guid.begin(), 16);
    PGresult_uptr res = conn->execute(fmt::format("SELECT guid, name FROM {} where guid in (select unnest($1::uuid[]))", tname), {
        { reinterpret_cast<const char*>(ids.data()), static_cast<int>(ids.size()), /*as binary=*/true }
    }, /*paramTypes*/{}, /*res as binary=*/true);
    BOOST_REQUIRE_EQUAL(PQntuples(res.get()), 1);
    unsigned char* bin_guid = reinterpret_cast<unsigned char*>(PQgetvalue(res.get(), 0, 0));
    int guid_size = PQgetlength(res.get(), 0, 0);
    BOOST_REQUIRE_EQUAL(Guid::static_size(), static_cast<size_t>(guid_size));
    Guid res_guid(bin_guid, bin_guid + guid_size);
    BOOST_REQUIRE_EQUAL(res_guid, guid);
    }

    BOOST_REQUIRE(exec_ddl(*conn, "DROP TABLE IF EXISTS {}", tname));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
