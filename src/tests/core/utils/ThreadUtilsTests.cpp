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

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <future>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/ThreadUtils.hpp>

#include <oes/testing/core/ScopedTimer.h>
#include <oes/testing/core/TestMain.h>


namespace {

using namespace oes::core;

/*!
 * Implement multi threading pipeline pattern.
 *
 * 2 threads are involved:
 * - the thread of the client code.
 * - the thread in charge of executing a specific task (IO process, etc).
 *
 * The client code requests for a task, and each time enough results are accumulated,
 * those results are provided back to the client that can start processing them, instead
 * of waiting for the end of the task.
 */
class PipelineScheduler {
public:
    
    PipelineScheduler() {
        // start immediately a new thread
        // TODO: start n threads (configurable) so that tasks can be done by several workers.
        worker_thread_ = std::thread(&PipelineScheduler::worker_thread_process, this);
    }

    ~PipelineScheduler() {
        {
            std::lock_guard<std::mutex> lock(task_mutex);
            stop = true;
        }
        task_cv.notify_one();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    template<typename T, typename TRes>
    std::future<TRes> async_wait(
        std::function<TRes(std::function<void(T)>)> task_func, // called in separate thread
        std::function<void(T)> cb_func // called in thread of caller
    ) {
        // schedule the task to be executed by a worker thread
        std::shared_ptr<Task<T, TRes>> task(new Task<T, TRes>(task_func));
        {
            std::lock_guard<std::mutex> lock(task_mutex);
            tasks_.push_back(task);
        }
        task_cv.notify_one();
        for (;;) {
            std::unique_lock<std::mutex> lock(task->output_mutex);
            // wait for results
            task->output_cv.wait(lock, [task] { return !task->outputs.empty() || task->stop; });
            std::vector<T> chunks(std::move(task->outputs));
            task->outputs.clear();
            if (task->stop) {
                return std::move(task->ret_val_future); 
            }
            lock.unlock();
            for (auto& chunk : chunks) {
                cb_func(std::move(chunk));
            }
        }
    }

private:
    // base class for a Task to execute in different thread
    struct TaskBase {
        virtual ~TaskBase() = default;

        std::mutex output_mutex;
        std::condition_variable output_cv;
        bool stop = false;

        TaskBase() = default;
        TaskBase(const TaskBase&) = delete;
        TaskBase& operator=(const TaskBase&) = delete;

        virtual void execute() = 0;
    };

    // concrete Task. T is the type of the results.
    template<typename T, typename TRes>
    struct Task : TaskBase {
        using TaskFunParam = std::function<void(T)>;
        using TaskFunc = std::function<TRes(TaskFunParam)>;

        explicit Task(TaskFunc task_func) 
            : task(task_func)
            , ret_val_promise()
            , ret_val_future(ret_val_promise.get_future())
        {}

        // task to execute in worker's thread
        TaskFunc task;
        // output
        std::vector<T> outputs;
        std::promise<TRes> ret_val_promise;
        std::future<TRes> ret_val_future;

        void execute() override {
            // execution of the task, from "worker" thread
            try {
                ret_val_promise.set_value(task([this](T result) {
                    {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        outputs.emplace_back(std::move(result));
                    }
                    output_cv.notify_one();
                }));
            } catch (...) {
                ret_val_promise.set_exception(std::current_exception());
            }
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                stop = true;
            }
            output_cv.notify_one();
        }
    };

    template<typename T>
    struct Task<T,void> : TaskBase {
        using TaskFunParam = std::function<void(T)>;
        using TaskFunc = std::function<void(TaskFunParam)>;

        explicit Task(TaskFunc task_func)
            : task(task_func)
            , ret_val_promise()
            , ret_val_future(ret_val_promise.get_future())
        {}

        // task to execute in worker's thread
        TaskFunc task;
        // std::packaged_task<void(TaskFunParam)> also manage exception and future
        // output
        std::vector<T> outputs;
        std::promise<void> ret_val_promise;
        std::future<void> ret_val_future;

        void execute() override {
            // execution of the task, from "worker" thread
            try {
                task([this](T result) {
                    {
                        std::lock_guard<std::mutex> lock(output_mutex);
                        outputs.emplace_back(std::move(result));
                    }
                    output_cv.notify_one();
                });
                ret_val_promise.set_value();
            } catch (...) {
                ret_val_promise.set_exception(std::current_exception());
            }
            {
                std::lock_guard<std::mutex> lock(output_mutex);
                stop = true;
            }
            output_cv.notify_one();
        }
    };

private:
    std::thread worker_thread_;

