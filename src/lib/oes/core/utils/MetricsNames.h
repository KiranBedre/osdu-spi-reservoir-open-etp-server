// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2022 EPAM Systems
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

namespace oes::core::metrics {

// Store protocol metrics
inline constexpr const char* STORE_OPS_COUNTER_GET_SUCCESS = "store_ops_counter_get_success";
inline constexpr const char* STORE_OPS_COUNTER_GET_ERROR = "store_ops_counter_get_error";
inline constexpr const char* STORE_OPS_COUNTER_PUT_SUCCESS = "store_ops_counter_put_success";
inline constexpr const char* STORE_OPS_COUNTER_PUT_ERROR = "store_ops_counter_put_error";
inline constexpr const char* STORE_OPS_COUNTER_DELETE_SUCCESS = "store_ops_counter_delete_success";
inline constexpr const char* STORE_OPS_COUNTER_DELETE_ERROR = "store_ops_counter_delete_error";
inline constexpr const char* STORE_LATENCY_GET = "store_latency_get";
inline constexpr const char* STORE_LATENCY_PUT = "store_latency_put";
inline constexpr const char* STORE_LATENCY_DELETE = "store_latency_delete";      

// Dataspace protocol metrics
inline constexpr const char* DATASPACE_OPS_COUNTER_GET_SUCCESS = "dataspace_ops_counter_get_success";
inline constexpr const char* DATASPACE_OPS_COUNTER_GET_ERROR = "dataspace_ops_counter_get_error";
inline constexpr const char* DATASPACE_OPS_COUNTER_PUT_SUCCESS = "dataspace_ops_counter_put_success";
inline constexpr const char* DATASPACE_OPS_COUNTER_PUT_ERROR = "dataspace_ops_counter_put_error";
inline constexpr const char* DATASPACE_OPS_COUNTER_DELETE_SUCCESS = "dataspace_ops_counter_delete_success";
inline constexpr const char* DATASPACE_OPS_COUNTER_DELETE_ERROR = "dataspace_ops_counter_delete_error";
inline constexpr const char* DATASPACE_LATENCY_GET = "dataspace_latency_get";    
inline constexpr const char* DATASPACE_LATENCY_PUT = "dataspace_latency_put";    
inline constexpr const char* DATASPACE_LATENCY_DELETE = "dataspace_latency_delete";

// Array protocol metrics
inline constexpr const char* ARRAY_OPS_COUNTER_GET_ARRAYS_SUCCESS = "array_ops_counter_get_arrays_success";
inline constexpr const char* ARRAY_OPS_COUNTER_GET_ARRAYS_ERROR = "array_ops_counter_get_arrays_error";
inline constexpr const char* ARRAY_OPS_COUNTER_GET_SUBARRAYS_SUCCESS = "array_ops_counter_get_subarrays_success";
inline constexpr const char* ARRAY_OPS_COUNTER_GET_SUBARRAYS_ERROR = "array_ops_counter_get_subarrays_error";
inline constexpr const char* ARRAY_OPS_COUNTER_PUT_ARRAYS_SUCCESS = "array_ops_counter_put_arrays_success";
inline constexpr const char* ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR = "array_ops_counter_put_arrays_error";
inline constexpr const char* ARRAY_OPS_COUNTER_PUT_SUBARRAYS_SUCCESS = "array_ops_counter_put_subarrays_success";
inline constexpr const char* ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR = "array_ops_counter_put_subarrays_error";
inline constexpr const char* ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_SUCCESS = "array_ops_counter_put_uninitialized_arrays_success";
inline constexpr const char* ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR = "array_ops_counter_put_uninitialized_arrays_error";
inline constexpr const char* ARRAY_LATENCY_GET_ARRAYS = "array_latency_get_arrays";
inline constexpr const char* ARRAY_LATENCY_GET_SUBARRAYS = "array_latency_get_subarrays";
inline constexpr const char* ARRAY_LATENCY_PUT_ARRAYS = "array_latency_put_arrays";
inline constexpr const char* ARRAY_LATENCY_PUT_SUBARRAYS = "array_latency_put_subarrays";
inline constexpr const char* ARRAY_LATENCY_PUT_UNINITIALIZED_ARRAYS = "array_latency_put_uninitialized_arrays";

// Discovery protocol metrics
inline constexpr const char* DISCOVERY_OPS_COUNTER_GET_RESOURCES_SUCCESS = "discovery_ops_counter_get_resources_success";
inline constexpr const char* DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR = "discovery_ops_counter_get_resources_error";
inline constexpr const char* DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_SUCCESS = "discovery_ops_counter_get_deleted_resources_success";
inline constexpr const char* DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR = "discovery_ops_counter_get_deleted_resources_error";
inline constexpr const char* DISCOVERY_LATENCY_GET_RESOURCES = "discovery_latency_get_resources";
inline constexpr const char* DISCOVERY_LATENCY_GET_DELETED_RESOURCES = "discovery_latency_get_deleted_resources";

// Transaction protocol metrics
inline constexpr const char* TRANSACTION_OPS_COUNTER_START_SUCCESS = "transaction_ops_counter_start_success";
inline constexpr const char* TRANSACTION_OPS_COUNTER_START_ERROR = "transaction_ops_counter_start_error";
inline constexpr const char* TRANSACTION_OPS_COUNTER_COMMIT_SUCCESS = "transaction_ops_counter_commit_success";
inline constexpr const char* TRANSACTION_OPS_COUNTER_COMMIT_ERROR = "transaction_ops_counter_commit_error";
inline constexpr const char* TRANSACTION_OPS_COUNTER_ROLLBACK_SUCCESS = "transaction_ops_counter_rollback_success";
inline constexpr const char* TRANSACTION_OPS_COUNTER_ROLLBACK_ERROR = "transaction_ops_counter_rollback_error";
inline constexpr const char* TRANSACTION_LATENCY_START = "transaction_latency_start";
inline constexpr const char* TRANSACTION_LATENCY_COMMIT = "transaction_latency_commit";
inline constexpr const char* TRANSACTION_LATENCY_ROLLBACK = "transaction_latency_rollback";

// ETP message payload-size histograms (on-the-wire bytes)
inline constexpr const char* ETP_REQUEST_BYTES = "etp_request_bytes";
inline constexpr const char* ETP_RESPONSE_BYTES = "etp_response_bytes";

} // namespace oes::core::metrics
