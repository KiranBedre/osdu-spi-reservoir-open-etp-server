// ============================================================================
// Copyright 2014-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/System.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace oes::core;

namespace {

BOOST_AUTO_TEST_SUITE(SystemTests)

BOOST_AUTO_TEST_CASE(test_get_time) {
    const double real = System::getRealTime();
    const double user = System::getUserTime();

    BOOST_CHECK(real >= 0);
    BOOST_CHECK(user >= 0);
}

BOOST_AUTO_TEST_CASE(test_get_memory_usage) {
    // Should have proper initialization to zero
    System::MemoryUsed mem;
    BOOST_REQUIRE_EQUAL(int64_t(0), mem.peak_bytes);
    BOOST_REQUIRE_EQUAL(int64_t(0), mem.virtual_bytes);
    BOOST_REQUIRE_EQUAL(int64_t(0), mem.resident_bytes);

    // Should be non-zero when queried
    mem = System::getMemoryUsage();
    BOOST_REQUIRE(mem.peak_bytes >= 0); // can be zero sometimes apparently
    BOOST_REQUIRE(mem.virtual_bytes > 0);
    BOOST_REQUIRE(mem.resident_bytes > 0);

    // Allocate memory by multiples of 10MB up-to 1GB until memory stats budge
    System::MemoryUsed new_mem;
    for (size_t i = 1; i <= 101; i += 10){
        // using i * 10 Mb of memory should be visible
        // or not depending on what tests ran beforehand
        std::vector<char> buffer(i * 10 * 1024 * 1024);
        new_mem = System::getMemoryUsage();
        if (new_mem.virtual_bytes > mem.virtual_bytes){
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    BOOST_CHECK(new_mem.peak_bytes     >= mem.peak_bytes); // depends on what ran before
    BOOST_CHECK(new_mem.virtual_bytes  >  mem.virtual_bytes);
    BOOST_CHECK(new_mem.resident_bytes >  mem.resident_bytes);

    System::MemoryUsed after_mem = System::getMemoryUsage();
    BOOST_CHECK(new_mem.peak_bytes     <= after_mem.peak_bytes); // can decrease still apparently
    BOOST_CHECK(new_mem.virtual_bytes  >  after_mem.virtual_bytes); // should have shrunk?
    BOOST_CHECK(new_mem.resident_bytes >  after_mem.resident_bytes);
}

BOOST_AUTO_TEST_CASE(test_memory_used_peak_mb) {
    System::MemoryUsed mem;
    const int64_t mb = 1024 * 1024;

    mem.peak_bytes = 1 * mb;
    BOOST_REQUIRE_EQUAL(std::string("1 MB"), mem.peakMB());

    mem.peak_bytes = 7 * mb;
    BOOST_REQUIRE_EQUAL(std::string("7 MB"), mem.peakMB());

    mem.peak_bytes = 54 * mb;
    BOOST_REQUIRE_EQUAL(std::string("54 MB"), mem.peakMB());

    mem.peak_bytes = 999 * mb;
    BOOST_REQUIRE_EQUAL(std::string("999 MB"), mem.peakMB());

    mem.peak_bytes = (1000 * mb) - 1; // rounds below, integer division
    BOOST_REQUIRE_EQUAL(std::string("999 MB"), mem.peakMB());

    mem.peak_bytes = 1000 * mb; // thousand separator
    BOOST_REQUIRE_EQUAL(std::string("1,000 MB"), mem.peakMB());

    mem.peak_bytes = 1001 * mb;
    BOOST_REQUIRE_EQUAL(std::string("1,001 MB"), mem.peakMB());

    mem.peak_bytes = 1000 * 1001 * mb; // no million separator
    BOOST_REQUIRE_EQUAL(std::string("1001,000 MB"), mem.peakMB());
}

BOOST_AUTO_TEST_CASE(test_env) {
    const bool value_on_invalid = true;
    String my_test_var = System::getEnv("MY_TEST_VAR");
    BOOST_REQUIRE_EQUAL("", my_test_var);
    BOOST_REQUIRE_EQUAL(false, System::getBoolEnv("MY_TEST_VAR"));
    BOOST_REQUIRE_EQUAL(value_on_invalid, System::getBoolEnv("MY_TEST_VAR", value_on_invalid));

    String test_var("true");
    System::setEnv("MY_TEST_VAR", test_var);
    my_test_var = System::getEnv("MY_TEST_VAR");
    BOOST_REQUIRE_EQUAL(test_var, my_test_var);
    BOOST_REQUIRE_EQUAL(true, System::getBoolEnv("MY_TEST_VAR"));
}


BOOST_AUTO_TEST_CASE(test_search_path) {
    const StringList path_list = {"/home", "/tmp"};
    String search_path_value = System::joinSearchPath(path_list);
#if defined(_WIN32)
    BOOST_REQUIRE_EQUAL("/home;/tmp", search_path_value);
#else
    BOOST_REQUIRE_EQUAL("/home:/tmp", search_path_value);
#endif
    StringList split_path;
    System::splitSearchPath(split_path, search_path_value);
    BOOST_REQUIRE_EQUAL(path_list.size(), split_path.size());
    BOOST_REQUIRE_EQUAL(path_list[0], split_path[0]);
    BOOST_REQUIRE_EQUAL(path_list[1], split_path[1]);

    System::setEnv("MY_SEARCH_PATH", search_path_value);
    StringList search_path;
    System::getSearchPath(search_path, "MY_SEARCH_PATH");
    BOOST_REQUIRE_EQUAL(path_list.size(), search_path.size());
    BOOST_REQUIRE_EQUAL(path_list[0], search_path[0]);
    BOOST_REQUIRE_EQUAL(path_list[1], search_path[1]);
}


BOOST_AUTO_TEST_SUITE_END()

} // namespace
