#pragma once
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

#include "../AvroUtils.hpp"
#include "../CompressUtils.hpp"

#include "Messages.h"
#include "MsgFlags.h"
#include "Protocols.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#  include <avro/Stream.hh>
#  include <websocketpp/frame.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <cassert>
#include <chrono>
#include <string>

/*!
 * \file oes/eml/etp12/EncoderSender.hpp
 * \brief Utility to encode C++ structures using Avro and WebSocketPP
 */

namespace oes::common::etp12 {

//============================================================================

struct EncodeAndSendRc {
    bool compressed_ = false;
    uint64_t header_bytes_ = 0;
    uint64_t body_bytes_ = 0;
    uint64_t bytes_sent_ = 0;
    double deflate_millis_ = 0;

    size_t actual_part_count_ = 0;
    size_t total_part_count_ = 0;

    bool was_split() const {
        return total_part_count_ && (actual_part_count_ != total_part_count_);
    }

    double body_zfactor() const {
        return 1. * body_bytes_ / (bytes_sent_ - header_bytes_);
    }
};

//============================================================================

inline
size_t updateHeader(
    ::avro::Encoder& enc, std::string& msg,
    const MsgHeader& hdr, size_t old_header_bytes
) {
    // Encoded header is between 5 bytes (all fields zeros) and
    // 35 bytes (all fields to their 32 or 64-bit min's or max's)
    // In practice, it's more in the 6-10 bytes range. Use 16 for the
    // increment and initial size, to avoid heap-alloc in std::string
    std::string hdr_str;
    StringAsAvroOStreamView hdr_aos(hdr_str, 16, 64);

    enc.init(hdr_aos);
    hdr.encode(enc);
    enc.flush();

    // Often, old_header_bytes == new_header_bytes, so replace() call below
    // can be in-place and fast. Sometimes, must move the body a little, which
    // is most expensive, unless that body is small
    const uint64_t new_header_bytes = hdr_aos.byteCount();
    assert(hdr_str.size() == new_header_bytes);
    msg.replace(0, old_header_bytes, hdr_str);

    return new_header_bytes;
}

//============================================================================

template <typename TConn, typename TPreFunc, typename TPostFunc>
bool doEncodeAndSend(
    TConn& conn, MsgHeader& hdr,
    std::function<void(avro::Encoder&)>& msg_enc,
    MsgId& msg_id, size_t max_size, size_t chunk_size,
    const ZCompressor* zcompressor,
    TPreFunc&& pre_send_func, TPostFunc&& post_send_func
) {
    // In the past, we used ::avro::memoryOutputStream() and ::avro::snapshot(),
    // but that results in needless copies. Now use a custom Avro-stream, using
    // an std::string as backend (with x2 growth, instead of linear growth previously),
    // which can then be handed directly (std:move'd) to the WebSocketPP send()
    std::string msg_str;
    max_size = std::max<size_t>(max_size, chunk_size);
    StringAsAvroOStreamView aos(msg_str, chunk_size, max_size - 1);
    // Using (max_size - 1) above, in case we have to split the message,
    // to ensure we always have room to add the trailing 0 byte to close
    // the collection (map or array) in enc.split_counts_->updateBody()
    // below, when we need to split an oversize message.

    const bool had_FIN_bit = has_FIN_bit(hdr);

    const bool can_compress =
        zcompressor &&

        // Protocols::Core::Acknowledge is zero sized
        (hdr.m_messageType != 1001) &&

        // Protocols::Core::ResquestSession responses are never compressed, i.e.
        // Protocols::Core::OpenSession, but also potential errors too!
        !(hdr.m_protocol == 0 && hdr.m_messageType == 2) && // Not OpenSession
        !(hdr.m_protocol == 0 && hdr.m_messageType == 1000) // Not ProtocolException

        // And large enough! Cannot know that now. Done later below
    ;

    /*
    ** For faster encoding of large float/double vectors, we use
    ** a special "pass-thru" encoder to an internal ::avro::binaryEncoder()
    ** but which allows to "intercept" calls to encode those vectors,
    ** and use a wire-compatible but faster implementation, specifically
    ** in the binary-encoding case (NOT the JSON case, still a TODO anyway).
    **
    const ::avro::EncoderPtr& enc = ::avro::binaryEncoder(); // TODO: JSON
    */
    PassThruEncoder enc(::avro::binaryEncoder(), /*binary*/true);

    bool done = false;
    for (int pass = 0; !done; ++pass) {
        assert(!pass || enc.wasSplit());

        EncodeAndSendRc rc;

        if (can_compress) {
            set_ZIP_bit(hdr);
            rc.compressed_ = true;
        }

        msg_str.clear();
        enc.init(aos);
        hdr.m_messageId = msg_id.nextId();
        hdr.encode(enc);
        enc.flush();   // calls ::avro::OutputStream::backup()

        uint64_t header_bytes = aos.byteCount();
        rc.header_bytes_ = header_bytes;

        bool header_needs_update = false;

        if (pass == 0) {
            assert(!enc.split_counts_);

            msg_enc(enc);   // throws if message is oversize, and cannot be split
            enc.flush();    // calls ::avro::OutputStream::backup(), for minimum message
            done = !enc.wasSplit();

            if (!done) {
                assert(enc.split_counts_);
                enc.split_counts_->updateBody(msg_str);

                if (had_FIN_bit) {
                    unset_FIN_bit(hdr);
                    header_needs_update = true;
                }
            }
        } else {
            assert(enc.split_encoder_);
            done = enc.split_encoder_(enc); // can still throw on oversize!
            enc.flush(); // see above

            // needed even when done, to add the 0 byte trailer (see above)
            enc.split_counts_->updateBody(msg_str);

            if (done && had_FIN_bit) {
                set_FIN_bit(hdr);
                header_needs_update = true;
            }
        }

        if (header_needs_update) {
            const uint64_t new_header_bytes = updateHeader(
                enc, msg_str, hdr, header_bytes
            );
            rc.header_bytes_ += (new_header_bytes - header_bytes);
            header_bytes = new_header_bytes;
        }

        size_t body_bytes = (msg_str.size() - header_bytes);
        rc.body_bytes_ = body_bytes;

        if (rc.compressed_) {
            assert(zcompressor);
            assert(body_bytes > 0); // body not zero-sized

            // Do not compress small messages, actually makes them larger typically
            if (body_bytes < 256) {
                unset_ZIP_bit(hdr);
                rc.compressed_ = false;
                const uint64_t new_header_bytes = updateHeader(
                    enc, msg_str, hdr, header_bytes
                );
                rc.header_bytes_ += (new_header_bytes - header_bytes);
                header_bytes = new_header_bytes;
            } else {
                const auto znow = std::chrono::high_resolution_clock::now();

                std::string_view body(msg_str);
                body = body.substr(header_bytes);

                std::string zbody;
                if (!zcompressor->deflate(body, zbody)) {
                    return false;
                }

                msg_str.replace(header_bytes, msg_str.size() - header_bytes, zbody);

                const std::chrono::duration<double, std::milli> zdur =
                    std::chrono::high_resolution_clock::now() - znow
                ;
                rc.deflate_millis_ = zdur.count();
            }
        }

        if (msg_str.size() > max_size) {
            return false; // not sure it's possible. So just in case
        }

        // Record the size before the message is moved-from
        rc.bytes_sent_ = msg_str.size();
        assert(
            rc.compressed_ ||
            (rc.bytes_sent_ == (rc.header_bytes_ + rc.body_bytes_))
        );

        // Let client know *BEFORE* we send(), with actual hdr used.
        // Important, before client must register its handler before send()
        // Also copies the message for tracing, if needed, before msg_str moved-from.
        if (std::string* p_msg_copy = pre_send_func(hdr)) {
            *p_msg_copy = msg_str; // FIXME: Avoid copy
        }

        // Avoid conn->send(msg_str), which implies another copy
        auto msg = conn->get_message(websocketpp::frame::opcode::BINARY, 0);
        msg->get_raw_payload() = std::move(msg_str);
        conn->send(std::move(msg)); // Async. So queued-up for sending really.

        // Let client know we did send(), for actual logging/tracing
        if (enc.split_counts_) {
            rc.actual_part_count_ = enc.split_counts_->actual_;
            rc.total_part_count_ = enc.split_counts_->total_;
        }
        post_send_func(hdr, rc);
    }

    return true;
}

//============================================================================

} // namespace oes::common::etp12
