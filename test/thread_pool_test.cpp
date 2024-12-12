// Header-only C++ implementation of thread pool.
// Adaptation of Python concurrent.futures.ThreadPoolExecutor.
// SPDX-FileCopyrightText: Copyright © 2024 Anatoly Petrov <petrov.projects@gmail.com>
// SPDX-License-Identifier: MIT

// Unit tests for pool::ThreadPoolExecutor (gtest framework).

#include <algorithm>
#include <chrono>
#include <functional>
#include <iostream>
#include <numeric>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "thread_pool.h"

// Lifetime statistics.
struct Stats {
    size_t default_ctor{0};
    size_t copy_ctor{0};
    size_t move_ctor{0};
    size_t copy_assigment{0};
    size_t move_assigment{0};
    size_t dtor{0};

    bool operator==(const Stats &rhs) const {
        return default_ctor == rhs.default_ctor and copy_ctor == rhs.copy_ctor
               and move_ctor == rhs.move_ctor and copy_assigment == rhs.
               copy_assigment and move_assigment == rhs.move_assigment and dtor
               == rhs.dtor;
    }

    bool operator !=(const Stats &rhs) const {
        return not(*this == rhs);
    }
};

// Helper class for retrieving lifetime statistics.
struct Observer {
    static Stats stats_;
    const std::vector<int> initargs_;

    template<typename... Ts>
    explicit Observer(Ts... nums) : initargs_{nums...} {
        ++stats_.default_ctor;
    }

    Observer(const Observer &) { ++stats_.copy_ctor; }
    Observer(Observer &&) noexcept { ++stats_.move_ctor; }

    Observer &operator =(const Observer &) {
        ++stats_.copy_assigment;
        return *this;
    }

    Observer &operator =(Observer &&) noexcept {
        ++stats_.move_assigment;
        return *this;
    }

    ~Observer() { ++stats_.dtor; }

    static void Reset() {
        stats_ = Stats();
    }

    [[nodiscard]] int Sum() const {
        return std::reduce(initargs_.begin(), initargs_.end());
    }
};

Stats Observer::stats_{};

// Helper class for retrieving lifetime statistics about initargs.
struct ObserverInitargs {
    Observer observer_;

    template<typename T>
        requires(std::is_same_v<std::remove_cvref_t<T>, Observer>)
    explicit
    ObserverInitargs(T &&observer): observer_(std::forward<T>(observer)) {
    }
};

// Sums without initializer

static int SumFn(int a, int b) { return a + b; }

template<typename T1, typename T2>
static auto SumTemplateFn(T1 a, T2 b) { return a + b; }

static const std::function SumWrappedFn = [](int a, int b) { return a + b; };

static constexpr auto SumLambda = [](int a, int b) { return a + b; };

struct SumFunctorT {
    int operator()(int a, int b) const { return a + b; }
};

static constexpr SumFunctorT SumFunctor;

static const auto kSumOps = std::make_tuple(SumFn, SumTemplateFn<int, int>,
                                            SumWrappedFn, SumLambda,
                                            SumFunctor);

// Sums with initializer

static int SumFnWithInit(Observer &obs, int a, int b) {
    return a + b + obs.Sum();
}

template<typename T1, typename T2>
static auto SumTemplateFnWithInit(Observer &obs, T1 a, T2 b) {
    return a + b + obs.Sum();
}

static const std::function SumWrappedFnWithInit = [
        ](Observer &obs, int a, int b) {
    return a + b + obs.Sum();
};

static constexpr auto SumLambdaWithInit = [](Observer &obs, int a, int b) {
    return a + b + obs.Sum();
};

struct SumFunctorWithInitT {
    int operator()(Observer &obs, int a, int b) const {
        return a + b + obs.Sum();
    }
};

static constexpr SumFunctorWithInitT SumFunctorWithInit;

static const auto kSumOpsWithInit = std::make_tuple(
    SumFnWithInit, SumTemplateFnWithInit<int, int>,
    SumWrappedFnWithInit, SumLambdaWithInit,
    SumFunctorWithInit);

// Fixture for typed tests.
template<typename Pos>
class ThreadPoolExecutorTypedTest : public testing::Test {
protected:
    using Future = pool::ThreadPoolExecutor<>::Future<int(int, int)>;
    using FutureWithInit = pool::ThreadPoolExecutor<Observer>::Future<int(
        Observer &, int, int)>;
    using Iterator = pool::ThreadPoolExecutor<>::Iterator<int(int, int)>;
    using IteratorWithInit = pool::ThreadPoolExecutor<Observer>::Iterator<int(
        Observer &, int, int)>;
    using Fn = decltype(std::get<Pos::value>(kSumOps));
    using FnWithInit = decltype(std::get<Pos::value>(kSumOpsWithInit));

    static const Fn fn_;
    static const FnWithInit fn_with_init_;

    pool::ThreadPoolExecutor<> executor_;
    pool::ThreadPoolExecutor<Observer> executor_with_init_{
        pool::kDefaultWorkers, pool::kNoTimeout, 0, 1, 2, 3, 4, 5
    };

    const std::vector<int> initargs_ = {0, 1, 2, 3, 4, 5}; // sum = 15
    const std::vector<int> a_ = {1, 2, 3, 4};
    const std::vector<int> b_ = {4, 5, 6, 7};
    const std::vector<int> sum_ = {5, 7, 9, 11};
    const std::vector<int> sum_with_init_ = {20, 22, 24, 26};
    const size_t size_ = 4;
};

template<typename Pos>
const typename ThreadPoolExecutorTypedTest<Pos>::Fn ThreadPoolExecutorTypedTest<
    Pos>::fn_{
    std::get<Pos::value>(kSumOps)
};

template<typename Pos>
const typename ThreadPoolExecutorTypedTest<Pos>::FnWithInit
ThreadPoolExecutorTypedTest<Pos>::fn_with_init_{
    std::get<Pos::value>(kSumOpsWithInit)
};

template<size_t N>
using Pos = std::integral_constant<size_t, N>;
using PosTypes = testing::Types<Pos<0>, Pos<1>, Pos<2>, Pos<3>, Pos<4> >;

TYPED_TEST_SUITE(ThreadPoolExecutorTypedTest, PosTypes);

TYPED_TEST(ThreadPoolExecutorTypedTest, TestSubmitMethod) {
    std::vector<int> res;
    for (int i = 0; i < this->size_; ++i) {
        typename TestFixture::Future ftr = this->executor_.template Submit<int(
            int, int)>(
            this->fn_, this->a_[i], this->b_[i]);
        res.push_back(ftr.get());
    }
    EXPECT_EQ(res, this->sum_);
}

TYPED_TEST(ThreadPoolExecutorTypedTest, TestSubmitMethodWithInitializer) {
    std::vector<int> res;
    for (int i = 0; i < this->size_; ++i) {
        typename TestFixture::FutureWithInit ftr = this->executor_with_init_.
                template Submit<int(
                    Observer &, int, int)>(
                    this->fn_with_init_, this->a_[i], this->b_[i]);
        res.push_back(ftr.get());
    }
    EXPECT_EQ(res, this->sum_with_init_);
}

TYPED_TEST(ThreadPoolExecutorTypedTest, TestMapMethod) {
    std::vector<int> res;
    auto first = this->executor_.template Map<int(int, int)>(
        this->fn_, this->a_.begin(), this->a_.end(), this->b_.begin(),
        this->b_.end());
    auto last = typename TestFixture::Iterator();
    std::copy(first, last, std::back_inserter(res));
    EXPECT_EQ(res, this->sum_);
}

TYPED_TEST(ThreadPoolExecutorTypedTest, TestMapMethodWithInitializer) {
    std::vector<int> res;
    auto first = this->executor_with_init_.template Map<int(
        Observer &, int, int)>(
        this->fn_with_init_, this->a_.begin(), this->a_.end(), this->b_.begin(),
        this->b_.end());
    auto last = typename TestFixture::IteratorWithInit();
    std::copy(first, last, std::back_inserter(res));
    EXPECT_EQ(res, this->sum_with_init_);
}

