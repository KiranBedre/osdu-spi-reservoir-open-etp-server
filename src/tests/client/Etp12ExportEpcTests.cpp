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
#include <oes/client/Client.hpp>
#include <oes/common/etp12/EmlUri.h>
#include <oes/eml/etp12/Extensions.h>

#include <oes/common/epc/Package.h>

#include <oes/core/utils/Logger.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/eml/ConsoleUtils.h>

#include <filesystem>

namespace fs = std::filesystem;
using namespace oes::eml::etp12;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

namespace {

using oes::testing::test_case_name;
using oes::testing::test_suite_name;

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

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
