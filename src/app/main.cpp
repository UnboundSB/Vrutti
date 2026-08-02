#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include "ui/compositor/Window.h"
#include "core/ipc/IPCClient.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    std::cout << "--- Vrutti IDE Core ---" << std::endl;
    
    std::string initialWorkspace = "";
    if (argc > 1) {
        initialWorkspace = argv[1];
    }
    
    // Initialize IPC Client for Extension Host
    vrutti::core::ipc::IPCClient ipc("vrutti_pipe");
    ipc.start();

#ifdef _WIN32
    // Auto-spawn the Node.js Extension Host
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    // Hide the console window for the node process
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));

    // Use the bundled node.exe
    std::string cmd = "src\\ext\\bin\\node.exe src/ext/bootstrapper.js --pipe=\\\\.\\pipe\\vrutti_pipe";
    
    std::cout << "[Core] Spawning Node.js Extension Host..." << std::endl;
    if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        std::cerr << "[Core] Failed to spawn Node.js Extension Host! Error: " << GetLastError() << std::endl;
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#endif

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
