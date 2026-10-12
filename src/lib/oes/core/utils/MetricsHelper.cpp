// ============================================================================
// Copyright © Microsoft Corporation
// ============================================================================

#include <oes/core/utils/MetricsHelper.h>
#include <oes/core/utils/Otel.h>

namespace oes::core {

void MetricsHelper::updateCounterMetric(const std::string& metricName) {
    try {
        Metrics::instance().getCounter(metricName).Increment();
    } catch (const MetricNotFound& e) {
        OES_WARNING("Failed to increment counter metric '{}': {}", metricName, e.what());
    }
}

void MetricsHelper::observeLatency(
    const std::string& metricName,
    const std::chrono::steady_clock::time_point& startTime
) {
    try {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
        Metrics::instance().getSummary(metricName).Observe(duration);
    } catch (const MetricNotFound& e) {
        OES_WARNING("Failed to observe latency metric '{}': {}", metricName, e.what());
    }
}

void MetricsHelper::recordCounterWithLatency(
    const std::string& counterName,
    const std::string& latencyName,
    const std::chrono::steady_clock::time_point& startTime
) {
    updateCounterMetric(counterName);
    observeLatency(latencyName, startTime);
}

} // namespace oes::core
