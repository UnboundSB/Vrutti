#include <iostream>
#include <cassert>
#include <stdexcept>
#include <thread>
#include <chrono>
#include "../src/core/concurrency/Async.h"
#include "../src/core/concurrency/CancellationToken.h"
#include "../src/core/events/Event.h"

int main() {
    using namespace vrutti::core::concurrency;
    
    // 1. Test Throttler exception suppression
    Throttler throttler;
    bool secondTaskRan = false;
    
    throttler.queue([]() {
        throw std::runtime_error("Simulated crash");
    });
    
    throttler.queue([&secondTaskRan]() {
        secondTaskRan = true;
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(secondTaskRan && "Throttler stopped running after exception!");
    
    // 2. Test CancellationToken Use-After-Free
    std::shared_ptr<CancellationToken> token;
    {
        CancellationTokenSource source;
        token = source.token();
        // source goes out of scope here
    }
    
    // If the Use-After-Free bug existed, this would read dangling memory
    bool cancelled = token->isCancellationRequested();
    assert(!cancelled && "Token read should be safe even after source is destroyed");
    
    std::cout << "Concurrency tests passed!\n";
    return 0;
}
