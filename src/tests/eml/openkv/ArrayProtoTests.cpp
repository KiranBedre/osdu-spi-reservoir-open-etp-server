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

#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/eml/TestingMocks.h>

#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/Guid.h>

#include <oes/common/etp12/ArrayUtils.h>
#include <oes/testing/eml/ServerTesting.hpp>

#include <oes/eml/openkv/ArrayProto.h>
#include <oes/eml/openkv/StoreProto.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;
namespace common12 = oes::common::etp12;
using namespace oes::common::etp12;

using oes::core::Guid;

//============================================================================

template <typename TAry>
const decltype(TAry::m_values)& ary_as(const common12::AryUnion& ary) {
    return std::get<TAry>(ary).m_values;
}

using bytes = std::vector<uint8_t>;

size_t countZeroBytes(const AnyArray_item_v& any_array, size_t* p_byte_count = nullptr) {
    size_t count = 0;
    size_t byte_count = 0;
    const void* buf = nullptr;

    auto get_vec_info = [&byte_count, &buf](const auto& vec) {
        byte_count = vec.size() * sizeof(*vec.data());
        if (!vec.empty()) {
            buf = vec.data();
        }
    };

    switch (any_array.index()) {
    case size_t(AnyArray_item_e::eArrayOfBoolean):
        for (bool b : ary_as<ArrayOfBoolean>(any_array)) {
            if (!b) {
                ++count;
            }
        }
        return count;
    case size_t(AnyArray_item_e::eArrayOfInt):     get_vec_info(ary_as<ArrayOfInt>(any_array)); break;
    case size_t(AnyArray_item_e::eArrayOfLong):    get_vec_info(ary_as<ArrayOfLong>(any_array)); break;
    case size_t(AnyArray_item_e::eArrayOfFloat):   get_vec_info(ary_as<ArrayOfFloat>(any_array)); break;
    case size_t(AnyArray_item_e::eArrayOfDouble):  get_vec_info(ary_as<ArrayOfDouble>(any_array)); break;
    case size_t(AnyArray_item_e::eArrayOfString):  get_vec_info(ary_as<ArrayOfString>(any_array)); break;
    case size_t(AnyArray_item_e::eBytes):          get_vec_info(std::get<bytes>(any_array)); break;
    default:
        BOOST_FAIL("Unknown array type");
    }

    BOOST_REQUIRE(buf);
    BOOST_REQUIRE(byte_count);
    auto* bytes = static_cast<const unsigned char*>(buf);
    for (size_t i = 0; i < byte_count; ++i) {
        if (!bytes[i]) { // cppcheck-suppress internalAstError
            ++count;
        }
    }
    if (p_byte_count) {
        *p_byte_count = byte_count;
    }
    return count;
}

//============================================================================

struct ArrayProtoFixture : public oes::testing::eml::OpenKVServerFixture {
    AryIdent do_test_DataArray9_PutUninitializedDataArrays_ND(
        int expected_rank, const EmlAryMock& mock_ary,
        size_t expected_zeros, size_t expected_byte_count
    );
};

//============================================================================

BOOST_FIXTURE_TEST_SUITE(ArrayProtoTests, ArrayProtoFixture)

BOOST_AUTO_TEST_CASE(test_DataArray9_PutUninitializedDataArrays_1D) {
    const EmlObjMock& mock_obj = oes::testing::eml::traj_mock();
    const EmlAryMock& mock_ary = mock_obj.arrays_[0];
    const size_t expected_zeros = 8491;
    const size_t expected_byte_count = 13384;

    BOOST_CHECK_EQUAL(int(mock_ary.type_), int(Datatypes::AnyArrayType::eArrayOfDouble));

    const AryIdent& ary_ident = do_test_DataArray9_PutUninitializedDataArrays_ND(
        1/*D*/, mock_ary, expected_zeros, expected_byte_count
    );

    auto put_subary_rpc = makeRpc(&OpenKVDataArray::PutDataSubarrays);

    AnyArray_item_v any_array;
    mock_ary.init_(any_array);
    const std::vector<double>& values = ary_as<ArrayOfDouble>(any_array);

    auto set_subarray = [&values](AnyArray_item_v& sub_array, int64_t start, int64_t count) {
        ArrayOfDouble ary_of;
        const double* p_subvalues = &values[start];
        ary_of.m_values.assign(p_subvalues, p_subvalues + count);
        sub_array = std::move(ary_of);
    };

    Datatypes::DataArrayTypes::PutDataSubarraysType part;
    part.m_uid = ary_ident;

    auto get_countZeroBytes = [this, ary_ident]() {
        auto get_array_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
        get_array_rpc.req_.m_dataArrays.emplace("ary#1", ary_ident);
        if (!get_array_rpc.call() || hasError(get_array_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_array_rpc.rc_));
        }
        BOOST_REQUIRE_EQUAL(1, get_array_rpc.res_.m_dataArrays.size());
        const auto& ary1 = get_array_rpc.res_.m_dataArrays.begin()->second;
        return countZeroBytes(ary1.m_data.m_item);
    };

    // Fully UnInit, i.e. ALL zeros
    const size_t zeros_before = get_countZeroBytes();
    BOOST_CHECK_EQUAL(zeros_before, expected_byte_count);

    // 1st part
    part.m_starts = { 0 };
    part.m_counts = { mock_ary.dims_[0] / 3 };
    set_subarray(part.m_data.m_item, part.m_starts[0], part.m_counts[0]);
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part1", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // fewer zeros than before, but not yet as much as the full array
    const size_t zeros_after1 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after1, expected_byte_count);
    BOOST_CHECK_GT(zeros_after1, expected_zeros);

    // 2nd part
    part.m_starts = part.m_counts;
    set_subarray(part.m_data.m_item, part.m_starts[0], part.m_counts[0]);
    put_subary_rpc.req_.m_dataSubarrays.clear();
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part2", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // still fewer zeros than before, but not yet as much as the full array
    const size_t zeros_after2 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after2, zeros_after1);
    BOOST_CHECK_GT(zeros_after2, expected_zeros);

    // 3rd (and last) part
    part.m_starts = { 2 * part.m_counts[0] };
    part.m_counts = { mock_ary.dims_[0] - part.m_starts[0] };
    set_subarray(part.m_data.m_item, part.m_starts[0], part.m_counts[0]);
    put_subary_rpc.req_.m_dataSubarrays.clear();
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part3", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // again still fewer zeros than before, but this time
    // fully written now, so should match original array
    const size_t zeros_after3 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after3, zeros_after2);
    BOOST_CHECK_EQUAL(zeros_after3, expected_zeros);
}

// 2D case, but which "degenerates" to 1D
BOOST_AUTO_TEST_CASE(test_DataArray9_PutUninitializedDataArrays_1D_point3d) {
    const EmlObjMock& mock_obj = oes::testing::eml::traj_mock();
    const EmlAryMock& mock_ary = mock_obj.arrays_[1];

    const size_t expected_zeros = 45;
    const size_t expected_byte_count = 40152;

    BOOST_CHECK_EQUAL(int(mock_ary.type_), int(Datatypes::AnyArrayType::eArrayOfDouble));

    // 2nd dim is "fixed", at [3] for (x,y,z). Only 1st dim "varies".
    const AryIdent& ary_ident = do_test_DataArray9_PutUninitializedDataArrays_ND(
        2/*D*/, mock_ary, expected_zeros, expected_byte_count
    );

    auto put_subary_rpc = makeRpc(&OpenKVDataArray::PutDataSubarrays);

    AnyArray_item_v any_array;
    mock_ary.init_(any_array);
    const std::vector<double>& values = ary_as<ArrayOfDouble>(any_array);

    auto set_subarray = [&values](AnyArray_item_v& sub_array, int64_t start, int64_t count) {
        ArrayOfDouble ary_of;
        const double* p_subvalues = &values[start * 3];
        ary_of.m_values.assign(p_subvalues, p_subvalues + count * 3);
        sub_array = std::move(ary_of);
    };

    Datatypes::DataArrayTypes::PutDataSubarraysType part;
    part.m_uid = ary_ident;

    auto get_countZeroBytes = [this, ary_ident]() {
        auto get_array_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
        get_array_rpc.req_.m_dataArrays.emplace("ary#1", ary_ident);
        if (!get_array_rpc.call() || hasError(get_array_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_array_rpc.rc_));
        }
        BOOST_REQUIRE_EQUAL(1, get_array_rpc.res_.m_dataArrays.size());
        const auto& ary1 = get_array_rpc.res_.m_dataArrays.begin()->second;
        return countZeroBytes(ary1.m_data.m_item);
    };

    // Fully UnInit, i.e. ALL zeros
    const size_t zeros_before = get_countZeroBytes();
    BOOST_CHECK_EQUAL(zeros_before, expected_byte_count);

    // 1st part
    part.m_starts = { 0, 0 };
    part.m_counts = { mock_ary.dims_[0] / 3, 3 };
    set_subarray(part.m_data.m_item, part.m_starts[0], part.m_counts[0]);
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part1", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // fewer zeros than before, but not yet as much as the full array
    const size_t zeros_after1 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after1, expected_byte_count);
    BOOST_CHECK_GT(zeros_after1, expected_zeros);

    // 2nd part
    part.m_starts = { part.m_counts[0], 0 };
    set_subarray(part.m_data.m_item, part.m_starts[0], part.m_counts[0]);
    put_subary_rpc.req_.m_dataSubarrays.clear();
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part2", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // still fewer zeros than before, but not yet as much as the full array
    const size_t zeros_after2 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after2, zeros_after1);
    BOOST_CHECK_GT(zeros_after2, expected_zeros);

    // 3rd (and last) part
    part.m_starts = { 2 * part.m_counts[0], 0 };
    part.m_counts = { mock_ary.dims_[0] - part.m_starts[0], 3 };
    set_subarray(part.m_data.m_item, part.m_starts[0], part.m_counts[0]);
    put_subary_rpc.req_.m_dataSubarrays.clear();
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part3", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // again still fewer zeros than before, but this time
    // fully written now, so should match original array
    const size_t zeros_after3 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after3, zeros_after2);
    BOOST_CHECK_EQUAL(zeros_after3, expected_zeros);
}

