// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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
#pragma once

#include <string>

#include <oes/core/utils/AppConfigReader.h>
#include <oes/eml/OpenETPServerConfig.h>
#include <oes/testing/core/export.h>

namespace oes::testing {

class OES_TESTING_CORE_EXPORT TestConfig: public oes::eml::openETPServerConfig {
public:
    TestConfig();
    ~TestConfig() = default;

    static std::shared_ptr<TestConfig> make();

    // Testing
    const std::string& oesProjectBuildDir() const noexcept;
    const std::string& oesExecutePerfTest() const noexcept;
    const std::string& oesExecuteManualTest() const noexcept;
    const std::string& unitTestsDataPath() const noexcept;
    const std::string& oesPostgresqlUtconfig() const noexcept;
    // plain test
    const std::string& jsonConfigTestVar() const noexcept;
    const std::string& oesJsonAdhocHandlerTestDataDir() const noexcept;
private:
    // Testing
    std::string oesProjectBuildDir_;
    std::string oesExecutePerfTest_;
    std::string oesExecuteManualTest_;
    std::string unitTestsDataPath_;
    std::string oesPostgresqlUtconfig_;
    // plain test
    std::string jsonConfigTestVar_;
    std::string oesJsonAdhocHandlerTestDataDir_;
    
};

using TestConfig_ptr = std::shared_ptr<TestConfig>;

} // oes::app::eml