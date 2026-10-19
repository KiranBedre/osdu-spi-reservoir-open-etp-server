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

#ifndef OES_COMMON_EPC_FILECONTENTTYPE
#define OES_COMMON_EPC_FILECONTENTTYPE

#include "../export.h"
#include "ContentType.h"

#include <map>

namespace oes::common::epc {

/****************************************************************************/

class OES_COMMON_EXPORT FileContentType {
public:
    using ContentTypeMap = std::map<std::string, ContentType>;

private:
    ContentTypeMap contentTypeMap_;

public:
    FileContentType() = default;

    bool empty() const { return contentTypeMap_.empty(); }
    size_t size() const { return contentTypeMap_.size(); }

    const ContentType& getContentType(const std::string& extensionOrPartName) const;
    const ContentTypeMap& getAllContentType() const;
    std::string toString() const;

    void addContentType(ContentType contentType);

    /**
     * Read a content type part from a string.
     */
    void readFromString(const std::string& textInput);
    bool readFromXmlInPlace(std::string xml);

    bool equals(const FileContentType& rhs) const;
};

/****************************************************************************/

inline bool operator==(const FileContentType& lhs, const FileContentType& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const FileContentType& lhs, const FileContentType& rhs) {
    return !lhs.equals(rhs);
}

/****************************************************************************/

} // namespace oes::common::epc

#endif
