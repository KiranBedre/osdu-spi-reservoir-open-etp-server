// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/testing/eml/ServerFixture.h>
#include <oes/testing/eml/OpenKVClientServerFixture.h>
#include <oes/testing/eml/ServerTesting.hpp>

#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/Guid.h>

#include <oes/common/etp12/EmlUri.h>
#include <oes/eml/etp12/MsgUtils.h>
#include <oes/eml/openkv/DiscoProto.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <chrono>
#include <cmath>

namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;
using namespace oes::testing::eml;

using oes::core::Guid;

struct RelCounts {
    int sources_ = 0;
    int targets_ = 0;

    static RelCounts of(const Resource& res);
};
bool operator==(const RelCounts& lhs, const RelCounts& rhs) {
    return (lhs.targets_ == rhs.targets_) && (lhs.sources_== rhs.sources_);
}
std::ostream& operator<<(std::ostream& os, const RelCounts& r) {
    return os << "{ targets: " << r.targets_ << "; sources: " << r.sources_ << " }";
}
RelCounts RelCounts::of(const Resource& res) {
    int targets = res.m_targetCount.value_or(-1);
    int sources = res.m_sourceCount.value_or(-1);
    return RelCounts{ sources, targets };
};

//============================================================================

BOOST_FIXTURE_TEST_SUITE(DiscoProtoTests, oes::testing::eml::OpenKVServerFixture)

BOOST_AUTO_TEST_CASE(test_GetResources_multiple) {
    // Starting the server and pushing test data
    OpenKVClientServerFixture serverFixture;
    serverFixture.startServer();

    const std::string rel_path = "data/eml";

    const std::string project_well = "WELL_RESQML";
    const EmlUri space_well = EmlUri::make_space("A/" + project_well);
    serverFixture.directImport(
        oes::testing::resolveTestDataFile(rel_path, project_well + ".epc", true),
        space_well.dataspace_
    );
    const std::string project_pillar = "PillarGrid";
    const EmlUri space_pillar = EmlUri::make_space("A/" + project_pillar);
    serverFixture.directImport(
        oes::testing::resolveTestDataFile(rel_path, project_pillar + ".epc", true),
        space_pillar.dataspace_
    );

    auto get_rpc_well = makeRpc(&OpenKVDiscovery::GetResources);
    get_rpc_well.req_.m_context.m_uri = space_well.to_string();
    get_rpc_well.req_.m_context.m_depth = 3;
    get_rpc_well.req_.m_scope = Object::ContextScopeKind::eSelf;
    get_rpc_well.req_.m_countObjects = true; // what we are testing here

    auto get_rpc_pillar = makeRpc(&OpenKVDiscovery::GetResources);
    get_rpc_pillar.req_.m_context.m_uri = space_pillar.to_string();
    get_rpc_pillar.req_.m_context.m_depth = 3; // supposed to be ignored
    get_rpc_pillar.req_.m_scope = Object::ContextScopeKind::eSelf;
    get_rpc_pillar.req_.m_countObjects = true; // what we are testing here

    // call getResources on the two dataspaces, and ensure results are consistent
    BOOST_REQUIRE(get_rpc_well.call());
    BOOST_REQUIRE(get_rpc_pillar.call());

    BOOST_REQUIRE_EQUAL(7 , get_rpc_well.res_.m_resources.size());
    BOOST_REQUIRE_EQUAL(105 , get_rpc_pillar.res_.m_resources.size());

    BOOST_REQUIRE(get_rpc_well.call());
    BOOST_REQUIRE(get_rpc_pillar.call());

    BOOST_REQUIRE_EQUAL(7 , get_rpc_well.res_.m_resources.size());
    BOOST_REQUIRE_EQUAL(105 , get_rpc_pillar.res_.m_resources.size());
}

