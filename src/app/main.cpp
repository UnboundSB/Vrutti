#include <iostream>
#include <thread>
#include <chrono>
#include "ui/compositor/Window.h"
#include "core/ipc/IPCClient.h"

int main(int argc, char* argv[]) {
    std::cout << "--- Vrutti IDE Core ---" << std::endl;
    
    std::string initialWorkspace = "";
    if (argc > 1) {
        initialWorkspace = argv[1];
    }
    
    // Initialize IPC Client for Extension Host
    // In a real app we might pass a dynamic pipe name or start the Node host here
    vrutti::core::ipc::IPCClient ipc("vrutti_pipe");
    ipc.start();

    // Launch UI
    vrutti::ui::Window window(1280, 720, "Vrutti IDE", &ipc, initialWorkspace);
    if (!window.init()) {
        std::cerr << "Failed to initialize UI window!" << std::endl;
        return 1;
    }
    // Start a background thread to simulate streaming logs to the frontend
    std::thread log_streamer([&window]() {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        window.logToOutput("System", "[System] C++ Native Engine connected to frontend successfully.");
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        window.logToOutput("Tasks", "[Tasks] Build daemon is idle.");
    });

    // Run the main IDE render loop
    window.run();

    log_streamer.detach();

    return 0;
}
