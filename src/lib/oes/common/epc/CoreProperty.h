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
#ifndef OES_COMMON_EPC_COREPROPERTIES
#define OES_COMMON_EPC_COREPROPERTIES

#include "../export.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace oes::common::epc {

/****************************************************************************/

enum class PtyType : unsigned char {
    eCategory = 0,
    eContentStatus,
    eCreated,
    eCreator,
    eDescription,
    eIdentifier,
    eKeywords,
    eLanguage,
    eLastModifiedBy,
    eLastPrinted,
    eModified,
    eRevision,
    eSubject,
    eTitle,
    eVersion,
    eUndefinedCoreProperty,
};

/****************************************************************************/

class OES_COMMON_EXPORT CoreProperty {
private:
    //! The type of the core property
    PtyType type_;

    //! The string value of the core property.
    //! Can be multiple for some core properties (such as keywords)
    std::vector<std::string> values_;

public:
    //! Enumeration of the various types of the core properties.
    //! \warning Legacy nested enum values. Prefer PtyType instead.
    using TypeProperty = PtyType;
    static const PtyType category                = PtyType::eCategory;
    static const PtyType contentStatus           = PtyType::eContentStatus;
    static const PtyType created                 = PtyType::eCreated;
    static const PtyType creator                 = PtyType::eCreator;
    static const PtyType description             = PtyType::eDescription;
    static const PtyType identifier              = PtyType::eIdentifier;
    static const PtyType keywords                = PtyType::eKeywords;
    static const PtyType language                = PtyType::eLanguage;
    static const PtyType lastModifiedBy          = PtyType::eLastModifiedBy;
    static const PtyType lastPrinted             = PtyType::eLastPrinted;
    static const PtyType modified                = PtyType::eModified;
    static const PtyType revision                = PtyType::eRevision;
    static const PtyType subject                 = PtyType::eSubject;
    static const PtyType title                   = PtyType::eTitle;
    static const PtyType version                 = PtyType::eVersion;
    static const PtyType undefinedCoreProperty   = PtyType::eUndefinedCoreProperty;

public:
    explicit CoreProperty(PtyType pType = PtyType::eUndefinedCoreProperty);
    CoreProperty(PtyType pType, const std::string& pValue);
    CoreProperty(PtyType pType, std::vector<std::string> pValue);

    //! Indicate whether a property has got a string value or not
    bool isEmpty() const;

    //! Get the type of the core property.
    PtyType getTypeProperty() const;

    //! Set the type of the core property
    //! \warning if type is undefinedCoreProperty, does NOT empty the values. Use clear() instead.
    void setTypeProperty(PtyType corePropertyType);

    //! resets this core property to empty and undefined
    void clear();

    //! Get all the string values. Relevant only for some core properties which can have several string values (such as keywords)
    const std::vector<std::string>& getAllValue() const;

    /**
     * Get the first string value of the core property.
     * Usually this method is the common one for retrieving string value of a core property
     * since most of time only one string value is necessary for one core property.
     */
    const std::string& getValue(int index = 0) const;

    //! Serialize the core property into an XML element embedded into a string.
    std::string toString() const;
    std::ostream& print(std::ostream& os) const;

    void setScalarValue(PtyType type, std::string value);

    void setCategory(std::string pValue);
    void setContentStatus(std::string pValue);
    void setCreated(std::string pValue);
    void setCreator(std::string pValue);
    void setDescription(std::string pValue);
    void setIdentifier(std::string pValue);
    void setKeywords(std::string pValue);
    void addKeywords(std::string pValue);
    void setLanguage(std::string pValue);
    void setLastModifiedBy(std::string pValue);
    void setLastPrinted(std::string pValue);
    void setModified(std::string pValue);
    void setRevision(std::string pValue);
    void setSubject(std::string pValue);
    void setTitle(std::string pValue);
    void setVersion(std::string pValue);

    bool equals(const CoreProperty& rhs) const;
};

/****************************************************************************/

inline bool operator==(const CoreProperty& lhs, const CoreProperty& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const CoreProperty& lhs, const CoreProperty& rhs) {
    return !lhs.equals(rhs);
}

/****************************************************************************/

} // namespace oes::common::epc

#endif
