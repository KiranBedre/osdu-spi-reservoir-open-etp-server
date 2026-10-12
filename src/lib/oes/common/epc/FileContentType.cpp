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

#include "FileContentType.h"

#include <cstring>
#include <sstream>

#include "rapidxml_ns.hpp"

using std::endl;

namespace oes::common::epc {

const ContentType& FileContentType::getContentType(
    const std::string& extensionOrPartName
) const {
    const char first = *extensionOrPartName.c_str(); // \0 if empty

    auto found = (first == '/')?
        contentTypeMap_.find(extensionOrPartName): // use name as-is
        contentTypeMap_.find('/' + extensionOrPartName) // preprend the missing slash
    ;
    if (found == contentTypeMap_.end()) {
        static const ContentType empty;
        return empty; // not found
    }
    return found->second;
}

const FileContentType::ContentTypeMap& FileContentType::getAllContentType() const {
    return contentTypeMap_;
}

std::string FileContentType::toString() const {
    std::ostringstream oss;
    oss <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        << endl;

    for (const auto& entry : contentTypeMap_) {
        oss << '\t';
        entry.second.print(oss);
        oss << endl;
    }

    oss << "</Types>" << endl;

    return oss.str();
}

void FileContentType::addContentType(ContentType contentType) {
    const std::string& key = contentType.getExtensionOrPartName();
    auto found = contentTypeMap_.find(key);
    if (found == contentTypeMap_.end()) {
        contentTypeMap_.emplace(std::string(key), std::move(contentType));
    } else {
        found->second = std::move(contentType);
    }
}

void FileContentType::readFromString(const std::string& textInput) {
    contentTypeMap_.clear();

    // Default extension
    size_t pos = textInput.find("Default");
    while (pos != std::string::npos)
    {
        size_t end = textInput.find("/>", pos + 7);
        if (end == std::string::npos)
            end = textInput.find("Default>", pos + 7);

        // Extension
        std::string extension;
        size_t attStart = textInput.find("Extension=\"", pos);
        if (attStart != std::string::npos && attStart < end)
        {
            attStart += 11;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos)
                extension = textInput.substr(attStart, attEnd - attStart);
        }

        // Content type
        std::string contentType;
        attStart = textInput.find("ContentType=\"", pos);
        if (attStart != std::string::npos && attStart < end)
        {
            attStart += 13;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos)
                contentType = textInput.substr(attStart, attEnd - attStart);
        }

        addContentType(ContentType(true, std::move(contentType), std::move(extension)));

        pos = textInput.find("Default", end + 1);
    }

    // Override partname
    pos = textInput.find("Override");
    while (pos != std::string::npos)
    {
        size_t end = textInput.find("/>", pos + 8);
        if (end == std::string::npos)
            end = textInput.find("Override>", pos + 8);

        // Extension
        std::string partName;
        size_t attStart = textInput.find("PartName=\"", pos);
        if (attStart != std::string::npos && attStart < end)
        {
            attStart += 10;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos)
                partName = textInput.substr(attStart, attEnd - attStart);
        }

        // Content type
        std::string contentType;
        attStart = textInput.find("ContentType=\"", pos);
        if (attStart != std::string::npos && attStart < end)
        {
            attStart += 13;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos)
                contentType = textInput.substr(attStart, attEnd - attStart);
        }

        addContentType(ContentType(false, std::move(contentType), std::move(partName)));

        pos = textInput.find("Override", end + 1);
    }
}

