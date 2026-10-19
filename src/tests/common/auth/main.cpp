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

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

#include <oes/core/utils/System.h>

#include <string>

static std::string arg0__;

namespace oes_test_exe_main {
    std::string arg0() { return arg0__; }
}

int main(int argc, char* argv[]) {
    arg0__ = *argv;

    // Only necessary on Linux, but doesn't hurt on Windows.
    // See also http://stackoverflow.com/questions/5325202
    oes::core::System::setEnv("BOOST_TEST_CATCH_SYSTEM_ERRORS", "no");

    oes::testing::ScopedTimer timer("*** tests_oes_common_auth_lib", true);
    return oes::testing::TestMain::main(argc, argv);
}
