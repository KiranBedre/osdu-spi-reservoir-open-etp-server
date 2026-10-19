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

#ifndef OES_EML_OPENKV_SPACEDDL_H
#define OES_EML_OPENKV_SPACEDDL_H

#include "../fwd.h"
#include "../export.h"

#include <string>
#include <vector>

namespace oes::eml::openkv {

//============================================================================

struct OES_EML_EXPORT SchemaVersionInfo {
    int version;
    const char* created;
    const char* creator;
    const char* description;
};

//============================================================================

OES_EML_EXPORT
void new_empty_eml_db(
    const oes::postgresql::Connection_ptr& eml_db
);

OES_EML_EXPORT
const std::vector<SchemaVersionInfo>& space_schema_versions(int& magic_cookie);

OES_EML_EXPORT
void add_deleted_resources_table(const Connection_ptr& conn, const std::string &schema);

OES_EML_EXPORT
void remove_deleted_resources_table(const Connection_ptr& conn, const std::string &schema);

OES_EML_EXPORT
void add_import_by_ref_triggers(const Connection_ptr conn, const std::string &schema);

OES_EML_EXPORT
void add_relations_view(const Connection_ptr& conn, const std::string &schema);

OES_EML_EXPORT
void update_resources_view(const Connection_ptr& conn, const std::string &schema);

//============================================================================

OES_EML_EXPORT
void new_eml_db_prolog(oes::postgresql::Connection_ptr& db, bool bulk_import);

OES_EML_EXPORT
void new_eml_db_epilog(oes::postgresql::Connection_ptr& db, bool vacuum, bool analyze);

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_SPACEDDL_H
