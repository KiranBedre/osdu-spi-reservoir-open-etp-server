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

#include <oes/testing/eml/ConsoleUtils.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>

#include <oes/common/etp12/EmlUri.h>
#include <oes/client/Client.hpp>
#include <oes/eml/etp12/Extensions.h>
#include <oes/eml/etp12/MsgUtils.h>

#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Json.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

using namespace oes::testing;
using namespace oes::testing::eml;
using namespace oes::core;
using namespace oes::eml::etp12;
using namespace oes::client;
using namespace oes::common::etp12;
using oes::eml::etp12::Datatypes::DataValue;

namespace {

//*****************************************************************************

BOOST_FIXTURE_TEST_SUITE(SpaceCmdsTests, OpenKVClientServerFixture)

// This test create the space and checks the results "directly",
// rather than going through more EML client commands, unlike next test.
// Also does an import using a "full" space-path, not just the folder.
BOOST_AUTO_TEST_CASE(test_import_epc) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );

    startServer();

    Client_uptr eml_clt = connectClient(); // inits clt_conf_.server_url_ used below

    // FIXME: Using 3s as timeout, since on Linux Release (Marsu), PutDataObjects
    //        takes 1.5s, while on Windows (Debug or Release) is ~ 200-250ms...
    const std::string space_path = "EPCs/OneWell";
    const std::chrono::milliseconds timeout(3000);

    {   // Create EPCs/OneWell ETP space
        auto space_rpc = rpcFor(eml_clt->space_proto(), &proto12::IDataspace::PutDataspaces);
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

    // 1st loop iter is an INSERT (initial import).
    // 2nd loop iter is an UPDATE, which overwrites everything in-place, in a "dumb" fashion
    for (int loop = 0; loop < 2; ++loop) {
        OESEmlRunner runner;
        runner.startAndWait({
            "space", "--import-epc", epc_input, "-s", space_path,
            "-u=foo", "-p=bar", "-k", jwtSecret_,
            "-S", clt_conf_.server_url_
        });
    
        { // Verify the import got all objects
            std::map<std::string, int32_t> obj_counts;
            auto types_rpc = rpcFor(eml_clt->types_proto(), &proto12::ISupportedTypes::GetSupportedTypes);
            types_rpc.req_.m_uri = oes::common::etp12::EmlUri::make_space(space_path).to_string();
            types_rpc.req_.m_returnEmptyTypes = false;
            types_rpc.req_.m_countObjects = true;
            types_rpc.req_.m_scope = data12::Object::ContextScopeKind::eSelf;
    
            std::ostringstream types_rpc_oss;
            auto types_rpc_ok = types_rpc.call(types_rpc_oss, timeout);
            BOOST_REQUIRE(types_rpc_ok.first && types_rpc_oss.str().empty());
    
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
            auto array_rpc = rpcFor(eml_clt->array_proto(), &proto12::IDataArray::GetDataArrayMetadata);
    
            using namespace DataArrayExt;
            array_rpc.req_ = to_list_arrays_req(space_path);
    
            std::ostringstream array_rpc_oss;
            auto array_rpc_ok = array_rpc.call(array_rpc_oss, timeout);
            BOOST_REQUIRE(array_rpc_ok.first && array_rpc_oss.str().empty());
    
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
                // Note: m_pathInResource returned by LiteKV always w/o leading slash
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
    }

    eml_clt.reset();
}

// This test create the space and checks the results via EML client commands,
// rather than doing it "directly", like the test above.
// Import done using "partial" space-path (folder only, name inferred from EPC)
BOOST_AUTO_TEST_CASE(test_space_import_epc) {
    const std::string epc_name = "PillarGrid";
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", epc_name + ".epc", true
    );

    startServer();

    Client_uptr eml_clt = connectClient(); // inits clt_conf_.server_url_ used below

    // FIXME: Using 3s as timeout, since on Linux Release (Marsu), PutDataObjects
    //        takes 1.5s, while on Windows (Debug or Release) is ~ 200-250ms...
    std::string partial_space_path = "From-Partial";
    std::string space_path = partial_space_path + "/default";
    const std::chrono::milliseconds timeout(3000);

    {   // Create EPCs/OneWell ETP space
        OESEmlRunner runner;
        runner.startAndWait({
            "space", "--new", "-s", space_path,
            "-u=foo", "-p=bar", "-k", jwtSecret_,
            "-S", clt_conf_.server_url_, "-v"
        });
    }

    // 1st loop iter is an INSERT (initial import).
    // 2nd loop iter is an UPDATE, which overwrites everything in-place, in a "dumb" fashion
    for (int loop = 0; loop < 2; ++loop) {
        OESEmlRunner runner;
        runner.startAndWait({
            "space", "--import-epc", epc_input, "-s", partial_space_path,
            "-u=foo", "-p=bar", "-k", jwtSecret_,
            "-S", clt_conf_.server_url_, "-v"
        });

        runner.startAndWait({
            "space", "--stats", epc_input, "-s", space_path,
            "-u=foo", "-p=bar", "-k", jwtSecret_,
            "-S", clt_conf_.server_url_
        });
        BOOST_CHECK(runner.outputHasLines({
            "                 1 eml20.obj_EpcExternalPartReference",
            "                 1 resqml20.obj_CategoricalProperty",
            "                 1 resqml20.obj_ContinuousProperty",
            "                22 resqml20.obj_FaultInterpretation",
            "                 6 resqml20.obj_GeneticBoundaryFeature",
            "                 1 resqml20.obj_GridConnectionSetRepresentation",
            "                 6 resqml20.obj_HorizonInterpretation",
            "                 1 resqml20.obj_IjkGridRepresentation",
            "                 1 resqml20.obj_LocalDepth3dCrs",
            "                 1 resqml20.obj_LocalTime3dCrs",
            "                 2 resqml20.obj_OrganizationFeature",
            "                 1 resqml20.obj_PropertyKind",
            "                 1 resqml20.obj_StratigraphicColumn",
            "                 1 resqml20.obj_StratigraphicColumnRankInterpretation",
            "                 7 resqml20.obj_StratigraphicUnitFeature",
            "                 7 resqml20.obj_StratigraphicUnitInterpretation",
            "                 2 resqml20.obj_StringTableLookup",
            "                21 resqml20.obj_SubRepresentation",
            "                22 resqml20.obj_TectonicBoundaryFeature",
            "               105 TOTAL"
        }));
    
        runner.startAndWait({
            "space", "--array-stats", epc_input, "-s", space_path,
            "-u=foo", "-p=bar", "-k", jwtSecret_,
            "-S", clt_conf_.server_url_
        });
        BOOST_CHECK(runner.outputHasLines({
            "        <<< Arrays by rank >>>",
            "                27 1D",
            "                 5 2D",
            "                 4 3D",
            "                 1 4D",
            "",
            "        <<< Arrays by element type >>>",
            "                 3 Int",
            "                28 Long",
            "                 4 Double",
            "                 2 Byte",
            "",
            "        <<< Arrays by bytes-range >>>",
            "                 0 (        1 -         2] bytes",
            "                 0 (        2 -         4] bytes",
            "                 0 (        4 -         8] bytes",
            "                 0 (        8 -        16] bytes",
            "                 1 (       16 -        32] bytes",
            "                 1 (       32 -        64] bytes",
            "                 4 (       64 -       128] bytes",
            "                 5 (      128 -       256] bytes",
            "                 5 (      256 -       512] bytes",
            "                 1 (      512 -     1,024] bytes",
            "                 6 (    1,024 -     2,048] bytes",
            "                 2 (    2,048 -     4,096] bytes",
            "                 2 (    4,096 -     8,192] bytes",
            "                 0 (    8,192 -    16,384] bytes",
            "                 1 (   16,384 -    32,768] bytes",
            "                 0 (   32,768 -    65,536] bytes",
            "                 2 (   65,536 -   131,072] bytes",
            "                 3 (  131,072 -   262,144] bytes",
            "                 2 (  262,144 -   524,288] bytes",
            "                 1 (  524,288 - 1,048,576] bytes",
            "                 1 (1,048,576 - 2,097,152] bytes",
            "",
            "        <<< Arrays by object type >>>",
            "           430,560  1x obj_CategoricalProperty",
            "           861,120  1x obj_ContinuousProperty",
            "           954,280  4x obj_GridConnectionSetRepresentation",
            "         1,384,584 10x obj_IjkGridRepresentation",
            "            11,232 21x obj_SubRepresentation",
            "         3,641,776 37x TOTAL",
            "",
            "        <<< Arrays bytes - Top 10 >>>",
            "         1,134,600 obj_IjkGridRepresentation(4a96f14a-6c32-4b74-9102-b0ce2304d824)",
            "                   RESQML/4a96f14a-6c32-4b74-9102-b0ce2304d824/PointParameters",
            "                   2D - 61 x 2,325 - Double",
            "           861,120 obj_ContinuousProperty(905b026a-42f3-46ad-81bc-7fa2f33613c8)",
            "                   RESQML/905b026a-42f3-46ad-81bc-7fa2f33613c8/values_patch0",
            "                   3D - 60 x 46 x 39 - Double",
            "           430,560 obj_CategoricalProperty(b5092e8a-b1ee-4e95-b5db-e0b540454ca4)",
            "                   RESQML/b5092e8a-b1ee-4e95-b5db-e0b540454ca4/values_patch0",
            "                   3D - 60 x 46 x 39 - Int",
            "           381,712 obj_GridConnectionSetRepresentation(d1f3bf39-d19d-4a68-b510-2912d57eeff0)",
            "                   RESQML/d1f3bf39-d19d-4a68-b510-2912d57eeff0/CellIndexPairs",
            "                   2D - 23,857 x 2 - Long",
            "           190,856 obj_GridConnectionSetRepresentation(d1f3bf39-d19d-4a68-b510-2912d57eeff0)",
            "                   RESQML/d1f3bf39-d19d-4a68-b510-2912d57eeff0/LocalFacePerCellIndexPairs",
            "                   2D - 23,857 x 2 - Int",
            "           190,856 obj_GridConnectionSetRepresentation(d1f3bf39-d19d-4a68-b510-2912d57eeff0)",
            "                   RESQML/d1f3bf39-d19d-4a68-b510-2912d57eeff0/InterpretationIndices/elements",
            "                   1D - 23,857 - Long",
            "           190,856 obj_GridConnectionSetRepresentation(d1f3bf39-d19d-4a68-b510-2912d57eeff0)",
            "                   RESQML/d1f3bf39-d19d-4a68-b510-2912d57eeff0/InterpretationIndices/cumulativeLength",
            "                   1D - 23,857 - Long",
            "           107,640 obj_IjkGridRepresentation(4a96f14a-6c32-4b74-9102-b0ce2304d824)",
            "                   RESQML/4a96f14a-6c32-4b74-9102-b0ce2304d824/CellGeometryIsDefined",
            "                   3D - 60 x 46 x 39 - Byte",
            "            90,240 obj_IjkGridRepresentation(4a96f14a-6c32-4b74-9102-b0ce2304d824)",
            "                   RESQML/4a96f14a-6c32-4b74-9102-b0ce2304d824/ControlPoints",
            "                   4D - 2 x 47 x 40 x 3 - Double",
            "            30,080 obj_IjkGridRepresentation(4a96f14a-6c32-4b74-9102-b0ce2304d824)",
            "                   RESQML/4a96f14a-6c32-4b74-9102-b0ce2304d824/controlPointParameters",
            "                   3D - 2 x 47 x 40 - Double",
        }));
    }

    eml_clt.reset();
}

