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

#include <map>

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

OES_EML_EXPORT
std::string to_json_meta_key(const common12::AryIdent& ident);

OES_EML_EXPORT
common12::AryIdent from_json_meta_key(std::string& ident_json);

OES_EML_EXPORT
common12::AryInfoVec move_to_info_vec(AryMetaMap& map);

} // namespace oes::eml::etp12::DataArrayExt

#endif
