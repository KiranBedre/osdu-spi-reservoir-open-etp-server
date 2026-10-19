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

#ifndef OES_EML_OPENKV_ARRAYPROTO_H
#define OES_EML_OPENKV_ARRAYPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>

namespace oes {
namespace eml {
namespace openkv {

using ::oes::common::etp12::Protocol::IRequest;
using ::oes::common::etp12::Protocol::EtpResponseHandler;

template <typename T> class LocalTransactionWriteRaii;

//============================================================================

struct OES_EML_EXPORT OpenKVDataArray : BaseProto, common::etp12::Protocol::IDataArray
{
    OpenKVDataArray(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    void GetDataArrays( // messageTypeId 2
        IRequest& ireq, GetDataArrays_req_t req,
        EtpResponseHandler<GetDataArrays_res_t> process_cb // messageTypeId 1
    ) final;
    
    void GetDataArraysBULK( // messageTypeId 2
        IRequest& ireq, GetDataArrays_req_t req,
        EtpResponseHandler<GetDataArrays_res_t> process_cb // messageTypeId 1
    );

    void GetDataSubarrays( // messageTypeId 3
        IRequest& ireq, GetDataSubarrays_req_t req,
        EtpResponseHandler<GetDataSubarrays_res_t> process_cb // messageTypeId 8
    ) final;
    
    void GetDataSubarraysBULK( // messageTypeId 3
        IRequest& ireq, GetDataSubarrays_req_t req,
        EtpResponseHandler<GetDataSubarrays_res_t> process_cb // messageTypeId 8
    );

    void PutDataArrays( // messageTypeId 4
        IRequest& ireq, PutDataArrays_req_t req,
        EtpResponseHandler<PutDataArrays_res_t> process_cb // messageTypeId 10
    ) final;
    
    void PutDataArraysBULK( // messageTypeId 4
        IRequest& ireq, PutDataArrays_req_t req,
        EtpResponseHandler<PutDataArrays_res_t> process_cb // messageTypeId 10
    );

    void PutDataSubarrays( // messageTypeId 5
        IRequest& ireq, PutDataSubarrays_req_t req,
        EtpResponseHandler<PutDataSubarrays_res_t> process_cb // messageTypeId 11
    ) final;
    
    void PutDataSubarraysBULK( // messageTypeId 5
        IRequest& ireq, PutDataSubarrays_req_t req,
        EtpResponseHandler<PutDataSubarrays_res_t> process_cb // messageTypeId 11
    );

    void GetDataArrayMetadata( // messageTypeId 6
        IRequest& ireq, GetDataArrayMetadata_req_t req,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    ) final;
    
    void GetDataArrayMetadataBULK( // messageTypeId 6
        IRequest& ireq, GetDataArrayMetadata_req_t req,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    );

    void PutUninitializedDataArrays( // messageTypeId 9
        IRequest& ireq, PutUninitializedDataArrays_req_t req,
        EtpResponseHandler<PutUninitializedDataArrays_res_t> process_cb // messageTypeId 12
    ) final;

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Called from GetDataArrayMetadata() above, on specific req
    void ListDataArrayMetadata(
        const std::string& space_uid_or_path,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    );

protected:
    friend LocalTransactionWriteRaii<OpenKVDataArray>;
};

//============================================================================

} // namespace openkv
} // namespace eml
} // namespace oes

#endif//OES_EML_OPENKV_ARRAYPROTO_H