    std::mutex task_mutex;
    std::vector<std::shared_ptr<TaskBase>> tasks_; // better as a queue?
    bool stop = false;
    std::condition_variable task_cv;

    // run on its own thread
    void worker_thread_process() {
        for (;;) {
            std::unique_lock<std::mutex> lock(task_mutex);
            task_cv.wait(lock, [this] { return !tasks_.empty() || stop; });
            if (stop) {
                return;
            }
            // TODO: with "multiple workers", take only 1 task, and calls task_cv.notify_one
            std::vector<std::shared_ptr<TaskBase>> cur_tasks(std::move(tasks_));
            tasks_.clear();
            lock.unlock();

            for (const std::shared_ptr<TaskBase>& cur : cur_tasks) {
                cur->execute();
            }
        }
    }
};

BOOST_AUTO_TEST_SUITE(ThreadUtilsTests)

BOOST_AUTO_TEST_CASE(test_pipeline) {

    auto main_thread_id = std::this_thread::get_id();

//     ThreadJoiner joiner;
    PipelineScheduler scheduler;

    // can execute on its own thread via
    // joiner.threads_.emplace_back
    // but not require here, can use the main thread
    int count = 0;
    scheduler.async_wait<std::vector<int>, void>(
        [&main_thread_id](std::function<void(std::vector<int>)> cb) {
            // simulate remote access/resource IO
            BOOST_REQUIRE(std::this_thread::get_id() != main_thread_id);
            // read/fetch a part
            cb({ 1, 2, 3 });
            // a second part
            cb({ 4, 5, 6 });
            // the end
            cb({ 7, 8 });
        },
        [&count, &main_thread_id](std::vector<int> chunk) {
            BOOST_REQUIRE(std::this_thread::get_id() == main_thread_id);
            if(count < 2) {
                BOOST_CHECK_EQUAL(size_t(3), chunk.size());
                BOOST_CHECK_EQUAL((count * 3 + 1), chunk[0]);
                BOOST_CHECK_EQUAL((count * 3 + 2), chunk[1]);
                BOOST_CHECK_EQUAL((count * 3 + 3), chunk[2]);
            } else {
                BOOST_CHECK_EQUAL(static_cast<size_t>(2), chunk.size());
                BOOST_CHECK_EQUAL(7, chunk[0]);
                BOOST_CHECK_EQUAL(8, chunk[1]);
            }
            ++count;
            BOOST_REQUIRE(count <= 3);
        }
    );
}

BOOST_AUTO_TEST_CASE(test_pipeline_exception) {
//     ThreadJoiner joiner;
    PipelineScheduler scheduler;

    // can execute on its own thread via
    // joiner.threads_.emplace_back
    // but not require here, can use the main thread
    auto future = scheduler.async_wait<int, void>(
        [](std::function<void(int)> /*cb*/) {
            // simulate error, should be reported to caller
            throw std::runtime_error("cannot process");
        },
        [](int /*result*/) {
            BOOST_FAIL("should not be called");
        }
    );
    try {
        future.get();
        BOOST_FAIL("expected runtime_error");
    } catch (const std::runtime_error& e) {
        BOOST_CHECK_EQUAL("cannot process", e.what());
    }
}

BOOST_AUTO_TEST_CASE(test_pipeline_exception_2) {
    // same as above, but client code "forget" to check returned value
    // then miss the exception
    PipelineScheduler scheduler;

//     try {
        scheduler.async_wait<int, void>(
            [](std::function<void(int)> /*cb*/) {
                // simulate error, should be reported to caller
                throw std::runtime_error("cannot process");
            },
            [](int /*result*/) {
                BOOST_FAIL("should not be called");
            }
        ); // end of scope of the anonymous returned value => exception? no...
//     } catch (const std::runtime_error& e) {
//         BOOST_CHECK_EQUAL("cannot process", e.what());
//     }
}

BOOST_AUTO_TEST_CASE(test_pipeline_return_scalar) {
//     ThreadJoiner joiner;
    PipelineScheduler scheduler;

    // can execute on its own thread via
    // joiner.threads_.emplace_back
    // but not require here, can use the main thread
    auto result = scheduler.async_wait<int, std::string>(
        [](std::function<void(int)> cb) {
            cb(1);
            cb(2);
            // "early" finish here, ie don't send empty chunk
            return "ok";
        },
        [](int /*chunk*/) {
            return;
        }
    );
    BOOST_REQUIRE_EQUAL("ok", result.get());
}


BOOST_AUTO_TEST_CASE(test_pipeline_n_threads) {
    std::thread::id thread1;
    std::thread::id thread2;
    std::thread::id thread3;

    {
    PipelineScheduler scheduler;
    ThreadJoiner joiner; // after scheduler! otherwise scheduler might be deleted before async_wait calls

    // run 3 independents 
    joiner.threads_.emplace_back([&scheduler, &thread1]() {
        scheduler.async_wait<int, void>(
            [&thread1](std::function<void(int)> cb) {
                thread1 = std::this_thread::get_id();
                cb(28);
            },
            [](int chunk) {
                BOOST_CHECK(chunk == 28);
            }
        );
    });
    joiner.threads_.emplace_back([&scheduler, &thread2]() {
        scheduler.async_wait<std::vector<int>, void>(
            [&thread2](std::function<void(std::vector<int>)> cb) {
                thread2 = std::this_thread::get_id();
                cb({ 1, 2, 3 }); // 1 chunk
            },
            [](std::vector<int> chunk) {
                BOOST_CHECK(chunk.size() == 3);
            }
        );
    });
    joiner.threads_.emplace_back([&scheduler, &thread3]() {
        scheduler.async_wait<std::string, void>(
            [&thread3](std::function<void(std::string)> cb) {
                thread3 = std::this_thread::get_id();
                cb("yep");
            },
            [](const std::string& chunk) {
                BOOST_CHECK(chunk == "yep");
            }
        );
    });
    } // join all

    BOOST_REQUIRE(thread1 == thread2);
    BOOST_REQUIRE(thread1 == thread3);
    BOOST_REQUIRE(thread1 != std::this_thread::get_id());
}

BOOST_AUTO_TEST_CASE(test_optional_mutex_empty) {
    OptionalMutex<> mutex;
    BOOST_CHECK(mutex.p_mutex_ == nullptr);
    mutex.lock();
    mutex.unlock();
    const bool locked = mutex.try_lock();
    BOOST_CHECK(locked);
    mutex.unlock();

    std::lock_guard<decltype(mutex)> lock(mutex);
}

template <typename M>
void try_exclusive_lock_then_unlock(M& mutex) {
    bool locked = false;

    // Warning: spurious no-lock possible/allowed... thus the loop
    for (int i = 0; i < 10; ++i) {
        locked = mutex.try_lock();
        if (locked) {
            break;
        }
    }
    BOOST_CHECK(locked);
    if (locked) {
        mutex.unlock();
    }
}

template <typename M>
void try_shared_lock_then_unlock(M& mutex) {
    bool locked = false;

    // Warning: spurious no-lock possible/allowed... thus the loop
    for (int i = 0; i < 10; ++i) {
        locked = mutex.try_lock_shared();
        if (locked) {
            break;
        }
    }
    BOOST_CHECK(locked);
    if (locked) {
        mutex.unlock_shared();
    }
}

BOOST_AUTO_TEST_CASE(test_optional_mutex_active) {
    // Warning: actual std::mutex here...
    // Relocking already-locked std::mutex is undefined-behavior!
    std::mutex real_exclusive_mutex;
    OptionalMutex<> mutex;
    mutex.p_mutex_ = &real_exclusive_mutex;

    mutex.lock();
    mutex.unlock();

    try_exclusive_lock_then_unlock(mutex);

    std::lock_guard<decltype(mutex)> lock(mutex);
}

BOOST_AUTO_TEST_CASE(test_optional_shared_mutex_empty) {
    OptionalSharedMutex<std::shared_timed_mutex> mutex;
    BOOST_CHECK(mutex.p_mutex_ == nullptr);

    // exclusive locking
    mutex.lock();
    mutex.unlock();
    const bool locked = mutex.try_lock();
    BOOST_CHECK(locked);
    mutex.unlock();

    {
        std::lock_guard<decltype(mutex)> lock(mutex);
    }

    // shared locking
    mutex.lock_shared();
    mutex.unlock_shared();

    try_shared_lock_then_unlock(mutex);

    {
        // "nested" locking (since shared)
        mutex.lock_shared();
        mutex.unlock_shared();
    }

    {
        // another nested locking, via RAII std::shared_lock
        std::shared_lock<decltype(mutex)> lock(mutex);
    }
    mutex.unlock_shared();
}

BOOST_AUTO_TEST_CASE(test_optional_shared_mutex_active) {
    // Warning: actual std::shared_timed_mutex here...
    // Relocking already-locked std::mutex is undefined-behavior!
    std::shared_timed_mutex real_shared_mutex;
    OptionalSharedMutex<decltype(real_shared_mutex)> mutex;
    mutex.p_mutex_ = &real_shared_mutex;

    // exclusive locking
    mutex.lock();
    mutex.unlock();
    const bool locked = mutex.try_lock();
    BOOST_CHECK(locked);
    mutex.unlock();

    {
        std::lock_guard<decltype(mutex)> lock(mutex);
    }

    // shared locking
    mutex.lock_shared();
    mutex.unlock_shared();

    try_shared_lock_then_unlock(mutex);

    {
        // "nested" locking (since shared)
        mutex.lock_shared();
        mutex.unlock_shared();
    }
    {
        // another nested locking, via RAII std::shared_lock
        std::shared_lock<decltype(mutex)> lock(mutex);
    }
    mutex.unlock_shared();
}

BOOST_AUTO_TEST_CASE(test_tracing_mutex) {
    // Warning: actual std::mutex here...
    // Relocking already-locked std::mutex is undefined-behavior!
    TracingMutex<std::mutex> mutex;

    mutex.lock();
    mutex.unlock();
    const std::string status1 = mutex.to_string();
  //BOOST_CHECK_EQUAL(status1, "In-lock 1x (0s), waited 0s (0%), max-wait 0s");
    BOOST_CHECK(boost::starts_with(status1, "In-lock 1x"));

    try_exclusive_lock_then_unlock(mutex);

    const std::string status2 = mutex.to_string();
  //BOOST_CHECK_EQUAL(status2, "In-lock 2x (0s), waited 0s (0%), max-wait 0s");
    BOOST_CHECK(boost::starts_with(status2, "In-lock 2x"));

    std::string status3;
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        status3 = mutex.to_string();
    }

