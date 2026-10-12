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

#include <oes/testing/eml/ConsoleUtils.h>
#include <oes/testing/core/TestMain.h>

using namespace oes::testing;
using namespace oes::testing::eml;

namespace {

//*****************************************************************************

BOOST_AUTO_TEST_SUITE(Hdf5CmdsTests)

BOOST_AUTO_TEST_CASE(test_info) {
    const std::string& h5_input = resolveTestDataFile(
        "data/eml", "WELL_RESQML.h5", true
    );

    OESEmlRunner runner;
    runner.startAndWait({ "hdf5", "--info", h5_input });
    BOOST_REQUIRE(runner.noErrors());
    BOOST_CHECK(runner.outputLinesMatch({
        "^4 Groups.*",
        "^4 Datasets.*",
        "^1 Group attribute.*",
        "^280 Group header bytes.*",
        "^1,088 Dataset header bytes.*",
        "^3,968 Group \\(index & heap\\) bytes.*",
        "^9,424 Dataset \\(index & heap\\) bytes.*",
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Dataset 'overhead' size - Top 10.*"
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^4 \\(2,048 - 4,096\\] bytes.*"
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Datasets by name - Top 10.*",
        "^1 controlPointParameters.*",
        "^1 controlPoints.*",
        "^1 mdValues.*",
        "^1 tangentVectors.*",
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Dataset 'overhead' size - Top 10.*",
        "^2,616 RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors.*",
        "^2,616 RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints.*",
        "^2,096 RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters.*",
        "^2,096 RESQML/c76021a6-c1e7-5b2d-848a-35764da8001d/mdValues.*",
    }));
}

BOOST_AUTO_TEST_CASE(test_stats) {
    const std::string& h5_input = oes::testing::resolveTestDataFile(
        "data/eml", "WELL_RESQML.h5", true
    );

    OESEmlRunner runner;
    runner.startAndWait({ "hdf5", "--stats", h5_input });
    BOOST_REQUIRE(runner.noErrors());
    BOOST_CHECK(runner.outputLinesMatch({
        "^204 Dataset stored bytes.*",
        "^200 Dataset bytes un-chunked & un-compressed.*",
        "^1\\.0x Compression ratio \\(exclude metadata overhead\\).*",
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Datasets by rank.*",
        "^2 1D.*",
        "^2 2D.*",
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Datasets by datatype class.*",
        "^4 Floating-Point.*",
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Datasets by rank & numeric type.*",
        "^2 1D - double.*",
        "^2 2D - double.*",
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Datasets by 'storage' size-range.*",
    }));

    BOOST_CHECK(runner.outputLinesMatch({
        "^2 \\( 16 -  32\\] bytes.*",
    }));

    BOOST_CHECK(runner.outputLinesMatch({
        "^1 \\( 32 -  64\\] bytes.*",
    }));

    BOOST_CHECK(runner.outputLinesMatch({
        "^1 \\( 64 - 128\\] bytes.*",
    }));
    BOOST_CHECK(runner.outputLinesMatch({
        "^<<< Dataset 'storage' size - Top 10.*",
        "^92 RESQML/c76021a6-c1e7-5b2d-848a-35764da8001d/mdValues.*",
        ".*1D - 11 - double \\(88 bytes, 1\\.0x\\)",
        "^57 RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints.*",
        ".*2D - 2 x 3 - double \\(48 bytes, 0\\.8x\\)",
        "^31 RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors.*",
        ".*2D - 2 x 3 - double \\(48 bytes, 1\\.5x\\)",
        "^24 RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters.*",
        ".*1D - 2 - double \\(16 bytes, 0\\.7x\\).*",
    }));
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
