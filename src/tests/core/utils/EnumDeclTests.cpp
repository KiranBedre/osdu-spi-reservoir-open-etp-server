// ============================================================================
// Copyright 2013-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

/*!
 * Tests LogicalSchema to PhysicalSchema conversion via PhysicalSchemaGenerator.
 */
#include <oes/core/utils/EnumDecl.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

using namespace oes::core;

/*****************************************************************************/

OES_DEFINE_ENUM(MyGlobalScopeEnum, -1, (Foo)(Bar));

namespace acme {
    OES_DEFINE_ENUM(MyNamespaceScopeEnum, -1, (Jack)(Jill));

    struct AcmeBuilder {
        OES_DEFINE_ENUM(MyClassScopeEnum, -1, (Cat)(Dog));
    };
} // namespace acme

namespace {
    OES_DEFINE_ENUM(MyOsType, -1, (Linux)(Apple)(Windows));

    struct MyProject {
        OES_DEFINE_ENUM(
            MyDomain, Unknown,
            (Unknown)(Time)(Depth)(TimeMigrated)
        );
    };
} // End anonymous namespace

OES_DEFINE_ENUM_TRAITS(MyGlobalScopeEnum);
OES_DEFINE_ENUM_TRAITS(acme::MyNamespaceScopeEnum);
OES_DEFINE_ENUM_TRAITS(acme::AcmeBuilder::MyClassScopeEnum);
OES_DEFINE_ENUM_TRAITS(MyOsType);
OES_DEFINE_ENUM_TRAITS(MyProject::MyDomain);

/*****************************************************************************/
/*****************************************************************************/

namespace {

BOOST_AUTO_TEST_SUITE(oes_core_utils_EnumDecl)

/*****************************************************************************/

BOOST_AUTO_TEST_CASE(test_name_from_traits) {
    BOOST_CHECK_EQUAL("MyGlobalScopeEnum", MyGlobalScopeEnumEnumTraits::name());
    BOOST_CHECK_EQUAL("MyNamespaceScopeEnum", acme::MyNamespaceScopeEnumEnumTraits::name());
    BOOST_CHECK_EQUAL("MyClassScopeEnum", acme::AcmeBuilder::MyClassScopeEnumEnumTraits::name());

    BOOST_CHECK_EQUAL("MyOsType", MyOsTypeEnumTraits::name());
    BOOST_CHECK_EQUAL("MyDomain", MyProject::MyDomainEnumTraits::name());
}


BOOST_AUTO_TEST_CASE(test_name_from_helper) {
    BOOST_CHECK_EQUAL("MyGlobalScopeEnum", enum_name_of<MyGlobalScopeEnum>());
    BOOST_CHECK_EQUAL("MyNamespaceScopeEnum", enum_name_of<acme::MyNamespaceScopeEnum>());
    BOOST_CHECK_EQUAL("MyClassScopeEnum", enum_name_of<acme::AcmeBuilder::MyClassScopeEnum>());

    BOOST_CHECK_EQUAL("MyOsType", enum_name_of<MyOsType>());
    BOOST_CHECK_EQUAL("MyDomain", enum_name_of<MyProject::MyDomain>());
}

/*****************************************************************************/

BOOST_AUTO_TEST_CASE(test_enum_top_level) {
    BOOST_CHECK_EQUAL("Apple", MyOsTypeEnumTraits::toString(Apple));
    BOOST_CHECK_EQUAL(Apple, MyOsTypeEnumTraits::fromString("Apple"));
    BOOST_CHECK_EQUAL(false, MyOsTypeEnumTraits::hasInvalid());
    BOOST_CHECK_THROW(MyOsTypeEnumTraits::invalid(), std::runtime_error);

    // same as above, using template helpers
    BOOST_CHECK_EQUAL("Apple", enum_string_of(Apple));
    BOOST_CHECK_EQUAL(Apple, enum_value_of<MyOsType>("Apple"));

    const std::vector<MyOsType> os_values = enum_values_of<MyOsType>();
    BOOST_CHECK_EQUAL(size_t(3), os_values.size());
    BOOST_CHECK_EQUAL(Linux, os_values[0]);
    BOOST_CHECK_EQUAL(Apple, os_values[1]);
    BOOST_CHECK_EQUAL(Windows, os_values[2]);

    // invalid values (codes or strings)
    BOOST_CHECK_EQUAL("", enum_string_of(static_cast<MyOsType>(127)));
    // No invalid enum value, so throws on enum_value_of()
    BOOST_CHECK_THROW(enum_value_of<MyOsType>("Billy"), std::runtime_error);
}

/*****************************************************************************/

BOOST_AUTO_TEST_CASE(test_enum_nested_type) {
    BOOST_CHECK_EQUAL("Depth", MyProject::MyDomainEnumTraits::toString(MyProject::Depth));
    BOOST_CHECK_EQUAL(MyProject::Depth, MyProject::MyDomainEnumTraits::fromString("Depth"));
    BOOST_CHECK_EQUAL(true, MyProject::MyDomainEnumTraits::hasInvalid());
    BOOST_CHECK_EQUAL(MyProject::Unknown, MyProject::MyDomainEnumTraits::invalid());

    // same as above, using template helpers
    BOOST_CHECK_EQUAL("Depth", enum_string_of(MyProject::Depth));
    BOOST_CHECK_EQUAL(MyProject::Depth, enum_value_of<MyProject::MyDomain>("Depth"));

    const std::vector<MyProject::MyDomain> values = enum_values_of<MyProject::MyDomain>();
    BOOST_CHECK_EQUAL(size_t(4), values.size());
    BOOST_CHECK_EQUAL(MyProject::Unknown, values[0]);
    BOOST_CHECK_EQUAL(MyProject::Time, values[1]);
    BOOST_CHECK_EQUAL(MyProject::Depth, values[2]);
    BOOST_CHECK_EQUAL(MyProject::TimeMigrated, values[3]);

    // invalid values (codes or strings)
    BOOST_CHECK_EQUAL("", enum_string_of(static_cast<MyProject::MyDomain>(127)));
    // Has invalid enum value, so just returns that one on enum_value_of()
    BOOST_CHECK_EQUAL(MyProject::Unknown, enum_value_of<MyProject::MyDomain>("Billy"));
}

/*****************************************************************************/

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace

