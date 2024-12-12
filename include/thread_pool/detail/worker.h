// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Worker. Representation of the worker thread.

#ifndef THREAD_POOL_DETAIL_WORKER_H
#define THREAD_POOL_DETAIL_WORKER_H

#include <cassert>

namespace pool::aux {
    // ========================================================================
    // Worker
    // ========================================================================

    // Representation of the worker thread.
    template<typename Traits>
    class Worker {
        using Initializer = typename Traits::Initializer;
        using Thread = typename Traits::Thread;
        using Lock = typename Traits::Lock;

    public:
        using Executor = typename Traits::Executor;

        // Creates an Initializer (if present) and starts a new thread.
        template<typename... ArgTypes>
        explicit
        Worker(Executor *executor,
               ArgTypes... initargs) : executor_(executor),
                                       initializer_(std::move(initargs)...) {
            thread_ = Thread([this] { Run(); });
        }

        // Blocks the current thread until the worker thread finishes.
        void Join() {
            assert(
                thread_.joinable() &&
                "Thread is not joinable [pool::aux::Worker::Join()]");
            thread_.join();
        }

    private:
        Executor *executor_;
        Initializer initializer_;
        Thread thread_;

        // Worker thread loop.
        void Run() {
            while (true) {
                Lock lock(executor_->mutex_);
                while (executor_->jobs_.empty()) {
                    if (executor_->on_terminate_) return;
                    // not on_terminate
                    Wait(lock);
                }
                ProcessTask(lock);
            }
        }

        // Waits for the new task.
        void Wait(Lock &lock) const {
            ++executor_->waiting_workers_;
            executor_->consumer_cv_.notify_one();
            executor_->producer_cv_.wait(lock);
            --executor_->waiting_workers_;
        }

        // Processes the new task.
        void ProcessTask(Lock &lock) {
            auto task = std::move(executor_->jobs_.front());
            executor_->jobs_.pop();
            // We should unlock mutex before starting the task!
            // Otherwise, our performance will drop dramatically!
            lock.unlock();
            task(initializer_);
        }
    };
}

#endif // THREAD_POOL_DETAIL_WORKER_H
