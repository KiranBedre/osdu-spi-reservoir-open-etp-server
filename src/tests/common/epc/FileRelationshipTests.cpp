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

#include <oes/common/epc/FileRelationship.h>

#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Guid.h>

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
            "{}x, {} rels, {} bytes in {:.3f}s, {:.0f} rels/s, {:.3f} MB/s",
            count_, rels_, bytes_, secs_, (rels_ / secs_), (bytes_ / 1024. / 1024. / secs_)
        );
    }
};

//============================================================================

BOOST_AUTO_TEST_SUITE(EpcFileRelationshipTests)

// Simulates the 17,098 rels files with 3 relations from OSDU R1 wells EPC
BOOST_AUTO_TEST_CASE(bench_file_relationship_parsing_3rels_per_file) {
    SimpleTimer setup;
    const int frel_count = oes::testing::perfValues(17, 17098);

    /*
    ** This benchmark takes this rels files, from OSDU R1, as typical to
    ** entities with just a few relations. (reformatted to fit screen)
    ** The rels of for this entity BTW
<resqml2:ContinuousProperty schemaVersion="2.0"
                            uuid="00008e2a-2735-4479-bd90-9b21a10fc01c"
                            xsi:type="resqml2:obj_ContinuousProperty">

    **

<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
    <Relationship Id="_46509486-87c4-47bd-876b-f500fbecf01c_10b37534-80a4-43fc-bd7b-675c287fe1b6"
                  Type="http://schemas.energistics.org/package/2012/relationships/destinationObject"
                  Target="obj_WellboreTrajectoryRepresentation_46509486-87c4-47bd-876b-f500fbecf01c.xml"/>
    <Relationship Id="_56c10317-5eab-4fef-ae1a-506859a63849_9bc84e3c-f25f-44cc-a2b7-725e3aa1234b"
                  Type="http://schemas.energistics.org/package/2012/relationships/mlToExternalPartProxy"
                  Target="obj_EpcExternalPartReference_56c10317-5eab-4fef-ae1a-506859a63849.xml"/>
    <Relationship Id="_46509486-87c4-47bd-876b-f500fbecf01c_c0befb57-4b3e-4363-9219-1eaec1635f3e"
                  Type="http://schemas.energistics.org/package/2012/relationships/destinationObject"
                  Target="obj_WellboreTrajectoryRepresentation_46509486-87c4-47bd-876b-f500fbecf01c.xml"/>
</Relationships>
    */

    std::vector<epc::FileRelationship> frels(frel_count);
    std::vector<std::string> frels_xml(frel_count);
    for (int i = 0; i < frel_count; ++i) {
        epc::FileRelationship& frel = frels[i];

        // Note: The second guid from the Id attribute seems random,
        // and is not related to the entity the relationship applies to...
        const std::string traj_uid = Guid::make().toXmlString();
        const std::string bulk_uid = Guid::make().toXmlString();

        epc::Relationship rel1;
        rel1.setTarget("obj_WellboreTrajectoryRepresentation_" + traj_uid);
        rel1.setId('_' + traj_uid + '_' + Guid::make().toXmlString());
        rel1.setDestinationObjectType();
        frel.addRelationship(rel1);

        epc::Relationship rel2;
        rel2.setTarget("obj_EpcExternalPartReference_" + bulk_uid);
        rel2.setId('_' + bulk_uid + '_' + Guid::make().toXmlString());
        rel2.setMlToExternalPartProxyType();
        frel.addRelationship(rel2);

        epc::Relationship rel3;
        rel3.setTarget("obj_WellboreTrajectoryRepresentation_" + traj_uid);
        rel3.setId('_' + traj_uid + '_' + Guid::make().toXmlString());
        rel3.setDestinationObjectType();
        frel.addRelationship(rel3);

        frels_xml[i] = frel.toString();
    }

    BOOST_REQUIRE_EQUAL(size_t(frel_count), frels.size());

    if (oes::testing::arePerformanceTestsEnabled()) {
        std::cout << test_case_name() << " setup in " << setup.elapsed() << std::endl;
    }

    // Parse them now

    std::vector<epc::FileRelationship> frels_out(frel_count);
    SimpleTimer parse;
    size_t total_rels = 0;
    size_t byte_count = 0;
    for (int i = 0; i < frel_count; ++i) {
        const std::string& frel_xml = frels_xml[i];
        byte_count += frel_xml.length();

        epc::FileRelationship& frel = frels_out[i];
        frel.readFromXmlInPlace(frel_xml);
        total_rels += frel.getAllRelationship().size();
    }

    if (oes::testing::arePerformanceTestsEnabled()) {
        Throughput stats;
        stats.count_ = frel_count;
        stats.rels_ = total_rels;
        stats.bytes_ = byte_count;
        stats.secs_ = parse.elapsedSec();
        std::cout << test_case_name() << " parse: " << stats.toString() << std::endl;
    } else {
        BOOST_CHECK(frels == frels_out);
    }
}

