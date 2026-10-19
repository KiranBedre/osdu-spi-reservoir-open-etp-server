// ============================================================================
// Copyright 2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/testing/eml/OpenKVClientServerFixture.h>

#include <oes/client/ExportEpc.h>
#include <oes/client/ImportEpc.h>
#include <oes/client/ImportExportUtils.h>
#include <oes/client/Client.hpp>
#include <oes/common/etp12/EmlUri.h>
#include <oes/eml/etp12/Extensions.h>

#include <oes/common/epc/Package.h>

#include <oes/core/utils/Logger.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/eml/ConsoleUtils.h>
#include <oes/testing/eml/TestingMocks.h>

#include <filesystem>

namespace fs = std::filesystem;
using namespace oes::eml::etp12;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;
using oes::common::etp12::EmlUri;
using oes::client::HdfPathKey;

namespace {

using oes::testing::test_case_name;
using oes::testing::test_suite_name;

BOOST_AUTO_TEST_SUITE(ExportEpcPlanHdfPathsTests)

BOOST_AUTO_TEST_CASE(test_deterministic_winner) {
    const HdfPathKey a{"A", "a/b"};
    const HdfPathKey b{"B", "/a/b"};
    for (const auto& keys : { std::vector<HdfPathKey>{ a, b }, std::vector<HdfPathKey>{ b, a } }) {
        const auto plan = oes::client::planHdfPaths(keys);
        BOOST_REQUIRE_EQUAL(plan.size(), size_t(2));
        BOOST_CHECK_EQUAL(plan.at(a), "a/b");
        BOOST_CHECK_EQUAL(plan.at(b), "B/a/b");
    }
}

BOOST_AUTO_TEST_CASE(test_same_key_deduplicated) {
    const auto plan = oes::client::planHdfPaths({ {"A", "/a/b"}, {"A", "a/b"}, {"A", "//a/b"} });
    BOOST_REQUIRE_EQUAL(plan.size(), size_t(3));
    for (const auto& entry : plan) {
        BOOST_CHECK_EQUAL(entry.second, "a/b");
    }
}

BOOST_AUTO_TEST_CASE(test_no_collision_unchanged) {
    const auto plan = oes::client::planHdfPaths({ {"A", "/a/b"}, {"B", "/a/c"}, {"A", "/d"} });
    BOOST_CHECK_EQUAL(plan.at({"A", "/a/b"}), "a/b");
    BOOST_CHECK_EQUAL(plan.at({"B", "/a/c"}), "a/c");
    BOOST_CHECK_EQUAL(plan.at({"A", "/d"}), "d");
}

BOOST_AUTO_TEST_CASE(test_dataset_group_overlap) {
    // a/b cannot be both a dataset and a group
    const auto plan = oes::client::planHdfPaths({ {"B", "a/b/c"}, {"A", "a/b"} });
    BOOST_CHECK_EQUAL(plan.at({"A", "a/b"}), "a/b");
    BOOST_CHECK_EQUAL(plan.at({"B", "a/b/c"}), "B/a/b/c");
}

BOOST_AUTO_TEST_CASE(test_suffix_when_taken) {
    const auto plan = oes::client::planHdfPaths({ {"A", "x"}, {"B", "x"}, {"C", "B/x"} });
    BOOST_CHECK_EQUAL(plan.at({"C", "B/x"}), "B/x");
    BOOST_CHECK_EQUAL(plan.at({"A", "x"}), "x");
    BOOST_CHECK_EQUAL(plan.at({"B", "x"}), "B_2/x");
}

BOOST_AUTO_TEST_CASE(test_container_sanitized) {
    const auto plan = oes::client::planHdfPaths({ {"A", "x"}, {"eml:///a('b')/c", "x"} });
    BOOST_CHECK_EQUAL(plan.at({"eml:///a('b')/c", "x"}), "eml____a__b___c/x");
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    for (size_t pos = s.find(from); pos != std::string::npos; pos = s.find(from, pos + to.size())) {
        s.replace(pos, from.size(), to);
    }
    return s;
}

EmlObjMock cloneMock(const EmlObjMock& mock, const std::string& new_uuid) {
    EmlObjMock clone = mock;
    const std::string old_uuid = mock.guid_.toXmlString();
    clone.xml_ = replaceAll(clone.xml_, "uuid=\"" + old_uuid + '"', "uuid=\"" + new_uuid + '"');
    clone.guid_ = oes::core::Guid::parse(new_uuid);
    return clone;
}

ArrayOfDouble doubles(int64_t count, double offset) {
    ArrayOfDouble ary;
    ary.m_values.resize(size_t(count));
    for (size_t i = 0; i < ary.m_values.size(); ++i) {
        ary.m_values[i] = offset + double(i);
    }
    return ary;
}

void createSpace(oes::client::Client& clt, const std::string& space_path, std::chrono::milliseconds timeout) {
    auto space_rpc = oes::client::rpcFor(clt.space_proto(), &IDataspace::PutDataspaces);
    Object::Dataspace space;
    space.m_path = space_path;
    space.m_uri = EmlUri::make_space(space_path).to_string();
    space_rpc.req_.m_dataspaces.emplace("1", space);

    std::ostringstream oss;
    auto ok = space_rpc.call(oss, timeout);
    if (!call_succeeded(ok, oss)) {
        BOOST_REQUIRE_MESSAGE(false, call_err_msg(ok, oss));
    }
}

std::vector<double> getDoubles(
    oes::client::Client& clt, const std::string& proxy_uri, const std::string& path,
    std::chrono::milliseconds timeout
) {
    auto rpc = oes::client::rpcFor(clt.array_proto(), &IDataArray::GetDataArrays);
    DataArrayTypes::DataArrayIdentifier ident;
    ident.m_uri = proxy_uri;
    ident.m_pathInResource = path;
    rpc.req_.m_dataArrays.emplace("1", ident);

    std::ostringstream oss;
    auto ok = rpc.call(oss, timeout);
    if (!call_succeeded(ok, oss)) {
        BOOST_REQUIRE_MESSAGE(false, path + ": " + call_err_msg(ok, oss));
    }
    BOOST_REQUIRE_EQUAL(rpc.res_.m_dataArrays.size(), size_t(1));
    const auto& item = rpc.res_.m_dataArrays.begin()->second.m_data.m_item;
    BOOST_REQUIRE_EQUAL(item.index(), size_t(AnyArray_item_e::eArrayOfDouble));
    return std::get<ArrayOfDouble>(item).m_values;
}

BOOST_FIXTURE_TEST_SUITE(Etp12ExportEpcTests, OpenKVClientServerFixture)

BOOST_AUTO_TEST_CASE(test_small_epc) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );
    const std::string& hdf_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.h5", true
    );
    const std::string space_path = "EPCs/OneWell";
    current_space_ = space_path;

    initServer();

    startServer();
    for (bool compress : { true, false }) {
        clt_conf_.compress_ = compress;

        auto clt = connectClient();
        directImport(epc_input, space_path);

        const std::chrono::milliseconds timeout(10'000); // for debugging

        const std::string& basename = test_suite_name() + '-' + test_case_name();
        const std::string epc_output = oes::testing::resolveTestDataFile(
            "data/openkv", basename + ".epc", false // output - deleted if exist
        );
        const std::string hdf_output = oes::testing::resolveTestDataFile(
            "data/openkv", basename + ".h5", false // output - deleted if exist
        );

        oes::client::ExportEpcOptions options;
        options.timeout_ = timeout;
        oes::client::EpcStatistics stats;

        const bool export_ok = exportEpc(
            *clt, space_path, epc_output, hdf_output, options, &stats
        );
        BOOST_CHECK(export_ok);

        clt.reset();

        const fs::path epc_path(epc_output);
        fs::file_status epc_stat = fs::status(epc_path);
        BOOST_REQUIRE(fs::is_regular_file(epc_stat));

        const fs::path hdf_path(hdf_output);
        fs::file_status hdf_stat = fs::status(hdf_path);
        BOOST_CHECK(fs::is_regular_file(hdf_stat));

        using namespace oes::common;
        epc::Package package;
        package.openForReading(epc_output);

        using ContentTypeMap = epc::FileContentType::ContentTypeMap;
        const ContentTypeMap& contentTypes = package.getFileContentType().getAllContentType();
        BOOST_CHECK_EQUAL(size_t(10), contentTypes.size());

        // TODO: More thorough checking of the EPC.
        // Note that relations of the input EPC seem wrong,
        // while the output one seem correct.
        // That input EPC is old and buggy it seems..
        BOOST_CHECK_EQUAL(7, stats.object_count_);
        BOOST_CHECK_EQUAL(7, stats.relation_count_);
        BOOST_CHECK_EQUAL(42606, stats.object_bytes_); //Paths in resources are different

        BOOST_CHECK_EQUAL(4, stats.array_count_);
        BOOST_CHECK_EQUAL(1, stats.array_chunks_);
        BOOST_CHECK_EQUAL(200, stats.array_bytes_);

        // Check the HDF5 content we exported, against the original
        OESEmlRunner runner;
        runner.startAndWait({ "hdf5", "--diff", hdf_input, "--with", hdf_output });
        BOOST_REQUIRE(runner.noErrors());
        BOOST_CHECK(runner.outputLinesMatch({
            ".*No diffs in 4 common datasets \\(200 uncompressed bytes\\)\\..*",
        }));
        BOOST_CHECK(runner.outBuf_.find("missing") == std::string::npos);
        BOOST_CHECK(runner.outBuf_.find("extra") == std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(test_large_epc) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "PillarGrid.epc", true
    );
    const std::string& hdf_input = oes::testing::resolveTestDataFile(
        "data/eml", "PillarGrid.h5", true
    );
    const std::string space_path = "EPCs/PillarGrid";
    current_space_ = space_path;

    initServer();

    startServer();
    auto clt = connectClient();
    directImport(epc_input, space_path);

    const std::chrono::milliseconds timeout(10'000); // for debugging

    const std::string& basename = test_suite_name() + '-' + test_case_name();
    const std::string epc_output = oes::testing::resolveTestDataFile(
        "data/openkv", basename + ".epc", false // output - deleted if exist
    );
    const std::string hdf_output = oes::testing::resolveTestDataFile(
        "data/openkv", basename + ".h5", false // output - deleted if exist
    );

    oes::client::ExportEpcOptions options;
    options.timeout_ = timeout;
    oes::client::EpcStatistics stats;

    const bool export_ok = exportEpc(
        *clt, space_path, epc_output, hdf_output, options, &stats
    );
    BOOST_CHECK(export_ok);

    clt.reset();

    const fs::path epc_path(epc_output);
    fs::file_status epc_stat = fs::status(epc_path);
    BOOST_REQUIRE(fs::is_regular_file(epc_stat));

    const fs::path hdf_path(hdf_output);
    fs::file_status hdf_stat = fs::status(hdf_path);
    BOOST_CHECK(fs::is_regular_file(hdf_stat));

    using namespace oes::common;
    epc::Package package;
    package.openForReading(epc_output);

    using ContentTypeMap = epc::FileContentType::ContentTypeMap;
    const ContentTypeMap& contentTypes = package.getFileContentType().getAllContentType();
    BOOST_CHECK_EQUAL(size_t(108), contentTypes.size());

    // TODO: More thorough checking of the EPC.
    BOOST_CHECK_EQUAL(105, stats.object_count_);
    BOOST_CHECK_EQUAL(112, stats.relation_count_);
    BOOST_CHECK_EQUAL(256955, stats.object_bytes_);

    BOOST_CHECK_EQUAL(37, stats.array_count_);
    BOOST_CHECK_EQUAL(1, stats.array_chunks_);
    BOOST_CHECK_EQUAL(3641776, stats.array_bytes_);

    // Check the HDF5 content we exported, against the original
    OESEmlRunner runner;
    runner.startAndWait({ "hdf5", "--diff", hdf_input, "--with", hdf_output, "--log_level=warning" });
    BOOST_REQUIRE(runner.noErrors());
    BOOST_CHECK(runner.outputLinesMatch({ // Diffs probably harmless. TODO to double-check
        ".*28 common datasets differ \\(out of 37\\). \\(Use -v and --top to see which ones\\).*",
        ".*ALL those differences could be harmless integer conversions:.*",
        ".*1 +short -> int.*",
        ".*26 +uint -> long.*",
        ".*1 +ulong -> long.*",
    }));
    BOOST_CHECK(runner.outBuf_.find("missing") == std::string::npos);
    BOOST_CHECK(runner.outBuf_.find("extra") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(test_large_epc_mt) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "PillarGrid.epc", true
    );
    const std::string& hdf_input = oes::testing::resolveTestDataFile(
        "data/eml", "PillarGrid.h5", true
    );
    const std::string space_path = "EPCs/PillarGrid";
    current_space_ = space_path;

    initServer();
    startServer();
    auto clt = connectClient();
    directImport(epc_input, space_path);

    const std::chrono::milliseconds timeout(10'000); // for debugging

    const std::string& basename = test_suite_name() + '-' + test_case_name();
    const std::string epc_output = oes::testing::resolveTestDataFile(
        "data/openkv", basename + ".epc", false // output - deleted if exist
    );
    const std::string hdf_output = oes::testing::resolveTestDataFile(
        "data/openkv", basename + ".h5", false // output - deleted if exist
    );

    oes::client::ExportEpcOptions options;
    options.timeout_ = timeout;
    options.thread_count_= 3;
    oes::client::EpcStatistics stats;

    const bool export_ok = exportEpcMt(
        *clt, space_path, epc_output, hdf_output, options, &stats
    );
    BOOST_CHECK(export_ok);

    clt.reset();

    const fs::path epc_path(epc_output);
    fs::file_status epc_stat = fs::status(epc_path);
    BOOST_REQUIRE(fs::is_regular_file(epc_stat));

    const fs::path hdf_path(hdf_output);
    fs::file_status hdf_stat = fs::status(hdf_path);
    BOOST_CHECK(fs::is_regular_file(hdf_stat));

    using namespace oes::common;
    epc::Package package;
    package.openForReading(epc_output);

    using ContentTypeMap = epc::FileContentType::ContentTypeMap;
    const ContentTypeMap& contentTypes = package.getFileContentType().getAllContentType();
    BOOST_CHECK_EQUAL(size_t(108), contentTypes.size());

    // TODO: More thorough checking of the EPC.
    BOOST_CHECK_EQUAL(105, stats.object_count_);
    BOOST_CHECK_EQUAL(112, stats.relation_count_);
    BOOST_CHECK_EQUAL(256955, stats.object_bytes_);

    BOOST_CHECK_EQUAL(37, stats.array_count_);
    BOOST_CHECK_EQUAL(1, stats.array_chunks_);
    BOOST_CHECK_EQUAL(3641776, stats.array_bytes_);

    // Check the HDF5 content we exported, against the original
    OESEmlRunner runner;
    runner.startAndWait({ "hdf5", "--diff", hdf_input, "--with", hdf_output, "--log_level=warning" });
    BOOST_REQUIRE(runner.noErrors());
    BOOST_CHECK(runner.outputLinesMatch({ // Diffs probably harmless. TODO to double-check
        ".*28 common datasets differ \\(out of 37\\). \\(Use -v and --top to see which ones\\).*",
        ".*ALL those differences could be harmless integer conversions:.*",
        ".*1 +short -> int.*",
        ".*26 +uint -> long.*",
        ".*1 +ulong -> long.*",
        }));
    BOOST_CHECK(runner.outBuf_.find("missing") == std::string::npos);
    BOOST_CHECK(runner.outBuf_.find("extra") == std::string::npos);
}

// Two EpcExternalPartReference (P1 < P2), each holding datasets at the same paths,
// with different values. T1 and T3 reference P1's datasets, T2 references P2's.
BOOST_AUTO_TEST_CASE(test_path_shared_by_two_proxies) {
    const std::string space_path = "EPCs/SharedPaths";
    const std::chrono::milliseconds timeout(10'000);

    const std::string p1_uuid = "e5e5230e-ecdd-4605-85ad-446926eeb47f";
    const std::string p2_uuid = "f5e5230e-ecdd-4605-85ad-446926eeb47f";
    const std::string t2_uuid = "4a6e1e5a-54f3-589a-b197-6e2ce3569902";
    const std::string t3_uuid = "4a6e1e5a-54f3-589a-b197-6e2ce3569903";
    const double p1_offset = 0.0;
    const double p2_offset = 1'000'000.0;

    const EmlObjMock& p1 = epc_ext_mock();
    BOOST_REQUIRE_EQUAL(p1.guid_.toXmlString(), p1_uuid);
    const EmlObjMock p2 = cloneMock(p1, p2_uuid);

    EmlObjMock t1 = traj_mock();
    EmlObjMock::erase_tag("resqml:RepresentedInterpretation", t1.xml_);
    EmlObjMock t2 = cloneMock(t1, t2_uuid);
    t2.xml_ = replaceAll(t2.xml_, p1_uuid, p2_uuid);
    const EmlObjMock t3 = cloneMock(t1, t3_uuid);

    initServer();
    startServer();
    auto clt = connectClient();

    createSpace(*clt, space_path, timeout);
    current_space_ = space_path;

    const EmlUri space_uri = EmlUri::make_space(space_path);
    auto proxyUri = [](const std::string& space, const EmlObjMock& proxy) {
        return EmlUri::make_obj(space, proxy.type_, proxy.guid_, EmlUri::eEML20).to_string();
    };

    {   // Built with RPCs, since importEpc() supports a single HDF5 file
        oes::client::ScopedTransaction tx(*clt, timeout);
        BOOST_REQUIRE(tx.start(space_uri.to_string(), "test", false));

        auto obj_rpc = oes::client::rpcFor(clt->store_proto(), &IStore::PutDataObjects);
        int key = 0;
        for (const EmlObjMock* mock : std::initializer_list<const EmlObjMock*>{
            &crs_mock(), &datum_mock(), &p1, &p2, &t1, &t2, &t3
        }) {
            Object::DataObject obj;
            obj.m_resource.m_uri = space_uri.child_type(mock->type_, EmlUri::eRESQML20).child_obj(mock->guid_).to_string();
            obj.m_resource.m_name = mock->name_;
            obj.m_format = "xml";
            obj.m_data.assign(mock->xml_.begin(), mock->xml_.end());
            obj_rpc.req_.m_dataObjects.emplace(std::to_string(++key), std::move(obj));
        }
        std::ostringstream obj_oss;
        auto obj_ok = obj_rpc.call(obj_oss, timeout);
        if (!call_succeeded(obj_ok, obj_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(obj_ok, obj_oss));
        }

        auto ary_rpc = oes::client::rpcFor(clt->array_proto(), &IDataArray::PutDataArrays);
        for (const auto& [proxy, offset] : { std::make_pair(&p1, p1_offset), std::make_pair(&p2, p2_offset) }) {
            for (const EmlAryMock& mock : t1.arrays_) {
                DataArrayTypes::PutDataArraysType ary;
                ary.m_uid.m_uri = proxyUri(space_path, *proxy);
                ary.m_uid.m_pathInResource = mock.path_;
                ary.m_array.m_dimensions = mock.dims_;
                ary.m_array.m_data.m_item = doubles(mock.size(), offset);
                ary_rpc.req_.m_dataArrays.emplace(std::to_string(++key), std::move(ary));
            }
        }
        std::ostringstream ary_oss;
        auto ary_ok = ary_rpc.call(ary_oss, timeout);
        if (!call_succeeded(ary_ok, ary_oss)) {
            BOOST_REQUIRE_MESSAGE(false, call_err_msg(ary_ok, ary_oss));
        }

        BOOST_REQUIRE(tx.commit());
    }

    const std::string traj_path = "RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints";
    const std::string relocated_path = p2_uuid + '/' + traj_path;
    auto partOf = [](const std::string& uuid) {
        return "obj_WellboreTrajectoryRepresentation_" + uuid + ".xml";
    };

    for (const bool mt : { false, true }) {
        BOOST_TEST_CONTEXT("multi-threaded: " << mt) {
            const std::string& basename = test_suite_name() + '-' + test_case_name() + (mt? "-mt": "");
            const std::string epc_output = oes::testing::resolveTestDataFile(
                "data/openkv", basename + ".epc", false // output - deleted if exist
            );
            const std::string hdf_output = oes::testing::resolveTestDataFile(
                "data/openkv", basename + ".h5", false // output - deleted if exist
            );

            oes::client::ExportEpcOptions options;
            options.timeout_ = timeout;
            oes::client::EpcStatistics stats;
            bool export_ok = false;
            if (mt) {
                options.thread_count_ = 3;
                export_ok = exportEpcMt(*clt, space_path, epc_output, hdf_output, options, &stats);
            } else {
                export_ok = exportEpc(*clt, space_path, epc_output, hdf_output, options, &stats);
            }
            BOOST_REQUIRE(export_ok);

            BOOST_CHECK_EQUAL(7, stats.object_count_);
            BOOST_CHECK_EQUAL(6, stats.array_count_); // 3 paths x 2 proxies

            {   // Only T2's part references relocated datasets
                using namespace oes::common;
                epc::Package package;
                BOOST_REQUIRE(package.openForReading(epc_output));

                const std::string t1_xml = package.extractFile(partOf(t1.guid_.toXmlString()));
                const std::string t2_xml = package.extractFile(partOf(t2_uuid));
                const std::string t3_xml = package.extractFile(partOf(t3_uuid));
                BOOST_REQUIRE(!t1_xml.empty() && !t2_xml.empty() && !t3_xml.empty());

                BOOST_CHECK(t1_xml.find(">/" + traj_path + "<") != std::string::npos);
                BOOST_CHECK(t3_xml.find(">/" + traj_path + "<") != std::string::npos);
                BOOST_CHECK(t2_xml.find(">/" + relocated_path + "<") != std::string::npos);
                BOOST_CHECK(t2_xml.find(">/RESQML/") == std::string::npos);
                BOOST_CHECK(t1_xml.find(p2_uuid) == std::string::npos);
                BOOST_CHECK(t3_xml.find(p2_uuid) == std::string::npos);
                package.close();
            }

            // Round trip: both proxies' values survive, at their (container, path)
            const std::string copy_path = space_path + (mt? "CopyMt": "Copy");
            createSpace(*clt, copy_path, timeout);
            created_spaces_.push_back(copy_path);

            oes::client::ImportEpcOptions import_options;
            import_options.timeout_ = timeout;
            BOOST_REQUIRE(oes::client::importEpc(clt->getConfig(), copy_path, epc_output, import_options));

            const std::vector<double> p1_values = getDoubles(*clt, proxyUri(copy_path, p1), traj_path, timeout);
            const std::vector<double> p2_values = getDoubles(*clt, proxyUri(copy_path, p2), relocated_path, timeout);
            BOOST_CHECK_EQUAL(p1_values.size(), size_t(1673 * 3));
            BOOST_CHECK_EQUAL(p2_values.size(), size_t(1673 * 3));
            BOOST_CHECK(p1_values == doubles(int64_t(p1_values.size()), p1_offset).m_values);
            BOOST_CHECK(p2_values == doubles(int64_t(p2_values.size()), p2_offset).m_values);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
