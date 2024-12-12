# thread_pool v. 1.0

**Header-only C++ implementation of thread pool.**

Adaptation of Python `concurrent.futures.ThreadPoolExecutor`.

The MIT License (MIT). Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>

# Rationale

Assume you have a bunch of I/O-bound or CPU-bond tasks. You may significantly 
improve the performance of your program if you execute them concurrently (I/O-bound)
or in parallel (CPU-bound).

But if you don't know the total number of tasks (for example, because they come from 
the I/O service), you may tend to create a new system thread for every task. 
This approach leads to significant overhead because thread creation is costly.

The popular workaround for this problem is thread reusing. You just need to create
the desired number of worker threads once and then reuse them for every task you'll encounter.

These reusable threads may be organized as a *thread pool*: a bunch of worker threads
that process tasks from the input queue and send results to the consumer.

Modern C++ provides some tools to deal with concurrency, including thread abstractions 
(`std::thread`, `std::jthread`), synchronization primitives (`std::mutex`, `std::contition_variable`, etc.), 
atomics, futures (`std::async`,`std::promise`/`std::future`, etc.), and parallel algorithms 
(with `std::execution` policies).

But the Standard doesn't guarantee that underlying threads will be pooled/cached in other ways
for further usage. So, every call of `std::thread`/`std::async`/ parallel algorithm 
may lead to the creation of a new system thread, for example:

> The function template std::async runs the function f asynchronously 
> (_potentially_ in a separate thread, which might be a part of a thread pool) 
> and returns a std::future that will eventually hold the result of that function call.  
> [cppreference/async](https://en.cppreference.com/w/cpp/thread/async)

And if, after all, a thread pool is created, we cannot tune the underlying
implementation for our needs (including the number of threads, their lifetime, initializers, etc.).

Also, the C++ concurrency library still provides only low-level API, which is
very strict to pitfalls and misunderstanding (hope `thread_pool` lib is not another evidence 
of this statement).

Because of the low-level API, we can't easily use concurrency features with the other parts of 
the Standard Library (iterators, algorithms, containers, streams, etc.).

Keeping this in mind, we have partially adopted the well-known Python module - 
[`concurrent.futures`](https://docs.python.org/3/library/concurrent.futures.html) -
to meet the needs of C++ developers and implemented `concurrent.futures.ThreadPoolExecutor` in pure C++.

`thread_pool` library, exactly `pool::ThreadPoolExecutor` class, provides a high-level interface 
for executing callables via a thread pool asynchronously. Library design fully complies with 
the Standard Library, so you may quickly bring concurrency to your everyday tasks with a nice and 
modern API.

# Usage example

Let's take a look at the simplified usage example. The full version is at
[example/main.cpp](example/main.cpp).

In this example, we will concurrently fetch multiple web pages.
Concurrent fetching significantly improves performance because now we may start loading the new page 
without waiting for the old one. Thus, we may load all pages in time for which a single-thread application 
loads only one.

```cpp
// ...

#include "thread_pool.h"

// Simulates I/O bound task (actual implementation may contain curl request, etc.).
std::string LoadByUrl(const std::string& url) {
    // ...
}

// The same task, but encapsulated into the std::function.
static std::function WrappedLoader = LoadByUrl;

// Resource URLs.
static std::array urls {
    "http://www.google.com/",
    "http://en.wikipedia.org/",
    "http://en.cppreference.com/"
};

int main(int, char *[]) {
    // 1) Async loading with ThreadPoolExecutor::Submit() method:
    
    // Create a new thread pool with the default number of worker threads.
    // The actual number of threads depends on the operating system and hardware (min 5, max 32).
    // Worker threads initialize only once; their lifetime is similar to the executor's.
    // Thus, we avoid overhead for the repetitive creation of threads for every task.
    pool::ThreadPoolExecutor executor;

    // Create a storage for std::future representing result of asynchronous operation.
    std::vector<std::future<std::string> > futures;
    
    // Schedule HTTP requests for execution (non-blocking).
    for (const auto& url: urls) {
        // Here, we need to provide a type hint (not needed for std::function<>).
        auto future = executor.Submit<std::string(const std::string&)>(LoadByUrl, url);
        futures.push_back(std::move(future));
    }
    
    // Now retrieve loaded resources (blocking).
    for (auto& future: futures) {
        std::string page = future.get();
        // ...
    }
    
    // 2) Async loading with ThreadPoolExecutor::Map() method:
    
    // We may simplify the example above with ThreadPoolExecutor::Map().
    // This method is non-blocking and returns an iterator over std::futures.
    // Retrieving results from the futures (by iterator dereferencing) is blocking.
    // However, we may set a timeout. Also, take a look at WrappedLoader().
    // Now it is std::function<>, so we don't need to provide a type hint.
    
    // Set a timeout.
    executor.SetTimeout(pool::Timeout(6));
    
    // Create an input iterator over the fetching web pages (non-blocking). 
    // Iterator is compatible with the Standard Library, including std::ranges.
    auto first = executor.Map(WrappedLoader, urls.begin(), urls.end());
   
    // Create the end-of-range iterator (sentinel).
    auto last = std::default_sentinel;
    
    // Create an output iterator.
    auto out = std::ostream_iterator<std::string>(std::cout, "\n");
    
    // Print web pages to std::cout (blocking).
    std::ranges::copy(first, last, out);
}
```

Here, all web pages will be loaded concurrently. You may also notice the following:
- `pool::ThreadPoolExecutor` interface effectively reproduces the interface of Python's
`concurrent.futures.ThreadPoolExecutor` (we still have some minor differences).
- The `pool::ThreadPoolExecutor` has two methods for asynchronous execution.
The first one, `Submit()`, receives a function and its arguments (like `std::async`),
schedules its concurrent execution, and returns a `std::future` to the result.
The second one, `Map()`, receives a function with N parameters and corresponding 
iterator ranges, consumes all iterables, schedules concurrent execution, and returns an input
iterator over the `std::future` representing the results. You may treat `Map()`
as a generalized and asynchronous analog of the `std::transform` algorithm.
- Both `Submit()` and `Map()` calls are non-blocking. Block of execution has place
only when you try to get the result from the returned `std::future`, directly (like with `Submit()`)
or indirectly (like with `Map()`).
- Worker threads are created only once; their lifetime depends on the `pool::ThreadPoolExecutor`
lifetime.
- You don't need to manually terminate worker threads (but if you want, you could do it with 
`Shutdown()` method; see below). Termination has a place within the `pool::ThreadPoolExecutor` destructor,
and it is guaranteed even in case of an exception. Thus, we don't need to reproduce `concurrent.futures.ThreadPoolExecutor` 
context manager protocol. In the C++ field, we have RAII for the same thing.

# Limitations

*First*, we assume that your tasks may be safely performed in multiple threads of execution. 
That is, you should use synchronization or atomics to avoid corruption of the data structures, 
for which multiple tasks have write access. You should also use synchronization to prevent race conditions 
over the shared data. You should take care of the deadlocks.

If you want to simplify things, use `thread_pool` only for isolated tasks without shared data 
(as in the example above).

This may significantly reduce the complexity of your code.

*Second*, both the `Submit()` and `Map()` methods always create a *copy* of your function and input arguments. 
This approach excludes dangling references on temporaries. It also eliminates the need to explicitly 
synchronize access to shared input data.

But, as a consequence, all input arguments must be `CopyConstructible` and `MoveConstructible`.
Also, the call of their copy ctor should be relatively cheap.

If you want to deal with original objects within tasks, use pointers or `std::reference_wrapper`.
In this case, you should implement synchronization by yourself.

*Third*, `thread_pool` library doesn't use vectorization under the hood.
This may be a benefit, because you may use `thread_pool` library for vectorization-unsafe tasks.

# References

## "thread_pool/constants.h"

| Entity             | Representation | Description                                                    |
|--------------------|----------------|----------------------------------------------------------------|
| `kDefaultWorkers`  | Constant       | The default number of worker threads (min 5, max 32).          |
| `EmptyInitializer` | Type           | Empty initializer for worker thread (stateless, does nothing). |
| `Timeout`          | Type           | Timeout for blocking operation in seconds.                     |
| `kNoTimeout`       | Constant       | Represents the lack of timeout.                                |
| `TimeoutError`     | Type           | Timeout error. Raises if blocking operation timeout occurs.    |

## "thread_pool/executor.h"

All functionality encapsulates within the `pool::ThreadPoolExecutor` class.

Nested types and constants:

| Entity        | Representation | Description                                                                      |
|---------------|----------------|----------------------------------------------------------------------------------|
| `Future<F>`   | Type           | Deduces `std::future` type returned by `Submit()` depends on the task signature. |
| `Iterator<F>` | Type           | Deduces forward iterator type returned by `Map()` depends on the task signature. |
| `kWorkersNum` | Constant       | Actual number of worker threads within a pool.                                   |

Class methods:

| Method                                                                                                | Description                                                                                                                                                                                                                                                                                                                                                           |
|-------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `ThreadPoolExecutor()`                                                                                | Creates a thread pool with the default number of worker threads and without timeout for blocking operations.                                                                                                                                                                                                                                                          |
| `ThreadPoolExecutor<ArgTypes...>(unsigned workers_num, Timeout timeout_sec, ArgTypes &&... initargs)` | Creates a thread pool with the specified number of worker threads and timeout. If the initializer is specified, it will be created for every worker thread and passed as a first argument to `fn`.                                                                                                                                                                    |
| `~ThreadPoolExecutor()`                                                                               | Terminates worker threads (if not already done).                                                                                                                                                                                                                                                                                                                      |
| `Future<F> Submit<F, ArgTypes...>(std::function<F> fn, ArgTypes &&... args)`                          | Schedules the callable, `fn`, to be executed as `fn(args...)`. Nonblocking. Returns a `std::future` object representing the execution of the callable.                                                                                                                                                                                                                |
| `Iterator<F> Map<F, Iters...>(std::function<F> fn, Iters... its)`                                     | Returns an iterator that asynchronously applies callable, `fn`, to all items in iterables. Nonblocking (but retrieving item from iterator is blocking). The returned iterator raises a `TimeoutError` if the result isn’t available after timeout seconds from the dereferencing call.                                                                                |
| `Iterator<F> Sentinel<F>() const`                                                                     | Returns an old-style sentinel, which has the same type as the iterator returned by `Map()`. For the new-style sentinel, use `std::default_sentinel`.                                                                                                                                                                                                                  |
| `void SetTimeout(const Timeout sec)`                                                                  | Sets timeout for the item retrieving via the `Map()` iterator.                                                                                                                                                                                                                                                                                                        |
| `Timeout GetTimeout() const`                                                                          | Gets timeout for the item retrieving via the `Map()` iterator.                                                                                                                                                                                                                                                                                                        |
| `void Shutdown(bool wait = true, bool cancel_futures = false)`                                        | Terminates all worker threads. If `wait == true`, then the call is blocking, and the method returns only after all worker threads are terminated. If `cancel_futures == true`, then the executor cancels all pending tasks; otherwise, thread termination occurs after all tasks are processed. There is no way to cancel tasks that have already started to execute. |


# Testing

`thread_pool` is hardly tested with `gtest` framework. Every public method has at least one test case.
Obviously, it doesn't exclude potential bugs. If you find one, feel free to make an issue on GitHub.

# Building

`thread_pool` is a header-only library. You can include it in your project without special compile/link steps.

The CMake files are provided for [thread_pool_test.cpp](test/thread_pool_test.cpp) (unit tests)
and [main.cpp](example/main.cpp) (usage example).

# License

`thread_pool` is licensed under the MIT License, see [LICENSE.txt](LICENSE.txt) for more information.
