// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2022 EPAM Systems
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

#include <oes/testing/eml/OsduOpenKVClientServerFixture.h>
#include <oes/testing/core/TestMain.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <thread>
#include <future>
#include <string>

using namespace oes::eml::etp12;
using namespace oes::testing::eml;
using namespace oes::common::etp12::Protocol;
using namespace oes::common::etp12::Datatypes;

namespace {

BOOST_FIXTURE_TEST_SUITE(OsduEtp12ServerTests, OsduOpenKVClientServerFixture)

BOOST_AUTO_TEST_CASE(test_max_msg_size_negotiation) {
    svr_conf_.max_mgs_size_ = 30*1000*1000; // 30 MB

    startServer();

    // Connect to server, using BASIC Auth client and first data partition
    {
        clt_conf_.max_mgs_size_ = 50*1024*1024; // 50 MiB
        clt_conf_.withBasicAuth("foo", "bar");

        const std::string data_partition_id = "osdu_test_data_partition";
        auto clt = connectClient(data_partition_id);

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);

        // server's value is smaller, so the negotiated one
        BOOST_CHECK_EQUAL(clt->negotiated_max_msg_size(), svr_conf_.max_mgs_size_);
    }

    // Connect to server, using JWT Auth client and second data partition
    {
        clt_conf_.max_mgs_size_ = 15*1024*1024; // 15 MiB
        clt_conf_.withJwtAuth("foo", "bar", "victoria");

        const std::string data_partition_id = "opendes_test_data_partition";
        auto clt = connectClient(data_partition_id);

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);

        // client's value is smaller, so the negotiated one
        BOOST_CHECK_EQUAL(clt->negotiated_max_msg_size(), clt_conf_.max_mgs_size_);
    }
}

BOOST_AUTO_TEST_CASE(test_auth_via_query_string) {
    startServer();

    // Connect to server, using BASIC Auth client and first data partition
    {
        clt_conf_.emulate_browser_client_ = true;
        clt_conf_.withBasicAuth("foo", "bar");

        const std::string data_partition_id = "osdu_test_data_partition";
        auto clt = connectClient(data_partition_id);

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);
    }

    // Connect to server, using JWT Auth client and second data partition
    {
        clt_conf_.emulate_browser_client_ = true;
        clt_conf_.withJwtAuth("foo", "bar", "victoria");

        const std::string data_partition_id = "opendes_test_data_partition";
        auto clt = connectClient(data_partition_id);

        // And once connected, start a conversation
        IProtocol* proto_ds = clt->getProtocol(Datatypes::Protocol::eDataspace);
        BOOST_CHECK(proto_ds);
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
