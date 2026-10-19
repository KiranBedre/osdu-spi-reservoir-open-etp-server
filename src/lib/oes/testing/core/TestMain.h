// ============================================================================
// Copyright 2012-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_TESTING_CORE_TESTMAIN_H
#define OES_TESTING_CORE_TESTMAIN_H

#include <oes/testing/core/export.h>

#include <string>
#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/noncopyable.hpp>

#define BOOST_TEST_ALTERNATIVE_INIT_API
#define BOOST_TEST_NO_MAIN  1 // Disable the automatic main generation
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes {
namespace testing {

OES_TESTING_CORE_EXPORT std::string test_case_name();
OES_TESTING_CORE_EXPORT std::string test_suite_name();

class OES_TESTING_CORE_EXPORT TestMain : boost::noncopyable {
public:
    static int main(int argc, char* argv[]);
    static int argc ();
    static char** argv ();
    static std::string get_exe_path();
};

OES_TESTING_CORE_EXPORT bool arePerformanceTestsEnabled();
OES_TESTING_CORE_EXPORT bool areManualTestsEnabled();

template <typename T>
T perfValues(T small_value, T large_value) {
    if (arePerformanceTestsEnabled()) {
        return large_value;
    }
    return small_value;
}

OES_TESTING_CORE_EXPORT
std::string resolveTestDataFile(
    const std::string& psc_relative_path,
    const std::string& name,
    bool is_input_file = true,
    const std::string& env_var_override = ""
);

OES_TESTING_CORE_EXPORT
bool readFile(const std::string& filename, std::vector<char>& buffer);

} // End namespace testing
} // End namespace oes

#define OES_TEST_MAIN()\
    int main(int argc, char* argv[]) {\
        return oes::testing::TestMain::main(argc, argv);\
    }

#define OES_TEST_CHECK_PERF_ENABLE()\
    if (!oes::testing::arePerformanceTestsEnabled()) { \
        OES_INFO("SKIPPED " + boost::unit_test::framework::current_test_case().p_name.value + ": Performance tests disabled."); \
        return; \
    }

#define OES_TEST_CHECK_MANUAL_ENABLE()\
    if (!oes::testing::areManualTestsEnabled()) { \
        OES_INFO("SKIPPED " + boost::unit_test::framework::current_test_case().p_name.value + ": Manual tests disabled."); \
        return; \
    }

#if defined (OES_DEBUG_BUILD)
#define OES_CHECK_EQUAL_DBG_OR_REL(expected_debug, expected_release, value)\
    BOOST_CHECK_EQUAL(expected_debug, value)
#else
#define OES_CHECK_EQUAL_DBG_OR_REL(expected_debug, expected_release, value)\
    BOOST_CHECK_EQUAL(expected_release, value)
#endif

#endif
