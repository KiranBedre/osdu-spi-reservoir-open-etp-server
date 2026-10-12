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

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/common/CompressUtils.hpp>

#include <oes/core/utils/ThousandSep.h>

#include <oes/testing/core/TestMain.h>
#include <oes/testing/core/ScopedTimer.h>

namespace {

using namespace oes::common;

const char obj_EpcExternalPartReference[] = R"xml(
<?xml version="1.0" encoding="UTF-8"?>
<eml:EpcExternalPartReference
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml="http://www.energistics.org/energyml/data/resqmlv2"
    xsi:type="eml:obj_EpcExternalPartReference"
    uuid="e5e5230e-ecdd-4605-85ad-446926eeb47f"
    schemaVersion="2.0.0.20140822"
>
 <eml:Citation xsi:type="eml:Citation">
  <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
  <eml:Originator xsi:type="eml:NameString">current User</eml:Originator>
  <eml:Creation xsi:type="xsd:dateTime">2016-06-13T11:38:11Z</eml:Creation>
  <eml:Format xsi:type="eml:DescriptionString">Paradigm From Petrel Test</eml:Format>
  <eml:VersionString xsi:type="xsd:string">2016-06-13T11:38:11Z</eml:VersionString>
 </eml:Citation>
 <eml:MimeType xsi:type="xsd:string">application/x-hdf5</eml:MimeType>
</eml:EpcExternalPartReference>
)xml";

const char obj_MdDatum[] = R"xml(
<?xml version="1.0" encoding="UTF-8"?>
<resqml:MdDatum
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml="http://www.energistics.org/energyml/data/resqmlv2"
    xsi:type="resqml:obj_MdDatum"
    uuid="0ee22f34-1049-5b48-b0e9-c5eb4e4f340f"
    schemaVersion="2.0.0.20140822"
>
 <eml:Citation xsi:type="eml:Citation">
  <eml:Title xsi:type="eml:DescriptionString">49025112030000 location</eml:Title>
  <eml:Originator xsi:type="eml:NameString">UNKNOWN</eml:Originator>
  <eml:Creation xsi:type="xsd:dateTime">1970-01-01T00:00:00Z</eml:Creation>
  <eml:Format xsi:type="eml:DescriptionString">Paradigm From Petrel Test</eml:Format>
  <eml:Editor xsi:type="eml:NameString">UNKNOWN</eml:Editor>
  <eml:LastUpdate xsi:type="xsd:dateTime">2016-06-13T09:36:34Z</eml:LastUpdate>
  <eml:Description xsi:type="eml:CommentString">88-1-AX-33</eml:Description>
 </eml:Citation>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/creatorGroup</resqml:Name>
  <resqml:Value xsi:type="xsd:string">Unassigned</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/project</resqml:Name>
  <resqml:Value xsi:type="xsd:string">68b47733-261a-484a-99ae-e048bfd05399</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/measuredDepthUom</resqml:Name>
  <resqml:Value xsi:type="xsd:string">ft[US]</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/KBElevation</resqml:Name>
  <resqml:Value xsi:type="xsd:string">5155.9896880037077</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:Location xsi:type="resqml:Point3d">
  <resqml:Coordinate1 xsi:type="xsd:double">798581.30283477437</resqml:Coordinate1>
  <resqml:Coordinate2 xsi:type="xsd:double">961389.17721849133</resqml:Coordinate2>
  <resqml:Coordinate3 xsi:type="xsd:double">-5155.9896880037077</resqml:Coordinate3>
 </resqml:Location>
 <resqml:MdReference xsi:type="resqml:MdReference">kelly bushing</resqml:MdReference>
 <resqml:LocalCrs xsi:type="eml:DataObjectReference">
  <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs</eml:ContentType>
  <eml:Title xsi:type="eml:DescriptionString">Default local depth CRS</eml:Title>
  <eml:UUID xsi:type="eml:UuidString">1c8bb605-c8e7-4da9-af4c-52e5a5e5147b</eml:UUID>
  <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
  <eml:VersionString xsi:type="eml:NameString">1970-01-01T00:00:00Z</eml:VersionString>
 </resqml:LocalCrs>
</resqml:MdDatum>
)xml";

const char obj_WellboreTrajectoryRepresentation[] = R"xml(
<?xml version="1.0" encoding="UTF-8"?>
<resqml:WellboreTrajectoryRepresentation
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml="http://www.energistics.org/energyml/data/resqmlv2"
    xsi:type="resqml:obj_WellboreTrajectoryRepresentation"
    uuid="4a6e1e5a-54f3-589a-b197-6e2ce35699f1"
    schemaVersion="2.0.0.20140822"
>
 <eml:Citation xsi:type="eml:Citation">
  <eml:Title xsi:type="eml:DescriptionString">POSITION_OPTIMIZED</eml:Title>
  <eml:Originator xsi:type="eml:NameString">UNKNOWN</eml:Originator>
  <eml:Creation xsi:type="xsd:dateTime">1970-01-01T00:00:00Z</eml:Creation>
  <eml:Format xsi:type="eml:DescriptionString">Paradigm From Petrel Test</eml:Format>
  <eml:Editor xsi:type="eml:NameString">UNKNOWN</eml:Editor>
  <eml:LastUpdate xsi:type="xsd:dateTime">2016-06-13T09:36:34Z</eml:LastUpdate>
  <eml:Description xsi:type="eml:CommentString">88-1-AX-33</eml:Description>
 </eml:Citation>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/creatorGroup</resqml:Name>
  <resqml:Value xsi:type="xsd:string">Unassigned</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/project</resqml:Name>
  <resqml:Value xsi:type="xsd:string">68b47733-261a-484a-99ae-e048bfd05399</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:RepresentedInterpretation xsi:type="eml:DataObjectReference">
  <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_WellboreInterpretation</eml:ContentType>
  <eml:Title xsi:type="eml:DescriptionString">49025112030000</eml:Title>
  <eml:UUID xsi:type="eml:UuidString">ec43b905-4faf-5284-b279-aa7b9b22cd52</eml:UUID>
  <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
  <eml:VersionString xsi:type="eml:NameString">2016-06-13T09:36:34Z</eml:VersionString>
 </resqml:RepresentedInterpretation>
 <resqml:StartMd xsi:type="xsd:double">0</resqml:StartMd>
 <resqml:FinishMd xsi:type="xsd:double">3153.9936920022683</resqml:FinishMd>
 <resqml:MdUom xsi:type="eml:LengthUom">ft[US]</resqml:MdUom>
 <resqml:Geometry xsi:type="resqml:ParametricLineGeometry">
  <resqml:LocalCrs xsi:type="eml:DataObjectReference">
   <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs</eml:ContentType>
   <eml:Title xsi:type="eml:DescriptionString">Default local depth CRS</eml:Title>
   <eml:UUID xsi:type="eml:UuidString">1c8bb605-c8e7-4da9-af4c-52e5a5e5147b</eml:UUID>
   <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
   <eml:VersionString xsi:type="eml:NameString">1970-01-01T00:00:00Z</eml:VersionString>
  </resqml:LocalCrs>
  <resqml:ControlPointParameters xsi:type="resqml:DoubleHdf5Array">
   <resqml:Values xsi:type="eml:Hdf5Dataset">
    <eml:PathInHdfFile xsi:type="xsd:string">/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters</eml:PathInHdfFile>
    <eml:HdfProxy xsi:type="eml:DataObjectReference">
     <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=EpcExternalPartReference</eml:ContentType>
     <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
     <eml:UUID xsi:type="eml:UuidString">e5e5230e-ecdd-4605-85ad-446926eeb47f</eml:UUID>
     <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
     <eml:VersionString xsi:type="eml:NameString">2016-06-13T11:38:11Z</eml:VersionString>
    </eml:HdfProxy>
   </resqml:Values>
  </resqml:ControlPointParameters>
  <resqml:ControlPoints xsi:type="resqml:Point3dHdf5Array">
   <resqml:Coordinates xsi:type="eml:Hdf5Dataset">
    <eml:PathInHdfFile xsi:type="xsd:string">/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints</eml:PathInHdfFile>
    <eml:HdfProxy xsi:type="eml:DataObjectReference">
     <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=EpcExternalPartReference</eml:ContentType>
     <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
     <eml:UUID xsi:type="eml:UuidString">e5e5230e-ecdd-4605-85ad-446926eeb47f</eml:UUID>
     <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
     <eml:VersionString xsi:type="eml:NameString">2016-06-13T11:38:11Z</eml:VersionString>
    </eml:HdfProxy>
   </resqml:Coordinates>
  </resqml:ControlPoints>
  <resqml:KnotCount xsi:type="xsd:positiveInteger">2</resqml:KnotCount>
  <resqml:LineKindIndex xsi:type="xsd:integer">5</resqml:LineKindIndex>
  <resqml:TangentVectors xsi:type="resqml:Point3dHdf5Array">
   <resqml:Coordinates xsi:type="eml:Hdf5Dataset">
    <eml:PathInHdfFile xsi:type="xsd:string">/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors</eml:PathInHdfFile>
    <eml:HdfProxy xsi:type="eml:DataObjectReference">
     <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=EpcExternalPartReference</eml:ContentType>
     <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
     <eml:UUID xsi:type="eml:UuidString">e5e5230e-ecdd-4605-85ad-446926eeb47f</eml:UUID>
     <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
     <eml:VersionString xsi:type="eml:NameString">2016-06-13T11:38:11Z</eml:VersionString>
    </eml:HdfProxy>
   </resqml:Coordinates>
  </resqml:TangentVectors>
 </resqml:Geometry>
 <resqml:MdDatum xsi:type="eml:DataObjectReference">
  <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_MdDatum</eml:ContentType>
  <eml:Title xsi:type="eml:DescriptionString">49025112030000 location</eml:Title>
  <eml:UUID xsi:type="eml:UuidString">0ee22f34-1049-5b48-b0e9-c5eb4e4f340f</eml:UUID>
  <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
  <eml:VersionString xsi:type="eml:NameString">2016-06-13T09:36:34Z</eml:VersionString>
 </resqml:MdDatum>
</resqml:WellboreTrajectoryRepresentation>
)xml";

//============================================================================

BOOST_AUTO_TEST_SUITE(CompressUtilsTests)

struct ZTestCase {
    std::string xml;
    std::string compressed_xml;

    // ignored by benchmarks
    unsigned expected_nu = 0; //  uncompressed size
    unsigned expected_nz = 0; //    compressed size
    unsigned expected_nzmin = 0; // compressed size (worse-case)
};

struct ZBenchResults {
    size_t total_uncompressed = 0;
    size_t total_compressed = 0;
    oes::testing::TimerDuration deflate_timer;
    oes::testing::TimerDuration inflate_timer;
};

struct ZBenchmark {
    unsigned repeat = 1;
    std::vector<ZTestCase> cases;

    oes::testing::TimerDuration deflate_timer;
    oes::testing::TimerDuration inflate_timer;

    ZBenchResults getResultsAndReset() {
        ZBenchResults res;
        for (ZTestCase& tc : cases) {
            res.total_uncompressed += tc.xml.size();
            res.total_compressed += tc.compressed_xml.size();
            tc.compressed_xml.clear(); // reset
        }
        res.total_uncompressed *= repeat;
        res.total_compressed   *= repeat;

        res.deflate_timer = deflate_timer;
        res.inflate_timer = inflate_timer;
        deflate_timer = {}; // reset
        inflate_timer = {};
        return res;
    }
};
using ZBenchmarks = std::vector<ZBenchmark>;

void runZTest(const bool perf, const ZTestCase& tc, const ZCompressor& zc) {
    const auto* u = reinterpret_cast<const unsigned char*>(tc.xml.c_str());
    const auto nu = static_cast<unsigned>(tc.xml.size());
    if (!perf) {
        BOOST_CHECK_EQUAL(nu, tc.expected_nu);
    }

    unsigned nzmin = 0;
    int rc = zc.nzmin_(zc.ztype_, u, nu, &nzmin, nullptr, 0);
    if (rc || !perf) {
        BOOST_CHECK_EQUAL(rc, 0);
        BOOST_CHECK_EQUAL(nzmin, tc.expected_nzmin);
    }

    std::vector<unsigned char> compressed(nzmin);
    auto* z = compressed.data();
    unsigned nz = nzmin;
    rc = zc.u2z_(zc.ztype_, u, nu, z, &nz, nullptr, 0);
    if (rc || !perf) {
        BOOST_CHECK_EQUAL(rc, 0);
        BOOST_CHECK_EQUAL(nz, tc.expected_nz);
    }
    compressed.resize(nz);
    z = compressed.data();

    std::vector<unsigned char> uncompressed(nu);
    auto* u2 = uncompressed.data();
    rc = zc.z2u_(z, nz, u2, nu, nullptr, 0);
    if (rc || !perf) {
        BOOST_CHECK_EQUAL(rc, 0);
        if (rc) {
            return;
        }
    }
    std::string xml2;
    xml2.assign(u2, u2 + nu);
    if (!perf) {
        BOOST_CHECK_EQUAL(tc.xml, xml2);
    }
}

const char* str(oes_eml_ztype_e ztype) {
    switch (ztype) {
    case eOES_EML_Z_FAST:  return "fast";
    case eOES_EML_Z_MID:   return "mid";
    case eOES_EML_Z_SMALL: return "small";
    }
    return "ERR";
}

using u4 = unsigned;

void runCompressor(const ZCompressor& zc, oes_eml_ztype_e ztype, u4 zmin[3], u4 zsize[3]) {
    const bool perf = oes::testing::arePerformanceTestsEnabled();
    const int perfN = oes::testing::perfValues(1, 10 * 1000);

    const ZTestCase testcases[] {
        { obj_EpcExternalPartReference,         {}, 1408, zsize[0], zmin[0] },
        { obj_MdDatum,                          {}, 3290, zsize[1], zmin[1] },
        { obj_WellboreTrajectoryRepresentation, {}, 6436, zsize[2], zmin[2] },
    };

    size_t u_total = 0;
    size_t z_total = 0;
    for (auto& tc : testcases) {
        u_total += tc.expected_nu;
        z_total += tc.expected_nz;
    }
    double zratio = (u_total * 1.) / z_total;

    oes::testing::ScopedTimer timer(
        fmt::format("{:<5} {:.2f}z {}x {}", zc.name_, zratio, oes::core::thou(perfN), str(ztype)), perf
    );
    for (int i = 0; i < perfN; ++i) {
        for (auto& tc : testcases) {
            runZTest(perf, tc, zc.with(ztype));
        }
    }
}

void runZLib(oes_eml_ztype_e ztype, u4 zmin[3], u4 zsize[3]) {
    runCompressor(ZLibCompressor, ztype, zmin, zsize);
}

void runGZip(oes_eml_ztype_e ztype, u4 zmin[3], u4 zsize[3]) {
    runCompressor(GZipCompressor, ztype, zmin, zsize);
}

BOOST_AUTO_TEST_CASE(test_zlib_zsmall)  { u4 zm[]{ 1415, 3297, 6444 }; u4 nz[]{  537, 1011, 1395 }; runZLib (eOES_EML_Z_SMALL, zm, nz); }
BOOST_AUTO_TEST_CASE(test_zlib_zmid)    { u4 zm[]{ 1415, 3297, 6444 }; u4 nz[]{  537, 1012, 1411 }; runZLib (eOES_EML_Z_MID,   zm, nz); }
BOOST_AUTO_TEST_CASE(test_zlib_zfast)   { u4 zm[]{ 1415, 3297, 6444 }; u4 nz[]{  566, 1092, 1622 }; runZLib (eOES_EML_Z_FAST,  zm, nz); }

BOOST_AUTO_TEST_CASE(test_gzip_zsmall)  { u4 zm[]{ 1433, 3315, 6462 }; u4 nz[]{  555, 1029, 1413 }; runGZip (eOES_EML_Z_SMALL, zm, nz); }
BOOST_AUTO_TEST_CASE(test_gzip_zmid)    { u4 zm[]{ 1433, 3315, 6462 }; u4 nz[]{  555, 1030, 1429 }; runGZip (eOES_EML_Z_MID,   zm, nz); }
BOOST_AUTO_TEST_CASE(test_gzip_zfast)   { u4 zm[]{ 1433, 3315, 6462 }; u4 nz[]{  584, 1110, 1640 }; runGZip (eOES_EML_Z_FAST,  zm, nz); }

