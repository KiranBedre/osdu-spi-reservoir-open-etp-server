// ============================================================================
// Copyright 2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_COMMON_QUEUEUTILS_HPP
#define OES_COMMON_QUEUEUTILS_HPP

#include <chrono>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <readerwriterqueue/readerwritercircularbuffer.h>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes::common {

template <typename T>
using FixedSizeBlockingQueue = moodycamel::BlockingReaderWriterCircularBuffer<T>;

//============================================================================

/*!
 * \brief Single-Producer Single-Consumer Wait-free and Lock-free Queue.
 *
 * Typically shared by two threads, one calling enqueue() (the producer),
 * while the other calling dequeue() (the consumer). When the queue is \em full,
 * enqueue() blocks until a slot becomes available, because the consumer side
 * dequeued an element (or the timeout expires).
 *
 * In environment where one does not fully control which thread the code calling
 * enqueue() and dequeue() runs on, such as tasks scheduled on Boost.Asio strands
 * running on a thread-pool, as long as those tasks are \em serialized, the actual
 * thread the calls are made on does not matter.
 *
 * This queue basically replaces a std::mutex + st::condition_variable combo, and
 * is faster to boot, provided both sides (producer and consumer) are single-threaded
 * (or serialized in general, see above).
 *
 * \see https://github.com/cameron314/readerwriterqueue (Simplified BSD License)
 */
template <typename Value>
struct SpscQueue {
    using queue_t = FixedSizeBlockingQueue<Value>;

    //! Underlying queue, with a larger API that the one wrapped here
    queue_t queue_;

    //! Timeout to apply on enqueue()/dequeue(). Warning: Not thread safe.
    std::chrono::milliseconds timeout_; // FIXME: Make it an atomic?

    /*!
     * \brief Instantiates a new SPSC queue.
     * \param max_size this queue's size (in \em value count, not bytes).
     * \param this queue's timeout.
     * \warning Changing timeout post-creation is not thread-safe.
     */
    SpscQueue(
        size_t max_size, std::chrono::milliseconds timeout
    ) : queue_(max_size)
      , timeout_(timeout)
    {
    }

    /*!
     * \brief Enqueues a value into this queue, blocking unless there is room.
     * 
     * \param v the value to enqueue. Zero-copy if an R-Value reference.
     * \return false on timeout. true otherwise, on successful enqueue.
     * \note the method \b blocks the calling thread when the queue is full.
     */
    bool enqueue(Value&& v) {
        return queue_.wait_enqueue_timed(
            std::forward<Value>(v), timeout_
        );
    }

    /*!
     * \brief Dequeues a value from this queue, blocking if empty.
     * 
     * \param v the value to enqueue. Zero-copy if an R-Value reference.
     * \return false on timeout. true otherwise, on successful enqueue.
     * \note the method \b blocks the calling thread when the queue is full.
     */
    bool dequeue(Value& v) {
        return queue_.wait_dequeue_timed(v, timeout_);
    }
    
    bool dequeue(Value& v, std::chrono::milliseconds timeout) {
        return queue_.wait_dequeue_timed(v, timeout);
    }
};

//============================================================================

} // namespace oes::common

#endif
