// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Pool and task traits.

#ifndef THREAD_POOL_DETAIL_TRAITS_H
#define THREAD_POOL_DETAIL_TRAITS_H

#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>

namespace pool {
    // Forward declaration.
    template<typename Init>
    class ThreadPoolExecutor;
}

namespace pool::aux {
    // ========================================================================
    // Pool traits
    // ========================================================================

    // Pool traits.
    // Internal data structures.
    // It may be deduced when ThreadPoolExecutor is instantiated.
    template<typename Init>
    struct PoolTraits {
        using Executor = ThreadPoolExecutor<Init>;
        using Initializer = Init;
        using Thread = std::thread;
        using Mutex = std::mutex;
        using Lock = std::unique_lock<Mutex>;
        using TaskWrapper = std::function<void(Initializer &)>;
        using TaskQueue = std::queue<TaskWrapper>;
    };

    // ========================================================================
    // Task traits
    // ========================================================================

    // Task traits (non-defined).
    template<typename Init, typename F>
    struct TaskTraits;

    // Task traits (empty initializer).
    // Data structures that are dependent on the function signature provided by the client.
    // It may be deduced only within ThreadPoolExecutor::Submit() or ThreadPoolExecutor::Map().
    template<typename R, typename... ArgTypes>
    struct TaskTraits<EmptyInitializer, R
                (ArgTypes...)> : PoolTraits<EmptyInitializer> {
        using Fn = std::function<R(ArgTypes...)>;
        using ArgTuple = std::tuple<std::decay_t<ArgTypes>...>;
        using Result = R;
        using Future = std::future<Result>;
        using FutureQueue = std::queue<Future>;
        using PackagedTask = std::packaged_task<R(ArgTypes...)>;
    };

    // We need this concept to avoid the ambiguity of TaskTraits specializations.
    template<typename T>
    concept NonEmptyInitializer = not std::is_same_v<T, EmptyInitializer>;

    // Task traits (non-empty initializer).
    // Data structures that are dependent on the function signature provided by the client.
    // It may be deduced only within ThreadPoolExecutor::Submit() or ThreadPoolExecutor::Map().
    template<NonEmptyInitializer Init, typename R, typename InitType, typename
        ... ArgTypes>
    struct TaskTraits<Init, R(InitType, ArgTypes...)> : PoolTraits<Init> {
        using Fn = std::function<R(InitType, ArgTypes...)>;
        // We don't store Initializer within task queue, so we should to exclude it.
        using ArgTuple = std::tuple<std::decay_t<ArgTypes>...>;
        using Result = R;
        using Future = std::future<Result>;
        using FutureQueue = std::queue<Future>;
        using PackagedTask = std::packaged_task<R(InitType, ArgTypes...)>;
    };
}

#endif // THREAD_POOL_DETAIL_TRAITS_H