bool runZBench(ZBenchmark& bench, const ZCompressor& zc) {
    const auto zstart = oes::testing::TimerStamp::now();
    for (unsigned i = 0; i < bench.repeat; ++i) {
        for (ZTestCase& tc : bench.cases) {
            if (!zc.deflate(tc.xml, tc.compressed_xml)) {
                return false;
            }
        }
    }

    const auto ustart = oes::testing::TimerStamp::now();
    for (unsigned i = 0; i < bench.repeat; ++i) {
        for (const ZTestCase& tc : bench.cases) {
            const auto nu = tc.xml.size();
            std::string uncompressed(nu, '\0');

            if (!zc.inflate(tc.compressed_xml, uncompressed, nu)) {
                return false;
            }

            if (tc.xml != uncompressed) {
                return false;
            }
        }
    }

    bench.inflate_timer = oes::testing::TimerDuration(ustart);
    bench.deflate_timer = oes::testing::TimerDuration(zstart, ustart);

    return true;
}

BOOST_AUTO_TEST_CASE(bench_all) {
    const bool perf = oes::testing::arePerformanceTestsEnabled();
    const int perfN = oes::testing::perfValues(
        1,
#ifdef OES_DEBUG_BUILD
        1000
#else
        10 * 1000
#endif
    );
    oes::testing::ScopedTimer timer("Test Various Compressors", perf);

    const ZCompressor compressors[] {
        GZipCompressor,
        ZLibCompressor
    };

    const ZTestCase testcases[] {
        { obj_EpcExternalPartReference,         {}, 0, 0, 0 },
        { obj_MdDatum,                          {}, 0, 0, 0 },
        { obj_WellboreTrajectoryRepresentation, {}, 0, 0, 0 },
    };

    ZBenchmark bench;
    bench.repeat = perfN;
    size_t uncompressed_total = 0;
    bench.cases.reserve(sizeof(testcases)/sizeof(*testcases));
    for (auto& tc : testcases) {
        bench.cases.emplace_back(tc);
        uncompressed_total += tc.xml.size();
    }

    const double MB = 1024 * 1024;
    const char* sep =
        "+-------------+-------+--------------------------"
        "+--------------------------"
        "+---------------+---------------+---------------+"
    ;

    if (perf) {
        std::cout << fmt::format(
            "{}x {} XML files, totalling {} MB ({} bytes)\n",
            oes::core::thou(bench.repeat),
            oes::core::thou(bench.cases.size()),
            oes::core::thou(bench.repeat * uncompressed_total / size_t(MB)),
            oes::core::thou(bench.repeat * uncompressed_total)
        );
        std::cout << sep << R"(
| Compressor  | Ratio |          Deflate         |          Inflate         |         De/Inflate + IO Bandwidth             |
|             |       |   MB/s  REAL(s)  USER(s) |   MB/s  REAL(s)  USER(s) | 1000 Mb  100% |  100 Mb  100% |   50 Mb  100% |
)" << sep << std::endl;
        std::cout.flush();
    }

    // Network throughput on its own
    auto net = [](size_t bytes, size_t Mbit_per_Sec) {
        return 8 * bytes / (Mbit_per_Sec * 1024. * 1024.); // bits not bytes
    };

    // Real-world throughtput (compression + decompression + COMPRESSED IO)
    auto io = [net](const ZBenchResults& res, size_t Mbit_per_Sec) {
        return
            res.deflate_timer.real_time_sec_ +
            res.inflate_timer.real_time_sec_ +
            net(res.total_compressed, Mbit_per_Sec)
       ;
    };

    // Is compression worth it, assuming a given networking speed?
    // We compare compressed-transfer (with compression/decompression)
    //       VS uncompressed-transfer (no compression at all).
    // Anything below 100% is a net gain.
    auto iop = [net, io](const ZBenchResults& res, size_t Mbit_per_Sec) {
        return 100. * io(res, Mbit_per_Sec) / net(res.total_uncompressed, Mbit_per_Sec);
    };

    const oes_eml_ztype_e ztypes[] {
        eOES_EML_Z_FAST,
        eOES_EML_Z_MID,
        eOES_EML_Z_SMALL
    };

    std::map<std::string, ZBenchResults> result_map;
    for (const ZCompressor& zc : compressors) {
        for (const oes_eml_ztype_e zt : ztypes) {
            ZCompressor zc2(zc.with(zt));

            runZBench(bench, zc2);
            ZBenchResults res = bench.getResultsAndReset();
            std::string key = fmt::format("{:<5} {:<5}", zc2.name_, str(zt));
            result_map.emplace(key, res);
            if (perf) {
                std::cout << fmt::format(
                    "| {} | {:.2f}z "
                    "| {:6.1f} {:6.3f}s ({:6.3f}s) " // deflate
                    "| {:6.1f} {:6.3f}s ({:6.3f}s) " // inflate
                    "| {:6.3f}s {:4.0f}% | {:6.3f}s {:4.0f}% | {:6.3f}s {:4.0f}% |\n", // De+Inflate + Ethernet
                    key,
                    (res.total_uncompressed * 1.) / res.total_compressed, // compression ratio
                    (res.total_uncompressed / res.deflate_timer.real_time_sec_ / MB),
                    res.deflate_timer.real_time_sec_, res.deflate_timer.user_time_sec_,
                    (res.total_uncompressed / res.inflate_timer.real_time_sec_ / MB),
                    res.inflate_timer.real_time_sec_, res.inflate_timer.user_time_sec_,
                    io(res, 1000), iop(res, 1000), io(res, 100), iop(res, 100), io(res, 50), iop(res, 50)
                );
                std::cout.flush();
            }
        }
        if (perf) {
            std::cout << sep << "\n";
            std::cout.flush();
        }
    }
}

BOOST_AUTO_TEST_CASE(test_zlib_versus_gzip) {
    const std::string xml = obj_WellboreTrajectoryRepresentation;

    std::string zxml_zlib;
    std::string zxml_gzip;

    BOOST_CHECK(ZLibCompressor.deflate(xml, zxml_zlib));
    BOOST_CHECK(GZipCompressor.deflate(xml, zxml_gzip));

    BOOST_CHECK_EQUAL(zxml_zlib.size(), size_t(1622)); // no header-footer
    BOOST_CHECK_EQUAL(zxml_gzip.size(), size_t(1640)); // header-footer from RFC-1952


    std::string xml_zlib;
    std::string xml_gzip;

    BOOST_CHECK(ZLibCompressor.inflate(zxml_zlib, xml_zlib, xml.size()));
    BOOST_CHECK(GZipCompressor.inflate(zxml_gzip, xml_gzip, xml.size()));

    BOOST_CHECK_EQUAL(xml, xml_zlib);
    BOOST_CHECK_EQUAL(xml, xml_gzip);
}

BOOST_AUTO_TEST_CASE(test_gzip_inflate) {
    const std::string xml = obj_WellboreTrajectoryRepresentation;

    std::string zxml_gzip;

    BOOST_CHECK(GZipCompressor.deflate(xml, zxml_gzip));

    {   // "Normal" decompress
        std::string xml_gzip;
        BOOST_CHECK(GZipCompressor.inflate(zxml_gzip, xml_gzip, xml.size()));
        BOOST_CHECK_EQUAL(xml, xml_gzip);
    }

    { // Add "garbage" at the end.
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2.resize(zxml_gzip2.size() + 1);
        zxml_gzip2.back() = 'A';

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
            BOOST_CHECK_EQUAL(xml, xml_gzip);
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            const size_t original_usize = xml.size() + 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(ok);
            BOOST_CHECK_EQUAL(xml_gzip.size() + 10, original_usize);
            BOOST_CHECK_EQUAL(trailing_z_bytes, 1);
            BOOST_CHECK_EQUAL(xml, xml_gzip);
        }

        { // W/o knowing the uncompressed size exactly. Without enough space!
            std::string xml_gzip;
            const size_t original_usize = xml.size() - 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    { // Truncate the compressed data
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2.resize(zxml_gzip2.size() - 1);

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            const size_t original_usize = xml.size() + 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }

        { // W/o knowing the uncompressed size exactly. Without enough space!
            std::string xml_gzip;
            const size_t original_usize = xml.size() - 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    { // Try to corrupt the compressed data in the trailing ISIZE
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2.back() += 1;

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            const size_t original_usize = xml.size() + 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }

        { // W/o knowing the uncompressed size exactly. Without enough space!
            std::string xml_gzip;
            const size_t original_usize = xml.size() - 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    { // Try to corrupt the compressed data in the trailing CRC32
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2[zxml_gzip2.size() - 6] += 1;

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            const size_t original_usize = xml.size() + 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }

        { // W/o knowing the uncompressed size exactly. Without enough space!
            std::string xml_gzip;
            const size_t original_usize = xml.size() - 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    { // Try to corrupt the compressed data in the middle, in the data-part
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2[zxml_gzip2.size() / 2] += 1;

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            const size_t original_usize = xml.size() + 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }

        { // W/o knowing the uncompressed size exactly. Without enough space!
            std::string xml_gzip;
            const size_t original_usize = xml.size() - 10;
            xml_gzip.resize(original_usize);
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(xml_gzip.size(), original_usize); // size unchanged on error
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    // Warning: By default, there's no Half-CRC32 for the GZip header,
    //          so it's not clear whether corruption there would be detected.
}

BOOST_AUTO_TEST_CASE(test_gzip_inflate2) {
    const std::string xml = obj_WellboreTrajectoryRepresentation;

    std::string zxml_gzip;

    BOOST_CHECK(GZipCompressor.deflate(xml, zxml_gzip));

    {   // "Normal" decompress (knowing the size a-priori)
        std::string xml_gzip;
        BOOST_CHECK(GZipCompressor.inflate(zxml_gzip, xml_gzip, xml.size()));
        BOOST_CHECK_EQUAL(xml, xml_gzip);
    }

    { // Add "garbage" at the end.
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2.resize(zxml_gzip2.size() + 1);
        zxml_gzip2.back() = 'A';

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
            BOOST_CHECK_EQUAL(xml, xml_gzip);
        }

        { // W/o knowing the uncompressed size exactly.
            std::string xml_gzip;
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate2(zxml_gzip2, xml_gzip);
            BOOST_CHECK(ok);
            BOOST_CHECK_EQUAL(trailing_z_bytes, 1);
            BOOST_CHECK_EQUAL(xml, xml_gzip);
        }
    }

    { // Truncate the compressed data
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2.resize(zxml_gzip2.size() - 1);

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate2(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    { // Try to corrupt the compressed data in the trailing ISIZE
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2.back() += 1;

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate2(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    { // Try to corrupt the compressed data in the trailing CRC32
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2[zxml_gzip2.size() - 6] += 1;

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate2(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    { // Try to corrupt the compressed data in the middle, in the data-part
        std::string zxml_gzip2(zxml_gzip);
        zxml_gzip2[zxml_gzip2.size() / 2] += 1;

        { // Knowing the uncompressed size a-priori
            std::string xml_gzip;
            BOOST_CHECK(!GZipCompressor.inflate(zxml_gzip2, xml_gzip, xml.size()));
        }

        { // W/o knowing the uncompressed size exactly. With enough space
            std::string xml_gzip;
            auto [ok, trailing_z_bytes] = GZipCompressor.inflate2(zxml_gzip2, xml_gzip);
            BOOST_CHECK(!ok);
            BOOST_CHECK_EQUAL(trailing_z_bytes, 0); // remains as-is on error
        }
    }

    // Warning: By default, there's no Half-CRC32 for the GZip header,
    //          so it's not clear whether corruption there would be detected.
}

// Tests to illustrate the fact the worse case size needed to "compress"
// (random) data is proportional to its size, and not a fixed overhead.
// That's explains simply by the fact that beside the header and trailer,
// the compressed data is divided in blocks, with a small overhead per-block
// and thus that proportionality. Overhead asymptot around 1/3rd of one thousands.
BOOST_AUTO_TEST_CASE(test_zlib_nzmin) {
    BOOST_CHECK_EQUAL(           25, GZipCompressor.bufsize(            0));
    BOOST_CHECK_EQUAL(           26, GZipCompressor.bufsize(            1));
    BOOST_CHECK_EQUAL(           35, GZipCompressor.bufsize(           10));
    BOOST_CHECK_EQUAL(          125, GZipCompressor.bufsize(          100));
    BOOST_CHECK_EQUAL(         1025, GZipCompressor.bufsize(         1000));
    BOOST_CHECK_EQUAL(       10'027, GZipCompressor.bufsize(       10'000));
    BOOST_CHECK_EQUAL(      100'055, GZipCompressor.bufsize(      100'000));
    BOOST_CHECK_EQUAL(    1'000'330, GZipCompressor.bufsize(    1'000'000));
    BOOST_CHECK_EQUAL(   10'003'076, GZipCompressor.bufsize(   10'000'000));
    BOOST_CHECK_EQUAL(  100'030'544, GZipCompressor.bufsize(  100'000'000));
    BOOST_CHECK_EQUAL(1'000'305'229, GZipCompressor.bufsize(1'000'000'000));
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace
