// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Task. Implements delayed function call with unified call signature.

#ifndef THREAD_POOL_DETAIL_TASK_H
#define THREAD_POOL_DETAIL_TASK_H

#include <tuple>

#include "thread_pool/constants.h"

namespace pool::aux {
    // ========================================================================
    // Task
    // ========================================================================

    // Task. Implements delayed function call with unified call signature.
    // Allows us to store different std::packaged_task in the same queue.
    template<typename Traits>
    class Task {
        using Fn = typename Traits::Fn;
        using ArgTuple = typename Traits::ArgTuple;
        using Result = typename Traits::Result;
        using PackagedTask = typename Traits::PackagedTask;

    public:
        using Initializer = typename Traits::Initializer;
        using Future = typename Traits::Future;

        // Creates a new task for the desired function call (parameter list).
        template<typename... Ts>
        explicit Task(Fn fn, Ts &&... args): task_(std::make_shared<
                                                 PackagedTask>(
                                                 std::move(fn))),
                                             args_(std::forward<Ts>(args)...) {
        }

        // Creates a new task for the desired function call (tuple).
        template<typename... Ts>
        explicit Task(Fn fn, std::tuple<Ts...> args): task_(std::make_shared<
                PackagedTask>(
                std::move(fn))),
            args_(std::move(args)) {
        }

        // Performs function call.
        void operator()(Initializer &init) {
            if constexpr (std::is_same_v<Initializer, EmptyInitializer>) {
                std::apply(*task_, std::move(args_));
            } else {
                // non-empty initializer
                auto ext_args = std::tuple_cat(
                    std::forward_as_tuple(init),
                    std::move(args_));
                std::apply(*task_, std::move(ext_args));
            }
        }

        // Returns std::future for the current Task.
        Future GetFuture() {
            return task_->get_future();
        }

    private:
        // std::function<> (aka WrappedTask) compatible only with CopyAssignable functor.
        // Thus, we encapsulate std::packaged_task in std::shared_ptr.
        std::shared_ptr<PackagedTask> task_;
        ArgTuple args_;
    };
}

#endif // THREAD_POOL_DETAIL_TASK_H
