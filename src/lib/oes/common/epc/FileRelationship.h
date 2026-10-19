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

#ifndef OES_COMMON_EPC_FILERELATIONSHIP
#define OES_COMMON_EPC_FILERELATIONSHIP

#include <vector>

#include "../export.h"
#include "Relationship.h"

namespace oes::common::epc {

/****************************************************************************/

/**
 * This class intends to represent any rel file of the package.
 */
class OES_COMMON_EXPORT FileRelationship {
private:
    std::string pathName_ = "_rels\\.rels";
    std::vector<Relationship> relationships_;

public:
    FileRelationship() = default;

    FileRelationship(FileRelationship&&) = default;
    FileRelationship(const FileRelationship&) = default;

    FileRelationship& operator=(FileRelationship&&) = default;
    FileRelationship& operator=(const FileRelationship&) = default;

    explicit FileRelationship(const Relationship& frRelationship);
    explicit FileRelationship(std::vector<Relationship> frRelationship);

    size_t size() const;
    bool empty() const;
    bool isEmpty() const { return empty(); }
    std::string getPathName() const;
    const std::vector<Relationship>& getAllRelationship() const;
    const Relationship& getIndexRelationship(int index) const;

    std::string toString() const; // to XML
    std::ostream& print(std::ostream&) const;

    void setPathName(std::string frPathName);
    void addRelationship(Relationship frRelationship);

    void readFromString(const std::string & textInput); // from XML
    bool readFromXmlInPlace(std::string xml);

    bool equals(const FileRelationship& rhs) const;
};

/****************************************************************************/

inline bool operator==(const FileRelationship& lhs, const FileRelationship& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const FileRelationship& lhs, const FileRelationship& rhs) {
    return !lhs.equals(rhs);
}

/****************************************************************************/

} // namespace oes::common::epc

#endif

