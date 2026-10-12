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

#include <oes/common/AvroUtils.hpp>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

#include <limits>

using namespace oes::common;

namespace {

template <typename T>
void do_test_encodeBinaryVector(const std::string& label) {
    const bool perf = oes::testing::arePerformanceTestsEnabled();

    const size_t chunk_size = 4*1024; // What Avro uses
#ifdef OES_DEBUG_BUILD
    const size_t max_size =  25*1024*1024; //  25 MiB faster for debug
#else
    const size_t max_size = 100*1024*1024; // 100 MiB large enough for benches
#endif

    std::vector<std::string> official_msgs;
    std::vector<std::string> custom_msgs;

    std::string msg_str;
    StringAsAvroOStreamView aos(msg_str, chunk_size, max_size);

    std::vector<size_t> vec_sizes{ 0, 1, 5, 7, 8, 9, 14, 15, 16, 17, 231 };
    if (perf) {
        vec_sizes.emplace_back(max_size / sizeof(T));
    }

    // create the vectors outside timers
    using Vec = std::vector<T>;
    std::vector<Vec> vecs;
    for (size_t n : vec_sizes) {
        vecs.emplace_back(n);
        Vec& vec = vecs.back();
        for (size_t i = 0; i < n; ++i) {
            vec[i] = T(i);
        }

        if (n > 100) {
            // Add special values, to tease out bugs
            size_t idx = n;
            vec[--idx] = std::numeric_limits<T>::max();
            vec[--idx] = std::numeric_limits<T>::min();
            vec[--idx] = std::numeric_limits<T>::lowest();
            vec[--idx] = std::numeric_limits<T>::epsilon();
            vec[--idx] = std::numeric_limits<T>::round_error();

            // Some software puts 2^31-1 values in long arrays.
            // So also force special *int* values into arrays too.
            // (conversion to float may approximate those, that's OK)
            vec[--idx] = T(std::numeric_limits<int>::max());
            vec[--idx] = T(std::numeric_limits<int>::min());
            vec[--idx] = T(std::numeric_limits<int>::lowest());
            vec[--idx] = T(std::numeric_limits<int>::epsilon());
            vec[--idx] = T(std::numeric_limits<int>::round_error());
        }
    }

    {
        oes::testing::ScopedTimer timer("Official encoder: " + label, true);

        for (const Vec& vec : vecs) {
            const ::avro::EncoderPtr& enc = ::avro::binaryEncoder();
            enc->init(aos);
            ::avro::encode(*enc, vec);
            enc->flush();

            official_msgs.emplace_back(std::move(msg_str));
            msg_str.clear();
        }

        if (!perf) {
            timer.reset(); // output unless benchmarking
        }
    }

    {
        oes::testing::ScopedTimer timer("Custom encoder: " + label, true);

        for (const Vec& vec : vecs) {
            PassThruEncoder enc(::avro::binaryEncoder(), /*binary*/true);
            enc.init(aos);
            ::avro::encode(enc, vec);
            enc.flush();

            custom_msgs.emplace_back(std::move(msg_str));
            msg_str.clear();
        }

        if (!perf) {
            timer.reset(); // output unless benchmarking
        }
    }

    if (!perf) {
        BOOST_REQUIRE_EQUAL(official_msgs.size(), custom_msgs.size());
        for (size_t i = 0; i < official_msgs.size(); ++i) {
            BOOST_CHECK_EQUAL(official_msgs[i], custom_msgs[i]);
        }
    }
}

// Note: There's another test-suite with the same name in Etp12MessagesTests.cpp
BOOST_AUTO_TEST_SUITE(AvroUtilsTests)

// Note: in Etp12MessagesTests.cpp, there's AvroUtilsTests/test_arrays too.
// That test is not moved here, to avoid duplication of helper code it needs,
// which is not specific to that test-case.

BOOST_AUTO_TEST_CASE(test_encodeBinaryIntVector) {
    do_test_encodeBinaryVector<int>("int");
}

BOOST_AUTO_TEST_CASE(test_encodeBinaryLongVector) {
    do_test_encodeBinaryVector<int64_t>("long");
}

BOOST_AUTO_TEST_CASE(test_encodeBinaryFloatVector) {
    do_test_encodeBinaryVector<float>("float");
}

BOOST_AUTO_TEST_CASE(test_encodeBinaryDoubleVector) {
    do_test_encodeBinaryVector<double>("double");
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