// Simulates the 3 rels files with more than 10K relations from OSDU R1 wells EPC
BOOST_AUTO_TEST_CASE(bench_file_relationship_parsing_Mrels_per_file) {
    SimpleTimer setup;

    /*
    ** eml:EpcExternalPartReference
    **
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
    <Relationship Id="Hdf5File" Type="http://schemas.energistics.org/package/2012/relationships/externalResource" Target="OSDU_R1_Demo.h5" TargetMode="External"/>
    <Relationship Id="_faa04737-ccbd-4e7b-a6f0-cffc9b0bb11a_343334e0-1a29-4a71-9602-f567d8c9b170" Type="http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl" Target="obj_DeviationSurveyRepresentation_faa04737-ccbd-4e7b-a6f0-cffc9b0bb11a.xml"/>
    <Relationship Id="_852ba890-4ccf-4433-ba78-19d22b26e84b_3f2e0f1b-5460-4aac-9d8d-ef6aa0729b7c" Type="http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl" Target="obj_ContinuousProperty_852ba890-4ccf-4433-ba78-19d22b26e84b.xml"/>
    <Relationship Id="_b864d350-3c4f-4f55-bc1d-46095d61882c_6a99bdb0-4a94-4093-b60a-2dbb1f510adf" Type="http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl" Target="obj_ContinuousProperty_b864d350-3c4f-4f55-bc1d-46095d61882c.xml"/>
    <Relationship Id="_217fd9a8-0cdc-438e-8793-c883b358cbe7_d04e9235-5a75-4984-a91a-343364410110" Type="http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl" Target="obj_WellboreTrajectoryRepresentation_217fd9a8-0cdc-438e-8793-c883b358cbe7.xml"/>
    <Relationship Id="_0fca6fb2-28af-400b-98d3-a04e84617a8d_794e9321-50f9-462a-a307-f0994626a5cc" Type="http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl" Target="obj_WellboreMarkerFrameRepresentation_0fca6fb2-28af-400b-98d3-a04e84617a8d.xml"/>
    <Relationship Id="_5e17a076-d434-419c-917a-6ff731774289_9d6628c7-4fa9-4dfc-9842-166851c28d35" Type="http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl" Target="obj_DeviationSurveyRepresentation_5e17a076-d434-419c-917a-6ff731774289.xml"/>
    <Relationship Id="_55a67e15-d3ce-4c76-9975-42478e6d0fbe_a2916845-1e79-4f2b-848d-77d746c51d55" Type="http://schemas.energistics.org/package/2012/relationships/externalPartProxyToMl" Target="obj_DeviationSurveyRepresentation_55a67e15-d3ce-4c76-9975-42478e6d0fbe.xml"/>
    ...
</Relationships>
    */
    
    const int bulk_rel_count = oes::testing::perfValues(38, 38887);
    epc::FileRelationship bulk_frel;
    {
        epc::Relationship ext_rel;
        ext_rel.setIsExternalTarget();
        ext_rel.setTarget("OSDU_R1_Demo.h5");
        ext_rel.setId("Hdf5File");
        ext_rel.setExternalResourceType();
        bulk_frel.addRelationship(ext_rel);

        const std::vector<std::string> types{
            "obj_ContinuousProperty",
            "obj_WellboreTrajectoryRepresentation",
            "obj_WellboreMarkerFrameRepresentation",
            "obj_DeviationSurveyRepresentation",
        };
        for (int i = 0; i < bulk_rel_count - 1; ++i) {
            epc::Relationship rel;
            const std::string dest_uid = Guid::make().toXmlString();
            rel.setTarget(types[i & 0x3] + '_' + dest_uid);
            rel.setId('_' + dest_uid + '_' + Guid::make().toXmlString());
            rel.setExternalPartProxyToMlType();
            bulk_frel.addRelationship(rel);
        }
    }
    BOOST_REQUIRE_EQUAL(size_t(bulk_rel_count), bulk_frel.getAllRelationship().size());

    /*
    ** resqml2:LocalDepth3dCrs
    **
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
    <Relationship Id="_6928d403-0900-41dc-a5a1-e55be5e4659e_35b91f8e-c901-4eb4-b432-4c17befc46f5" Type="http://schemas.energistics.org/package/2012/relationships/destinationObject" Target="obj_PropertyKind_6928d403-0900-41dc-a5a1-e55be5e4659e.xml"/>
    <Relationship Id="_e150f7e9-085f-4550-b6a1-59dfd6982f28_ce2b364b-639f-4e3a-a2dc-41902340707b" Type="http://schemas.energistics.org/package/2012/relationships/destinationObject" Target="obj_PropertyKind_e150f7e9-085f-4550-b6a1-59dfd6982f28.xml"/>
    <Relationship Id="_52161c6c-697c-4716-99e1-efd06fae9692_e9856f82-cced-492c-b4c7-ed34ec556110" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_MdDatum_52161c6c-697c-4716-99e1-efd06fae9692.xml"/>
    <Relationship Id="_52161c6c-697c-4716-99e1-efd06fae9692_d44e619c-b934-45dd-afb6-8f0ef4aa9dc1" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_MdDatum_52161c6c-697c-4716-99e1-efd06fae9692.xml"/>
    <Relationship Id="_52161c6c-697c-4716-99e1-efd06fae9692_746138cd-8704-4997-b8df-8a00f62ee210" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_MdDatum_52161c6c-697c-4716-99e1-efd06fae9692.xml"/>
    <Relationship Id="_217fd9a8-0cdc-438e-8793-c883b358cbe7_148ba7b8-743d-4f94-a4ea-a68e2b476407" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_WellboreTrajectoryRepresentation_217fd9a8-0cdc-438e-8793-c883b358cbe7.xml"/>
    ...
</Relationship>
    */
    const int lcrs_rel_count = oes::testing::perfValues(23, 23701);
    epc::FileRelationship lcrs_frel;
    {
        for (int i = 0; i < 2; ++i) {
            epc::Relationship rel;
            const std::string dest_uid = Guid::make().toXmlString();
            rel.setTarget("obj_PropertyKind_" + dest_uid);
            rel.setId('_' + dest_uid + '_' + Guid::make().toXmlString());
            rel.setDestinationObjectType();
            lcrs_frel.addRelationship(rel);
        }

        const std::vector<std::string> types{
            "obj_MdDatum",
            "obj_MdDatum",
            "obj_MdDatum",
            "obj_WellboreTrajectoryRepresentation",
        };
        for (int i = 0; i < lcrs_rel_count - 2; ++i) {
            epc::Relationship rel;
            const std::string dest_uid = Guid::make().toXmlString();
            rel.setTarget(types[i & 0x3] + '_' + dest_uid);
            rel.setId('_' + dest_uid + '_' + Guid::make().toXmlString());
            rel.setSourceObjectType();
            lcrs_frel.addRelationship(rel);
        }
    }
    BOOST_REQUIRE_EQUAL(size_t(lcrs_rel_count), lcrs_frel.getAllRelationship().size());

    /*
    ** resqml2:PropertyKind
    **
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
    <Relationship Id="_52161c6c-697c-4716-99e1-efd06fae9692_9fa3aa29-0232-4c45-b1a9-408fed985516" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_MdDatum_52161c6c-697c-4716-99e1-efd06fae9692.xml"/>
    <Relationship Id="_52161c6c-697c-4716-99e1-efd06fae9692_56b519b2-d1a2-46e5-863c-19bc34b6131d" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_MdDatum_52161c6c-697c-4716-99e1-efd06fae9692.xml"/>
    <Relationship Id="_63979d1e-45dc-4767-ae58-14a2b3bbef35_79bf46a1-5351-4b50-a647-719f141e1b0b" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_MdDatum_63979d1e-45dc-4767-ae58-14a2b3bbef35.xml"/>
    <Relationship Id="_63979d1e-45dc-4767-ae58-14a2b3bbef35_84207534-c511-4a01-9916-da0f234ca6f9" Type="http://schemas.energistics.org/package/2012/relationships/sourceObject" Target="obj_MdDatum_63979d1e-45dc-4767-ae58-14a2b3bbef35.xml"/>
    ...
</Relationship>
    */
    const int ptyk_rel_count = oes::testing::perfValues(12, 11400); // must be even
    epc::FileRelationship ptyk_frel;
    {
        for (int i = 0; i < ptyk_rel_count / 2; ++i) {
            epc::Relationship rel;
            const std::string dest_uid = Guid::make().toXmlString();
            rel.setTarget("obj_MdDatum_" + dest_uid);
            rel.setId('_' + dest_uid + '_' + Guid::make().toXmlString());
            rel.setSourceObjectType();
            ptyk_frel.addRelationship(rel);
            ptyk_frel.addRelationship(rel); // they are all duplicated in that file...
        }
    }
    BOOST_REQUIRE_EQUAL(size_t(ptyk_rel_count), ptyk_frel.getAllRelationship().size());

    const std::string bulk_frel_xml = bulk_frel.toString();
    const std::string lcrs_frel_xml = lcrs_frel.toString();
    const std::string ptyk_frel_xml = ptyk_frel.toString();

    if (oes::testing::arePerformanceTestsEnabled()) {
        std::cout << test_case_name() << " setup in " << setup.elapsed() << std::endl;
    }

    // Parse them now

    epc::FileRelationship frel1;
    epc::FileRelationship frel2;
    epc::FileRelationship frel3;
    epc::FileRelationship* frels_in[]{
        &bulk_frel, &lcrs_frel, &ptyk_frel
    };
    epc::FileRelationship* frels_out[] {
        &frel1, &frel2, &frel3
    };
    const std::string* frels_xml[] {
        &bulk_frel_xml, &lcrs_frel_xml, &ptyk_frel_xml
    };

    for (int i = 0; i < 3; ++i) {
        epc::FileRelationship frel_out = *frels_out[i];
        const std::string& frel_xml = *frels_xml[i];

        SimpleTimer parse;
        frel_out.readFromXmlInPlace(frel_xml);
        if (oes::testing::arePerformanceTestsEnabled()) {
            Throughput stats;
            stats.count_ = 1;
            stats.rels_ = frel_out.size();
            stats.bytes_ = frel_xml.length();
            stats.secs_ = parse.elapsedSec();
            std::cout << test_case_name() << " parse: " << stats.toString() << std::endl;
        } else {
            epc::FileRelationship frel_in = *frels_in[i];
            BOOST_CHECK(frel_in == frel_out);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

//============================================================================

} // End anonymous namespace
