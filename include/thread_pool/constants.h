// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Typedefs and constants.

#ifndef THREAD_POOL_CONSTANTS_H
#define THREAD_POOL_CONSTANTS_H

#include <chrono>
#include <thread>

namespace pool {
    // Default number of worker threads (min 5, max 32).
    const unsigned kDefaultWorkers = std::min(static_cast<unsigned>(32),
                                              (std::thread::hardware_concurrency()
                                                   ? std::thread::hardware_concurrency()
                                                   : 1) + 4);

    // Empty worker thread initializer (stateless, does nothing).
    struct EmptyInitializer {
    };

    // Timeout for blocking operation in seconds.
    using Timeout = std::chrono::seconds;

    // No-timeout constant.
    constexpr Timeout kNoTimeout = std::chrono::seconds::max();

    // Timeout error.
    // Raises if blocking operation timeout occurs.
    class TimeoutError final : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };
}

#endif // THREAD_POOL_CONSTANTS_H
