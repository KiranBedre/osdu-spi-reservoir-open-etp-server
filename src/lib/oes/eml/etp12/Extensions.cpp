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

#include "Extensions.h"

#include <oes/common/etp12/EmlUri.h>
#include <oes/common/AvroUtils.hpp>

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

/*!
 * \file oes/eml/etp12/Extensions.h
 * \brief Custom extensions to Energistics ETP 1.2.
 */

namespace oes::eml::etp12::DataArrayExt {

using namespace ::oes::common::etp12::Protocol::DataArray;
using ::oes::common::etp12::EmlUri;

//============================================================================

bool is_list_arrays_req(const GetDataArrayMetadata& req, std::string& dataspace) {
    if (req.m_dataArrays.size() != 1) {
        dataspace.clear();
        return false;
    }

    auto p_first = req.m_dataArrays.begin();
    const std::string& cid = p_first->first;
    const common12::AryIdent& ary_id = p_first->second;
    EmlUri uri = EmlUri::parse(ary_id.m_uri);
    if (cid == "ALL" && ary_id.m_pathInResource == "*" && uri.isSpace()) {
        dataspace = std::move(uri.dataspace_);
        return true;
    }

    dataspace.clear();
    return false;
}

GetDataArrayMetadata to_list_arrays_req(
    const std::string& dataspace_uid // or _path for OpenKV
) {
    GetDataArrayMetadata req;

    common12::AryIdent ident;
    ident.m_uri = EmlUri::make_space(dataspace_uid).to_string();
    ident.m_pathInResource = "*"; // WARNING: Custom EXTENSION. Not official ETP 1.2
    req.m_dataArrays.emplace("ALL", ident);

    return req;
}

//============================================================================

namespace {

//! Split \p s on '|' into tokens (empty tokens preserved).
std::vector<std::string> split_marker(const std::string& s) {
    std::vector<std::string> out;
    std::string::size_type beg = 0;
    for (;;) {
        const auto pos = s.find('|', beg);
        out.emplace_back(s.substr(beg, pos == std::string::npos ? pos : pos - beg));
        if (pos == std::string::npos) break;
        beg = pos + 1;
    }
    return out;
}

//! Parse a whole token as a finite double. Rejects empty / trailing garbage /
//! nan / inf so a marker like "STATS|1e400|2" or "STATS|nan|1" is refused.
bool parse_finite_double(const std::string& tok, double& v) {
    if (tok.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const double d = std::strtod(tok.c_str(), &end);
    if (end == tok.c_str() || *end != '\0' || errno == ERANGE) return false;
    if (!std::isfinite(d)) return false;
    v = d;
    return true;
}

//! Parse a whole token as an unsigned integer in [1, max].
bool parse_bins(const std::string& tok, uint32_t max, uint32_t& v) {
    if (tok.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long u = std::strtoull(tok.c_str(), &end, 10);
    if (end == tok.c_str() || *end != '\0' || errno == ERANGE) return false;
    if (tok[0] == '-' || tok[0] == '+') return false; // strtoull accepts a sign
    if (u == 0 || u > max) return false;
    v = static_cast<uint32_t>(u);
    return true;
}

} // namespace

bool is_array_stats_req(
    const GetDataArrayMetadata& req, StatsQuery& out, std::string& err
) {
    out = StatsQuery{};
    err.clear();
    if (req.m_dataArrays.size() != 1) {
        return false;
    }

    auto p_first = req.m_dataArrays.begin();
    const std::string& cid = p_first->first;
    const common12::AryIdent& ary_id = p_first->second;

    // Marker: exactly "STATS", or "STATS|..." A plain correlation id that merely
    // starts with the letters (e.g. "STATS_poro") is NOT a compute request.
    if (cid != "STATS" && cid.rfind("STATS|", 0) != 0) {
        return false;
    }

    out.cid = cid;
    out.uri = ary_id.m_uri;
    out.path = ary_id.m_pathInResource;

    if (cid == "STATS") {
        return true;
    }

    // "STATS|<lo>|<hi>" or "STATS|<lo>|<hi>|<bins>". Anything else is a
    // malformed compute request: report it, never fall back to plain metadata.
    const std::vector<std::string> tok = split_marker(cid);
    if (tok.size() != 3 && tok.size() != 4) {
        err = "malformed STATS marker '" + cid +
              "': expected STATS|<lo>|<hi>[|<bins>]";
        return true;
    }
    double lo = 0.0, hi = 0.0;
    if (!parse_finite_double(tok[1], lo) || !parse_finite_double(tok[2], hi)) {
        err = "malformed STATS marker '" + cid + "': bounds must be finite numbers";
        return true;
    }
    if (lo > hi) {
        err = "malformed STATS marker '" + cid + "': lo > hi";
        return true;
    }
    out.has_range = true;
    out.range_lo = lo;
    out.range_hi = hi;

    if (tok.size() == 4) {
        uint32_t bins = 0;
        if (!parse_bins(tok[3], kMaxStatsHistBins, bins)) {
            err = "malformed STATS marker '" + cid + "': bins must be in [1, " +
                  std::to_string(kMaxStatsHistBins) + "]";
            return true;
        }
        if (lo == hi) {
            err = "malformed STATS marker '" + cid + "': histogram needs lo < hi";
            return true;
        }
        out.has_histogram = true;
        out.hist_bins = bins;
    }
    return true;
}

GetDataArrayMetadata to_array_stats_req(
    const std::string& dataspace_uid,
    const std::string& path,
    bool has_range,
    double range_lo,
    double range_hi,
    uint32_t hist_bins
) {
    GetDataArrayMetadata req;

    common12::AryIdent ident;
    ident.m_uri = EmlUri::make_space(dataspace_uid).to_string();
    ident.m_pathInResource = path;

    std::string cid = "STATS";
    if (has_range) {
        char buf[112];
        if (hist_bins > 0) {
            std::snprintf(buf, sizeof(buf), "STATS|%.17g|%.17g|%u",
                          range_lo, range_hi, hist_bins);
        } else {
            std::snprintf(buf, sizeof(buf), "STATS|%.17g|%.17g", range_lo, range_hi);
        }
        cid = buf;
    }
    req.m_dataArrays.emplace(std::move(cid), std::move(ident));

    return req;
}

//============================================================================

namespace {

//! Parse a whole token as a non-negative integer (no sign, no garbage).
bool parse_index(const std::string& tok, uint64_t& v) {
    if (tok.empty() || tok[0] == '-' || tok[0] == '+') return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long u = std::strtoull(tok.c_str(), &end, 10);
    if (end == tok.c_str() || *end != '\0' || errno == ERANGE) return false;
    v = static_cast<uint64_t>(u);
    return true;
}

//! Parse the "<lo>|<hi>" pair shared by every spatial marker: both finite,
//! lo <= hi. On failure \p err names the marker.
bool parse_range(
    const std::string& cid, const std::string& lo_tok, const std::string& hi_tok,
    double& lo, double& hi, std::string& err
) {
    if (!parse_finite_double(lo_tok, lo) || !parse_finite_double(hi_tok, hi)) {
        err = "malformed marker '" + cid + "': bounds must be finite numbers";
        return false;
    }
    if (lo > hi) {
        err = "malformed marker '" + cid + "': lo > hi";
        return false;
    }
    return true;
}

} // namespace

bool is_array_reduce_req(
    const GetDataArrayMetadata& req, ReduceQuery& out, std::string& err
) {
    out = ReduceQuery{};
    err.clear();
    if (req.m_dataArrays.size() != 1) {
        return false;
    }

    auto p_first = req.m_dataArrays.begin();
    const std::string& cid = p_first->first;
    const common12::AryIdent& ary_id = p_first->second;

    if (cid.rfind("REDUCE|", 0) != 0) {
        return false;
    }

    out.cid = cid;
    out.uri = ary_id.m_uri;
    out.path = ary_id.m_pathInResource;

    // "REDUCE|<op>[|<k0>|<k1>[|<lo>|<hi>[|<dz>]]]" -> 2, 4, 6 or 7 tokens.
    const std::vector<std::string> tok = split_marker(cid);
    if (tok.size() != 2 && tok.size() != 4 && tok.size() != 6 && tok.size() != 7) {
        err = "malformed REDUCE marker '" + cid +
              "': expected REDUCE|<op>[|<k0>|<k1>[|<lo>|<hi>[|<dz>]]]";
        return true;
    }
    if (tok[1].empty()) {
        err = "malformed REDUCE marker '" + cid + "': empty operator";
        return true;
    }
    out.op = tok[1];

    if (tok.size() >= 4) {
        if (tok[2] == "-" && tok[3] == "-") {
            // explicit "no K-zone" placeholder keeps the later fields positional.
        } else {
            uint64_t k0 = 0, k1 = 0;
            if (!parse_index(tok[2], k0) || !parse_index(tok[3], k1)) {
                err = "malformed REDUCE marker '" + cid +
                      "': k0/k1 must be non-negative integers or '-'";
                return true;
            }
            if (k1 <= k0) {
                err = "malformed REDUCE marker '" + cid + "': k1 <= k0";
                return true;
            }
            out.has_k_range = true;
            out.k0 = k0;
            out.k1 = k1;
        }
    }
    if (tok.size() >= 6) {
        double lo = 0.0, hi = 0.0;
        if (!parse_range(cid, tok[4], tok[5], lo, hi, err)) {
            return true;
        }
        out.has_range = true;
        out.range_lo = lo;
        out.range_hi = hi;
    }
    if (tok.size() == 7) {
        double dz = 0.0;
        if (!parse_finite_double(tok[6], dz) || !(dz > 0.0)) {
            err = "malformed REDUCE marker '" + cid + "': dz must be finite and > 0";
            return true;
        }
        out.has_dz = true;
        out.cell_dz = dz;
    }
    return true;
}

GetDataArrayMetadata to_array_reduce_req(
    const std::string& dataspace_uid,
    const std::string& path,
    const std::string& op,
    bool has_k_range,
    uint64_t k0,
    uint64_t k1,
    bool has_range,
    double range_lo,
    double range_hi,
    bool has_dz,
    double cell_dz
) {
    GetDataArrayMetadata req;

    common12::AryIdent ident;
    ident.m_uri = EmlUri::make_space(dataspace_uid).to_string();
    ident.m_pathInResource = path;

    // Fields are positional: a range or dz implies a (possibly "-|-") K-zone
    // slot, and dz implies a range slot.
    std::string cid = "REDUCE|" + op;
    if (has_k_range || has_range || has_dz) {
        char buf[64];
        if (has_k_range) {
            std::snprintf(buf, sizeof(buf), "|%llu|%llu",
                          static_cast<unsigned long long>(k0),
                          static_cast<unsigned long long>(k1));
            cid += buf;
        } else {
            cid += "|-|-";
        }
    }
    if (has_range || has_dz) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "|%.17g|%.17g", range_lo, range_hi);
        cid += buf;
    }
    if (has_dz) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "|%.17g", cell_dz);
        cid += buf;
    }
    req.m_dataArrays.emplace(std::move(cid), std::move(ident));

    return req;
}

//============================================================================

bool is_array_where_req(
    const GetDataArrayMetadata& req, WhereQuery& out, std::string& err
) {
    out = WhereQuery{};
    err.clear();
    if (req.m_dataArrays.size() != 1) {
        return false;
    }

    auto p_first = req.m_dataArrays.begin();
    const std::string& cid = p_first->first;
    const common12::AryIdent& ary_id = p_first->second;

    if (cid.rfind("WHERE|", 0) != 0) {
        return false;
    }

    out.cid = cid;
    out.uri = ary_id.m_uri;
    out.path = ary_id.m_pathInResource;

    const std::vector<std::string> tok = split_marker(cid);
    if (tok.size() != 3) {
        err = "malformed WHERE marker '" + cid + "': expected WHERE|<lo>|<hi>";
        return true;
    }
    double lo = 0.0, hi = 0.0;
    if (!parse_range(cid, tok[1], tok[2], lo, hi, err)) {
        return true;
    }
    out.range_lo = lo;
    out.range_hi = hi;
    return true;
}

GetDataArrayMetadata to_array_where_req(
    const std::string& dataspace_uid,
    const std::string& path,
    double range_lo,
    double range_hi
) {
    GetDataArrayMetadata req;

    common12::AryIdent ident;
    ident.m_uri = EmlUri::make_space(dataspace_uid).to_string();
    ident.m_pathInResource = path;

    char buf[112];
    std::snprintf(buf, sizeof(buf), "WHERE|%.17g|%.17g", range_lo, range_hi);
    req.m_dataArrays.emplace(std::string(buf), std::move(ident));

    return req;
}

//============================================================================

bool is_array_multiwhere_req(
    const GetDataArrayMetadata& req, MultiWhereQuery& out, std::string& err
) {
    out = MultiWhereQuery{};
    err.clear();

    // Any WHEREN-keyed entry makes this a WHEREN request; from then on every
    // entry must be a well-formed WHEREN key (a mixed batch is malformed, not
    // a plain metadata request).
    bool any = false;
    for (const auto& kv : req.m_dataArrays) {
        if (kv.first.rfind("WHEREN|", 0) == 0) {
            any = true;
            break;
        }
    }
    if (!any) {
        return false;
    }

    if (req.m_dataArrays.size() > kMaxMultiWhereEntries) {
        err = std::to_string(req.m_dataArrays.size()) +
              " entries, at most " + std::to_string(kMaxMultiWhereEntries) + " allowed";
        return true;
    }

    for (const auto& kv : req.m_dataArrays) {
        const std::string& cid = kv.first;
        const common12::AryIdent& ary_id = kv.second;
        if (cid.rfind("WHEREN|", 0) != 0) {
            err = "entry '" + cid + "' is not a WHEREN|<idx>|<lo>|<hi> key";
            return true;
        }
        const std::vector<std::string> tok = split_marker(cid);
        uint64_t idx = 0;
        if (tok.size() != 4 || !parse_index(tok[1], idx)) {
            err = "malformed WHEREN marker '" + cid + "': expected WHEREN|<idx>|<lo>|<hi>";
            return true;
        }
        double lo = 0.0, hi = 0.0;
        if (!parse_range(cid, tok[2], tok[3], lo, hi, err)) {
            return true;
        }
        MultiWhereEntry e;
        e.cid = cid;
        e.uri = ary_id.m_uri;
        e.path = ary_id.m_pathInResource;
        e.range_lo = lo;
        e.range_hi = hi;
        out.entries.push_back(std::move(e));
    }

    out.cid = out.entries.front().cid;
    return true;
}

GetDataArrayMetadata to_array_multiwhere_req(
    const std::string& dataspace_uid,
    const std::vector<MultiWhereEntry>& entries
) {
    GetDataArrayMetadata req;

    const std::string space_uri = EmlUri::make_space(dataspace_uid).to_string();
    int idx = 0;
    for (const auto& e : entries) {
        common12::AryIdent ident;
        ident.m_uri = e.uri.empty() ? space_uri : e.uri;
        ident.m_pathInResource = e.path;

        char buf[128];
        std::snprintf(buf, sizeof(buf), "WHEREN|%03d|%.17g|%.17g", idx, e.range_lo, e.range_hi);
        req.m_dataArrays.emplace(std::string(buf), std::move(ident));
        ++idx;
    }

    return req;
}

//============================================================================

std::string to_json_meta_key(const common12::AryIdent& ident) {
    return oes::common::to_json(ident, false);
}

common12::AryIdent from_json_meta_key(std::string& ident_json) {
    common12::AryIdent ident;
    oes::common::from_json(ident, ident_json);
    return ident;
}

common12::AryInfoVec move_to_info_vec(AryMetaMap& map) {
    common12::AryInfoVec vec;
    vec.reserve(map.size());
    for (auto& entry : map) {
        // must copy, from_json_meta_key() requires LValue
        std::string ident_json = entry.first;
        common12::AryIdent ident = from_json_meta_key(ident_json);
        common12::AryMeta& meta = entry.second;
        vec.emplace_back(common12::AryInfo{ std::move(ident), std::move(meta) });
    }
    return vec;
}

//============================================================================

} // namespace namespace oes::eml::etp12::DataArrayExt
