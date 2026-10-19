// ============================================================================
// Copyright 2025 Aspen Technology, Inc. All rights reserved.
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

#include <oes/common/epc/XmlUtils.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <limits>

namespace {

using namespace oes::common::epc;
using namespace std::string_literals;

BOOST_AUTO_TEST_SUITE(XmlUtilsTests)

BOOST_AUTO_TEST_CASE(XmlVersionAndEnconding) {
    XmlDoc mydoc;
    const auto xml_str = mydoc.toString();

    const std::string comp_str = R"(<?xml version="1.0" encoding="UTF-8"?>)";
    BOOST_REQUIRE_EQUAL(xml_str, comp_str);  
}

BOOST_AUTO_TEST_CASE(XmlRootAndBasicAttr) {
    XmlDoc mydoc;
    auto node1 = mydoc.newNode("node001");
    mydoc.setRootNode(node1);
    node1->setAttr("attr001:type_int"s, static_cast<int>(7));
    node1->setAttr("attr002:type_string"s, "seven_string");

    const auto xml_str = mydoc.toString();

    const std::string comp_str = R"(<?xml version="1.0" encoding="UTF-8"?>
<node001 attr001:type_int="7" attr002:type_string="seven_string"/>
)";
    BOOST_REQUIRE_EQUAL(xml_str, comp_str);
}

BOOST_AUTO_TEST_CASE(XmlChildAndParentNodes) {
    const std::string comp_str = R"(<?xml version="1.0" encoding="UTF-8"?>
<node001>
    <node002/>
</node001>
)";
    {
        XmlDoc mydoc;
        auto node1 = mydoc.newNode("node001");
        mydoc.setRootNode(node1);
        auto node2 = mydoc.newNode("node002");
        node1->addChild(node2);

        const auto xml_str = mydoc.toString();
        BOOST_REQUIRE_EQUAL(xml_str, comp_str);

    }
    {
        XmlDoc mydoc;
        auto node1 = mydoc.newNode("node001");
        mydoc.setRootNode(node1);
        auto node2 = mydoc.newNode("node002");
        node2->setParent(node1);

        const auto xml_str = mydoc.toString();

        BOOST_REQUIRE_EQUAL(xml_str, comp_str);
    }
}

BOOST_AUTO_TEST_CASE(XmlSetAttrs) {
    XmlDoc mydoc;
    auto node1 = mydoc.newNode("node001");
    mydoc.setRootNode(node1);
    node1->setAttr("attr001:type_int"s, static_cast<int>(7));
    node1->setAttr("attr002:type_string"s, "seven_string");
    node1->setAttrs({
        {"attr001:type_ccharP", "c_string"},
        {"attr002:type_string", "std::string"s},
        {"attr003:type_int_min", std::numeric_limits<int>::min()},
        {"attr004:type_int_max", std::numeric_limits<int>::max()},
        {"attr005:type_unsigned_min", std::numeric_limits<unsigned>::min()},
        {"attr006:type_unsigned_max", std::numeric_limits<unsigned>::max()},
        {"attr007:type_int64_min", std::numeric_limits<int64_t>::min()},
        {"attr008:type_int64_max", std::numeric_limits<int64_t>::max()},
        {"attr009:type_uint64_min", std::numeric_limits<uint64_t>::min()},
        {"attr009:type_uint64_max", std::numeric_limits<uint64_t>::max()},
        {"attr010:type_bool_t", true},
        {"attr011:type_bool_f", false},
        {"attr012:type_min", std::numeric_limits<double>::min()},
        {"attr013:type_max", std::numeric_limits<double>::max()},
        {"attr014:type_float_min", std::numeric_limits<float>::min()},
        {"attr015:type_float_max", std::numeric_limits<float>::max()},
    });

    const auto xml_str = mydoc.toString();

    const std::string comp_str = R"(<?xml version="1.0" encoding="UTF-8"?>
<node001 attr001:type_int="7" attr002:type_string="std::string" attr001:type_ccharP="c_string" attr003:type_int_min="-2147483648" attr004:type_int_max="2147483647" attr005:type_unsigned_min="0" attr006:type_unsigned_max="4294967295" attr007:type_int64_min="-9223372036854775808" attr008:type_int64_max="9223372036854775807" attr009:type_uint64_min="0" attr009:type_uint64_max="18446744073709551615" attr010:type_bool_t="true" attr011:type_bool_f="false" attr012:type_min="2.2250738585072014e-308" attr013:type_max="1.7976931348623157e+308" attr014:type_float_min="1.1754944e-38" attr015:type_float_max="3.4028235e+38"/>
)";
    BOOST_REQUIRE_EQUAL(xml_str, comp_str);
}

