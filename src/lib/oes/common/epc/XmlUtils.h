// ============================================================================
// Copyright 2025 Emerson Paradigm Holding LLC. All rights reserved.
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
#ifndef OES_COMMON_EPC_XML_UTILS
#define OES_COMMON_EPC_XML_UTILS

#include "../export.h"

#include <oes/common/etp12/EmlUri.h>

#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <initializer_list>
#include <variant>
#include <unordered_map>

namespace oes::common::epc {

class XmlNode;
using XmlNode_ptr = std::shared_ptr<XmlNode>;

class XmlDoc;
using XmlDoc_ptr = std::shared_ptr<XmlDoc>;

using attr_t = std::variant<const char*,
                            std::string,
                            int,
                            unsigned,
                            int64_t,
                            uint64_t,
                            bool,
                            double,
                            float>;

/*
    XmlDoc class
 * This class designed to start an Xml document using tinyxml2 using the structure of a tree. 
 * It must be used to create ALL nodes and it is responsibility of the developer to assign which
 * node is parent or children.
 * 
 * A node create by an XmlDoc object XmlDoc_A will only be shown in the XmlDoc_A if it is be assigned as children of 
 * any other node created with XmlDoc_A and it is also created by XmlDoc_A.
 * 
 * The root xml element must be set by setRootNode method.
*/ 
class OES_COMMON_EXPORT XmlDoc {

public:
    XmlDoc();
    XmlDoc(const XmlDoc&) = delete;
    XmlDoc& operator=(const XmlDoc&) = delete;
    ~XmlDoc();

    void setRootNode(XmlNode_ptr& node);
    XmlNode_ptr newNode(const std::string& name);
    XmlNode_ptr newNode(const std::string& name, attr_t value);
    XmlNode_ptr& newNodeMapped(const std::string& name, const std::string& key);
    XmlNode_ptr& newNodeMapped(const std::string& name, const std::string& key, attr_t value);

    XmlNode_ptr& at(const std::string& key);
    XmlNode_ptr& operator[](const std::string& key);

    std::string toString() const;

private:
    class Impl_;
    std::unique_ptr<Impl_> impl_;


    friend class XmlNode;

    std::unique_ptr<std::unordered_map<std::string, XmlNode_ptr>> p_map_ = nullptr;
};

/*
    XmlNode class
 * This class is designed to work with XmlDoc. It can not be instantiated wihout calling newNode method from
 * an XmlDoc instance.
 * 
 * Each node keep a shared_pointer to its children nodes and its parent node, if it is not the root element.
*/ 
class OES_COMMON_EXPORT XmlNode {
public:
    XmlNode() = delete;
    XmlNode(XmlDoc& doc, const std::string& name);
    XmlNode(XmlDoc& doc, const std::string& name, attr_t value);
public:
    ~XmlNode();

    XmlNode_ptr getParent();
    void setParent(XmlNode_ptr parent);
    void setThisNodeRoot(XmlDoc& doc);

    std::vector<XmlNode_ptr> getChildren();

    XmlNode_ptr addChild(const std::string& name);
    void addChild(XmlNode_ptr child);

    
    void setAttr(const std::string& name, const char* value);
    void setAttr(const std::string& name, std::string value);
    void setAttr(const std::string& name, int value);
    void setAttr(const std::string& name, unsigned value);
    void setAttr(const std::string& name, int64_t value);
    void setAttr(const std::string& name, uint64_t value);
    void setAttr(const std::string& name, bool value);
    void setAttr(const std::string& name, double value);
    void setAttr(const std::string& name, float value);
    
    void setAttrs(std::initializer_list<std::pair<std::string, attr_t>>);
    void setAttrs(const std::vector<std::pair<std::string, attr_t>>& vec);
    void setNodeValue(attr_t value);

    std::string toString() const;

    friend XmlNode_ptr newXmlDoc(const std::string& name);
    friend class XmlDoc;


private:
    class Impl_;
    std::unique_ptr<Impl_> impl_;

    std::vector<XmlNode_ptr> children_;
    XmlNode_ptr parent_;
};

// XmlHelper class

using oes::common::etp12::EmlUri;

/*
    xmlHeaderInfo struct
 * Used to keep info about xml header.
*/ 
struct OES_COMMON_EXPORT xmlHeaderInfo {
    std::string name;
    std::string schemaVersion;
    std::string xsiType;
};

/*
    XmlHelper class
 * Designed to help developer to create new XmlNodes following the osdu pattern of xml file.
*/ 
class OES_COMMON_EXPORT XmlHelper final {
public:
    XmlHelper() = default;
    ~XmlHelper() = default;

    void setCurrentMl(EmlUri::ML ml);
    std::string withMl(const std::string& n, EmlUri::ML ml);
    std::string operator()(const std::string& n, EmlUri::ML ml);
    std::string withMl(const std::string& n);
    std::string operator()(const std::string& n);
    std::string with(const std::string& n, const std::string& prefix);
    std::string operator()(const std::string& n, const std::string& prefix);


    xmlHeaderInfo getHeaderInfo(const std::string& node_name, EmlUri::ML ml = EmlUri::eNO_ML);
    xmlHeaderInfo getHeaderInfo(const std::string& node_name);
    
    static std::vector<std::pair<std::string, attr_t>> getDefaultAttrs();
    static std::string getDateTime();

    static EmlUri::ML xmlnsToML(const std::string& xmlns, const std::string& schema_version);


private:
    EmlUri::ML current_ml_ = EmlUri::eNO_ML;
};

} // namespace oes::common::epc

#endif // OES_COMMON_EPC_XML_UTILS