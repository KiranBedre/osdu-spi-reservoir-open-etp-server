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

#include "FilePart.h"
#include "ContentType.h"

#include "rapidxml_ns.hpp"
#include <sstream>
#include <regex>
#include <stdexcept>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/config.hpp>
#include <boost/algorithm/string/erase.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace { // RapidXml utilities for EML/RESQML

using namespace oes::common::epc;
using namespace oes::common::epc::xml;
const char RSQ[50] = "http://www.energistics.org/energyml/data/resqmlv2";
const char EML[50] = "http://www.energistics.org/energyml/data/commonv2";
const char WIT[50] = "http://www.energistics.org/energyml/data/witsmlv2";
const char PRD[50] = "http://www.energistics.org/energyml/data/prodmlv2";

bool equalsStr(const char* lhs, size_t llen, const char* rhs, size_t rlen) {
    return rapidxml_ns::internal::compare(lhs, llen, rhs, rlen);
}

template <size_t N>
bool is_namespace_uri(const rapidxml_ns::xml_node<char>* node, const char(&uri)[N]) {
    if( node == nullptr){
        return false;
    }
    const size_t len = (N - 1);
    if (node->namespace_uri_size() == len) {
        return equalsStr(uri, len, node->namespace_uri(), len);
    }
    return false;
}

bool is_eml(const rapidxml_ns::xml_node<char>* node) {
    return is_namespace_uri(node, EML);
}

bool is_resqml(const rapidxml_ns::xml_node<char>* node) {
    return is_namespace_uri(node, RSQ);
}

bool is_witsml(const rapidxml_ns::xml_node<char>* node) {
    return is_namespace_uri(node, WIT);
}

bool is_prodml(const rapidxml_ns::xml_node<char>* node) {
    return is_namespace_uri(node, PRD);
}

template <size_t N>
bool is_local_name(const rapidxml_ns::xml_node<char>* node, const char(&lit)[N]) {
    if(node==nullptr){
        return false;
    }
    const size_t len = (N - 1);
    if (node->local_name_size() == len) {
        return equalsStr(lit, len, node->local_name(), len);
    }
    return false;
}

auto next_eml_sibling(const rapidxml_ns::xml_node<char>* node) {
    while (node = node->next_sibling(), node) {
        if (is_eml(node)) {
            return node;
        }
    }
    return node;
}

auto next_non_eml_sibling(const rapidxml_ns::xml_node<char>* node) {
    while (node = node->next_sibling(), node) {
        if (!is_eml(node)) {
            return node;
        }
    }
    return node;
}

std::string text_data(const rapidxml_ns::xml_node<char>* node) {
    std::string text;
    rapidxml_ns::xml_node<>* child = node->first_node();
    if (child && (child->type() == rapidxml_ns::node_data) && !child->next_sibling()) {
        // ideal case, a single text node. std::string_view is OK in that case
        const char* s = child->value();
        return std::string(s, s + child->value_size());
    }
    // Hmmm, more than one node. std::string_view needs "backing"...
    for (; child; child = child->next_sibling()) {
        switch (child->type()) {
        case rapidxml_ns::node_data:
        case rapidxml_ns::node_cdata:
            if (size_t n = child->value_size()) {
                assert(text.empty());
                text.insert(text.end(), child->value(), child->value() + n);
            }
            break;
        default:
            break;
        }
    }
    return text;
}

