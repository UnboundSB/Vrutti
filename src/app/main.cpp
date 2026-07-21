#include <iostream>
#include "ui/compositor/Window.h"
#include "core/ipc/IPCClient.h"

int main() {
    std::cout << "--- Vrutti IDE Core ---" << std::endl;
    
    // Initialize IPC Client for Extension Host
    // In a real app we might pass a dynamic pipe name or start the Node host here
    vrutti::core::ipc::IPCClient ipc("vrutti_pipe");
    ipc.start();

    // Launch UI
    vrutti::ui::Window window(1280, 720, "Vrutti IDE", &ipc);
    if (!window.init()) {
        std::cerr << "Failed to initialize UI window!" << std::endl;
        return 1;
    }

    // Run the main IDE render loop
    window.run();

    return 0;
}
