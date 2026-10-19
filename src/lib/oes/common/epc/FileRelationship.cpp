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

#include "FileRelationship.h"

#include <sstream>
#include <cstring>

#include "rapidxml_ns.hpp"

namespace oes::common::epc {

FileRelationship::FileRelationship(
    const Relationship& frRelationship
) : relationships_(&frRelationship, &frRelationship + 1)
{
}

FileRelationship::FileRelationship(
    std::vector<Relationship> frRelationship
) : relationships_(std::move(frRelationship))
{
}

size_t FileRelationship::size() const {
    return relationships_.size();
}

bool FileRelationship::empty() const {
    return relationships_.empty();
}

std::string FileRelationship::getPathName() const {
    return pathName_;
}

const std::vector<Relationship>& FileRelationship::getAllRelationship() const {
    return relationships_;
}

const Relationship& FileRelationship::getIndexRelationship(int index) const {
    return relationships_[index];
}

std::string FileRelationship::toString() const {
    if (empty()) {
        return {};
    }

    std::ostringstream oss;
    print(oss);
    return oss.str();
}

std::ostream& FileRelationship::print(std::ostream& os) const {
    os  << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        << std::endl << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        << std::endl;

    for (const auto& rel : relationships_) {
        os << '\t' << rel.toString() << std::endl;
    }

    return os << "</Relationships>" << std::endl;
}

void FileRelationship::setPathName(std::string frPathName) {
    pathName_ = std::move(frPathName);
}

void FileRelationship::addRelationship(Relationship frRelationship) {
    relationships_.emplace_back(std::move(frRelationship));
}

/**
 * Read a relationships part from a string.
 */
void FileRelationship::readFromString(const std::string& textInput) {
    relationships_.clear();

    size_t pos = textInput.find("Relationship ");
    if (pos == std::string::npos) {
        pos = textInput.find("Relationship>");
    }

    while (pos != std::string::npos) {
        size_t end = textInput.find("/>", pos + 12);
        if (end == std::string::npos) {
            end = textInput.find("Relationship>", pos + 12); // closing tag : </Relationship>
        }

        //ID
        std::string id;
        size_t attStart = textInput.find("Id=\"", pos);
        if (attStart != std::string::npos && attStart < end) {
            attStart += 4;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos) {
                id = textInput.substr(attStart, attEnd - attStart);
            }
        }

        //type
        std::string type;
        attStart = textInput.find("Type=\"", pos);
        if (attStart != std::string::npos && attStart < end) {
            attStart += 6;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos) {
                type = textInput.substr(attStart, attEnd - attStart);
            }
        }

        //Target
        std::string target;
        attStart = textInput.find("Target=\"", pos);
        if (attStart != std::string::npos && attStart < end) {
            attStart += 8;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos) {
                target = textInput.substr(attStart, attEnd - attStart);
            }
        }

        //TargetMode
        bool internalTarget = true;
        attStart = textInput.find("TargetMode=\"", pos);
        if (attStart != std::string::npos && attStart < end) {
            attStart += 12;
            const size_t attEnd = textInput.find('\"', attStart);
            if (attStart != std::string::npos && attEnd != std::string::npos) {
                std::string readString = textInput.substr(attStart, attEnd - attStart);
                if (readString.compare("External") == 0) {
                    internalTarget = false;
                }
            }
        }

        relationships_.emplace_back(
            std::move(target), std::move(type), std::move(id), internalTarget
        );

        pos = textInput.find("Relationship ", end + 1);
        if (pos == std::string::npos) {
            pos = textInput.find("Relationship>", end + 1);
        }
    }
}

bool FileRelationship::readFromXmlInPlace(std::string xml) try { // cppcheck-suppress passedByValue
    rapidxml_ns::xml_document<char> doc;
    doc.parse<rapidxml_ns::parse_default>(xml.data());

    relationships_.clear();

    rapidxml_ns::xml_node<char>* root = doc.first_node("Relationships", 13);
    if (!root) {
        return false;
    }

    const char* uri = root->namespace_uri();
    const size_t uriz = root->namespace_uri_size();
    if (!uri || (uriz != 60)) {
        return false;
    }

    if (0 != std::strncmp(uri, "http://schemas.openxmlformats.org/package/2006/relationships", 60)) {
        return false;
    }

    // Computed via EPC dump of OSDU's R1 demo EPC, and the query below:
    // select sum(rels_count), sum(length(rels)), sum(length(rels))/sum(rels_count) from parts
    // which yields (depending on xml.size()
    // ALL rel files: 588,687 relations, 160,534,871 bytes, 272 bytes/rel
    // >  1,000 rels: 126,014 relations,  32,478,330 bytes, 257 bytes/rel
    // > 10,000 rels:  73,988 relations,  18,768,126 bytes, 253 bytes/rel
    if (const size_t size_hint = (xml.size() / 250)) {
        relationships_.reserve(size_hint);
    }

    {
        rapidxml_ns::xml_node<>* rel = root->first_node_ns(uri, uriz, "Relationship", 12);
        for (; rel; rel = rel->next_sibling_ns(uri, uriz, "Relationship", 12)) {
            bool external = false;
            std::string id, type, target;
            rapidxml_ns::xml_attribute<>* attr = rel->first_attribute();
            for (; attr; attr = attr->next_attribute()) {
                const char* s = attr->name();
                switch (attr->name_size()) {
                case  2: // Id
                    if (s[0] == 'I' && s[1] == 'd') {
                        id.assign(attr->value(), attr->value_size());
                    }
                    break;
                case  4: // Type
                    if (s[0] == 'T' && s[1] == 'y' && s[2] == 'p' && s[3] == 'e') {
                        type.assign(attr->value(), attr->value_size());
                    }
                    break;
                case  6: // Target
                    if (s[0] == 'T' && s[1] == 'a' && s[2] == 'r' && s[3] == 'g' && s[4] == 'e' && s[5] == 't') {
                        target.assign(attr->value(), attr->value_size());
                    }
                    break;
                case 10: // TargetMode
                    if (0 == std::strncmp(s, "TargetMode", 10)) {
                        const char* v = attr->value();
                        if ((attr->value_size() == 8) && (0 == std::strncmp(v, "External", 8))) {
                            external = true;
                        }
                    }
                    break;
                default:
                    break; // unexpected attribute?
                }
            }

            relationships_.emplace_back(
                std::move(target), std::move(type), std::move(id), !external
            );
        }
    }

    return true;
} catch (...) {
    return false;
}

bool FileRelationship::equals(const FileRelationship& rhs) const {
    return
        (pathName_ == rhs.pathName_) &&
        (relationships_ == rhs.relationships_)
    ;
}

} // namespace oes::common::epc