void init_citation(
    AbstractCitedDataObject& cited, const rapidxml_ns::xml_node<char>* elem
) {
    // This is a <sequence>, child elements must be in a given
    // order, thus we do not need a map lookup, thus is faster
    int seq_idx = 0;
    const rapidxml_ns::xml_node<>* subelem = elem->first_node();
    for (; subelem; subelem = next_eml_sibling(subelem), ++seq_idx) {
        switch (seq_idx) {
        case 0:
            if (is_local_name(subelem, "Title")) {
                cited.Title_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 1:
            if (is_local_name(subelem, "Originator")) {
                cited.Originator_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 2:
            if (is_local_name(subelem, "Creation")) {
                cited.Creation_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 3:
            if (is_local_name(subelem, "Format")) {
                cited.Format_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 4:
            if (is_local_name(subelem, "Editor")) {
                cited.Editor_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 5:
            if (is_local_name(subelem, "LastUpdate")) {
                cited.LastUpdate_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 6:
            if (is_local_name(subelem, "VersionString")) {
                cited.VersionString_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 7:
            if (is_local_name(subelem, "Description")) {
                cited.Description_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 8:
            if (is_local_name(subelem, "DescriptiveKeywords")) {
                cited.DescriptiveKeywords_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        default:
            break; // not normal
        }
    }
}

void init_xdata(
    NameValuePair& xdata, const rapidxml_ns::xml_node<char>* elem
) {
    // This is a <sequence>, child elements must be in a given
    // order, thus we do not need a map lookup, thus is faster
    int seq_idx = 0;
    const rapidxml_ns::xml_node<>* subelem = elem->first_node();
    for (; subelem; subelem = next_non_eml_sibling(subelem), ++seq_idx) {
        switch (seq_idx) {
        case 0:
            if (is_local_name(subelem, "Name")) {
                xdata.first = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 1:
            if (is_local_name(subelem, "Value")) {
                xdata.second = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        default:
            break; // not normal
        }
    }
}

/*
 * Fill reference from EML 2.0 element
*/
void init_reference_within(
    DataObjectReference& ref, const rapidxml_ns::xml_node<char>* subelem
) {
    // Warning: we are already *inside* a DataObjectReference-typed element,
    // but we do not know the name of that outer element (nor care)
    assert(is_local_name(subelem, "ContentType"));
    std::string ml, format, version, type;
    const bool parse_ok = ContentType::parse(
        text_data(subelem), ml, format, version, type
    );
    if(parse_ok && version.length()>2){
        ref.QualifiedType_ = fmt::format("{}{}{}.{}",ml, version[0],version[2], type);
    }

    int seq_idx = 0;
    subelem = next_eml_sibling(subelem); // NOT first_node()! See above
    for (; subelem; subelem = next_eml_sibling(subelem), ++seq_idx) {
        switch (seq_idx) {
        case 0:
            if (is_local_name(subelem, "Title")) {
                ref.Title_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 1:
            if (is_local_name(subelem, "UUID")) {
                ref.Uuid_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 2:
            if (is_local_name(subelem, "UuidAuthority")) {
                ref.LocatorUrl_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 3:
            if (is_local_name(subelem, "VersionString")) {
                ref.ObjectVersion_ = text_data(subelem);
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        case 4:
            if (is_local_name(subelem, "EpcExternalPartReference")) {
                // TODO
                break;
            }
            ++seq_idx;
            [[fallthrough]];
        default:
            break; // not normal
        }
    }
}

/*
 * Fill reference from EML 2.3 element
*/
void init_reference_within23(
    DataObjectReference& ref, const rapidxml_ns::xml_node<char>* elem
) {
    if( elem == nullptr){
        return;
    }
    int seq_idx = 0;
    const rapidxml_ns::xml_node<>* subelem = elem->first_node();
    for (; subelem; subelem = next_eml_sibling(subelem), ++seq_idx) {
        if (is_local_name(subelem, "Uuid")) {
            ref.Uuid_ = text_data(subelem);
        } else if (is_local_name(subelem, "ObjectVersion")) {
            ref.ObjectVersion_ = text_data(subelem);
        } else if (is_local_name(subelem, "QualifiedType")) {
            ref.QualifiedType_ = text_data(subelem);
        } else if (is_local_name(subelem, "Title")) {
            ref.Title_ = text_data(subelem);
        } else if (is_local_name(subelem, "EnergisticsUri")) {
            ref.EnergisticsUri_ = text_data(subelem);
        } else if (is_local_name(subelem, "LocatorUrl")) {
            ref.LocatorUrl_ = text_data(subelem);
        }
    }
}

void init_reference(
    DataObjectReference& ref, const rapidxml_ns::xml_node<char>* elem
) {
    if( elem == nullptr){
        return ;
    }
    // This is a <sequence>, child elements must start with ContentType
    rapidxml_ns::xml_node<>* subelem = elem->first_node();
    if (is_eml(subelem) && is_local_name(subelem, "ContentType")) {
        init_reference_within(ref, subelem);
    }else if (is_eml(subelem) && is_local_name(subelem, "EnergisticsUri")) {
        init_reference_within23(ref, elem);
    }
}

/*
 * Fill dataset info from EML 2.0 element
*/
bool init_dataset_within(
    Hdf5Dataset& hdf, const rapidxml_ns::xml_node<char>* subelem
) {
    if( subelem == nullptr){
        return false;
    }
    // Warning: we are already *inside* a Hdf5Dataset-typed element,
    // but we do not know the name of that outer element (nor care)
    assert(is_local_name(subelem, "PathInHdfFile"));
    hdf.PathInHdfFile_ = text_data(subelem);

    subelem = next_eml_sibling(subelem);
    if (!is_local_name(subelem, "HdfProxy")) {
        return false;
    }
    assert(!next_eml_sibling(subelem)); // XML/XSD "validation"

    init_reference(hdf.HdfProxy_, subelem);
    hdf.HdfProxy_.eml_HdfProxy_ = true;
    hdf.HdfProxy_.LocatorUrl_ = "application/x-hdf5";
    return true;
}

/*
 * Fill dataset info from EML 2.3 element
*/
bool init_dataset_within23(
    Hdf5Dataset& hdf, const rapidxml_ns::xml_node<char>* subelem
) {
    if( subelem == nullptr){
        return false;
    }
    // Warning: we are already *inside* a Hdf5Dataset-typed element,
    // but we do not know the name of that outer element (nor care)
    assert(is_local_name(subelem, "PathInExternalFile"));
    hdf.PathInHdfFile_ = text_data(subelem);

    subelem = next_eml_sibling(subelem);
    while(subelem && is_local_name(subelem, "StartIndex")) {
        subelem = next_eml_sibling(subelem);
    }

    if (!is_local_name(subelem, "URI")) {
        return false;
    }
    hdf.HdfProxy_.EnergisticsUri_ = text_data(subelem);

    subelem = next_eml_sibling(subelem);
    if(!subelem){
        return false;
    }
    if(subelem && is_local_name(subelem, "MimeType")){
        hdf.HdfProxy_.LocatorUrl_ = text_data(subelem);
    }

    assert(!next_eml_sibling(subelem)); // XML/XSD "validation"

    hdf.HdfProxy_.eml_HdfProxy_ = false;
    return true;
}

using XNode = rapidxml_ns::xml_node<char>;
using XNodes = std::vector<const XNode*>;

std::string stackToXPath(
    const XNodes& stack,
    const char* version,
    std::function<const char*(const char* ns_uri, size_t ns_uri_len)> uri_mapper
) {
    bool first = true;
    assert(!stack.empty());
    std::ostringstream oss;

    const auto* root = stack.front();
    rapidxml_ns::xml_attribute<>* attr = root->first_attribute();
    std::string schema_version; 
    for (; attr; attr = attr->next_attribute()) {
        const char*const s = attr->name();
        size_t n = attr->name_size();
        if (equalsStr(s, n, "schemaVersion", n)) {
            schema_version.assign(attr->value(), attr->value_size());
            break;
        }
    }

    bool is_eml20 = (0 == std::strncmp(version,"2.0",3));

    const auto* last_node = stack.back();
    for (const auto* node : stack) {
        if (node == last_node) {
            break; // because we are already *within* the node we care about
        }

        if (first) {
            first = false;
        } else {
            oss << '/';
        }

        if (!node->namespace_uri()) {
            // no namespace, no prefix
        } else if (is_eml(node)) {
            oss << (is_eml20 ? "eml" : "eml23") << ':';
        } else if (is_resqml(node)) {
            oss << (is_eml20 ? "rsq" : "rsq22") << ':';
        } else if (is_witsml(node)) {
            oss << "wit21" << ':';
        } else if (is_prodml(node)) {
            oss << "prd22" << ':';
        } else {
            if (uri_mapper) {
                if (const char* mapper_prefix = uri_mapper(node->namespace_uri(), node->namespace_uri_size())) {
                    oss << mapper_prefix << ':';
                } else if (const char* node_prefix = node->prefix()) {
                    oss.write(node_prefix, node->prefix_size()) << ':';
                } else {
                    // default namespace. Write the full-namespace
                    oss.write(node->namespace_uri(), node->namespace_uri_size()) << ':';
                }
            } else if (const char* node_prefix = node->prefix()) {
                oss.write(node_prefix, node->prefix_size()) << ':';
            } else {
                // default namespace. Write the full-namespace
                oss.write(node->namespace_uri(), node->namespace_uri_size()) << ':';
            }
        }
        oss.write(node->local_name(), node->local_name_size());
    }
    return oss.str();
}

template <typename Func>
FilePart::ScanRc do_recurse_depth_first(XNodes& stack, Func& functor) {
    assert(!stack.empty());
    auto* node = stack.back();
    switch (node->type()) {
    case rapidxml_ns::node_element:
        for (auto child = node->first_node(); child; child = child->next_sibling()) {
            stack.emplace_back(child);
            switch (auto rc = functor(stack)) {
            case FilePart::eOK_DONE: break; // continue recursing
            case FilePart::eOK_STOPPED:
                // function "consumed" the siblings (or ignores them),
                // and is telling us to resume on the parent's siblings,
                // abandoning this parent's subtree.
                stack.pop_back();
                return FilePart::eOK_DONE;
            default:
                return rc; // unwind
            }
            if (auto rc = do_recurse_depth_first(stack, functor)) {
                return rc;
            }
            stack.pop_back();
        }
        break;
    default:
        break;
    }
    return FilePart::eOK_DONE;
}

template <typename Func>
FilePart::ScanRc recurse_depth_first(const XNode* node, Func functor) {
    XNodes stack{ node };
    return do_recurse_depth_first(stack, functor);
}

} // namespace

namespace oes::common::epc {

namespace xml {

//! \return LastUpdate_ if defined, Creation_ otherwise
const std::string& AbstractCitedDataObject::lastChanged() const {
    return !LastUpdate_.empty()? LastUpdate_: Creation_;
}

} // namespace xml

FilePart::FilePart(
    std::string outputPartPath
) {
    setFinalPathName(std::move(outputPartPath));
}

const std::string& FilePart::getFinalPathName() const {
    return finalPathName_;
}

const FileRelationship& FilePart::getFileRelationship() const {
    return fileRelationship_;
}

const Relationship& FilePart::getIndexRelationship(int index) const{
    return fileRelationship_.getIndexRelationship(index);
}

void FilePart::setFinalPathName(std::string finalPath) {
    finalPathName_ = std::move(finalPath);

    std::string relPath(finalPathName_);

    // Clean the potential ending slashes of relPath
    while (
        relPath.back() == '/' ||
        relPath.back() == '\\'
    ) {
        relPath.resize(relPath.size() - 1);
    }

    // extract the directory path
    size_t slashPos = relPath.find_last_of("/\\");
    if (slashPos == std::string::npos) {
        relPath.clear();
    } else {
        relPath.resize(slashPos + 1); // keep the slash!!!
    }

    // Create the path for the associated relationship part
    relPath += "_rels/"; // Note: not "/_rels/" !!!
    relPath += finalPathName_;
    relPath += ".rels";

    fileRelationship_.setPathName(std::move(relPath));
}

void FilePart::createRelationship(
    std::string rsTarget, std::string rsType, std::string rsId, bool internalTarget
) {
    fileRelationship_.addRelationship(Relationship(
        std::move(rsTarget), std::move(rsType), std::move(rsId), internalTarget
    ));
}

void FilePart::addRelationship(Relationship relationship) {
    fileRelationship_.addRelationship(std::move(relationship));
}

bool FilePart::rootElementInfo(
    std::string xml, std::string& xmlns_uri, std::string& local_name // cppcheck-suppress passedByValue
) try {
    rapidxml_ns::xml_document<char> doc;
    doc.parse<rapidxml_ns::parse_default>(xml.data());

    rapidxml_ns::xml_node<char>* root = doc.first_node();
    if (!root) {
        return false;
    }

    const char* uri = root->namespace_uri();
    const size_t uriz = root->namespace_uri_size();
    xmlns_uri.assign(uri, uriz);

    const char* ncn = root->local_name();
    const size_t ncnz = root->local_name_size();
    local_name.assign(ncn, ncnz);

    return true;
} catch (...) {
    return false;
}

FilePart::ScanRc FilePart::scan(
    std::string xml, // cppcheck-suppress passedByValue
    std::function<ScanRc(const xml::XmlName&, const xml::AbstractCitedDataObject&)> on_cited,
    std::function<ScanRc(const xml::NameValuePair&)> on_xdata,
    std::function<ScanRc(const xml::XmlPathGetter&, const xml::DataObjectReference&)> on_reference,
    std::function<ScanRc(const xml::XmlPathGetter&, const xml::Hdf5Dataset&)> on_dataset
) try {
    rapidxml_ns::xml_document<char> doc;
    doc.parse<rapidxml_ns::parse_default>(xml.data());

    rapidxml_ns::xml_node<char>* root = doc.first_node();
    if (!root) {
        return eKO_XML_ERROR;
    }

    std::vector<xml::XmlName> path;
    const bool do_path = (on_reference || on_dataset);

    auto pushNode = [&path, do_path, root](rapidxml_ns::xml_node<char>* node) {
        if (do_path || (node == root)) {
            path.emplace_back();
            xml::XmlName& name = path.back();
            name.xmlns_uri_.assign(node->namespace_uri(), node->namespace_uri_size());
            name.xmlns_prefix_.assign(node->prefix(), node->prefix_size());
            name.local_name_.assign(node->local_name(), node->local_name_size());
            name.qualified_name_.assign(node->name(), node->name_size());
        }
    };
    auto popNode = [&path, do_path]() {
        if (do_path) {
            path.pop_back();
        }
    };

    pushNode(root);

    xml::AbstractCitedDataObject cited;

    if (on_cited) {
        rapidxml_ns::xml_attribute<>* attr = root->first_attribute();
        for (; attr; attr = attr->next_attribute()) {
            const char*const s = attr->name();
            switch (size_t n = attr->name_size()) {
            case  4:
                if (s[0] == 'u' && s[1] == 'u' && s[2] == 'i' && s[3] == 'd') {
                    cited.uuid_.assign(attr->value(), attr->value_size());
                }
                break;
            case 13:
                if (equalsStr(s, n, "schemaVersion", n)) {
                    path.back().schema_version_.assign(attr->value(), attr->value_size());
                }
                break;
            case 8:
                if (equalsStr(s, n, "xsi:type", n)) {
                    path.back().xsi_type_.assign(attr->value(), attr->value_size());
                }
                break;
            }
        }
    }

    {
        rapidxml_ns::xml_node<>* elem = root->first_node();
        for (; elem; elem = elem->next_sibling(), popNode()) {
            pushNode(elem);
            if (elem->type() != rapidxml_ns::node_element) {
                continue;
            }
            if (is_eml(elem) && is_local_name(elem, "Citation")) {
                if (!on_cited) {
                    continue;
                }
                init_citation(cited, elem);
                if (auto rc = on_cited(path.front(), cited)) {
                    return rc;
                }
            } else {
                if (
                    is_local_name(elem, "ExtensionNameValue") || 
                    is_local_name(elem, "ExtraMetadata")
                ) {
                    if (!on_xdata) {
                        continue;
                    }
                    xml::NameValuePair xdata;
                    init_xdata(xdata, elem);
                    if (auto rc = on_xdata(xdata)) {
                        return rc;
                    }
                } else {
                    if (!do_path) {
                        // no need to explore subtrees in not
                        // interested in references or datasets
                        continue;
                    }
                    // Some other RESQML top-level element
                    // (i.e. directly inside the root element)
                    // Recurse depth-first to find references and datasets
                    std::string PathInHdfFile;
                    auto recurse_rc = recurse_depth_first(
                        elem,
                        [&on_reference, &on_dataset](XNodes& stack) {
                            auto* descendant = stack.back();
                            if (!is_eml(descendant)) { // e.g. RESQML
                                return eOK_DONE; // continue recursing down
                            }

                            if (is_local_name(descendant, "PathInHdfFile")) {
                                if (on_dataset) { // Eml 2.0 HDF
                                    xml::Hdf5Dataset hdf;
                                    init_dataset_within(hdf, descendant);
                                    auto stack_getter = [&stack](
                                        const char*,
                                        std::function<const char*(const char* ns_uri, size_t ns_uri_len)> uri_mapper
                                    ) {
                                        return stackToXPath(
                                            stack, "2.0", uri_mapper
                                        );
                                    };
                                    if (auto rc = on_dataset(stack_getter, hdf)) {
                                        return rc;
                                    }
                                } else if (on_reference) {
                                    // that way HdfProxy shows up as a reference
                                    return eOK_DONE; // continue recursing down
                                }
                                return eOK_STOPPED; // parent subtree consumed (or ignored)
                            } else if (is_local_name(descendant, "PathInExternalFile")) {
                                if (on_dataset) { // Eml 2.3 HDF and others external
                                    xml::Hdf5Dataset hdf;
                                    init_dataset_within23(hdf, descendant);
                                    auto stack_getter = [&stack](
                                        const char*,
                                        std::function<const char*(const char* ns_uri, size_t ns_uri_len)> uri_mapper
                                    ) {
                                        return stackToXPath(
                                            stack, "2.3", uri_mapper
                                        );
                                    };
                                    if (auto rc = on_dataset(stack_getter, hdf)) {
                                        return rc;
                                    }
                                } else if (on_reference) {
                                    // that way HdfProxy shows up as a reference
                                    return eOK_DONE; // continue recursing down
                                }
                                return eOK_STOPPED; // parent subtree consumed (or ignored)
                            } else if (is_local_name(descendant, "ContentType")) {
                                if (on_reference) { // EML 2.0 data object reference
                                    xml::DataObjectReference ref;
                                    init_reference_within(ref, descendant);
                                    auto stack_getter = [&stack](
                                        const char*,
                                        std::function<const char*(const char* ns_uri, size_t ns_uri_len)> uri_mapper
                                    ) {
                                        return stackToXPath(
                                            stack, "2.0", uri_mapper
                                        );
                                    };

                                    ref.paramTitle_ = "" ;
                                    const auto parent = stack.size()>2 ? stack[stack.size()-3] : nullptr;
                                    if( parent && is_local_name(parent, "Parameter")) {
                                        for (auto child = parent->first_node(); child; child = child->next_sibling()) {
                                            if(is_local_name(child, "Title")){
                                                ref.isParam_ = true ;
                                                ref.paramTitle_ = text_data(child);
                                                break;
                                            }
                                        }
                                    }
                                    if (auto rc = on_reference(stack_getter, ref)) {
                                        return rc;
                                    }
                                }
                                return eOK_STOPPED; // parent subtree consumed (or ignored)
                            }else if(is_local_name(descendant, "QualifiedType")){
                                if (on_reference) { // EML 2.3 data object reference
                                    const auto parent = stack[stack.size()-2];
                                    // Differentiate between DataObjectReference (Uuid) and ComponentReference (Uid)
                                    bool hasUuid = false;
                                    for (auto child = parent->first_node(); child; child = child->next_sibling()) {
                                        if(is_local_name(child, "Uuid")){
                                            hasUuid = true;
                                            break;
                                        }
                                    }
                                    if(hasUuid){
                                        xml::DataObjectReference ref;
                                        init_reference_within23(ref, parent);
                                        auto stack_getter = [&stack](
                                            const char* ,
                                            std::function<const char*(const char* ns_uri, size_t ns_uri_len)> uri_mapper
                                        ) {
                                                return stackToXPath(
                                                    stack, "2.3", uri_mapper
                                                );
                                        };
                                        if (auto rc = on_reference(stack_getter, ref)) {
                                            return rc;
                                        }
                                    }
                                }
                                return eOK_STOPPED; // parent subtree consumed (or ignored)
                            }

                            return eOK_DONE; // continue recursing down
                        }
                    );
                    if (recurse_rc) {
                        return recurse_rc;
                    }
                }
            }
        }
    }

    return eOK_DONE;
} catch (...) {
    return eKO_XML_ERROR;
}

/*!
 * \brief Convenience method around scan().
 * \param xml the XML to parse
 * \param flags which parts to record, each flag corresponding to a scan() callback.
 * \return the recorded scan data, with scan()'s return code.
 */
auto FilePart::scanData(
    std::string xml, int flags
) -> std::pair<ScanData, ScanRc>
{
    assert(flags & (eCited|eXData|eRefs|eHdfs)); // i.e. scanning something

    ScanData scanned;

    std::function<epc::FilePart::ScanRc(const XmlName&, const AbstractCitedDataObject&)> on_cited;
    std::function<epc::FilePart::ScanRc(const NameValuePair&)> on_xdata;
    std::function<epc::FilePart::ScanRc(const XmlPathGetter&, const DataObjectReference&)> on_refs;
    std::function<epc::FilePart::ScanRc(const XmlPathGetter&, const Hdf5Dataset&)> on_hdfs;

    if (flags & eCited) {
        on_cited = [&scanned](const XmlName& name, const AbstractCitedDataObject& cited) {
            scanned.name_ = name;
            scanned.cited_ = cited;
            return epc::FilePart::eOK_DONE;
        };
    }

    if (flags & eXData) {
        on_xdata = [&scanned](const NameValuePair& nvp) {
            scanned.xdata_.emplace_back(nvp);
            return epc::FilePart::eOK_DONE;
        };
    }

    if (flags & eRefs) {
        on_refs = [&scanned](const XmlPathGetter& xpg, const DataObjectReference& ref) {
            // "a48c9c25-1e3a-43c8-be6a-044224cc69cb" is the well know uid for base property type
            if(ref.Uuid_!="a48c9c25-1e3a-43c8-be6a-044224cc69cb"){
                scanned.refs_.emplace_back(xpg("2.0", nullptr), ref);
            }
            return epc::FilePart::eOK_DONE;
        };
    }

    if (flags & eHdfs) {
        on_hdfs = [&scanned](const XmlPathGetter& xpg, const Hdf5Dataset& hdf) {
            scanned.hdfs_.emplace_back(xpg("2.0", nullptr), hdf);
            return epc::FilePart::eOK_DONE;
        };
    }

    epc::FilePart::ScanRc rc = epc::FilePart::scan(
        std::move(xml), on_cited, on_xdata, on_refs, on_hdfs
    );

    return { scanned, rc };
}

/*!
 * \brief Convenience method for XML value replacement.
 * \param xml the XML to parse
 * \param tag the tag of the value to replace. Tags can be prefixed by namespaces, only tags without namespace is needed.
 * \param oldValue the old value to replace, the old value can be part of a regex pattern
 * \param newValue the new value
 * \return XML with the replaced value.
 */
std::string FilePart::replaceXMLValue(
    const std::string& xml, const std::string& tag, const std::string& oldValue, const std::string& newValue
) {
    std::regex pattern("<([a-zA-Z0-9]*:)?"+tag+"( ?[a-zA-Z0-9:=\"]*)>("+oldValue+")</([a-zA-Z0-9]*:)?"+tag+">");
    
    // Find the beginning and and of the block macthing the pattern
    std::smatch match;
    std::string result = xml;
    if(std::regex_search(result, match, pattern)) {
        std::string block = match[0];
        std::string newBlock = "<"+match[1].str()+tag+match[2].str()+">"+newValue+"</"+match[4].str()+tag+">";
        result = std::regex_replace(result, pattern, newBlock);
    }
    return result;
}

/*!
 * \brief Convenience method for nested XML value replacement. It replaces the value of a tag inside a parent tag.
 * Safer that using a simple tag since collision less likely to occurs
 * \param xml the XML to parse
 * \param parentTag the parent tag for the tag of the value to replace. Tags can be prefixed by namespaces, only tags without namespace is needed.
 * \param oldValue the old value to replace, the old value can be part of a regex pattern
 * \param tag the tag of the value to replace. Tags can be prefixed by namespaces, only tags without namespace is needed.
 * \param newValue the new value
 * \return XML with the replaced value.
 */
std::string FilePart::replaceNestedXMLValue(
    const std::string& xml, const std::string& parentTag, const std::string& tag, const std::string& oldValue, const std::string& newValue
) {
    std::regex parentPattern("<([a-zA-Z0-9]*:)?"+parentTag+"( ?[a-zA-Z0-9:=\"]*)>([\\s\\S]*)</([a-zA-Z0-9]*:)?"+parentTag+">");
    std::smatch parentMatch;
    std::string result;
    std::string partial = xml;

    while (std::regex_search(partial, parentMatch, parentPattern)) {
        std::string newParentBlock = "<"+parentMatch[1].str()+parentTag+parentMatch[2].str()+">"+replaceXMLValue(parentMatch[3].str(), tag, oldValue, newValue)+"</"+parentMatch[4].str()+parentTag+">";
        std::string result2=std::regex_replace(partial, parentPattern, newParentBlock, std::regex_constants::format_first_only);
        size_t pos = parentMatch.position();
        result += result2.substr(0, pos+newParentBlock.length());
        partial = result2.substr(pos+newParentBlock.length());
    }
    return result+partial;
}

/*!
 * \brief Changes the path of selected datasets, leaving all other bytes of \a xml untouched.
 *
 * Datasets are found as in scan() (EML 2.0 PathInHdfFile, EML 2.3 PathInExternalFile).
 * \param xml the XML to rewrite
 * \param remap called for each dataset, returns its new (unescaped) path, or nothing to keep it.
 * \return \a xml with the new paths XML-escaped and spliced in.
 * \throws std::exception on XML parse errors, or when a path to change is not plain text.
 */
std::string FilePart::rewriteDatasetPaths(
    const std::string& xml,
    const std::function<std::optional<std::string>(const xml::Hdf5Dataset&)>& remap
) {
    // Non-destructive parsing keeps node values pointing into buf, giving us their offsets
    std::string buf(xml);
    rapidxml_ns::xml_document<char> doc;
    doc.parse<rapidxml_ns::parse_non_destructive>(buf.data());

    const XNode* root = doc.first_node();
    if (!root) {
        throw std::runtime_error("No XML root element");
    }

    struct Edit {
        size_t offset_;
        size_t size_;
        std::string text_;
    };
    std::vector<Edit> edits;

    auto onDataset = [&](const XNode* path_elem, const xml::Hdf5Dataset& hdf) {
        std::optional<std::string> new_path = remap(hdf);
        if (!new_path) {
            return;
        }
        const XNode* text = path_elem->first_node();
        const bool is_data = text && (text->type() == rapidxml_ns::node_data);
        const bool is_cdata = text && (text->type() == rapidxml_ns::node_cdata);
        if (!(is_data || is_cdata) || text->next_sibling()) {
            throw std::runtime_error(fmt::format(
                "Cannot rewrite dataset path '{}': not a single text node", hdf.PathInHdfFile_
            ));
        }

        std::string escaped;
        if (is_cdata) {
            escaped = std::move(*new_path);
        } else {
            escaped.reserve(new_path->size());
            for (char c : *new_path) {
                switch (c) {
                case '&': escaped += "&amp;"; break;
                case '<': escaped += "&lt;"; break;
                case '>': escaped += "&gt;"; break;
                default:  escaped += c; break;
                }
            }
        }
        edits.push_back({ size_t(text->value() - buf.data()), text->value_size(), std::move(escaped) });
    };

    // Same traversal as scan(), when only interested in datasets
    for (const XNode* elem = root->first_node(); elem; elem = elem->next_sibling()) {
        if (elem->type() != rapidxml_ns::node_element) {
            continue;
        }
        if (is_eml(elem) && is_local_name(elem, "Citation")) {
            continue;
        }
        if (is_local_name(elem, "ExtensionNameValue") || is_local_name(elem, "ExtraMetadata")) {
            continue;
        }
        recurse_depth_first(elem, [&onDataset](XNodes& stack) {
            const XNode* descendant = stack.back();
            if (!is_eml(descendant)) {
                return eOK_DONE;
            }
            if (is_local_name(descendant, "PathInHdfFile")) {
                xml::Hdf5Dataset hdf;
                init_dataset_within(hdf, descendant);
                onDataset(descendant, hdf);
                return eOK_STOPPED;
            }
            if (is_local_name(descendant, "PathInExternalFile")) {
                xml::Hdf5Dataset hdf;
                init_dataset_within23(hdf, descendant);
                onDataset(descendant, hdf);
                return eOK_STOPPED;
            }
            if (is_local_name(descendant, "ContentType") || is_local_name(descendant, "QualifiedType")) {
                return eOK_STOPPED;
            }
            return eOK_DONE;
        });
    }

    std::string result(xml);
    for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
        result.replace(it->offset_, it->size_, it->text_);
    }
    return result;
}

} // namespace oes::common::epc