BOOST_AUTO_TEST_CASE(test_DataArray9_PutUninitializedDataArrays_2D) {
    const EmlObjMock& mock_obj = oes::testing::eml::grid2d_mock();
    const EmlAryMock& mock_ary = mock_obj.arrays_[0];

    const size_t expected_zeros = 231567;
    const size_t expected_byte_count = 439544;

    BOOST_CHECK_EQUAL(int(mock_ary.type_), int(Datatypes::AnyArrayType::eArrayOfDouble));

    // 2nd dim is "fixed", at [3] for (x,y,z). Only 1st dim "varies".
    const AryIdent& ary_ident = do_test_DataArray9_PutUninitializedDataArrays_ND(
        2/*D*/, mock_ary, expected_zeros, expected_byte_count
    );

    auto put_subary_rpc = makeRpc(&OpenKVDataArray::PutDataSubarrays);

    AnyArray_item_v any_array;
    mock_ary.init_(any_array);
    const std::vector<double>& values = ary_as<ArrayOfDouble>(any_array);

    const int64_t n0 = mock_ary.dims_[0];
    const int64_t n1 = mock_ary.dims_[1];
    auto set_subarray = [&values, n0, n1](AnyArray_item_v& sub_array, int64_t start, int64_t count) {
        ArrayOfDouble ary_of;
        ary_of.m_values.resize(n0 * count);
        double* dest = ary_of.m_values.data();
        for (int64_t i0 = 0; i0 < n0; ++i0, dest += count) {
            const double* p_subvalues = &values[i0 * n1 + start];
            std::memcpy(dest, p_subvalues, count * sizeof(double));
        }
        sub_array = std::move(ary_of);
    };

    Datatypes::DataArrayTypes::PutDataSubarraysType part;
    part.m_uid = ary_ident;

    auto get_countZeroBytes = [this, ary_ident]() {
        auto get_array_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
        get_array_rpc.req_.m_dataArrays.emplace("ary#1", ary_ident);
        if (!get_array_rpc.call() || hasError(get_array_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_array_rpc.rc_));
        }
        BOOST_REQUIRE_EQUAL(1, get_array_rpc.res_.m_dataArrays.size());
        const auto& ary1 = get_array_rpc.res_.m_dataArrays.begin()->second;
        return countZeroBytes(ary1.m_data.m_item);
    };

    // Fully UnInit, i.e. ALL zeros
    const size_t zeros_before = get_countZeroBytes();
    BOOST_CHECK_EQUAL(zeros_before, expected_byte_count);

    // 1st part
    part.m_starts = { 0, 0 };
    part.m_counts = { n0, n1 / 3 };
    set_subarray(part.m_data.m_item, part.m_starts[1], part.m_counts[1]);
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part1", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // fewer zeros than before, but not yet as much as the full array
    const size_t zeros_after1 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after1, expected_byte_count);
    BOOST_CHECK_GT(zeros_after1, expected_zeros);

    // 2nd part
    part.m_starts = { 0, part.m_counts[1] };
    set_subarray(part.m_data.m_item, part.m_starts[1], part.m_counts[1]);
    put_subary_rpc.req_.m_dataSubarrays.clear();
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part2", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // still fewer zeros than before, but not yet as much as the full array
    const size_t zeros_after2 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after2, zeros_after1);
    BOOST_CHECK_GT(zeros_after2, expected_zeros);

    // 3rd (and last) part
    part.m_starts = { 0, 2 * part.m_counts[1] };
    part.m_counts = { n0, n1 - part.m_starts[1] };
    set_subarray(part.m_data.m_item, part.m_starts[1], part.m_counts[1]);
    put_subary_rpc.req_.m_dataSubarrays.clear();
    put_subary_rpc.req_.m_dataSubarrays.emplace("ary1#part3", part);

    if (!put_subary_rpc.call() || hasError(put_subary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_subary_rpc.rc_));
    }

    // again still fewer zeros than before, but this time
    // fully written now, so should match original array
    const size_t zeros_after3 = get_countZeroBytes();
    BOOST_CHECK_LT(zeros_after3, zeros_after2);
    BOOST_CHECK_EQUAL(zeros_after3, expected_zeros);
}