/*
** Example XML file:
**
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
<Override PartName="/docProps/extendedCore.xml" ContentType="application/x-extended-core-properties+xml"/>
<Override PartName="/obj_EpcExternalPartReference_e5e5230e-ecdd-4605-85ad-446926eeb47f.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_EpcExternalPartReference"/>
<Override PartName="/obj_LocalDepth3dCrs_1c8bb605-c8e7-4da9-af4c-52e5a5e5147b.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs"/>
<Override PartName="/obj_MdDatum_0ee22f34-1049-5b48-b0e9-c5eb4e4f340f.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_MdDatum"/>
<Override PartName="/obj_WellboreFeature_7d9e5e11-7fff-53c3-b645-121655c6922c.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreFeature"/>
<Override PartName="/obj_WellboreInterpretation_ec43b905-4faf-5284-b279-aa7b9b22cd52.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreInterpretation"/>
<Override PartName="/obj_WellboreMarkerFrameRepresentation_c76021a6-c1e7-5b2d-848a-35764da8001d.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreMarkerFrameRepresentation"/>
<Override PartName="/obj_WellboreTrajectoryRepresentation_4a6e1e5a-54f3-589a-b197-6e2ce35699f1.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreTrajectoryRepresentation"/>
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml" />
</Types>
*/
bool FileContentType::readFromXmlInPlace(std::string xml) try { // cppcheck-suppress passedByValue
    rapidxml_ns::xml_document<char> doc;
    doc.parse<rapidxml_ns::parse_default>(xml.data());

    contentTypeMap_.clear();

    const rapidxml_ns::xml_node<char>* root = doc.first_node("Types", 5);
    if (!root) {
        return false;
    }

    const char* uri = root->namespace_uri();
    const size_t uriz = root->namespace_uri_size();
    if (!uri || (uriz != 60)) {
        return false;
    }

    if (0 != std::strncmp(uri, "http://schemas.openxmlformats.org/package/2006/content-types", 60)) {
        return false;
    }

    /*
    ** This requires switch std::unordered_map, but that implies
    ** non-deterministic order for processing parts. Worth it?
    ** Improves perf from 110 MB/s to 120 MB/s, but deterministism matters.
    **
    // Computed based on OSDU's R1 demo EPC
    if (const size_t size_hint = (xml.size() / 175)) {
        contentTypeMap_.reserve(size_hint);
    }
    */

    {
        rapidxml_ns::xml_node<>* elem = root->first_node_ns(uri, uriz);
        for (; elem; elem = elem->next_sibling_ns(uri, uriz)) {
            if (elem->type() != rapidxml_ns::node_element) {
                continue;
            }

            bool is_override = false;

            const char* elem_name = elem->name();
            switch (auto elem_name_len = elem->name_size()) {
            case  7: // Default
                if (0 == std::strncmp(elem_name, "Default", elem_name_len)) {
                    break;
                }
                continue; // unexpected element

            case  8: // Override
                if (0 == std::strncmp(elem_name, "Override", elem_name_len)) {
                    is_override = true;
                    break;
                }
                continue; // unexpected element

            default:
                continue; // unexpected element
            }

            std::string partName, contentType, extension;
            rapidxml_ns::xml_attribute<>* attr = elem->first_attribute();
            for (; attr; attr = attr->next_attribute()) {
                const char* attr_name = attr->name();
                switch (auto attr_name_len = attr->name_size()) {
                case  8: // PartName
                    if (0 == std::strncmp(attr_name, "PartName", attr_name_len)) {
                        partName.assign(attr->value(), attr->value_size());
                    }
                    break;

                case  9: // Extension
                    if (0 == std::strncmp(attr_name, "Extension", attr_name_len)) {
                        extension.assign(attr->value(), attr->value_size());
                    }
                    break;

                case 10:
                    break; // unexpected attribute?

                case 11: // ContentType
                    if (0 == std::strncmp(attr_name, "ContentType", attr_name_len)) {
                        contentType.assign(attr->value(), attr->value_size());
                    }
                    break;

                default:
                    break; // unexpected attribute?
                }
            }

            if (is_override) {
                addContentType(ContentType(false, std::move(contentType), std::move(partName)));
            } else if (!extension.empty()) {
                addContentType(ContentType(true, std::move(contentType), std::move(extension)));
            }
        }
    }

    return true;
} catch (...) {
    return false;
}

bool FileContentType::equals(const FileContentType& rhs) const {
    return contentTypeMap_ == rhs.contentTypeMap_;
}

} // namespace oes::common::epc