  //BOOST_CHECK_EQUAL(status3, "In-lock 3x (0s), waited 0s (0%), max-wait 0s");
    BOOST_CHECK(boost::starts_with(status3, "In-lock 3x"));
}

BOOST_AUTO_TEST_CASE(test_tracing_shared_mutex) {
    // Warning: actual std::shared_timed_mutex here...
    // Relocking already-locked std::mutex is undefined-behavior!
    TracingSharedMutex<std::shared_timed_mutex> mutex;

    // exclusive locking
    mutex.lock();
    mutex.unlock();

    const std::string xstatus1 = mutex.to_string();
  //BOOST_CHECK_EQUAL(xstatus1, "In-lock 1x (0s), waited 0s (0%), max-wait 0s");
    BOOST_CHECK(boost::starts_with(xstatus1, "In-lock 1x"));

    const std::string sstatus1 = mutex.shared_to_string();
  //BOOST_CHECK_EQUAL(sstatus1, "In-Shared-lock 0x (0x unlocks), waited 0s, max-wait 0s");
    BOOST_CHECK(boost::starts_with(sstatus1, "In-Shared-lock 0x (0x unlocks)"));

    try_exclusive_lock_then_unlock(mutex);
    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 2x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 0x (0x unlocks)"));

    try_shared_lock_then_unlock(mutex);
    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 2x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 1x (1x unlocks)"));

    {
        std::lock_guard<decltype(mutex)> lock(mutex);
    }
    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 3x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 1x (1x unlocks)"));

    // shared locking
    mutex.lock_shared();
    mutex.unlock_shared();

    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 3x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 2x (2x unlocks)"));

    try_shared_lock_then_unlock(mutex);
    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 3x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 3x (3x unlocks)"));

    mutex.lock_shared();
    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 3x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 4x (3x unlocks)"));
    {
        // "nested" locking (since shared)
        mutex.lock_shared();
        mutex.unlock_shared();
        BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 3x"));
        BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 5x (4x unlocks)"));
    }
    {
        // another nested locking, via RAII std::shared_lock
        std::shared_lock<decltype(mutex)> lock(mutex);
    }
    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 3x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 6x (5x unlocks)"));

    mutex.unlock_shared();
    BOOST_CHECK(boost::starts_with(mutex.to_string(), "In-lock 3x"));
    BOOST_CHECK(boost::starts_with(mutex.shared_to_string(), "In-Shared-lock 6x (6x unlocks)"));
}

