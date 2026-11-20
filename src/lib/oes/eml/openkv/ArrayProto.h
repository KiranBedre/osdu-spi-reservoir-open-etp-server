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

// Forward-declared; full definitions live in ../etp12/Extensions.h,
// ../etp12/MultiPartReply.hpp and oes/core/utils/Otel.h and are only needed
// by ArrayProto.cpp.
} // namespace openkv
} // namespace eml
} // namespace oes
namespace oes::eml::etp12::DataArrayExt {
struct StatsQuery;
struct ReduceQuery;
struct WhereQuery;
struct MultiWhereQuery;
}
namespace oes::eml::etp12 { template <typename TResponse> class MultiPartReply; }
namespace oes::core { class OtelSpan; }
namespace oes {
namespace eml {
namespace openkv {

// WARNING: Custom extension. NOT part of official ETP 1.2
// Result of the compute-core array resolution step. Full definitions live in
// ArrayProto.cpp (keeps SqlAry / ErrorInfo out of this header).
struct ComputeOutcome;        // ok flag + the ETP error to surface on failure
struct ResolvedComputeArray;  // the opened reader + resolved SqlAry on success
struct ResolvedComputeArrays; // batched form: one SqlAry per input + readers

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

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Called from tryDispatchComputeOp() on a "STATS" req. Folds a named array
    // into a small summary inside the store and returns it via the reply's
    // DataArrayMetadata.m_customData - the array bytes never leave the store.
    // Returns true on success (a result part was sent), false when an error
    // part was sent instead.
    bool GetDataArrayStats(
        const etp12::DataArrayExt::StatsQuery& query,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    );

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Called from tryDispatchComputeOp() on a "REDUCE|..." req. Collapses a
    // 3-D [Nk, Nj, Ni] property over K into a 2-D [Nj, Ni] column map returned
    // via m_customData ("reduce.map"). Same true/false contract as STATS.
    bool GetDataArrayReduce(
        const etp12::DataArrayExt::ReduceQuery& query,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    );

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Called from tryDispatchComputeOp() on a "WHERE|lo|hi" req. Streams one
    // property through a masked predicate and returns count + ijk bbox + ijk
    // centroid via m_customData ("where.*"). Same true/false contract as STATS.
    bool GetDataArrayWhere(
        const etp12::DataArrayExt::WhereQuery& query,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    );

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Called from tryDispatchComputeOp() on a "WHEREN|idx|lo|hi" batch. ANDs N
    // per-array predicates over one grid and answers EVERY request key: the
    // first key carries the full result, the others a "where.ref" back to it.
    // Same true/false contract as STATS.
    bool GetDataArrayMultiWhere(
        const etp12::DataArrayExt::MultiWhereQuery& query,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb // messageTypeId 7
    );

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Shared compute detect+dispatch used by both GetDataArrayMetadata and
    // GetDataArrayMetadataBULK. Detects a compute op in the request, rejects a
    // malformed marker, enforces the process-wide concurrency gate, runs the
    // fold and records the success/error/rejection metrics. Returns true if
    // the request was a compute op - already answered via `reply` - and false
    // for an ordinary metadata request the caller should keep processing.
    bool tryDispatchComputeOp(
        GetDataArrayMetadata_req_t& req,
        oes::eml::etp12::MultiPartReply<GetDataArrayMetadata_res_t>& reply,
        EtpResponseHandler<GetDataArrayMetadata_res_t> process_cb,
        oes::core::OtelSpan& span);

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Compute-core resolution step: parse the URI, resolve the dataspace,
    // check CAN_READ access, open a reader and resolve the named array into a
    // SqlAry - independent of any ETP response encoding. On success fills
    // `out` and returns an ok ComputeOutcome; on failure returns the ETP error
    // to surface.
    ComputeOutcome resolveComputeArray(
        const std::string& uri_s, const std::string& path,
        ResolvedComputeArray& out);

    // WARNING: Custom extension. NOT part of official ETP 1.2
    // Batched form of resolveComputeArray for multi-array compute ops: groups
    // the inputs by dataspace, pays the access check + reader open once per
    // dataspace and resolves the arrays' metadata through the same bulk SQL
    // path the GetDataArrayMetadata handler uses (one round-trip per
    // `bulk_size` arrays). `out.resolved[i]` is invalid where input i could
    // not be resolved; the caller decides how to report that.
    ComputeOutcome resolveComputeArrays(
        const std::vector<std::string>& uris, const std::vector<std::string>& paths,
        std::size_t bulk_size, ResolvedComputeArrays& out);

protected:
    friend LocalTransactionWriteRaii<OpenKVDataArray>;
};

//============================================================================

} // namespace openkv
} // namespace eml
} // namespace oes

#endif//OES_EML_OPENKV_ARRAYPROTO_H
