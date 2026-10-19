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

#include "CoreProperty.h"

#include <cassert>
#include <sstream>

namespace oes::common::epc {

CoreProperty::CoreProperty(
    PtyType pType
) : type_(pType)
{
}

CoreProperty::CoreProperty(
    PtyType pType, const std::string& pValue
) : type_(pType)
  , values_(&pValue, &pValue + 1)
{
}

CoreProperty::CoreProperty(
    PtyType pType, std::vector<std::string> pValue
) : type_(pType)
  , values_(std::move(pValue))
{
}

bool CoreProperty::isEmpty() const {
    return values_.empty();
}

PtyType CoreProperty::getTypeProperty() const {
    return type_;
}

void CoreProperty::setTypeProperty(PtyType corePropertyType) {
    type_ = corePropertyType;
}

void CoreProperty::clear() {
    type_ = undefinedCoreProperty;
    values_.clear();
}

const std::vector<std::string>& CoreProperty::getAllValue() const {
    return values_;
}

const std::string& CoreProperty::getValue(int index) const {
    return values_[index];
}

std::string CoreProperty::toString() const {
    std::ostringstream oss;
    print(oss);
    return oss.str();
}

std::ostream& CoreProperty::print(std::ostream& os) const {
    switch (type_) {
    case PtyType::eUndefinedCoreProperty:
        break;
    case PtyType::eCategory:
        return os << "<category>" << getValue() << "</category>";
    case PtyType::eContentStatus:
        return os << "<contentStatus>" << getValue() << "</contentStatus>";
    case PtyType::eCreated:
        return os << "<dcterms:created xsi:type=\"dcterms:W3CDTF\">" << getValue() << "</dcterms:created>";
    case PtyType::eCreator:
        return os << "<dc:creator>" << getValue() << "</dc:creator>";
    case PtyType::eDescription:
        return os << "<dc:description>" << getValue() << "</dc:description>";
    case PtyType::eIdentifier:
        return os << "<dc:identifier>" << getValue() << "</dc:identifier>";
    case PtyType::eKeywords:
        {
            assert(!isEmpty());
            const std::vector<std::string>& wk_Vector = getAllValue();
            const size_t wk_SizeVector = wk_Vector.size();

            os << "<keywords xml:lang=\"" << wk_Vector[0].substr(0, 5) << "\"> "
                << wk_Vector[0].substr(5) << std::endl;

            for (size_t i = 1; i < wk_SizeVector; ++i) {
                os << "<value xml:lang=\"" << wk_Vector[i].substr(0, 5) << "\">"
                    << wk_Vector[i].substr(5) << "</value>" << std::endl;
            }

            return os << "</keywords>" << std::endl;
        }
    case PtyType::eLanguage:
        return os << "<dc:language>" << getValue() << "</dc:language>";
    case PtyType::eLastModifiedBy:
        return os << "<lastModifiedBy>" << getValue() << "</lastModifiedBy>";
    case PtyType::eLastPrinted:
        return os << "<lastPrinted>" << getValue() << "</lastPrinted>";
    case PtyType::eModified:
        return os << "<dcterms:modified xsi:type=\"dcterms:W3CDTF\">" << getValue() << "</dcterms:modified>";
    case PtyType::eRevision:
        return os << "<revision>" << getValue() << "</revision>";
    case PtyType::eSubject:
        return os << "<dc:subject>" << getValue() << "</dc:subject>";
    case PtyType::eTitle:
        return os << "<dc:title>" << getValue() << "</dc:title>";
    case PtyType::eVersion:
        return os << "<version>" << getValue() << "</version>";
    } // no default on purpose

    return os;
}

void CoreProperty::setScalarValue(PtyType type, std::string value) {
    type_ = type;
    values_.clear();
    values_.emplace_back(std::move(value));
}

void CoreProperty::setCategory(std::string pValue) {
    setScalarValue(category, std::move(pValue));
}

void CoreProperty::setContentStatus(std::string pValue) {
    setScalarValue(contentStatus, std::move(pValue));
}

void CoreProperty::setCreated(std::string pValue) {
    setScalarValue(created, std::move(pValue));
}

void CoreProperty::setCreator(std::string pValue) {
    setScalarValue(creator, std::move(pValue));
}

void CoreProperty::setDescription(std::string pValue) {
    setScalarValue(description, std::move(pValue));
}

void CoreProperty::setIdentifier(std::string pValue) {
    setScalarValue(identifier, std::move(pValue));
}

void CoreProperty::setKeywords(std::string pValue) {
    setScalarValue(keywords, std::move(pValue));
}

void CoreProperty::addKeywords(std::string pValue) {
    type_ = keywords;
    values_.emplace_back(std::move(pValue));
}

void CoreProperty::setLanguage(std::string pValue) {
    setScalarValue(language, std::move(pValue));
}

void CoreProperty::setLastModifiedBy(std::string pValue) {
    setScalarValue(lastModifiedBy, std::move(pValue));
}

void CoreProperty::setLastPrinted(std::string pValue) {
    setScalarValue(lastPrinted, std::move(pValue));
}

void CoreProperty::setModified(std::string pValue) {
    setScalarValue(modified, std::move(pValue));
}

void CoreProperty::setRevision(std::string pValue) {
    setScalarValue(revision, std::move(pValue));
}

void CoreProperty::setSubject(std::string pValue) {
    setScalarValue(subject, std::move(pValue));
}

void CoreProperty::setTitle(std::string pValue) {
    setScalarValue(title, std::move(pValue));
}

void CoreProperty::setVersion(std::string pValue) {
    setScalarValue(version, std::move(pValue));
}

bool CoreProperty::equals(const CoreProperty& rhs) const {
    return
        type_ == rhs.type_ &&
        values_ == rhs.values_
    ;
}

} // namespace oes::common::epc
