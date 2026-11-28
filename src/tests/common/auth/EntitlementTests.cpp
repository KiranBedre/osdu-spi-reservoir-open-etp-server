// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <map>
#include <fstream>
#include <stdlib.h>

#include <oes/common/auth/Entitlement.h>
#include <oes/common/auth/AuthN.h>

#include <oes/osdu/api/EntitlementClient.h>
#include <oes/osdu/api/BaseClient.h>
#include <oes/osdu/api/EntitlementDataspace.h>
#include <oes/osdu/api/EntitlementUtils.h>
#include <oes/osdu/api/EntitlementClientPolicy.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <fmt/format.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cstdlib>
#include <oes/core/utils/Json.hpp>
#include <oes/core/http/HTTP.h>
#include <oes/core/utils/Guid.h>

using oes::common::auth::AuthenticatedUser;
using oes::common::auth::AuthenticatedUser_ptr;
using oes::common::auth::Entitlement;
using oes::common::auth::EntitlementGroups;
using oes::common::auth::EntitlementPolicy;
using oes::common::auth::EntitlementConfig;
using namespace oes::osdu::api;
using namespace oes::common::auth;
namespace {

class EntitlementOSDUConfig_Test : public EntitlementConfigBase {
public:
    EntitlementOSDUConfig_Test() {
        storage_url_ = "STORAGE_URL";
        entitlements_url_ = "ENTITLEMENTS_URL";
        data_partition_id_ = "DATA_PARTITION_ID";
        schema_id_ = "SCHEMA_ID" ;
        domain_name_ = "DOMAIN_NAME";
        legal_tags_ = "LEGAL_TAGS";
        legal_countries_ = "LEGAL_COUNTRIES";
        connectivity_mode_ = "osdu";
        evaluatePolicyPath_ = "/evaluate";
        policy_delegate_url_ = "POLICY_URL";
        records_path_ = "/records";
        authorization_mode_ = "osdu";
        authorization_bulk_size_ = "2";
    }

    static EntitlementConfig_ptr make() {
        return std::make_shared<EntitlementOSDUConfig_Test>();
    }
};

class EntitlementOSDUConfig_Real : public EntitlementConfigBase {
public:
    EntitlementOSDUConfig_Real() {
        static const char* conf_file = "/source/src/tests/data/postgresql/real_osdu_entitlements.json";
        std::ifstream ifs(conf_file);
        if (!ifs.good()) {
            return;
        }
        try {
            nlohmann::json j = nlohmann::json::parse(ifs);
            storage_url_ = j["storage_url"];
            entitlements_url_ = j["entitlements_url"];
            data_partition_id_ = j["data_partition_id"];
            schema_id_ = j["schema_id"];
            domain_name_ = j["domain_name"];
            legal_tags_ = j["legal_tags"];
            legal_countries_ = j["legal_countries"];
        } catch (...) {
            return;
        }
        has_config_ = true;
        connectivity_mode_ = "osdu";
        evaluatePolicyPath_ = "/evaluate";
        authorization_mode_ = "osdu";
    }

    static EntitlementConfig_ptr make() {
        return std::make_shared<EntitlementOSDUConfig_Real>();
    }
public:
    bool has_config_ = false;
};

//--

using namespace testing;

class HTTPMock : public oes::core::HTTP {
public:
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4373)  // For previous versions of the compiler did not override when parameters only differed by const/volatile qualifiers
#endif
    MOCK_METHOD4 (get, Response (const std::string&, const size_t, const Headers&, const Options&));
    MOCK_METHOD5 (post, Response (const std::string&, const size_t, const Headers&, const std::string&, const Options&));
    MOCK_METHOD5 (put, Response (const std::string&, const size_t, const Headers&, const std::string&, const Options&));
    MOCK_METHOD4 (del, Response (const std::string&, const size_t, const Headers&, const Options&));
#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif
};

using HTTPMock_ptr = std::shared_ptr<HTTPMock>;

//--

class EntitlementFixture {
public:
    EntitlementFixture() : 
        cfg_(std::make_shared<EntitlementOSDUConfig_Test>()),
        access_token_("ACCESS_TOKEN"),
        http_ptr_(std::make_shared<HTTPMock>()) {
        base_client_ = std::make_shared<oes::osdu::api::BaseClient>(
            cfg_->dataPartitionId(), 
            [this](){ 
                return http_ptr_; 
            }
        );
    }

public:
    EntitlementConfig_ptr cfg_;
    std::string access_token_;
    oes::osdu::api::BaseClient_ptr base_client_;
    HTTPMock_ptr http_ptr_;
};

//--

BOOST_FIXTURE_TEST_SUITE(EntitlementTests, EntitlementFixture)

// EntitlementDataspace

BOOST_AUTO_TEST_CASE(EntitlementDataspaceGroups_isValid) {
    EntitlementDataspaceGroups ds;
    BOOST_REQUIRE(!ds.isValid());
    BOOST_REQUIRE(!bool(ds));
    ds = { "path", "uuid" , std::vector<std::string>(), std::vector<std::string>()};
    BOOST_REQUIRE(ds.isValid());
    BOOST_REQUIRE(bool(ds));
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceGroups_idForQuery) {
    EntitlementDataspaceGroups ds("uuid", "path", std::vector<std::string>(), std::vector<std::string>());
    std::string expected = "DATA_PARTITION_ID:dataset--ETPDataspace:uuid";
    BOOST_REQUIRE_EQUAL(ds.idForQuery(*cfg_), expected);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspace_deserialize) {
    const std::string response_body = 
R"rb(
    {
        "acl": {
            "owners": [
                "data.rdms-TEST_DATASPACE_PATH.owners@DATA_PARTITION_ID.DOMAIN_NAME"
            ],
            "viewers": [
                "data.rdms-TEST_DATASPACE_PATH.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
            ]
        }
    }
)rb";
    EntitlementDataspaceGroups ds;
    BOOST_REQUIRE(ds.deserialize(response_body));
    std::set expected{"data.rdms-TEST_DATASPACE_PATH.viewers@DATA_PARTITION_ID.DOMAIN_NAME"};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(ds.viewers_group_.begin(), ds.viewers_group_.end(), expected.begin(), expected.end());
    std::set expected1{"data.rdms-TEST_DATASPACE_PATH.owners@DATA_PARTITION_ID.DOMAIN_NAME"};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(ds.owners_group_.begin(), ds.owners_group_.end(), expected1.begin(), expected1.end());
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceGroups_serialize) {
        const std::string expected = nlohmann::json::parse(
R"rb(
[
  {
    "acl": {
      "owners": [
        "data.rdms-TEST_DATASPACE_PATH.owners@DATA_PARTITION_ID.DOMAIN_NAME"
      ],
      "viewers": [
        "data.rdms-TEST_DATASPACE_PATH.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
      ]
    },
    "data": {
      "DatasetProperties": {
        "URI": "eml:///dataspace(/folder-name/project-name)"
      }
    },
    "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
    "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
    "legal": {
      "legaltags": [
        "US", "UK"
      ],
      "otherRelevantDataCountries": [
        "FR"
      ],
      "status": "compliant"
    }
  }
]
)rb").dump(2);

    EntitlementDataspaceGroups ds("uuid", "/folder-name/project-name", {"US","UK"}, {"FR"});
    ds.addGroup(EntitlementGroups::VIEWERS, "data.rdms-TEST_DATASPACE_PATH.viewers@DATA_PARTITION_ID.DOMAIN_NAME");
    ds.addGroup(EntitlementGroups::OWNERS, "data.rdms-TEST_DATASPACE_PATH.owners@DATA_PARTITION_ID.DOMAIN_NAME");

    auto result = ds.serialize(*cfg_);
    
    BOOST_REQUIRE_EQUAL(nlohmann::json::parse(result).dump(2), expected);
}

// EntitlementDataspacePolicy

BOOST_AUTO_TEST_CASE(EntitlementDataspacePolicy_isValid) {
    EntitlementDataspacePolicy ds;
    BOOST_REQUIRE(!ds.isValid());
    BOOST_REQUIRE(!bool(ds));
    PermissionMap map = {};
    ds = EntitlementDataspacePolicy("path", "uuid", "DS_URI", std::vector<std::string>(), std::vector<std::string>());
    BOOST_REQUIRE(ds.isValid());
    BOOST_REQUIRE(bool(ds));
}

BOOST_AUTO_TEST_CASE(EntitlementDataspacePolicy_serialize) {
        const std::string expected = nlohmann::json::parse(
R"rb(
[
  {
    "data": {
      "DatasetProperties": {
        "URI": "DS_URI"
      }
    },
    "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
    "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
    "legal": {
      "legaltags": [
        "US", "UK"
      ],
      "otherRelevantDataCountries": [
        "FR"
      ],
      "status": "compliant"
    }
  }
]
)rb").dump(2);

    EntitlementDataspacePolicy ds("uuid", "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"});
    auto result = ds.serialize(*cfg_);
    
    BOOST_REQUIRE_EQUAL(nlohmann::json::parse(result).dump(2), expected);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspacePolicy_serializePolicy) {


    auto config = EntitlementOSDUConfig_Test::make();
    EntitlementDataspacePolicy ds("uuid", "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"});
    auto result = ds.serializePolicy(*config, DSPermissionFlags::eCreate);

    const std::string expected = nlohmann::json::parse(
R"rb(
{
  "input": {
    "operation": "Create",
    "records": [
      {
        "data": {
          "DatasetProperties": {
            "URI": "DS_URI"
          }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
          "legaltags": [
            "US",
            "UK"
          ],
          "otherRelevantDataCountries": [
            "FR"
          ],
          "status": "compliant"
        }
      }
    ]
  }
}
)rb").dump(2);
    
    BOOST_REQUIRE_EQUAL(nlohmann::json::parse(result).dump(2), expected);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspacePolicy_serializeSimplePolicy) {

    EntitlementDataspacePolicy ds("uuid", "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"});
    auto result = ds.serializeSimplePolicy(DSPermissionFlags::eCreate);

    const std::string expected = nlohmann::json::parse(
R"rb(
{
  "operation": "Create",
  "dataUris": ["DS_URI"]
}
)rb").dump(2);
    
    BOOST_REQUIRE_EQUAL(nlohmann::json::parse(result).dump(2), expected);
}

// EntitlementDataspacePolicyBulk

BOOST_AUTO_TEST_CASE(EntitlementDataspacePolicyBulk_serialize_singleDS) {
    EntitlementDataspacePolicyBulk ds({"DS_URI"}, std::vector<std::string>(), std::vector<std::string>());
    
    auto config = EntitlementOSDUConfig_Test::make();
    config->setAuthorizationMode("simple");

    auto result = ds.serialize(*config, DSPermissionFlags::eCreate);
    BOOST_REQUIRE_EQUAL(1, result.size());

    const std::string expected = nlohmann::json::parse(
R"rb(
{
  "operation": "Create",
  "dataUris": ["DS_URI"]
}
)rb").dump(2);
    
    BOOST_REQUIRE_EQUAL(nlohmann::json::parse(result.at(0)).dump(2), expected);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspacePolicyBulk_serialize_multipleDS) {
    EntitlementDataspacePolicyBulk ds({"DS_URI_1", "DS_URI_2", "DS_URI_3"}, std::vector<std::string>(), std::vector<std::string>());
    
    auto config = EntitlementOSDUConfig_Test::make(); // bulk size is 2
    config->setAuthorizationMode("simple");
    
    auto result = ds.serialize(*config, DSPermissionFlags::eCreate);

    BOOST_REQUIRE_EQUAL(2, result.size()); // we expect two bodies

    const std::string expected_1 = nlohmann::json::parse(
R"rb(
{
  "operation": "Create",
  "dataUris": ["DS_URI_1", "DS_URI_2"]
}
)rb").dump(2);
    
    const std::string expected_2 = nlohmann::json::parse(
R"rb(
{
  "operation": "Create",
  "dataUris": ["DS_URI_3"]
}
)rb").dump(2);
    
    BOOST_REQUIRE_EQUAL(nlohmann::json::parse(result.at(0)).dump(2), expected_1);
    BOOST_REQUIRE_EQUAL(nlohmann::json::parse(result.at(1)).dump(2), expected_2);
}

// EntitlementClient

BOOST_AUTO_TEST_CASE(EntitlementClientGroups_listGroups) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
    {
    "desId": "andrey.rodenko@emerson.com",
    "memberEmail": "andrey.rodenko@emerson.com",
    "groups": [
        {
            "name": "users",
            "description": "Datalake users",
            "email": "users@opendes.contoso.com"
        },
        {
            "name": "service.entitlements.user",
            "description": "Datalake Entitlements users",
            "email": "service.entitlements.user@opendes.contoso.com"
        },
        {
            "name": "data.default.viewers",
            "description": "Default data viewers",
            "email": "data.default.viewers@opendes.contoso.com"
        },
        {
            "name": "service.entitlements.admin",
            "description": "Datalake Entitlements admins",
            "email": "service.entitlements.admin@opendes.contoso.com"
        },
        {
            "name": "data.default.owners",
            "description": "Default data owners",
            "email": "data.default.owners@opendes.contoso.com"
        }
    ]
}
)rr";
    std::string email_expected_v = "andrey.rodenko@emerson.com";
    std::vector<std::string> groups_expected_v {
        "users@opendes.contoso.com",
        "service.entitlements.user@opendes.contoso.com",
        "data.default.viewers@opendes.contoso.com",
        "service.entitlements.admin@opendes.contoso.com",
        "data.default.owners@opendes.contoso.com"
    };

    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    oes::osdu::api::EntitlementClient entitlement_client(cfg_, base_client_);
    EntitlementError err;
    auto [memberEmail, groups] = entitlement_client.listGroups(access_token_, err);

    BOOST_REQUIRE(err.not_err());
    BOOST_REQUIRE(err.code_ == res.code);

    BOOST_REQUIRE(memberEmail == email_expected_v);

    BOOST_REQUIRE(groups.size() == groups_expected_v.size());
    for (const auto& e : groups_expected_v) {
        BOOST_REQUIRE(groups.find(e) != groups.end());
    }
}

// EntitlementClientPolicy

BOOST_AUTO_TEST_CASE(EntitlementClientPolicy_getUserPermission_owners) {/////
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
    {
    "desId": "name.surname@aspentech.com",
    "memberEmail": "name.surname@aspentech.com",
    "groups": [
        {
            "name": "some.name",
            "description": "some description",
            "email": "service.reservoir-dms.owners@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    oes::osdu::api::EntitlementClientPolicy client(cfg_, base_client_);
    EntitlementError err;

    auto perm = client.getUserPermission(access_token_, err);

    BOOST_REQUIRE(err.not_err());
    BOOST_REQUIRE(err.code_ == res.code);
    
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eRead));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eUpdate));
    
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eRead));
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eUpdate));
}