TYPED_TEST(ThreadPoolExecutorTypedTest, TestMapMethodRanges) {
    std::vector<int> res;
    auto first = this->executor_.template Map<int(int, int)>(
        this->fn_, this->a_.begin(), this->a_.end(), this->b_.begin(),
        this->b_.end());
    auto last = std::default_sentinel;
    std::ranges::copy(first, last, std::back_inserter(res));
    EXPECT_EQ(res, this->sum_);
}

TYPED_TEST(ThreadPoolExecutorTypedTest, TestMapMethodRangesWithInitializer) {
    std::vector<int> res;
    auto first = this->executor_with_init_.template Map<int(
        Observer &, int, int)>(
        this->fn_with_init_, this->a_.begin(), this->a_.end(), this->b_.begin(),
        this->b_.end());
    auto last = std::default_sentinel;
    std::ranges::copy(first, last, std::back_inserter(res));
    EXPECT_EQ(res, this->sum_with_init_);
}

TEST(ThreadPoolExecutorTest, TestMapMethodTimeout) {
    pool::ThreadPoolExecutor executor(1, pool::Timeout(1));
    std::function task = [](int) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
    };
    std::vector nums = {0, 1, 2, 3, 4, 5};
    auto iter = executor.Map(task, nums.begin(), nums.end());
    EXPECT_THROW(*iter, pool::TimeoutError);
    executor.Shutdown(true, true);
}

TEST(ThreadPoolExecutorTest, TestShutdownMethodWithCanceledFutures) {
    pool::ThreadPoolExecutor<> executor;
    std::function task = [] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    };
    for (int i = 0; i < 1024; ++i) {
        executor.Submit(task);
    }
    // If pending futures are not canceled, shutdown takes min 1024 / 32 = 32 secs.
    // Otherwise, near 1 sec.
    auto before = std::chrono::system_clock::now();
    EXPECT_NO_THROW(executor.Shutdown(true, true));
    auto after = std::chrono::system_clock::now();
    EXPECT_LT(after - before, std::chrono::seconds(2));
}

TEST(ThreadPoolExecutorTest, TestShutdownMethodWithEmptyPool) {
    pool::ThreadPoolExecutor<> executor;
    std::function task = [] {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    };
    executor.Submit(task);
    // Check that there is no deadlock/exception in case of an empty pool.
    EXPECT_NO_THROW(executor.Shutdown(true, false));
}

TEST(ThreadPoolExecutorTest, TestMinWorkersNum) {
    pool::ThreadPoolExecutor<> executor;
    EXPECT_GE(executor.kWorkersNum, 5);
}

TEST(ThreadPoolExecutorTest, TestInitializerLifetime) {
    Observer::Reset();
    pool::ThreadPoolExecutor<Observer> executor(
        pool::kDefaultWorkers, pool::kNoTimeout, 0, 1, 2, 3, 4, 5);
    std::function<void(Observer &)> task = [this](const Observer &) {
    };
    executor.Submit<void(Observer &)>(task);
    executor.Shutdown();
    EXPECT_EQ(Observer::stats_,
              (Stats{pool::kDefaultWorkers, 0, 0, 0, 0, pool::kDefaultWorkers}
              ));
}

TEST(ThreadPoolExecutorTest, TestInitargsLifetime) {
    Observer::Reset();
    Observer obs;
    pool::ThreadPoolExecutor<ObserverInitargs> executor(
        pool::kDefaultWorkers, pool::kNoTimeout, obs);
    std::function<void(ObserverInitargs &)> task = [this
            ](const ObserverInitargs &) {
    };
    executor.Submit<void(ObserverInitargs &)>(task);
    executor.Shutdown();
    EXPECT_EQ(Observer::stats_,
              (Stats{1, pool::kDefaultWorkers, pool::kDefaultWorkers, 0, 0,
                  pool::kDefaultWorkers * 2}));
}

TEST(ThreadPoolExecutorTest, TestArgsLifetime) {
    Observer::Reset();
    pool::ThreadPoolExecutor<> executor;
    std::function task = [this](const Observer &) {
    };
    Observer obs;
    executor.Submit<void(const Observer &)>(task, obs);
    executor.Shutdown();
    EXPECT_EQ(Observer::stats_, (Stats{1, 1, 2, 0, 0, 3}));
}
