// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/testing/eml/OpenKVClientServerFixture.h>

#include <oes/client/Client.hpp>
#include <oes/common/etp12/EmlUri.h>
#include <oes/eml/etp12/Extensions.h>
#include <oes/client/ImportEpc.h>
#include <oes/eml/etp12/MsgUtils.h>

#include <oes/core/utils/Logger.h>

#include <oes/testing/core/TestMain.h>


using namespace oes::client;
using namespace oes::eml::etp12;
using namespace oes::testing::eml;
using namespace oes::common::etp12;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;
using oes::common::etp12::EmlUri;

namespace {

BOOST_FIXTURE_TEST_SUITE(Etp12ImportEpcTests, OpenKVClientServerFixture)

BOOST_AUTO_TEST_CASE(test_import_small_epc) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );

    startServer();

    auto clt = connectClient();

    // Note: W/o an SQLite TX, import of such a small project can be quite slow
    //       on Linux, 1.5s vs 200ms on Windows (Release!). But with a TX, it's
    //       mere milliseconds. fsync on Linux seems to be costlier than on Windows.
    const std::string space_path = "EPCs/OneWell";
    const std::chrono::milliseconds timeout(10'000);

    {   // Create EPCs/OneWell ETP space
        auto space_rpc = rpcFor(clt->space_proto(), &IDataspace::PutDataspaces);
        data12::Object::Dataspace space;
        space.m_path = space_path;
        space_rpc.req_.m_dataspaces.emplace("1", space);

        std::ostringstream oss;
        auto ok = space_rpc.call(oss, timeout);
        if (!call_succeeded(ok, oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
        }
        current_space_ = space_path;
    }

    ImportEpcOptions options;
    options.timeout_ = timeout;
    const bool import_ok = importEpc(clt->getConfig(), space_path, epc_input, options);
    BOOST_CHECK(import_ok);

    { // Verify the import got all objects
        std::map<std::string, int32_t> obj_counts;
        auto types_rpc = rpcFor(clt->types_proto(), &proto12::ISupportedTypes::GetSupportedTypes);
        types_rpc.req_.m_uri = EmlUri::make_space(space_path).to_string();
        types_rpc.req_.m_returnEmptyTypes = false;
        types_rpc.req_.m_countObjects = true;
        types_rpc.req_.m_scope = data12::Object::ContextScopeKind::eSelf;

        std::ostringstream types_rpc_oss;
        auto types_rpc_ok = types_rpc.call(types_rpc_oss, timeout);
        if (!call_succeeded(types_rpc_ok, types_rpc_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(types_rpc_ok, types_rpc_oss));
        }

        for (const data12::Object::SupportedType& supp_type : types_rpc.res_.m_supportedTypes) {
            BOOST_REQUIRE(supp_type.m_objectCount);
            const int32_t count = *supp_type.m_objectCount;
            BOOST_REQUIRE(count > 0);
            obj_counts.emplace(supp_type.m_dataObjectType, count);
        }
        const std::map<std::string, int32_t> expected_obj_counts{
            { "eml20.obj_EpcExternalPartReference", 1 }, // FIXME: To have or not have this one
            { "resqml20.obj_LocalDepth3dCrs", 1 },
            { "resqml20.obj_MdDatum", 1 },
            { "resqml20.obj_WellboreFeature", 1 },
            { "resqml20.obj_WellboreInterpretation", 1 },
            { "resqml20.obj_WellboreMarkerFrameRepresentation", 1 },
            { "resqml20.obj_WellboreTrajectoryRepresentation", 1 },
        };
        BOOST_CHECK(obj_counts == expected_obj_counts);
    }

    { // Verify the import got all arrays
        auto array_rpc = rpcFor(clt->array_proto(), &proto12::IDataArray::GetDataArrayMetadata);

        using namespace DataArrayExt;
        array_rpc.req_ = to_list_arrays_req(space_path);

        std::ostringstream array_rpc_oss;
        auto array_rpc_ok = array_rpc.call(array_rpc_oss, timeout);
        if (!call_succeeded(array_rpc_ok, array_rpc_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(array_rpc_ok, array_rpc_oss));
        }

        auto arrays = DataArrayExt::move_to_info_vec(array_rpc.res_.m_arrayMetadata);
        BOOST_REQUIRE_EQUAL(arrays.size(), 4);

        std::sort(arrays.begin(), arrays.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.ident_.m_pathInResource < rhs.ident_.m_pathInResource;
        });

        auto asInfo = [](
            std::string uri, std::string path, // AryIdent
            std::vector<int64_t> dims, data12::AnyArrayType type // AryMeta
        ) {
            AryInfo info;

            AryIdent& ident = info.ident_;
            ident.m_uri = std::move(uri);
            ident.m_pathInResource = std::move(path);

            AryMeta& meta = info.meta_;
            meta.m_dimensions = std::move(dims);
            meta.m_transportArrayType = type;

            return info;
        };
        const char* traj_uri = "eml:///dataspace('EPCs/OneWell')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)";
        const char* wmfr_uri = "eml:///dataspace('EPCs/OneWell')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)";
        constexpr auto eDoubleAry = data12::AnyArrayType::eArrayOfDouble;
        const AryInfoVec expected{
            // Note: m_pathInResource returned by OpenKV always w/o leading slash
            asInfo(traj_uri, "RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters", { 2 }, eDoubleAry),
            asInfo(traj_uri, "RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints", { 2, 3 }, eDoubleAry),
            asInfo(traj_uri, "RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors", { 2, 3 }, eDoubleAry),
            asInfo(wmfr_uri, "RESQML/c76021a6-c1e7-5b2d-848a-35764da8001d/mdValues", { 11 }, eDoubleAry),
        };
        BOOST_REQUIRE_EQUAL(arrays.size(), expected.size());

        // No op== for these structs. Manual comparison...
        for (size_t i = 0; i < arrays.size(); ++i) {
            const AryInfo& lhs = arrays[i];
            const AryInfo& rhs = expected[i];
            BOOST_CHECK_EQUAL(lhs.ident_.m_uri, rhs.ident_.m_uri);
            BOOST_CHECK_EQUAL(lhs.ident_.m_pathInResource, rhs.ident_.m_pathInResource);
            BOOST_CHECK(lhs.meta_.m_dimensions == rhs.meta_.m_dimensions);
            BOOST_CHECK(lhs.meta_.m_transportArrayType == rhs.meta_.m_transportArrayType);
        }
    }

    clt.reset();
}

