// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Helper functions for working with iterator ranges.

#ifndef THREAD_POOL_DETAIL_UTILS_H
#define THREAD_POOL_DETAIL_UTILS_H

#include <array>
#include <algorithm>
#include <tuple>

namespace pool::aux {
    template<typename Tuple>
    auto BatchImpl(Tuple tuple) { return tuple; }

    template<typename Tuple, typename Iter, typename... Iters>
    auto BatchImpl(Tuple tuple, Iter first, Iter last, Iters... its) {
        return BatchImpl(
            std::tuple_cat(tuple, std::make_tuple(std::make_pair(first, last))),
            its...);
    }

    // Batches iterators as a tuple of ranges (pairs of first/last).
    template<typename... Iters>
    auto Batch(Iters... its) {
        static_assert(sizeof...(Iters) % 2 == 0 and sizeof...(Iters) != 0,
                      "Wrong num of params [pool::aux::Batch()]");
        return BatchImpl(std::tuple(), its...);
    }

    // Increments the first iterator in every range.
    template<size_t I = 0, typename Ranges>
    void Increment(Ranges &ranges) {
        if constexpr (I != std::tuple_size_v<Ranges>) {
            ++std::get<I>(ranges).first;
            Increment<I + 1>(ranges);
        }
    }

    template<typename Ranges, size_t... Is>
    auto DereferenceImpl(Ranges &ranges, std::index_sequence<Is...>) {
        return std::make_tuple(*std::get<Is>(ranges).first...);
    }

    // Dereferences the first iterator in every range and returns the tuple
    // of retrieved items.
    template<typename Ranges>
    auto Dereference(Ranges &ranges) {
        return DereferenceImpl(
            ranges,
            std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<
                Ranges> > >());
    }

    template<typename Ranges, size_t... Is>
    auto ExhaustedImpl(const Ranges &ranges, std::index_sequence<Is...>) {
        std::array<bool, sizeof...(Is)> all_comp{
            std::get<Is>(ranges).first == std::get<Is>(ranges).second...
        };
        return std::any_of(all_comp.begin(), all_comp.end(),
                           [](auto comp) { return comp; });
    }

    // Returns true if any iterator range is exhausted.
    template<typename Ranges>
    bool Exhausted(const Ranges &ranges) {
        return ExhaustedImpl(
            ranges,
            std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<
                Ranges> > >());
    }
}

#endif // THREAD_POOL_DETAIL_UTILS_H
