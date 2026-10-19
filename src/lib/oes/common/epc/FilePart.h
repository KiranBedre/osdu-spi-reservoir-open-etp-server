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

#ifndef OES_COMMON_EPC_FILEPART
#define OES_COMMON_EPC_FILEPART

#include "FileRelationship.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace oes::common::epc {

/****************************************************************************/

// TODO: Switch to std::string_view once on EP20 platform
// These structs are meant as callbacks with strings which point
// inside the RapidXML "in-place" buffers, not for by-value storage.

// Warning: structs below correspond to XSD *types*, not necessarily
// XML element names. Notably Hdf5Dataset is used as the (indirect) type
// for variously named elements, like "Values", "Coordinates", etc...
namespace xml {

struct OES_COMMON_EXPORT AbstractCitedDataObject {
    // attributes
    std::string uuid_;              // required

    // Citation element
    std::string Title_;             // required
    std::string Originator_;        // required
    std::string Creation_;          // required
    std::string Format_;            // required
    std::string Editor_;
    std::string LastUpdate_;
    std::string VersionString_;
    std::string Description_;
    std::string DescriptiveKeywords_;

    const std::string& lastChanged() const;
};

struct OES_COMMON_EXPORT EpcExternalPartReference : AbstractCitedDataObject {
    std::string MimeType_;          // required
};

using NameValuePair = std::pair<std::string, std::string>;

struct OES_COMMON_EXPORT DataObjectReference {
    std::string QualifiedType_;     // required
    std::string Title_;             // required
    std::string Uuid_;              // required
    std::string EnergisticsUri_;
    std::string LocatorUrl_;
    std::string ObjectVersion_;

    bool isParam_ = false;
    std::string paramTitle_;

    bool eml_HdfProxy_ = false;
    EpcExternalPartReference EpcExternalPartReference_;
};

struct OES_COMMON_EXPORT Hdf5Dataset {
    std::string PathInHdfFile_;     // required
    DataObjectReference HdfProxy_;  // required
};

struct XmlName {
    std::string xmlns_uri_;
    std::string xmlns_prefix_;
    std::string local_name_;
    std::string qualified_name_;
    std::string xsi_type_;
    std::string schema_version_;
};

using XmlPathGetter = std::function<
    std::string(
        const char* version,
        std::function<const char*(const char* ns_uri, size_t ns_uri_len)>
    )
>;

} // namespace xml

/****************************************************************************/

class OES_COMMON_EXPORT FilePart {
private:
    std::string finalPathName_;            /// The path of the part in the package.
    FileRelationship fileRelationship_;    /// The associated rel file to the part

public:
    /**
     * Constructor
     * @param inputContent        The content which will be included into a part of the package
     * @param outputPartPath        The path of the part within the package.
     */
    explicit FilePart(std::string outputPartPath);

    const std::string& getFinalPathName() const;
    const FileRelationship& getFileRelationship() const;
    const Relationship& getIndexRelationship(int index) const;

    void setFinalPathName(std::string finalPath);

    /**
     * Copy an existing relationship into the relationship set of the part.
     */
    void addRelationship(Relationship relationship);

    /**
     * Creates a new relationship into the relationship set of the part according to the supplied parameters
     */
    void createRelationship(
        std::string rsTarget, std::string rsType, std::string rsId,
        bool internalTarget = true
    );

    static bool rootElementInfo(
        std::string xml, std::string& xmlns_uri, std::string& local_name
    );

    enum ScanRc {
        eOK_DONE = 0,
        eOK_STOPPED,
        eKO_ABORTED, // by client-callback
        eKO_XML_ERROR // internal XML (parser?) error
    };
    static ScanRc scan(
        std::string xml,
        std::function<ScanRc(const xml::XmlName&, const xml::AbstractCitedDataObject&)> on_cited,
        std::function<ScanRc(const xml::NameValuePair&)> on_xdata,
        std::function<ScanRc(const xml::XmlPathGetter&, const xml::DataObjectReference&)> on_reference,
        std::function<ScanRc(const xml::XmlPathGetter&, const xml::Hdf5Dataset&)> on_dataset
    );

    using NamedRef = std::pair<std::string, xml::DataObjectReference>;
    using NamedHdf = std::pair<std::string, xml::Hdf5Dataset>;

    struct ScanData {
        xml::XmlName name_;
        xml::AbstractCitedDataObject cited_;
        std::vector<xml::NameValuePair> xdata_;
        std::vector<NamedRef> refs_;
        std::vector<NamedHdf> hdfs_;
    };
    enum ScanFlag {
        eCited = 1, eXData = 2, eRefs = 4, eHdfs = 8,
    };
    static std::pair<ScanData, ScanRc> scanData(
        std::string xml, int flags = eCited|eXData|eRefs|eHdfs
    );
    static std::string replaceXMLValue(
        const std::string& xml, const std::string& tag, const std::string& oldValue, const std::string& newValue
    );
    static std::string replaceNestedXMLValue(
        const std::string& xml, const std::string& parentTag, const std::string& tag, const std::string& oldValue, const std::string& newValue
    );

    static std::string rewriteDatasetPaths(
        const std::string& xml,
        const std::function<std::optional<std::string>(const xml::Hdf5Dataset&)>& remap
    );
};

/****************************************************************************/

} // namespace oes::common::epc

#endif