BOOST_AUTO_TEST_CASE(test_import_small_epc_batch_2) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );

    startServer();

    auto clt = connectClient();

    const std::string space_path = "EPCs/OneWell";
    const std::chrono::milliseconds timeout(10'000); // for debugging

    {   // Create EPCs/OneWell ETP space
        auto space_rpc = rpcFor(clt->space_proto(), &IDataspace::PutDataspaces);
        data12::Object::Dataspace space;
        space.m_path = space_path;
        space_rpc.req_.m_dataspaces.emplace("1", space);

        std::ostringstream oss;
        auto ok = space_rpc.call(oss, timeout);
        if (!call_succeeded(ok, oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
        }
        current_space_ = space_path;
    }

    // Used to fail, before missing references were properly tracked inside the TX's writer.
    // We have only 6 objects, so 3x batches of 2, and we happen to have an object and one
    // of its reference in different batches, exactly as we wanted.
    ImportEpcOptions options;
    options.timeout_ = timeout;
    options.max_batch_size_ = 2;
    const bool import_ok = importEpc(clt->getConfig(), space_path, epc_input, options);
    BOOST_CHECK(import_ok);
}

BOOST_AUTO_TEST_CASE(test_import_large_epc_max_msg_size) {
/*
** Note the difference between storage-size (3,320,531) and the uncompressed
** and unchunked one (9,054,592). The latter is compared to clt_conf_.max_mgs_size_
**
openETPServer hdf5 --stats src/tests/data/eml/interp_well_survey.h5

        12,890,674 Dataset stored bytes
        58,037,260 Dataset bytes un-chunked & un-compressed
              4.5x Compression ratio (exclude metadata overhead)

       <<< Dataset 'storage' size - Top 10 >>>
         3,320,531 RESQML/3e36eae8-c51a-47b0-b5bd-a26b50131a27/values_patch0
                   2D - 1,114 x 1,016 - double (9,054,592 bytes, 2.7x)
         3,123,548 RESQML/f24e7261-265d-43b2-978c-2e672fffe20a/values_patch0
                   2D - 1,114 x 1,016 - double (9,054,592 bytes, 2.9x)
         2,226,093 RESQML/97907d02-4533-4778-b092-e82d7410e5d0/points_patch0
                   2D - 1,114 x 1,016 - double (9,054,592 bytes, 4.1x)
         2,226,093 RESQML/7c6c75b5-ba4e-4442-b32b-ccb7c5dc5993/points_patch0
                   2D - 1,114 x 1,016 - double (9,054,592 bytes, 4.1x)
           784,676 RESQML/8a1aa846-84fe-4749-aa90-f37f9344d96c/points_patch0
                   2D - 99,233 x 3 - double (2,381,592 bytes, 3.0x)
           319,377 RESQML/fcb667d6-dd2d-4347-bd0d-814d95d186a5/values_patch0
                   2D - 1,114 x 1,016 - int (4,527,296 bytes, 14.2x)
           319,377 RESQML/b75fe70b-72ae-44b1-9067-f99b3b2d5f4c/values_patch0
                   2D - 1,114 x 1,016 - int (4,527,296 bytes, 14.2x)
           288,081 RESQML/6cfc0469-e76c-4a87-a98b-f5fd36f7368d/points_patch0
                   2D - 1,016 x 1,114 - double (9,054,592 bytes, 31.4x)
            36,839 RESQML/6f31da3d-5d43-5109-bc82-c81730ad35c0/values_patch0
                   1D - 12,910 - float (51,640 bytes, 1.4x)
            16,802 RESQML/33d2a7a7-c31a-5617-a900-d1c0df95914c/values_patch0
                   1D - 12,910 - float (51,640 bytes, 3.1x)
*/
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "interp_well_survey.epc", true
    );

    startServer();

    bool first = true;
    const size_t KB = 1000;
    const size_t MiB = 1024*1024;
    const std::string space_path = "EPCs/interp_well_survey";
    const std::chrono::milliseconds timeout(10'000); // for debugging

    std::string zcompressor = "gzip";

    // At 16 MiB, no arrays needs sending in pieces.
    // At  8 MiB, 2 pieces max per large array, of unequal lengths.
    // At  1 MiB, 10 pieces per large array, of equal lengths, except last.

    /*
    ** July 2021: Getting rid of 10 KB, as this is too low. We're losing coverage,
    **            which we should gain back a different way. Now that we strictly
    **            enforce max-msg-size, this interferes, and is not realistic anyway
    **
    // At 10 KB, we are so low that even the fast-dim of the 2D 1,114 x 1,016
    //           arrays is above the threshold, and triggers the "slow code path"
    //           with non-contiguous read/writes. For code coverage.
    //           (1,016 * 8 = 8,128) > (805 of 10,000 = 8,000)
    */

    // FIXME: Removed 10KB iteration, which is the only one currently activating
    //        a specific code-path, since crashes in CI (but not on devs' machines...)
    //        On Windows only for now, at least exec it on Linux GCC9
    for (size_t max_mgs_size : { 16*MiB, 8*MiB, 1*MiB, 64*KB }) {
        if (max_mgs_size >= MiB) {
            OES_INFO("=====\n--max_mgs_size = {} MiB", (max_mgs_size/MiB));
        } else {
            OES_INFO("=====\n--max_mgs_size = {} KB", (max_mgs_size/KB));
        }

        clt_conf_.max_mgs_size_ = max_mgs_size;

        // rotate between no-compression, then GZIP
        if (zcompressor == "") {
            zcompressor = "gzip";
        } else if (zcompressor == "gzip") {
            zcompressor = "";
        }
        clt_conf_.compress_ = !zcompressor.empty();
        clt_conf_.zcompressors_ = { zcompressor };
        if (clt_conf_.compress_) {
            OES_INFO("Using compression: {}", zcompressor);
        }

        auto clt = connectClient();

        if (!first) {
            auto space_rpc = rpcFor(clt->space_proto(), &IDataspace::DeleteDataspaces);
            space_rpc.req_.m_uris.emplace("1", EmlUri::make_space(space_path).to_string());

            std::ostringstream oss;
            auto ok = space_rpc.call(oss, timeout);
            if (!call_succeeded(ok, oss)) {
                BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
            }
        }

        first = false;

        {   // Create EPCs/OneWell ETP space
            auto space_rpc = rpcFor(clt->space_proto(), &IDataspace::PutDataspaces);
            data12::Object::Dataspace space;
            space.m_path = space_path;
            space_rpc.req_.m_dataspaces.emplace("1", space);

            std::ostringstream oss;
            auto ok = space_rpc.call(oss, timeout);
            if (!call_succeeded(ok, oss)) {
                BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
            }
            current_space_ = space_path;
        }

        ImportEpcOptions options;
        options.timeout_ = timeout;
        const bool import_ok = importEpc(clt->getConfig(), space_path, epc_input, options);
        BOOST_REQUIRE(import_ok);

        clt_conf_.max_mgs_size_ = 16*MiB; // FIXME: Must use a different client, until
        auto clt_16MB = connectClient(); //         we honor max-msg-size server-side too

        { // Verify the import got all objects
            auto types_rpc = rpcFor(clt_16MB->types_proto(), &proto12::ISupportedTypes::GetSupportedTypes);
            types_rpc.req_.m_uri = EmlUri::make_space(space_path).to_string();
            types_rpc.req_.m_returnEmptyTypes = false;
            types_rpc.req_.m_countObjects = true;
            types_rpc.req_.m_scope = data12::Object::ContextScopeKind::eSelf;

            std::ostringstream types_rpc_oss;
            auto types_rpc_ok = types_rpc.call(types_rpc_oss, timeout);
            if (!call_succeeded(types_rpc_ok, types_rpc_oss)) {
                BOOST_REQUIRE_MESSAGE(false, call_err_msg(types_rpc_ok, types_rpc_oss));
            }

            int32_t obj_count = 0;
            for (const data12::Object::SupportedType& supp_type : types_rpc.res_.m_supportedTypes) {
                BOOST_REQUIRE(supp_type.m_objectCount);
                const int32_t count = *supp_type.m_objectCount;
                BOOST_REQUIRE(count > 0);
                obj_count += count;
            }
            BOOST_CHECK_EQUAL(obj_count, 100);
        }

        { // Verify the import got all arrays
            auto array_rpc = rpcFor(clt_16MB->array_proto(), &proto12::IDataArray::GetDataArrayMetadata);

            using namespace DataArrayExt;
            array_rpc.req_ = to_list_arrays_req(space_path);

            std::ostringstream array_rpc_oss;
            auto array_rpc_ok = array_rpc.call(array_rpc_oss, timeout);
            if (!call_succeeded(array_rpc_ok, array_rpc_oss)) {
                BOOST_REQUIRE_MESSAGE(false, call_err_msg(array_rpc_ok, array_rpc_oss));
            }

            BOOST_CHECK_EQUAL(array_rpc.res_.m_arrayMetadata.size(), 49);

            int64_t ary_bytes = 0;
            for (const auto& entry : array_rpc.res_.m_arrayMetadata) {
                const AryMeta& meta = entry.second;

                int64_t bytes = 0;
                switch (meta.m_transportArrayType) {
                case data12::AnyArrayType::eArrayOfBoolean:   bytes = 1; break;
                case data12::AnyArrayType::eArrayOfInt:       bytes = 4; break;
                case data12::AnyArrayType::eArrayOfLong:      bytes = 8; break;
                case data12::AnyArrayType::eArrayOfFloat:     bytes = 4; break;
                case data12::AnyArrayType::eArrayOfDouble:    bytes = 8; break;
                case data12::AnyArrayType::eArrayOfString:    bytes = 0; break; // unsupported
                case data12::AnyArrayType::eBytes:            bytes = 1; break;
                }

                switch (meta.m_dimensions.size()) {
                case  4: bytes *= meta.m_dimensions[3]; BOOST_FALLTHROUGH;
                case  3: bytes *= meta.m_dimensions[2]; BOOST_FALLTHROUGH;
                case  2: bytes *= meta.m_dimensions[1]; BOOST_FALLTHROUGH;
                case  1: bytes *= meta.m_dimensions[0]; break;
                default: BOOST_FAIL("Impossible");
                }

                ary_bytes += bytes;
            }
            // This tells us the arrays are defined properly
            // (well, since X * Y = Y * X, not really, but good enough here)
            BOOST_CHECK_EQUAL(ary_bytes, 58037260); // ~ 58 MB
        }

        { // Verify more in depth one of the chunked arrays is as expected
            auto array_rpc = rpcFor(clt_16MB->array_proto(), &proto12::IDataArray::GetDataArrays);

            DataArrayTypes::DataArrayIdentifier msg;
            msg.m_uri = EmlUri::make_obj(
                space_path,
                "EpcExternalPartReference",
                oes::core::Guid::parse("010dd1b8-ef54-443a-bfc5-35c92db2dc73"),
                EmlUri::eEML20
            ).to_string();
            msg.m_pathInResource = "RESQML/3e36eae8-c51a-47b0-b5bd-a26b50131a27/values_patch0";
            array_rpc.req_.m_dataArrays.emplace(msg.m_pathInResource, msg);

            std::ostringstream array_rpc_oss;
            auto array_rpc_ok = array_rpc.call(array_rpc_oss, timeout);
            if (!call_succeeded(array_rpc_ok, array_rpc_oss)) {
                BOOST_REQUIRE_MESSAGE(false, call_err_msg(array_rpc_ok, array_rpc_oss));
            }

            BOOST_REQUIRE_EQUAL(array_rpc.res_.m_dataArrays.size(), 1);

            const DataArrayTypes::DataArray& ary = array_rpc.res_.m_dataArrays.begin()->second;
            BOOST_REQUIRE_EQUAL(ary.m_dimensions.size(), 2);
            BOOST_CHECK_EQUAL(ary.m_dimensions[0], 1114);
            BOOST_CHECK_EQUAL(ary.m_dimensions[1], 1016);
            BOOST_CHECK_EQUAL(ary.m_data.m_item.index(), size_t(AnyArray_item_e::eArrayOfDouble));

            const std::vector<double>& vals = std::get<ArrayOfDouble>(ary.m_data.m_item).m_values;
            BOOST_CHECK_EQUAL(vals.size(), ary.m_dimensions[0] * ary.m_dimensions[1]);

            // Compute a hash we compare against was obtained from the 16MB case, which sends
            // the array in 1 piece, so expected OK. When sending in pieces/parts, we should
            // have the same hash value.
            std::string hash_str = oes::core::Guid::make_deterministic(
                std::string((char*)vals.data(), size_t(vals.size()))
                ).toStringNoDash();
            BOOST_CHECK_EQUAL(hash_str, "518c77d0f53d5e80af2d9be91a637884");

            // TODO: Compute hash from input .h5 Dataset directly, to make sure it's the same.
        }
    }
}

BOOST_AUTO_TEST_CASE(test_copy_dataspace) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );

    startServer();

    auto clt = connectClient();

    const std::string space_origin_path = "EPCs/Clone";
    const std::chrono::milliseconds timeout(1000'000);

    {   // Create EPCs/Clone ETP space
        auto space_rpc = rpcFor(clt->space_proto(), &IDataspace::PutDataspaces);
        data12::Object::Dataspace space;
        space.m_path = space_origin_path;
        space_rpc.req_.m_dataspaces.emplace("1", space);

        std::ostringstream oss;
        auto ok = space_rpc.call(oss, timeout);
        if (!call_succeeded(ok, oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
        }
        current_space_ = space_origin_path;
    }

    ImportEpcOptions options;
    options.timeout_ = timeout;
    const bool import_ok = importEpc(clt->getConfig(), space_origin_path, epc_input, options);
    BOOST_CHECK(import_ok);

    const std::string space_path = "EPCs/CloneCopy";

    {   // Create EPCs/CloneCopy ETP space
        auto space_rpc = rpcFor(clt->space_proto(), &IDataspace::PutDataspaces);
        data12::Object::Dataspace space;
        space.m_path = space_path;
        xdataEmplace(space.m_customData,"fromDataspace",EmlUri::make_space(space_origin_path).to_string());
        space_rpc.req_.m_dataspaces.emplace("1", space);

        std::ostringstream oss;
        auto ok = space_rpc.call(oss, timeout);
        if (!call_succeeded(ok, oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
        }
    }

    { // Verify the import got all objects
        std::map<std::string, int32_t> obj_counts;
        auto types_rpc = rpcFor(clt->types_proto(), &proto12::ISupportedTypes::GetSupportedTypes);
        types_rpc.req_.m_uri = EmlUri::make_space(space_path).to_string();
        types_rpc.req_.m_returnEmptyTypes = false;
        types_rpc.req_.m_countObjects = true;
        types_rpc.req_.m_scope = data12::Object::ContextScopeKind::eSelf;

        std::ostringstream types_rpc_oss;
        auto types_rpc_ok = types_rpc.call(types_rpc_oss, timeout);
        if (!call_succeeded(types_rpc_ok, types_rpc_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(types_rpc_ok, types_rpc_oss));
        }

        for (const data12::Object::SupportedType& supp_type : types_rpc.res_.m_supportedTypes) {
            BOOST_REQUIRE(supp_type.m_objectCount);
            const int32_t count = *supp_type.m_objectCount;
            BOOST_REQUIRE(count > 0);
            obj_counts.emplace(supp_type.m_dataObjectType, count);
        }
        const std::map<std::string, int32_t> expected_obj_counts{
            { "eml20.obj_EpcExternalPartReference", 1 }, // FIXME: To have or not have this one
            { "resqml20.obj_LocalDepth3dCrs", 1 },
            { "resqml20.obj_MdDatum", 1 },
            { "resqml20.obj_WellboreFeature", 1 },
            { "resqml20.obj_WellboreInterpretation", 1 },
            { "resqml20.obj_WellboreMarkerFrameRepresentation", 1 },
            { "resqml20.obj_WellboreTrajectoryRepresentation", 1 },
        };
        BOOST_CHECK(obj_counts == expected_obj_counts);
    }

    { // Verify the import got all arrays
        auto array_rpc = rpcFor(clt->array_proto(), &proto12::IDataArray::GetDataArrayMetadata);

        using namespace DataArrayExt;
        array_rpc.req_ = to_list_arrays_req(space_path);

        std::ostringstream array_rpc_oss;
        auto array_rpc_ok = array_rpc.call(array_rpc_oss, timeout);
        if (!call_succeeded(array_rpc_ok, array_rpc_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(array_rpc_ok, array_rpc_oss));
        }

        auto arrays = DataArrayExt::move_to_info_vec(array_rpc.res_.m_arrayMetadata);
        BOOST_REQUIRE_EQUAL(arrays.size(), 4);

        std::sort(arrays.begin(), arrays.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.ident_.m_pathInResource < rhs.ident_.m_pathInResource;
        });

        auto asInfo = [](
            std::string uri, std::string path, // AryIdent
            std::vector<int64_t> dims, data12::AnyArrayType type // AryMeta
        ) {
            AryInfo info;

            AryIdent& ident = info.ident_;
            ident.m_uri = std::move(uri);
            ident.m_pathInResource = std::move(path);

            AryMeta& meta = info.meta_;
            meta.m_dimensions = std::move(dims);
            meta.m_transportArrayType = type;

            return info;
        };
        const char* traj_uri = "eml:///dataspace('EPCs/CloneCopy')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)";
        const char* wmfr_uri = "eml:///dataspace('EPCs/CloneCopy')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)";
        constexpr auto eDoubleAry = data12::AnyArrayType::eArrayOfDouble;
        const AryInfoVec expected{
            // Note: m_pathInResource returned by OpenKV always w/o leading slash
            asInfo(traj_uri, "RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters", { 2 }, eDoubleAry),
            asInfo(traj_uri, "RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints", { 2, 3 }, eDoubleAry),
            asInfo(traj_uri, "RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors", { 2, 3 }, eDoubleAry),
            asInfo(wmfr_uri, "RESQML/c76021a6-c1e7-5b2d-848a-35764da8001d/mdValues", { 11 }, eDoubleAry),
        };
        BOOST_REQUIRE_EQUAL(arrays.size(), expected.size());

        // No op== for these structs. Manual comparison...
        for (size_t i = 0; i < arrays.size(); ++i) {
            const AryInfo& lhs = arrays[i];
            const AryInfo& rhs = expected[i];
            BOOST_CHECK_EQUAL(lhs.ident_.m_uri, rhs.ident_.m_uri);
            BOOST_CHECK_EQUAL(lhs.ident_.m_pathInResource, rhs.ident_.m_pathInResource);
            BOOST_CHECK(lhs.meta_.m_dimensions == rhs.meta_.m_dimensions);
            BOOST_CHECK(lhs.meta_.m_transportArrayType == rhs.meta_.m_transportArrayType);
        }
    }

    {   // Delete EPCs/Clone ETP space
        auto delete_space_rpc = rpcFor(clt->space_proto(), &IDataspace::DeleteDataspaces);
        delete_space_rpc.req_.m_uris.emplace("1", EmlUri::make_space(space_origin_path).to_string());
        delete_space_rpc.req_.m_uris.emplace("2", EmlUri::make_space(space_path).to_string());

        std::ostringstream oss;
        auto ok = delete_space_rpc.call(oss, timeout);
        if (!call_succeeded(ok, oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
        }
    }

    clt.reset();
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
