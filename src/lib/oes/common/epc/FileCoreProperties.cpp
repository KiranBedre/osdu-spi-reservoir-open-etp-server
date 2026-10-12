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

#include "FileCoreProperties.h"

#include <oes/core/utils/Guid.h>

#include <ctime>
#include <sstream>
#include <iterator>
#if defined(_WIN32)
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#  include <windows.h> // cannot use WIN32_LEAN_AND_MEAN or NOGDI
#elif defined(__gnu_linux__)
#  include <unistd.h>
#  include <pwd.h>
#endif

using namespace std;

namespace oes::common::epc {

FileCoreProperties::FileCoreProperties()
  : properties_(size_t(PtyType::eUndefinedCoreProperty)+1) {
};

bool FileCoreProperties::hasProperty(PtyType type) const {
    return !properties_[size_t(type)].isEmpty();
}

const CoreProperty& FileCoreProperties::getProperty(PtyType index) const {
    return properties_[size_t(index)];
}

void FileCoreProperties::initDefaultCoreProperties()
{
    // CREATOR
    if (!hasProperty(CoreProperty::creator)) {
#if defined(__gnu_linux__)
        struct passwd *pw;
        uid_t uid;

        uid = geteuid ();
        pw = getpwuid (uid);
        if (pw)
            setCreator(pw->pw_name);
        else
            setCreator("unknown");
#elif defined(_WIN32)
        char acUserName[32];
        DWORD nUserName = sizeof(acUserName);
        GetUserNameA(acUserName, &nUserName);
        setCreator(acUserName);
#endif
    }

    // CREATED
    if (!hasProperty(CoreProperty::created)) {
        time_t timestamp;
        struct tm * t;
        timestamp = time(NULL);
        t = gmtime(&timestamp);
        int year = 1900 + t->tm_year;
        int month = 1 + t->tm_mon;
        int day = t->tm_mday;
        int hour = t->tm_hour;
        int minute = t->tm_min;
        int second = t->tm_sec;

        ostringstream oss;
        oss << year << "-";
        if (month > 9)
            oss << month<< "-";
        else
            oss << '0' << month<< "-";
        if (day > 9)
            oss << day << "T";
        else
            oss << '0' << day << "T";
        if (hour > 9)
            oss << hour << ":";
        else
            oss << '0' << hour << ":";
        if (minute > 9)
            oss << minute << ":";
        else
            oss << '0' << minute << ":";
        if (second > 9)
            oss << second << "Z";
        else
            oss << '0' << second << "Z";
        setCreated(oss.str());
    }

    // VERSION
    if (!hasProperty(CoreProperty::version)) {
        setVersion("1.0");
    }

    // Identifier
    if (!hasProperty(CoreProperty::identifier)) {
        oes::core::Guid uuid = oes::core::Guid::make();
        ostringstream oss;
        oss << "urn:uuid:" << uuid.toXmlString();
        setIdentifier(oss.str());
    }
}

std::string FileCoreProperties::toString() const {
    std::ostringstream oss;
    print(oss);
    return oss.str();
}

std::ostream& FileCoreProperties::print(std::ostream& os) const {
    // XML def + namespaces def
    os <<
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<coreProperties xmlns=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\"\n"
        "xmlns:dcterms=\"http://purl.org/dc/terms/\"\n"
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
    ;

    // content
    for (const auto& pty : properties_) {
        os << '\t';
        pty.print(os);
        os << endl;
    }

    // end tag
    return os << "</coreProperties>" << endl;
}

void FileCoreProperties::setScalarValue(PtyType type, std::string value) {
    properties_[size_t(type)].setScalarValue(type, std::move(value));
}

void FileCoreProperties::setCategory(std::string value) {
    setScalarValue(CoreProperty::category, std::move(value));
}

void FileCoreProperties::setContentStatus(std::string value) {
    setScalarValue(CoreProperty::contentStatus, std::move(value));
}

void FileCoreProperties::setCreated(std::string value) {
    setScalarValue(CoreProperty::created, std::move(value));
}

void FileCoreProperties::setCreator(std::string value) {
    setScalarValue(CoreProperty::creator, std::move(value));
}

void FileCoreProperties::setDescription(std::string value) {
    setScalarValue(CoreProperty::description, std::move(value));
}

void FileCoreProperties::setIdentifier(std::string value) {
    setScalarValue(CoreProperty::identifier, std::move(value));
}

void FileCoreProperties::setKeywords(std::string value) {
    setScalarValue(CoreProperty::keywords, std::move(value));
}

void FileCoreProperties::addKeywords(std::string value) {
    properties_[size_t(PtyType::eKeywords)].addKeywords(std::move(value));
}

void FileCoreProperties::setLanguage(std::string value) {
    setScalarValue(CoreProperty::language, std::move(value));
}

void FileCoreProperties::setLastModifiedBy(std::string value) {
    setScalarValue(CoreProperty::lastModifiedBy, std::move(value));
}

void FileCoreProperties::setLastPrinted(std::string value) {
    setScalarValue(CoreProperty::lastPrinted, std::move(value));
}

void FileCoreProperties::setModified(std::string value) {
    setScalarValue(CoreProperty::modified, std::move(value));
}

void FileCoreProperties::setRevision(std::string value) {
    setScalarValue(CoreProperty::revision, std::move(value));
}

void FileCoreProperties::setSubject(std::string value) {
    setScalarValue(CoreProperty::subject, std::move(value));
}

void FileCoreProperties::setTitle(std::string value) {
    setScalarValue(CoreProperty::title, std::move(value));
}

void FileCoreProperties::setVersion(std::string value) {
    setScalarValue(CoreProperty::version, std::move(value));
}

void FileCoreProperties::clear() {
    for (auto& pty : properties_) {
        pty.clear();
    }
}

void FileCoreProperties::readFromString(const string& textInput) {
    clear();

    size_t start = textInput.find("<coreProperties");
    size_t end = textInput.find("</coreProperties>", start);

    //Read all properties_ based on TypeProperty order
    const char* pname[] = {
       "category","contentStatus","created","creator","description",
       "identifier", "keywords","language","lastModifiedBy","lastPrinted",
       "modified", "revision", "subject", "title", "version"
    };
    for (size_t i = 0; i < size_t(CoreProperty::undefinedCoreProperty); ++i) {
        size_t attStart = textInput.find(pname[i], start);
        if (attStart != string::npos && attStart < end){
            attStart = textInput.find(">", attStart);
            if (attStart != string::npos){
                attStart++;

                if (i == size_t(CoreProperty::keywords)) {
                    size_t attEnd = textInput.find(std::string("</keywords"), attStart);
                    if (attStart != string::npos && attEnd != string::npos){
                        std::string str = textInput.substr(attStart, attEnd - attStart);
                        size_t valStart = str.find(">", 0);
                        while(valStart != string::npos){
                            valStart++;

                            std::vector<std::string> list;
                            size_t valEnd = str.find(std::string("</"), valStart);
                            if (valStart != string::npos && valEnd != string::npos){
                                std::string valstr = textInput.substr(valStart, valEnd - valStart);
                                list.emplace_back(std::move(valstr));
                            }
                            properties_[size_t(CoreProperty::keywords)] = CoreProperty(CoreProperty::keywords, std::move(list));
                            valEnd = str.find(">", valEnd); //Match previous </
                            if(valEnd == string::npos){
                                break;
                            }
                            valStart = str.find(">", valEnd); //New value
                        }
                    }
                } else {
                    size_t attEnd = textInput.find(std::string("</"), attStart);
                    if (attStart != string::npos && attEnd != string::npos){
                        std::string str = textInput.substr(attStart, attEnd - attStart);
                        CoreProperty::TypeProperty type = static_cast<CoreProperty::TypeProperty>(i);
                        properties_[i] = CoreProperty(type,str);
                    }
                }
            }
        }
    }
}

bool FileCoreProperties::equals(const FileCoreProperties& rhs) const {
    return properties_ == rhs.properties_;
}

} // namespace oes::common::epc
