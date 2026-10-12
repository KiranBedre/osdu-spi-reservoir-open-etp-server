#include <oes/testing/core/AssertTestTools.h>
#include <oes/testing/core/TestConfig.h>
#include <oes/core/utils/System.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <utility>
#include <stdlib.h>


namespace {

BOOST_AUTO_TEST_CASE(test_json_config) {
    {
        oes::core::System::setEnv("JSON_CONFIG_TEST_VAR", "my_test_var_value");
        auto config = oes::testing::TestConfig::make();
        std::string str = config->jsonConfigTestVar();
        BOOST_CHECK_EQUAL(str, "my_test_var_value");
    }
    {
        auto config = oes::testing::TestConfig::make();
        std::string str = config->jsonConfigTestVar();
        BOOST_CHECK_EQUAL(str, "my_test_var_value");
    }
    {
        oes::core::System::setEnv("JSON_CONFIG_TEST_VAR", "another_value");
        auto config = oes::testing::TestConfig::make();
        std::string str = config->jsonConfigTestVar();
        BOOST_CHECK_EQUAL(str, "another_value");
    }
    {
        oes::core::System::setEnv("OES_ENV", "local");
        auto config = oes::testing::TestConfig::make();
        BOOST_CHECK_EQUAL(config->jsonConfigTestVar(), "local_test_var_1");
    }
}

}
