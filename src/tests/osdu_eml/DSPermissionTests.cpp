// ============================================================================
// Copyright 2024 Aspen Technology, Inc. All rights reserved.
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

#include <oes/osdu/api/DSPermission.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <chrono>
#include <thread>

namespace {

using namespace oes::osdu::api;

BOOST_AUTO_TEST_SUITE(DSPermissionTests)

BOOST_AUTO_TEST_CASE(DSPermission_constructors) {
    {
        const DSPermission p1 (DSPermissionFlags::eUpdate, false);
        const DSPermission p2 ("100", false); // update
        const DSPermission p3 (4, false); // update

        BOOST_CHECK_EQUAL(p1, p2);
        BOOST_CHECK_EQUAL(p2, p3);
    }
}

BOOST_AUTO_TEST_CASE(DSPermission_constructor_exception) {
    std::string msg ("DSPermission::count_ must not be used.");
    auto check_message = [&msg](const std::logic_error& ex) -> bool {
        BOOST_CHECK_EQUAL(ex.what(), msg);
        return true;
    };

    BOOST_CHECK_EXCEPTION(
        DSPermission(DSPermissionFlags::count_, false),
        std::logic_error,
        check_message
    );

    msg = "DSPermission must be initialzied with a valid value";
    auto invalid_num = BitsetType(std::string(FlagsNum, '1')).to_ulong() + 1;

    BOOST_CHECK_EXCEPTION(
        DSPermission(static_cast<IType>(invalid_num), false),
        std::logic_error,
        check_message
    );
}

BOOST_AUTO_TEST_CASE(DSPermission_permissions)
{
    bool expected {false};
    
    const auto all_flags = getAllPermissionFlags();
    for(const auto& flag : all_flags) {
        DSPermission p(flag, true);
        for(const auto& check_flag : all_flags) {
            auto res = p.hasPermission(check_flag);

            // Check if both hasPermission has the same behavior
            auto res2 = p.hasPermission(DSPermission(check_flag, false));
            BOOST_CHECK(res == res2);

            expected = (flag == check_flag);
            BOOST_CHECK_EQUAL(res, expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(DSPermission_add_clear_permissions) {
    auto p = DSPermission::makeNonePermission();
        
    bool expected {false};
    for (const auto& flag : getAllPermissionFlags()) {
        BOOST_CHECK_EQUAL(p.hasPermission(flag), expected);
    }

    p.addPermission(DSPermissionFlags::eDelete, true);
    for (const auto& flag : getAllPermissionFlags()) {
        if (flag == DSPermissionFlags::eDelete) {
            expected = true;
        } else {
            expected = false;
        }

        BOOST_CHECK_EQUAL(p.hasPermission(flag), expected);
    }

    p.clearPermission(DSPermissionFlags::eDelete);
    p.addPermission(DSPermissionFlags::eRead, true);
    for (const auto& flag : getAllPermissionFlags()) {
        if (flag == DSPermissionFlags::eRead) {
            expected = true;
        } else {
            expected = false;
        }

        BOOST_CHECK_EQUAL(p.hasPermission(flag), expected);
    }

    p.addPermission(DSPermissionFlags::eCreate, true);
    p.addPermission(DSPermissionFlags::eDelete, true);
    p.addPermission(DSPermissionFlags::eUpdate, true);
    expected = true;
    for (const auto& flag : getAllPermissionFlags()) {
        BOOST_CHECK_EQUAL(p.hasPermission(flag), expected);
    }
    
    p.clearAllPermission();
    expected = false;
    for (const auto& flag : getAllPermissionFlags()) {
        BOOST_CHECK_EQUAL(p.hasPermission(flag), expected);
    }
}

BOOST_AUTO_TEST_CASE(Cached_permissions) {
    DSPermission::setCacheExpirationTime(std::chrono::seconds(1));

    DSPermission p(DSPermissionFlags::eUpdate, true);
        
    BOOST_CHECK_EQUAL(p.isValid(DSPermissionFlags::eUpdate), true);
    BOOST_CHECK_EQUAL(p.isValid(DSPermissionFlags::eDelete), false);
    BOOST_CHECK_EQUAL(p.hasPermission(DSPermissionFlags::eUpdate), true);
    BOOST_CHECK_EQUAL(p.hasPermission(DSPermissionFlags::eDelete), false);

    // check after expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));
    BOOST_CHECK_EQUAL(p.isValid(DSPermissionFlags::eUpdate), false);
    BOOST_CHECK_EQUAL(p.hasPermission(DSPermissionFlags::eUpdate), false);
    
}

BOOST_AUTO_TEST_CASE(Cached_permissions_add_with_cache) {
    DSPermission::setCacheExpirationTime(std::chrono::seconds(1));

    DSPermission p(DSPermissionFlags::eUpdate, false);
    p.addPermission(DSPermissionFlags::eDelete, true);
    
    BOOST_CHECK_EQUAL(p.isValid(DSPermissionFlags::eUpdate), false);
    BOOST_CHECK_EQUAL(p.isValid(DSPermissionFlags::eDelete), true);
    BOOST_CHECK_EQUAL(p.hasPermission(DSPermissionFlags::eUpdate), false);
    BOOST_CHECK_EQUAL(p.hasPermission(DSPermissionFlags::eDelete), true);

    // Check after expiration
    std::this_thread::sleep_for(std::chrono::seconds(2));
    BOOST_CHECK_EQUAL(p.isValid(DSPermissionFlags::eUpdate), false);
    BOOST_CHECK_EQUAL(p.isValid(DSPermissionFlags::eDelete), false);
    BOOST_CHECK_EQUAL(p.hasPermission(DSPermissionFlags::eUpdate), false);
    BOOST_CHECK_EQUAL(p.hasPermission(DSPermissionFlags::eDelete), false);
}

BOOST_AUTO_TEST_SUITE_END()

}
