#pragma once

// --------------------------------------------------------------------------
// Compute-extensions governance.
//
// The array-compute extensions (STATS) are the only handlers that are
// CPU-bound rather than I/O-bound: they tie up a worker thread for the full
// duration of the fold. Two process-wide guards keep them from starving the
// store service:
//
//   COMPUTE_MAX_CONCURRENCY  max concurrent compute ops (default 4).
//                            Excess requests are rejected with ELIMIT_EXCEEDED
//                            instead of queueing on - and blocking - the worker
//                            threads. "0" == unlimited (explicit opt-out).
//   COMPUTE_MAX_RUNTIME_MS   per-op wall-clock budget (default 60000).
//                            Checked once per ~1 MiB read window; an op that
//                            exceeds it fails with ETIMED_OUT. "0" == no limit
//                            (explicit opt-out).
//
// Both limits are ON by default. An unset, empty or malformed value yields the
// default (never "unlimited"); only an explicit "0" disables a guard. Values
// are read on each call (a single getenv is negligible next to a fold) so they
// can be exercised deterministically by tests.
// --------------------------------------------------------------------------

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>

namespace oes {
namespace eml {
namespace openkv {

inline constexpr int  kDefaultComputeMaxConcurrency = 4;
inline constexpr long kDefaultComputeMaxRuntimeMs   = 60000L; // 60 s

// Process-wide count of compute-extension operations currently executing.
inline std::atomic<int> g_compute_inflight{0};

// Parse a COMPUTE_MAX_CONCURRENCY value. Unset/empty/malformed or outside
// [0, 100000] -> kDefaultComputeMaxConcurrency. "0" -> 0 (unlimited).
inline int parse_compute_max_concurrency(const char* v) {
    if (v == nullptr || *v == '\0') return kDefaultComputeMaxConcurrency;
    errno = 0;
    char* end = nullptr;
    const long n = std::strtol(v, &end, 10);
    if (errno != 0 || end == v || *end != '\0' || n < 0 || n > 100000) {
        return kDefaultComputeMaxConcurrency;
    }
    return static_cast<int>(n);
}

inline int compute_max_concurrency() {
    return parse_compute_max_concurrency(std::getenv("COMPUTE_MAX_CONCURRENCY"));
}

// RAII token for a compute slot. Construct before running a compute-extension
// handler; `acquired_` reports whether a slot was available. When the cap
// (max_concurrency) is reached the token is not acquired and the caller must
// reject the request. A non-positive cap means unlimited. The slot is released
// in the destructor, so the token must outlive the computation.
struct ComputeGate {
    bool acquired_ = false;

    explicit ComputeGate(int max_concurrency) {
        if (max_concurrency <= 0) { // 0 (or negative) == unlimited
            g_compute_inflight.fetch_add(1, std::memory_order_relaxed);
            acquired_ = true;
            return;
        }
        int prev = g_compute_inflight.load(std::memory_order_relaxed);
        while (prev < max_concurrency) {
            if (g_compute_inflight.compare_exchange_weak(
                    prev, prev + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                acquired_ = true;
                return;
            }
        }
    }

    ~ComputeGate() {
        if (acquired_) {
            g_compute_inflight.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    ComputeGate(const ComputeGate&) = delete;
    ComputeGate& operator=(const ComputeGate&) = delete;
};

// Parse COMPUTE_MAX_RUNTIME_MS. Unset/empty/malformed or outside
// [0, 86400000] (24h) -> kDefaultComputeMaxRuntimeMs. "0" -> 0 (no limit).
inline long parse_compute_max_runtime_ms(const char* v) {
    if (v == nullptr || *v == '\0') return kDefaultComputeMaxRuntimeMs;
    errno = 0;
    char* end = nullptr;
    const long n = std::strtol(v, &end, 10);
    if (errno != 0 || end == v || *end != '\0' || n < 0 || n > 86400000L) {
        return kDefaultComputeMaxRuntimeMs;
    }
    return n;
}

inline long compute_max_runtime_ms() {
    return parse_compute_max_runtime_ms(std::getenv("COMPUTE_MAX_RUNTIME_MS"));
}

// Cheap per-op deadline. Construct once before a fold's read-window loop and
// call expired() once per window. When the budget is 0 the deadline is
// disabled and expired() is always false (a single cached bool, no clock read).
struct ComputeDeadline {
    std::chrono::steady_clock::time_point deadline_;
    bool enabled_;

    explicit ComputeDeadline(long budget_ms)
        : deadline_(std::chrono::steady_clock::now() + std::chrono::milliseconds(budget_ms))
        , enabled_(budget_ms > 0) {}

    bool expired() const {
        return enabled_ && std::chrono::steady_clock::now() >= deadline_;
    }
};

} // namespace openkv
} // namespace eml
} // namespace oes