BOOST_AUTO_TEST_CASE(test_DataArray9_blob_sharding) {
    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    const int64_t SMALL =     64*1024; // 64 KiB
    const int64_t CHUNK = 1*1024*1024; //  1 MiB

    const int64_t small_in_row_nu = SMALL / sizeof(double) / 3;

    EmlAryMock small_in_row{
        "0 RESQML/uuid/small_in_row",
        { small_in_row_nu },
        AnyArrayType::eArrayOfDouble, {},
        [nu = small_in_row_nu](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(nu);
            for (int64_t u = 0; u < nu; ++u) {
                dbl_ary.m_values[u] = double(u);
            }
            any_ary = std::move(dbl_ary);
        }
    };

    const int64_t exact_in_row_nu = SMALL / sizeof(double);

    EmlAryMock exact_in_row{
        "1 RESQML/uuid/exact_in_row",
        { exact_in_row_nu },
        AnyArrayType::eArrayOfDouble, {},
        [nu = exact_in_row_nu](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(nu);
            for (int64_t u = 0; u < nu; ++u) {
                dbl_ary.m_values[u] = double(u);
            }
            any_ary = std::move(dbl_ary);
        }
    };

    const int64_t small_out_of_row_nu = (SMALL / sizeof(double)) + 1;

    EmlAryMock small_out_of_row{
        "2 RESQML/uuid/small_out_of_row",
        { small_out_of_row_nu },
        AnyArrayType::eArrayOfDouble, {},
        [nu = small_out_of_row_nu](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(nu);
            for (int64_t u = 0; u < nu; ++u) {
                dbl_ary.m_values[u] = double(u);
            }
            any_ary = std::move(dbl_ary);
        }
    };

    const int64_t exact_1chunk_out_of_row_nu = (CHUNK / sizeof(double));

    EmlAryMock exact_1chunk_out_of_row{
        "3 RESQML/uuid/exact_1chunk_out_of_row",
        { exact_1chunk_out_of_row_nu },
        AnyArrayType::eArrayOfDouble, {},
        [nu = exact_1chunk_out_of_row_nu](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(nu);
            for (int64_t u = 0; u < nu; ++u) {
                dbl_ary.m_values[u] = double(u);
            }
            any_ary = std::move(dbl_ary);
        }
    };

    const int64_t small_2chunk_out_of_row_nu = (CHUNK / sizeof(double)) + 3;

    EmlAryMock small_2chunk_out_of_row{
        "4 RESQML/uuid/small_2chunk_out_of_row",
        { small_2chunk_out_of_row_nu },
        AnyArrayType::eArrayOfDouble, {},
        [nu = small_2chunk_out_of_row_nu](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(nu);
            for (int64_t u = 0; u < nu; ++u) {
                dbl_ary.m_values[u] = double(u);
            }
            any_ary = std::move(dbl_ary);
        }
    };

    const int64_t medium_11chunk_out_of_row_nu = 11*(CHUNK / sizeof(double)) - 7;

    EmlAryMock medium_11chunk_out_of_row{
        "5 RESQML/uuid/medium_11chunk_out_of_row_nu",
        { medium_11chunk_out_of_row_nu },
        AnyArrayType::eArrayOfDouble, {},
        [nu = medium_11chunk_out_of_row_nu](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(nu);
            for (int64_t u = 0; u < nu; ++u) {
                dbl_ary.m_values[u] = double(u);
            }
            any_ary = std::move(dbl_ary);
        }
    };

    /*
    ** Too big/slow for UTs
    const int64_t extra_large_out_of_row_nu = 1118;
    const int64_t extra_large_out_of_row_nv = 56692;

    EmlAryMock extra_large_out_of_row{
        "5 RESQML/uuid/extra_large_out_of_row",
        { extra_large_out_of_row_nu, extra_large_out_of_row_nv, 3 },
        AnyArrayType::eArrayOfDouble, {},
        [ nu = extra_large_out_of_row_nu
        , nv = extra_large_out_of_row_nv
        ](etp12::Datatypes::AnyArray_item_v& any_ary) {
            etp12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(nu * nv * 3); // ~ 1.5 GB
            int64_t idx = 0;
            for (int64_t u = 0; u < nu; ++u)
            for (int64_t v = 0; v < nv; ++v, idx += 3) {
                dbl_ary.m_values[idx + 0] = double(idx *  3);     // x
                dbl_ary.m_values[idx + 1] = double(idx *  7 + 1); // y
                dbl_ary.m_values[idx + 2] = double(idx * 11 + 2); // z
            }
            any_ary = std::move(dbl_ary);
        }
    };
    */

    auto reload_and_check_ary = [this](const EmlAryMock& mock, const AryIdent& ident) {
        auto get_ary_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
        get_ary_rpc.req_.m_dataArrays.emplace(mock.path_, ident);
        if (!get_ary_rpc.call() || hasError(get_ary_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_ary_rpc.rc_));
        }
        BOOST_REQUIRE_EQUAL(1, get_ary_rpc.res_.m_dataArrays.size());
        const auto& path = get_ary_rpc.res_.m_dataArrays.begin()->first;
        const auto& ary = get_ary_rpc.res_.m_dataArrays.begin()->second;
        const bool ok_whole = mock.equals(path, ary); // meta-data && data
        if (!ok_whole) {
            return false;
        }

        for (int chunk_cnt : { 7 })
        for (ChunkOrder chunk_order : {
                ChunkOrder::eAscending, ChunkOrder::eDescending, ChunkOrder::eRandom
            }
        ) {
            Datatypes::DataArrayTypes::DataArray ary2;
            ary2.m_dimensions = ary.m_dimensions;
            fillAryInChunks(ident, chunk_cnt, ary2, chunk_order);
            const bool ok_chunked = mock.equals(path, ary2); // meta-data && data
            if (!ok_chunked) {
                return false;
            }
        }

        return true;
    };

    EmlAryMock* mocks[] {
        &small_in_row,
        &exact_in_row,
        &small_out_of_row,
        &exact_1chunk_out_of_row,
        &small_2chunk_out_of_row,
        &medium_11chunk_out_of_row,
        /*
        ** Too big/slow for UTs, even in release
#ifndef OES_DEBUG_BUILD
        &extra_large_out_of_row
#endif
        */
    };

    // 1st pass, we PUT the arrays "whole"
    int i = 0;
    for (EmlAryMock* p_mock : mocks) {
        BOOST_REQUIRE(p_mock);
        const EmlAryMock& mock = *p_mock;

        auto ary = putMockAry(space, epc_uri_str, mock);
        BOOST_CHECK(mock == ary);
        AryInfoVec arys = allAryMeta(space);
        BOOST_REQUIRE_EQUAL(i + 1, arys.size());
        BOOST_CHECK(mock == arys[i]);
        BOOST_CHECK(reload_and_check_ary(mock, ary.m_uid));
        ++i;
    }

    // 2nd pass, we PUT the arrays "in-pieces" (sharded)
    for (int chunk_cnt : { 7 })
    for (ChunkOrder chunk_order : {
            ChunkOrder::eAscending, ChunkOrder::eDescending, ChunkOrder::eRandom
        }
    )
    for (EmlAryMock* p_mock : mocks) {
        BOOST_REQUIRE(p_mock);
        EmlAryMock& mock = *p_mock;

        // tweak the path, so new arrays sort *after* the ones above
        mock.path_.insert(0, 1, 'X');

        auto ary = putMockAryInChunks(
            epc_uri_str, chunk_cnt, mock, chunk_order,
            [&mock, &reload_and_check_ary](
                const data12::DataArrayTypes::DataArrayIdentifier& ary_ident,
                bool last_chunk
            ) {
                // Here we are testing we can get "whole" arrays,
                // despite not yet having written all pieces/chunks;
                // i.e. that we can read "sparse" arrays.
                bool ok = reload_and_check_ary(mock, ary_ident);
                // We assume that only after the last chunk was written,
                // can we expect the array to be identical to the original.
                // This is data-dependent, of course, but our mock arrays
                // are not filled with zeros at the end, so OK here.
                BOOST_CHECK(!ok || last_chunk);
            }
        );
        BOOST_CHECK(mock == ary);
        AryInfoVec arys = allAryMeta(space);
        BOOST_REQUIRE_EQUAL(i + 1, arys.size());
        BOOST_CHECK(mock == arys[i]);
        bool reload_and_check_ary_ok = reload_and_check_ary(mock, ary.m_uid);
        if (!reload_and_check_ary_ok) {
            BOOST_CHECK(reload_and_check_ary_ok);
        }
        ++i;
    }
}

BOOST_AUTO_TEST_CASE(test_DataArray9_PutUninitializedDataArrays_1GB_plus) {
    /*
    ** Emulate the size of Volve's SGrid
D:\oes\trunk>openETPServer hdf5 --stats D:\oes\data\Gold-EPCs\Volve_Geo_Grid2003.h5 --timer
    ...
     1,521,159,744 RESQML/a42daac6-2328-4456-9cb0-a568d1de5fea/Points
                   3D - 1,118 x 56,692 x 3 - double (1,521,159,744 bytes, 1.0x)
    */
    EmlAryMock volve_ary{
        "RESQML/a42daac6-2328-4456-9cb0-a568d1de5fea/Points",
        { 1118, 56692, 3 },
        AnyArrayType::eArrayOfDouble, {},
        [](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(1118 * 56692 * 3); // ~ 1.5 GB
            size_t idx = 0;
            for (size_t u = 0; u < 1118; ++u)
            for (size_t v = 0; v < 56692; ++v, idx += 3) {
                dbl_ary.m_values[idx + 0] = double(idx *  3);     // x
                dbl_ary.m_values[idx + 1] = double(idx *  7 + 1); // y
                dbl_ary.m_values[idx + 2] = double(idx * 11 + 2); // z
            }
            any_ary = std::move(dbl_ary);
        }
    };

    const size_t expected_zeros = 519163434;
    const size_t expected_byte_count = 1118 * 56692 * 3 * 8;

    const AryIdent& ary_ident = do_test_DataArray9_PutUninitializedDataArrays_ND(
        3/*D*/, volve_ary, expected_zeros, expected_byte_count
    );
    OES_UNUSED(ary_ident);
    // TODO: fill and get it.
}

struct GetDataSubarraysTestCase {
    std::string name;
    std::vector<int64_t> starts;
    std::vector<int64_t> counts;
};

