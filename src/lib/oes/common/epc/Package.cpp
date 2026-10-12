#include "Package.h"

#include <cassert>
#include <sstream>
#include <utility>

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

#include <oes/core/utils/config_enter_3rdparty.h>
#ifdef WIN32
#  include <minizip/iowin32.h>
#endif
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Utf8.h>
#include <oes/core/utils/Logger.h>

using std::string;
using std::vector;
using std::endl;


namespace oes::common::epc {

/*
   Compare two filename (fileName1,fileName2).
   If CASESENSITIVITY = 1, comparision is case sensitivity (like strcmp)
   If CASESENSITIVITY = 2, comparision is not case sensitivity (like strcmpi
                                or strcasecmp)
   If CASESENSITIVITY = 0, case sensitivity is default of your operating system
    (like 1 on Unix, 2 on Windows)
*/
static constexpr int CASESENSITIVITY = 0;

static constexpr char CORE_PROP_REL_TYPE[] = "http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties";
static constexpr char EXTENDED_CORE_PROP_REL_TYPE[] = "http://oes.com/package/2014/relationships/extended-core-properties";


unzFile open_package(const std::string& filePath)
{
#ifdef WIN32
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64W(&ffunc);

    const auto unicodePath = oes::core::utf8::fromUtf8ToWstr(filePath);
    zipFile unzipped = unzOpen2_64(unicodePath.c_str(), &ffunc);
#else
    zipFile unzipped = unzOpen64(filePath.c_str());
#endif

    if (unzipped == nullptr)
    {
        OES_ERROR("Cannot open ZIP for reading: {}", filePath);
    }
    return unzipped;
}

// Converts ZIP time to `std::time_t`, returns -1 if time cannot be represented as a std::time_t object
std::time_t fromZipTime(const tm_unz& tmZip)
{
    tm ctm{};
    ctm.tm_year = tmZip.tm_year - 1900;
    ctm.tm_mon = tmZip.tm_mon;
    ctm.tm_mday = tmZip.tm_mday;
    ctm.tm_hour = tmZip.tm_hour;
    ctm.tm_min = tmZip.tm_min;
    ctm.tm_sec = tmZip.tm_sec;
    return std::mktime(&ctm);
}

// Converts `std::time_t` to ZIP time
tm_zip toZipTime(std::time_t time)
{
    tm ctm{};
    tm_zip zipTm{};

    // Build local time (use platform specific functions for avoid issues with thread-unsafe std::localtime())
#ifdef WIN32
    // Use VS CRT function
    if (localtime_s(&ctm, &time) != 0) {
        return zipTm;
    }
#else
    // Use Unix function (added to C23 standard)
    if (localtime_r(&time, &ctm) == nullptr) {
        return zipTm;
    }
#endif

    zipTm.tm_year = ctm.tm_year + 1900;
    zipTm.tm_mon = ctm.tm_mon;
    zipTm.tm_mday = ctm.tm_mday;
    zipTm.tm_hour = ctm.tm_hour;
    zipTm.tm_min = ctm.tm_min;
    zipTm.tm_sec = ctm.tm_sec;
    return zipTm;
}

string do_extract_currentfile(unzFile uf, const char* password);


/**
 * Create a Package with CoreProperties file, ContentType file, Relationship file, vector of Part file and pathname.
 */
Package::Package(
    FileCoreProperties pkgFileCP, FileContentType pkgFileCT,
    FileRelationship pkgFileRS, PartMap pkgFileP, string pkgPathName
) : fileCoreProperties(std::move(pkgFileCP))
  , fileContentType(std::move(pkgFileCT))
  , filePrincipalRelationship(std::move(pkgFileRS))
  , allFileParts(std::move(pkgFileP))
  , pathName(std::move(pkgPathName))
{
}

/**
 * Destructor : free memory allocated for the parts
 */
Package::~Package() {
    close();
}

/*!
 * \brief Opens the ZIP file and scans the zip entries / table of content.
 *
 * \param pkgPathName the ZIP EPC file to open.
 * \param chunk_size How many entries to report to the \a scanner at a time.
 * \param xml_info whether to parse the ZIP entry as an XML, and get its root element
 * \param scanner Functor to be called repeatedly with groups (chunks) of ZIP entries.
 * \return true on successful scan; false otherwise. The \a scanner can stop / abort the
 *     the scan, by returning either eZIP_SCAN_STOP_OK or eZIP_SCAN_STOP_KO, making this
 *     scan method return true or false, respectively.
 */
bool Package::scan(
    const std::string& pkgPathName,
    size_t chunk_size, bool xml_info,
    PackageScanner scanner
) {
    pathName.assign(pkgPathName);

    unzipped = open_package(pathName);
    if (unzipped == nullptr) {
        return false;
    }

    unz_global_info64 zinfo;
    if (UNZ_OK != unzGetGlobalInfo64(unzipped, &zinfo)) {
        OES_ERROR("Cannot get ZIP entry-count: {}", pathName);
        return false;
    }

    PackageInfo pinfo;
    pinfo.entry_count_ = zinfo.number_entry;
    if (zinfo.size_comment > 0) {
        pinfo.global_comment_.resize(zinfo.size_comment);
        if (0 > unzGetGlobalComment(unzipped, pinfo.global_comment_.data(), zinfo.size_comment)) {
            OES_ERROR("Cannot get ZIP global-comment: {}", pathName);
            return false;
        }
    }

    PackageEntryVec entries;
    if (chunk_size == 0) {
        const ZipScannerRc rc = scanner(pinfo, entries);
        return (rc == eZIP_SCAN_STOP_OK)? true: false;
    }
    entries.reserve(chunk_size);

    unz_file_info64 file_info;
    int err = unzGoToFirstFile(unzipped);

    while (err == UNZ_OK) {
        // Get current file info
        err = unzGetCurrentFileInfo64(unzipped, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
        if (UNZ_OK != err)
        {
            OES_ERROR("Failed to get current file information from ZIP, error code: {}", err);
            return false;
        }

        PackageEntry& entry = entries.emplace_back();

        // Get current file name
        entry.zip_entry_.resize(file_info.size_filename);
        err = unzGetCurrentFileInfo64(unzipped, nullptr, entry.zip_entry_.data(), static_cast<uLong>(entry.zip_entry_.size()), nullptr, 0, nullptr, 0);
        if (UNZ_OK != err)
        {
            OES_ERROR("Failed to get current file name from ZIP, error code: {}", err);
            return false;
        }

        entry.zsize_ = file_info.compressed_size;
        entry.size_ = file_info.uncompressed_size;
        entry.disk_offset_ = unzGetOffset64(unzipped);
        entry.tstamp_ = fromZipTime(file_info.tmu_date);
        entry.crc_ = file_info.crc;

        if (xml_info) {
            std::string xml = do_extract_currentfile(unzipped, nullptr);
            FilePart::rootElementInfo(std::move(xml), entry.xmlns_uri_, entry.xml_ncname_);
        }

        if (entries.size() == chunk_size) {
            const ZipScannerRc rc = scanner(pinfo, std::move(entries));
            if (rc != eZIP_SCAN_CONTINUE) {
                return (rc == eZIP_SCAN_STOP_OK)? true: false;
            }
            entries.clear();
            entries.reserve(chunk_size);
        }

        err = unzGoToNextFile(unzipped);
    }

    if (!entries.empty()) {
        const ZipScannerRc rc = scanner(pinfo, std::move(entries));
        if (rc != eZIP_SCAN_CONTINUE) {
            return (rc == eZIP_SCAN_STOP_OK)? true: false;
        }
    }

    return true;
}

/**
 * Open the package for writing purpose
 */
void Package::openForWriting(const std::string& pkgPathName)
{
    pathName.assign(pkgPathName);

    // Clean the potential ending slashes
    while (pathName.back() == '/' || pathName.back() == '\\')
        pathName.resize(pathName.size()-1);

    ContentType contentTypeRel(true, "application/vnd.openxmlformats-package.relationships+xml", ".rels");
    addContentType(std::move(contentTypeRel));

    ContentType contentTypeCoreProp(false, "application/vnd.openxmlformats-package.core-properties+xml", "docProps/core.xml");
    addContentType(std::move(contentTypeCoreProp));

    Relationship relToCoreProp("docProps/core.xml", CORE_PROP_REL_TYPE, "CoreProperties");
    addRelationship(std::move(relToCoreProp));

#ifdef WIN32
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64W(&ffunc);

    const auto unicodePath = oes::core::utf8::fromUtf8ToWstr(pathName);
    zf = zipOpen2_64(unicodePath.c_str(), 0, NULL, &ffunc);
#else
    zf = zipOpen64(pathName.c_str(), 0);
#endif

    if (zf == nullptr)
    {
        OES_ERROR("Cannot open ZIP for writing: {}", pathName);
    }
}

/**
 * Open the package for reading purpose
 */
bool Package::openForReading(const std::string& pkgPathName) {
    pathName.assign(pkgPathName);

    unzipped = open_package(pathName);
    if (unzipped == nullptr) {
        return false;
    }

    // Speedup part lookup, by caching zipped file positions using file name
    if (!cacheZipEntries())
    {
        OES_ERROR("Unable to cache ZIP file entries: {}", pkgPathName);
        return false;
    }

    // Package relationships : core properties
    {
        string relFile = extractFile("_rels/.rels", "");
        filePrincipalRelationship.readFromXmlInPlace(std::move(relFile));
        const vector<Relationship>& pckRelset = filePrincipalRelationship.getAllRelationship();
        for (const Relationship& rel : pckRelset) {
            if (rel.getType().compare(CORE_PROP_REL_TYPE) == 0) {
                string corePropFile = extractFile(rel.getTarget(), "");
                fileCoreProperties.readFromString(corePropFile);
            }
        }
    }

    // Package relationships : extended core properties
    string extendedCpRelFilePath = extractFile("docProps/_rels/core.xml.rels", "");
    if (!extendedCpRelFilePath.empty())
    {
        FileRelationship extendedCpRelFile;
        extendedCpRelFile.readFromXmlInPlace(std::move(extendedCpRelFilePath));
        const vector<Relationship>& extendedCpRelSet = extendedCpRelFile.getAllRelationship();
        for (const Relationship& rel : extendedCpRelSet) {
            std::string target = rel.getTarget();
            if (target.find('/') == std::string::npos) {
                // The path is assumed to be relative to docProps.
                // There seems to be a RESQML business rule requesting it.
                // But there are plenty of EPC files out there that dont fulfill
                // this rule. We want to be able to read the extended
                // core properties of the latter, as our apps will use the
                // information there to decide if the file can be safely imported.
                target = "docProps/" + target;
            }
            if (!fileExists(target)) {
                continue;
            }
            string extendedCorePropFile = extractFile(target, "");
            std::istringstream iss(extendedCorePropFile);

            std::string line;
            while (std::getline(iss, line))
            {
                if (line[0] == '\t') // To find a better condition
                {
                    size_t start = line.find('<');
                    size_t end = line.find('>');
                    if (start != string::npos && end != string::npos)
                    {
                        size_t end2 = line.find('<', end + 1);
                        if (end2 != string::npos)
                        {
                            extendedCoreProperties.emplace(line.substr(start + 1, end - start - 1), line.substr(end + 1, end2 - end - 1));
                        }
                    }
                }
            }
        }
    }

    // Package content type
    string contentTypeFile = extractFile("[Content_Types].xml", "");
    fileContentType.readFromXmlInPlace(std::move(contentTypeFile));

    return true;
}

/**
 * Close the package
 */
void Package::close() {
    name2file.clear();

    if (unzipped) {
        unzClose(unzipped);
        unzipped = nullptr;
    }
}

const FileCoreProperties& Package::getFileCoreProperties() const
{
    return fileCoreProperties;
}

const FileContentType& Package::getFileContentType() const
{
    return fileContentType;
}

const FileRelationship& Package::getPrincipalRelationship() const
{
    return filePrincipalRelationship;
}

const Package::PartMap& Package::getAllFilePart() const
{
    return allFileParts;
}

const string & Package::getPathname() const
{
    return pathName;
}

void Package::setFileFileCoreProperties(FileCoreProperties pkgFileCP)
{
    fileCoreProperties = std::move(pkgFileCP);
}

void Package::addProperty(const CoreProperty::TypeProperty & pkgTypeProperty, const string & pkgPropertyValue)
{
    switch (pkgTypeProperty)
    {
    case CoreProperty::undefinedCoreProperty:
        break;
    case CoreProperty::category:
        fileCoreProperties.setCategory(pkgPropertyValue);
        break;
    case CoreProperty::contentStatus:
        fileCoreProperties.setContentStatus(pkgPropertyValue);
        break;
    case CoreProperty::created:
        fileCoreProperties.setCreated(pkgPropertyValue);
        break;
    case CoreProperty::creator:
        fileCoreProperties.setCreator(pkgPropertyValue);
        break;
    case CoreProperty::description:
        fileCoreProperties.setDescription(pkgPropertyValue);
        break;
    case CoreProperty::identifier:
        fileCoreProperties.setIdentifier(pkgPropertyValue);
        break;
    case CoreProperty::keywords:
        fileCoreProperties.addKeywords(pkgPropertyValue);
        break;
    case CoreProperty::language:
        fileCoreProperties.setLanguage(pkgPropertyValue);
        break;
    case CoreProperty::lastModifiedBy:
        fileCoreProperties.setLastModifiedBy(pkgPropertyValue);
        break;
    case CoreProperty::lastPrinted:
        fileCoreProperties.setLastPrinted(pkgPropertyValue);
        break;
    case CoreProperty::modified:
        fileCoreProperties.setModified(pkgPropertyValue);
        break;
    case CoreProperty::revision:
        fileCoreProperties.setRevision(pkgPropertyValue);
        break;
    case CoreProperty::subject:
        fileCoreProperties.setSubject(pkgPropertyValue);
        break;
    case CoreProperty::title:
        fileCoreProperties.setTitle(pkgPropertyValue);
        break;
    case CoreProperty::version:
        fileCoreProperties.setVersion(pkgPropertyValue);
        break;
    } // no default on purpose
}

void Package::setFileContentType(FileContentType pkgFileCT)
{
    fileContentType = std::move(pkgFileCT);
}

void Package::addContentType(ContentType contentType)
{
    fileContentType.addContentType(std::move(contentType));
};

void Package::setPrincipalRelationship(FileRelationship pkgFileRS)
{
    filePrincipalRelationship = std::move(pkgFileRS);
}

void Package::addRelationship(Relationship relationship)
{
    filePrincipalRelationship.addRelationship(std::move(relationship));
}

FilePart* Package::createPart(const std::string & inputContent, const std::string & outputPartPath)
{
    const auto result = allFileParts.emplace(outputPartPath, outputPartPath);
    if (!result.second) {
        OES_WARNING("ZIP entry with the same path already exists: {}", outputPartPath);
    }

    writeStringIntoNewPart( inputContent, outputPartPath );
    return &result.first->second;
}

FilePart* Package::findPart(const std::string& outputPartPath)
{
    const auto it = allFileParts.find(outputPartPath);
    return it == allFileParts.end() ? nullptr : &it->second;
}

void Package::writeStringIntoNewPart(const std::string &input, const std::string & partPath)
{
    // Initialize the parameters for the creation of a file in the zip archive
    zip_fileinfo zi;
    zi.dosDate = 0;
    zi.internal_fa = 0;
    zi.external_fa = 0;
    zi.tmz_date = toZipTime(std::time(nullptr));

    // Open the content type part in the zip archive
    int err = zipOpenNewFileInZip64(zf,partPath.c_str(),&zi,
        nullptr, 0, nullptr, 0, nullptr /* comment*/,
        Z_DEFLATED,                        // method
        Z_DEFAULT_COMPRESSION,             // level
        1);                                // Zip64, this MUST be '1' if the uncompressed size is >= 0xffffffff
    if (err != ZIP_OK)
        OES_ERROR("Failed to open zipFile: {}", partPath);

    // Write the content of the content type part
    err = zipWriteInFileInZip(zf, input.c_str(), static_cast<unsigned>(input.size()));
    if (err<0)
    {
        OES_ERROR("Failed to write zipFile: {}", partPath);
    }

    // Close the content type part
    err = zipCloseFileInZip(zf);
    if (err!=ZIP_OK)
        OES_ERROR("Failed to close zipFile: {}", partPath);
}

bool Package::cacheZipEntries()
{
    // Speedup part lookup, by caching zipped file positions using file name
    name2file.clear();

    // pre-size the cache, based on ZIP's entry-count
    unz_global_info64 zinfo;
    if (UNZ_OK != unzGetGlobalInfo64(unzipped, &zinfo)) {
        OES_ERROR("Cannot get entry-count of ZIP {}", pathName);
        return false;
    }
    name2file.reserve(zinfo.number_entry);

    unz_file_info64 file_info;
    std::string current_filename;
    int err = unzGoToFirstFile(unzipped);
    while (err == UNZ_OK)
    {
        // Get current file info
        err = unzGetCurrentFileInfo64(unzipped, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
        if (UNZ_OK != err)
        {
            OES_ERROR("Failed to get current file information from ZIP, error code: {}", err);
            return false;
        }

        // Get current file name
        current_filename.resize(file_info.size_filename);
        err = unzGetCurrentFileInfo64(unzipped, nullptr, current_filename.data(), static_cast<uLong>(current_filename.size()), nullptr, 0, nullptr, 0);
        if (UNZ_OK != err)
        {
            OES_ERROR("Failed to get current file name from ZIP, error code: {}", err);
            return false;
        }

        // Get current file position
        unz64_file_pos filePos;
        if (unzGetFilePos64(unzipped, &filePos) != UNZ_OK)
        {
            OES_ERROR("Failed to get file position from ZIP, error code: {}", current_filename);
            return false;
        }

        name2file.emplace(current_filename, filePos);
        err = unzGoToNextFile(unzipped);
    }

    assert(name2file.size() == zinfo.number_entry);
    return true;
}

void Package::writePackage()
{
    fileCoreProperties.initDefaultCoreProperties();
    writeStringIntoNewPart(fileCoreProperties.toString(), "docProps/core.xml");
    if (!extendedCoreProperties.empty())
    {
        // XML def + namespaces def
        std::ostringstream oss;
        oss << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>" << endl;
        oss << "<extendedCoreProperties xmlns=\"http://schemas.f2i-consulting.com/package/2014/metadata/extended-core-properties\"" << endl;
        oss << "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">" << endl;

        // content

        for (const auto& entry : extendedCoreProperties) {
            oss << "\t<" << entry.first << ">" + entry.second + "</" + entry.first + ">" << endl;
        }

        // end tag
        oss << "</extendedCoreProperties>" << endl;

        writeStringIntoNewPart(oss.str(), "docProps/extendedCore.xml");

        // Add the content type for extended core properties part
        ContentType contentTypeExtendedCoreProp(false, "application/x-extended-core-properties+xml", "docProps/extendedCore.xml");
        addContentType(std::move(contentTypeExtendedCoreProp));

        // Relationship with the standard core properties part.
        // Apparently path expected to be relative to docProps/, not the "root" of the EPC,
        // so not "docProps/extendedCore.xml" anymore, just "extendedCore.xml" (see SHARED-5973)
        Relationship relToExtCoreProp("extendedCore.xml", EXTENDED_CORE_PROP_REL_TYPE, "ExtendedCoreProperties");
        FileRelationship fileExtCorePropRep(relToExtCoreProp);
        fileExtCorePropRep.setPathName("docProps/_rels/core.xml.rels");
        writeStringIntoNewPart(fileExtCorePropRep.toString(), fileExtCorePropRep.getPathName());
    }

    writeStringIntoNewPart(fileContentType.toString(), "[Content_Types].xml");

    // write Principal Relationships file.
    if (!filePrincipalRelationship.isEmpty())
        writeStringIntoNewPart(filePrincipalRelationship.toString(), "_rels/.rels");

    // Write all the part relationships
    for (const auto& entry : allFileParts) {
        const FilePart& part = entry.second;
        if (!part.getFileRelationship().isEmpty()) {
            writeStringIntoNewPart(
                part.getFileRelationship().toString(),
                part.getFileRelationship().getPathName()
            );
        }
    }

    // Close the zip archive
    const int err = zipClose(zf, nullptr);
    zf = nullptr;
    if (err != ZIP_OK)
        OES_ERROR("Failed to close zipFile: {}", pathName.substr(0, pathName.size() - 4));
}

string do_extract_currentfile(unzFile uf, const char* password)
{
    std::string data;

    int err = unzOpenCurrentFilePassword(uf, password);
    if (err == UNZ_OK)
    {
        // Get current file info
        unz_file_info64 file_info;
        err = unzGetCurrentFileInfo64(uf, &file_info, nullptr, 0, nullptr, 0, nullptr, 0);
        if (err == UNZ_OK)
        {
            // Extract file
            data.resize(file_info.uncompressed_size);
            err = unzReadCurrentFile(uf, data.data(), static_cast<unsigned>(data.size()));
            if (err < 0)
            {
                data.clear();
                OES_ERROR("Failed to extract file from ZIP, error code: {}", err);
            }
        }
        else
        {
            OES_ERROR("Failed to get current file information from ZIP, error code: {}", err);
        }

        // Close file
        err = unzCloseCurrentFile(uf);
        if (err != UNZ_OK)
        {
            OES_ERROR("Failed to close current file in ZIP, error code: {}", err);
        }
    }
    else
    {
        OES_ERROR("Failed to open current file in ZIP, error code: {}", err);
    }

    return data;
}

bool Package::fileExists(const string & filename) const
{
    if (unzipped == nullptr){
        return false;
    }

    // Find in cache
    const auto it = name2file.find(filename);
    if (it != name2file.end())
    {
        return true;
    }

    return unzLocateFile(unzipped,filename.c_str(),CASESENSITIVITY) == UNZ_OK;
}

string Package::extractFile(const string & filename, const string & password) {
    if (unzipped == nullptr)
        return "";

    auto it = name2file.find(filename);
    if (it == name2file.end()) {
        if (unzLocateFile(unzipped,filename.c_str(),CASESENSITIVITY) != UNZ_OK) {
            return "";
        }

        unz64_file_pos filePos;
        if (unzGetFilePos64(unzipped, &filePos) != UNZ_OK)
        {
            return "";
        }

        name2file.emplace(filename, filePos);
    } else {
        if (unzGoToFilePos64(unzipped, &it->second) != UNZ_OK)
        {
            return "";
        }
    }

    const char* passwd = password.empty()? nullptr: password.c_str();
    return do_extract_currentfile(unzipped, passwd);
}

} // namespace oes::common::epc
