#include <iostream>
#include "core/memory/ArenaAllocator.h"

int main() {
    std::cout << "Vrutti IDE Core Initialization..." << std::endl;

    // Test the low-level foundation
    vrutti::core::memory::ArenaAllocator arena(4096);
    int* testValue = arena.allocateObject<int>(42);

    if (testValue && *testValue == 42) {
        std::cout << "[SUCCESS] Core memory allocator initialized correctly." << std::endl;
    } else {
        std::cout << "[ERROR] Core memory allocator failure." << std::endl;
    }

    return 0;
}
