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

#include <oes/eml/openkv/NumericType.h>
#include <oes/eml/openkv/ArrayStats.h> // numericElemSize

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cstdint>
#include <string>
#include <type_traits>

namespace
{
    using namespace oes::eml::openkv;
    using oes::common::hdf::HdfAryType;

    //! Capture the C++ type the visitor dispatched to, as a stable tag string, so we
    //! can assert the runtime HdfAryType -> compile-time type mapping without relying
    //! on implementation details of the kernels that consume it.
    template <typename T>
    const char *typeTag()
    {
        if (std::is_same<T, int32_t>::value) return "i32";
        if (std::is_same<T, int64_t>::value) return "i64";
        if (std::is_same<T, float>::value)   return "f32";
        if (std::is_same<T, double>::value)  return "f64";
        if (std::is_same<T, int8_t>::value)  return "i8";
        return "?";
    }

    BOOST_AUTO_TEST_SUITE(NumericTypeTests)

    // ---- numeric types map to the expected C++ type and visit exactly once -----

    BOOST_AUTO_TEST_CASE(Visit_NumericTypes_MapToExpectedCppType)
    {
        struct Case { HdfAryType type; const char *tag; };
        const Case cases[] = {
            {oes::common::hdf::eAryOfInt,     "i32"},
            {oes::common::hdf::eAryOfLong,    "i64"},
            {oes::common::hdf::eAryOfFloat,   "f32"},
            {oes::common::hdf::eAryOfDouble,  "f64"},
            {oes::common::hdf::eAryOfBoolean, "i8"},
        };

        for (const auto &c : cases)
        {
            int calls = 0;
            std::string seen;
            const bool numeric = visitNumericType(c.type, [&](auto tag) {
                ++calls;
                seen = typeTag<decltype(tag)>();
            });
            BOOST_CHECK_MESSAGE(numeric, "type " << c.type << " should be numeric");
            BOOST_CHECK_EQUAL(calls, 1); // called exactly once
            BOOST_CHECK_EQUAL(seen, std::string(c.tag));
        }
    }

    // ---- non-numeric types never call the visitor and return false -------------

    BOOST_AUTO_TEST_CASE(Visit_NonNumericTypes_SkipAndReturnFalse)
    {
        const HdfAryType non_numeric[] = {
            oes::common::hdf::eAryOfBytes,
            oes::common::hdf::eAryOfString,
            oes::common::hdf::eAryOfVoid,
        };

        for (HdfAryType t : non_numeric)
        {
            int calls = 0;
            const bool numeric = visitNumericType(t, [&](auto) { ++calls; });
            BOOST_CHECK_MESSAGE(!numeric, "type " << t << " should be non-numeric");
            BOOST_CHECK_EQUAL(calls, 0); // f is never invoked
        }
    }

    // ---- the visitor is the single source of truth behind numericElemSize ------

    BOOST_AUTO_TEST_CASE(Visit_ElemSize_MatchesNumericElemSize)
    {
        const HdfAryType all[] = {
            oes::common::hdf::eAryOfInt,
            oes::common::hdf::eAryOfLong,
            oes::common::hdf::eAryOfFloat,
            oes::common::hdf::eAryOfDouble,
            oes::common::hdf::eAryOfBoolean,
            oes::common::hdf::eAryOfBytes,
            oes::common::hdf::eAryOfString,
            oes::common::hdf::eAryOfVoid,
        };

        for (HdfAryType t : all)
        {
            size_t via_visitor = 0;
            const bool numeric = visitNumericType(t, [&](auto tag) {
                via_visitor = sizeof(decltype(tag));
            });
            // Non-numeric -> 0 (visitor never ran); numeric -> the mapped width. Either
            // way it must agree with the public numericElemSize() the kernels size on.
            BOOST_CHECK_EQUAL(via_visitor, numericElemSize(t));
            if (!numeric)
                BOOST_CHECK_EQUAL(via_visitor, 0u);
        }

        // Spot-check the concrete widths so a wrong-type mapping can't slip through.
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfInt), sizeof(int32_t));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfLong), sizeof(int64_t));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfFloat), sizeof(float));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfDouble), sizeof(double));
        BOOST_CHECK_EQUAL(numericElemSize(oes::common::hdf::eAryOfBoolean), sizeof(int8_t));
    }

    BOOST_AUTO_TEST_SUITE_END()

} // namespace
