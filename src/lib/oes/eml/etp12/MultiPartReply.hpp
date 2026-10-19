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

#ifndef OES_EML_ETP12_MULTIPARTREPLY_H
#define OES_EML_ETP12_MULTIPARTREPLY_H

#include "Server.h"
#include <oes/common/etp12/Protocols.h>

#include <cassert>
#include <string>

namespace oes::eml::etp12 {

namespace Protocol = oes::common::etp12::Protocol;
namespace Datatypes = oes::common::etp12::Datatypes;

//============================================================================

/**
 * This struct represents the errors of a multi-part ETP response message.
 */
struct MultiPartError {
    Protocol::EtpResponseStatus rc_;

    /**
     * Returns the number of errors in the ETP response message.
    */
    size_t errorCount() const {
        return rc_.ex.m_errors.size();
    }

    /**
     * Adds an error to the ETP response message.
     * 
     * @param cid The ID of the error.
     * @param errc The error code.
     * @param msg The error message.
     * 
    */
    void addError(const std::string& cid, Datatypes::ErrorCodes errc, std::string msg) {
        addErrorStatus(rc_, cid, errc, std::move(msg));
    }

    /**
     * Adds an error to the ETP response message.
     * 
     * @param cid The ID of the error.
     * @param err The error information.
     * 
    */
    void addError(const std::string& cid, ErrorInfo err) {
        addErrorStatus(rc_, cid, std::move(err));
    }
};

//============================================================================

/**
 * This class represents a multi-part ETP response message. It inherits from the MultiPartError struct.
 */
template <typename TResponse>
class MultiPartReply : public MultiPartError {
    size_t cur_bytes_ = 0;

    Protocol::EtpResponseHandler<TResponse>& process_cb_;

    void callProcessCB() {
        process_cb_(std::move(rc_), std::move(resp_));
    }

public:
    const size_t max_count_;
    const size_t max_bytes_;

    TResponse resp_;

public:
    /**
     * Constructs a MultiPartReply object.
     *
     * @param session The ETP session.
     * @param process_cb The callback function to process the ETP response.
     */
    MultiPartReply(
        const Session& session,
        Protocol::EtpResponseHandler<TResponse>& process_cb
    ) : process_cb_(process_cb)
      , max_count_(session.max_batch_size())
      , max_bytes_(session.negotiated_max_msg_size())
    {
        static_assert(TResponse::info.isMultiPart());
    }

    /**
     * Returns the number of parts in the ETP response message.
     *
     * @return The number of parts in the ETP response message.
     */
    size_t partCount(){
        return resp_.range().size();
    }

    /**
     * Adds a vector part to the ETP response message.
     *
     * @return A reference to the newly added vector part.
     */
    auto& addVecPart() {
        return resp_.range().emplace_back();
    }

    /**
     * Adds a map part to the ETP response message.
     *
     * @param cid The ID of the map part.
     * @return A reference to the newly added map part.
     */
    auto addMapPart(const std::string& cid) {
        auto& r = resp_.range();
        using map_t = typename std::remove_reference<decltype(r)>::type;
        using val_t = typename map_t::mapped_type;
        return r.emplace(cid, val_t{});
    }

    /**
     * Adds a map part to the ETP response message.
     *
     * @param cid The ID of the map part.
     * @param val The value of the map part.
     * @return A reference to the newly added map part.
     */
    template <typename T>
    auto addMapPart(std::string cid, T&& val) {
        return resp_.range().emplace(
            std::move(cid),
            std::forward<T>(val)
        );
    }

    /**
     * Erases a part from the ETP response message.
     *
     * @param it An iterator to the part to be erased.
     * @param bytes The number of bytes to be subtracted from the total number of bytes in the ETP response message.
     * @return void
     * @throws None
     * @partof MultiPartReply
     */
    template <typename It>
    auto eraseMapPart(It&& it, size_t bytes = 0) {
        resp_.range().erase(std::forward<It>(it));
        subtractBytes(bytes);
    }

    /**
     * Adds to the total number of bytes in the ETP response message.
     *
     * @throws None
     * @return void
     * @partof MultiPartReply
     */
    void addBytes(size_t bytes) {
        cur_bytes_ += bytes;
    }

