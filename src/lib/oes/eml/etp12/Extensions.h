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

#ifndef OES_EML_ETP12_EXTENSIONS_H
#define OES_EML_ETP12_EXTENSIONS_H

#include <oes/eml/export.h>

#include <oes/common/etp12/ArrayUtils.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace oes::eml::etp12::DataArrayExt {

namespace common12 = oes::common::etp12;

using AryMetaMap = std::map<std::string, common12::AryMeta>;
namespace Protocol = ::oes::common::etp12::Protocol;

OES_EML_EXPORT
bool is_list_arrays_req(
    const common::etp12::Protocol::DataArray::GetDataArrayMetadata& req,
    std::string& dataspace
);

OES_EML_EXPORT
Protocol::DataArray::GetDataArrayMetadata to_list_arrays_req(
    const std::string& dataspace_uid // or _path for OpenKV
);

//----------------------------------------------------------------------------
// Server-side array statistics query (custom EXTENSION, not official ETP 1.2).
//
// Overloads GetDataArrayMetadata so the store can fold an array into a small
// summary (count/min/max/mean/stddev/sum, plus an optional in-range count)
// without ever shipping the cell values to the caller. The correlation-id key
// carries the marker and, optionally, a range predicate and a histogram-bin
// count:
//   "STATS"                  -> summary only
//   "STATS|<lo>|<hi>"        -> summary + count of finite values in [lo, hi]
//   "STATS|<lo>|<hi>|<bins>" -> as above + a <bins>-bin histogram over [lo, hi]
//                               and p10/p50/p90 derived from it
// The array is named by the single AryIdent (m_uri + m_pathInResource).
// The marker must be exactly "STATS" or start with "STATS|"; any other key
// (e.g. a plain correlation id "STATS_poro") is NOT a compute request.
// A key that starts with "STATS|" but does not parse (missing/invalid/
// non-finite bounds, lo > hi, bins == 0 or > kMaxStatsHistBins) is reported
// through \p err and the caller MUST reject the request rather than fall back
// to plain metadata.
//
// The reply reuses GetDataArrayMetadata: the single DataArrayMetadata carries
// the result in its m_customData map (key -> DataValue). Keys:
//   compute.schema     long    kComputeSchemaVersion
//   stats.valid        bool    false when the array had no finite value
//   stats.count        long    finite (non-null, non-NaN) value count
//   stats.nullCount    long    null/NaN count
//   stats.min/max      double  }
//   stats.mean/stddev  double  } present only when stats.valid
//   stats.sum          double  }
//   stats.inRangeCount long    ) present only for "STATS|lo|hi[|bins]"
//   stats.rangeLo/Hi   double  )
//   stats.histogram    long[]  ) present only for "STATS|lo|hi|bins";
//   stats.histBins     long    )   histogram is a native ArrayOfLong of
//   stats.histLo/Hi    double  )   per-bin counts over [histLo, histHi]
//   stats.histUnderflow long   )   with the out-of-domain tails split out,
//   stats.histOverflow long    )   and p10/p50/p90 interpolated from it
//   stats.p10/p50/p90  double  )   (approximate to within one bin width)
//----------------------------------------------------------------------------

//! Schema version of the compute-extension wire contract (marker syntax and
//! the m_customData result keys). Echoed in every compute reply under
//! kComputeSchemaKey so a client can detect the result shape. Bump ONLY on a
//! breaking change; additive keys do not require a bump.
inline constexpr int64_t kComputeSchemaVersion = 1;

//! customData key carrying kComputeSchemaVersion in every compute reply.
inline constexpr const char* kComputeSchemaKey = "compute.schema";

//! Hard upper bound on the histogram bin count accepted from the wire
//! (65536 bins = 512 KiB of counters). Larger requests are rejected.
inline constexpr uint32_t kMaxStatsHistBins = 65536;

struct StatsQuery {
    std::string cid;            //!< correlation-id / reply key echoed back.
    std::string uri;            //!< dataspace/resource URI from the AryIdent.
    std::string path;           //!< array path within the resource.
    bool has_range = false;     //!< true when a [lo, hi] predicate was supplied.
    double range_lo = 0.0;
    double range_hi = 0.0;
    bool has_histogram = false; //!< true when a histogram-bin count was supplied.
    uint32_t hist_bins = 0;     //!< number of fixed bins over [range_lo, range_hi].
};

//! Detect the STATS extension request.
//! Returns true when the request is a STATS request (exactly one data-array
//! entry whose key is "STATS" or starts with "STATS|"). On true, either
//! \p out is filled and \p err is empty, or the marker was malformed and
//! \p err holds a human-readable reason (the request must then be rejected).
//! Returns false for any non-STATS request (no error).
OES_EML_EXPORT
bool is_array_stats_req(
    const Protocol::DataArray::GetDataArrayMetadata& req,
    StatsQuery& out,
    std::string& err
);

//! Build a STATS extension request for the given array, optionally with an
//! inclusive [lo, hi] range predicate and a histogram-bin count over that range.
OES_EML_EXPORT
Protocol::DataArray::GetDataArrayMetadata to_array_stats_req(
    const std::string& dataspace_uid,
    const std::string& path,
    bool has_range = false,
    double range_lo = 0.0,
    double range_hi = 0.0,
    uint32_t hist_bins = 0
);

//----------------------------------------------------------------------------
// Server-side *spatial* array reductions (custom EXTENSIONS, not official
// ETP 1.2). All overload GetDataArrayMetadata exactly like STATS and carry
// the answer in the reply's DataArrayMetadata::m_customData. The grid shape
// (Ni, Nj, Nk) is taken from the stored array's dimensions ({Nk, Nj, Ni} in
// C order), so the markers stay small. The same strictness as STATS applies:
// a key that starts with a known marker prefix but does not parse is reported
// through \p err and MUST be rejected (never served as plain metadata).
//
// REDUCE — collapse a 3-D property [Nk, Nj, Ni] over the K axis into a 2-D
// [Nj, Ni] column map. Marker (fields are positional):
//   "REDUCE|<op>"                           whole-column fold
//   "REDUCE|<op>|<k0>|<k1>"                 restricted to the [k0, k1) K-zone
//   "REDUCE|<op>|<k0>|<k1>|<lo>|<hi>"       count/net use the [lo, hi] predicate
//   "REDUCE|<op>|<k0>|<k1>|<lo>|<hi>|<dz>"  net multiplies by the cell thickness dz
// op in {sum, mean, min, max, count, net}. <k0>|<k1> must be integers with
// 0 <= k0 < k1, or the placeholder "-|-" meaning "no K-zone" (used when only
// the range / dz fields are wanted). <lo> <= <hi> must be finite; <dz> must
// be finite and > 0. count/net REQUIRE a range; sum/mean/min/max refuse one.
// Reply customData:
//   compute.schema   long     kComputeSchemaVersion
//   reduce.op        string   echoed operator
//   reduce.ni/nj     long     map shape ([nj, ni], row-major)
//   reduce.nk        long     source layer count
//   reduce.k0/k1     long     effective K-zone (k1 clamped to nk), when given
//   reduce.rangeLo/Hi double  echoed predicate (count/net)
//   reduce.dz        double   effective cell thickness (net)
//   reduce.columns   long     columns that folded >= 1 value
//   reduce.map       double[] native ArrayOfDouble, nj*ni values
//                             (NaN for empty columns under mean/min/max)
// The map is capped at kMaxReduceMapCells cells (ELIMIT_EXCEEDED beyond).
//
// WHERE — masked predicate over one property: count the cells whose value is
// in [lo, hi] (inclusive, finite, lo <= hi) and return their inclusive ijk
// bounding box + ijk centroid. Marker: "WHERE|<lo>|<hi>". Reply customData:
//   compute.schema   long
//   where.valid      bool     false when no cell passed
//   where.count      long     cells in range
//   where.rangeLo/Hi double   echoed predicate
//   where.iMin/iMax/jMin/jMax/kMin/kMax  long   (valid only)
//   where.ci/cj/ck   double   ijk centroid (valid only)
//
// WHEREN — AND of N per-array [lo, hi] predicates over arrays of the SAME
// grid shape. Every map entry is keyed "WHEREN|<idx>|<lo>|<hi>" where <idx>
// is a non-negative integer that only keeps the keys distinct; at most
// kMaxMultiWhereEntries entries. A batch mixing WHEREN and other keys is
// malformed. EVERY request key gets a reply part (or, on failure, an error):
// the lexicographically first key carries the full result (where.* keys as
// for WHERE, plus where.arrays, where.arraysStreamed (fewer than
// where.arrays when an empty mask short-circuited the remaining reads) and
// the largest 6-connected body
// where.components/bodyCount/biMin..bkMax/bci/bcj/bck); every other key
// carries its own array's metadata plus where.ref = <first key> and its
// echoed predicate (where.rangeLo/Hi).
//----------------------------------------------------------------------------

//! Maximum number of cells (nj*ni) a REDUCE map may carry in one reply
//! (1 Mi doubles = 8 MiB, well inside the default ETP message payload cap).
inline constexpr uint64_t kMaxReduceMapCells = uint64_t(1) << 20;

//! Maximum number of WHEREN predicate arrays in one request.
inline constexpr std::size_t kMaxMultiWhereEntries = 16;

struct ReduceQuery {
    std::string cid;        //!< correlation-id / reply key echoed back.
    std::string uri;        //!< dataspace/resource URI from the AryIdent.
    std::string path;       //!< array path within the resource.
    std::string op;         //!< raw operator token (validated by the handler).
    bool has_k_range = false;
    uint64_t k0 = 0;
    uint64_t k1 = 0;
    bool has_range = false; //!< [lo, hi] predicate for count/net.
    double range_lo = 0.0;
    double range_hi = 0.0;
    bool has_dz = false;
    double cell_dz = 1.0;   //!< per-cell thickness for net.
};

struct WhereQuery {
    std::string cid;  //!< correlation-id / reply key echoed back.
    std::string uri;  //!< dataspace/resource URI from the AryIdent.
    std::string path; //!< array path within the resource.
    double range_lo = 0.0;
    double range_hi = 0.0;
};

//! One per-array predicate of a WHEREN request.
struct MultiWhereEntry {
    std::string cid;  //!< this entry's own request key ("WHEREN|<idx>|<lo>|<hi>").
    std::string uri;  //!< dataspace/resource URI from the AryIdent.
    std::string path; //!< array path within the resource.
    double range_lo = 0.0;
    double range_hi = 0.0;
};

struct MultiWhereQuery {
    std::string cid;                      //!< first key; carries the full result.
    std::vector<MultiWhereEntry> entries; //!< 1..kMaxMultiWhereEntries, in key order.
};

//! Detect the REDUCE extension request (single entry keyed "REDUCE|...").
//! Same contract as is_array_stats_req: true + empty \p err on success, true +
//! non-empty \p err when the marker is malformed, false when not a REDUCE.
OES_EML_EXPORT
bool is_array_reduce_req(
    const Protocol::DataArray::GetDataArrayMetadata& req,
    ReduceQuery& out,
    std::string& err
);

//! Build a REDUCE extension request for the given array + operator.
OES_EML_EXPORT
Protocol::DataArray::GetDataArrayMetadata to_array_reduce_req(
    const std::string& dataspace_uid,
    const std::string& path,
    const std::string& op,
    bool has_k_range = false,
    uint64_t k0 = 0,
    uint64_t k1 = 0,
    bool has_range = false,
    double range_lo = 0.0,
    double range_hi = 0.0,
    bool has_dz = false,
    double cell_dz = 1.0
);

//! Detect the WHERE extension request (single entry keyed "WHERE|<lo>|<hi>").
//! Same contract as is_array_stats_req.
OES_EML_EXPORT
bool is_array_where_req(
    const Protocol::DataArray::GetDataArrayMetadata& req,
    WhereQuery& out,
    std::string& err
);

//! Build a WHERE extension request for the given array + [lo, hi] predicate.
OES_EML_EXPORT
Protocol::DataArray::GetDataArrayMetadata to_array_where_req(
    const std::string& dataspace_uid,
    const std::string& path,
    double range_lo,
    double range_hi
);

//! Detect the WHEREN extension request: ANY entry keyed "WHEREN|..." makes
//! the batch a WHEREN request; it is malformed (\p err) unless EVERY entry is
//! a well-formed "WHEREN|<idx>|<lo>|<hi>" key and there are at most
//! kMaxMultiWhereEntries of them. Same contract as is_array_stats_req.
OES_EML_EXPORT
bool is_array_multiwhere_req(
    const Protocol::DataArray::GetDataArrayMetadata& req,
    MultiWhereQuery& out,
    std::string& err
);

//! Build a WHEREN request from N (path, [lo, hi]) predicates. Each entry's
//! uri, when empty, defaults to the dataspace URI of \p dataspace_uid.
OES_EML_EXPORT
Protocol::DataArray::GetDataArrayMetadata to_array_multiwhere_req(
    const std::string& dataspace_uid,
    const std::vector<MultiWhereEntry>& entries
);

OES_EML_EXPORT
std::string to_json_meta_key(const common12::AryIdent& ident);

OES_EML_EXPORT
common12::AryIdent from_json_meta_key(std::string& ident_json);

OES_EML_EXPORT
common12::AryInfoVec move_to_info_vec(AryMetaMap& map);

} // namespace oes::eml::etp12::DataArrayExt

#endif
