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

#ifndef OES_EML_OPENKV_STOREPROTO_H
#define OES_EML_OPENKV_STOREPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>

namespace oes::eml::openkv {

using oes::common::etp12::Protocol::IRequest;
using oes::common::etp12::Protocol::EtpResponseHandler;

template <typename T> class LocalTransactionWriteRaii;

//============================================================================

struct OES_EML_EXPORT OpenKVStore : BaseProto, oes::common::etp12::Protocol::IStore
{
    OpenKVStore(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    void GetDataObjects( // messageTypeId 1
        IRequest& ireq, GetDataObjects_req_t req,
        EtpResponseHandler<GetDataObjects_res_t> process_cb // messageTypeId 4
    ) final;

    void PutDataObjects( // messageTypeId 2
        IRequest& ireq, PutDataObjects_req_t req,
        EtpResponseHandler<PutDataObjects_res_t> process_cb // messageTypeId 9
    ) final;

    void DeleteDataObjects( // messageTypeId 3
        IRequest& ireq, DeleteDataObjects_req_t req,
        EtpResponseHandler<DeleteDataObjects_res_t> process_cb // messageTypeId 10
    ) final;

    void Chunk( // messageTypeId 8
        IRequest& ireq, Chunk_req_t req,
        EtpResponseHandler<PutDataObjects_res_t> process_cb // messageTypeId 9
    ) final;

protected:
    void PutDataObjectsImpl(
        int64_t messageId,
        PutDataObjects_req_t req,
        EtpResponseHandler<PutDataObjects_res_t> process_cb
    );
    friend LocalTransactionWriteRaii<OpenKVStore>;
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_STOREPROTO_H
