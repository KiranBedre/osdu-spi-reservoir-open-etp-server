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

#include <algorithm>
#include <filesystem>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/common/epc/Package.h>

#if WIN32
#include <oes/core/utils/Utf8.h>
#define FORCE_UTF8_ENCODING
#endif

#include <oes/core/utils/Guid.hpp>
#include <oes/testing/core/TestMain.h>


namespace {

using namespace oes::common;

namespace FileSystem {

/*!
 * \brief Creates the directory path
 * \param[in] path the path to the new directory to create,, assumed to be UTF-8 encoded.
 * \retval true if a new directory was created, otherwise false.
 */
bool createDirectory(const std::string& path) {
#ifdef FORCE_UTF8_ENCODING
    const std::wstring filename = oes::core::utf8::fromUtf8ToWstr(path);
#else
    const std::string filename = path;
#endif
    return std::filesystem::create_directory(filename);
}

/*!
 * \brief Checks if the given file path corresponds to an existing file or directory
 * \details
 * \param[in] path to examine, assumed to be UTF-8 encoded.
 * \retval true if the file exists
 * \retval false otherwise
 * \warning This function should be used instead of std::filesystem::exists(),
 * because the latter does not correctly deal with UTF-8 encoded filenames on Windows.
 */
bool fileExists(const std::string& path) {
#ifdef FORCE_UTF8_ENCODING
    const std::wstring filename = oes::core::utf8::fromUtf8ToWstr(path);
#else
    const std::string filename = path;
#endif
    return std::filesystem::exists(filename);
}
}

//============================================================================

BOOST_AUTO_TEST_SUITE(EpcPackageTests)

BOOST_AUTO_TEST_CASE(test_openForWriting) {
    const std::string& epc_output = oes::testing::resolveTestDataFile(
        "data/epc", "test_openForWriting.epc", false
    );

    epc::Package package;
    package.openForWriting(epc_output);
    package.writePackage();
    package.close();

    BOOST_CHECK(FileSystem::fileExists(epc_output));

    using ContentTypeMap = epc::FileContentType::ContentTypeMap;
    const ContentTypeMap& contentTypes = package.getFileContentType().getAllContentType();
    BOOST_CHECK_EQUAL(size_t(2), contentTypes.size());
}

BOOST_AUTO_TEST_CASE(test_openForReading) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );

    epc::Package package;
    package.openForReading(epc_input);

    using ContentTypeMap = epc::FileContentType::ContentTypeMap;
    const ContentTypeMap& contentTypes = package.getFileContentType().getAllContentType();
    BOOST_CHECK_EQUAL(size_t(10), contentTypes.size());
}

BOOST_AUTO_TEST_CASE(test_fileExists) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );
    const std::string expectedFileEntry = "docProps/_rels/core.xml.rels";

    // Check for the existence of ZIP entries
    epc::Package package;
    package.openForReading(epc_input);
    BOOST_CHECK_EQUAL(true, package.fileExists(expectedFileEntry));
    BOOST_CHECK_EQUAL(false, package.fileExists("docProps/_rels/unknown_file.txt"));
}

BOOST_AUTO_TEST_CASE(test_extractFile) {
    const std::string& epc_output = oes::testing::resolveTestDataFile(
        "data/eml", "interp_well_survey.epc", true
    );
    const std::string zipFileEntry = "obj_WellboreMarkerFrameRepresentation_03a31de4-5235-55c7-b649-99cd36adaa12.xml";
    constexpr size_t expectedFileSize = 48592;
    const std::string expectedFileHash = "19704c3e-c979-5121-99cb-0aa5c28168cf";

    // Extract file
    epc::Package package;
    package.openForReading(epc_output);
    const auto actualFileContent = package.extractFile(zipFileEntry);
    BOOST_CHECK_EQUAL(expectedFileSize, actualFileContent.size());

    // Calculate and check hash
    const auto actualFileHash = oes::core::Guid::make_deterministic(actualFileContent).toXmlString();
    BOOST_CHECK_EQUAL(expectedFileHash, actualFileHash);
}

BOOST_AUTO_TEST_CASE(test_createPart) {
    const std::string& epc_output = oes::testing::resolveTestDataFile(
        "data/epc", "test_createPart.epc", false
    );
    const std::string zipFileEntry = "test/test.txt";
    const std::string expectedFileContent = "test data";

    // Create test package
    epc::Package package;
    package.openForWriting(epc_output);
    const auto filePart = package.createPart(expectedFileContent, zipFileEntry);
    BOOST_CHECK(filePart != nullptr);
    package.writePackage();
    package.close();

    BOOST_CHECK(FileSystem::fileExists(epc_output));

    // Extract file
    package.openForReading(epc_output);
    const auto actualFileContent = package.extractFile(zipFileEntry);
    BOOST_CHECK_EQUAL(expectedFileContent, actualFileContent);
}