BOOST_AUTO_TEST_CASE(XmlParentsAndChildren) {
    const std::string comp_str = R"(<?xml version="1.0" encoding="UTF-8"?>
<node001 attr001:type_int="7" attr002:type_string="seven_string">
    <node002 attr003:type_double="7.71" attr004:type_charP="some_more_strings">
        <node0003 attr005:type_string="a_bit_more"/>
    </node002>
    <node0004 attr006:type_bool="true" attr007:type_bool="false">
        <node0005>Some strings as value</node0005>
        <node0006>a few more strings as value</node0006>
    </node0004>
</node001>
)";

    {
        XmlDoc mydoc;
        auto node1 = mydoc.newNode("node001");
        mydoc.setRootNode(node1);
        node1->setAttr("attr001:type_int"s, static_cast<int>(7));
        node1->setAttr("attr002:type_string"s, "seven_string");

        auto node2 = mydoc.newNode("node002");
        node1->addChild(node2);

        node2->setAttrs({{"attr003:type_double", static_cast<double>(7.71)},
                         {"attr004:type_charP", "some_more_strings"}});

        auto node3 = mydoc.newNode("node0003");
        node3->setAttr("attr005:type_string", "a_bit_more"s);
        node2->addChild(node3);

        auto node4 = mydoc.newNode("node0004");
        node4->setAttr("attr006:type_bool", true);
        node4->setAttr("attr007:type_bool", false);
        node1->addChild(node4);

        auto node5 = mydoc.newNode("node0005", "Some strings as value");
        node5->setParent(node4);
        auto node6 = mydoc.newNode("node0006");
        node6->setNodeValue("a few more strings as value");
        node4->addChild(node6);

        const auto xml_str = mydoc.toString();
        BOOST_REQUIRE_EQUAL(xml_str, comp_str);
    }
    { // using newNodeMapped
        XmlDoc mydoc;
        mydoc.newNodeMapped("node001", "node1");
        mydoc.setRootNode(mydoc.at("node1"));
        mydoc.at("node1")->setAttr("attr001:type_int"s, static_cast<int>(7));
        mydoc.at("node1")->setAttr("attr002:type_string"s, "seven_string");

        mydoc.newNodeMapped("node002", "node2");
        mydoc.at("node1")->addChild(mydoc.at("node2"));

        mydoc.at("node2")->setAttrs({{"attr003:type_double", static_cast<double>(7.71)},
                                     {"attr004:type_charP", "some_more_strings"}});

        mydoc.newNodeMapped("node0003", "node3");
        mydoc.at("node3")->setAttr("attr005:type_string", "a_bit_more"s);
        mydoc.at("node2")->addChild(mydoc.at("node3"));

        mydoc.newNodeMapped("node0004", "node4");
        mydoc.at("node4")->setAttr("attr006:type_bool", true);
        mydoc.at("node4")->setAttr("attr007:type_bool", false);
        mydoc.at("node1")->addChild(mydoc.at("node4"));

        mydoc.newNodeMapped("node0005", "node5", "Some strings as value");
        mydoc.newNodeMapped("node0006", "node6", "a few more strings as value");

        mydoc.at("node4")->addChild(mydoc.at("node5"));
        mydoc.at("node4")->addChild(mydoc.at("node6"));

        const auto xml_str = mydoc.toString();
        BOOST_REQUIRE_EQUAL(xml_str, comp_str);
    }
}


BOOST_AUTO_TEST_SUITE_END()

}
