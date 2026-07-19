#include <iostream>
#include "core/memory/ArenaAllocator.h"
#include "core/concurrency/ThreadPool.h"
#include "core/events/Event.h"
#include <string>

void runEventSystemTests() {
    std::cout << "[TEST] Running Event System Tests..." << std::endl;
    vrutti::core::events::Emitter<std::string> onTextChanged;
    
    // Secure subscription using DisposableStore
    vrutti::core::memory::DisposableStore store;
    
    int callCount = 0;
    store.add(onTextChanged([&callCount](const std::string& text) {
        if (text == "secure") callCount++;
    }));

    onTextChanged.fire("secure"); // Should increase callCount
    onTextChanged.fire("ignored"); // Should not increase

    // Test disposal (unsubscribe)
    store.dispose();
    onTextChanged.fire("secure"); // Should NOT increase because it's disposed

    if (callCount == 1) {
        std::cout << "[SUCCESS] Event system subscriptions and safe disposal verified." << std::endl;
    } else {
        std::cout << "[ERROR] Event system failed security/disposal checks." << std::endl;
    }
}

int main() {
    std::cout << "Vrutti IDE Core Initialization..." << std::endl;

    runEventSystemTests();

    // TODO: Initialize core subsystems (Memory, Concurrency, VFS)
    // TODO: Launch application window and compositor

    return 0;
}
