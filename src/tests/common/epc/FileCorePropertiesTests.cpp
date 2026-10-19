// ============================================================================
// Copyright 2022-2023 Aspen Technology, Inc. All rights reserved.
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

#include <oes/common/epc/FileCoreProperties.h>

#include <boost/test/unit_test.hpp>

namespace {

using namespace oes::common::epc;

//============================================================================

BOOST_AUTO_TEST_SUITE(FileCorePropertiesTests)

BOOST_AUTO_TEST_CASE(DefaultPropertiesTest)
{
    FileCoreProperties properties;
    
    // Check if default properties are initialized correctly
    BOOST_CHECK(!properties.hasProperty(CoreProperty::creator));
    BOOST_CHECK(!properties.hasProperty(CoreProperty::created));
    BOOST_CHECK(!properties.hasProperty(CoreProperty::version));
    BOOST_CHECK(!properties.hasProperty(CoreProperty::identifier));
    
    properties.initDefaultCoreProperties();
    
    // Check if default properties are set after initialization
    BOOST_CHECK(properties.hasProperty(CoreProperty::creator));
    BOOST_CHECK(properties.hasProperty(CoreProperty::created));
    BOOST_CHECK(properties.hasProperty(CoreProperty::version));
    BOOST_CHECK(properties.hasProperty(CoreProperty::identifier));
}

BOOST_AUTO_TEST_CASE(PropertyAccessTest)
{
    FileCoreProperties properties;
    
    properties.initDefaultCoreProperties();
    
    // Check if the initialized default properties can be accessed correctly
    const CoreProperty& creatorProp = properties.getProperty(CoreProperty::creator);
    const CoreProperty& createdProp = properties.getProperty(CoreProperty::created);
    const CoreProperty& versionProp = properties.getProperty(CoreProperty::version);
    const CoreProperty& identifierProp = properties.getProperty(CoreProperty::identifier);
    
    BOOST_CHECK(creatorProp.getTypeProperty() == CoreProperty::creator);
    BOOST_CHECK(createdProp.getTypeProperty() == CoreProperty::created);
    BOOST_CHECK(versionProp.getTypeProperty() == CoreProperty::version);
    BOOST_CHECK(identifierProp.getTypeProperty() == CoreProperty::identifier);
    
    BOOST_CHECK(!creatorProp.isEmpty());
    BOOST_CHECK(!createdProp.isEmpty());
    BOOST_CHECK(!versionProp.isEmpty());
    BOOST_CHECK(!identifierProp.isEmpty());
    
    // Additional checks for specific property values can be added here
}

BOOST_AUTO_TEST_CASE(ToStringTest)
{
    FileCoreProperties properties;
    
    properties.initDefaultCoreProperties();
    
    // Check if the toString() method returns the expected XML string
    std::string expectedXmlString =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<coreProperties xmlns=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\"\n"
        "xmlns:dcterms=\"http://purl.org/dc/terms/\"\n"
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n";

    std::string output = properties.toString();
    BOOST_CHECK(output.rfind(expectedXmlString,0)==0); // starts with the expected string
    BOOST_CHECK(output.find("<dcterms:created xsi:type=\"dcterms:W3CDTF\">") != std::string::npos);
    BOOST_CHECK(output.find("<dc:creator>") != std::string::npos);
    BOOST_CHECK(output.find("<dc:identifier>") != std::string::npos);
    BOOST_CHECK(output.find("<version>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(PropertyModificationTest)
{
    FileCoreProperties properties;
    
    properties.initDefaultCoreProperties();
    
    // Modify a property value and check if it is updated correctly
    properties.setCreator("John Doe");
    
    const CoreProperty& creatorProp = properties.getProperty(CoreProperty::creator);
    
    BOOST_CHECK(creatorProp.getValue() == "John Doe");
    BOOST_CHECK(properties.toString().find("<dc:creator>John Doe</dc:creator>") != std::string::npos);
    
    // Additional property modification checks
    int index = 0;
    std::vector<std::string> input_parameters = {
        "setCategory", 
        "setContentStatus", 
        "setCreated", 
        "setCreator",
        "setDescription",
        "setIdentifier",
        "setKeywords",
        "setLanguage",
        "setLastModifiedBy",
        "setLastPrinted",
        "setModified",
        "setRevision",
        "setSubject",
        "setTitle",
        "setVersion",
        "setScalarValue"
     };
    // Modify a property value and check if it is updated correctly
    properties.setCategory(input_parameters[index++]);
    properties.setContentStatus(input_parameters[index++]);
    properties.setCreated(input_parameters[index++]);
    properties.setCreator(input_parameters[index++]);
    properties.setDescription(input_parameters[index++]);
    properties.setIdentifier(input_parameters[index++]);
    properties.setKeywords(input_parameters[index++]);
    properties.setLanguage(input_parameters[index++]);
    properties.setLastModifiedBy(input_parameters[index++]);
    properties.setLastPrinted(input_parameters[index++]);
    properties.setModified(input_parameters[index++]);
    properties.setRevision(input_parameters[index++]);
    properties.setSubject(input_parameters[index++]);
    properties.setTitle(input_parameters[index++]);
    properties.setVersion(input_parameters[index++]);
    properties.setScalarValue(CoreProperty::undefinedCoreProperty, input_parameters[index++]);

    for (unsigned char i = static_cast<unsigned char>(PtyType::eCategory);
         i <= static_cast<unsigned char>(PtyType::eUndefinedCoreProperty);
         ++i)
    {
        PtyType type = static_cast<PtyType>(i);
        const CoreProperty& pty = properties.getProperty(type);
        BOOST_CHECK(pty.getTypeProperty() == type);
        BOOST_CHECK(pty.getValue() == input_parameters[i]);
    }

    properties.addKeywords("addKeywords");
    const CoreProperty& keywordsProp = properties.getProperty(CoreProperty::keywords);
    BOOST_CHECK(keywordsProp.getTypeProperty() == PtyType::eKeywords);
    BOOST_CHECK(keywordsProp.getAllValue().size() == 2);
    BOOST_CHECK(keywordsProp.getValue(1) == "addKeywords");
}

BOOST_AUTO_TEST_CASE(PropertyClearTest)
{
    FileCoreProperties properties;
    
    properties.initDefaultCoreProperties();
    
    // Clear all properties and check if they are empty
    properties.clear();
    
    for (unsigned char i = static_cast<unsigned char>(PtyType::eCategory);
         i <= static_cast<unsigned char>(PtyType::eUndefinedCoreProperty);
         ++i)
    {
        PtyType type = static_cast<PtyType>(i);
        const CoreProperty& pty = properties.getProperty(type);
        BOOST_CHECK(pty.getTypeProperty() == CoreProperty::undefinedCoreProperty);
        BOOST_CHECK(pty.isEmpty());
    }
}

BOOST_AUTO_TEST_CASE(PropertyReadFromStringTest)
{
    FileCoreProperties properties;
    
    std::string xmlString =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<coreProperties xmlns=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\"\n"
        "xmlns:dcterms=\"http://purl.org/dc/terms/\"\n"
        "xmlns:dc=\"http://purl.org/dc/elements/1.1/\"\n"
        "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">\n"
        "\t<dc:creator>John Doe</dc:creator>\n"
        "\t<dcterms:created>2023-01-01T12:00:00Z</dcterms:created>\n"
        "\t<version>2.0</version>\n"
        "\t<dc:identifier>12345</dc:identifier>\n"
        "</coreProperties>\n";
    
    properties.readFromString(xmlString);
    
    BOOST_CHECK(properties.hasProperty(CoreProperty::creator));
    BOOST_CHECK(properties.hasProperty(CoreProperty::created));
    BOOST_CHECK(properties.hasProperty(CoreProperty::version));
    BOOST_CHECK(properties.hasProperty(CoreProperty::identifier));
    
    const CoreProperty& creatorProp = properties.getProperty(CoreProperty::creator);
    const CoreProperty& createdProp = properties.getProperty(CoreProperty::created);
    const CoreProperty& versionProp = properties.getProperty(CoreProperty::version);
    const CoreProperty& identifierProp = properties.getProperty(CoreProperty::identifier);
    
    BOOST_CHECK(creatorProp.getValue() == "John Doe");
    BOOST_CHECK(createdProp.getValue() == "2023-01-01T12:00:00Z");
    BOOST_CHECK(versionProp.getValue() == "2.0");
    BOOST_CHECK(identifierProp.getValue() == "12345");
}

BOOST_AUTO_TEST_CASE(PropertyEqualityTest)
{
    FileCoreProperties properties1;
    properties1.initDefaultCoreProperties();
    
    FileCoreProperties properties2;
    properties2.initDefaultCoreProperties();

    // properties1 and properties2 should differs by their identifiers
    BOOST_CHECK(!properties1.equals(properties2));

    const CoreProperty& identifierProp1 = properties1.getProperty(CoreProperty::identifier);
    properties2.setIdentifier(identifierProp1.getValue());
    // Now they should be the same
    BOOST_CHECK(properties1.equals(properties2));
    
    // Modify a property in properties2 and check if they are not equal
    properties2.setCreator("John Doe");
    
    BOOST_CHECK(!properties1.equals(properties2));
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace