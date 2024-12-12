// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Thread pool executor.
// A high-level interface for asynchronously executing callables.

#ifndef THREAD_POOL_EXECUTOR_H
#define THREAD_POOL_EXECUTOR_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <vector>

#include "thread_pool/constants.h"
#include "thread_pool/detail/iterator.h"
#include "thread_pool/detail/task.h"
#include "thread_pool/detail/traits.h"
#include "thread_pool/detail/worker.h"

namespace pool {
    // ========================================================================
    // Thread pool executor
    // ========================================================================

    // Thread pool executor.
    // Provides methods to execute calls asynchronously via a thread pool.
    template<typename Initializer = EmptyInitializer>
    class ThreadPoolExecutor {
        using Traits = aux::PoolTraits<Initializer>;
        using TaskQueue = typename Traits::TaskQueue;
        using Mutex = typename Traits::Mutex;
        using Lock = typename Traits::Lock;
        using Worker = aux::Worker<Traits>;
        friend Worker;
        template<typename F>
        using Task = aux::Task<aux::TaskTraits<Initializer, F> >;

    public:
        template<typename F>
        using Future = typename aux::TaskTraits<Initializer, F>::Future;
        template<typename F>
        using Iterator = aux::Iterator<aux::TaskTraits<Initializer, F> >;
        const unsigned kWorkersNum;

        // Creates a thread pool with the default number of worker threads
        // and without timeout for blocking operations.
        ThreadPoolExecutor(): ThreadPoolExecutor(kDefaultWorkers, kNoTimeout) {
        }

        // Creates a thread pool with the specified number of worker threads and timeout.
        // If the initializer is specified, it will be created for every worker thread
        // and passed as a first argument to Fn.
        template<typename... ArgTypes>
        ThreadPoolExecutor(
            const unsigned workers_num,
            const Timeout timeout_sec,
            ArgTypes &&... initargs) : kWorkersNum{workers_num},
                                       timeout_sec_(timeout_sec) {
            // We capture Worker 'this' with lambda, which initializes the thread.
            // Thus, we should avoid possible reallocation of the Worker because
            // after reallocation, captured 'this' will point to the wrong address.
            pool_.reserve(kWorkersNum);
            for (unsigned n = 0; n < kWorkersNum; ++n) {
                // No std::move(initargs) here! Otherwise, only the first Worker
                // gets the valid args.
                pool_.emplace_back(this, initargs...);
            }
            Lock lock(mutex_);
            consumer_cv_.wait(lock, [this] {
                return waiting_workers_ == kWorkersNum;
            });
        }

        // Terminates worker threads (if not already done).
        ~ThreadPoolExecutor() { if (not on_terminate_) { Shutdown(); } }

        // Schedules the callable, fn, to be executed as fn(args...). Nonblocking.
        // Returns a std::future object representing the execution of the callable.
        template<typename F, typename... ArgTypes>
        Future<F> Submit(std::function<F> fn, ArgTypes &&... args) {
            assert(
                not on_terminate_ &&
                "Already terminated [pool::ThreadPoolExecutor::Submit()]");
            Lock lock(mutex_);
            Task<F> task(std::move(fn), std::forward<ArgTypes>(args)...);
            Future<F> future = task.GetFuture();
            jobs_.emplace(std::move(task));
            lock.unlock();
            if (waiting_workers_) { producer_cv_.notify_one(); }
            return future;
        }

        // Returns an iterator that asynchronously applies callable, fn, to all
        // items in iterables. Nonblocking (but retrieving item from iterator is blocking).
        // The returned iterator raises a pool::TimeoutError if the result isn’t available
        // after timeout seconds from the dereferencing call.
        template<typename F, typename... Iters>
        Iterator<F> Map(std::function<F> fn, Iters... its) {
            assert(
                not on_terminate_ &&
                "Already terminated [pool::ThreadPoolExecutor::Map()]");
            return Iterator<F>(this, timeout_sec_, std::move(fn), its...);
        }

        // Returns an old-style sentinel, which has the same type as the iterator
        // returned by the ThreadPoolExecutor::Map().
        // For the new-style sentinel, use std::default_sentinel.
        template<typename F>
        Iterator<F> Sentinel() const {
            return Iterator<F>();
        }

        // Sets timeout for the item retrieving via the ThreadPoolExecutor::Map() iterator.
        void SetTimeout(const Timeout sec) { timeout_sec_ = sec; }

        // Gets timeout for the item retrieving via the ThreadPoolExecutor::Map() iterator.
        Timeout GetTimeout() const { return timeout_sec_; }

        // Terminates all worker threads.
        // If wait == true, then call is blocking, and the method returns
        // after all worker threads are terminated.
        // If cancel_futures == true, then the executor cancels all pending tasks;
        // otherwise, thread termination occurs after all tasks are processed.
        // There is no way to cancel tasks that have already started to execute.
        void Shutdown(const bool wait = true,
                      const bool cancel_futures = false) {
            Lock lock(mutex_);
            if (cancel_futures) {
                TaskQueue empty;
                std::swap(jobs_, empty);
            }
            // Busy worker thread (not waiting on cv) will miss our notification
            // about termination. Thus, we should guarantee that all threads are
            // in waiting state.
            consumer_cv_.wait(lock, [this] {
                return waiting_workers_ == kWorkersNum;
            });
            on_terminate_ = true;
            lock.unlock();
            producer_cv_.notify_all();
            if (wait) {
                for (auto &worker: pool_) { worker.Join(); }
                pool_.clear();
            }
        }

    private:
        std::vector<Worker> pool_;
        Mutex mutex_; // jobs_ synchronization and cv notifications
        TaskQueue jobs_;
        std::condition_variable producer_cv_; // new task or termination
        std::condition_variable consumer_cv_; // free worker
        std::atomic_uint waiting_workers_{0};
        std::atomic_bool on_terminate_{false};
        Timeout timeout_sec_;
    };
}

#endif // THREAD_POOL_EXECUTOR_H
