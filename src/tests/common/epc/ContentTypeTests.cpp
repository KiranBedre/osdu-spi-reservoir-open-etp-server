// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/common/epc/ContentType.h>

#include <oes/testing/core/TestMain.h>

namespace {

using namespace oes::common;

BOOST_AUTO_TEST_SUITE(EpcContentTypeTests)

BOOST_AUTO_TEST_CASE(test_parse_ok) {
    const char* content_types[] {
        "application/x-resqml+xml;version=2.0;type=obj_WellboreInterpretation",
        "application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs",
        "application/x-resqml+xml;version=2.0;type=EpcExternalPartReference", // incorrect
        "application/x-eml+xml;version=2.0;type=EpcExternalPartReference", // correct
        "application/x-resqml+xml;version=2.0;type=obj_MdDatum",
        "application/x-resqml+xml;version=2.0.1;type=obj_ActivityTemplate"
    };
    for (std::string content_type : content_types) {
        std::string ml, format, version, type;
        const bool parse_ok = epc::ContentType::parse(
            content_type, ml, format, version, type
        );
        BOOST_CHECK(parse_ok);

        if (ml != "resqml") {
            // only type that can belong to a different ML
            BOOST_CHECK_EQUAL(type, "EpcExternalPartReference");
        }

        if (type.find("obj_") != 0) {
            // only type not prefixed with obj_
            BOOST_CHECK_EQUAL(type, "EpcExternalPartReference");
        }

        if (type == "obj_ActivityTemplate") {
            BOOST_CHECK_EQUAL(version, "2.0.1");
        } else {
            BOOST_CHECK_EQUAL(version, "2.0");
        }

        std::size_t last_eq = content_type.rfind('=');
        BOOST_REQUIRE(last_eq != std::string::npos);
        BOOST_CHECK_EQUAL(content_type.substr(last_eq + 1), type);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_ko) {
    const char* content_types[]{
        "application/x-resqml"/*+xml*/";version=2.0;type=obj_WellboreInterpretation",
        "application/x-resqml+xml;version=2.0;",//type=obj_LocalDepth3dCrs",
        "application/x"/*-resqml*/"+xml;version=2.0;type=EpcExternalPartReference", // incorrect
        "app"/*lication*/"/x-eml+xml;version=2.0;type=EpcExternalPartReference", // correct
        /*"application*/"x-resqml+xml;version=2.0;type=obj_MdDatum",
        "application/x-resqml+xml;"/*version=2.0.1*/"type=obj_ActivityTemplate"
    };
    for (std::string content_type : content_types) {
        std::string ml, format, version, type;
        const bool parse_ok = epc::ContentType::parse(
            content_type, ml, format, version, type
        );
        BOOST_CHECK(!parse_ok);
    }
}

BOOST_AUTO_TEST_CASE(test_equals) {
    epc::ContentType empty1, empty2;
    BOOST_CHECK(empty1 == empty2);
    BOOST_CHECK(!(empty1 != empty2));

    const std::string contentType = "application/x-resqml+xml;version=2.0;type=obj_ContinuousProperty";
    const std::string extensionOrPartName = "/obj_ContinuousProperty_00008e2a-2735-4479-bd90-9b21a10fc01c.xml";

    const epc::ContentType via_ctor(true, contentType, extensionOrPartName);

    epc::ContentType via_setters;
    via_setters.setContentTypeString(contentType);
    via_setters.setExtensionOrPartName(extensionOrPartName);

    BOOST_CHECK(via_setters == via_ctor);
    BOOST_CHECK(!(via_setters != via_ctor));

    BOOST_CHECK_EQUAL(via_setters.getContentTypeString(), via_ctor.getContentTypeString());
    BOOST_CHECK_EQUAL(via_setters.getExtensionOrPartName(), via_ctor.getExtensionOrPartName());

    BOOST_CHECK_EQUAL(via_setters.toString(), via_ctor.toString());
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace
