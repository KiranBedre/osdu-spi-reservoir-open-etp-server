// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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

#include <filesystem>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <benchmark/benchmark.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/common/epc/Package.h>
#include <oes/testing/core/TestMain.h>


// ============================================================================
// Test data
// ============================================================================

static constexpr char testContentType[] = "application/x-resqml+xml;version=2.0;type=obj_WellboreInterpretation";
static constexpr char testEpcFile[] = "WELL_RESQML.epc";

static constexpr char testFileContentType[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
	<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>
	<Override PartName="/docProps/extendedCore.xml" ContentType="application/x-extended-core-properties+xml"/>
	<Override PartName="/obj_EpcExternalPartReference_e5e5230e-ecdd-4605-85ad-446926eeb47f.xml" ContentType="application/x-eml+xml;version=2.0;type=obj_EpcExternalPartReference"/>
	<Override PartName="/obj_LocalDepth3dCrs_1c8bb605-c8e7-4da9-af4c-52e5a5e5147b.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs"/>
	<Override PartName="/obj_MdDatum_0ee22f34-1049-5b48-b0e9-c5eb4e4f340f.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_MdDatum"/>
	<Override PartName="/obj_WellboreFeature_7d9e5e11-7fff-53c3-b645-121655c6922c.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreFeature"/>
	<Override PartName="/obj_WellboreInterpretation_ec43b905-4faf-5284-b279-aa7b9b22cd52.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreInterpretation"/>
	<Override PartName="/obj_WellboreMarkerFrameRepresentation_c76021a6-c1e7-5b2d-848a-35764da8001d.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreMarkerFrameRepresentation"/>
	<Override PartName="/obj_WellboreTrajectoryRepresentation_4a6e1e5a-54f3-589a-b197-6e2ce35699f1.xml" ContentType="application/x-resqml+xml;version=2.0;type=obj_WellboreTrajectoryRepresentation"/>
	<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml" />
</Types>
)";

static constexpr char testFileRelationShip[] = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
	<Relationship Id="_4a96f14a-6c32-4b74-9102-b0ce2304d824_d00081ad-b2e8-4c1e-9494-392cb43cd0a9" Type="http://schemas.energistics.org/package/2012/relationships/destinationObject" Target="obj_IjkGridRepresentation_4a96f14a-6c32-4b74-9102-b0ce2304d824."/>
	<Relationship Id="_37ff7491-c597-5362-9446-9c29c38f5243_86406b58-dae9-4d65-992b-d3cd14d78aa7" Type="http://schemas.energistics.org/package/2012/relationships/destinationObject" Target="obj_PropertyKind_37ff7491-c597-5362-9446-9c29c38f5243."/>
	<Relationship Id="_f78b236a-79a6-458f-a798-fb36f3ac36e3_74ad1797-e14a-43b3-bedf-e4ab09e009e2" Type="http://schemas.energistics.org/package/2012/relationships/destinationObject" Target="obj_StringTableLookup_f78b236a-79a6-458f-a798-fb36f3ac36e3."/>
	<Relationship Id="_24c6c433-fa48-4b67-9c89-4f9271f38dea_7788a40b-bb34-4668-ad4f-1960e1b11e23" Type="http://schemas.energistics.org/package/2012/relationships/mlToExternalPartProxy" Target="obj_EpcExternalPartReference_24c6c433-fa48-4b67-9c89-4f9271f38dea.xml"/>
</Relationships>
)";

// ============================================================================
// Benchmarks
// ============================================================================

void Epc_ContentTypeParse(benchmark::State& state)
{
    for (auto _ : state) {
        std::string ml, format, version, type;
        if (!oes::common::epc::ContentType::parse(testContentType, ml, format, version, type)) {
            state.SkipWithError("Unable to parse ContentType");
        }
    }
}
BENCHMARK(Epc_ContentTypeParse);


void Epc_FileContentTypeParseXml(benchmark::State& state)
{
    for (auto _ : state) {
        oes::common::epc::FileContentType fileContentType;
        if (!fileContentType.readFromXmlInPlace(testFileContentType)) {
            state.SkipWithError("Unable to read FileContentType from XML");
        }
    }
}
BENCHMARK(Epc_FileContentTypeParseXml);


void Epc_FileContentTypeToXml(benchmark::State& state)
{
    oes::common::epc::FileContentType fileContentType;
    fileContentType.readFromXmlInPlace(testFileContentType);

    for (auto _ : state) {
        benchmark::DoNotOptimize(fileContentType.toString());
    }
}
BENCHMARK(Epc_FileContentTypeToXml);


void Epc_FileRelationshipParseXml(benchmark::State& state)
{
    for (auto _ : state) {
        oes::common::epc::FileRelationship fileRelationship;
        if (!fileRelationship.readFromXmlInPlace(testFileRelationShip)) {
            state.SkipWithError("Unable to read FileRelationship from XML");
            break;
        }
    }
}
BENCHMARK(Epc_FileRelationshipParseXml);


void Epc_FileRelationshipToXml(benchmark::State& state)
{
    oes::common::epc::FileRelationship fileRelationship;
    fileRelationship.readFromXmlInPlace(testFileRelationShip);

    for (auto _ : state) {
        benchmark::DoNotOptimize(fileRelationship.toString());
    }
}
BENCHMARK(Epc_FileRelationshipToXml);


void Epc_PackageOpenForReading(benchmark::State& state)
{
    // Check prerequisites
    const std::string epcInput = oes::testing::resolveTestDataFile("data/eml", testEpcFile, true);
    if (epcInput.empty() || !std::filesystem::exists(epcInput)) {
        return state.SkipWithError("Test EPC file does not exist");
    }

    for (auto _ : state) {
        oes::common::epc::Package package;
        if (!package.openForReading(epcInput)) {
            state.SkipWithError("Unable to open EPC file for reading");
            break;
        }
    }
}
BENCHMARK(Epc_PackageOpenForReading)->Unit(benchmark::kMicrosecond);
