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

#include <oes/core/utils/Utf8.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace {

BOOST_AUTO_TEST_SUITE(Utf8Tests)

// Test C++ API
BOOST_AUTO_TEST_CASE(test_convert_cpp) {
    // UFT8 encoded: "Hello" in Russian
    const std::string utf8_string = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
    const std::wstring wstring = oes::core::utf8::fromUtf8ToWstr(utf8_string);
    const std::string converted_string = oes::core::utf8::fromWstrToUtf8(wstring);
    BOOST_CHECK_EQUAL(utf8_string, converted_string);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace