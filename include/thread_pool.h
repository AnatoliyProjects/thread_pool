// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Single header with all the content.

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "thread_pool/constants.h"
#include "thread_pool/detail/iterator.h"
#include "thread_pool/detail/task.h"
#include "thread_pool/detail/traits.h"
#include "thread_pool/detail/utils.h"
#include "thread_pool/detail/worker.h"
#include "thread_pool/executor.h"

#endif // THREAD_POOL_H
