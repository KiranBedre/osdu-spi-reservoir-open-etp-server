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

#include <oes/common/epc/FilePart.h>

#include <oes/testing/eml/TestingMocks.h>

#include <oes/testing/core/TestMain.h>

namespace {

using namespace oes::common;
using namespace oes::common::epc::xml;

using namespace oes::testing::eml;

//============================================================================

BOOST_AUTO_TEST_SUITE(EpcFilePartTests)

BOOST_AUTO_TEST_CASE(test_rootElementInfo) {
    struct {
        std::string xml;
        std::string expected_xmlns_uri;
        std::string expected_local_name;
    } testcases[] {
        { epc_ext_mock().xml_, xmlns_eml,      "EpcExternalPartReference" },
        { datum_mock().xml_,   xmlns_resqml,   "MdDatum" },
        { traj_mock().xml_,    xmlns_resqml,   "WellboreTrajectoryRepresentation" },
    };
    for (auto& tc : testcases) {
        std::string xmlns_uri;
        std::string local_name;
        const bool ok = epc::FilePart::rootElementInfo(tc.xml, xmlns_uri, local_name);
        BOOST_CHECK(ok);
        BOOST_CHECK_EQUAL(xmlns_uri, tc.expected_xmlns_uri);
        BOOST_CHECK_EQUAL(local_name, tc.expected_local_name);
    }
}

BOOST_AUTO_TEST_CASE(test_scan_EpcExternalPartReference) {
    std::string xml = epc_ext_mock().xml_;
    XmlName root;
    AbstractCitedDataObject cited;
    std::vector<NameValuePair> xdata;
    std::vector<DataObjectReference> refs;
    std::vector<Hdf5Dataset> hdfs;
    const epc::FilePart::ScanRc rc = epc::FilePart::scan(
        xml,
        [&root, &cited](const XmlName& n, const AbstractCitedDataObject& c) {
            root = n;
            cited = c;
            return epc::FilePart::eOK_DONE;
        },
        [&xdata](const NameValuePair& x) {
            xdata.emplace_back(x);
            return epc::FilePart::eOK_DONE;
        },
        [&refs](const XmlPathGetter&, const DataObjectReference& r) {
            refs.emplace_back(r);
            return epc::FilePart::eOK_DONE;
        },
        [&hdfs](const XmlPathGetter&, const Hdf5Dataset& d) {
            hdfs.emplace_back(d);
            return epc::FilePart::eOK_DONE;
        }
    );
    BOOST_CHECK_EQUAL(rc, epc::FilePart::eOK_DONE);

    BOOST_CHECK_EQUAL(xmlns_eml, root.xmlns_uri_);
    BOOST_CHECK_EQUAL("EpcExternalPartReference", root.local_name_);

    BOOST_CHECK_EQUAL("e5e5230e-ecdd-4605-85ad-446926eeb47f", cited.uuid_);
    BOOST_CHECK_EQUAL("2.0.0.20140822", root.schema_version_);

    BOOST_CHECK_EQUAL("Hdf5File", cited.Title_);
    BOOST_CHECK_EQUAL("current User", cited.Originator_);
    BOOST_CHECK_EQUAL("2016-06-13T11:38:11Z", cited.Creation_);
    BOOST_CHECK_EQUAL("Paradigm From Petrel Test", cited.Format_);
    BOOST_CHECK(cited.Editor_.empty());
    BOOST_CHECK(cited.LastUpdate_.empty());
    BOOST_CHECK_EQUAL("2016-06-13T11:38:11Z", cited.VersionString_);
    BOOST_CHECK(cited.Description_.empty());
    BOOST_CHECK(cited.DescriptiveKeywords_.empty());

    BOOST_CHECK(xdata.empty());
    BOOST_CHECK(refs.empty());
    BOOST_CHECK(hdfs.empty());
}

BOOST_AUTO_TEST_CASE(test_scan_MdDatum) {
    std::string xml = datum_mock().xml_;
    XmlName root;
    AbstractCitedDataObject cited;
    std::vector<NameValuePair> xdata;
    std::vector<DataObjectReference> refs;
    std::vector<Hdf5Dataset> hdfs;
    const epc::FilePart::ScanRc rc = epc::FilePart::scan(
        xml,
        [&root, &cited](const XmlName& n, const AbstractCitedDataObject& c) {
            root = n;
            cited = c;
            return epc::FilePart::eOK_DONE;
        },
        [&xdata](const NameValuePair& x) {
            xdata.emplace_back(x);
            return epc::FilePart::eOK_DONE;
        },
        [&refs](const XmlPathGetter&, const DataObjectReference& r) {
            refs.emplace_back(r);
            return epc::FilePart::eOK_DONE;
        },
        [&hdfs](const XmlPathGetter&, const Hdf5Dataset& d) {
            hdfs.emplace_back(d);
            return epc::FilePart::eOK_DONE;
        }
    );
    BOOST_CHECK_EQUAL(rc, epc::FilePart::eOK_DONE);

    BOOST_CHECK_EQUAL(xmlns_resqml, root.xmlns_uri_);
    BOOST_CHECK_EQUAL("MdDatum", root.local_name_);

    BOOST_CHECK_EQUAL("0ee22f34-1049-5b48-b0e9-c5eb4e4f340f", cited.uuid_);
    BOOST_CHECK_EQUAL("2.0.0.20140822", root.schema_version_);

    BOOST_CHECK_EQUAL("49025112030000 location", cited.Title_);
    BOOST_CHECK_EQUAL("UNKNOWN", cited.Originator_);
    BOOST_CHECK_EQUAL("1970-01-01T00:00:00Z", cited.Creation_);
    BOOST_CHECK_EQUAL("Paradigm From Petrel Test", cited.Format_);
    BOOST_CHECK_EQUAL(cited.Editor_, "UNKNOWN");
    BOOST_CHECK_EQUAL(cited.LastUpdate_, "2016-06-13T09:36:34Z");
    BOOST_CHECK(cited.VersionString_.empty());
    BOOST_CHECK_EQUAL(cited.Description_, "88-1-AX-33");
    BOOST_CHECK(cited.DescriptiveKeywords_.empty());

    // Extra-Meta-Data
    BOOST_REQUIRE_EQUAL(xdata.size(), size_t(4));

    BOOST_CHECK_EQUAL(xdata[0].first, "pdgm/dx/resqml,/creatorGroup");
    BOOST_CHECK_EQUAL(xdata[0].second, "Unassigned");

    BOOST_CHECK_EQUAL(xdata[1].first, "pdgm/dx/resqml,/project");
    BOOST_CHECK_EQUAL(xdata[1].second, "68b47733-261a-484a-99ae-e048bfd05399");

    BOOST_CHECK_EQUAL(xdata[2].first, "pdgm/dx/resqml,/measuredDepthUom");
    BOOST_CHECK_EQUAL(xdata[2].second, "ft[US]");

    BOOST_CHECK_EQUAL(xdata[3].first, "pdgm/dx/resqml,/KBElevation");
    BOOST_CHECK_EQUAL(xdata[3].second, "5155.9896880037077");

    // References
    BOOST_REQUIRE_EQUAL(refs.size(), size_t(1));
    BOOST_CHECK_EQUAL(refs[0].QualifiedType_, "resqml20.obj_LocalDepth3dCrs");
    BOOST_CHECK_EQUAL(refs[0].Title_, "Default local depth CRS");
    BOOST_CHECK_EQUAL(refs[0].Uuid_, "1c8bb605-c8e7-4da9-af4c-52e5a5e5147b");
    BOOST_CHECK_EQUAL(refs[0].ObjectVersion_, "1970-01-01T00:00:00Z");
  //BOOST_CHECK_EQUAL(refs[0].EpcExternalPartReference_, ???);

    // Datasets
    BOOST_CHECK(hdfs.empty());
}

BOOST_AUTO_TEST_CASE(test_scan_WellboreTrajectoryRepresentation) {
    std::string xml = traj_mock().xml_;
    XmlName root;
    AbstractCitedDataObject cited;
    std::vector<NameValuePair> xdata;
    std::vector<DataObjectReference> refs;
    std::vector<Hdf5Dataset> hdfs;
    const epc::FilePart::ScanRc rc = epc::FilePart::scan(
        xml,
        [&root, &cited](const XmlName& n, const AbstractCitedDataObject& c) {
            root = n;
            cited = c;
            return epc::FilePart::eOK_DONE;
        },
        [&xdata](const NameValuePair& x) {
            xdata.emplace_back(x);
            return epc::FilePart::eOK_DONE;
        },
        [&refs](const XmlPathGetter&, const DataObjectReference& r) {
            refs.emplace_back(r);
            return epc::FilePart::eOK_DONE;
        },
        [&hdfs](const XmlPathGetter&, const Hdf5Dataset& d) {
            hdfs.emplace_back(d);
            return epc::FilePart::eOK_DONE;
        }
    );
    BOOST_CHECK_EQUAL(rc, epc::FilePart::eOK_DONE);

    BOOST_CHECK_EQUAL(xmlns_resqml, root.xmlns_uri_);
    BOOST_CHECK_EQUAL("WellboreTrajectoryRepresentation", root.local_name_);

    BOOST_CHECK_EQUAL("4a6e1e5a-54f3-589a-b197-6e2ce35699f1", cited.uuid_);
    BOOST_CHECK_EQUAL("2.0.0.20140822", root.schema_version_);

    BOOST_CHECK_EQUAL("POSITION_OPTIMIZED", cited.Title_);
    BOOST_CHECK_EQUAL("UNKNOWN", cited.Originator_);
    BOOST_CHECK_EQUAL("1970-01-01T00:00:00Z", cited.Creation_);
    BOOST_CHECK_EQUAL("Paradigm From Petrel Test", cited.Format_);
    BOOST_CHECK_EQUAL(cited.Editor_, "UNKNOWN");
    BOOST_CHECK_EQUAL(cited.LastUpdate_, "2016-06-13T09:36:34Z");
    BOOST_CHECK(cited.VersionString_.empty());
    BOOST_CHECK_EQUAL(cited.Description_, "88-1-AX-33");
    BOOST_CHECK(cited.DescriptiveKeywords_.empty());

    // Extra-Meta-Data
    BOOST_REQUIRE_EQUAL(xdata.size(), size_t(2));

    BOOST_CHECK_EQUAL(xdata[0].first, "pdgm/dx/resqml,/creatorGroup");
    BOOST_CHECK_EQUAL(xdata[0].second, "Unassigned");

    BOOST_CHECK_EQUAL(xdata[1].first, "pdgm/dx/resqml,/project");
    BOOST_CHECK_EQUAL(xdata[1].second, "68b47733-261a-484a-99ae-e048bfd05399");

    // References. 3, not 6, since HdfProxy references are reported via Datasets instead
    BOOST_REQUIRE_EQUAL(refs.size(), size_t(3));

    // resqml:RepresentedInterpretation
    BOOST_CHECK_EQUAL(refs[0].QualifiedType_, "resqml20.obj_WellboreInterpretation");
    BOOST_CHECK_EQUAL(refs[0].Title_, "49025112030000");
    BOOST_CHECK_EQUAL(refs[0].Uuid_, "ec43b905-4faf-5284-b279-aa7b9b22cd52");
    BOOST_CHECK_EQUAL(refs[0].ObjectVersion_, "2016-06-13T09:36:34Z");
    BOOST_CHECK_EQUAL(refs[0].eml_HdfProxy_, false);
  //BOOST_CHECK_EQUAL(refs[0].EpcExternalPartReference_, ???);

    // resqml:Geometry/resqml:LocalCrs
    BOOST_CHECK_EQUAL(refs[1].QualifiedType_, "resqml20.obj_LocalDepth3dCrs");
    BOOST_CHECK_EQUAL(refs[1].Title_, "Default local depth CRS");
    BOOST_CHECK_EQUAL(refs[1].Uuid_, "1c8bb605-c8e7-4da9-af4c-52e5a5e5147b");
    BOOST_CHECK_EQUAL(refs[1].ObjectVersion_, "1970-01-01T00:00:00Z");
    BOOST_CHECK_EQUAL(refs[1].eml_HdfProxy_, false);
  //BOOST_CHECK_EQUAL(refs[1].EpcExternalPartReference_, ???);

    // resqml:MdDatum
    BOOST_CHECK_EQUAL(refs[2].QualifiedType_, "resqml20.obj_MdDatum");
    BOOST_CHECK_EQUAL(refs[2].Title_, "49025112030000 location");
    BOOST_CHECK_EQUAL(refs[2].Uuid_, "0ee22f34-1049-5b48-b0e9-c5eb4e4f340f");
    BOOST_CHECK_EQUAL(refs[2].ObjectVersion_, "2016-06-13T09:36:34Z");
    BOOST_CHECK_EQUAL(refs[2].eml_HdfProxy_, false);
  //BOOST_CHECK_EQUAL(refs[2].EpcExternalPartReference_, ???);

    // Datasets
    BOOST_CHECK_EQUAL(hdfs.size(), size_t(3));
    for (size_t i = 0; i < hdfs.size(); ++i) {
        const std::string& path = hdfs[i].PathInHdfFile_;
        switch (i) {
        case  0: BOOST_CHECK_EQUAL(path, "/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters"); break;
        case  1: BOOST_CHECK_EQUAL(path, "/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints"); break;
        case  2: BOOST_CHECK_EQUAL(path, "/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors"); break;
        default:
            BOOST_CHECK(false);
        }
        // **/eml:HdfProxy
        // Same ref for all datasets...
        const DataObjectReference& ref = hdfs[i].HdfProxy_;
        BOOST_CHECK_EQUAL(ref.QualifiedType_, "resqml20.EpcExternalPartReference");
        BOOST_CHECK_EQUAL(ref.Title_, "Hdf5File");
        BOOST_CHECK_EQUAL(ref.Uuid_, "e5e5230e-ecdd-4605-85ad-446926eeb47f");
        BOOST_CHECK_EQUAL(ref.ObjectVersion_, "2016-06-13T11:38:11Z");
        BOOST_CHECK_EQUAL(ref.eml_HdfProxy_, true);
      //BOOST_CHECK_EQUAL(ref.EpcExternalPartReference_, ???);
    }
}

BOOST_AUTO_TEST_CASE(test_scan_WellboreTrajectoryRepresentation_no_on_dataset) {
    std::string xml = traj_mock().xml_;
    XmlName root;
    AbstractCitedDataObject cited;
    std::vector<NameValuePair> xdata;
    std::vector<DataObjectReference> refs;
    const epc::FilePart::ScanRc rc = epc::FilePart::scan(
        xml,
        [&root, &cited](const XmlName& n, const AbstractCitedDataObject& c) {
            root = n;
            cited = c;
            return epc::FilePart::eOK_DONE;
        },
        [&xdata](const NameValuePair& x) {
            xdata.emplace_back(x);
            return epc::FilePart::eOK_DONE;
        },
        [&refs](const XmlPathGetter&, const DataObjectReference& r) {
            refs.emplace_back(r);
            return epc::FilePart::eOK_DONE;
        },
        nullptr
    );
    BOOST_CHECK_EQUAL(rc, epc::FilePart::eOK_DONE);

    BOOST_CHECK_EQUAL(xmlns_resqml, root.xmlns_uri_);
    BOOST_CHECK_EQUAL("WellboreTrajectoryRepresentation", root.local_name_);

    BOOST_CHECK_EQUAL("4a6e1e5a-54f3-589a-b197-6e2ce35699f1", cited.uuid_);
    BOOST_CHECK_EQUAL("2.0.0.20140822", root.schema_version_);

    BOOST_CHECK_EQUAL("POSITION_OPTIMIZED", cited.Title_);
    // etc... Checked in test above already

    // Extra-Meta-Data
    BOOST_REQUIRE_EQUAL(xdata.size(), size_t(2));

    // References. 6 now, no longer 3, since no on_dataset handler
    BOOST_REQUIRE_EQUAL(refs.size(), size_t(6));
}

BOOST_AUTO_TEST_CASE(test_scan_WellboreTrajectoryRepresentation_xml_paths) {
    std::string xml = traj_mock().xml_;
    XmlName root;
    std::vector<std::string> ref_xpaths;
    std::vector<std::string> hdf_xpaths;
    epc::FilePart::ScanRc rc = epc::FilePart::scan(
        xml,
        nullptr,
        nullptr,
        [&ref_xpaths](const XmlPathGetter& xpg, const DataObjectReference&) {
            ref_xpaths.emplace_back(
                xpg("2.0", nullptr)
            );
            return epc::FilePart::eOK_DONE;
        },
        nullptr
    );
    BOOST_CHECK_EQUAL(rc, epc::FilePart::eOK_DONE);

    BOOST_REQUIRE_EQUAL(hdf_xpaths.size(), size_t(0));
    BOOST_REQUIRE_EQUAL(ref_xpaths.size(), size_t(6));
    BOOST_CHECK_EQUAL(ref_xpaths[0], "rsq:RepresentedInterpretation");
    BOOST_CHECK_EQUAL(ref_xpaths[1], "rsq:Geometry/rsq:LocalCrs");
    BOOST_CHECK_EQUAL(ref_xpaths[2], "rsq:Geometry/rsq:ControlPointParameters/rsq:Values/eml:HdfProxy");
    BOOST_CHECK_EQUAL(ref_xpaths[3], "rsq:Geometry/rsq:ControlPoints/rsq:Coordinates/eml:HdfProxy");
    BOOST_CHECK_EQUAL(ref_xpaths[4], "rsq:Geometry/rsq:TangentVectors/rsq:Coordinates/eml:HdfProxy");
    BOOST_CHECK_EQUAL(ref_xpaths[5], "rsq:MdDatum");

    ref_xpaths.clear();
    hdf_xpaths.clear();

    rc = epc::FilePart::scan(
        xml,
        nullptr,
        nullptr,
        [&ref_xpaths](const XmlPathGetter& xpg, const DataObjectReference&) {
            ref_xpaths.emplace_back(
                xpg("2.0", nullptr)
            );
            return epc::FilePart::eOK_DONE;
        },
        [&hdf_xpaths](const XmlPathGetter& xpg, const Hdf5Dataset&) {
            hdf_xpaths.emplace_back(
                xpg("2.0", nullptr)
            );
            return epc::FilePart::eOK_DONE;
        }
    );
    BOOST_CHECK_EQUAL(rc, epc::FilePart::eOK_DONE);

    BOOST_REQUIRE_EQUAL(ref_xpaths.size(), size_t(3));
    BOOST_CHECK_EQUAL(ref_xpaths[0], "rsq:RepresentedInterpretation");
    BOOST_CHECK_EQUAL(ref_xpaths[1], "rsq:Geometry/rsq:LocalCrs");
    BOOST_CHECK_EQUAL(ref_xpaths[2], "rsq:MdDatum");

    // Notice the lack of .../eml:HdfProxy suffix, this time, because
    // the DataObjectReference to the HdfProxy is explicit and "inside"
    BOOST_REQUIRE_EQUAL(hdf_xpaths.size(), size_t(3));
    BOOST_CHECK_EQUAL(hdf_xpaths[0], "rsq:Geometry/rsq:ControlPointParameters/rsq:Values");
    BOOST_CHECK_EQUAL(hdf_xpaths[1], "rsq:Geometry/rsq:ControlPoints/rsq:Coordinates");
    BOOST_CHECK_EQUAL(hdf_xpaths[2], "rsq:Geometry/rsq:TangentVectors/rsq:Coordinates");
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace
