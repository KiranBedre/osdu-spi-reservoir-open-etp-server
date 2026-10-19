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

#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsNames.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include "prometheus/client_metric.h"
#include "prometheus/counter.h"
#include "prometheus/family.h"
#include "prometheus/registry.h"
#include <prometheus/summary.h>
#include <prometheus/histogram.h>
#include <oes/core/utils/config_leave_3rdparty.h>


namespace oes::eml::etp12 {
// Declare metrics
void setUpPrometheus() {
    auto registry = oes::core::Metrics::instance().getRegistry();

    // Declare counters
    auto& http_requests_counter = prometheus::BuildCounter().Name("http_requests_total")
                                                            .Help("Number of HTTP requests")
                                                            .Register(*registry);
    auto& ws_messages_counter = prometheus::BuildCounter().Name("ws_messages_total")
                                                        .Help("Number of WS messages")
                                                        .Register(*registry);
    auto& etp_messages_counter = prometheus::BuildCounter().Name("etp_messages_total")
                                                           .Help("Number of ETP messages")
                                                           .Register(*registry);
    auto& etp_ops_counter = prometheus::BuildCounter().Name("etp_operations_total")
                                                      .Help("Total number of ETP protocol operations")
                                                      .Register(*registry);

    auto& health_ep_counter = http_requests_counter.Add({{"endpoint", "health"}});
    auto& metrics_ep_counter = http_requests_counter.Add({{"endpoint", "metrics"}});
    auto& any_message_counter = ws_messages_counter.Add({{"ws", "on_validate"}});
    auto& any_error_counter = ws_messages_counter.Add({{"ws", "on_fail"}});
    auto& ping_pong_counter = etp_messages_counter.Add({{"message", "ping-pong"}});

    // Store protocol counters
    auto& store_ops_counter_get_success = etp_ops_counter.Add({{"protocol", "store"}, {"operation", "get"}, {"status", "success"}});
    auto& store_ops_counter_get_error = etp_ops_counter.Add({{"protocol", "store"}, {"operation", "get"}, {"status", "error"}});
    auto& store_ops_counter_put_success = etp_ops_counter.Add({{"protocol", "store"}, {"operation", "put"}, {"status", "success"}});
    auto& store_ops_counter_put_error = etp_ops_counter.Add({{"protocol", "store"}, {"operation", "put"}, {"status", "error"}});
    auto& store_ops_counter_delete_success = etp_ops_counter.Add({{"protocol", "store"}, {"operation", "delete"}, {"status", "success"}});
    auto& store_ops_counter_delete_error = etp_ops_counter.Add({{"protocol", "store"}, {"operation", "delete"}, {"status", "error"}});

    // Dataspace protocol counters
    auto& dataspace_ops_counter_get_success = etp_ops_counter.Add({{"protocol", "dataspace"}, {"operation", "get"}, {"status", "success"}});
    auto& dataspace_ops_counter_get_error = etp_ops_counter.Add({{"protocol", "dataspace"}, {"operation", "get"}, {"status", "error"}});
    auto& dataspace_ops_counter_put_success = etp_ops_counter.Add({{"protocol", "dataspace"}, {"operation", "put"}, {"status", "success"}});
    auto& dataspace_ops_counter_put_error = etp_ops_counter.Add({{"protocol", "dataspace"}, {"operation", "put"}, {"status", "error"}});
    auto& dataspace_ops_counter_delete_success = etp_ops_counter.Add({{"protocol", "dataspace"}, {"operation", "delete"}, {"status", "success"}});
    auto& dataspace_ops_counter_delete_error = etp_ops_counter.Add({{"protocol", "dataspace"}, {"operation", "delete"}, {"status", "error"}});

    // Array protocol counters
    auto& array_ops_counter_get_arrays_success = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "get_arrays"}, {"status", "success"}});
    auto& array_ops_counter_get_arrays_error = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "get_arrays"}, {"status", "error"}});
    auto& array_ops_counter_get_subarrays_success = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "get_subarrays"}, {"status", "success"}});
    auto& array_ops_counter_get_subarrays_error = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "get_subarrays"}, {"status", "error"}});
    auto& array_ops_counter_put_arrays_success = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "put_arrays"}, {"status", "success"}});
    auto& array_ops_counter_put_arrays_error = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "put_arrays"}, {"status", "error"}});
    auto& array_ops_counter_put_subarrays_success = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "put_subarrays"}, {"status", "success"}});
    auto& array_ops_counter_put_subarrays_error = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "put_subarrays"}, {"status", "error"}});
    auto& array_ops_counter_put_uninitialized_arrays_success = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "put_uninitialized_arrays"}, {"status", "success"}});
    auto& array_ops_counter_put_uninitialized_arrays_error = etp_ops_counter.Add({{"protocol", "array"}, {"operation", "put_uninitialized_arrays"}, {"status", "error"}});

    // Discovery protocol counters
    auto& discovery_ops_counter_get_resources_success = etp_ops_counter.Add({{"protocol", "discovery"}, {"operation", "get_resources"}, {"status", "success"}});
    auto& discovery_ops_counter_get_resources_error = etp_ops_counter.Add({{"protocol", "discovery"}, {"operation", "get_resources"}, {"status", "error"}});
    auto& discovery_ops_counter_get_deleted_resources_success = etp_ops_counter.Add({{"protocol", "discovery"}, {"operation", "get_deleted_resources"}, {"status", "success"}});
    auto& discovery_ops_counter_get_deleted_resources_error = etp_ops_counter.Add({{"protocol", "discovery"}, {"operation", "get_deleted_resources"}, {"status", "error"}});

    // Transaction protocol counters
    auto& transaction_ops_counter_start_success = etp_ops_counter.Add({{"protocol", "transaction"}, {"operation", "start"}, {"status", "success"}});
    auto& transaction_ops_counter_start_error = etp_ops_counter.Add({{"protocol", "transaction"}, {"operation", "start"}, {"status", "error"}});
    auto& transaction_ops_counter_commit_success = etp_ops_counter.Add({{"protocol", "transaction"}, {"operation", "commit"}, {"status", "success"}});
    auto& transaction_ops_counter_commit_error = etp_ops_counter.Add({{"protocol", "transaction"}, {"operation", "commit"}, {"status", "error"}});
    auto& transaction_ops_counter_rollback_success = etp_ops_counter.Add({{"protocol", "transaction"}, {"operation", "rollback"}, {"status", "success"}});
    auto& transaction_ops_counter_rollback_error = etp_ops_counter.Add({{"protocol", "transaction"}, {"operation", "rollback"}, {"status", "error"}});

    oes::core::Metrics::instance().setCounter("health_ep_counter", health_ep_counter);
    oes::core::Metrics::instance().setCounter("metrics_ep_counter", metrics_ep_counter);
    oes::core::Metrics::instance().setCounter("any_message_counter", any_message_counter);
    oes::core::Metrics::instance().setCounter("any_error_counter", any_error_counter);
    oes::core::Metrics::instance().setCounter("ping_pong_counter", ping_pong_counter);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::STORE_OPS_COUNTER_GET_SUCCESS, store_ops_counter_get_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::STORE_OPS_COUNTER_GET_ERROR, store_ops_counter_get_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::STORE_OPS_COUNTER_PUT_SUCCESS, store_ops_counter_put_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::STORE_OPS_COUNTER_PUT_ERROR, store_ops_counter_put_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::STORE_OPS_COUNTER_DELETE_SUCCESS, store_ops_counter_delete_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::STORE_OPS_COUNTER_DELETE_ERROR, store_ops_counter_delete_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_SUCCESS, dataspace_ops_counter_get_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DATASPACE_OPS_COUNTER_GET_ERROR, dataspace_ops_counter_get_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_SUCCESS, dataspace_ops_counter_put_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DATASPACE_OPS_COUNTER_PUT_ERROR, dataspace_ops_counter_put_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_SUCCESS, dataspace_ops_counter_delete_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DATASPACE_OPS_COUNTER_DELETE_ERROR, dataspace_ops_counter_delete_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_GET_ARRAYS_SUCCESS, array_ops_counter_get_arrays_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_GET_ARRAYS_ERROR, array_ops_counter_get_arrays_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_GET_SUBARRAYS_SUCCESS, array_ops_counter_get_subarrays_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_GET_SUBARRAYS_ERROR, array_ops_counter_get_subarrays_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_SUCCESS, array_ops_counter_put_arrays_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_ARRAYS_ERROR, array_ops_counter_put_arrays_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_SUCCESS, array_ops_counter_put_subarrays_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_SUBARRAYS_ERROR, array_ops_counter_put_subarrays_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_SUCCESS, array_ops_counter_put_uninitialized_arrays_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::ARRAY_OPS_COUNTER_PUT_UNINITIALIZED_ARRAYS_ERROR, array_ops_counter_put_uninitialized_arrays_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_SUCCESS, discovery_ops_counter_get_resources_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_RESOURCES_ERROR, discovery_ops_counter_get_resources_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_SUCCESS, discovery_ops_counter_get_deleted_resources_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::DISCOVERY_OPS_COUNTER_GET_DELETED_RESOURCES_ERROR, discovery_ops_counter_get_deleted_resources_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_SUCCESS, transaction_ops_counter_start_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR, transaction_ops_counter_start_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_SUCCESS, transaction_ops_counter_commit_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR, transaction_ops_counter_commit_error);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::TRANSACTION_OPS_COUNTER_ROLLBACK_SUCCESS, transaction_ops_counter_rollback_success);
    oes::core::Metrics::instance().setCounter(oes::core::metrics::TRANSACTION_OPS_COUNTER_ROLLBACK_ERROR, transaction_ops_counter_rollback_error);

    // Declare summaries
    auto& message_latencies_summary = prometheus::BuildSummary().Name("ws_message_latencies")
                                                                .Help("Latencies of WS messages")
                                                                .Register(*registry);

    auto& ws_message_latencies = message_latencies_summary.Add(
        {}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});

    oes::core::Metrics::instance().setSummary("ws_message_latencies", ws_message_latencies);

    // Consolidated ETP operation latency summary
    auto& etp_latency_summary = prometheus::BuildSummary().Name("etp_operation_latency")
                                                          .Help("ETP protocol operation latency in milliseconds")
                                                          .Register(*registry);

    // Store protocol latencies
    auto& store_latency_get = etp_latency_summary.Add(
        {{"protocol", "store"}, {"operation", "get"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& store_latency_put = etp_latency_summary.Add(
        {{"protocol", "store"}, {"operation", "put"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& store_latency_delete = etp_latency_summary.Add(
        {{"protocol", "store"}, {"operation", "delete"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});

    oes::core::Metrics::instance().setSummary(oes::core::metrics::STORE_LATENCY_GET, store_latency_get);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::STORE_LATENCY_PUT, store_latency_put);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::STORE_LATENCY_DELETE, store_latency_delete);

    // Dataspace protocol latencies
    auto& dataspace_latency_get = etp_latency_summary.Add(
        {{"protocol", "dataspace"}, {"operation", "get"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& dataspace_latency_put = etp_latency_summary.Add(
        {{"protocol", "dataspace"}, {"operation", "put"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& dataspace_latency_delete = etp_latency_summary.Add(
        {{"protocol", "dataspace"}, {"operation", "delete"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});

    oes::core::Metrics::instance().setSummary(oes::core::metrics::DATASPACE_LATENCY_GET, dataspace_latency_get);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::DATASPACE_LATENCY_PUT, dataspace_latency_put);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::DATASPACE_LATENCY_DELETE, dataspace_latency_delete);

    // Array protocol latencies
    auto& array_latency_get_arrays = etp_latency_summary.Add(
        {{"protocol", "array"}, {"operation", "get_arrays"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& array_latency_get_subarrays = etp_latency_summary.Add(
        {{"protocol", "array"}, {"operation", "get_subarrays"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& array_latency_put_arrays = etp_latency_summary.Add(
        {{"protocol", "array"}, {"operation", "put_arrays"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& array_latency_put_subarrays = etp_latency_summary.Add(
        {{"protocol", "array"}, {"operation", "put_subarrays"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& array_latency_put_uninitialized_arrays = etp_latency_summary.Add(
        {{"protocol", "array"}, {"operation", "put_uninitialized_arrays"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});

    oes::core::Metrics::instance().setSummary(oes::core::metrics::ARRAY_LATENCY_GET_ARRAYS, array_latency_get_arrays);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::ARRAY_LATENCY_GET_SUBARRAYS, array_latency_get_subarrays);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::ARRAY_LATENCY_PUT_ARRAYS, array_latency_put_arrays);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::ARRAY_LATENCY_PUT_SUBARRAYS, array_latency_put_subarrays);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::ARRAY_LATENCY_PUT_UNINITIALIZED_ARRAYS, array_latency_put_uninitialized_arrays);

    // Discovery protocol latencies
    auto& discovery_latency_get_resources = etp_latency_summary.Add(
        {{"protocol", "discovery"}, {"operation", "get_resources"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& discovery_latency_get_deleted_resources = etp_latency_summary.Add(
        {{"protocol", "discovery"}, {"operation", "get_deleted_resources"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});

    oes::core::Metrics::instance().setSummary(oes::core::metrics::DISCOVERY_LATENCY_GET_RESOURCES, discovery_latency_get_resources);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::DISCOVERY_LATENCY_GET_DELETED_RESOURCES, discovery_latency_get_deleted_resources);

    // Transaction protocol latencies
    auto& transaction_latency_start = etp_latency_summary.Add(
        {{"protocol", "transaction"}, {"operation", "start"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& transaction_latency_commit = etp_latency_summary.Add(
        {{"protocol", "transaction"}, {"operation", "commit"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});
    auto& transaction_latency_rollback = etp_latency_summary.Add(
        {{"protocol", "transaction"}, {"operation", "rollback"}}, prometheus::Summary::Quantiles{{0.5, 0.05}, {0.9, 0.01}, {0.99, 0.001}});

    oes::core::Metrics::instance().setSummary(oes::core::metrics::TRANSACTION_LATENCY_START, transaction_latency_start);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::TRANSACTION_LATENCY_COMMIT, transaction_latency_commit);
    oes::core::Metrics::instance().setSummary(oes::core::metrics::TRANSACTION_LATENCY_ROLLBACK, transaction_latency_rollback);

    // ETP message payload-size histograms (on-the-wire bytes).
    auto& etp_message_bytes = prometheus::BuildHistogram().Name("etp_message_bytes")
                                                          .Help("On-the-wire size in bytes of ETP messages")
                                                          .Register(*registry);
    const prometheus::Histogram::BucketBoundaries byte_buckets = {
        1000, 64000, 1000000, 10000000, 50000000, 100000000, 250000000, 400000000, 500000000};
    auto& etp_request_bytes = etp_message_bytes.Add({{"direction", "request"}}, byte_buckets);
    auto& etp_response_bytes = etp_message_bytes.Add({{"direction", "response"}}, byte_buckets);
    oes::core::Metrics::instance().setHistogram(oes::core::metrics::ETP_REQUEST_BYTES, etp_request_bytes);
    oes::core::Metrics::instance().setHistogram(oes::core::metrics::ETP_RESPONSE_BYTES, etp_response_bytes);
}
} // oes::eml::etp12