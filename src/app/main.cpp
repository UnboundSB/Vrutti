#include <iostream>
#include "core/memory/ArenaAllocator.h"
#include "core/concurrency/ThreadPool.h"

int main() {
    std::cout << "Vrutti IDE Core Initialization..." << std::endl;

    // Test Phase 1: Memory Foundation
    vrutti::core::memory::ArenaAllocator arena(4096);
    int* testValue = arena.allocateObject<int>(42);

    if (testValue && *testValue == 42) {
        std::cout << "[SUCCESS] Core memory allocator initialized correctly." << std::endl;
    } else {
        std::cout << "[ERROR] Core memory allocator failure." << std::endl;
    }

    // Test Phase 2: Concurrency Foundation
    vrutti::core::concurrency::ThreadPool threadPool(4);
    auto futureResult = threadPool.enqueue([](int a, int b) {
        return a + b;
    }, 10, 32);

    if (futureResult.get() == 42) {
        std::cout << "[SUCCESS] Thread pool dispatched and returned correctly." << std::endl;
    } else {
        std::cout << "[ERROR] Thread pool failure." << std::endl;
    }

    return 0;
}
