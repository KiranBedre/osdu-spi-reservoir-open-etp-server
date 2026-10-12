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

#ifndef OES_EML_OPENKV_TYPESPROTO_H
#define OES_EML_OPENKV_TYPESPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>

namespace oes::eml::openkv {

using oes::common::etp12::Protocol::IRequest;
using oes::common::etp12::Protocol::EtpResponseHandler;

//============================================================================

struct OES_EML_EXPORT OpenKVSupportedTypes : BaseProto, oes::common::etp12::Protocol::ISupportedTypes
{
    OpenKVSupportedTypes(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    void GetSupportedTypes( // messageTypeId 1
        IRequest& ireq, GetSupportedTypes_req_t req,
        EtpResponseHandler<GetSupportedTypes_res_t> process_cb // messageTypeId 2
    ) final;
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_TYPESPROTO_H
