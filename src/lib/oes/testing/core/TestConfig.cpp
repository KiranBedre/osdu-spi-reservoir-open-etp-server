#include "TestConfig.h"

#include <oes/core/utils/AppConfigReader.h>

namespace oes::testing {

TestConfig::TestConfig()
    : oes::eml::openETPServerConfig("tests_config.json") {
    // Testing
    // cppcheck-suppress useInitializationList
    oesProjectBuildDir_ = env_map_ptr_->at("/testing/oesprojectbuilddir");
    oesExecutePerfTest_ = env_map_ptr_->at("/testing/oesexecuteperftest");
    oesExecuteManualTest_ = env_map_ptr_->at("/testing/oesexecutemanualtest");
    unitTestsDataPath_ = env_map_ptr_->at("/testing/unittestsdatapath");
    oesPostgresqlUtconfig_ = env_map_ptr_->at("/testing/oespostgresqlutconfig");
    // plain test
    jsonConfigTestVar_ = env_map_ptr_->at("/plaintests/jsonconfigtestvar");
    oesJsonAdhocHandlerTestDataDir_ = env_map_ptr_->at("/plaintests/oesjsonadhochandlertestdatadir");
    env_map_ptr_.reset();
}

std::shared_ptr<TestConfig> TestConfig::make() {
    return std::make_shared<TestConfig>();
}

// Testing
const std::string& TestConfig::oesProjectBuildDir() const noexcept {
    return oesProjectBuildDir_;
}
const std::string& TestConfig::oesExecutePerfTest() const noexcept {
    return oesExecutePerfTest_;
}
const std::string& TestConfig::oesExecuteManualTest() const noexcept {
    return oesExecuteManualTest_;
}
const std::string& TestConfig::unitTestsDataPath() const noexcept {
    return unitTestsDataPath_;
}
const std::string& TestConfig::oesPostgresqlUtconfig() const noexcept {
    return oesPostgresqlUtconfig_;
}
// plain test
const std::string& TestConfig::jsonConfigTestVar() const noexcept {
    return jsonConfigTestVar_;
}
const std::string& TestConfig::oesJsonAdhocHandlerTestDataDir() const noexcept {
    return oesJsonAdhocHandlerTestDataDir_;
}
} // oes::app::eml