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

#ifndef OES_EML_OPENKV_SPACEPROTO_H
#define OES_EML_OPENKV_SPACEPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>

namespace oes::eml::openkv {

using oes::common::etp12::Protocol::IRequest;
using oes::common::etp12::Protocol::EtpResponseHandler;

//============================================================================

struct OES_EML_EXPORT OpenKVDataspace : BaseProto, oes::common::etp12::Protocol::IDataspace
{
    OpenKVDataspace(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    void GetDataspaces( // messageTypeId 1
        IRequest& ireq, GetDataspaces_req_t req,
        EtpResponseHandler<GetDataspaces_res_t> process_cb // messageTypeId 2
    ) final;

    void PutDataspaces( // messageTypeId 3
        IRequest& ireq, PutDataspaces_req_t req,
        EtpResponseHandler<PutDataspaces_res_t> process_cb // messageTypeId 6
    ) final;

    void DeleteDataspaces( // messageTypeId 4
        IRequest& ireq, DeleteDataspaces_req_t req,
        EtpResponseHandler<DeleteDataspaces_res_t> process_cb // messageTypeId 5
    ) final;

    static oes::common::etp12::Protocol::EtpResponseStatus doPutDataspaces(
        oes::eml::openkv::BaseProto& base,
        std::map<std::string, oes::eml::etp12::Datatypes::Object::Dataspace>& dataspaceMap,
        std::map<std::string, std::string>& successMap
    );
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_SPACEPROTO_H
