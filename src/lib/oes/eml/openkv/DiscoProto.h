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

#ifndef OES_EML_OPENKV_DISCOPROTO_H
#define OES_EML_OPENKV_DISCOPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>

namespace oes::eml::etp12{
    template <typename TResponse> class MultiPartReply;
}

namespace oes::eml::openkv {

using ::oes::common::etp12::Protocol::IRequest;
using ::oes::common::etp12::Protocol::EtpResponseHandler;
using oes::eml::etp12::MultiPartReply;
using GetResourcesEdges_res_t = oes::common::etp12::Protocol::Discovery::GetResourcesEdgesResponse;

//============================================================================

struct OES_EML_EXPORT OpenKVDiscovery : BaseProto, ::oes::common::etp12::Protocol::IDiscovery
{
    OpenKVDiscovery(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    void GetResources( // messageTypeId 1
        IRequest& ireq, GetResources_req_t req,
        EtpResponseHandler<GetResources_res_t> process_cb // messageTypeId 4
    ) final;

    void GetDeletedResources( // messageTypeId 5
        IRequest& ireq, GetDeletedResources_req_t req,
        EtpResponseHandler<GetDeletedResources_res_t> process_cb // messageTypeId 6
    ) final;

private:
    void getObjectSelf(
        const EmlUri& uri, const Connection_ptr& spacedb,
        IRequest& ireq, GetResources_req_t req,
        MultiPartReply<GetResources_res_t>& reply
    );
    void getSpaceResources(
        const EmlUri& uri, const Connection_ptr& spacedb,
        IRequest& ireq, GetResources_req_t req,
        MultiPartReply<GetResources_res_t>& reply
    );
    void getResourcesTargets(
        bool self, const EmlUri& uri, const Connection_ptr& spacedb,
        IRequest& ireq, GetResources_req_t req,
        MultiPartReply<GetResources_res_t>& reply
    );
    void getResourcesSources(
        bool self, const EmlUri& uri, const Connection_ptr& spacedb,
        IRequest& ireq, GetResources_req_t req,
        MultiPartReply<GetResources_res_t>& reply
    );
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_DISCOPROTO_H