// Test m_countObject = true, when discovery an ETP dataspace
BOOST_AUTO_TEST_CASE(test_GetResources_countObjects) {
    // Starting the server and pushing test data
    OpenKVClientServerFixture serverFixture;
    serverFixture.startServer();

    const EmlUri space = EmlUri::make_space("A/B");
    const std::string rel_path = "data/eml";
    const std::string project = "WELL_RESQML";
    serverFixture.directImport(
        oes::testing::resolveTestDataFile(rel_path, project + ".epc", true),
        space.dataspace_
    );

    auto get_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    get_rpc.req_.m_context.m_uri = space.to_string();
    get_rpc.req_.m_context.m_depth = 3; // supposed to be ignored
    get_rpc.req_.m_scope = Object::ContextScopeKind::eSourcesOrSelf; // should be eSelf, but ignored anyway
    get_rpc.req_.m_countObjects = true; // what we are testing here

    BOOST_REQUIRE(get_rpc.call());
    BOOST_REQUIRE_EQUAL(7, get_rpc.res_.m_resources.size());

    auto getRes = [&get_rpc](const std::string& uri_fragment) -> const Resource* {
        for (const auto& res : get_rpc.res_.m_resources){
            if (res.m_uri.find(uri_fragment) != std::string::npos) {
                return &res;
            }
        }
        BOOST_FAIL("Resource not found");
        return nullptr;
    };
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("EpcExt")), (RelCounts{ 0, 0 })); // FIXME

    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("3dCrs")), (RelCounts{ 2, 0 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("Datum")), (RelCounts{ 1, 1 }));

    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreFeature")), (RelCounts{ 1, 0 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreInterpr")), (RelCounts{ 2, 1 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreMarkerF")), (RelCounts{ 0, 2 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreTraject")), (RelCounts{ 1, 3 }));

    const auto traj = *getRes("WellboreTraject");
    const auto traj_rc = RelCounts::of(traj);

    get_rpc.req_.m_context.m_uri = traj.m_uri;
    get_rpc.req_.m_context.m_depth = 1;
    get_rpc.req_.m_scope = Object::ContextScopeKind::eTargets;
    get_rpc.req_.m_countObjects = true;

    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(traj_rc.targets_, get_rpc.res_.m_resources.size());
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("3dCrs")), (RelCounts{ 2, 0 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("Datum")), (RelCounts{ 1, 1 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreInterpr")), (RelCounts{ 2, 1 }));

    get_rpc.req_.m_scope = Object::ContextScopeKind::eTargetsOrSelf;

    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(traj_rc.targets_ + 1, get_rpc.res_.m_resources.size());
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("3dCrs")), (RelCounts{ 2, 0 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("Datum")), (RelCounts{ 1, 1 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreInterpr")), (RelCounts{ 2, 1 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreTraject")), traj_rc);

    get_rpc.req_.m_scope = Object::ContextScopeKind::eSources;

    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(traj_rc.sources_, get_rpc.res_.m_resources.size());
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreMarkerF")), (RelCounts{ 0, 2 }));

    get_rpc.req_.m_scope = Object::ContextScopeKind::eSourcesOrSelf;

    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(traj_rc.sources_ + 1, get_rpc.res_.m_resources.size());
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreMarkerF")), (RelCounts{ 0, 2 }));
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreTraject")), traj_rc);

    get_rpc.req_.m_scope = Object::ContextScopeKind::eSelf;

    BOOST_REQUIRE(get_rpc.call());
    BOOST_CHECK_EQUAL(1, get_rpc.res_.m_resources.size());
    BOOST_CHECK_EQUAL(RelCounts::of(*getRes("WellboreTraject")), traj_rc);

    serverFixture.stopServer();
}

// FYI: Discovery-related tests formerly in OpenKVCmdsTests::test_check_init_check_import
BOOST_AUTO_TEST_CASE(test_GetResources) {
    // Starting the server and pushing test data
    OpenKVClientServerFixture serverFixture;
    serverFixture.startServer();

    const EmlUri space = EmlUri::make_space("ds1/prj1");
    const std::string rel_path = "data/eml";
    const std::string project = "WELL_RESQML";
    serverFixture.directImport(
        oes::testing::resolveTestDataFile(rel_path, project + ".epc", true),
        space.dataspace_
    );

    const size_t expected_obj_instance_count = 7;

    auto rpc = makeRpc(&OpenKVDiscovery::GetResources);

    Discovery::GetResources& req = rpc.req_;
    req.m_context.m_uri = space.to_string();
    req.m_context.m_depth = 1; // moot for eSelf I believe
    req.m_context.m_dataObjectTypes.clear(); // i.e. All types
    req.m_countObjects = false;
    req.m_scope = Object::ContextScopeKind::eSelf;
    Discovery::GetResourcesResponse& res = rpc.res_;
    auto sort_resources = [&rpc]() {
        std::sort(
            rpc.res_.m_resources.begin(), rpc.res_.m_resources.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.m_uri < rhs.m_uri;
            }
        );
        //!FIXME: Some objects are reported twice, removing them here, but we should not have to
        const auto& new_end_it = std::unique(rpc.res_.m_resources.begin(), rpc.res_.m_resources.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.m_uri == rhs.m_uri;
            }
        );
        rpc.res_.m_resources.resize(std::distance(rpc.res_.m_resources.begin(), new_end_it));
        // int i = 0;
        // for(const auto& rsrc: rpc.res_.m_resources) {
        //     std::cout << i++ << ": "<< rsrc.m_uri << std::endl;
        // }
        //! end FIXME
    };

    BOOST_REQUIRE(rpc.call());
    BOOST_CHECK_EQUAL(
        res.m_resources.size(),
        expected_obj_instance_count
    );
    std::map<std::string, std::string> expected_uri2title{
        { "eml:///dataspace('ds1/prj1')/eml20.obj_EpcExternalPartReference(e5e5230e-ecdd-4605-85ad-446926eeb47f)",                "Hdf5File" },
        { "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)",                  "Default local depth CRS" },
        { "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)",                          "49025112030000 location" },
        { "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreFeature(7d9e5e11-7fff-53c3-b645-121655c6922c)",                  "49025112030000 feature" },
        { "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)",           "49025112030000" },
        { "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)","BEST_MARKER_SET" },
        { "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)", "POSITION_OPTIMIZED" },
    };
    size_t zero_m_lastChanged = 0;
    // timestamps in micro-seconds UTC since Epoch
    constexpr int64_t oldestLastChanged = 1'465'810'594'000'000;
    for (const Object::Resource& rsrc : res.m_resources) {
        auto type_found = expected_uri2title.find(rsrc.m_uri);
        BOOST_REQUIRE(type_found != expected_uri2title.end());
        BOOST_CHECK_EQUAL(type_found->first, rsrc.m_uri);
        BOOST_CHECK_EQUAL(type_found->second, rsrc.m_name);
        if (rsrc.m_lastChanged > 0) {
            if (rsrc.m_name == "Hdf5File") { // timestamps in micro-seconds UTC since Epoch
                BOOST_CHECK_EQUAL(rsrc.m_lastChanged, 1'465'817'891'000'000); // 2016-06-13T11:38:11Z
            } else {
                BOOST_CHECK_EQUAL(rsrc.m_lastChanged, oldestLastChanged); // 2016-06-13T09:36:34Z
            }
        } else {
            BOOST_CHECK_EQUAL(rsrc.m_name, "Default local depth CRS");
            ++zero_m_lastChanged;
        }
        expected_uri2title.erase(type_found);
    }
    BOOST_CHECK_EQUAL(zero_m_lastChanged, size_t(1)); // CRS, see above
    BOOST_CHECK(expected_uri2title.empty());

    // Test last-changed-filtering
    req.m_storeLastWriteFilter = oldestLastChanged - 1;
    BOOST_REQUIRE(rpc.call());
    BOOST_CHECK_EQUAL(
        res.m_resources.size(),
        expected_obj_instance_count
    );

    // Test type-filtering

    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_MdDatum");
    BOOST_REQUIRE(rpc.call());
    BOOST_CHECK_EQUAL(res.m_resources.size(), size_t(1));

    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreMarkerFrameRepresentation");
    BOOST_REQUIRE(rpc.call());
    BOOST_CHECK_EQUAL(res.m_resources.size(), size_t(2));

    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_LocalDepth3dCrs");
    BOOST_REQUIRE(rpc.call());
    BOOST_CHECK_EQUAL(res.m_resources.size(), size_t(3));

    // reset last-change filtering to see it now
    req.m_storeLastWriteFilter.reset();
    BOOST_REQUIRE(rpc.call());
    BOOST_CHECK_EQUAL(res.m_resources.size(), size_t(3));
    BOOST_CHECK_EQUAL(res.m_resources.size(), req.m_context.m_dataObjectTypes.size()); // since 1 per-type

    //const auto prev_res1 = res;

    //// Redo the last request, this time using the space's UID, not its PATH
    //// Exercises the _by_uid_ caching/mapping into the _by_path_ cache
    //req.m_context.m_uri = ds1_prj1_uri_str;
    //BOOST_REQUIRE(rpc.call());
    //BOOST_CHECK_EQUAL(res.m_resources.size(), prev_res1.m_resources.size());

    //// Another by-UID request, but to a dataspace we haven't accessed yet.
    //// Will thus lookup into the main DB by-uid, not by-path.
    //req.m_context.m_uri = ds1_Para_uri_str;
    //BOOST_REQUIRE(rpc.call());
    //BOOST_CHECK_EQUAL(res.m_resources.size(), size_t(3));

    // Discovery of eTargets
    // First, lets start simple, with a direct targets only (i.e. depth=1),
    // or any type. Should find the parent resqml:WellboreFeature only.
    req.m_scope = Object::ContextScopeKind::eTargets;
    req.m_context.m_uri = "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)";
    req.m_context.m_depth = 1;
    req.m_context.m_dataObjectTypes.clear();
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreFeature(7d9e5e11-7fff-53c3-b645-121655c6922c)");

    // And self (uses the CTE query internally, so quite different impl)
    req.m_scope = Object::ContextScopeKind::eTargetsOrSelf;
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(2));
    sort_resources();
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreFeature(7d9e5e11-7fff-53c3-b645-121655c6922c)");
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");

    req.m_scope = Object::ContextScopeKind::eTargets;

    // Use an entity with more than 1 relation (3 in fact)
    req.m_context.m_uri = "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)";
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(3));
    sort_resources();
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");

    //Check secondary sources
    req.m_context.m_includeSecondarySources = true;
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(5));
    sort_resources();
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");
    BOOST_CHECK_EQUAL(res.m_resources[3].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)");
    BOOST_CHECK_EQUAL(res.m_resources[4].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");
    req.m_context.m_includeSecondarySources = false;

    // 1 more relation, if recursing one level down, the interp's feature
    req.m_context.m_depth = 2;
    BOOST_REQUIRE(rpc.call());
    //!FIXME: CRS is listed twice
    sort_resources();
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(4));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreFeature(7d9e5e11-7fff-53c3-b645-121655c6922c)");
    BOOST_CHECK_EQUAL(res.m_resources[3].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");

    // Use an entity with more than 1 relation
    req.m_context.m_uri = "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)";
    req.m_context.m_depth = 1;
    BOOST_REQUIRE(rpc.call());
    sort_resources();
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(2));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");

    // This entity OTOH, the MarkerFrame, has the Trajectory are relation,
    // so recursing combines the 3 Trajectory relations, with the two above,
    // except they both share the same WellInterp, sp 3+2-1=4
    req.m_context.m_depth = 2;
    BOOST_REQUIRE(rpc.call());
    //!FIXME: WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52) is listed twice
    sort_resources();
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(5));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreFeature(7d9e5e11-7fff-53c3-b645-121655c6922c)");
    BOOST_CHECK_EQUAL(res.m_resources[3].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");
    //BOOST_CHECK_EQUAL(res.m_resources[4].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");

    // Filtering when finding eTargets
    req.m_scope = Object::ContextScopeKind::eTargets;
    req.m_context.m_uri = "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)";
    req.m_context.m_depth = 1;
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreTrajectoryRepresentation");
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");

    req.m_scope = Object::ContextScopeKind::eTargetsOrSelf;
    req.m_context.m_depth = 2;
    req.m_context.m_dataObjectTypes.clear();
    BOOST_REQUIRE(rpc.call());
    //!FIXME:
    sort_resources();
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(6));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");                   // level 2
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                           // level 2
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreFeature(7d9e5e11-7fff-53c3-b645-121655c6922c)");                   // level 2
    BOOST_CHECK_EQUAL(res.m_resources[3].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");            // level 2
    BOOST_CHECK_EQUAL(res.m_resources[4].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // eSelf
    BOOST_CHECK_EQUAL(res.m_resources[5].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");  // level 1

    // Filtering when finding eTargetsOrSelf
    // Search for object type on each level
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreMarkerFrameRepresentation"); // eSelf
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreTrajectoryRepresentation");  // level 1
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_LocalDepth3dCrs");                   // level 2
    BOOST_REQUIRE(rpc.call());
    sort_resources();
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(3));  // 3 out of 6
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");                   // level 2
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // eSelf
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");  // level 1

    // Search for object type on the first level only
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreTrajectoryRepresentation"); // level 1
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));  // 1 out of 6
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)"); // level 1

    // Search for object type on the second level only
    // Notice that the first level object type "obj_WellboreTrajectoryRepresentation" not requested
    // and we are aiming for object type on the 2nd level.
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_MdDatum");  // level 2
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));  // 1 out of 6
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");  // level 2

    // Search for object type on the second level and self, ignoring the intermediate first level
    // Notice that the first level object type "obj_WellboreTrajectoryRepresentation" not requested
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_MdDatum");                           // level 2
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreMarkerFrameRepresentation"); // eSelf
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(2));  // 2 out of 6
    sort_resources();
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                           // level 2
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // eSelf

    // Now doing the REVERSE, i.e. finding eSources
    req.m_context.m_depth = 1;
    req.m_scope = Object::ContextScopeKind::eSources;
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_uri = "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)";
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(2));
    sort_resources(); // Both the Datum and WellTraj reference the CRS
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                           // level 1
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");  // level 1

    //Check secondary targets
    req.m_context.m_includeSecondaryTargets = true;
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(4));
    sort_resources(); // Both the Datum and WellTraj reference the CRS
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)"); // Secondary target
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                           // level 1
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreInterpretation(ec43b905-4faf-5284-b279-aa7b9b22cd52)");  // Secondary target
    BOOST_CHECK_EQUAL(res.m_resources[3].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");  // level 1
    req.m_context.m_includeSecondaryTargets = false;

    // Filtering when finding eSources
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreTrajectoryRepresentation"); // level 1
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)"); // level 1

    req.m_scope = Object::ContextScopeKind::eSourcesOrSelf;
    req.m_context.m_dataObjectTypes.clear();
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(3));
    sort_resources();
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");                  // eSelf
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                          // level 1
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)"); // level 1

    // Filtering when finding eSourcesOrSelf
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreTrajectoryRepresentation");
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1)); // level 1
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)"); // level 1

    req.m_context.m_depth = 2;
    req.m_scope = Object::ContextScopeKind::eSources;
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_uri = "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)";
    BOOST_REQUIRE(rpc.call());
    //!FIXME:
    sort_resources(); // MarkerFrame references WellTraj, so gets added too
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(3));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                           // level 1
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // level 2
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");  // level 1

    req.m_scope = Object::ContextScopeKind::eSourcesOrSelf;
    req.m_context.m_dataObjectTypes.clear();
    BOOST_REQUIRE(rpc.call());
    //!FIXME: obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1 is present twice
    sort_resources(); // Just adds eSelf, i.e. the CRS
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(4));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");                   // eSelf
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                           // level 1
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // level 2
    BOOST_CHECK_EQUAL(res.m_resources[3].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");  // level 1

    // Filtering on eSources
    req.m_scope = Object::ContextScopeKind::eSources;
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreTrajectoryRepresentation"); // level 1
    BOOST_REQUIRE(rpc.call());
    //!FIXME: obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1) is listed twice
    sort_resources();
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)"); // level 1

    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreMarkerFrameRepresentation"); // level 2
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));
    sort_resources(); // MarkerFrame references WellTraj, so gets added too
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // level 2

    // Filtering on eSourcesOrSelf
    // Search for object type on each level
    req.m_scope = Object::ContextScopeKind::eSourcesOrSelf;
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_LocalDepth3dCrs");                   // eSelf
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_MdDatum");                           // level 1
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreMarkerFrameRepresentation"); // level 2
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(3));  // 3 out of 4
    sort_resources();
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");                   // eSelf
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_MdDatum(0ee22f34-1049-5b48-b0e9-c5eb4e4f340f)");                           // level 1
    BOOST_CHECK_EQUAL(res.m_resources[2].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // level 2

    // Search for object type on the first level only
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreTrajectoryRepresentation"); // level 1
    BOOST_REQUIRE(rpc.call());

    //! FIXME: obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1) is listed twice
    sort_resources();
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));  // 1 out of 4
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreTrajectoryRepresentation(4a6e1e5a-54f3-589a-b197-6e2ce35699f1)");  // level 1

    // Search for object type on the second level only
    // Notice that the first level object type "obj_WellboreTrajectoryRepresentation" not requested
    // and we are aiming for object type on the 2nd level.
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreMarkerFrameRepresentation"); // level 2
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(1));  // 1 out of 4
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // level 2

    // Search for object type on the second level and self, ignoring the intermediate first level
    // Notice that the first level object type "obj_WellboreTrajectoryRepresentation" not requested
    req.m_context.m_dataObjectTypes.clear();
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_LocalDepth3dCrs");                   // eSelf
    req.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_WellboreMarkerFrameRepresentation"); // level 2
    BOOST_REQUIRE(rpc.call());
    BOOST_REQUIRE_EQUAL(res.m_resources.size(), size_t(2));  // 2 out of 4
    sort_resources();
    BOOST_CHECK_EQUAL(res.m_resources[0].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_LocalDepth3dCrs(1c8bb605-c8e7-4da9-af4c-52e5a5e5147b)");                   // eSelf
    BOOST_CHECK_EQUAL(res.m_resources[1].m_uri, "eml:///dataspace('ds1/prj1')/resqml20.obj_WellboreMarkerFrameRepresentation(c76021a6-c1e7-5b2d-848a-35764da8001d)"); // level 2


    serverFixture.stopServer();
}

BOOST_AUTO_TEST_SUITE_END() // DiscoProtoTests

//============================================================================

} // End anonymous namespace