BOOST_AUTO_TEST_CASE(test_DataArray9_GetDataSubarrays_4D) {
    EmlAryMock jade_4d_ary{
        "RESQML/a42daac6-2328-4456-9cb0-a568d1de5fea/value_patch0",
        { 43, 92, 81, 6 }, // only 4D array from Jade
        AnyArrayType::eArrayOfDouble, {},
        [](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(43 * 92 * 81 * 6);
            size_t idx = 0;
            for (size_t u = 0; u < 43; ++u)
            for (size_t v = 0; v < 92; ++v)
            for (size_t w = 0; w < 81; ++w, idx += 6) {
                double base = (u * 100'000.) + (v * 1'000.) + (w * 10.);
                dbl_ary.m_values[idx + 0] = base + 0;
                dbl_ary.m_values[idx + 1] = base + 1;
                dbl_ary.m_values[idx + 2] = base + 2;
                dbl_ary.m_values[idx + 3] = base + 3;
                dbl_ary.m_values[idx + 4] = base + 4;
                dbl_ary.m_values[idx + 5] = base + 5;
            }
            any_ary = std::move(dbl_ary);
        }
    };

    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    putMockAry(space, epc_uri_str, jade_4d_ary);

    auto get_sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays);

    Datatypes::DataArrayTypes::GetDataSubarraysType part;
    part.m_uid.m_uri = epc_uri_str;
    part.m_uid.m_pathInResource = jade_4d_ary.path_;

    std::vector<GetDataSubarraysTestCase> test_cases = {
        // Full 1D pillars
        {"pillar#1", { 11, 11, 0, 0 }, { 1, 1, 81, 6 }},
        {"pillar#2", { 11, 0, 11, 0 }, { 1, 92, 1, 6 }},
        {"pillar#3", { 0, 11, 11, 0 }, { 43, 1, 1, 6 }},

        // Partial 1D pillars
        {"partial pillar#1", { 11, 11, 1, 0 }, { 1, 1, 79, 6 }},
        {"partial pillar#2", { 11, 1, 11, 0 }, { 1, 91, 1, 6 }},
        {"partial pillar#3", { 1, 11, 11, 0 }, { 41, 1, 1, 6 }},

        // Small 1D cases
        {"small pillar#1", { 11, 11, 1, 0 }, { 1, 2, 2, 6 }},
        {"small pillar#2", { 11, 1, 11, 0 }, { 2, 1, 2, 6 }},
        {"small pillar#3", { 1, 11, 11, 0 }, { 2, 2, 1, 6 }},

        // Full 2D slices
        {"slice#1", { 22, 0, 0, 0 }, { 1, 92, 81, 6 }},
        {"slice#2", { 0, 22, 0, 0 }, { 42, 1, 81, 6 }},
        {"slice#3", { 0, 0, 22, 0 }, { 43, 92, 1, 6 }},

        // Partial 2D slices
        {"partial slice#1", { 22, 1, 1, 0 }, { 1, 91, 80, 6 }},
        {"partial slice#2", { 1, 22, 1, 0 }, { 41, 1, 80, 6 }},
        {"partial slice#3", { 1, 1, 22, 0 }, { 42, 91, 1, 6 }},

        // Small 2D cases
        {"small slice#1", { 22, 1, 1, 0 }, { 1, 2, 2, 6 }},
        {"small slice#2", { 1, 22, 1, 0 }, { 2, 1, 2, 6 }},
        {"small slice#3", { 1, 1, 22, 0 }, { 2, 2, 1, 6 }},

        // 3D cube
        {"full cube", { 0, 0, 0, 0 }, { 43, 92, 81, 6 }},

        // 3D sub-cubes
        {"subcube#1", { 1, 1, 1, 0 }, { 41, 90, 79, 6 }}, // inner cube
        {"subcube#2", { 3, 3, 3, 0 }, { 21, 12, 27, 6 }},

        // Small 3D cases
        {"small cube#1", { 3, 3, 3, 0 }, { 1, 1, 1, 6}},
        {"small cube#2", { 3, 3, 3, 0 }, { 2, 2, 2, 6 }},
        {"small cube#3", { 3, 3, 3, 0 }, { 10, 10, 10, 6 }},

        // Now we are going to read this array in two pieces, but in the
        // worse possible way, by splitting in two the fastest dimension,
        // to exercise ArrayProto.cpp's readBlobPartially4D()
        {"part#1", { 0, 0, 0, 0 }, { 43, 92, 81, 2 }},
        {"part#2", { 0, 0, 0, 2 }, { 43, 92, 81, 4 }},

        // More slicing in the 4th dimension
        {"4th dim cube", { 0, 0, 0, 1 }, { 43, 92, 81, 1 }},
        {"4th dim slice#1", { 0, 0, 22, 1 }, { 43, 92, 1, 1 }},
        {"4th dim slice#2", { 0, 22, 0, 1 }, { 43, 1, 81, 1 }},
        {"4th dim slice#3", { 22, 0, 0, 1 }, { 1, 92, 81, 1 }},
        {"4th dim pillar#1", { 0, 11, 11, 1 }, { 43, 1, 1, 1 }},
        {"4th dim pillar#2", { 11, 11, 0, 1 }, { 1, 1, 81, 1 }},
        {"4th dim pillar#3", { 11, 0, 11, 1 }, { 1, 92, 1, 1 }},
        {"4th dim small cube#1", { 3, 3, 3, 1 }, { 1, 1, 1, 1 }}, // single value
        {"4th dim small cube#2", { 3, 3, 3, 1 }, { 2, 2, 2, 2 }},
        {"4th dim small slice#1", { 2, 2, 2, 1 }, { 1, 2, 2, 1 }},
        {"4th dim small slice#2", { 2, 2, 2, 1 }, { 1, 2, 2, 2 }},
        {"4th dim small pillar#1", { 1, 1, 1, 1 }, { 1, 1, 2, 1 }},
        {"4th dim small pillar#2", { 1, 1, 1, 1 }, { 1, 1, 2, 2 }},
    };

    for (auto test : test_cases) {
        part.m_starts = test.starts;
        part.m_counts = test.counts;
        BOOST_CHECK_EQUAL(4, part.m_starts.size());
        BOOST_CHECK_EQUAL(4, part.m_counts.size());

        get_sub_rpc.req_.m_dataSubarrays.clear();
        get_sub_rpc.req_.m_dataSubarrays.emplace(test.name, part);
        if (!get_sub_rpc.call() || hasError(get_sub_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_sub_rpc.rc_));
        }

        // Check the dimension of the returned array
        BOOST_REQUIRE_EQUAL(1, get_sub_rpc.res_.m_dataSubarrays.size());

        // Check the total size
        const auto& sub = ary_as<ArrayOfDouble>(
            get_sub_rpc.res_.m_dataSubarrays.begin()->second.m_data.m_item
        );
        BOOST_CHECK_EQUAL(sub.size(), part.m_counts[0] * part.m_counts[1] * part.m_counts[2] * part.m_counts[3]);

        // Check the values
        size_t idx = 0;
        const size_t u_start = part.m_starts[0];
        const size_t u_end = part.m_starts[0] + part.m_counts[0];
        const size_t v_start = part.m_starts[1];
        const size_t v_end = part.m_starts[1] + part.m_counts[1];
        const size_t w_start = part.m_starts[2];
        const size_t w_end = part.m_starts[2] + part.m_counts[2];
        const size_t i_start = part.m_starts[3];
        const size_t i_end = part.m_starts[3] + part.m_counts[3];
        for (size_t u = u_start; u < u_end; ++u)
        for (size_t v = v_start; v < v_end; ++v)
        for (size_t w = w_start; w < w_end; ++w) {
            double base = (u * 100'000.) + (v * 1'000.) + (w * 10.);
            for (size_t i = i_start; i < i_end; ++i, ++idx) {
                const bool ok = (sub[idx] == (base + i));
                if (!ok) {
                    BOOST_REQUIRE(ok);
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_DataArray9_GetDataSubarrays_3D) {
    EmlAryMock jade_3d_ary{
        "RESQML/a42daac6-2328-4456-9cb0-a568d1de5fea/value_patch0",
        { 43, 92, 81 }, // 3D array
        AnyArrayType::eArrayOfDouble, {},
        [](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(43 * 92 * 81);
            size_t idx = 0;
            for (size_t u = 0; u < 43; ++u)
            for (size_t v = 0; v < 92; ++v)
            for (size_t w = 0; w < 81; ++w, ++idx) {
                double base = (u * 1'000.) + (v * 10.) + w;
                dbl_ary.m_values[idx] = base;
            }
            any_ary = std::move(dbl_ary);
        }
    };

    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    putMockAry(space, epc_uri_str, jade_3d_ary);

    auto get_sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays);

    Datatypes::DataArrayTypes::GetDataSubarraysType part;
    part.m_uid.m_uri = epc_uri_str;
    part.m_uid.m_pathInResource = jade_3d_ary.path_;


    std::vector<GetDataSubarraysTestCase> test_cases = {
        // Full 1D pillars
        {"pillar#1", { 11, 11, 0 }, { 1, 1, 81 }},
        {"pillar#2", { 11, 0, 11 }, { 1, 92, 1 }},
        {"pillar#3", { 0, 11, 11 }, { 43, 1, 1 }},

        // Partial 1D pillars
        {"partial pillar#1", { 11, 11, 1 }, { 1, 1, 79 }},
        {"partial pillar#2", { 11, 1, 11 }, { 1, 91, 1 }},
        {"partial pillar#3", { 1, 11, 11 }, { 41, 1, 1 }},

        // Small 1D cases
        {"small pillar#1", { 11, 11, 1 }, { 1, 2, 2 }},
        {"small pillar#2", { 11, 1, 11 }, { 2, 1, 2 }},
        {"small pillar#3", { 1, 11, 11 }, { 2, 2, 1 }},

        // Full 2D slices
        {"slice#1", { 22, 0, 0 }, { 1, 92, 81 }},
        {"slice#2", { 0, 22, 0 }, { 42, 1, 81 }},
        {"slice#3", { 0, 0, 22 }, { 43, 92, 1 }},

        // Partial 2D slices
        {"partial slice#1", { 22, 1, 1 }, { 1, 91, 80 }},
        {"partial slice#2", { 1, 22, 1 }, { 41, 1, 80 }},
        {"partial slice#3", { 1, 1, 22 }, { 42, 91, 1 }},

        // Small 2D cases
        {"small slice#1", { 22, 1, 1 }, { 1, 2, 2 }},
        {"small slice#2", { 1, 22, 1 }, { 2, 1, 2 }},
        {"small slice#3", { 1, 1, 22 }, { 2, 2, 1 }},

        // 3D cube
        {"full cube", { 0, 0, 0 }, { 43, 92, 81 }},

        // 3D sub-cubes
        {"subcube#1", { 1, 1, 1 }, { 41, 90, 79 }}, // inner cube
        {"subcube#2", { 3, 3, 3 }, { 21, 12, 27 }},

        // Small 3D cases
        {"small cube#1", { 3, 3, 3 }, { 1, 1, 1 }},
        {"small cube#2", { 3, 3, 3 }, { 2, 2, 2 }},
        {"small cube#3", { 3, 3, 3 }, { 10, 10, 10 }},
    };

    for (auto test : test_cases) {
        part.m_starts = test.starts;
        part.m_counts = test.counts;
        BOOST_CHECK_EQUAL(3, part.m_starts.size());
        BOOST_CHECK_EQUAL(3, part.m_counts.size());

        get_sub_rpc.req_.m_dataSubarrays.clear();
        get_sub_rpc.req_.m_dataSubarrays.emplace(test.name, part);
        if (!get_sub_rpc.call() || hasError(get_sub_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_sub_rpc.rc_));
        }

        // Check the dimension of the returned array
        BOOST_REQUIRE_EQUAL(1, get_sub_rpc.res_.m_dataSubarrays.size());

        // Check the total size
        const auto& sub = ary_as<ArrayOfDouble>(
            get_sub_rpc.res_.m_dataSubarrays.begin()->second.m_data.m_item
        );
        BOOST_CHECK_EQUAL(sub.size(), part.m_counts[0] * part.m_counts[1] * part.m_counts[2]);

        // Check the values
        size_t idx = 0;
        const size_t u_start = part.m_starts[0];
        const size_t u_end = part.m_starts[0] + part.m_counts[0];
        const size_t v_start = part.m_starts[1];
        const size_t v_end = part.m_starts[1] + part.m_counts[1];
        const size_t w_start = part.m_starts[2];
        const size_t w_end = part.m_starts[2] + part.m_counts[2];
        for (size_t u = u_start; u < u_end; ++u)
        for (size_t v = v_start; v < v_end; ++v)
        for (size_t w = w_start; w < w_end; ++w, ++idx) {
            double base = (u * 1'000.) + (v * 10.) + w;
            const bool ok = (sub[idx] == base);
            if (!ok) {
                BOOST_REQUIRE(ok);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_DataArray9_GetDataSubarrays_2D) {
    EmlAryMock jade_2d_ary{
        "RESQML/a42daac6-2328-4456-9cb0-a568d1de5fea/value_patch0",
        { 143, 192 }, // 2D array
        AnyArrayType::eArrayOfDouble, {},
        [](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(143 * 192);
            size_t idx = 0;
            for (size_t u = 0; u < 143; ++u)
            for (size_t v = 0; v < 192; ++v, ++idx) {
                double base = (u * 10.) + v;
                dbl_ary.m_values[idx] = base;
            }
            any_ary = std::move(dbl_ary);
        }
    };

    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    putMockAry(space, epc_uri_str, jade_2d_ary);

    auto get_sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays);

    Datatypes::DataArrayTypes::GetDataSubarraysType part;
    part.m_uid.m_uri = epc_uri_str;
    part.m_uid.m_pathInResource = jade_2d_ary.path_;


    std::vector<GetDataSubarraysTestCase> test_cases = {
        // Full 1D pillars
        {"pillar#1", { 11, 0 }, { 1, 192 }},
        {"pillar#2", { 0, 11 }, { 143, 1 }},

        // Partial 1D pillars
        {"partial pillar#1", { 11, 1 }, { 1, 191 }},
        {"partial pillar#2", { 1, 11 }, { 141, 1 }},

        // Small 1D cases
        {"small pillar#1", { 11, 1 }, { 1, 2 }},
        {"small pillar#2", { 1, 11 }, { 2, 1 }},

        // Full 2D slice
        {"slice#1", { 0, 0 }, { 143, 192 }},

        // Partial 2D slice
        {"partial slice#1", { 1, 1 }, { 142, 191 }},

        // Small 2D cases
        {"small slice#1", { 1, 1 }, { 2, 2 }},
    };

    for (auto test : test_cases) {
        part.m_starts = test.starts;
        part.m_counts = test.counts;
        BOOST_CHECK_EQUAL(2, part.m_starts.size());
        BOOST_CHECK_EQUAL(2, part.m_counts.size());

        get_sub_rpc.req_.m_dataSubarrays.clear();
        get_sub_rpc.req_.m_dataSubarrays.emplace(test.name, part);
        if (!get_sub_rpc.call() || hasError(get_sub_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_sub_rpc.rc_));
        }

        // Check the dimension of the returned array
        BOOST_REQUIRE_EQUAL(1, get_sub_rpc.res_.m_dataSubarrays.size());

        // Check the total size
        const auto& sub = ary_as<ArrayOfDouble>(
            get_sub_rpc.res_.m_dataSubarrays.begin()->second.m_data.m_item
        );
        BOOST_CHECK_EQUAL(sub.size(), part.m_counts[0] * part.m_counts[1]);

        // Check the values
        size_t idx = 0;
        const size_t u_start = part.m_starts[0];
        const size_t u_end = part.m_starts[0] + part.m_counts[0];
        const size_t v_start = part.m_starts[1];
        const size_t v_end = part.m_starts[1] + part.m_counts[1];
        for (size_t u = u_start; u < u_end; ++u)
        for (size_t v = v_start; v < v_end; ++v, ++idx) {
            double base = (u * 10.) + v;
            const bool ok = (sub[idx] == base);
            if (!ok) {
                BOOST_REQUIRE(ok);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_DataArray9_GetDataSubarrays_1D) {
    EmlAryMock jade_1d_ary{
        "RESQML/a42daac6-2328-4456-9cb0-a568d1de5fea/value_patch0",
        { 9143 }, // 1D array
        AnyArrayType::eArrayOfDouble, {},
        [](common12::Datatypes::AnyArray_item_v& any_ary) {
            common12::Datatypes::ArrayOfDouble dbl_ary;
            dbl_ary.m_values.resize(9143);
            size_t idx = 0;
            for (size_t u = 0; u < 9143; ++u, ++idx) {
                double base = (u * 10.);
                dbl_ary.m_values[idx] = base;
            }
            any_ary = std::move(dbl_ary);
        }
    };

    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    putMockAry(space, epc_uri_str, jade_1d_ary);

    auto get_sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays);

    Datatypes::DataArrayTypes::GetDataSubarraysType part;
    part.m_uid.m_uri = epc_uri_str;
    part.m_uid.m_pathInResource = jade_1d_ary.path_;


    std::vector<GetDataSubarraysTestCase> test_cases = {
        // Full 1D pillar
        {"pillar#1", { 0 }, { 9143 }},

        // Partial 1D pillars
        {"partial pillar#1", { 1 }, { 9141 }},
        {"partial pillar#2", { 1 }, { 2000 }},
        {"partial pillar#3", { 1 }, { 200 }},
        {"partial pillar#4", { 1 }, { 20 }},

        // Small 1D case
        {"small pillar#1", { 1 }, { 2 }},
    };

    for (auto test : test_cases) {
        part.m_starts = test.starts;
        part.m_counts = test.counts;
        BOOST_CHECK_EQUAL(1, part.m_starts.size());
        BOOST_CHECK_EQUAL(1, part.m_counts.size());

        get_sub_rpc.req_.m_dataSubarrays.clear();
        get_sub_rpc.req_.m_dataSubarrays.emplace(test.name, part);
        if (!get_sub_rpc.call() || hasError(get_sub_rpc.rc_)) {
            BOOST_REQUIRE_MESSAGE(false, errorOf(get_sub_rpc.rc_));
        }

        // Check the dimension of the returned array
        BOOST_REQUIRE_EQUAL(1, get_sub_rpc.res_.m_dataSubarrays.size());

        // Check the total size
        const auto& sub = ary_as<ArrayOfDouble>(
            get_sub_rpc.res_.m_dataSubarrays.begin()->second.m_data.m_item
        );
        BOOST_CHECK_EQUAL(sub.size(), part.m_counts[0]);

        // Check the values
        size_t idx = 0;
        const size_t u_start = part.m_starts[0];
        const size_t u_end = part.m_starts[0] + part.m_counts[0];
        for (size_t u = u_start; u < u_end; ++u, ++idx) {
            double base = u * 10.;
            const bool ok = (sub[idx] == base);
            if (!ok) {
                BOOST_REQUIRE(ok);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(test_DataArray9_GetDataSubarrays_empty_1d_int) {
    // mimic and empty region of an SGrid
    EmlAryMock empty_1d_ary{
        "RESQML/61659fe9-feb7-45d3-9cf5-b472b929cef8/subRepresentationPatch0",
        { 0 }, // 1D empty array
        AnyArrayType::eArrayOfInt, {},
        [](common12::Datatypes::AnyArray_item_v& any_ary) {
            any_ary.emplace<ArrayOfInt>();
        }
    };

    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    putMockAry(space, epc_uri_str, empty_1d_ary);

    auto get_ary_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
    auto get_sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays);
    auto get_meta_rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);

    Datatypes::DataArrayTypes::GetDataSubarraysType part;
    part.m_uid.m_uri = epc_uri_str;
    part.m_uid.m_pathInResource = empty_1d_ary.path_;

    const GetDataSubarraysTestCase test{ "EmptyArray", { 0 }, { 0 }};

    //=====================
    // 1st, get a sub-array

    part.m_starts = test.starts;
    part.m_counts = test.counts;
    BOOST_CHECK_EQUAL(1, part.m_starts.size());
    BOOST_CHECK_EQUAL(1, part.m_counts.size());
    get_sub_rpc.req_.m_dataSubarrays.emplace(test.name, part);

    if (!get_sub_rpc.call() || hasError(get_sub_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_sub_rpc.rc_));
    }

    // Check the (sub)array was found
    BOOST_REQUIRE_EQUAL(1, get_sub_rpc.res_.m_dataSubarrays.size());

    // Check the array's full size, and type (int)
    const auto& sub_ary = get_sub_rpc.res_.m_dataSubarrays.begin()->second;
    const auto& sub_any_ary = ary_as<ArrayOfInt>(sub_ary.m_data.m_item);
    BOOST_CHECK(sub_any_ary.empty());
    BOOST_REQUIRE_EQUAL(1, sub_ary.m_dimensions.size());
    BOOST_CHECK_EQUAL(0, sub_ary.m_dimensions[0]);

    //=====================
    // 2nd, get the "full" array (kinda weird, for an "empty" array)

    Datatypes::DataArrayTypes::DataArrayIdentifier ary_ident;
    get_ary_rpc.req_.m_dataArrays.emplace(test.name, part.m_uid);
    if (!get_ary_rpc.call() || hasError(get_ary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_ary_rpc.rc_));
    }

    // Check the (sub)array was found
    BOOST_REQUIRE_EQUAL(1, get_ary_rpc.res_.m_dataArrays.size());

    // Check the array's full size, and type (int)
    const auto& ary = get_ary_rpc.res_.m_dataArrays.begin()->second;
    const auto& ary_any_ary = ary_as<ArrayOfInt>(ary.m_data.m_item);
    BOOST_CHECK(ary_any_ary.empty());
    BOOST_REQUIRE_EQUAL(1, ary.m_dimensions.size());
    BOOST_CHECK_EQUAL(0, ary.m_dimensions[0]);

    //=====================
    // 3rd, get the array's meta-data

    get_meta_rpc.req_.m_dataArrays.emplace(test.name, part.m_uid);

    if (!get_meta_rpc.call() || hasError(get_meta_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_meta_rpc.rc_));
    }

    // Check the (sub)array was found
    BOOST_REQUIRE_EQUAL(1, get_meta_rpc.res_.m_arrayMetadata.size());

    // Check the array's meta-data and type (int)
    const auto& meta = get_meta_rpc.res_.m_arrayMetadata.begin()->second;
    BOOST_CHECK_EQUAL(int(meta.m_transportArrayType), int(Datatypes::AnyArrayType::eArrayOfInt));
    BOOST_REQUIRE_EQUAL(meta.m_dimensions.size(), 1);
    BOOST_CHECK_EQUAL(meta.m_dimensions[0], 0);
}

BOOST_AUTO_TEST_CASE(test_DataArray9_GetDataSubarrays_empty_2d_double) {
    // mimic and empty region of an SGrid
    EmlAryMock empty_2d_ary{
        "RESQML/61659fe9-feb7-45d3-9cf5-b472b929cef8/someeGrid2D",
        { 0, 3 }, // 2D empty array
        AnyArrayType::eArrayOfDouble, {},
        [](common12::Datatypes::AnyArray_item_v& any_ary) {
            any_ary.emplace<ArrayOfDouble>();
        }
    };

    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    putMockAry(space, epc_uri_str, empty_2d_ary);

    auto get_ary_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
    auto get_sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays);
    auto get_meta_rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);

    Datatypes::DataArrayTypes::GetDataSubarraysType part;
    part.m_uid.m_uri = epc_uri_str;
    part.m_uid.m_pathInResource = empty_2d_ary.path_;

    const GetDataSubarraysTestCase test{ "EmptyArray2D", { 0, 0 }, { 0, 3 }};

    //=====================
    // 1st, get a sub-array

    part.m_starts = test.starts;
    part.m_counts = test.counts;
    BOOST_CHECK_EQUAL(2, part.m_starts.size());
    BOOST_CHECK_EQUAL(2, part.m_counts.size());

    get_sub_rpc.req_.m_dataSubarrays.clear();
    get_sub_rpc.req_.m_dataSubarrays.emplace(test.name, part);

    if (!get_sub_rpc.call() || hasError(get_sub_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_sub_rpc.rc_));
    }

    // Check the (sub)array was found
    BOOST_REQUIRE_EQUAL(1, get_sub_rpc.res_.m_dataSubarrays.size());

    // Check the array's full size, and type (int)
    const auto& sub_ary = get_sub_rpc.res_.m_dataSubarrays.begin()->second;
    const auto& sub_any_ary = ary_as<ArrayOfDouble>(sub_ary.m_data.m_item);
    BOOST_CHECK(sub_any_ary.empty());
    BOOST_REQUIRE_EQUAL(2, sub_ary.m_dimensions.size());
    BOOST_CHECK_EQUAL(0, sub_ary.m_dimensions[0]);
    BOOST_CHECK_EQUAL(3, sub_ary.m_dimensions[1]);

    //=====================
    // 2nd, get the "full" array (kinda weird, for an "empty" array)

    Datatypes::DataArrayTypes::DataArrayIdentifier ary_ident;
    get_ary_rpc.req_.m_dataArrays.emplace(test.name, part.m_uid);
    if (!get_ary_rpc.call() || hasError(get_ary_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_ary_rpc.rc_));
    }

    // Check the (sub)array was found
    BOOST_REQUIRE_EQUAL(1, get_ary_rpc.res_.m_dataArrays.size());

    // Check the array's full size, and type (int)
    const auto& ary = get_ary_rpc.res_.m_dataArrays.begin()->second;
    const auto& ary_any_ary = ary_as<ArrayOfDouble>(ary.m_data.m_item);
    BOOST_CHECK(ary_any_ary.empty());
    BOOST_REQUIRE_EQUAL(2, ary.m_dimensions.size());
    BOOST_CHECK_EQUAL(0, ary.m_dimensions[0]);
    BOOST_CHECK_EQUAL(3, ary.m_dimensions[1]);

    //=====================
    // 3rd, get the array's meta-data

    get_meta_rpc.req_.m_dataArrays.emplace(test.name, part.m_uid);

    if (!get_meta_rpc.call() || hasError(get_meta_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_meta_rpc.rc_));
    }

    // Check the (sub)array was found
    BOOST_REQUIRE_EQUAL(1, get_meta_rpc.res_.m_arrayMetadata.size());

    // Check the array's meta-data and type (int)
    const auto& meta = get_meta_rpc.res_.m_arrayMetadata.begin()->second;
    BOOST_CHECK_EQUAL(int(meta.m_transportArrayType), int(Datatypes::AnyArrayType::eArrayOfDouble));
    BOOST_REQUIRE_EQUAL(meta.m_dimensions.size(), 2);
    BOOST_CHECK_EQUAL(meta.m_dimensions[0], 0);
    BOOST_CHECK_EQUAL(meta.m_dimensions[1], 3);
}

BOOST_AUTO_TEST_CASE(test_DataArray9_PutDataArrays_upsert) {
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);
    auto put_array_rpc = makeRpc(&OpenKVDataArray::PutDataArrays);

    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // ETP space should be empty, initially

    const Guid txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());

    // Needs CRS and DATUM refs of TRAJ for TX-less PutDataObjects to succeed
    putMockObj(space, oes::testing::eml::crs_mock());
    putMockObj(space, oes::testing::eml::datum_mock());

    auto traj = oes::testing::eml::traj_mock(); // copy
    oes::testing::eml::EmlObjMock::erase_tag("resqml:RepresentedInterpretation", traj.xml_);
    putMockObj(space, traj);

    BOOST_CHECK_EQUAL(3u, objCount(space));

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    for (const auto& ary : traj.arrays_) {
        putMockAry(space, epc_uri_str, ary);
    }

    bool txOK = commitTx(txUid);
    BOOST_REQUIRE(txOK);

    auto tan_vec = traj.arrays_.back();
    if (tan_vec.path_[0] == '/') tan_vec.path_.erase(0, 1);
    BOOST_REQUIRE(boost::ends_with(tan_vec.path_, "/tangentVectors"));
    BOOST_REQUIRE_EQUAL(int(tan_vec.type_), int(common12::Datatypes::AnyArrayType::eArrayOfDouble));

    const auto arys_before = allAryMeta(space);
    BOOST_REQUIRE_EQUAL(3u, arys_before.size());
    BOOST_CHECK_EQUAL(arys_before.back().ident_.m_pathInResource, tan_vec.path_);
    BOOST_CHECK_EQUAL(int(arys_before.back().meta_.m_transportArrayType), int(tan_vec.type_));

    // Convert the last array from Double to Float
    tan_vec.type_ = common12::Datatypes::AnyArrayType::eArrayOfFloat;
    tan_vec.init_ = [traj_size = tan_vec.dims_[0]](common12::Datatypes::AnyArray_item_v& any_ary) {
        std::vector<float> values(traj_size * 3, 0.0);
        for (size_t i = 0; i < values.size(); i += 3) {
            // x, y of vector remain at zero
            values[i + 2] = (float)(i + 1);
        }
        common12::Datatypes::ArrayOfFloat float_ary;
        float_ary.m_values = std::move(values);
        any_ary = std::move(float_ary);
    };
    tan_vec.data_cached_.emplace<0>(); // "reset" cache

    // This should be an UPDATE, not an insert
    putMockAry(space, epc_uri_str, tan_vec);

    const auto arys_after = allAryMeta(space);
    BOOST_REQUIRE_EQUAL(3u, arys_after.size()); // still 3
    BOOST_CHECK_EQUAL(arys_after.back().ident_.m_pathInResource, tan_vec.path_);
    BOOST_CHECK_EQUAL(int(arys_after.back().meta_.m_transportArrayType), int(tan_vec.type_)); // i.e. NEW type!
    BOOST_CHECK_EQUAL(int(arys_after.back().meta_.m_transportArrayType), int(common12::Datatypes::AnyArrayType::eArrayOfFloat));
}

BOOST_AUTO_TEST_CASE(test_DataArray9_PutDataArrays_wrong_container) {
    auto put_store_rpc = makeRpc(&OpenKVStore::PutDataObjects);
    auto put_array_rpc = makeRpc(&OpenKVDataArray::PutDataArrays);

    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // ETP space should be empty, initially

    const Guid txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());

    const auto hdf_mock = epc_ext_mock();

    // Needs CRS and DATUM refs of TRAJ for TX-less PutDataObjects to succeed
    putMockObj(space, hdf_mock);


    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    auto traj = oes::testing::eml::traj_mock(); // copy
    auto mock_ary = traj.arrays_.front();

    data12::DataArrayTypes::PutDataArraysType ary;
    ary.m_uid.m_uri = epc_uri_str; // the "owner" of the data must be an ExtPartRef
    
    ary.m_uid.m_pathInResource = mock_ary.path_;
    ary.m_array.m_dimensions = mock_ary.dims_;
    mock_ary.init_(ary.m_array.m_data.m_item);
    put_array_rpc.req_.m_dataArrays.emplace("ary#1", ary);
    BOOST_REQUIRE(put_array_rpc.call());
    bool txOK = rollbackTx(txUid);
    BOOST_REQUIRE(txOK);
}

BOOST_AUTO_TEST_CASE(test_DataArray9_bool_1d_in_row) {
    auto put_rpc = makeRpc(&OpenKVDataArray::PutDataArrays);
    auto pus_rpc = makeRpc(&OpenKVDataArray::PutDataSubarrays);
    auto get_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
    auto sub_rpc = makeRpc(&OpenKVDataArray::GetDataSubarrays);
    auto met_rpc = makeRpc(&OpenKVDataArray::GetDataArrayMetadata);

    // create an ETP space we can PUT arrays into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    // Start a transaction, to allow temporary RI violations (arrays w/o its parent object)
    const Guid txUid = startTx(space_uri.to_string(), false);
    BOOST_REQUIRE(txUid.isValid());

    // Creating HDF container
    const oes::testing::eml::EmlObjMock& mock_hdf = oes::testing::eml::epc_ext_mock();
    putMockObj(space, mock_hdf);
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, mock_hdf.type_, mock_hdf.guid_, EmlUri::eEML20
    );
    const std::string& epc_uri_str = epc_uri.to_string();

    // PUT a 1D boolean array "in-full"
    data12::DataArrayTypes::PutDataArraysType ary;
    ary.m_uid.m_uri = epc_uri_str;
    ary.m_uid.m_pathInResource = "/foo/bar";
    const std::vector<bool> values{ true,  false, false, true,  true,  false, true  };
    const std::vector<bool> invert{ false, true,  true,  false, false, true,  false };
    ary.m_array.m_dimensions = { int64_t(values.size()) };
    data12::ArrayOfBoolean bvalues;
    bvalues.m_values = values;
    ary.m_array.m_data.m_item = bvalues;
    put_rpc.req_.m_dataArrays.emplace("ary#1", ary);
    if (!put_rpc.call() || hasError(put_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_rpc.rc_));
    }

    // Get the metadata of that array
    met_rpc.req_.m_dataArrays.emplace("ary#1", ary.m_uid);
    if (!met_rpc.call() || hasError(met_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(met_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(1, met_rpc.res_.m_arrayMetadata.size());
    const auto& met_res0 = met_rpc.res_.m_arrayMetadata.begin()->second;

    // Get that array "in-full"
    get_rpc.req_.m_dataArrays.emplace("ary#1", ary.m_uid);
    if (!get_rpc.call() || hasError(get_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(1, get_rpc.res_.m_dataArrays.size());
    const auto get_res0 = get_rpc.res_.m_dataArrays.begin()->second;
    // WARNING: ^^^^^^^ taking a copy, since will clear get_rpc.res_ later

    // Compare the responses are consistent fr type, size, byte-size
    BOOST_CHECK_EQUAL(int(aryTypeOf(get_res0)), int(aryTypeOf(met_res0)));
    BOOST_CHECK_EQUAL(arySizeOf(get_res0), arySizeOf(met_res0));
    BOOST_CHECK_EQUAL(byteSizeOf(get_res0), byteSizeOf(met_res0));
    BOOST_CHECK(get_res0.m_dimensions == met_res0.m_dimensions);

    // Compare the data/bytes we got are conform to the original
    std::vector<int8_t> bools1, bools2;
    AryMemRef put_mem = aryMemoryOf(ary.m_array, bools1);
    AryMemRef get_mem = aryMemoryOf(get_res0, bools2);
    BOOST_CHECK_EQUAL(put_mem.size(), get_mem.size());
    BOOST_CHECK_EQUAL(put_mem.byteSize(), get_mem.byteSize());
    std::vector<uint8_t> put_bytes(put_mem.begin(), put_mem.end());
    std::vector<uint8_t> get_bytes(get_mem.begin(), get_mem.end());
    BOOST_CHECK(put_bytes == get_bytes);

    // Get that array "in-parts"
    const int64_t total = int64_t(values.size());
    const int64_t split = int64_t(total * .67);
    BOOST_REQUIRE((0 < split) && ((split + 1) < total));
    Datatypes::DataArrayTypes::GetDataSubarraysType sub1, sub2;
    sub1.m_uid = sub2.m_uid = ary.m_uid;
    sub1.m_starts = { 0 };     sub2.m_starts = { split };
    sub1.m_counts = { split }; sub2.m_counts = { total - split };
    sub_rpc.req_.m_dataSubarrays.emplace("sub#1", sub1);
    sub_rpc.req_.m_dataSubarrays.emplace("sub#2", sub2);
    if (!sub_rpc.call() || hasError(sub_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(sub_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(2, sub_rpc.res_.m_dataSubarrays.size());
    const auto& sub_res1 = sub_rpc.res_.m_dataSubarrays.begin()->second;
    const auto& sub_res2 = (++sub_rpc.res_.m_dataSubarrays.begin())->second;

    BOOST_CHECK(sub_res1.m_dimensions == sub1.m_counts);
    BOOST_CHECK(sub_res2.m_dimensions == sub2.m_counts);

    BOOST_CHECK_EQUAL(int(aryTypeOf(sub_res1)), int(aryTypeOf(ary.m_array)));
    BOOST_CHECK_EQUAL(int(aryTypeOf(sub_res2)), int(aryTypeOf(ary.m_array)));
    BOOST_CHECK_EQUAL(arySizeOf(sub_res1)  + arySizeOf(sub_res2),  arySizeOf(ary.m_array));
    BOOST_CHECK_EQUAL(byteSizeOf(sub_res1) + byteSizeOf(sub_res2), byteSizeOf(ary.m_array));

    // Concatenate the two parts we got, and compare them to the original array
    std::vector<int8_t> bools3, bools4;
    AryMemRef sub1_mem = aryMemoryOf(sub_res1, bools3);
    AryMemRef sub2_mem = aryMemoryOf(sub_res2, bools4);
    std::vector<uint8_t> sub_bytes(sub1_mem.begin(), sub1_mem.end());
    sub_bytes.insert(sub_bytes.end(), sub2_mem.begin(), sub2_mem.end());
    BOOST_CHECK(put_bytes == sub_bytes);

    // Now re-write the array, this time "in-parts", "inverting" it.
    Datatypes::DataArrayTypes::PutDataSubarraysType pus1, pus2;
    pus1.m_uid = pus2.m_uid = ary.m_uid;
    pus1.m_starts = { 0 };     pus2.m_starts = { split };
    pus1.m_counts = { split }; pus2.m_counts = { total - split };

    data12::ArrayOfBoolean bpart1, bpart2;
    bpart1.m_values.assign(invert.begin(), invert.begin() + split);
    bpart2.m_values.assign(invert.begin() + split, invert.end());
    pus1.m_data.m_item = bpart1;
    pus2.m_data.m_item = bpart2;

    pus_rpc.req_.m_dataSubarrays.emplace("sub#1", pus1);
    pus_rpc.req_.m_dataSubarrays.emplace("sub#2", pus2);
    if (!pus_rpc.call() || hasError(pus_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(pus_rpc.rc_));
    }

    // RE-Get that array "in-full"
    get_rpc.res_.m_dataArrays.clear();
    get_rpc.req_.m_dataArrays.emplace("ary#1", ary.m_uid);
    if (!get_rpc.call() || hasError(get_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(1, get_rpc.res_.m_dataArrays.size());
    const auto& reget_res0 = get_rpc.res_.m_dataArrays.begin()->second;

    // Compare the 1st get, with the 2nd get (the "re-get")
    BOOST_CHECK_EQUAL(int(aryTypeOf(get_res0)),  int(aryTypeOf(reget_res0)));
    BOOST_CHECK_EQUAL(arySizeOf(get_res0),  arySizeOf(reget_res0));
    BOOST_CHECK_EQUAL(byteSizeOf(get_res0), byteSizeOf(reget_res0));
    BOOST_CHECK(get_res0.m_dimensions == reget_res0.m_dimensions);

    // Compare the data/bytes we got are conform to the original
    std::vector<int8_t> bools5;
    AryMemRef reget_mem = aryMemoryOf(reget_res0, bools5);
    BOOST_CHECK_EQUAL(get_mem.size(), reget_mem.size());
    BOOST_CHECK_EQUAL(get_mem.byteSize(), reget_mem.byteSize());
    std::vector<uint8_t> reget_bytes(reget_mem.begin(), reget_mem.end());
    BOOST_CHECK(get_bytes != reget_bytes); // since we inverted it

    std::vector<uint8_t> reget_want(invert.begin(), invert.end());
    BOOST_CHECK(reget_want == reget_bytes); // Yep, was inverted alright

    // Rollback to avoid an error, since our array is an "Orphan"
    bool txOK = rollbackTx(txUid);
    BOOST_REQUIRE(txOK);
}

BOOST_AUTO_TEST_SUITE_END() // ArrayProtoTests

//============================================================================

AryIdent ArrayProtoFixture::do_test_DataArray9_PutUninitializedDataArrays_ND(
    int expected_rank, const EmlAryMock& mock_ary,
    size_t expected_zeros, size_t expected_byte_count
) {
    auto space = putSpace();
    BOOST_CHECK_EQUAL(0, aryCount(space));

    BOOST_REQUIRE_EQUAL(mock_ary.rank(), expected_rank);

    // FIXME: Dummy EpcExt URI for now. Not used at all server-side in practice...
    const EmlUri epc_uri = EmlUri::make_obj(
        EmlUri::parse(space.m_uri).dataspace_, "EpcExternalPartReference", Guid::make(), EmlUri::eEML20
    );

    data12::DataArrayTypes::PutUninitializedDataArrayType ary;
    ary.m_uid.m_uri = epc_uri.to_string(); // NOT the "real owner" of the data, but the EpcExt
    ary.m_uid.m_pathInResource = mock_ary.path_;
    ary.m_metadata.m_dimensions = mock_ary.dims_;

    // A bit wasteful, but only way to get the type right now.
    size_t byte_count = 0;
    const size_t zeros_before = countZeroBytes(mock_ary.data(), &byte_count);
    BOOST_CHECK_EQUAL(zeros_before, expected_zeros);
    BOOST_CHECK_EQUAL(byte_count, expected_byte_count);

    ary.m_metadata.m_transportArrayType = mock_ary.type_;
    BOOST_CHECK_EQUAL(int(mock_ary.type_), int(EmlAryMock::type_of(mock_ary.data())));

    auto put_array_rpc = makeRpc(&OpenKVDataArray::PutUninitializedDataArrays);
    put_array_rpc.req_.m_dataArrays.emplace("ary#1", ary);

    if (!put_array_rpc.call() || hasError(put_array_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(put_array_rpc.rc_));
    }

    // "empty" array is visible.
    BOOST_CHECK_EQUAL(1, aryCount(space));

    auto get_array_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
    get_array_rpc.req_.m_dataArrays.emplace("ary#1", ary.m_uid);
    if (!get_array_rpc.call() || hasError(get_array_rpc.rc_)) {
        BOOST_REQUIRE_MESSAGE(false, errorOf(get_array_rpc.rc_));
    }
    BOOST_REQUIRE_EQUAL(1, get_array_rpc.res_.m_dataArrays.size());
    const auto& ary1 = get_array_rpc.res_.m_dataArrays.begin()->second;

    BOOST_REQUIRE_EQUAL(mock_ary.dims_.size(), ary1.m_dimensions.size());
    for (size_t i = 0; i < mock_ary.dims_.size(); ++i) {
        BOOST_CHECK_EQUAL(mock_ary.dims_[i], ary1.m_dimensions[i]);
    }

    const size_t zeros_after1 = countZeroBytes(ary1.m_data.m_item);
    BOOST_CHECK_EQUAL(zeros_after1, byte_count); // i.e. ALL zeros

    return ary.m_uid;
}

//============================================================================

} // End anonymous namespace
