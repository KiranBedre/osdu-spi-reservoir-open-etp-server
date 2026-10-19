// ============================================================================
// Copyright 2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_COMMON_ETP12_MSGFLAGS_H
#define OES_COMMON_ETP12_MSGFLAGS_H

#include "Messages.h"

namespace oes::common::etp12 {

using MsgHeader = Datatypes::MessageHeader;

//============================================================================

enum MsgFlags {
    //! \deprecated No longer part of ETP 1.2
    eUNUSED1 = 0x01, // was eMULTIPART

    /*!
     * \brief Serves as a "finish bit" (FIN bit) to indicate the "end" of a
     *     message or the last message of a multipart request or response.
     *
     * It MUST be set by the sender role only.
     * It MUST be set on an individual ETP message and on the last message ONLY
     * in a multipart response or request.
     *
     * Examples:
     * a) If an ETP server is streaming individual data points, one in each message;
     *    set this flag on each message.
     * b) If a request or response is composed of only 1 message, set this flag.
     * c) If a request or response is composed of a series of messages,
     *    set this flag on the last message of the series only (i.e., for a
     *    request composed of 5 messages, set this flag on the 5th message only.)
     * 
     * \sa has_FIN_bit
     * \sa set_FIN_bit
     */
    eFINALPART = 0x02,

    //! \deprecated No longer part of ETP 1.2
    eUNUSED2 = 0x04, //was eMULTINONE

    /*!
     * \brief Indicates that the body of this message is compressed.
     *
     * It may be set by either role. The body of any message (except those
     * explicitly excluded in Protocol 0), can be compressed, regardless of
     * role, based on the compression encoding negotiated during
     * initialization of the ETP session..
     *
     * \sa has_ZIP_bit
     * \sa set_ZIP_bit
     */
    eCOMPRESSED_BODY = 0x08,

    /*!
     * \brief Indicate that the sender is requesting an Acknowledge receipt of the message.
     *
     * An AcknowledgeMessage MUST NOT have this bit flag set.
     *
     * \sa has_ACK_bit
     * \sa set_ACK_bit
     */
    eACKNOWLEDGE_RECEIPT = 0x10,

    /*!
     * \brief Indicates the presence of an additional extended header
     *     between the regular header and the body.
     *
     * Usage of extended header must have been negotiated before.
     *
     * \sa has_EXT_bit
     * \sa set_EXT_bit
     */
    eHEADER_EXTENSION = 0x20,
};

//============================================================================

inline
bool has_FIN_bit(const MsgHeader& hdr) {
    return hdr.m_messageFlags & MsgFlags::eFINALPART;
}
inline
void set_FIN_bit(MsgHeader& hdr) {
    hdr.m_messageFlags |= MsgFlags::eFINALPART;
}
inline
void unset_FIN_bit(MsgHeader& hdr) {
    hdr.m_messageFlags &= ~MsgFlags::eFINALPART;
}

inline
bool has_ACK_bit(const MsgHeader& hdr) {
    return hdr.m_messageFlags & MsgFlags::eACKNOWLEDGE_RECEIPT;
}
inline
void set_ACK_bit(MsgHeader& hdr) {
    hdr.m_messageFlags |= MsgFlags::eACKNOWLEDGE_RECEIPT;
}

inline
bool has_EXT_bit(const MsgHeader& hdr) {
    return hdr.m_messageFlags & MsgFlags::eHEADER_EXTENSION;
}
inline
void set_EXT_bit(MsgHeader& hdr) {
    hdr.m_messageFlags |= MsgFlags::eHEADER_EXTENSION;
}

inline
bool has_ZIP_bit(const MsgHeader& hdr) {
    return hdr.m_messageFlags & MsgFlags::eCOMPRESSED_BODY;
}
inline
void set_ZIP_bit(MsgHeader& hdr) {
    hdr.m_messageFlags |= MsgFlags::eCOMPRESSED_BODY;
}
inline
void unset_ZIP_bit(MsgHeader& hdr) {
    hdr.m_messageFlags &= ~MsgFlags::eCOMPRESSED_BODY;
}

//============================================================================

} // namespace oes::common::etp12

#endif
