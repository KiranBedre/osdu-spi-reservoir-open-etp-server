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

#include <iostream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/common/epc/FileContentType.h>

#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>

#include <oes/testing/core/TestMain.h>


namespace {

using oes::core::Guid;
using oes::testing::test_case_name;
using SimpleTimer = oes::core::HighResTimer;
using namespace oes::common;

//============================================================================

struct Throughput {
    size_t count_ = 0; size_t rels_ = 0; size_t bytes_ = 0; double secs_ = 0;

    Throughput& operator+=(const Throughput& rhs) {
        count_ += rhs.count_; rels_ += rhs.rels_; bytes_ += rhs.bytes_; secs_ += rhs.secs_;
        return *this;
    }
    std::string toString() const {
        if (!count_ || (secs_ < 0.001)) {
            return "";
        }
        return fmt::format(
            "{}x, {} entries, {} bytes in {:.3f}s, {:.0f} rels/s, {:.3f} MB/s",
            count_, rels_, bytes_, secs_, (rels_ / secs_), (bytes_ / 1024. / 1024. / secs_)
        );
    }
};

//============================================================================

BOOST_AUTO_TEST_SUITE(EpcFileContentTypeTests)

// Simulates parsing 57,533 parts TOC from OSDU R1 wells EPC
BOOST_AUTO_TEST_CASE(bench_parse_toc) {
    const bool perf = oes::testing::arePerformanceTestsEnabled();

    const struct {
        const char* type_;
        int count_;
    } per_type_count[] {
        { "ContinuousProperty",                   15485 },
        { "DeviationSurveyRepresentation",        11392 },
        { "EpcExternalPartReference",                 1 },
        { "GeneticBoundaryFeature",                 319 },
        { "HorizonInterpretation",                  319 },
        { "LocalDepth3dCrs",                          1 },
        { "MdDatum",                               5700 },
        { "PropertyKind",                             7 },
        { "PropertySet",                            899 },
        { "RepresentationSetRepresentation",          1 },
        { "WellboreFeature",                       5700 },
        { "WellboreFrameRepresentation",            899 },
        { "WellboreInterpretation",                5700 },
        { "WellboreMarkerFrameRepresentation",     5410 },
        { "WellboreTrajectoryRepresentation",      5700 },
    };

    SimpleTimer setup;

    /*
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
	<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
	<Override PartName="/docProps/extendedCore.xml" ContentType="application/x-extended-core-properties+xml"/>
	<Override PartName="/obj_ContinuousProperty_00008e2a-2735-4479-bd90-9b21a10fc01c.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_ContinuousProperty"/>
	<Override PartName="/obj_ContinuousProperty_00045ab7-ddba-4fe5-b5b0-cc19aa7953d5.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_ContinuousProperty"/>
	<Override PartName="/obj_ContinuousProperty_0004be55-a153-4f6e-9c52-cc18c0b78c47.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_ContinuousProperty"/>
    ...
	<Override PartName="/obj_WellboreTrajectoryRepresentation_ffe9e363-d7c4-46f1-831c-5083c443f850.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreTrajectoryRepresentation"/>
	<Override PartName="/obj_WellboreTrajectoryRepresentation_fffbc9e0-57c3-4be3-8dbe-3f62fc1489f5.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreTrajectoryRepresentation"/>
	<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml" />
</Types>
    */

    epc::FileContentType toc;

    // The 3 "fixed" parts
    toc.addContentType(epc::ContentType(
        false, "application/vnd.openxmlformats-package.core-properties+xml", "/docProps/core.xml"
    ));
    toc.addContentType(epc::ContentType(
        false, "application/x-extended-core-properties+xml", "/docProps/extendedCore.xml"
    ));
    toc.addContentType(epc::ContentType(
        true, "application/vnd.openxmlformats-package.relationships+xml", "rels"
    ));

    for (const auto& entry : per_type_count) {
        std::string type = "application/x-resqml+xml;version=2.0;type=obj_";
        type += entry.type_;

        const int min_count = std::max<int>(1, entry.count_ / 1000);
        const int count = perf? entry.count_: min_count;

        for (int i = 0; i < count; ++i) {
            std::string name = fmt::format(
                "/obj_{}_{}.xml", entry.type_, Guid::make().toXmlString()
            );
            toc.addContentType(epc::ContentType(false, type, name));
        }
    }

    std::string toc_xml = toc.toString();
    const size_t toc_xml_len = toc_xml.length();

    if (perf) {
        BOOST_CHECK_EQUAL(size_t(57536), toc.size());
        BOOST_CHECK_EQUAL(size_t(10132545), toc_xml.length()); // ~ 10 MB
    } else {
        BOOST_CHECK_EQUAL(size_t(62), toc.size());
        BOOST_CHECK_EQUAL(size_t(10849), toc_xml.length()); // ~ 10 KB
    }

    if (oes::testing::arePerformanceTestsEnabled()) {
        OES_INFO("{} setup in {}", test_case_name(), setup.elapsed());
    }

    //// Parse them now

    epc::FileContentType toc_out;

    SimpleTimer parse;
    toc_out.readFromXmlInPlace(std::move(toc_xml));
    if (perf) {
        Throughput stats;
        stats.count_ = 1;
        stats.rels_ = toc_out.size();
        stats.bytes_ = toc_xml_len; // since toc_xml moved-from
        stats.secs_ = parse.elapsedSec();
        OES_INFO("{} parse: {}", test_case_name(), stats.toString());
    } else {
        BOOST_CHECK(toc == toc_out);
    }
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace
