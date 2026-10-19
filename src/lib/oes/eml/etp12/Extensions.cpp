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
