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

#include "TypesProto.h"

#include "Repo.h"
#include "Session.h"

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Statement.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "../etp12/Server.hpp"
#include "../etp12/MultiPartReply.hpp"

using namespace oes::eml::etp12::Datatypes;

using oes::eml::etp12::errorStatus;

namespace oes::eml::openkv {

//============================================================================

OpenKVSupportedTypes::OpenKVSupportedTypes(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

// See DataServer.cpp DxInterfaceTypeHandler::accept_get_supported_types
void OpenKVSupportedTypes::GetSupportedTypes(
    IRequest& ireq, GetSupportedTypes_req_t req,
    EtpResponseHandler<GetSupportedTypes_res_t> process_cb
) try {
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);
    const auto api(txApi(req.info.api_));

    etp12::MultiPartReply reply(*session, process_cb);
    EmlUri uri = EmlUri::parse(req.m_uri);

    if (!uri.isValid()) {
        return reply.fatalError(EINVALID_URI, "Invalid URI: " + req.m_uri);
    }
    if (uri.type_ != EmlUri::eDATA_SPACE) {
        return reply.fatalError(EINVALID_URI, "Expected Dataspace URI: " + req.m_uri);
    }

    Space space = repo_->findSpaceInfo(uri.dataspace_);
    if(!space.isValid()){
        return reply.fatalError(ENOT_FOUND, fmt::format("Dataspace not found: {}", uri.dataspace_));
    }

    ErrorInfo err;
    Connection_ptr spacedb = repo_->getSpaceConnection(space, err);
    if (!spacedb) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        return reply.fatalError();
    }

    if (!checkDataspaceAccess(space, AccessType::CAN_READ, err)) {
        reply.rc_.ok = false;
        reply.rc_.ex.m_error = std::move(err);
        return reply.fatalError();
    }

    OES_DEBUG_ASSERT(req.m_scope == Object::ContextScopeKind::eSelf); // unused

    if (req.m_countObjects) {
        std::map<std::string, int32_t> count_by_type;
        {
            const char* sql = R"sql(

select ml||'.'||xml, cnt
  from (
        select typ_id, count(*) cnt
          from res
         group by typ_id
       ) r
  join typ on typ.id = r.typ_id
  join uri on uri.id = typ.uri_id

)sql";
            oes::postgresql::Statement stmt = spacedb->prepare(std::move(sql));
            while (stmt.step()) {
                auto rc2 = count_by_type.emplace(stmt.getString(0), stmt.getInt(1));
                OES_ASSERT(rc2.second, ;);
            }
        }

        if (req.m_returnEmptyTypes) {
            // TODO: Add empty types
        }
        for (const auto& entry : count_by_type) {
            Object::SupportedType& supp_type = reply.addVecPart();
            supp_type.m_dataObjectType = entry.first;
            supp_type.m_objectCount = entry.second;
        }
    } else {
        std::set<std::string> types;
        {
            const char* sql = R"sql(

select ml||'.'||xml
  from typ
  join uri on uri.id = typ.uri_id
 where exists
       (
        select 1 from res where typ_id = typ.id limit 1
       )

)sql";
            oes::postgresql::Statement stmt = spacedb->prepare(std::move(sql));
            while (stmt.step()) {
                auto rc2 = types.emplace(stmt.getString(0));
                OES_ASSERT(rc2.second, ;);
            }
        }
        if (req.m_returnEmptyTypes) {
            // TODO: Add empty types
        }
        for (const std::string& type : types) {
            Object::SupportedType& supp_type = reply.addVecPart();
            supp_type.m_dataObjectType = type;
            supp_type.m_objectCount.reset();
        }
    }

    reply.finalProcess();
} catch (...) {
    etp12::handleException(process_cb);
}

//============================================================================

} // namespace oes::eml::openkv
