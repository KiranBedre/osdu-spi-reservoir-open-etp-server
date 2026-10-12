#include "XmlUtils.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <tinyxml2.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/ChronoUtils.h>

#include <stdexcept>
#include <chrono>

namespace oes::common::epc {

// ------------------------------------------------------------------

class XmlDoc::Impl_ {
public:
    Impl_();
    ~Impl_() = default;

    std::shared_ptr<tinyxml2::XMLDocument> p_doc_;
    tinyxml2::XMLElement* newElement(const char* name);
    void setRoot(tinyxml2::XMLElement* root);

    std::string toString() const;
};

XmlDoc::Impl_::Impl_()
    : p_doc_ {std::make_shared<tinyxml2::XMLDocument>()} {
    auto dclr = p_doc_->NewDeclaration(); // by default adds <?xml version="1.0" encoding="UTF-8"?>
    p_doc_->InsertFirstChild(dclr);
}

tinyxml2::XMLElement* XmlDoc::Impl_::newElement(const char* name) {
    return p_doc_->NewElement(name);
}

void XmlDoc::Impl_::setRoot(tinyxml2::XMLElement* root) {
    p_doc_->InsertEndChild(root);
}

std::string XmlDoc::Impl_::toString() const {
    tinyxml2::XMLPrinter printer;
    p_doc_->Accept(&printer);
    return std::string(printer.CStr());
}

// XmlDOc
XmlDoc::XmlDoc()
    : impl_ {std::make_unique<XmlDoc::Impl_>()} {}

XmlDoc::~XmlDoc() {}

/*
    setRootNode method
 * Set a given node as root of this Xml document
*/
void XmlDoc::setRootNode(XmlNode_ptr& node) {
    node->setThisNodeRoot(*this);
}

/*
    newNode method
 * creates a new node from this XmlDoc
*/
XmlNode_ptr XmlDoc::newNode(const std::string& name) {
    return std::make_shared<XmlNode>(*this, name);
}

XmlNode_ptr XmlDoc::newNode(const std::string& name, attr_t value) {
    return std::make_shared<XmlNode>(*this, name, value);
}

/*
    newNodeMapped method
 * creates a new node from this XmlDoc it is hold by the XmlDoc instance using a key.
 * The developer does not need to assign it to any variable to keep it existing and can
 * access the node using the method at() or operator[].
*/
XmlNode_ptr& XmlDoc::newNodeMapped(const std::string& name, const std::string& key) {

    if (!p_map_) {
        p_map_ = std::make_unique<std::unordered_map<std::string, XmlNode_ptr>>();
    }

    if (p_map_->find(key) != p_map_->end()) {
        throw std::runtime_error("Duplicated xml key found.");
    }

    p_map_->insert(std::make_pair(key, std::make_shared<XmlNode>(*this ,name)));

    return p_map_->at(key);
}

XmlNode_ptr& XmlDoc::newNodeMapped(const std::string& name, const std::string& key, attr_t value) {
    auto node = newNodeMapped(name, key);
    node->setNodeValue(value);
    return node;
}

/*
    newNodeMapped method
 * returns the node corresponding to its key. If it does not exist it throws an error
*/
XmlNode_ptr& XmlDoc::at(const std::string& key) {
    if (!p_map_) {
        throw std::runtime_error("Xml doc is not mapped yet");
    }
    return p_map_->at(key);
}

/*
    operator[] method
 * returns the node corresponding to its key. If it does not exist it throws an error
*/
XmlNode_ptr& XmlDoc::operator[](const std::string& key) {
    return at(key);
}

/*
    toString method
 * returns the whole xml document string.
*/
std::string XmlDoc::toString() const {
    return impl_->toString();
}


// ================================================================
// ====================== XmlNode::Impl_ ==========================
// ================================================================


class XmlNode::Impl_ {
public:
    friend class XmlNode;

    Impl_(tinyxml2::XMLElement* elem);
    ~Impl_() = default;
    tinyxml2::XMLElement* p_this_node_;

    void addChild(XmlNode::Impl_& child);
    void setParent(XmlNode::Impl_& parent);

    std::string toString() const;
    
};

XmlNode::Impl_::Impl_(tinyxml2::XMLElement* elem)
    : p_this_node_ {elem} {
    }

void XmlNode::Impl_::addChild(XmlNode::Impl_& child) {
    p_this_node_->InsertEndChild(child.p_this_node_);
}

void XmlNode::Impl_::setParent(XmlNode::Impl_& parent) {
    parent.p_this_node_->InsertEndChild(p_this_node_);
}

std::string XmlNode::Impl_::toString() const {
    tinyxml2::XMLPrinter printer;

    p_this_node_->Accept(&printer);
    return std::string(printer.CStr());
}

//--------------------------------------------------------------

XmlNode::XmlNode(XmlDoc& doc, const std::string& name)
    : impl_ { std::make_unique<Impl_>( doc.impl_->newElement(name.c_str()) )} {}

XmlNode::XmlNode(XmlDoc& doc, const std::string& name, attr_t value)
    : XmlNode(doc, name) {
        setNodeValue(value);
    }

XmlNode::~XmlNode() {}

/*
    getParent method
 * Returns the node's parent (can return nullptr)
*/
XmlNode_ptr XmlNode::getParent() {
    return parent_;
}

/*
    setParent method
 * set a XmlNode parent
*/
void XmlNode::setParent(XmlNode_ptr parent) {
    impl_->setParent(*(parent->impl_));
    parent_ = parent;
}

/*
    setThisNodeRoot method
 * Set the node as root node in a given XmlDoc
*/
void XmlNode::setThisNodeRoot(XmlDoc& doc) {
    doc.impl_->setRoot(impl_->p_this_node_);
}

/*
    getParent method
 * Returns the node's children (can be empty)
*/
std::vector<XmlNode_ptr> XmlNode::getChildren() {
    return children_;
}

/*
    getParent method
 * Add a child to this node.
*/
void XmlNode::addChild(XmlNode_ptr child) {
    impl_->addChild(*(child->impl_));
    children_.push_back(child);
}

/*
    setAttr method
 * Set an xml attribute.
*/
void XmlNode::setAttr(const std::string& name, const char* value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}
void XmlNode::setAttr(const std::string& name, std::string value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value.c_str());
}
void XmlNode::setAttr(const std::string& name, int value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}
void XmlNode::setAttr(const std::string& name, unsigned value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}
void XmlNode::setAttr(const std::string& name, int64_t value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}
void XmlNode::setAttr(const std::string& name, uint64_t value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}
void XmlNode::setAttr(const std::string& name, bool value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}
void XmlNode::setAttr(const std::string& name, double value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}
void XmlNode::setAttr(const std::string& name, float value) {
    impl_->p_this_node_->SetAttribute(name.c_str(), value);
}

/*
    setAttrs method
 * Set multiple xml attributes.
*/
void XmlNode::setAttrs(std::initializer_list<std::pair<std::string, attr_t>> list) {
    for (auto& p : list) {
        if (auto x = std::get_if<const char*>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<std::string>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<int>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<unsigned>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<int64_t>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<uint64_t>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<bool>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<double>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<float>(&(p.second))) {
            setAttr(p.first, *x);
        }
    }
} 

void XmlNode::setAttrs(const std::vector<std::pair<std::string, attr_t>>& vec) {
    for (auto& p : vec) {
        if (auto x = std::get_if<const char*>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<std::string>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<int>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<unsigned>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<int64_t>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<uint64_t>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<bool>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<double>(&(p.second))) {
            setAttr(p.first, *x);
        } else if (auto x = std::get_if<float>(&(p.second))) {
            setAttr(p.first, *x);
        }
    }
} 

/*
    setNodeValue method
 * Set a node value.
*/
void XmlNode::setNodeValue(attr_t value) {
    // impl_->p_this_node_->SetText()

    if (auto x = std::get_if<const char*>(&value)) {
        impl_->p_this_node_->SetText(*x);
    } else if (auto x = std::get_if<std::string>(&value)) {
        impl_->p_this_node_->SetText((*x).c_str());
    } else if (auto x = std::get_if<int>(&value)) {
        impl_->p_this_node_->SetText(*x);
    } else if (auto x = std::get_if<unsigned>(&value)) {
        impl_->p_this_node_->SetText(*x);
    } else if (auto x = std::get_if<int64_t>(&value)) {
        impl_->p_this_node_->SetText(*x);
    } else if (auto x = std::get_if<uint64_t>(&value)) {
        impl_->p_this_node_->SetText(*x);
    } else if (auto x = std::get_if<bool>(&value)) {
        impl_->p_this_node_->SetText(*x);
    } else if (auto x = std::get_if<double>(&value)) {
        impl_->p_this_node_->SetText(*x);
    } else if (auto x = std::get_if<float>(&value)) {
        impl_->p_this_node_->SetText(*x);
    }
}

/*
    toString method
 * Return the xml string of this node. All children nodes will be present in this string as well.
*/
std::string XmlNode::toString() const {
    return impl_->toString();
}

// ============================================================
std::pair<std::string, std::string> get_ml_info(EmlUri::ML ml) {
    std::pair<std::string, std::string> p;
    switch (ml) {
        case EmlUri::eEML20:
            p.first = "eml";
            p.second = "2.0";
            break;
        case EmlUri::eEML23:
            p.first = "eml";
            p.second = "2.3";
            break;
        case EmlUri::eRESQML20:
            p.first = "resqml2";
            p.second = "2.0";
            break;
        case EmlUri::eRESQML22:
            p.first = "resqml2";
            p.second = "2.2";
            break;
        case EmlUri::eWITSML21:
            p.first = "witsml2";
            p.second = "2.1";
            break;
        case EmlUri::ePRODML22:
            p.first = "prodml2";
            p.second = "2.2";
            break;
        case EmlUri::eNO_ML:
            p.first = "";
            p.second = "";
            break;
        default:
            p.first = "energistics";
            p.second = "1.0";
            break;
    }

    return p;
}

/*
    setCurrentMl function
 * For a given instance of XmlHelper, the developer does not need to specify which pattern he wants to use
 * After being called, this function set a ml pattern of every call, except if the user forces another pattern
*/
void XmlHelper::setCurrentMl(EmlUri::ML ml) {
    current_ml_ = ml;
}

/*
    withMl function
 * for a given pattern it returns "<ml-pattern>:string".
*/
std::string XmlHelper::withMl(const std::string& n, EmlUri::ML ml) {
    auto ml_str = get_ml_info(ml).first;
    if (ml_str.empty()) {
        return n;
    }
    return fmt::format(
        "{}:{}",
        ml_str,
        n
    );
}

std::string XmlHelper::operator()(const std::string& n, EmlUri::ML ml) {
    return withMl(n, ml);
}

/*
    withMl function
 * for a given pattern it returns "<pattern>:string".
 * If ml not passed, then the current ml is used to get the pattern.
*/
std::string XmlHelper::withMl(const std::string& n) {
    return withMl(n, current_ml_);
}

std::string XmlHelper::operator()(const std::string& n) {
    return withMl(n, current_ml_);
}

/*
    with function
 * returns "prefix:string" pattern
*/
std::string XmlHelper::with(const std::string& n, const std::string& prefix) {
    return fmt::format("{}:{}", prefix, n);
}

std::string XmlHelper::operator()(const std::string& n, const std::string& prefix) {
    return fmt::format("{}:{}", prefix, n);
}

