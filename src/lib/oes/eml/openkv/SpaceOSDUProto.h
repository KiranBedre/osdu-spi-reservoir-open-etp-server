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

#ifndef OES_EML_OPENKV_SPACEOSDUPROTO_H
#define OES_EML_OPENKV_SPACEOSDUPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>

namespace oes::eml::openkv {

using oes::common::etp12::Protocol::IRequest;
using oes::common::etp12::Protocol::EtpResponseHandler;

template <typename T> class LocalTransactionWriteRaii;

//============================================================================

struct OES_EML_EXPORT OpenKVDataspaceOSDU : BaseProto, oes::common::etp12::Protocol::IDataspaceOSDU
{
    OpenKVDataspaceOSDU(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    virtual void GetDataspaceInfo( // messageTypeId 1
        IRequest& ireq, GetDataspaceInfo_req_t req,
        EtpResponseHandler<GetDataspaceInfo_res_t> process_cb // messageTypeId 2
    ) final;

    virtual void CopyDataspacesContent( // messageTypeId 3
        IRequest& ireq, CopyDataspacesContent_req_t req,
        EtpResponseHandler<CopyDataspacesContent_res_t> process_cb // messageTypeId 4
    ) final;

    virtual void LockDataspaces( // messageTypeId 5
        IRequest& ireq, LockDataspaces_req_t req,
        EtpResponseHandler<LockDataspaces_res_t> process_cb // messageTypeId 6
    ) final;

    virtual void CopyToDataspace( // messageTypeId 7
        IRequest& ireq, CopyToDataspace_req_t req,
        EtpResponseHandler<CopyToDataspace_res_t> process_cb // messageTypeId 8
    ) final;
    
    virtual void CopyDataspaceFromRemote( // messageTypeId 9
        IRequest& ireq, CopyDataspaceFromRemote_req_t req,
        EtpResponseHandler<CopyDataspaceFromRemote_res_t> process_cb // messageTypeId 10
    ) final;

protected:
    void CopyDataspaceFromRemote_trigger(
        CopyDataspaceFromRemote_req_t req,
        EtpResponseHandler<CopyDataspaceFromRemote_res_t> process_cb
    );
    
    void CopyDataspaceFromRemote_dump(
        CopyDataspaceFromRemote_req_t req,
        EtpResponseHandler<CopyDataspaceFromRemote_res_t> process_cb
    );

    friend LocalTransactionWriteRaii<OpenKVDataspaceOSDU>;
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_SPACEOSDUPROTO_H
