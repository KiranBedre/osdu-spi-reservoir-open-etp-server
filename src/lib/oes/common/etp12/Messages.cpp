// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "MsgFlags.h"
#include "Protocols.h"
#include "Messages.hpp"
#include "Metadata.h"
#include "../AvroUtils.hpp"
#ifndef OES_COMMON_ETP12_NO_AVRO_JSON
#include "../EtpSchemas.h"
#endif

#include "codegen/Messages.i.ipp"

namespace oes::common::etp12 {

std::string msgHeaderAsString(
    const Datatypes::MessageHeader& msg_hdr,
    const MsgInfo* msg_info,
    const bool full_flag_desc
) {
    std::string flags;
    switch (msg_hdr.m_messageFlags) {
    case 0: flags = '-'; break; // no flags
    case MsgFlags::eFINALPART: flags = full_flag_desc ? "Final(F)" : "F"; break;
    default:
        if (msg_hdr.m_messageFlags & MsgFlags::eFINALPART) {
            full_flag_desc ? flags.append("Final(F)") : flags.append(1, 'F');
        }
        if (msg_hdr.m_messageFlags & MsgFlags::eCOMPRESSED_BODY) {
            full_flag_desc ? flags.append("CompressedBody(Z)") : flags.append(1, 'Z');
        }
        if (msg_hdr.m_messageFlags & MsgFlags::eACKNOWLEDGE_RECEIPT) {
            full_flag_desc ? flags.append("AcknowledgeReceipt(A)") : flags.append(1, 'A');
        }
        if (msg_hdr.m_messageFlags & MsgFlags::eHEADER_EXTENSION) {
            full_flag_desc ? flags.append("HeaderExtension(X)") : flags.append(1, 'X');
        }

        // make invalid flags visible
        constexpr int unknown_flag_mask = ~(
            MsgFlags::eFINALPART |
            MsgFlags::eCOMPRESSED_BODY|
            MsgFlags::eACKNOWLEDGE_RECEIPT|
            MsgFlags::eHEADER_EXTENSION
        );
        if (msg_hdr.m_messageFlags & unknown_flag_mask) {
            flags.append(1, '?');
        }
    }

    const int32_t mtype_id = msg_hdr.m_messageType;
    const bool cross_proto_msg = (1000 <= mtype_id && mtype_id < 2000);
    const int32_t proto_id = cross_proto_msg?
        int32_t(Datatypes::Protocol::eCore):
        msg_hdr.m_protocol
    ;

    if (msg_info) {
        assert(proto_id == msg_info->type_.proto_);
        assert(mtype_id == msg_info->type_.type_);
    } else {
        msg_info = MsgInfo::lookup(proto_id, mtype_id);
    }

    if (msg_info) {
        if (proto_id == msg_hdr.m_protocol) {
            // Normal message
            return fmt::format(
                "Msg({})_PMsg({}) {} {}",
                msg_hdr.m_messageId, msg_hdr.m_correlationId,
                msg_info->label_, flags
            );
        } else {
            // Cross-protocol message (i.e. ProtocolException or Acknowledge)
            // We must lookup the Protocol label independently
            auto proto_found = protocols().find(msg_hdr.m_protocol);
            if (proto_found != protocols().end()) {
                return fmt::format(
                    "Msg({})_PMsg({}) {}.{}({}) {}",
                    msg_hdr.m_messageId, msg_hdr.m_correlationId,
                    proto_found->second.label(), msg_info->name_, msg_hdr.m_messageType, flags
                );
            }
        }
    }

    // Unknown message
    return fmt::format(
        // avoid ??( trigraph warning on GCC
        "Msg({})_PMsg({}) " "???" "({})." "???" "({}) {}",
        msg_hdr.m_messageId, msg_hdr.m_correlationId,
        msg_hdr.m_protocol, msg_hdr.m_messageType, flags
    );
}

} // namespace oes::common::etp12