/*
    getHeaderInfo
 * For a given pattern it returns info about the xml header.
 * returns:
 *  1) name: <ml-pattern>:string
 *  2) xsiType: <>ml_pattern:obj_string
 *  3) corresponding schema version of the given ml
*/
xmlHeaderInfo XmlHelper::getHeaderInfo(const std::string& node_name, EmlUri::ML ml) {
    xmlHeaderInfo info;

    info.name = withMl(node_name, ml);

    info.xsiType = withMl(
        fmt::format("obj_{}", node_name),
        ml
    );

    info.schemaVersion = get_ml_info(ml).second;
    return info;
}

/*
    getHeaderInfo
 * For a given pattern it returns info about the xml header. Uses the current set ml.
 * returns:
 *  1) name: <ml-pattern>:string
 *  2) xsiType: <>ml_pattern:obj_string
 *  3) corresponding schema version of the current ml
*/
xmlHeaderInfo XmlHelper::getHeaderInfo(const std::string& node_name) {
    return getHeaderInfo(node_name, current_ml_);
}

/*
    getDefautAttrs function
 * Returns the some pre defined attributes to be used on a XmlNode root node.
*/
std::vector<std::pair<std::string, attr_t>> XmlHelper::getDefaultAttrs() {
    return std::vector<std::pair<std::string, attr_t>> {
        {"xmlns:SOAP-ENV", "http://schemas.xmlsoap.org/soap/envelope/"},
        {"xmlns:SOAP-ENC", "http://schemas.xmlsoap.org/soap/encoding/"},
        {"xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance"},
        {"xmlns:xsd", "http://www.w3.org/2001/XMLSchema"},
        {"xmlns:gts", "http://www.isotc211.org/2005/gts"},
        {"xmlns:gsr", "http://www.isotc211.org/2005/gsr"},
        {"xmlns:dc", "http://purl.org/dc/terms/"},
        {"xmlns:resqml1", "http://www.resqml.org/schemas/1series"},
        {"xmlns:resqml2", "http://www.energistics.org/energyml/data/resqmlv2"},
        {"xmlns:witsml1", "http://www.witsml.org/schemas/1series"},
        {"xmlns:eml", "http://www.energistics.org/energyml/data/commonv2"},
        {"xmlns:gml", "http://www.opengis.net/gml/3.2"},
        {"xmlns:xlink", "http://www.w3.org/1999/xlink"},
        {"xmlns:gmd", "http://www.isotc211.org/2005/gmd"},
        {"xmlns:gco", "http://www.isotc211.org/2005/gco"},
        {"xmlns:ptm", "http://www.f2i-consulting.com/PropertyTypeMapping"},
        {"xmlns:abstract", "http://www.energistics.org/schemas/abstract"},
    };
}

/*
    getDateTime function
 * Returns the strftime of the current datetime
*/
std::string XmlHelper::getDateTime() {
    const auto time = std::chrono::system_clock::now();
    return oes::core::formatDateTime("%Y-%m-%dT%H:%M:%SZ", time, oes::core::FracSecRes::eMilliSecRound);
}

/*
    xmlnsToMl function
 * Returns the corresponding ML of a given xmlns
*/
EmlUri::ML XmlHelper::xmlnsToML(const std::string& xmlns, const std::string& schema_version) {
    auto schema_version_str = schema_version.substr(0, 3);

    switch (xmlns.size()) {
    case  0: return EmlUri::eNO_ML;
    case 49: return (xmlns == "http://www.energistics.org/energyml/data/resqmlv2") ? (schema_version_str == "2.2" ? EmlUri::eRESQML22 : EmlUri::eRESQML20):
                    (xmlns == "http://www.energistics.org/energyml/data/commonv2") ? (schema_version_str == "2.3" ? EmlUri::eEML23 : EmlUri::eEML20):
                    (xmlns == "http://www.energistics.org/energyml/data/witsmlv2") ? EmlUri::eWITSML21 :
                    (xmlns == "http://www.energistics.org/energyml/data/prodmlv2") ? EmlUri::ePRODML22 :
                                                                                     EmlUri::eUNKNOWN_ML;
    default: break;
    }
    return EmlUri::eUNKNOWN_ML;
}


} // namespace oes::common::epc