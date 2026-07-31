#include <stdexcept>
#include <string>
#include <atomic>
#include <chrono>
#include <functional>
#include "../src/core/concurrency/ThreadPool.h"

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + std::to_string(expected) + " == " + std::to_string(actual) + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

extern void run_test(const std::string& name, std::function<void()> test_func);

namespace {
    void test_thread_pool_basic() {
        vrutti::core::concurrency::ThreadPool pool(2);
        auto future = pool.enqueue([]() { return 42; });
        ASSERT_EQ(42, future.get());
    }

    void test_thread_pool_multiple_tasks() {
        vrutti::core::concurrency::ThreadPool pool(4);
        std::atomic<int> counter{0};
        std::vector<std::future<void>> futures;

        for (int i = 0; i < 100; ++i) {
            futures.push_back(pool.enqueue([&counter]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                counter++;
            }));
        }

        for (auto& f : futures) {
            f.get();
        }

        ASSERT_EQ(100, counter.load());
    }

    void test_thread_pool_void_task() {
        vrutti::core::concurrency::ThreadPool pool(1);
        bool executed = false;
        auto future = pool.enqueue([&executed]() { executed = true; });
        future.get();
        ASSERT_TRUE(executed);
    }
}

void run_concurrency_tests() {
    run_test("ThreadPool - Basic return value", test_thread_pool_basic);
    run_test("ThreadPool - Multiple concurrent tasks", test_thread_pool_multiple_tasks);
    run_test("ThreadPool - Void task execution", test_thread_pool_void_task);
}
