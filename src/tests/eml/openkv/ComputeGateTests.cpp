// ============================================================================
// Copyright 2023-2026 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/eml/openkv/ComputeGate.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <vector>

namespace
{
    using namespace oes::eml::openkv;

    // Reset the shared in-flight counter so each test starts from a known
    // state (the tests in this binary run sequentially).
    struct ComputeGateFixture {
        ComputeGateFixture()  { g_compute_inflight.store(0); }
        ~ComputeGateFixture() { g_compute_inflight.store(0); }
    };

    BOOST_FIXTURE_TEST_SUITE(ComputeGateTests, ComputeGateFixture)

    // ---- parse_compute_max_concurrency -------------------------------------
    // The cap is ON by default: unset / empty / malformed values yield the
    // default, and only an explicit "0" opts out (unlimited).

    BOOST_AUTO_TEST_CASE(Parse_UnsetOrEmpty_IsDefault)
    {
        BOOST_CHECK_EQUAL(kDefaultComputeMaxConcurrency, 4);
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency(nullptr), kDefaultComputeMaxConcurrency);
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency(""), kDefaultComputeMaxConcurrency);
    }

    BOOST_AUTO_TEST_CASE(Parse_ValidPositive)
    {
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("1"), 1);
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("4"), 4);
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("100000"), 100000);
    }

    BOOST_AUTO_TEST_CASE(Parse_ZeroIsUnlimited)
    {
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("0"), 0);
    }

    BOOST_AUTO_TEST_CASE(Parse_InvalidIsDefault)
    {
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("-1"), kDefaultComputeMaxConcurrency);     // negative
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("abc"), kDefaultComputeMaxConcurrency);    // non-numeric
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("4x"), kDefaultComputeMaxConcurrency);     // trailing junk
        BOOST_CHECK_EQUAL(parse_compute_max_concurrency("100001"), kDefaultComputeMaxConcurrency); // over the cap
    }

    // ---- ComputeGate acquisition -------------------------------------------

    BOOST_AUTO_TEST_CASE(Gate_Unlimited_AlwaysAcquires)
    {
        ComputeGate a(0);
        ComputeGate b(0);
        ComputeGate c(-5); // negative also means unlimited
        BOOST_CHECK(a.acquired_);
        BOOST_CHECK(b.acquired_);
        BOOST_CHECK(c.acquired_);
        BOOST_CHECK_EQUAL(g_compute_inflight.load(), 3);
    }

    BOOST_AUTO_TEST_CASE(Gate_RejectsBeyondCap)
    {
        const int cap = 2;
        ComputeGate a(cap);
        ComputeGate b(cap);
        ComputeGate c(cap); // third must be rejected
        BOOST_CHECK(a.acquired_);
        BOOST_CHECK(b.acquired_);
        BOOST_CHECK(!c.acquired_);
        BOOST_CHECK_EQUAL(g_compute_inflight.load(), 2);
    }

    BOOST_AUTO_TEST_CASE(Gate_ReleasesOnDestruction)
    {
        const int cap = 1;
        {
            ComputeGate a(cap);
            BOOST_CHECK(a.acquired_);
            BOOST_CHECK_EQUAL(g_compute_inflight.load(), 1);

            ComputeGate b(cap); // cap reached
            BOOST_CHECK(!b.acquired_);
            BOOST_CHECK_EQUAL(g_compute_inflight.load(), 1);
        }
        // Both tokens destroyed; 'a' released its slot, 'b' held none.
        BOOST_CHECK_EQUAL(g_compute_inflight.load(), 0);

        // A fresh request now fits again.
        ComputeGate c(cap);
        BOOST_CHECK(c.acquired_);
        BOOST_CHECK_EQUAL(g_compute_inflight.load(), 1);
    }

    BOOST_AUTO_TEST_CASE(Gate_RejectedTokenDoesNotReleaseOthersSlot)
    {
        const int cap = 1;
        ComputeGate a(cap);
        BOOST_CHECK(a.acquired_);
        {
            ComputeGate rejected(cap);
            BOOST_CHECK(!rejected.acquired_);
        } // destroying a non-acquired token must NOT decrement the counter
        BOOST_CHECK_EQUAL(g_compute_inflight.load(), 1);
        BOOST_CHECK(a.acquired_);
    }

    // ---- parse_compute_max_runtime_ms / ComputeDeadline ---------------------

    BOOST_AUTO_TEST_CASE(Runtime_UnsetOrEmpty_IsDefault)
    {
        BOOST_CHECK_EQUAL(kDefaultComputeMaxRuntimeMs, 60000L);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms(nullptr), kDefaultComputeMaxRuntimeMs);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms(""), kDefaultComputeMaxRuntimeMs);
    }

    BOOST_AUTO_TEST_CASE(Runtime_ValidAndZero)
    {
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("1"), 1L);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("250"), 250L);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("86400000"), 86400000L);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("0"), 0L); // explicit opt-out
    }

    BOOST_AUTO_TEST_CASE(Runtime_InvalidIsDefault)
    {
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("-1"), kDefaultComputeMaxRuntimeMs);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("abc"), kDefaultComputeMaxRuntimeMs);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("10ms"), kDefaultComputeMaxRuntimeMs);
        BOOST_CHECK_EQUAL(parse_compute_max_runtime_ms("86400001"), kDefaultComputeMaxRuntimeMs);
    }

    BOOST_AUTO_TEST_CASE(Deadline_ZeroBudgetNeverExpires)
    {
        ComputeDeadline d(0);
        BOOST_CHECK(!d.expired());
    }

    BOOST_AUTO_TEST_CASE(Deadline_ExpiresAfterBudget)
    {
        ComputeDeadline far(60000);
        BOOST_CHECK(!far.expired());
        ComputeDeadline soon(1);
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (!soon.expired() && std::chrono::steady_clock::now() < until) {
        }
        BOOST_CHECK(soon.expired());
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace
