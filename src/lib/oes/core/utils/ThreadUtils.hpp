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

#ifndef OES_CORE_TYPES_THREAD_UTILS_HPP
#define OES_CORE_TYPES_THREAD_UTILS_HPP

#include <oes/core/utils/ThreadUtils.h>
#include <oes/core/utils/System.h>

namespace oes {
namespace core {

/*****************************************************************************/

template <typename M>
void OptionalMutex<M>::lock() {
    if (p_mutex_) {
        p_mutex_->lock();
    }
}

template <typename M>
bool OptionalMutex<M>::try_lock() {
    if (p_mutex_) {
        return p_mutex_->try_lock();
    }
    return true;
}

template <typename M>
void OptionalMutex<M>::unlock() {
    if (p_mutex_) {
        p_mutex_->unlock();
    }
}

/*****************************************************************************/

template <typename M>
void OptionalSharedMutex<M>::lock_shared() {
    if (this->p_mutex_) {
        this->p_mutex_->lock_shared();
    }
}

template <typename M>
bool OptionalSharedMutex<M>::try_lock_shared() {
    if (this->p_mutex_) {
        return this->p_mutex_->try_lock_shared();
    }
    return true;
}

template <typename M>
void OptionalSharedMutex<M>::unlock_shared() {
    if (this->p_mutex_) {
        this->p_mutex_->unlock_shared();
    }
}

/*****************************************************************************/

template <typename M>
double TracingMutex<M>::wait_percent() const {
    if (wait_time_ < 0.001) {
        return 0.0;
    }
    return 100.0 * (wait_time_ / total_time_);
}

template <typename M>
std::string TracingMutex<M>::to_string() const {
    return tracing_mutex_to_string(lock_count_, total_time_, wait_time_, wait_percent(), max_wait_time_);
}

template <typename M>
void TracingMutex<M>::lock() {
    const double wait_start = System::getRealTime();
    mutex_.lock();

    // now that we are locked, we can safely write to member vars
    const double cur_wait_time = (System::getRealTime() - wait_start);
    wait_time_ += cur_wait_time;
    last_lock_start_ = wait_start;
    if (cur_wait_time > max_wait_time_) {
        max_wait_time_ = cur_wait_time;
    }
    ++lock_count_;
}

template <typename M>
bool TracingMutex<M>::try_lock() {
    const double wait_start = System::getRealTime();

    if (mutex_.try_lock()) {
        // now that we are locked, we can safely write to member vars

        // not supposed to wait, so not recording any wait-time
        //const double cur_wait_time = (System::getRealTime() - wait_start);
        //wait_time_ += cur_wait_time;
        //if (cur_wait_time > max_wait_time_) {
        //    max_wait_time_ = cur_wait_time;
        //}

        last_lock_start_ = wait_start;
        ++lock_count_;
        return true;
    }

    // not locked, so not safe to write to our state
    return false;
}

template <typename M>
void TracingMutex<M>::unlock() {
    total_time_ += (System::getRealTime() - last_lock_start_);

    // warning, no longer safe to write to any member vars
    mutex_.unlock();
}

/*****************************************************************************/

template <typename M>
std::string TracingSharedMutex<M>::shared_to_string() {
    std::lock_guard<decltype(shared_mt_)> guard(shared_mt_);
    return shared_tracing_mutex_to_string(
        shared_lock_count_, shared_unlock_count_, shared_wait_time_, shared_max_wait_time_
    );
}

template <typename M>
void TracingSharedMutex<M>::lock_shared() {
    const double wait_start = System::getRealTime();
    this->mutex_.lock_shared();

    const double cur_wait_time = (System::getRealTime() - wait_start);

    // we are locked, but in shared-mode only. Several shared-mutexes
    // could be trying to update this state at the same time... We thus
    // most use our own exclusive mutex to update our own state, kinda
    // slowing down (and perhaps defeating?) the main draw of shared-locks.
    std::lock_guard<decltype(shared_mt_)> guard(shared_mt_);
    shared_wait_time_ += cur_wait_time;
    if (cur_wait_time > shared_max_wait_time_) {
        shared_max_wait_time_ = cur_wait_time;
    }
    ++shared_lock_count_;
}

template <typename M>
bool TracingSharedMutex<M>::try_lock_shared() {
    if (this->mutex_.try_lock_shared()) {
        // not supposed to wait, so not recording any wait-time
        //const double cur_wait_time = (System::getRealTime() - wait_start);
        //wait_time_ += cur_wait_time;
        //if (cur_wait_time > max_wait_time_) {
        //    max_wait_time_ = cur_wait_time;
        //}

        std::lock_guard<decltype(shared_mt_)> guard(shared_mt_);
        ++shared_lock_count_;
        return true;
    }

    // not locked, so not safe to write to our state
    return false;
}

template <typename M>
void TracingSharedMutex<M>::unlock_shared() {
    // since shared-locks can "overlap", we have no way to tell
    // from within the lock itself how long spend in the shared_lock.
    //total_time_ += (System::getRealTime() - last_lock_start_);

    this->mutex_.unlock_shared();

    std::lock_guard<decltype(shared_mt_)> guard(shared_mt_);
    ++shared_unlock_count_;
}

template <typename M>
inline std::ostream& operator<<(std::ostream& os, TracingMutex<M>& m) {
    std::lock_guard<std::mutex> lock(m.mutex_);
    return os << m.to_string();
}

} // namespace core
} // namespace oes

#endif // OES_CORE_TYPES_THREAD_UTILS_HPP