    /**
     * Removes from the total number of bytes in the ETP response message.
     * 
     * @throws None
     * @return void
     * @partof MultiPartReply
     */
    void subtractBytes(size_t bytes) {
        assert(cur_bytes_ >= bytes);
        cur_bytes_ -= bytes;
    }

    /**
     * Processes an additional message to be included in the multipart reply.
     * 
     * @param additional A shared pointer to the additional message to be included.
     * @return void
     * @throws None
     * @partof MultiPartReply
     */
    void processAdditional(std::shared_ptr<oes::common::etp12::AbstractMessage> additional) {
        rc_.ok = true;
        rc_.final_part = false;
        rc_.extra_messages.push_back(std::move(additional));
        rc_.ex.m_errors.clear();
        rc_.ex.m_error.reset();
    }

    /**
     * Add the bytes that needs to be sent by chunks
     * 
     * @param uuid of the chunk
     * @param chunks value to send
     * @return void
     * @throws None
     * @partof MultiPartReply
     */
    void addChunks(Datatypes::Uuid uuid, std::vector<uint8_t> chunks) {
        rc_.chunk_uuid = std::move(uuid);
        rc_.chunks     = std::move(chunks);
    }

    /**
     * Processes the ETP response message based on the current state of the MultiPartReply object.
     *
     * @param next_bytes The number of bytes in the next part of the ETP response message.
     * @param force Whether to force the processing of the ETP response message.
     * @return void
     * @throws None
     * @partof MultiPartReply
     */
    void maybeProcess(size_t next_bytes, bool force = false) {
        // Break large responses into multi-part ones.
        // Avoids stalling concurrent response pipelines.
        // Also avoids getting over the max-msg-size limit too often.
        // We do auto-split too-large responses, but we also want to
        // avoid "stalls", and returns results to the client ASAP.

        const bool too_big = (cur_bytes_ + next_bytes > max_bytes_);
        const bool too_many = (resp_.range().size() >= max_count_);
        // TODO: Time-based criteria too perhaps

        if (too_big || too_many || force) {
            rc_.ok = true;
            rc_.final_part = false;

            callProcessCB();

            cur_bytes_ = 0;
            resp_.range().clear();
            rc_.ex.m_errors.clear();
            rc_.ex.m_error.reset();
        }
    }

    /**
     * Processes the final part of the ETP response message.
     * 
     * @return void
     * @throws None
     * @partof MultiPartReply
     * @see MultiPartReply::maybeProcess
    */
    void finalProcess() {
        // A scalar error should have gone through fatalError(), not here.
        assert(!rc_.ex.m_error.has_value());

        /*
        // Partial success does imply a mix of parts and error, so OK and does happen.
        assert(rc_.ex.m_errors.empty());
        */

        /*
        // It is OK to end a multi-response reply with an "empty" response.
        assert(!resp_.range().empty());
        */

        rc_.ok = true;
        rc_.final_part = true;

        callProcessCB();
    }

    /**
     * Processes a fatal error in the ETP response message.
     * 
     * @return void
     * @throws None
     * @partof MultiPartReply
    */
    void fatalError() {
        /*
        assert(!rc_.ok);

        // Should have a *scalar* error, with non-zero code and non-empty message
        assert(rc_.ex.m_error.has_value());
        assert(rc_.ex.m_error->m_code != 0);
        assert(!rc_.ex.m_error->m_message.empty());

        // These two may be wrong. Perhaps the fatalError() occurred after
        // some processing that did results in some "parts" or errors...
        // .clear() those two instead of asserting them?
        assert(rc_.ex.m_errors.empty());
        assert(resp_.range().empty());

        assert(rc_.final_part);
*/
        callProcessCB();
    }

    /**
     * Processes a fatal error in the ETP response message.
     * 
     * @param errc The error code.
     * @param msg The error message.
     * @return void
     * @throws None
     * @partof MultiPartReply
    */
    void fatalError(Datatypes::ErrorCodes errc, std::string msg) {
        rc_ = errorStatus(errc, std::move(msg));
        assert(!rc_.ok && rc_.final_part);

        // resp_.range().clear() or NOT???
        // there could be stuff in there, e.g. OpenKVStore::PutDataObjects

        callProcessCB();
    }
};

//============================================================================

} // namespace oes::eml::etp12

#endif
