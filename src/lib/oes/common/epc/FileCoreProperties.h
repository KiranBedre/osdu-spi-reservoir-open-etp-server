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

#ifndef OES_COMMON_EPC_FILECOREPROPERTIES
#define OES_COMMON_EPC_FILECOREPROPERTIES

#include "../export.h"

#include "CoreProperty.h"

namespace oes::common::epc {

class OES_COMMON_EXPORT FileCoreProperties {
private:
    std::vector<CoreProperty> properties_;

public:
    FileCoreProperties();

    bool hasProperty(PtyType type) const;
    const CoreProperty& getProperty(PtyType index) const;

    std::string toString() const;
    std::ostream& print(std::ostream& os) const;

    /**
     * Initialize all the core properties with their default values.
     * If a core property has not default value then this method keep it unchanged.
     */
    void initDefaultCoreProperties();
    void clear();

    void setScalarValue(PtyType type, std::string value);

    void setCategory(std::string value);
    void setContentStatus(std::string value);
    void setCreated(std::string value);
    void setCreator(std::string value);
    void setDescription(std::string value);
    void setIdentifier(std::string value);
    void setKeywords(std::string value);
    void addKeywords(std::string value);
    void setLanguage(std::string value);
    void setLastModifiedBy(std::string value);
    void setLastPrinted(std::string value);
    void setModified(std::string value);
    void setRevision(std::string value);
    void setSubject(std::string value);
    void setTitle(std::string value);
    void setVersion(std::string value);

    /**
     * Read a core properties part from a string.
     */
    void readFromString(const std::string& textInput);

    bool equals(const FileCoreProperties& rhs) const;
};

/****************************************************************************/

inline bool operator==(const FileCoreProperties& lhs, const FileCoreProperties& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const FileCoreProperties& lhs, const FileCoreProperties& rhs) {
    return !lhs.equals(rhs);
}

/****************************************************************************/

} // namespace oes::common::epc

#endif
