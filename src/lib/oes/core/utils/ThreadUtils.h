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

#ifndef OES_CORE_TYPES_THREAD_UTILS_H
#define OES_CORE_TYPES_THREAD_UTILS_H

#include <oes/core/utils/export.h>
#include <oes/core/utils/fwd.h>

#include <mutex>
#include <thread>

namespace oes {
namespace core {

/*****************************************************************************/

template <typename M = std::mutex>
struct OptionalMutex {
    using Mutex_t = M;
    using Lock_t = std::lock_guard<OptionalMutex<M>>;

    Mutex_t* p_mutex_ = nullptr;

    void lock();
    bool try_lock();
    void unlock();
};

// template parameter M could be:
// - std::shared_mutex (since C++17)
// - std::shared_timed_mutex (since C++14)
template <typename M>
struct OptionalSharedMutex : OptionalMutex<M> {
    /*
    ** Would require #include <shared_mutex> too
    using ULock_t = std::unique_lock<OptionalMutex<M>>;
    using SLock_t = std::shared_lock<OptionalMutex<M>>;
    */

    void lock_shared();
    bool try_lock_shared();
    void unlock_shared();
};

/*****************************************************************************/

template <typename M = std::mutex>
struct TracingMutex {
    using Lock_t = std::lock_guard<TracingMutex<M>>;

    void lock();
    bool try_lock();
    void unlock();

    double wait_percent() const;
    std::string to_string() const;

protected:
    M mutex_;

private:
    size_t lock_count_ = 0;
    double wait_time_ = 0;
    double total_time_ = 0;
    double max_wait_time_ = 0;
    double last_lock_start_ = 0;
};

// template parameter M could be:
// - std::shared_mutex (since C++17)
// - std::shared_timed_mutex (since C++14)
template <typename M>
struct TracingSharedMutex : TracingMutex<M> {
    void lock_shared();
    bool try_lock_shared();
    void unlock_shared();

    std::string shared_to_string();

private:
    std::mutex shared_mt_;
    size_t shared_lock_count_ = 0;
    size_t shared_unlock_count_ = 0;
    double shared_wait_time_ = 0;
    double shared_max_wait_time_ = 0;
};

OES_CORE_UTILS_EXPORT
std::string tracing_mutex_to_string(
    size_t lock_count,
    double total_time,
    double wait_time,
    double wait_percent,
    double max_wait_time
);

OES_CORE_UTILS_EXPORT
std::string shared_tracing_mutex_to_string(
    size_t lock_count,
    size_t unlock_count,
    double wait_time,
    double max_wait_time
);

/*****************************************************************************/

class OES_CORE_UTILS_EXPORT ThreadJoiner {
public:
    explicit ThreadJoiner(size_t max_size = 0);
    ~ThreadJoiner();

    void join_all();

    std::vector<std::thread> threads_;

    ThreadJoiner(const ThreadJoiner&) = delete;
    ThreadJoiner& operator =(const ThreadJoiner&) = delete;
};

/*****************************************************************************/

OES_CORE_UTILS_EXPORT
unsigned thisThreadId(bool reset_ids = false);

/*****************************************************************************/

} // namespace core
} // namespace oes

#endif // OES_CORE_TYPES_THREAD_UTILS_H
