// ============================================================================
// Copyright 2017-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/ThreadUtils.h>

#include <sstream>

#include <algorithm>
#include <thread>
#include <vector>

/*! \file oes/core/utils/ThreadUtils.h
 * \brief Defines classes used in multiple-thread environment.
 */

namespace oes {
namespace core {

/*****************************************************************************/

/*! \class OptionalMutex oes/core/utils/ThreadUtils.h
 * \brief BasicLockable class that handle optional mutex.
 * \ingroup oes_core_utils
 *
 * Used to hide code complexity.
 */

/*! \class TracingMutex oes/core/utils/ThreadUtils.h
 * \brief Mutex decorator with timings/tracing.
 * \ingroup oes_core_utils
 */

 /*! \class TracingSharedMutex oes/core/utils/ThreadUtils.h
 * \brief SharedMutex decorator with timings/tracing.
 * \ingroup oes_core_utils
 */

 /*****************************************************************************/

std::string tracing_mutex_to_string(
    size_t lock_count, double total_time,
    double wait_time, double wait_percent,
    double max_wait_time
) {
    std::ostringstream oss;
    oss.precision(3); // millisecond precision is plenty for us
    oss << "In-lock " << lock_count << "x "
        << "(" << total_time << "s), "
        << "waited " << wait_time << "s "
        << "(" << wait_percent << "%), "
        << "max-wait " << max_wait_time << "s";
    return oss.str();
}

std::string shared_tracing_mutex_to_string(
    size_t lock_count,
    size_t unlock_count,
    double wait_time,
    double max_wait_time
) {
    std::ostringstream oss;
    oss.precision(3); // millisecond precision is plenty for us
    oss << "In-Shared-lock " << lock_count << "x "
        << "(" << unlock_count << "x unlocks), "
        << "waited " << wait_time << "s"
        << ", max-wait " << max_wait_time << "s";
    return oss.str();
}

/*****************************************************************************/

/*! \class ThreadJoiner oes/core/utils/ThreadUtils.h
 * \brief RAII Thread container, joining all added threads when deleted.
 * \ingroup oes_core_utils
 */

ThreadJoiner::ThreadJoiner(size_t max_size) {
    if (max_size) {
        threads_.reserve(max_size);
    }
}

ThreadJoiner::~ThreadJoiner() {
    join_all();
}

void ThreadJoiner::join_all() {
    if (threads_.empty()) {
        return;
    }

    auto threads = std::move(threads_);
    threads_.clear();
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

/*****************************************************************************/

/*!
 * \brief Convenience to name threads using 1-based short integers.
 *
 * Once discovered, a thread keep the same ID. Could be a different thread though
 * that happen to shared the same std::thread::id as a previous thread.
 *
 * \param reset_ids (Defaults to false) to be used for unit-testing only.
 * \return this thread's 1-based ID.
 *
 * \note thread-safe, but not meant for performance critical code.
 */
unsigned thisThreadId(bool reset_ids) {
    // Note: Not using std::shared_timed_mutex, despite the mostly-read access
    // pattern here, since a plain std::mutex is 25% faster on Linux (arachne),
    // ~ 5M calls/sec on 8 threads, and a whopping > 2x faster on Windows, at
    // ~ 4M calls/sec on 32 threads. Code is much simpler too as a side-effect.
    static std::mutex known_threads_mt_;
    using entry_t = std::pair<std::thread::id, unsigned>;
    static std::vector<entry_t> known_threads__;

    std::thread::id curr = std::this_thread::get_id();

    std::lock_guard<std::mutex> xguard(known_threads_mt_);

    if (reset_ids || known_threads__.empty()) {
        if (reset_ids) {
            known_threads__.clear();
        }
        // if Hyperthreading is ON, does NOT return the *hardware* core-count,
        // but the logical one. Getting the real physical core-count is not "simple",
        // even GetLogicalProcessorInformation() may not work on exotic hardware it seems.
        auto core_count = std::thread::hardware_concurrency();
        known_threads__.reserve(2 * core_count);
        known_threads__.emplace_back(curr, 1u);
        return 1u;
    }

    entry_t curr_entry(curr, 0);
    auto found = std::lower_bound(
        known_threads__.begin(), known_threads__.end(), curr_entry,
        [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; }
    );
    if (found == known_threads__.end() || found->first != curr) {
        unsigned tid = 1 + static_cast<unsigned>(known_threads__.size());
        found = known_threads__.emplace(found, curr, tid);
        return tid;
    }

    return found->second;
}

/*****************************************************************************/

} // namespace core
} // namespace oes
