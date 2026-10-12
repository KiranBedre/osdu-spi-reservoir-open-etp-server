// ============================================================================
// Copyright 2024 Emerson Paradigm Holding LLC. All rights reserved.
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
#ifndef OES_COMMON_EPC_ACTIVITY_UTILS
#define OES_COMMON_EPC_ACTIVITY_UTILS

#include "../export.h"

#include <set>
#include <string>
#include <utility>
#include <unordered_map>

namespace oes::common::epc {

struct OES_COMMON_EXPORT AnalyzedObject {
public:
    AnalyzedObject() = delete;
    AnalyzedObject(
        const std::string& uuid,
        const std::string& ml,
        const std::string& version,
        const std::string& type,
        const std::string& title
    );
    ~AnalyzedObject() = default;

    std::string uuid_;
    std::string ml_;
    std::string version_;
    std::string type_;
    std::string title_;
};

OES_COMMON_EXPORT
std::pair<std::string, std::string> get_xml_header_and_footer(
    const std::string& xml_type,
    const std::string& obj_type,
    const std::string& obj_uuid
);

OES_COMMON_EXPORT
std::string get_import_activity_Citation(
    const std::string& title,
    const std::string& originator,
    const std::string& creation_dt,
    const std::string& format
);

OES_COMMON_EXPORT
std::string get_import_activity_ADescriptor( // dataobject reference
    const std::string& desc_type, // eml20:DataObjectReference
    const std::string& type, // obj_ActivityTemplate
    const std::string& title,
    const std::string& uuid,
    const std::string& authority
);

OES_COMMON_EXPORT
std::string get_import_activity_Parameter( // dataobject parameter
    const std::string& parameter_type, // resqml20:DataObjectParameter
    const std::string& parameter_title,
    const std::string& obj_ml,
    const std::string& obj_version,
    const std::string& obj_type,
    const std::string& obj_title,
    const std::string& obj_uuid,
    const std::string& obj_authority,
    const size_t index
);

OES_COMMON_EXPORT
std::string get_import_activity_Extradata(
    const std::string& name, // activityStatus
    const std::string& status // Done
);

// activity template
OES_COMMON_EXPORT
std::string get_activity_template_data(const std::string& user_id, const std::string& dt_creation);

OES_COMMON_EXPORT
std::string extract_string_between(const std::string& xml, const std::string& str_beg, const std::string& str_end);

OES_COMMON_EXPORT
std::set<std::string> extract_activities_template_titles(const std::string& xml, const std::string& prefix);
} // namespace oes::common::epc

#endif // OES_COMMON_EPC_ACTIVITY_UTILS