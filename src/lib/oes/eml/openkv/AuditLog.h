// ============================================================================
//  Copyright ┬® Microsoft Corporation
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
// =============================================================================

#pragma once

#include <string>

namespace oes::providers::core {
class ProvidersConfig;
}

namespace oes::eml::openkv {

// Forward declarations
class OpenKVSession;

/**
 * @brief Constants for audit logging
 * 
 * Centralized constants for operation names, result types, and action types
 * used across all ETP protocol audit logging calls.
 */
namespace AuditLogConstants {
    // Operation names
    inline constexpr const char* OP_PUT_DATA_OBJECTS = "PutDataObjects";
    inline constexpr const char* OP_DELETE_DATA_OBJECTS = "DeleteDataObjects";
    inline constexpr const char* OP_PUT_DATA_ARRAYS = "PutDataArrays";
    inline constexpr const char* OP_PUT_DATA_SUBARRAYS = "PutDataSubarrays";
    inline constexpr const char* OP_PUT_UNINITIALIZED_DATA_ARRAYS = "PutUninitializedDataArrays";
    inline constexpr const char* OP_PUT_DATA_SPACES = "PutDataSpaces";
    inline constexpr const char* OP_DELETE_DATA_SPACES = "DeleteDataSpaces";
    inline constexpr const char* OP_UPDATE_DATA_SPACES = "UpdateDataSpaces";
    inline constexpr const char* OP_COMMIT_TRANSACTION = "CommitTransaction";
    inline constexpr const char* OP_ROLLBACK_TRANSACTION = "RollbackTransaction";
    inline constexpr const char* OP_COPY_DATA_OBJECTS_BY_VALUE = "CopyDataObjectsByValue";

    // Result types
    inline constexpr const char* RESULT_SUCCESS = "SUCCESS";
    inline constexpr const char* RESULT_FAILURE = "FAILURE";

    // Action types
    inline constexpr const char* ACTION_CREATE = "CREATE";
    inline constexpr const char* ACTION_DELETE = "DELETE";
    inline constexpr const char* ACTION_UPDATE = "UPDATE";
    inline constexpr const char* ACTION_EXECUTE = "EXECUTE";
}

/**
 * @brief Caller information extracted from JWT token
 */
struct CallerInfo {
    std::string id;    // Caller ID (oid or appid)
    std::string type;  // Caller type: "OBJECT_ID" or "APPLICATION_ID"
};

/**
 * @brief Audit log utility for ETP operations
 * 
 * Provides a single log() method that handles JWT parsing, session extraction,
 * JSON formatting, and writing audit logs for all ETP operations.
 */
class AuditLog {
public:
    /**
     * @brief Log an audit entry for an ETP operation
     * 
     * This method extracts user information from the session (including JWT token parsing),
     * builds the complete audit entry with all required fields, formats it as JSON,
     * and writes it to stdout with the "ifxauditappl:" prefix.
     * 
     * @param session The current ETP session (can be nullptr)
     * @param operation_name The ETP operation name (e.g., "PutDataObjects", "DeleteDataObjects")
     * @param result_type Result of the operation
     * @param action The action performed
     * @param resource_uri URI of the target resource
     * @param message Human-readable message describing the operation result
     */
    static void log(
        const oes::providers::core::ProvidersConfig& providers_config,
        const OpenKVSession* session,
        const std::string& operation_name,
        const std::string& result_type,
        const std::string& action,
        const std::string& resource_uri,
        const std::string& message
    );

private:
    static constexpr const char* AUDIT_PREFIX = "ifxauditappl:";
    
    /**
     * @brief Extract caller identity from JWT token
     * 
     * Parses the JWT token and extracts caller identity
     * @param token JWT token string
     * @return CallerInfo with id and type (empty id if not found)
     */
    static CallerInfo extractCallerIdFromToken(const std::string& token);
    
    /**
     * @brief Get action ID for operation
     * 
     * Maps ETP operation names to action IDs
     * 
     * @param operation_name The ETP operation name
     * @return Action ID string (e.g., "ETP001")
     */
    static std::string getActionId(const std::string& operation_name);
};

} // namespace oes::eml::openkv