BOOST_AUTO_TEST_CASE(test_default_space) {
    create_default_space_ = true;

    startServer();

    Client_uptr eml_clt = connectClient(); // inits clt_conf_.server_url_ used below
    OES_UNUSED(eml_clt);

    OESEmlRunner runner;
    runner.startAndWait({
        "space", "--list",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_, "--log_level=warning"
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputLinesMatch({
        "",
        "Default dataspace \\(no folder\\):",
        "- <Default> rw .* \\[0 kB\\]",
        "1 dataspace, in 0 folder.",
    }));

    runner.startAndWait({
        "space", "--list", "--flat",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_, "--log_level=warning"
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputLinesMatch({
        "1 dataspace:",
        "- <Default> rw .* \\[0 kB\\]",
    }));
  //BOOST_CHECK(runner.noErrors());                    // Wed Feb 17 14:15:48 2021
    BOOST_CHECK(runner.outputLineMatches("^- <Default> rw ... ... .. ..:..:.. 202. (\\(\\d+ custom data\\))? \\[0 kB\\]$"));
    const std::string dt_reg = runner.outBuf_;

    runner.startAndWait({
        "space", "--list", "--flat", "--iso",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_, "-v"
    });
  //BOOST_CHECK(runner.noErrors());                    // 2021-02-17T13:12:58
    BOOST_CHECK(runner.outputLineMatches("^- <Default> rw 202.-..-..T..:..:.. (\\(\\d+ custom data\\))? \\[0 kB\\]$"));
    const std::string dt_iso = runner.outBuf_;

    runner.startAndWait({
        "space", "--list", "--flat", "--utc",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_, "-v"
    });
  //BOOST_CHECK(runner.noErrors());                    // Wed Feb 17 14:15:48 2021 UTC
    BOOST_CHECK(runner.outputLineMatches("^- <Default> rw ... ... .. ..:..:.. 202. UTC (\\(\\d+ custom data\\))? \\[0 kB\\]$"));
    const std::string dt_utc = runner.outBuf_;

    runner.startAndWait({
        "space", "--list", "--flat", "--utc", "--iso",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_, "-v"
    });
  //BOOST_CHECK(runner.noErrors());                    // 2021-02-17T13:12:58 UTC
    BOOST_CHECK(runner.outputLineMatches("^- <Default> rw 202.-..-..T..:..:.. UTC (\\(\\d+ custom data\\))? \\[0 kB\\]$"));
    const std::string dt_iso_utc = runner.outBuf_;

    const auto not_found = std::string::npos;
    BOOST_CHECK(dt_reg.find("UTC") == not_found);
    BOOST_CHECK(dt_iso.find("UTC") == not_found);
    BOOST_CHECK(dt_utc.find("UTC") != not_found);
    BOOST_CHECK(dt_iso_utc.find("UTC") != not_found);

    // All outputs are different
    BOOST_CHECK_NE(dt_reg, dt_iso);
    BOOST_CHECK_NE(dt_reg, dt_utc);
    BOOST_CHECK_NE(dt_reg, dt_iso_utc);
    BOOST_CHECK_NE(dt_iso, dt_utc);
    BOOST_CHECK_NE(dt_iso, dt_iso_utc);
    BOOST_CHECK_NE(dt_utc, dt_iso_utc);

    runner.startAndWait({
        "space", "--info", "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_, "--log_level=warning"
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Dataspace <Default>:",
        "           URI: eml:///",
        "           Uid: ",
        "          Path: ",
    }));
    //                                                    Thu Feb 18 15:25:23 2021
    BOOST_CHECK(runner.outputLineMatches("^Last-Modified: ... ... .. ..:..:.. 202.$"));

/* TODO FXME: stats commands is broken on default space
    runner.startAndWait({
        "space", "--stats", "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "No ETP types found"
    }));

    // Try an EPC import in the default space
    runner.startAndWait({
        "space", "--import-epc", epc_input, "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
  //BOOST_CHECK(runner.noErrors());

    // Get row/object stats on default space
    runner.startAndWait({
        "space", "--stats", epc_input, "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "                 1 eml20.obj_EpcExternalPartReference",
        "                 1 resqml20.obj_LocalDepth3dCrs",
        "                 1 resqml20.obj_MdDatum",
        "                 1 resqml20.obj_WellboreFeature",
        "                 1 resqml20.obj_WellboreInterpretation",
        "                 1 resqml20.obj_WellboreMarkerFrameRepresentation",
        "                 1 resqml20.obj_WellboreTrajectoryRepresentation",
        "                 7 TOTAL",
    }));

    // Should get the same output, via Discovery(3) instead of SupportedTypes(25)
    // Also exercise --acknowledge, checking for its --debug messages
    runner.startAndWait({
        "space", "--stats", epc_input, "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_,
        "--shun-protocols", "types",
        "--acknowledge", "--debug"
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Shunning SupportedTypes(25) protocol",
        "No SupportedTypes(25) support: Using slower Discovery(3) instead"
    }, false));
    BOOST_CHECK(runner.outputLinesMatch({
        R"re(CLT RES ~\d \$1 #5\(4\) Discovery\(3\)\.Acknowledge\(1001\) -)re",
        R"re(CLT ACK of #4\(0\) Discovery\(3\)\.GetResources\(1\) FA \(\d+\.\dms later\))re",
    }));
    BOOST_CHECK(runner.outputHasLines({
        "                 1 eml20.obj_EpcExternalPartReference",
        "                 1 resqml20.obj_LocalDepth3dCrs",
        "                 1 resqml20.obj_MdDatum",
        "                 1 resqml20.obj_WellboreFeature",
        "                 1 resqml20.obj_WellboreInterpretation",
        "                 1 resqml20.obj_WellboreMarkerFrameRepresentation",
        "                 1 resqml20.obj_WellboreTrajectoryRepresentation",
        "                 7 TOTAL",
    }));

    // Get array stats on default space
    runner.startAndWait({
        "space", "--array-stats", epc_input, "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "        <<< Arrays by rank >>>",
        "                 2 1D",
        "                 2 2D",
        "",
        "        <<< Arrays by element type >>>",
        "                 4 Double",
        "",
        "        <<< Arrays by bytes-range >>>",
        "                 1 ( 16 -  32] bytes",
        "                 2 ( 32 -  64] bytes",
        "                 1 ( 64 - 128] bytes",
        "",
        "        <<< Arrays by object type >>>",
        "                88 1x obj_WellboreMarkerFrameRepresentation",
        "               112 3x obj_WellboreTrajectoryRepresentation",
        "               200 4x TOTAL",
        "",
        "        <<< Arrays bytes - Top 10 >>>",
        "                88 obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)",
        "                   RESQML/c76021a6-c1e7-5b2d-848a-35764da8001d/mdValues",
        "                   1D - 11 - Double",
        "                48 obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)",
        "                   RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors",
        "                   2D - 2 x 3 - Double",
        "                48 obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)",
        "                   RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints",
        "                   2D - 2 x 3 - Double",
        "                16 obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)",
        "                   RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters",
        "                   1D - 2 - Double",
    }));

    // Try deleting the default space. Expected to fail
    runner.startAndWaitExpecting(1, {
        "space", "--delete", epc_input, "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
    BOOST_CHECK(runner.errorHasLines({
        "Cannot use <Default> unnamed dataspace"
    }));

    // Try again deleting the default space.
    // Above it's the command itself that errors out.
    // Below, we force the client to try anyway, and
    // it's the server that's expected to error out this time
    runner.startAndWaitExpecting(1, {
        "space", "--delete", epc_input, "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_,
        "--force"
    });
    BOOST_CHECK(runner.errorHasLines({
        "Dataspace(24).DeleteDataspaces(4) failed: ETP#5: Cannot delete default space"
    }));

    // Get the TOC of the default space
    runner.startAndWait({
        "space", "--toc", "-s", "", "--top", "2",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_,
        "--utc", "--iso" // Use UTC & ISO, to avoid diffs by site on CI machines
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Discovering eml:///...",
        "Got 7 resources.",
        "",
        "              Name: Hdf5File",
        "              Type: eml20.EpcExternalPartReference",
        "              Guid: e5e5230e-ecdd-4605-85ad-446926eeb47f",
        "     Last-Modified: 2016-06-13T11:38:11 UTC",
        "  Store-Last-Write: 1970-01-01T00:00:00 UTC",
        "       Custom-Data: 1 entry",
        "",
        "              Name: Default local depth CRS",
        "              Type: resqml20.obj_LocalDepth3dCrs",
        "              Guid: 1c8bb605-c8e7-4da9-af4c-52e5a5e5147b",
        "     Last-Modified: 1970-01-01T00:00:00 UTC",
        "  Store-Last-Write: 1970-01-01T00:00:00 UTC",
        "       Custom-Data: 1 entry",
        "",
        "(5 not shown. remaining. Increase --top or use --skip to see more)",
        "(1 resource has no Last-Modified timestamp)",
        "(7 resources have no Store-Last-Write timestamp)",
        "(6 resources Last-Modified between 2016-06-13T09:36:34 UTC",
        "                               and 2016-06-13T11:38:11 UTC)",
    }));

    // Variant of the TOC, with with filtering. Several things to note:
    // - added resqml20. prefix automatically (shows type in --verbose)
    // - New URIs entries in --verbose
    // - Filtering did find a single entry.
    runner.startAndWait({
        "space", "--toc", "--types", "obj_WellboreTrajectoryRepresentation", "-s", "", "--top", "2",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_, "-v",
        "--utc", "--iso" // Use UTC & ISO, to avoid diffs by site on CI machines
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Filtering on type: resqml20.obj_WellboreTrajectoryRepresentation",
        "Discovering eml:///...",
        "Got 1 resource.",
        "",
        "               URI: eml:///resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)",
        "    Alternate-URIs: 0",
        "              Name: POSITION_OPTIMIZED",
        "              Type: resqml20.obj_WellboreTrajectoryRepresentation",
        "              Guid: 4a6e1e5a-54f3-589a-b197-6e2ce35699f1",
        "     Last-Modified: 2016-06-13T09:36:34 UTC",
        "  Store-Last-Write: 1970-01-01T00:00:00 UTC",
        "       Custom-Data: 1 entry",
    }));

    // Variant again of the TOC, with multi-type filtering
    runner.startAndWait({
        "space", "--toc", "--types", "resqml20.obj_WellboreFeature|obj_WellboreInterpretation",
        "-s", "", "--top", "5",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_,
        "--utc", "--iso" // Use UTC & ISO, to avoid diffs by site on CI machines
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Discovering eml:///...",
        "Got 2 resources.",
        "",
        "              Name: 49025112030000 feature",
        "              Type: resqml20.obj_WellboreFeature",
        "              Guid: 7d9e5e11-7fff-53c3-b645-121655c6922c",
        "     Last-Modified: 2016-06-13T09:36:34 UTC",
        "  Store-Last-Write: 1970-01-01T00:00:00 UTC",
        "       Custom-Data: 1 entry",
        "",
        "              Name: 49025112030000",
        "              Type: resqml20.obj_WellboreInterpretation",
        "              Guid: ec43b905-4faf-5284-b279-aa7b9b22cd52",
        "     Last-Modified: 2016-06-13T09:36:34 UTC",
        "  Store-Last-Write: 1970-01-01T00:00:00 UTC",
        "       Custom-Data: 1 entry",
        "",
        "(2 resources have no Store-Last-Write timestamp)",
        "(2 resources Last-Modified on 2016-06-13T09:36:34 UTC)",
    }));

    // Variant again of the TOC, with multi-type filtering
    runner.startAndWait({
        "space", "--toc", "--types", "resqml20.obj_WellboreFeature|obj_WellboreInterpretation",
        "-s", "", "--top", "5",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_,
        "--utc", "--iso", // Use UTC & ISO, to avoid diffs by site on CI machines
        "--since", "24h"
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Discovering eml:///...",
        "No resources matching filter(s).",
    }));

    // Check output to JON file, instead of the console.
    const std::string& json_output = oes::testing::resolveTestDataFile(
        "src/tests/executables/oes_eml", "DefaultSpace.json", false
    );

    // Get the TOC of the default space
    runner.startAndWait({
        "space", "--toc", json_output, "-s", "",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Discovering eml:///...",
        "Wrote 7 resources to JSON file " + json_output,
    }));

    std::vector<char> buffer;
    BOOST_REQUIRE(oes::testing::readFile(json_output, buffer));
    BOOST_CHECK_EQUAL(buffer.size(), size_t(1628));
    BOOST_REQUIRE(json_validate_syntax(buffer.data(), buffer.size()));
    std::string head(&buffer[0], &buffer[std::min<>(size_t(41), buffer.size())]);
    BOOST_CHECK_EQUAL(head, R"json({"dataspace":"","resources":[{"uri":"eml:)json");
*/
}

BOOST_AUTO_TEST_CASE(test_new_space) {
    startServer();

    Client_uptr eml_clt = connectClient(); // inits clt_conf_.server_url_ used below
    OES_UNUSED(eml_clt);

    OESEmlRunner runner;
    runner.startAndWait({
        "space", "--list", "--flat",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "No dataspaces",
    }));

    runner.startAndWait({
        "space", "--new", "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    runner.startAndWait({
        "space", "--info",  "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
  //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Dataspace foo/bar:",
        "           URI: eml:///dataspace('foo/bar')",
        "           Uid: foo/bar",
        "          Path: foo/bar",
    }));

    runner.startAndWait({
        "space", "--delete", "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    runner.startAndWait({
        "space", "--new", "-s", "foo/bar", "--space-guid",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    runner.startAndWait({
        "space", "--info",  "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
    //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outBuf_.find("Path: foo/bar")  != std::string::npos);
    BOOST_CHECK(runner.outBuf_.find("URI: eml:///dataspace('foo/bar')") == std::string::npos); // Should be generated uuid
    BOOST_CHECK(runner.outBuf_.find("Uid: foo/bar") == std::string::npos); // Should be generated uuid instead of foo/bar
    BOOST_CHECK(runner.outBuf_.find("custom data:") != std::string::npos);

    runner.startAndWait({
        "space", "--lock", "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    runner.startAndWaitExpecting(1,{
        "space", "--delete", "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    BOOST_CHECK(runner.errBuf_.find("Dataspace(24).DeleteDataspaces(4) failed:")  != std::string::npos);
    BOOST_CHECK(runner.errBuf_.find("has no permissions to")  != std::string::npos);

    runner.startAndWait({
        "space", "--unlock", "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    runner.startAndWait({
        "space", "--delete", "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    // test creation of new space with xdata
    const std::string guid = Guid::make().toXmlString();
    XDataMap xmap;
    XDataMapBuilder xmb{xmap};
    xmap.emplace("nil", Datatypes::DataValue());
    xmb.emplace("key", "value");
    const std::string xdata= xdataToJson(xmap);

    runner.startAndWait({
        "space", "--new", "-s", "foo/bar", "--space-guid", guid,
        "-u=foo", "-p=bar", "--xdata", xdata, "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });

    runner.startAndWait({
        "space", "--info",  "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
    //BOOST_CHECK(runner.noErrors());
    BOOST_CHECK(runner.outputHasLines({
        "Dataspace foo/bar:",
        "           URI: eml:///dataspace('" + guid + "')",
        "           Uid: " + guid,
        "          Path: foo/bar",
    }));
    BOOST_CHECK(runner.outputHasLines({
        "5 custom data:",
        "           key: true",
        "        locked: false",
        "           nil: <NULL>",
        "     read-only: false",
        "          size: 232 kB"
    }));
    runner.startAndWait({
        "space", "--delete", "-s", "foo/bar",
        "-u=foo", "-p=bar", "-k", jwtSecret_,
        "-S", clt_conf_.server_url_
    });
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