BOOST_AUTO_TEST_CASE(test_createPart_batch) {
    constexpr size_t TestZipEntriesCount = 20;
    const std::string& epc_output = oes::testing::resolveTestDataFile(
        "data/epc", "test_createPart_batch.epc", false
    );
    std::vector<std::pair<std::string, oes::common::epc::FilePart*>> fileParts;
    fileParts.reserve(TestZipEntriesCount);

    // Create test package
    epc::Package package;
    package.openForWriting(epc_output);
    for (size_t i=0; i< TestZipEntriesCount; ++i)
    {
        const std::string content = std::to_string(i);
        const std::string zipFileEntry = fmt::format("test/test_{}.txt", i);
        fileParts.emplace_back(zipFileEntry, package.createPart(content, zipFileEntry));
    }

    // Pointers of created parts should not be invalidated after creation new parts
    for (const auto& filePart : fileParts) {
        BOOST_CHECK_EQUAL(filePart.second, package.findPart(filePart.first));
    }

    package.writePackage();
    package.close();
    BOOST_CHECK(FileSystem::fileExists(epc_output));

    // Check that all parts were successfully saved
    package.openForReading(epc_output);
    BOOST_CHECK_EQUAL(TestZipEntriesCount, package.getAllFilePart().size());
}

BOOST_AUTO_TEST_CASE(test_createPart_entryTime) {
    const std::string& epc_output = oes::testing::resolveTestDataFile(
        "data/epc", "test_createPart_entryTime.epc", false
    );
    const std::string zipFileEntry = "test/test.txt";
    const std::string expectedFileContent = "test data";

    // Create test package
    epc::Package package;
    package.openForWriting(epc_output);
    const std::time_t startWriteTime = std::time(nullptr);
    //const std::time_t startWriteTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    package.createPart(expectedFileContent, zipFileEntry);
    const std::time_t endWriteTime = std::time(nullptr);
    //const std::time_t endWriteTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    package.writePackage();
    package.close();

    BOOST_CHECK(FileSystem::fileExists(epc_output));

    // Read ZIP-entries descriptors
    epc::PackageEntryVec pkg_entries;
    const bool scan_rc = package.scan(epc_output, 10, false,
        [&pkg_entries](const epc::PackageInfo&, epc::PackageEntryVec entries) {
        pkg_entries = std::move(entries);
        return epc::eZIP_SCAN_STOP_OK;
    });
    BOOST_REQUIRE(scan_rc);

    // Find test entry and check time
    const auto testEntryIt = std::find_if(pkg_entries.cbegin(), pkg_entries.cend(), [&zipFileEntry](const auto& entry) {
        return entry.zip_entry_ == zipFileEntry;
    });
    BOOST_REQUIRE(testEntryIt != pkg_entries.cend());
    // There is extended test interval by 1 second for avoid intermittent test fail (probably due switch threads)
    BOOST_CHECK(testEntryIt->tstamp_ >= startWriteTime-1 && testEntryIt->tstamp_ <= endWriteTime);
}

BOOST_AUTO_TEST_CASE(test_scan) {
    const std::string& epc_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.epc", true
    );

    const size_t expected_zip_entries = 19;

    { // get only the file info
        epc::Package package;
        epc::PackageInfo pkg_info;
        const bool scan_rc = package.scan(
            epc_input, 0, false,
            [&pkg_info](const epc::PackageInfo& pinfo, epc::PackageEntryVec) {
                pkg_info = pinfo;
                return epc::eZIP_SCAN_STOP_OK;
            }
        );
        BOOST_CHECK(scan_rc);
        BOOST_CHECK_EQUAL(expected_zip_entries, pkg_info.entry_count_);
        BOOST_CHECK_EQUAL("", pkg_info.global_comment_);
    }

    {
        size_t chunks = 0;
        epc::Package package;
        epc::PackageEntryVec all_entries;
        package.scan(
            epc_input, 7, false,
            [&chunks, &all_entries](const epc::PackageInfo&, epc::PackageEntryVec entries) {
                ++chunks;
                std::move(
                    entries.begin(), entries.end(),
                    std::back_inserter(all_entries)
                );
                return epc::eZIP_SCAN_CONTINUE;
            }
        );
        BOOST_CHECK_EQUAL(size_t(3), chunks);
        BOOST_CHECK_EQUAL(expected_zip_entries, all_entries.size());

        size_t total_size = 0;
        size_t total_zsize = 0;
        for (const epc::PackageEntry& entry : all_entries) {
            total_size += entry.size_;
            total_zsize += entry.zsize_;
        }
        BOOST_CHECK_EQUAL(size_t(52463), total_size);
        BOOST_CHECK_EQUAL(size_t(11755), total_zsize);
    }
}

BOOST_AUTO_TEST_CASE(test_scan_longZipFileEntry) {
    const std::string& epc_output = oes::testing::resolveTestDataFile(
        "data/epc", "test_long_zip_file_entry.epc", false
    );
    const std::string longZipFileEntry =
        "Zip internal sub-directory for test the long path that contains more than 255 characters/"
        "Zip internal sub-directory for test the long path that contains more than 255 characters/"
        "Zip internal sub-directory for test the long path that contains more than 255 characters/"
        "test zip file with the long name";
    assert(longZipFileEntry.size() > 255);

    { // Create test package
        epc::Package package;
        package.openForWriting(epc_output);
        package.createPart("test data", longZipFileEntry);
        package.writePackage();
        package.close();
    }

    BOOST_CHECK(FileSystem::fileExists(epc_output));

    { // Read ZIP-entries descriptors
        constexpr size_t expected_zip_entries = 4;

        epc::Package package;
        epc::PackageEntryVec pkg_entries;
        const bool scan_rc = package.scan(epc_output, 10, false,
            [&pkg_entries](const epc::PackageInfo&, epc::PackageEntryVec entries) {
            pkg_entries = std::move(entries);
            return epc::eZIP_SCAN_STOP_OK;
        });
        BOOST_CHECK(scan_rc);
        BOOST_CHECK_EQUAL(expected_zip_entries, pkg_entries.size());
        BOOST_CHECK_EQUAL(longZipFileEntry, pkg_entries.front().zip_entry_);
    }
}

std::string utf8_epc_file_name() {
    // UFT8 encoded: "Hello" in Russian
    std::string utf8_file_name = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    // UFT8 encoded: "Hello" in Chinese
    std::string utf8_result_dir = "\xE4\xBD\xA0\xE5\xA5\xBD";

    std::string folder = oes::testing::resolveTestDataFile(
        "data/epc", utf8_result_dir + "/", false
    );
    FileSystem::createDirectory(folder);

    return oes::testing::resolveTestDataFile(
        "data/epc/" + utf8_result_dir,
        utf8_file_name + ".epc", false
    );
}

BOOST_AUTO_TEST_CASE(test_utf8_openForWriting) {
    const std::string epc_output = utf8_epc_file_name();

    epc::Package package;
    package.openForWriting(epc_output);
    package.writePackage();
    package.close();

    BOOST_CHECK(FileSystem::fileExists(epc_output));

    using ContentTypeMap = epc::FileContentType::ContentTypeMap;
    const ContentTypeMap& contentTypes = package.getFileContentType().getAllContentType();
    BOOST_CHECK_EQUAL(size_t(2), contentTypes.size());
}

BOOST_AUTO_TEST_CASE(test_utf8_openForReading) {
    const std::string epc_input = utf8_epc_file_name();
    if (!FileSystem::fileExists(epc_input)) {
        epc::Package package;
        package.openForWriting(epc_input);
        package.writePackage();
        package.close();
    }

    epc::Package package;
    package.openForReading(epc_input);

    using ContentTypeMap = epc::FileContentType::ContentTypeMap;
    const ContentTypeMap& contentTypes = package.getFileContentType().getAllContentType();
    BOOST_CHECK_EQUAL(size_t(2), contentTypes.size());
}

BOOST_AUTO_TEST_CASE(test_utf8_scan) {
    const std::string epc_input = utf8_epc_file_name();

    if (!FileSystem::fileExists(epc_input)) {
        epc::Package package;
        package.openForWriting(epc_input);
        package.writePackage();
        package.close();
    }

    const size_t expected_zip_entries = 3;
    { // get only the file info
        epc::Package package;
        epc::PackageInfo pkg_info;
        const bool scan_rc = package.scan(
            epc_input, 0, false,
            [&pkg_info](const epc::PackageInfo& pinfo, epc::PackageEntryVec) {
                pkg_info = pinfo;
                return epc::eZIP_SCAN_STOP_OK;
            }
        );
        BOOST_CHECK(scan_rc);
        BOOST_CHECK_EQUAL(expected_zip_entries, pkg_info.entry_count_);
        BOOST_CHECK_EQUAL("", pkg_info.global_comment_);
    }
}
BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace
