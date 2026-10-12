// ============================================================================
//  Copyright © Microsoft Corporation
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
// ============================================================================

#include "AuditLog.h"
#include "Session.h"
#include "../etp12/Server.h"

#include <oes/common/auth/AuthN.h>
#include <oes/core/utils/Logger.h>
#include <oes/providers/core/ProvidersConfig.h>
#include <oes/core/utils/config_enter_3rdparty.h>
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <chrono>
#include <ctime>
#include <iostream>

namespace oes::eml::openkv {

// Extract caller identity from JWT token
CallerInfo AuditLog::extractCallerIdFromToken(const std::string& token) {
    CallerInfo info;
    info.id = "";
    info.type = "";

    if (token.empty() || token == "system") {
        return info;
    }

    try {
        // JWT structure: header.payload.signature
        size_t first_dot = token.find('.');
        size_t second_dot = token.find('.', first_dot + 1);

        if (first_dot == std::string::npos || second_dot == std::string::npos) {
            OES_DEBUG("Invalid JWT structure for given token");
            return info;
        }

        // Extract payload
        std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);

        // JWT uses base64url encoding, decode it
        std::string payload_json = oes::common::auth::base64urlDecode(payload_b64);

        // Parse JSON payload
        auto json_payload = nlohmann::json::parse(payload_json, nullptr, false);
        if (json_payload.is_discarded()) {
            OES_DEBUG("Failed to parse JWT payload as JSON");
            return info;
        }

        // Extracting claims from payload
        std::string upn = json_payload.value("upn", "");
        std::string unique_name = json_payload.value("unique_name", "");
        std::string oid = json_payload.value("oid", "");
        std::string appid = json_payload.value("appid", "");  // v1.0 token
        if (appid.empty()) {
            appid = json_payload.value("azp", "");  // v2.0 token fallback
        }

        // Customer/User token: upn exists with oid
        if (!upn.empty() && !oid.empty()) {
            OES_DEBUG("Customer call (upn) - Caller ID set to oid: {}", oid);
            info.id = oid;
            info.type = "OBJECT_ID";
            return info;
        }

        // Customer/User token: unique_name exists with oid
        if (!unique_name.empty() && !oid.empty()) {
            OES_DEBUG("Customer call (unique_name) - Caller ID set to oid: {}", oid);
            info.id = oid;
            info.type = "OBJECT_ID";
            return info;
        }

        // Service token: appid exists (no upn/unique_name)
        if (!appid.empty()) {
            OES_DEBUG("Internal service call - Caller ID set to appid: {}", appid);
            info.id = appid;
            info.type = "APPLICATION_ID";
            return info;
        }

        OES_DEBUG("No valid caller ID found in JWT token");
        return info;

    } catch (const std::exception& e) {
        OES_DEBUG("Error parsing JWT token: {}", e.what());
        return info;
    } catch (...) {
        OES_DEBUG("Unknown error parsing JWT token");
        return info;
    }
}

// Get action ID for operation
std::string AuditLog::getActionId(const std::string& operation_name) {
    if (operation_name == AuditLogConstants::OP_PUT_DATA_OBJECTS) return "ETP001";
    if (operation_name == AuditLogConstants::OP_DELETE_DATA_OBJECTS) return "ETP002";
    if (operation_name == AuditLogConstants::OP_PUT_DATA_ARRAYS) return "ETP003";
    if (operation_name == AuditLogConstants::OP_PUT_DATA_SUBARRAYS) return "ETP004";
    if (operation_name == AuditLogConstants::OP_PUT_UNINITIALIZED_DATA_ARRAYS) return "ETP005";
    if (operation_name == AuditLogConstants::OP_PUT_DATA_SPACES) return "ETP006";
    if (operation_name == AuditLogConstants::OP_DELETE_DATA_SPACES) return "ETP007";
    if (operation_name == AuditLogConstants::OP_UPDATE_DATA_SPACES) return "ETP008";
    if (operation_name == AuditLogConstants::OP_COMMIT_TRANSACTION) return "ETP009";
    if (operation_name == AuditLogConstants::OP_ROLLBACK_TRANSACTION) return "ETP010";
    return "ETP999";  // Unknown
}

// Main log method
void AuditLog::log(
    const oes::providers::core::ProvidersConfig& providers_config,
    const OpenKVSession* session,
    const std::string& operation_name,
    const std::string& result_type,
    const std::string& action,
    const std::string& resource_uri,
    const std::string& message
) {
    // Early exit if audit logging is disabled
    if (!providers_config.auditLogsEnabled()) {
        return;
    }

    // Build JSON audit log entry
    nlohmann::json j;

    // Core operation fields
    j["OperationName"] = operation_name;
    j["ResultType"] = result_type;
    j["action"] = action;
    j["actionId"] = getActionId(operation_name);
    j["message"] = message;

    // Audit categories
    j["AuditCategories"] = nlohmann::json::array({"RESOURCE_MANAGEMENT"});

    // CallerIdentities - extract from session JWT token
    j["CallerIdentities"] = nlohmann::json::array();

    if (session && session->user_) {
        std::string token = session->user_->header_;
        CallerInfo caller_info = extractCallerIdFromToken(token);

        if (!caller_info.id.empty()) {
            nlohmann::json caller;
            caller["CallerIdentityType"] = caller_info.type;
            caller["CallerIdentityValue"] = caller_info.id;
            j["CallerIdentities"].push_back(caller);
        }
    }

    // TargetResources
    j["TargetResources"] = nlohmann::json::array();
    nlohmann::json target;
    target["TargetResourceType"] = "resource";
    target["TargetResourceName"] = operation_name + "-" + resource_uri;
    j["TargetResources"].push_back(target);

    // Request tracking
    std::string request_id = "unknown";
    if (session) {
        request_id = session->handshake_.session_id_;
    }
    j["requestId"] = request_id;

    // Service identification
    j["ServiceName"] = "ReservoirOpenETPServer.audit";

    // Timestamp (ISO 8601 UTC)
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t time_t = std::chrono::system_clock::to_time_t(now);
    std::tm gmt;
#ifdef _WIN32
    gmtime_s(&gmt, &time_t);
#else
    gmtime_r(&time_t, &gmt);
#endif
    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &gmt);
    j["env_time"] = fmt::format("{}.{:03d}+00:00", time_buf, ms.count());

    // Audit envelope metadata
    j["env_ver"] = "2.1";
    j["env_name"] = "#Ifx.AuditSchema";
    j["env_cv"] = "##00000000-0000-0000-0000-000000000000_00000000-0000-0000-0000-000000000000_00000000-0000-0000-0000-000000000000";
    j["AuditType"] = "APPLICATION";

    // Data partition
    std::string partition_id = "unknown";
    if (session) {
        partition_id = session->handshake_.data_partition_id_.empty() ? "unknown" : session->handshake_.data_partition_id_;
    }
    j["data-partition-id"] = partition_id;

    // Write to stdout with AUDIT_PREFIX
    std::cout << AUDIT_PREFIX << " " << j.dump() << std::endl;
}

} // namespace oes::eml::openkv