BOOST_AUTO_TEST_CASE(EntitlementClientPolicy_getUserPermission_viewers) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
    {
    "desId": "name.surname@aspentech.com",
    "memberEmail": "name.surname@aspentech.com",
    "groups": [
        {
            "name": "some.name",
            "description": "some description",
            "email": "service.reservoir-dms.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    oes::osdu::api::EntitlementClientPolicy client(cfg_, base_client_);
    EntitlementError err;

    auto perm = client.getUserPermission(access_token_, err);

    BOOST_REQUIRE(err.not_err());
    BOOST_REQUIRE(err.code_ == res.code);
    
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eRead));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eUpdate));
    
    BOOST_REQUIRE(!perm.hasPermission(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(!perm.hasPermission(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eRead));
    BOOST_REQUIRE(!perm.hasPermission(DSPermissionFlags::eUpdate));
}

BOOST_AUTO_TEST_CASE(EntitlementClientPolicy_getUserPermission_notvalid) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
    {
    "desId": "name.surname@aspentech.com",
    "memberEmail": "name.surname@aspentech.com",
    "groups": [
        {
            "name": "some.name",
            "description": "some description",
            "email": "service.reservoir-dms.NOT_VALID@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    oes::osdu::api::EntitlementClientPolicy client(cfg_, base_client_);
    EntitlementError err;

    auto perm = client.getUserPermission(access_token_, err);

    BOOST_REQUIRE(err.not_err());
    BOOST_REQUIRE(err.code_ == res.code);
    
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eRead));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eUpdate));
    
    BOOST_REQUIRE(!perm.hasPermission(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(!perm.hasPermission(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(!perm.hasPermission(DSPermissionFlags::eRead));
    BOOST_REQUIRE(!perm.hasPermission(DSPermissionFlags::eUpdate));
}

BOOST_AUTO_TEST_CASE(EntitlementClientPolicy_getUserPermission_simple) {
    
    auto config = std::make_shared<EntitlementOSDUConfig_Test>();
    config->setAuthorizationMode("simple");

    oes::osdu::api::EntitlementClientPolicy client(config, base_client_);
    EntitlementError err;

    auto perm = client.getUserPermission(access_token_, err);

    //BOOST_REQUIRE(err.not_err());
    //BOOST_REQUIRE(err.code_ == res.code);
    
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eRead));
    BOOST_REQUIRE(perm.isValid(DSPermissionFlags::eUpdate));
    
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eCreate));
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eDelete));
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eRead));
    BOOST_REQUIRE(perm.hasPermission(DSPermissionFlags::eUpdate));
}

// EntitlementDataspaceClient

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClient_createDataspaceRecord) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Authorization: Bearer ACCESS_TOKEN" 
    };
    std::string body =
R"rr0(
[
    {
        "acl": {
            "owners": [
                "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"
            ],
            "viewers": [
                "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
            ]
        },
        "data": {
            "DatasetProperties": {
                "URI": "eml:///dataspace(/path1/path2)" 
            }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
            "legaltags": [
                "LEGAL_TAGS"
            ],
            "otherRelevantDataCountries": [
                "LEGAL_COUNTRIES"
            ],
            "status": "compliant"
        }
    }
]
)rr0";
    body = nlohmann::json::parse(body).dump();

    oes::core::HTTP::Response res;
    res.code = 201;
    res.body = 
R"rr1(
{
    "recordCount": 1,
    "recordIds": [
        "DATA_PARTITION_ID:dataset--ETPDataspace:uuid"
    ],
    "skippedRecordIds": [],
    "recordIdVersions": [
        "DATA_PARTITION_ID:dataset--ETPDataspace:uuid:1656576985408681"
    ]
}
)rr1";
    const std::string req_url = "STORAGE_URL/records";

    EXPECT_CALL (*http_ptr_, put (req_url, _, header, body, _))
        .WillOnce (Return (res));

    EntitlementDataspaceGroups ds("uuid", "/path1/path2", std::vector<std::string>(), std::vector<std::string>());
    ds.addGroup(EntitlementGroups::VIEWERS, "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME");
    ds.addGroup(EntitlementGroups::OWNERS, "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME");
    oes::osdu::api::EntitlementDataspaceClient entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    bool ret = entitlement_ds_client.createDataspaceRecord(access_token_, ds, err);

    BOOST_REQUIRE(ret);
    BOOST_REQUIRE(err.code_ == res.code);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClient_delDataspaceRecord) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Authorization: Bearer ACCESS_TOKEN" 
    };

    oes::core::HTTP::Response res;
    res.code = 204;
    const std::string req_url = "STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid:delete";

    EXPECT_CALL (*http_ptr_, post (req_url, _, header, std::string(), _))
        .WillOnce (Return (res));

    EntitlementDataspaceGroups ds("uuid", "/path1/path2", std::vector<std::string>(), std::vector<std::string>());
    oes::osdu::api::EntitlementDataspaceClient entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    bool ret = entitlement_ds_client.delDataspaceRecord(access_token_, ds, err);

    BOOST_REQUIRE(ret);
    BOOST_REQUIRE(err.code_ == res.code);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClient_getDataspaceRecord) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN" 
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body =
R"rr0(
{
    "acl": {
        "owners": [
            "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"
        ],
        "viewers": [
            "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
        ]
    },
    "data": {
        "DatasetProperties": {
            "URI": "eml:///dataspace(/path1/path2)" 
        }
    },
    "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
    "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
    "legal": {
        "legaltags": [
            "LEGAL_TAGS"
        ],
        "otherRelevantDataCountries": [
            "LEGAL_COUNTRIES"
        ],
        "status": "compliant"
    }
}
)rr0";

    const std::string req_url = "STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid";

    EXPECT_CALL (*http_ptr_, get (req_url, _, header, _))
        .WillOnce (Return (res));

    EntitlementDataspaceGroups ds("uuid", "/path1/path2", std::vector<std::string>(), std::vector<std::string>());
    oes::osdu::api::EntitlementDataspaceClient entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    bool ret = entitlement_ds_client.getDataspaceRecord(access_token_, ds, err);

    std::set expected{"data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(ds.viewers_group_.begin(), ds.viewers_group_.end(), expected.begin(), expected.end());
    std::set expected1{"data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"};
    BOOST_REQUIRE_EQUAL_COLLECTIONS(ds.owners_group_.begin(), ds.owners_group_.end(), expected1.begin(), expected1.end());
    BOOST_REQUIRE(ret);
    BOOST_REQUIRE(err.code_ == res.code);
}

// EntitlementDataspaceGroupsBulk

namespace {
EntitlementDataspaceGroupsBulk makeGroupsBulk() {
    return EntitlementDataspaceGroupsBulk({
        std::make_shared<EntitlementDataspaceGroups>("uuid1", "/path1", std::vector<std::string>(), std::vector<std::string>()),
        std::make_shared<EntitlementDataspaceGroups>("uuid2", "/path2", std::vector<std::string>(), std::vector<std::string>())
    });
}

const char* GROUPS_BULK_QUERY = "?kind=SCHEMA_ID%3awks%3adataset--ETPDataspace%3a1.0.0&limit=100";

const char* GROUPS_BULK_PAGE = R"rr0(
{
    "cursor": "c1=",
    "results": [
        {
            "acl": {
                "owners": ["owners1@DATA_PARTITION_ID.DOMAIN_NAME"],
                "viewers": ["viewers1@DATA_PARTITION_ID.DOMAIN_NAME"]
            },
            "data": { "DatasetProperties": { "URI": "eml:///dataspace(/path1)" } },
            "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid1",
            "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0"
        },
        {
            "acl": {
                "owners": ["other@DATA_PARTITION_ID.DOMAIN_NAME"],
                "viewers": ["other@DATA_PARTITION_ID.DOMAIN_NAME"]
            },
            "id": "DATA_PARTITION_ID:dataset--ETPDataspace:other",
            "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0"
        }
    ]
}
)rr0";
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceGroupsBulk_queryParams) {
    auto ds = makeGroupsBulk();
    BOOST_REQUIRE_EQUAL(ds.queryParams(*cfg_, ""), GROUPS_BULK_QUERY);
    BOOST_REQUIRE_EQUAL(ds.queryParams(*cfg_, "c1="), std::string(GROUPS_BULK_QUERY) + "&cursor=c1%3d");
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceGroupsBulk_deserialize) {
    auto ds = makeGroupsBulk();
    const auto index_by_id = ds.indexById(*cfg_);
    std::vector<bool> found(2, false);
    std::string cursor;

    BOOST_REQUIRE(ds.deserialize(GROUPS_BULK_PAGE, index_by_id, found, cursor));
    BOOST_REQUIRE(found.at(0));
    BOOST_REQUIRE(!found.at(1));
    BOOST_REQUIRE_EQUAL(cursor, "c1=");
    BOOST_REQUIRE(ds.dataspaces_.at(0)->owners_group_ == std::set<std::string>{"owners1@DATA_PARTITION_ID.DOMAIN_NAME"});
    BOOST_REQUIRE(ds.dataspaces_.at(0)->viewers_group_ == std::set<std::string>{"viewers1@DATA_PARTITION_ID.DOMAIN_NAME"});
    BOOST_REQUIRE(ds.dataspaces_.at(1)->owners_group_.empty());

    // Last page: empty results end the listing even if a cursor is returned
    BOOST_REQUIRE(ds.deserialize(R"({"cursor": "c2", "results": []})", index_by_id, found, cursor));
    BOOST_REQUIRE(cursor.empty());

    BOOST_REQUIRE(!ds.deserialize(R"([])", index_by_id, found, cursor));
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClient_getDataspaceRecordsBulk) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res1;
    res1.code = 200;
    res1.body = GROUPS_BULK_PAGE;
    oes::core::HTTP::Response res2;
    res2.code = 200;
    res2.body = R"({"cursor": null, "results": []})";

    const std::string req_url = std::string("STORAGE_URL/records") + GROUPS_BULK_QUERY;
    EXPECT_CALL (*http_ptr_, get (req_url, _, header, _))
        .WillOnce (Return (res1));
    EXPECT_CALL (*http_ptr_, get (req_url + "&cursor=c1%3d", _, header, _))
        .WillOnce (Return (res2));

    auto ds = makeGroupsBulk();
    oes::osdu::api::EntitlementDataspaceClient entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    std::vector<bool> found;
    BOOST_REQUIRE(entitlement_ds_client.getDataspaceRecordsBulk(access_token_, ds, found, err));
    BOOST_REQUIRE(found == std::vector<bool>({true, false}));
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClient_getDataspaceRecordsBulk_forbidden) {
    oes::core::HTTP::Response res;
    res.code = 403;
    EXPECT_CALL (*http_ptr_, get (_, _, _, _))
        .WillOnce (Return (res));

    auto ds = makeGroupsBulk();
    oes::osdu::api::EntitlementDataspaceClient entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    std::vector<bool> found;
    BOOST_REQUIRE(entitlement_ds_client.getDataspaceRecordsBulk(access_token_, ds, found, err));
    BOOST_REQUIRE(found == std::vector<bool>({false, false}));
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClient_getDataspaceRecordsBulk_unsupported) {
    oes::core::HTTP::Response res;
    res.code = 405;
    EXPECT_CALL (*http_ptr_, get (_, _, _, _))
        .WillOnce (Return (res));

    auto ds = makeGroupsBulk();
    oes::osdu::api::EntitlementDataspaceClient entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    std::vector<bool> found;
    BOOST_REQUIRE(!entitlement_ds_client.getDataspaceRecordsBulk(access_token_, ds, found, err));
    BOOST_REQUIRE_EQUAL(err.code_, 405);
}

BOOST_AUTO_TEST_CASE(EntitlementGroups_canReadUpdateDataspaceBulk) {
    oes::core::HTTP::Response res1;
    res1.code = 200;
    res1.body = GROUPS_BULK_PAGE;
    oes::core::HTTP::Response res2;
    res2.code = 200;
    res2.body = R"({"cursor": null, "results": []})";
    EXPECT_CALL (*http_ptr_, get (_, _, _, _))
        .WillOnce (Return (res1))
        .WillOnce (Return (res2))
        .WillOnce (Return (res1))
        .WillOnce (Return (res2));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";

    std::vector<bool> can_read;
    std::vector<bool> can_update;
    {
        EntitlementGroups entitlement(config);
        entitlement.init("DATA_PARTITION_ID", authenticated_user, "user@mail", {"viewers1@DATA_PARTITION_ID.DOMAIN_NAME"});
        BOOST_REQUIRE(entitlement.canReadUpdateDataspaceBulk(
            std::make_shared<EntitlementDataspaceGroupsBulk>(makeGroupsBulk()), can_read, can_update));
        BOOST_REQUIRE(can_read == std::vector<bool>({true, false}));
        BOOST_REQUIRE(can_update == std::vector<bool>({false, false}));
    }
    {
        EntitlementGroups entitlement(config);
        entitlement.init("DATA_PARTITION_ID", authenticated_user, "user@mail", {"owners1@DATA_PARTITION_ID.DOMAIN_NAME"});
        BOOST_REQUIRE(entitlement.canReadUpdateDataspaceBulk(
            std::make_shared<EntitlementDataspaceGroupsBulk>(makeGroupsBulk()), can_read, can_update));
        BOOST_REQUIRE(can_read == std::vector<bool>({true, false}));
        BOOST_REQUIRE(can_update == std::vector<bool>({true, false}));
    }
}

// EntitlementDataspaceClientPolicy

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClientPolicy_getDataspaceUserPermission) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "x-user-id: uuid",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    std::string body =
R"rb(
{
  "input": {
    "operation": "Create",
    "records": [
      {
        "data": {
          "DatasetProperties": {
            "URI": "DS_URI"
          }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:ds_id",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
          "legaltags": [
            "US",
            "UK"
          ],
          "otherRelevantDataCountries": [
            "FR"
          ],
          "status": "compliant"
        }
      }
    ]
  }
}
)rb";
    body = nlohmann::json::parse(body).dump();

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = "true";

    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementDataspacePolicy ds("ds_id", "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"});

    auto req_url  = "POLICY_URL/evaluate?policy_id=policy_id&include_auth=true";
    EXPECT_CALL (*http_ptr_, post (req_url, _, header, body, _))
        .WillOnce (Return (res));

    EntitlementError err;
    auto status = entitlement_ds_client.getDataspaceUserPermission(
        "uuid",
        access_token_,
        ds,
        DSPermissionFlags::eCreate,
        err,
        "CLIENT_ID" // useless
    );

    BOOST_REQUIRE_EQUAL(status, true);
    BOOST_REQUIRE_EQUAL(err.code_, res.code);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClientPolicy_getDataspaceUserPermission_simple) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "x-user-id: uuid",
        "X-Etp-ClientId: CLIENT_ID",
        "Authorization: Bearer ACCESS_TOKEN",
    };

    std::string body =
R"rb(
{
  "dataUris": ["DS_URI"],
  "operation": "Create"
}
)rb";
    body = nlohmann::json::parse(body).dump();

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = "[true]";
    
    auto config = EntitlementOSDUConfig_Test::make();
    config->setAuthorizationMode("simple");

    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(config, base_client_);
    EntitlementDataspacePolicy ds("ds_id", "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"});

    auto req_url  = "POLICY_URL/evaluate";
    EXPECT_CALL (*http_ptr_, post (req_url, _, header, body, _))
        .WillOnce (Return (res));

    EntitlementError err;
    auto status = entitlement_ds_client.getDataspaceUserPermission(
        "uuid",
        access_token_,
        ds,
        DSPermissionFlags::eCreate,
        err,
        "CLIENT_ID"
    );

    BOOST_REQUIRE_EQUAL(status, true);
    BOOST_REQUIRE_EQUAL(err.code_, res.code);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClientPolicy_getAllDataspaceUserPermission_simple) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "x-user-id: uuid",
        "X-Etp-ClientId: CLIENT_ID",
        "Authorization: Bearer ACCESS_TOKEN",
    };

    std::string body_1 =
R"rb(
{
  "dataUris": ["DS_URI_1", "DS_URI_2"],
  "operation": "Create"
}
)rb";
    body_1 = nlohmann::json::parse(body_1).dump();
    
    std::string body_2 =
R"rb(
{
  "dataUris": ["DS_URI_3"],
  "operation": "Create"
}
)rb";
    body_2 = nlohmann::json::parse(body_2).dump();

    oes::core::HTTP::Response res_1;
    res_1.code = 200;
    res_1.body = "[true,false]";
    
    oes::core::HTTP::Response res_2;
    res_2.code = 200;
    res_2.body = "[true]";
    
    auto config = EntitlementOSDUConfig_Test::make(); // bulk size is 2
    config->setAuthorizationMode("simple");

    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(config, base_client_);
    EntitlementDataspacePolicyBulk ds({"DS_URI_1", "DS_URI_2", "DS_URI_3"}, std::vector<std::string>(), std::vector<std::string>());

    std::vector<bool> expected {true, false, true};
    auto req_url  = "POLICY_URL/evaluate";
    Sequence s;
    EXPECT_CALL (*http_ptr_, post (req_url, _, header, body_1, _))
        .InSequence(s)
        .WillOnce (Return (res_1));
    EXPECT_CALL (*http_ptr_, post (req_url, _, header, body_2, _))
        .InSequence(s)
        .WillOnce (Return (res_2));

    EntitlementError err;
    auto result = entitlement_ds_client.getAllDataspacesUserPermission(
        "uuid",
        access_token_,
        ds,
        DSPermissionFlags::eCreate,
        err,
        "CLIENT_ID"
    );

    BOOST_REQUIRE_EQUAL(3, result.size());
    BOOST_REQUIRE_EQUAL_COLLECTIONS(expected.begin(), expected.end(), result.begin(), result.end());
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClientPolicy_createDataspaceRecord) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Authorization: Bearer ACCESS_TOKEN" 
    };
    std::string body =
R"rb(
[
    {
    "data": {
        "DatasetProperties": {
        "URI": "DS_URI"
        }
    },
    "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
    "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
    "legal": {
        "legaltags": [
        "US",
        "UK"
        ],
        "otherRelevantDataCountries": [
        "FR"
        ],
        "status": "compliant"
    }
    }
]
)rb";
    body = nlohmann::json::parse(body).dump();

    oes::core::HTTP::Response res;
    res.code = 201;
    res.body = 
R"rr1(
{
    "recordCount": 1,
    "recordIds": [
        "DATA_PARTITION_ID:dataset--ETPDataspace:uuid"
    ],
    "skippedRecordIds": [],
    "recordIdVersions": [
        "DATA_PARTITION_ID:dataset--ETPDataspace:uuid:1656576985408681"
    ]
}
)rr1";
    const std::string req_url = "STORAGE_URL/records";

    EXPECT_CALL (*http_ptr_, put (req_url, _, header, body, _))
        .WillOnce (Return (res));

    EntitlementDataspacePolicy ds("uuid", "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"});
    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    bool ret = entitlement_ds_client.createDataspaceRecord(access_token_, ds, err);

    BOOST_REQUIRE(ret);
    BOOST_REQUIRE(err.code_ == res.code);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClientPolicy_delDataspaceRecord) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Authorization: Bearer ACCESS_TOKEN" 
    };

    oes::core::HTTP::Response res;
    res.code = 204;
    const std::string req_url = "STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid:delete";

    EXPECT_CALL (*http_ptr_, post (req_url, _, header, std::string(), _))
        .WillOnce (Return (res));
    
    EntitlementDataspacePolicy ds("uuid", "/path1/path2", "DS_URI", {"US","UK"}, {"FR"});
    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    bool ret = entitlement_ds_client.delDataspaceRecord(access_token_, ds, err);

    BOOST_REQUIRE(ret);
    BOOST_REQUIRE(err.code_ == res.code);
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClientPolicy_getDataspaceRecord) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN" 
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body =
R"rb(
{
  "input": {
    "operation": "Create",
    "records": [
      {
        "data": {
          "DatasetProperties": {
            "URI": "DS_URI"
          }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
          "legaltags": [
            "US",
            "UK"
          ],
          "otherRelevantDataCountries": [
            "FR"
          ],
          "status": "compliant"
        }
      }
    ]
  }
}
)rb";

    const std::string req_url = "STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid";

    EXPECT_CALL (*http_ptr_, get (req_url, _, header, _))
        .WillOnce (Return (res));

    EntitlementDataspacePolicy ds("uuid", "/path1/path2", "DS_URI", {"US","UK"}, {"FR"});
    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementError err;
    bool ret = entitlement_ds_client.getDataspaceRecord(access_token_, ds, err);

    BOOST_REQUIRE(ret);
    BOOST_REQUIRE(err.code_ == res.code);
    BOOST_REQUIRE(ds.usersPermissions_.find("uuid") != ds.usersPermissions_.end());
    BOOST_REQUIRE(ds.usersPermissions_.at("uuid").hasPermission(DSPermissionFlags::eCreate));
}

BOOST_AUTO_TEST_CASE(EntitlementDataspaceClientPolicy_simple) {
    EntitlementConfig config;
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    config.osdu_->setAuthorizationMode("simple");
    
    EntitlementDataspacePolicy ds("uuid", "/path1/path2", "DS_URI", {"US","UK"}, {"FR"});
    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(config.osdu_, base_client_);
    EntitlementError err;

    {
        bool ret = entitlement_ds_client.createDataspaceRecord(access_token_, ds, err);
        BOOST_REQUIRE(!ret);
    }
    {
        bool ret = entitlement_ds_client.getDataspaceRecord(access_token_, ds, err);
        BOOST_REQUIRE(!ret);
    }
    {
        bool ret = entitlement_ds_client.delDataspaceRecord(access_token_, ds, err);
        BOOST_REQUIRE(!ret);
    }
}

// Entitlement

BOOST_AUTO_TEST_CASE(Entitlement_isValid) {
    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    entitlement.init("DATA_PARTITION_ID", authenticated_user, std::string(), std::vector<std::string>());
    BOOST_REQUIRE(entitlement.isValid());

    config.HTTP_factory_ = [] {
        return nullptr;
    };
    EntitlementGroups entitlement2(config);
    BOOST_REQUIRE(false == entitlement2.isValid());
}

BOOST_AUTO_TEST_CASE(Entitlement_isUserInViewersGroup) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
    {
    "desId": "andrey.rodenko@emerson.com",
    "memberEmail": "andrey.rodenko@emerson.com",
    "groups": [
        {
            "name": "service.reservoir-dms.viewers",
            "description": "reservoir-dms",
            "email": "service.reservoir-dms.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";
    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    std::string memberEmail;
    std::vector<std::string> groups;
    entitlement.init("DATA_PARTITION_ID", authenticated_user, memberEmail, groups);

    BOOST_REQUIRE(entitlement.isUserInViewersGroup());

    // second call used cached data
    BOOST_REQUIRE(entitlement.isUserInViewersGroup());
}

BOOST_AUTO_TEST_CASE(Entitlement_not_isUserInViewersGroup) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 404;
    res.body = 
R"rr(
    {
    "desId": "andrey.rodenko@emerson.com",
    "memberEmail": "andrey.rodenko@emerson.com",
    "groups": [
    ]
}
)rr";
    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    entitlement.init("DATA_PARTITION_ID", authenticated_user, std::string(), std::vector<std::string>());
    BOOST_REQUIRE(!entitlement.isUserInViewersGroup());
    BOOST_REQUIRE_EQUAL(entitlement.err().code_, res.code);
}

BOOST_AUTO_TEST_CASE(Entitlement_isUserInOwnersGroup) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
    {
    "desId": "andrey.rodenko@emerson.com",
    "memberEmail": "andrey.rodenko@emerson.com",
    "groups": [
        {
            "name": "service.reservoir-dms.owners",
            "description": "reservoir-dms",
            "email": "service.reservoir-dms.owners@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";
    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    entitlement.init("DATA_PARTITION_ID", authenticated_user, std::string(), std::vector<std::string>());
    BOOST_REQUIRE(entitlement.isUserInOwnersGroup());

    // second call used cached data
    BOOST_REQUIRE(entitlement.isUserInOwnersGroup());
}

BOOST_AUTO_TEST_CASE(Entitlement_not_isUserInOwnersGroup) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 404;
    res.body = 
R"rr(
    {
    "desId": "andrey.rodenko@emerson.com",
    "memberEmail": "andrey.rodenko@emerson.com",
    "groups": [
    ]
}
)rr";
    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header, _))
        .WillOnce (Return (res));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    entitlement.init("DATA_PARTITION_ID", authenticated_user, std::string(), std::vector<std::string>());
    BOOST_REQUIRE(!entitlement.isUserInOwnersGroup());
    BOOST_REQUIRE_EQUAL(entitlement.err().code_, res.code);
}

