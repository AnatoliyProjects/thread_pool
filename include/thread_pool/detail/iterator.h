// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Iterator. Returns by ThreadPoolExecutor::Map() method.

#ifndef THREAD_POOL_DETAIL_ITERATOR_H
#define THREAD_POOL_DETAIL_ITERATOR_H

#include <cassert>
#include <cstddef>
#include <future>
#include <iterator>

#include "thread_pool/constants.h"
#include "thread_pool/detail/utils.h"

namespace pool::aux {
    // ========================================================================
    // Iterator
    // ========================================================================

    // Input iterator retrieving results of asynchronous call (single-pass).
    // Has reduced end-of-range version (sentinel).
    template<typename Traits>
    class Iterator {
        using Executor = typename Traits::Executor;
        using Fn = typename Traits::Fn;
        using Result = typename Traits::Result;
        using Future = typename Traits::Future;
        using FutureQueue = typename Traits::FutureQueue;

    public:
        using difference_type = std::ptrdiff_t;
        using value_type = Result;

        // Constructs the end-of-range iterator (old-style sentinel).
        Iterator(): kIsSentinel(true) {
        }

        // Constructs the start-of-range iterator.
        // Collects all iterables and schedules asynchronous tasks.
        template<typename... Iters>
        explicit
        Iterator(Executor *executor, const Timeout timeout_sec, Fn fn,
                 Iters... its): kIsSentinel(false),
                                executor_(executor),
                                timeout_sec_(timeout_sec),
                                futures_{std::make_shared<FutureQueue>()} {
            auto ranges = aux::Batch(its...);
            while (not aux::Exhausted(ranges)) {
                auto args = aux::Dereference(ranges);
                Future future = executor_->Submit(fn, std::move(args));
                futures_->push(std::move(future));
                aux::Increment(ranges);
            }
        }

        // Returns true if the lhs iterator is at the end of the range (old-style sentinel).
        // Rhs iterator should always be a sentinel.
        bool operator==(const Iterator &other) const {
            assert(other.kIsSentinel &&
                "Not a sentinel [pool::aux::Iterator::operator==()]");
            return futures_->empty();
        }

        // Returns true if the lhs iterator is not at the end of the range (old-style sentinel).
        // Rhs iterator should always be a sentinel.
        bool operator!=(const Iterator &other) const {
            return not(*this == other);
        }

        // Returns true if the lhs iterator is at the end of the range (new-style sentinel).
        bool operator ==(const std::default_sentinel_t &) const {
            return futures_->empty();
        }

        // Returns true if the rhs iterator is at the end of the range (new-style sentinel).
        friend bool operator ==(const std::default_sentinel_t &,
                                const Iterator &other) {
            return other.futures_->empty();
        }

        // Moves the iterator to the next future (prefix form). Nonblocking.
        Iterator &operator ++() {
            Next();
            return *this;
        }

        // Moves the iterator to the next future (postfix form). Nonblocking.
        Iterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        // Awaits the future result. Blocking.
        // Raises a pool::TimeoutError if the result isn’t available
        // after timeout seconds from the dereferencing.
        Result operator*() const { return Await(); }

    private:
        bool kIsSentinel;
        Executor *executor_{nullptr};
        Timeout timeout_sec_{kNoTimeout};
        // We need to provide effective iterator copying to support the postfix increment.
        // Copying the queue may be costly, so we are sharing the single queue
        // among the source iterator and its copies. Our iterator is single-pass,
        // so increment operations on the copy are invalid.
        std::shared_ptr<FutureQueue> futures_{nullptr};

        // Moves iterator to the next Future.
        void Next() {
            assert(
                not futures_->empty() &&
                "Out of range [pool::aux::Iterator::Next()]");
            futures_->pop();
        }

        // Awaits for the future result.
        Result Await() const {
            Future &future = futures_->front();
            assert(
                future.valid() &&
                "Invalid future [pool::aux::Iterator::Await()]");
            if (timeout_sec_ == kNoTimeout) { return future.get(); }
            // with timeout
            switch (future.wait_for(timeout_sec_)) {
                case std::future_status::ready: return future.get();
                case std::future_status::timeout: throw TimeoutError(
                        "Future timeout [pool::aux::Iterator::Await()]");
                // future_status::deferred
                default: assert(
                        false &&
                        "Wrong task status [pool::aux::Iterator::Await()]");
            }
        }
    };
}

#endif // THREAD_POOL_DETAIL_ITERATOR_H
