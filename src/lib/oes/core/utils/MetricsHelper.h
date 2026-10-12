// ============================================================================
// Copyright © Microsoft Corporation
// ============================================================================

#pragma once

#include <chrono>
#include <string>

#include <oes/core/utils/export.h>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/Logger.h>

namespace oes::core {

/**
 * @brief Helper class for simplified metrics recording with automatic error handling
 */
class OES_CORE_UTILS_EXPORT MetricsHelper {
public:
    /**
     * @brief Increment a counter metric safely
     * @param metricName Name of the counter metric to increment
     *
     * Automatically handles MetricNotFound exceptions and logs warnings
     */
    static void updateCounterMetric(const std::string& metricName);

    /**
     * @brief Observe latency metric by calculating duration from start time
     * @param metricName Name of the summary metric to observe
     * @param startTime The start time point for latency calculation
     *
     * Calculates duration in milliseconds and observes the metric.
     * Automatically handles MetricNotFound exceptions and logs warnings.
     */
    static void observeLatency(
        const std::string& metricName,
        const std::chrono::steady_clock::time_point& startTime
    );

    /**
     * @brief Record both counter and latency metrics in one call
     * @param counterName Name of the counter metric to increment
     * @param latencyName Name of the summary metric to observe
     * @param startTime The start time point for latency calculation
     *
     * Convenience method that combines updateCounterMetric and observeLatency.
     */
    static void recordCounterWithLatency(
        const std::string& counterName,
        const std::string& latencyName,
        const std::chrono::steady_clock::time_point& startTime
    );
};

} // namespace oes::core