BOOST_AUTO_TEST_CASE(Entitlement_canViewEditDeleteDataspace) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
  {
    "acl": {
      "owners": [
        "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"
      ],
      "viewers": [
        "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
      ]
    },
    "data": {
      "DatasetProperties": {
        "URI": "eml:///dataspace(/path1/path2)"
      }
    },
    "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
    "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
    "legal": {
      "legaltags": [
        "LEGAL_TAGS"
      ],
      "otherRelevantDataCountries": [
        "LEGAL_COUNTRIES"
      ],
      "status": "compliant"
    }
  }
)rr";
    EXPECT_CALL (*http_ptr_.get(), get ("STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid", _, header, _))
        .WillRepeatedly (Return (res));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    std::string memberEmail;
    auto viewers_group = "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME";
    auto owners_group = "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME";
    std::vector<std::string> groups;
    groups.push_back(viewers_group);
    groups.push_back(owners_group);

    entitlement.init("DATA_PARTITION_ID", authenticated_user, memberEmail, groups);
    std::vector<std::string> empty;
    std::vector<std::string> owners = {"data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"};
    std::vector<std::string> viewers = {"data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"};
    auto ds = std::make_shared<EntitlementDataspaceGroups>(
        "uuid", "/path1/path2",empty,empty,viewers,owners
    );
    BOOST_REQUIRE(entitlement.canReadDataspace(ds));
    BOOST_REQUIRE(entitlement.canUpdateDataspace(ds));
    BOOST_REQUIRE(entitlement.canDeleteDataspace(ds));
    BOOST_REQUIRE_EQUAL(entitlement.err().code_, res.code);
}

BOOST_AUTO_TEST_CASE(Entitlement_can_only_View_not_Edit_not_DeleteDataspace) {
    oes::core::HTTP::Headers header{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
  {
    "acl": {
      "owners": [
      ],
      "viewers": [
        "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
      ]
    },
    "data": {
      "DatasetProperties": {
        "URI": "eml:///dataspace(/path1/path2)"
      }
    },
    "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
    "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
    "legal": {
      "legaltags": [
        "LEGAL_TAGS"
      ],
      "otherRelevantDataCountries": [
        "LEGAL_COUNTRIES"
      ],
      "status": "compliant"
    }
  }
)rr";
    EXPECT_CALL (*http_ptr_.get(), get ("STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid", _, header, _))
        .WillRepeatedly (Return (res));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    std::string memberEmail;
    auto group_name = "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME";
    std::vector<std::string> groups;
    groups.push_back(group_name);
    entitlement.init("DATA_PARTITION_ID", authenticated_user, memberEmail, groups);

    std::vector<std::string> empty;
    std::vector<std::string> viewers = {"data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"};
    auto ds = std::make_shared<EntitlementDataspaceGroups>("uuid", "/path1/path2",empty,empty,viewers,empty);
    BOOST_REQUIRE(entitlement.canReadDataspace(ds));
    BOOST_REQUIRE(!entitlement.canUpdateDataspace(ds));
    BOOST_REQUIRE(!entitlement.canDeleteDataspace(ds));
    BOOST_REQUIRE_EQUAL(entitlement.err().code_, res.code);
}

struct ReqResParams {
    ReqResParams(const std::string& body, short res_code, const std::string& res_body) {
        // cppcheck-suppress useInitializationList
        req_body_ = !body.empty() ? nlohmann::json::parse(body).dump() : std::string();
        res_.code = res_code;
        res_.body = !res_body.empty() ? nlohmann::json::parse(res_body).dump() : std::string();
    }
    std::string req_body_;
    oes::core::HTTP::Response res_;
};

