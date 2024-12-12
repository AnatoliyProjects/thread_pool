// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Usage example.

#include <array>
#include <chrono>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "thread_pool.h"

// Operation delay.
static std::chrono::seconds kLoadTime{5};

// Prints information about the event to std::cout (thread-safe).
void Print(const std::string& event) {
    static std::mutex m;
    std::lock_guard guard(m);
    auto time = std::chrono::system_clock::now();
    std::cout<<"Thread "<<std::this_thread::get_id();
    std::cout<<std::vformat(" [{:%r}]: {}", std::make_format_args(time, event))<<std::endl;
}

// Simulates I/O bound task (actual implementation may contain curl request, etc.).
std::string LoadByUrl(const std::string& url) {
    Print(std::format("Loading started: {}", url));
    std::this_thread::sleep_for(kLoadTime);
    Print(std::format("Loading ended: {}", url));
    return std::format("<Page at {}>", url);
}

// The same task, but encapsulated into the std::function.
static std::function WrappedLoader = [](const std::string& url) {
    std::this_thread::sleep_for(kLoadTime);
    return std::format("<Page at {}>", url);
};

static std::function WrappedLoadr = LoadByUrl;

// Resource URLs.
static std::array urls {
    "http://www.google.com/",
    "http://en.wikipedia.org/",
    "http://en.cppreference.com/"
};

int main(int, char *[]) {
    // This example shows how to use thread_pool to reduce delays in I/O-bound tasks.
    // We will fetch several web pages using their URLs concurrently.

    // 1) Async loading with ThreadPoolExecutor::Submit() method:
    std::cout<<"ThreadPoolExecutor::Submit()"<<std::endl;

    // Create a new thread pool with the default number of worker threads.
    // The actual number of threads depends on the operating system and hardware (min 5, max 32).
    // Worker threads initialize only once; their lifetime is similar to the executor's.
    // Thus, we avoid overhead for the repetitive creation of threads for every task.
    pool::ThreadPoolExecutor executor;

    // Create a storage for std::future representing result of asynchronous operation.
    std::vector<std::future<std::string> > futures;

    // Schedule HTTP requests for execution (non-blocking).
    for (const auto& url: urls) {
        // Here we need to provide a type hint (not needed for std::function<>).
        auto future = executor.Submit<std::string(const std::string&)>(LoadByUrl, url);
        futures.push_back(std::move(future));
    }
    Print("Batch loading started");

    // Now retrieve loaded resources (blocking).
    for (auto& future: futures) {
        Print(future.get());
    }
    Print("Batch loading ended");

    // 2) Async loading with ThreadPoolExecutor::Map() method:
    std::cout<<"ThreadPoolExecutor::Map()"<<std::endl;

    // We may simplify the example above with ThreadPoolExecutor::Map().
    // This method is non-blocking and returns an iterator over std::futures.
    // Retrieving results from futures (by iterator dereferencing) is blocking.
    // However, we may set a timeout. Also, take a look at WrappedLoader().
    // It is std::function<>, so we don't need to provide a type hint.

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
    Print("Batch loading started");
    std::ranges::copy(first, last, out);
    Print("Batch loading ended");
}
