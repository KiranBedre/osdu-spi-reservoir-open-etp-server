/*-----------------------------------------------------------------------
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"; you may not use this file except in compliance
with the License.  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
-----------------------------------------------------------------------*/

#ifndef OES_COMMON_EPC_PACKAGE
#define OES_COMMON_EPC_PACKAGE

// ============================================================================
// Copyright 2013-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include "FileCoreProperties.h"
#include "FileContentType.h"
#include "FileRelationship.h"
#include "FilePart.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <minizip/zip.h>
#include <minizip/unzip.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <unordered_map>
#include <functional>
#include <vector>
#include <ctime>

namespace oes::common::epc {

struct OES_COMMON_EXPORT PackageInfo {
    std::string global_comment_;
    size_t entry_count_ = 0;
};
struct OES_COMMON_EXPORT PackageEntry {
    std::string zip_entry_;     //!< entry's file name / path
    std::string xmlns_uri_;     //!< Root XML element Namespace URI
    std::string xml_ncname_;    //!< Root XML element Local Name

    size_t zsize_ = 0;          //!< entry's compressed size
    size_t size_ = 0;           //!< entry's uncompressed size
    std::time_t tstamp_ = 0;    //!< last modified datetime (since epoch)
    size_t disk_offset_ = 0;
    unsigned crc_ = 0;
};
enum ZipScannerRc {
    eZIP_SCAN_CONTINUE,
    eZIP_SCAN_STOP_OK,
    eZIP_SCAN_STOP_KO
};
using PackageEntryVec = std::vector<PackageEntry>;
using PackageScanner = std::function<ZipScannerRc(const PackageInfo&, PackageEntryVec)>;

class OES_COMMON_EXPORT Package {
public:
    using PartMap = std::unordered_map<std::string, FilePart>;
    using PropMap = std::unordered_map<std::string, std::string>;

private :
    FileCoreProperties  fileCoreProperties;         /// Core Properties file
    FileContentType     fileContentType;            /// ContentTypes file
    FileRelationship    filePrincipalRelationship;  /// Relationships file
    PartMap             allFileParts;               /// Set of parts file

    PropMap             extendedCoreProperties;     /// Set of non standard (extended) core properties
    std::string         pathName;                   /// Pathname of package
    unzFile             unzipped = nullptr;
    zipFile             zf = nullptr;
    std::unordered_map<std::string, unz64_file_pos> name2file;

    /**
    * This method allows to write a string content to a non existing part of the package.
    * This method does not create any new FilePart in 'allFileParts' class member.
    */
    void writeStringIntoNewPart(const std::string &input, const std::string & partPath);

    /**
    * @brief makes cache of entries for current opened ZIP.
    * @return 'true' when success, otherwise 'false'.
    */
    bool cacheZipEntries();

public:
    Package() = default;
    Package(const Package&) = delete;
    Package(
        FileCoreProperties pkgFileCP, FileContentType pkgFileCT,
        FileRelationship pkgFileRS, PartMap pkgFileP, std::string pkgPathName
    );

    Package& operator=(const Package&) = delete;

    ~Package();

    bool scan(
        const std::string& pkgPathName,
        size_t chunk_size, bool xml_info,
        PackageScanner scanner
    );
    void openForWriting(const std::string& pkgPathName);
    bool openForReading(const std::string& pkgPathName);
    void close();

    /**
     * @return CoreProperties file.
     */
    const FileCoreProperties& getFileCoreProperties() const;

    /**
     * @return ContentType file.
     */
    const FileContentType& getFileContentType() const;

    /**
     * @return Relationship file.
     */
    const FileRelationship& getPrincipalRelationship() const;

    /**
     * vector of part file.
     */
    const PartMap & getAllFilePart() const;

    /**
     * Get a const string reference on the name of the package.
     */
    const std::string & getPathname() const;

    /**
     * @brief set the CoreProperties file.
     * @param pkgFileCP CoreProperties file.
     */
    void setFileFileCoreProperties(FileCoreProperties pkgFileCP);

    /**
     * Get in read/write access all the non standard core properties of this package
     * All added non standard core properties will be stored in a single part which will be linked to the standard core properties part.
     */
    PropMap& getExtendedCoreProperty() {return extendedCoreProperties;}

    /**
     * @brief add a Property in the CoreProperties file of package.
     * @param pkgTypeProperty    The type of the property to add.
     * @param pkgPropertyValue    The value of the property to add.
     */
    void addProperty(const CoreProperty::TypeProperty & pkgTypeProperty, const std::string & pkgPropertyValue);

    /**
     * @brief set the ContentTypes file.
     * @param pkgFileCT    The ContentType file.
     */
    void setFileContentType(FileContentType pkgFileCT);

    /**
     * @brief add a ContentType in the ContentTypes file of package.
     * @param contentType The ContentType to add.
     */
    void addContentType(ContentType contentType);

    /**
     * @brief set the Relationships file.
     * @param pkgFileRS Relationship file.
     */
    void setPrincipalRelationship(FileRelationship pkgFileRS);

    /**
     * @brief add a Relationship in the Relationships file of package.
     * @param relationship Relationship file.
     */
    void addRelationship(Relationship relationship);

    /**
     * @brief Creates a part in the package and returns it. This part will be automatically destroy with the package.
     * @return a new part.
     */
    FilePart* createPart(const std::string & inputContent, const std::string & outputPartPath);

    /**
     * @brief Find a part corresponding to the given path
     */
    FilePart* findPart(const std::string& outputPartPath);

    /**
     * Check that a given file exists in the zip file
     */
    bool fileExists(const std::string & filename) const;

    /**
     * Extract the content of a given file from the zip file
     */
    std::string extractFile(const std::string & filename, const std::string & password = "");

    void writePackage();
};

} // namespace oes::common::epc

#endif