BOOST_AUTO_TEST_CASE(Entitlement_registerDataspace) {
    oes::core::HTTP::Headers header_get {
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };
    
    oes::core::HTTP::Headers header {
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    // expectation of one get and one put dataset for storage
    oes::core::HTTP::Response res;
    res.code = 200;
    res.body = 
R"rr(
    {
    "desId": "andrey.rodenko@emerson.com",
    "memberEmail": "andrey.rodenko@emerson.com",
    "groups": [
        {
            "name": "data.rdms-uuid.viewers",
            "description": "Tests POST",
            "email": "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
        },
        {
            "name": "data.rdms-uuid.owners",
            "description": "Tests POST",
            "email": "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    ReqResParams p3(
R"rr4(
[
    {
        "acl": {
            "owners": [
                "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"
            ],
            "viewers": [
                "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
            ]
        },
        "data": {
            "DatasetProperties": {
                "URI": "eml:///dataspace(/path1/path2)" 
            }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
            "legaltags": [
                "LEGAL_TAGS"
            ],
            "otherRelevantDataCountries": [
                "LEGAL_COUNTRIES"
            ],
            "status": "compliant"
        }
    }
]
)rr4",
    201,
    ""
);

    Sequence s;

    const std::string req_url1("ENTITLEMENTS_URL/groups");
    EXPECT_CALL (*http_ptr_.get(), get (req_url1, _, header_get, _))
        .InSequence(s)
        .WillOnce (Return (res));
    const std::string req_url2("STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid");
    EXPECT_CALL (*http_ptr_.get(), get (req_url2, _, header_get, _))
        .InSequence(s)
        .WillOnce (Return (p3.res_));
    const std::string req_url3("STORAGE_URL/records");
    EXPECT_CALL (*http_ptr_, put (req_url3, _, header, p3.req_body_, _))
        .InSequence(s)
        .WillOnce (Return (p3.res_));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    entitlement.init("DATA_PARTITION_ID", authenticated_user, std::string(), std::vector<std::string>());
    {
        auto ds = std::make_shared<EntitlementDataspaceGroups>("uuid", "/path1/path2",
            std::vector<std::string> ({"LEGAL_TAGS"}),
            std::vector<std::string> ({"LEGAL_COUNTRIES"}),
            std::vector<std::string> ({"data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"}),
            std::vector<std::string> ({"data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"}));
        BOOST_REQUIRE(entitlement.registerDataspace(ds));
        BOOST_REQUIRE_EQUAL(entitlement.err().code_, p3.res_.code);
    }
    {
        auto ds = std::make_shared<EntitlementDataspaceGroups>("uuid", "/path1/path2",
            std::vector<std::string> ({"LEGAL_TAGS"}),
            std::vector<std::string> ({"LEGAL_COUNTRIES"}),
            std::vector<std::string> ({"unregistered.viewers@DATA_PARTITION_ID.DOMAIN_NAME"}),
            std::vector<std::string> ({"unregistered.owners@DATA_PARTITION_ID.DOMAIN_NAME"}));
        BOOST_REQUIRE(!entitlement.registerDataspace(ds));
    }
}

BOOST_AUTO_TEST_CASE(Entitlement_unregisterDataspace) {
    oes::core::HTTP::Headers header_get {
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Headers header {
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    // expectation of one get and one delete dataset for storage
    oes::core::HTTP::Response res_get;
    res_get.code = 200;
    res_get.body = 
R"rr(
    {
    "desId": "andrey.rodenko@emerson.com",
    "memberEmail": "andrey.rodenko@emerson.com",
    "groups": [
        {
            "name": "data.rdms-uuid.viewers",
            "description": "Tests POST",
            "email": "data.rdms-uuid.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
        },
        {
            "name": "data.rdms-uuid.owners",
            "description": "Tests POST",
            "email": "data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    Sequence s;
    const std::string get_req_url("ENTITLEMENTS_URL/groups");
    EXPECT_CALL (*http_ptr_.get(), get (get_req_url, _, header_get, _))
        .InSequence(s)
        .WillOnce (Return (res_get));
    oes::core::HTTP::Response res;
    res.code = 204;
    const std::string req_url = "STORAGE_URL/records/DATA_PARTITION_ID:dataset--ETPDataspace:uuid:delete";
    EXPECT_CALL (*http_ptr_, post (req_url, _, header, std::string(), _))
        .InSequence(s)
        .WillOnce (Return (res));

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = "ACCESS_TOKEN";
    entitlement.init("DATA_PARTITION_ID", authenticated_user, std::string(), std::vector<std::string>());

    {
        auto ds = std::make_shared<EntitlementDataspaceGroups>("uuid", "/path1/path2",
            std::vector<std::string>(), // legal_tags
            std::vector<std::string>(), // legal_countries
            std::vector<std::string> (), // viewers
            std::vector<std::string> ({"data.rdms-uuid.owners@DATA_PARTITION_ID.DOMAIN_NAME"})); // owners
        BOOST_REQUIRE(entitlement.unregisterDataspace(ds));
        BOOST_REQUIRE_EQUAL(entitlement.err().code_, res.code);
    }
    {
        auto ds = std::make_shared<EntitlementDataspaceGroups>("uuid", "/path1/path2",
            std::vector<std::string>(), // legal_tags
            std::vector<std::string>(), // legal_countries
            std::vector<std::string> (), // viewers
            std::vector<std::string> ({"unregistered.owners@DATA_PARTITION_ID.DOMAIN_NAME"})); // owners
        BOOST_REQUIRE(!entitlement.unregisterDataspace(ds));
    }
}

// EntitlementPolicy

BOOST_AUTO_TEST_CASE(EntitlementPolicy_entitlement_factory) {
    EntitlementConfig config;
    config.HTTP_factory_ = [&] { return http_ptr_; };

    {
        auto ent = entitlement_factory(EntitlementType::Groups, config);
        BOOST_REQUIRE(ent->isGroupBased());
        BOOST_REQUIRE(!ent->isPolicyBased());
    }
    {
        auto ent = entitlement_factory(EntitlementType::Policy, config);
        BOOST_REQUIRE(!ent->isGroupBased());
        BOOST_REQUIRE(ent->isPolicyBased());
    }
}

BOOST_AUTO_TEST_CASE(EntitlementPolicy_basics) {
    EntitlementConfig config;
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    config.HTTP_factory_ = [&] { return http_ptr_; };

    EntitlementPolicy ent(config);
    AuthenticatedUser_ptr user = std::make_shared<AuthenticatedUser>();
    user->uuid_ = "uuid";
    user->header_ = "ACCESS_TOKEN";

    ent.init("DATA_PARTITION_ID", user, user->uuid_, "CLIENT_ID", DSPermission::makeNonePermission(false));

    BOOST_REQUIRE(ent.isValid());
    BOOST_REQUIRE_EQUAL(ent.getUserId(), user->uuid_);
}

BOOST_AUTO_TEST_CASE(EntitlementPolicy_canCreateDataspace_osdu) {
    EntitlementConfig config;
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    config.osdu_->setConnectivityMode("osdu");
    config.HTTP_factory_ = [&] { return http_ptr_; };

    EntitlementPolicy ent(config);
    AuthenticatedUser_ptr user = std::make_shared<AuthenticatedUser>();
    user->uuid_ = "uuid";
    user->header_ = "ACCESS_TOKEN";

    ent.init("DATA_PARTITION_ID", user, user->uuid_, "CLIENT_ID", DSPermission::makeNonePermission(false));

    BOOST_REQUIRE(ent.isValid());
    BOOST_REQUIRE_EQUAL(ent.getUserId(), user->uuid_);


    oes::core::HTTP::Headers header_entitlement{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res_entitlement;
    res_entitlement.code = 200;
    res_entitlement.body = 
R"rr(
    {
    "desId": "name.surname@aspentech.com",
    "memberEmail": "name.surname@aspentech.com",
    "groups": [
        {
            "name": "some.name",
            "description": "some description",
            "email": "service.reservoir-dms.owners@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    oes::core::HTTP::Headers header_ds{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "x-user-id: uuid",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    std::string body_ds =
R"rb(
{
  "input": {
    "operation": "Create",
    "records": [
      {
        "data": {
          "DatasetProperties": {
            "URI": "DS_URI"
          }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
          "legaltags": [
            "US",
            "UK"
          ],
          "otherRelevantDataCountries": [
            "FR"
          ],
          "status": "compliant"
        }
      }
    ]
  }
}
)rb";
    body_ds = nlohmann::json::parse(body_ds).dump();

    oes::core::HTTP::Response res_ds;
    res_ds.code = 200;
    res_ds.body = "true";

    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementDataspacePolicy_ptr ds (new EntitlementDataspacePolicy(user->uuid_, "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"}));

    Sequence s;
    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header_entitlement, _))
        .InSequence(s)
        .WillOnce (Return (res_entitlement));

    auto req_url_ds  = "POLICY_URL/evaluate?policy_id=policy_id&include_auth=true";
    EXPECT_CALL (*http_ptr_, post (req_url_ds, _, header_ds, body_ds, _))
        .InSequence(s)
        .WillOnce (Return (res_ds));


    auto res = ent.canCreateDataspace(ds);
    BOOST_REQUIRE(res);
}

BOOST_AUTO_TEST_CASE(EntitlementPolicy_canCreateDataspace_viewers_osdu) {
    EntitlementConfig config;
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    config.osdu_->setConnectivityMode("osdu");
    config.HTTP_factory_ = [&] { return http_ptr_; };

    EntitlementPolicy ent(config);
    AuthenticatedUser_ptr user = std::make_shared<AuthenticatedUser>();
    user->uuid_ = "uuid";
    user->header_ = "ACCESS_TOKEN";

    ent.init("DATA_PARTITION_ID", user, user->uuid_, "CLIENT_ID", DSPermission::makeNonePermission(false));

    BOOST_REQUIRE(ent.isValid());
    BOOST_REQUIRE_EQUAL(ent.getUserId(), user->uuid_);


    oes::core::HTTP::Headers header_entitlement{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res_entitlement;
    res_entitlement.code = 200;
    res_entitlement.body = 
R"rr(
    {
    "desId": "name.surname@aspentech.com",
    "memberEmail": "name.surname@aspentech.com",
    "groups": [
        {
            "name": "some.name",
            "description": "some description",
            "email": "service.reservoir-dms.viewers@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    oes::core::HTTP::Headers header_ds{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "x-user-id: uuid",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    std::string body_ds =
R"rb(
{
  "input": {
    "operation": "Create",
    "records": [
      {
        "data": {
          "DatasetProperties": {
            "URI": "DS_URI"
          }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
          "legaltags": [
            "US",
            "UK"
          ],
          "otherRelevantDataCountries": [
            "FR"
          ],
          "status": "compliant"
        }
      }
    ]
  }
}
)rb";
    body_ds = nlohmann::json::parse(body_ds).dump();

    oes::core::HTTP::Response res_ds;
    res_ds.code = 200;
    res_ds.body = "true";

    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementDataspacePolicy_ptr ds (new EntitlementDataspacePolicy(user->uuid_, "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"}));

    
    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header_entitlement, _))
        .WillOnce (Return (res_entitlement));

    auto res = ent.canCreateDataspace(ds);
    BOOST_REQUIRE(!res);
}

BOOST_AUTO_TEST_CASE(EntitlementPolicy_canCreateDataspace_DS_return_false_osdu) {
    EntitlementConfig config;
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    config.osdu_->setConnectivityMode("osdu");
    config.HTTP_factory_ = [&] { return http_ptr_; };

    EntitlementPolicy ent(config);
    AuthenticatedUser_ptr user = std::make_shared<AuthenticatedUser>();
    user->uuid_ = "uuid";
    user->header_ = "ACCESS_TOKEN";

    ent.init("DATA_PARTITION_ID", user, user->uuid_, "CLIENT_ID", DSPermission::makeNonePermission(false));

    BOOST_REQUIRE(ent.isValid());
    BOOST_REQUIRE_EQUAL(ent.getUserId(), user->uuid_);


    oes::core::HTTP::Headers header_entitlement{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "Accept: application/json",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    oes::core::HTTP::Response res_entitlement;
    res_entitlement.code = 200;
    res_entitlement.body = 
R"rr(
    {
    "desId": "name.surname@aspentech.com",
    "memberEmail": "name.surname@aspentech.com",
    "groups": [
        {
            "name": "some.name",
            "description": "some description",
            "email": "service.reservoir-dms.owners@DATA_PARTITION_ID.DOMAIN_NAME"
        }
    ]
}
)rr";

    oes::core::HTTP::Headers header_ds{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "x-user-id: uuid",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    std::string body_ds =
R"rb(
{
  "input": {
    "operation": "Create",
    "records": [
      {
        "data": {
          "DatasetProperties": {
            "URI": "DS_URI"
          }
        },
        "id": "DATA_PARTITION_ID:dataset--ETPDataspace:uuid",
        "kind": "SCHEMA_ID:wks:dataset--ETPDataspace:1.0.0",
        "legal": {
          "legaltags": [
            "US",
            "UK"
          ],
          "otherRelevantDataCountries": [
            "FR"
          ],
          "status": "compliant"
        }
      }
    ]
  }
}
)rb";
    body_ds = nlohmann::json::parse(body_ds).dump();

    oes::core::HTTP::Response res_ds;
    res_ds.code = 200;
    res_ds.body = "false";

    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementDataspacePolicy_ptr ds (new EntitlementDataspacePolicy(user->uuid_, "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"}));

    Sequence s;
    EXPECT_CALL (*http_ptr_.get(), get ("ENTITLEMENTS_URL/groups", _, header_entitlement, _))
        .InSequence(s)
        .WillOnce (Return (res_entitlement));

    auto req_url_ds  = "POLICY_URL/evaluate?policy_id=policy_id&include_auth=true";
    EXPECT_CALL (*http_ptr_, post (req_url_ds, _, header_ds, body_ds, _))
        .InSequence(s)
        .WillOnce (Return (res_ds));


    auto res = ent.canCreateDataspace(ds);
    BOOST_REQUIRE(!res);
}

BOOST_AUTO_TEST_CASE(EntitlementPolicy_canCreateDataspace_simple) {
    EntitlementConfig config;
    config.osdu_ = EntitlementOSDUConfig_Test::make();
    config.osdu_->setAuthorizationMode("simple");
    config.HTTP_factory_ = [&] { return http_ptr_; };

    EntitlementPolicy ent(config);
    AuthenticatedUser_ptr user = std::make_shared<AuthenticatedUser>();
    user->uuid_ = "uuid";
    user->header_ = "ACCESS_TOKEN";

    ent.init("DATA_PARTITION_ID", user, user->uuid_, "CLIENT_ID", DSPermission::makeNonePermission(false));

    BOOST_REQUIRE(ent.isValid());
    BOOST_REQUIRE_EQUAL(ent.getUserId(), user->uuid_);

    oes::core::HTTP::Headers header_ds{
        "content-type: application/json",
        "data-partition-id: DATA_PARTITION_ID",
        "x-user-id: uuid",
        "X-Etp-ClientId: CLIENT_ID",
        "Authorization: Bearer ACCESS_TOKEN"
    };

    std::string body_ds =
R"rb(
{
  "dataUris": ["DS_URI"],
  "operation": "Create"
}
)rb";
    body_ds = nlohmann::json::parse(body_ds).dump();

    oes::core::HTTP::Response res_ds;
    res_ds.code = 200;
    res_ds.body = "[true]";

    oes::osdu::api::EntitlementDataspaceClientPolicy entitlement_ds_client(cfg_, base_client_);
    EntitlementDataspacePolicy_ptr ds (new EntitlementDataspacePolicy(user->uuid_, "/folder-name/project-name", "DS_URI", {"US","UK"}, {"FR"}));

    auto req_url_ds  = cfg_->policyDelegateUrl() + cfg_->evaluatePolicyPath();
    EXPECT_CALL (*http_ptr_, post (req_url_ds, _, header_ds, body_ds, _))
        .WillOnce (Return (res_ds));

    auto res = ent.canCreateDataspace(ds);
    BOOST_REQUIRE(res);
}

BOOST_AUTO_TEST_SUITE_END()

struct EntitlementFixtureReal {
    EntitlementFixtureReal() : 
        cfg_(EntitlementOSDUConfig_Real::make()),
        access_token_(getenv("DBG_ACCESS_TOKEN")),
        http_ptr_(std::make_shared<oes::core::HTTP>()) {
    }
    EntitlementConfig_ptr cfg_;
    std::string access_token_;
    oes::common::auth::HTTP_ptr http_ptr_;
};

boost::test_tools::assertion_result has_token(boost::unit_test::test_unit_id) {
    //return false;
    return getenv("DBG_ACCESS_TOKEN");
}

BOOST_FIXTURE_TEST_SUITE(EntitlementReal, EntitlementFixtureReal, 
    *boost::unit_test::precondition(has_token))

BOOST_AUTO_TEST_CASE(register_check_unregister_dataspace) {
    auto real_cfg = std::dynamic_pointer_cast<EntitlementOSDUConfig_Real>(cfg_);
    if (!real_cfg || !real_cfg->has_config_) {
        BOOST_TEST_MESSAGE("No OSDU config");
        return;
    }
    auto ds_guid = oes::core::Guid::make().toStringNoDash();
    auto ds = std::make_shared<EntitlementDataspaceGroups>(ds_guid, "/path1/path2",std::vector<std::string>(),std::vector<std::string>());

    EntitlementConfig config;
    config.HTTP_factory_ = [&] {
        return http_ptr_;
    };
    config.osdu_ = EntitlementOSDUConfig_Real::make();
    EntitlementGroups entitlement(config);
    AuthenticatedUser_ptr authenticated_user = std::make_shared<AuthenticatedUser>();
    authenticated_user->header_ = access_token_;
    entitlement.init("DATA_PARTITION_ID", authenticated_user, std::string(), std::vector<std::string>());
    BOOST_REQUIRE(entitlement.isValid());

    // register
    bool ret = entitlement.registerDataspace(ds);
    BOOST_REQUIRE_MESSAGE(ret, "ERR: " << entitlement.err().to_string());
    BOOST_REQUIRE(entitlement.err().not_err());

    // check
    ret = entitlement.canReadDataspace(ds);
    BOOST_REQUIRE_MESSAGE(ret, "ERR: " << entitlement.err().to_string());
    BOOST_REQUIRE(entitlement.err().not_err());
    ret = entitlement.canUpdateDataspace(ds);
    BOOST_REQUIRE_MESSAGE(ret, "ERR: " << entitlement.err().to_string());
    BOOST_REQUIRE(entitlement.err().not_err());
    ret = entitlement.canDeleteDataspace(ds);
    BOOST_REQUIRE_MESSAGE(ret, "ERR: " << entitlement.err().to_string());
    BOOST_REQUIRE(entitlement.err().not_err());

    // unregister
    ret = entitlement.unregisterDataspace(ds);
    BOOST_REQUIRE_MESSAGE(ret, "ERR: " << entitlement.err().to_string());
    BOOST_REQUIRE(entitlement.err().not_err());
}


BOOST_AUTO_TEST_SUITE_END()

} // End anonymous namespace
