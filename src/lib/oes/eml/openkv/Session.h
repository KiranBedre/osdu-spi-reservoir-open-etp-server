// ============================================================================
// Copyright 2020-2023 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_EML_OPENKV_SESSION_H
#define OES_EML_OPENKV_SESSION_H

#include "../fwd.h"
#include "../export.h"

#include "AdminDB.h"
#include "ServerConfig.h"
#include "SpaceDML.h"

#include "../etp12/Server.h"

#include <oes/common/etp12/EmlUri.h>
#include <oes/common/epc/ActivityUtils.h>

typedef long long int eml_int64;

#include <chrono>

namespace oes {
namespace eml {
namespace openkv {

using oes::core::Guid;
using oes::common::etp12::Protocol::Store::PutDataObjects;

//============================================================================

using oes::common::etp12::Datatypes::Object::DataObject;
using oes::common::etp12::Datatypes::Object::Resource;
using ::oes::common::etp12::ApiType;
constexpr int nApiType = static_cast<int>(ApiType::eMisc) + 1;

// TransactionActControl: is a class to hold some object data to be processed later
class OES_EML_EXPORT TransactionActControl {
public:
    TransactionActControl() = default;
    ~TransactionActControl() = default;

    /*
    New objects
    List of uuids
    */
    std::unordered_map<std::string, oes::common::epc::AnalyzedObject> analyzed_objects_;
    std::set<Guid> new_objs_uuids_;

    /*
    Activity info container
    activity_obj_parameters = {
        "activity_1_uuid": { ---------------------------------------> activity uuid (key)
            "activity_1_template_uuid", -------------------------> activity template uuid (first)
            "actv_1_uuid_title_type" { --------------------------> List if object uuid and its activity template title
                {"obj_1_uuid", "obj_1_activity_template_title"},
                {"obj_2_uuid", "obj_2_activity_template_title"},
                ...
            }
        },
        "activity_2_uuid" : {
            ...
        }
    }
    */
    using Uuid_title_t = std::unordered_map<std::string, std::string>;
    using Descriptor_params_t = std::pair<std::string, Uuid_title_t>;
    std::unordered_map<std::string, Descriptor_params_t> activity_obj_parameters_;


    /* 
    Activity Templates
    activities_template_ = {
        "activity_1_template_uuid": { ----------> activity template uuid (key)
            activity_1_template_template_1, ----> activity template title (ONLY IF IT IS OUTPUT)
            activity_1_template_template_2,
            ...
        },
        "activity_2_template__uuid": {...}
    }
    */
    std::unordered_map<std::string, std::set<std::string>> activities_template_; // key is uuid and value is list of titles (OUTPUT_ONLY)
    std::unordered_map<std::string, std::set<std::string>> db_activities_template_; // key is uuid and value is list of titles (OUTPUT_ONLY)

    bool upsertActivityObjs(SpaceWriter& writer, const std::string& user_id, const std::string& space_uri);

    static const std::string import_activity_template_uuid_;
    static const std::string import_activity_template_title_;
    static const std::string import_activity_template_parameter_title_;

    bool has_import_template_already_ = false;
private:
    using ObjMapVal_t = std::map<std::string, oes::common::etp12::Datatypes::Object::DataObject>::value_type;
    std::vector<ObjMapVal_t> get_activities_to_put(SpaceWriter& writer, const std::string& cid);

    static void fill_resource_(
        Resource& res,
        const std::string& name,
        const std::string& uri,
        const int64_t time
    );
};

struct OES_EML_EXPORT OpenKVTransaction {
    OpenKVTransaction(const Connection_ptr& dbConnection, bool ro, std::string msg, std::vector<EmlUri> dataspaceUris, bool local);

    ~OpenKVTransaction();

    static bool dataspaceWriteable(const std::string& dataspaceUri){
        return dataspace_being_written_.find(dataspaceUri) == dataspace_being_written_.end();
    }

    inline const bool local() const {
        return local_ ;
    }

    Guid uid_;
    bool ro_;
    bool in_api_;
    bool local_;
    std::string msg_;
    std::vector<EmlUri> dataspaceUris_;
    unsigned api_calls_[nApiType] = { 0 };
    std::chrono::high_resolution_clock::time_point begin_;
    std::chrono::high_resolution_clock::time_point last_api_[nApiType];

    // TODO: Multi-space TX
    // TODO: Non-space TX, e.g. on AdminDB for PutDataspaces
    postgresql::Transaction db_transaction_;
    SpaceReader_ptr space_reader_;
    SpaceWriter_ptr space_writer_;

    struct OrphanAry {
        eml_int64 ary_id;
        eml_int64 obj_id = -1;
        eml_int64 xpa_id = -1;
    };

    struct MissingAry {
        eml_int64 ary_id;
        eml_int64 obj_id = -1;
        eml_int64 xpa_id = -1;
    };

    // See https://stackoverflow.com/a/35525806/103724 for why we use an
    // explict std::less<> below (hint: to allow .find(std::string_view))
    using OrphanAryMap = std::map<
        std::string, OpenKVTransaction::OrphanAry, std::less<>
    >;
    using MissingAryMap = std::map<
        std::string, OpenKVTransaction::MissingAry, std::less<>
    >;

    // FIXME: Should these maps be per-space? Or per-EpcExternalReferencePart?
    OrphanAryMap orphan_arys_;
    MissingAryMap missing_arys_;

    using TransactionCtlMap_t = std::map<
        std::string, TransactionActControl
    >;
    std::shared_ptr<TransactionCtlMap_t> trans_ctl_;
#ifndef _BullseyeCoverage
    std::chrono::high_resolution_clock::time_point lastApi() const;
    std::chrono::high_resolution_clock::duration idleFor();
#endif
    private:
        static std::set<std::string> dataspace_being_written_;
};

struct [[nodiscard]] OES_EML_EXPORT OpenKVScopedTxApi {
    std::shared_ptr<OpenKVTransaction> tx_;
    ApiType api_type_;

    OpenKVScopedTxApi(std::shared_ptr<OpenKVTransaction>& tx, ApiType api_type);
    explicit OpenKVScopedTxApi(ApiType api_type);
    ~OpenKVScopedTxApi();

    void heartbeat();
};

//============================================================================

struct OES_EML_EXPORT OpenKVSession : etp12::Session
{
    // Cache of dataspaces
    std::map<std::string, Space> etp_space_by_path_; // by-path
    std::map<std::string, std::string> etp_space_by_uid_; // TODO: Use BMI
    std::map<int64_t, PutDataObjects> pending_put_data_objects_; // Pending PutDataObjects with Chunks

    std::shared_ptr<OpenKVTransaction> tx_;

    OpenKVSession(
        etp12::Handshake handshake = {},
        oes::common::auth::AuthenticatedUser_ptr user = nullptr,
        oes::common::auth::Entitlement_ptr entitlement = oes::common::auth::Entitlement_ptr()
    );

    OpenKVSession(OpenKVSession&&) = default;
    OpenKVSession& operator=(OpenKVSession&&) = default;

    OpenKVSession(const OpenKVSession&) = delete;
    OpenKVSession& operator=(const OpenKVSession&) = delete;

    virtual const bool hasTransaction() const {
        return tx_ && !tx_->local();
    }

protected:
    virtual void resumeFromExistingSession(std::shared_ptr<Session> other) override;
};

//============================================================================

} // namespace openkv
} // namespace eml
} // namespace oes

#endif//OES_EML_OPENKV_SESSION_H
