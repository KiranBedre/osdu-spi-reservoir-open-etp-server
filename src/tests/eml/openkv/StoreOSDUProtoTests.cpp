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
// ============================================================================

#include <oes/testing/eml/ServerFixture.h>

#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/Guid.h>

#include <oes/eml/etp12/MsgUtils.h>
#include <oes/testing/eml/ServerTesting.hpp>

#include <oes/eml/openkv/ArrayProto.h>
#include <oes/eml/openkv/DiscoProto.h>
#include <oes/eml/openkv/StoreOSDUProto.h>

#include <cmath>
#include <regex>

namespace {

//============================================================================

using namespace oes::eml;
using namespace oes::eml::etp12;
using namespace oes::eml::openkv;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

using oes::core::Guid;

//============================================================================

BOOST_FIXTURE_TEST_SUITE(StoreOSDUProtoTests, oes::testing::eml::OpenKVServerFixture)

BOOST_AUTO_TEST_CASE(test_StoreOSDU4_CopyDataObjectsByValue) {
    // create an ETP space we can PutDataObject into
    const auto space = putSpace();
    const auto space_uri = EmlUri::parse(space.m_uri);

    auto txUid = startTx(space.m_uri, false); // read-write transaction

    // ETP space should be empty, initially
    auto get_disco_rpc = makeRpc(&OpenKVDiscovery::GetResources);
    get_disco_rpc.req_.m_context.m_uri = space.m_uri;
    BOOST_REQUIRE(get_disco_rpc.call());
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 0);

    data12::Object::DataObject crs_obj = putCrsObj(space);

    // Check Resource was correctly put
    BOOST_REQUIRE(get_disco_rpc.call()); // should see 1 res now
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 1);
    BOOST_CHECK_EQUAL(get_disco_rpc.res_.m_resources[0].m_name, crs_obj.m_resource.m_name);
    
    // Copy the CRS
    auto copy_storeOSDU_rpc = makeRpc(&OpenKVStoreOSDU::CopyDataObjectsByValue);
    copy_storeOSDU_rpc.req_.m_uri = crs_obj.m_resource.m_uri;
    copy_storeOSDU_rpc.req_.m_sourcesDepth = 0;
    BOOST_REQUIRE(copy_storeOSDU_rpc.call());
    BOOST_REQUIRE_EQUAL(copy_storeOSDU_rpc.res_.m_copiedDataObjects.size(), 1);
    BOOST_REQUIRE(copy_storeOSDU_rpc.res_.m_copiedDataObjects[0] != crs_obj.m_resource.m_uri);

    // Check Resource was correctly copied
    BOOST_REQUIRE(get_disco_rpc.call()); // should see 2 res now
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 2);
    for (const auto& resource : get_disco_rpc.res_.m_resources) {
        BOOST_REQUIRE_EQUAL(EmlUri::parse(resource.m_uri).to_type(), "resqml20.obj_LocalDepth3dCrs");
    }
    
    //add a Grid2d to the repo
    auto grid2d_obj = putGrid2dObj(space);
    auto grid2d_ary = putGrid2dAry(space);
    
    BOOST_REQUIRE(get_disco_rpc.call()); // should see 4 res now
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 4);

    // Copy the CRS once again with a depth leading to the 2d grid copy
    copy_storeOSDU_rpc.req_.m_sourcesDepth = 2;
    BOOST_REQUIRE(copy_storeOSDU_rpc.call());
    BOOST_REQUIRE_EQUAL(copy_storeOSDU_rpc.res_.m_copiedDataObjects.size(), 2);

    bool txOK = commitTx(txUid);
    BOOST_CHECK(txOK);

    // Check Resources were correctly copied
    get_disco_rpc.req_.m_context.m_dataObjectTypes.emplace_back("resqml20.obj_Grid2dRepresentation");
    BOOST_REQUIRE(get_disco_rpc.call()); // should now see two grid 2d
    BOOST_REQUIRE_EQUAL(get_disco_rpc.res_.m_resources.size(), 2);
    oes::common::etp12::Datatypes::DataArrayTypes::DataArray dataArraySource = grid2d_ary.m_array;
    oes::common::etp12::Datatypes::DataArrayTypes::DataArray dataArrayTarget;
    for (const auto& resource : get_disco_rpc.res_.m_resources) {
        if (resource.m_uri != grid2d_obj.m_resource.m_uri) {
            Datatypes::DataArrayTypes::DataArrayIdentifier daIdentifier = grid2d_ary.m_uid;
            daIdentifier.m_pathInResource = std::regex_replace(daIdentifier.m_pathInResource, 
                std::regex(EmlUri::parse(grid2d_obj.m_resource.m_uri).guid_.toXmlString()),
                EmlUri::parse(resource.m_uri).guid_.toXmlString());
            auto get_dataArray_rpc = makeRpc(&OpenKVDataArray::GetDataArrays);
            get_dataArray_rpc.req_.m_dataArrays.emplace("0", daIdentifier);
            BOOST_REQUIRE(get_dataArray_rpc.call());
            dataArrayTarget = std::move(get_dataArray_rpc.res_.m_dataArrays.begin()->second);
        }
    }
    BOOST_REQUIRE(std::equal(
        dataArraySource.m_dimensions.begin(), 
        dataArraySource.m_dimensions.begin() + dataArraySource.m_dimensions.size(),
        dataArrayTarget.m_dimensions.begin()));
    BOOST_REQUIRE_EQUAL(dataArraySource.m_data.m_item.index(), dataArrayTarget.m_data.m_item.index());
    auto&& doubleDataArraySource = std::get<ArrayOfDouble>(dataArraySource.m_data.m_item).m_values;
    BOOST_REQUIRE(std::equal(doubleDataArraySource.begin(), 
        doubleDataArraySource.begin() + doubleDataArraySource.size(),
        std::get<ArrayOfDouble>(dataArrayTarget.m_data.m_item).m_values.begin()));
}

BOOST_AUTO_TEST_SUITE_END() // StoreOSDUProtoTests

//============================================================================

} // End anonymous namespace