BOOST_AUTO_TEST_CASE(test_thisThreadId) {
    const unsigned id0 = thisThreadId(true);
    BOOST_CHECK_EQUAL(id0, 1u);

    const unsigned id1 = thisThreadId();
    BOOST_CHECK_EQUAL(id1, 1u); // still one

    auto core_count = std::thread::hardware_concurrency();
    const unsigned thread_count = oes::testing::perfValues(4u, core_count);
    const unsigned loop_count = oes::testing::perfValues(10u, 100u*1000u);
    const unsigned max_tid = 1u + thread_count; // +1 for this thread (tid = 1)

    double elapsed = 0;
    double thread_overhead = 0;
    const unsigned call_count = thread_count * loop_count;
    {
        // Note that the timer includes the overhead
        // of starting and joining on all those threads.
        oes::testing::ScopedTimer timer(
            fmt::format(
                "{} calls to thisThreadId() on {} threads",
                call_count, thread_count
            )
        );

        {
            ThreadJoiner joiner(thread_count);
            for (auto i = 0u; i < thread_count; ++i) {
                joiner.threads_.emplace_back([loop_count, max_tid]() {
                    const unsigned my_tid = thisThreadId();
                    if (my_tid > max_tid) {
                        BOOST_CHECK(my_tid <= max_tid);
                    }

                    for (auto j = 0u; j < loop_count; ++j) {
                        const unsigned tid = thisThreadId();
                        if (tid != my_tid) { // explicit test to avoid slow stringification
                            BOOST_CHECK_EQUAL(tid, my_tid); // still one
                        }
                    }
                });
            }
        }  // waiting on all threads here

        elapsed = timer.elapsed();
        timer.log(); // print now
        timer.reset(); // and not in Dtor

        // Must reset thread IDs, new group of threads
        const unsigned new_id0 = thisThreadId(true);
        BOOST_CHECK_EQUAL(new_id0, 1u);

        // Try to deduce the thread overhead from the calls themselves
        oes::testing::ScopedTimer timer_thread_overhead("");
        {
            ThreadJoiner joiner(thread_count);
            for (auto i = 0u; i < thread_count; ++i) {
                joiner.threads_.emplace_back([max_tid]() {
                    const unsigned my_tid = thisThreadId();
                    if (my_tid > max_tid) {
                        BOOST_CHECK(my_tid <= max_tid);
                    }
                });
            }
        }  // waiting on all threads here

        thread_overhead = timer_thread_overhead.elapsed();
        timer_thread_overhead.reset(); // do not print anything
    }

    if (oes::testing::arePerformanceTestsEnabled()) {
        OES_INFO(
            fmt::format("{} calls/sec (deduced {:.3f}s threading-overhead from {:.3f}s)"
            , static_cast<unsigned>(call_count / (elapsed - thread_overhead)), thread_overhead, elapsed
        ));
    }

    // check resetting IDs restarts from 1, for a different thread than this one
    std::thread::id first;
    {
        ThreadJoiner joiner;
        joiner.threads_.emplace_back([&first]() {
            first = std::this_thread::get_id();
            const unsigned my_tid = thisThreadId(true);
            BOOST_CHECK_EQUAL(my_tid, 1u);
        });
    }
    // Must serialize these two threads with 2 separate Joiners.
    // If using a single one for both, 2nd thread can start before the 1st!
    // (happened in CI on Intel only)
    {
        ThreadJoiner joiner;
        joiner.threads_.emplace_back([first]() {
            std::thread::id second = std::this_thread::get_id();
            const unsigned my_tid = thisThreadId();
            if (first == second) {
                // first thread is "logically" dead,
                // but somehow this "new" thread got the same ID.
                BOOST_CHECK_EQUAL(my_tid, 1u);
            } else {
                // New std::thread::id, so incremented
                BOOST_CHECK_EQUAL(my_tid, 2u);
            }
        });
    } // waiting on all threads here

    // reset thread IDs again on "main" thread, just in case, to avoid side-effects
    const unsigned new_id0 = thisThreadId(true);
    BOOST_CHECK_EQUAL(new_id0, 1u);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
