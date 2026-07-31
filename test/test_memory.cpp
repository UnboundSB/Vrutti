#include <stdexcept>
#include <string>
#include "../src/core/memory/ArenaAllocator.h"

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #expected + " == " + #actual + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

extern void run_test(const std::string& name, void (*test_func)());

namespace {
    void test_arena_basic_allocation() {
        vrutti::core::memory::ArenaAllocator arena(1024);
        void* ptr1 = arena.allocate(100);
        ASSERT_TRUE(ptr1 != nullptr);
        void* ptr2 = arena.allocate(200);
        ASSERT_TRUE(ptr2 != nullptr);
        ASSERT_TRUE(ptr1 != ptr2);
    }

    void test_arena_alignment() {
        vrutti::core::memory::ArenaAllocator arena(1024);
        void* ptr1 = arena.allocate(1, 8);
        ASSERT_EQ(0, reinterpret_cast<uintptr_t>(ptr1) % 8);

        void* ptr2 = arena.allocate(1, 16);
        ASSERT_EQ(0, reinterpret_cast<uintptr_t>(ptr2) % 16);
    }

    void test_arena_large_allocation() {
        vrutti::core::memory::ArenaAllocator arena(1024);
        void* ptr = arena.allocate(2048); // Larger than default block size
        ASSERT_TRUE(ptr != nullptr);
    }

    void test_arena_reset() {
        vrutti::core::memory::ArenaAllocator arena(1024);
        void* ptr1 = arena.allocate(500);
        arena.reset();
        void* ptr2 = arena.allocate(500);
        // ptr2 might or might not be equal to ptr1 depending on implementation,
        // but it definitely shouldn't crash.
        ASSERT_TRUE(ptr2 != nullptr);
    }
}

void run_memory_tests() {
    run_test("ArenaAllocator - Basic Allocation", test_arena_basic_allocation);
    run_test("ArenaAllocator - Alignment", test_arena_alignment);
    run_test("ArenaAllocator - Large Allocation", test_arena_large_allocation);
    run_test("ArenaAllocator - Reset", test_arena_reset);
}
