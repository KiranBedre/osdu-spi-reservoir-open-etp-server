// ============================================================================
// Copyright 2026 Microsoft. All rights reserved.
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
#pragma once

// Centralized, client-facing error message strings shared across the codebase.
// Kept as constants so they can be reused and refactored consistently over time.
// New error strings should be added here so the catalog grows in one place.
namespace oes::common::error_messages {

// Register / create dataspace
inline constexpr const char* NULL_DATASPACE_REFERENCE =
    "internal error: null dataspace reference";
inline constexpr const char* NOT_MEMBER_OWNER_GROUP =
    "you are not a member of the required owner group for this dataspace";
inline constexpr const char* REQUIRED_FIELDS_EMPTY =
    "one or more required fields are empty. Provide all of: viewers, owners, legaltags, otherRelevantDataCountries";
inline constexpr const char* DATASPACE_EXISTS_NO_ACCESS =
    "a dataspace with this name already exists and you do not have access to it";
inline constexpr const char* REPLACE_EXISTING_RECORD_FAILED =
    "failed to replace the existing dataspace record";
inline constexpr const char* STORAGE_CREATE_RECORD_FAILED =
    "the storage service could not create the dataspace record";
inline constexpr const char* OWNERS_FIELD_EMPTY =
    "the 'owners' field is empty";

// Unregister / delete dataspace
inline constexpr const char* STORAGE_DELETE_RECORD_FAILED =
    "the storage service could not delete the dataspace record";

} // namespace oes::common::error_messages
