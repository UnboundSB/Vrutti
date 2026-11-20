#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include "ui/compositor/Window.h"
#include "core/ipc/IPCClient.h"
#include "core/ipc/VSCodeProtocol.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
    // Unbuffered stdout: the diagnostics of the extension host handshake are
    // worthless if they sit in a buffer when the process is terminated.
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "--- Vrutti IDE Core ---" << std::endl;
    
    std::string initialWorkspace;
    if (argc > 1) {
        std::filesystem::path wp(argv[1]);
        initialWorkspace = std::filesystem::absolute(wp).string();
    } else {
        initialWorkspace = "";
    }
    
#ifdef _WIN32
    // Fix CWD to the Vrutti installation / development root so relative paths work
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    std::filesystem::path basePath = exePath.parent_path();
    if (basePath.filename() == "build") {
        basePath = basePath.parent_path();
    }
    SetCurrentDirectoryA(basePath.string().c_str());
#endif

    // Initialize IPC Client for Extension Host (per-instance pipe to avoid collisions)
    std::string pipeName = "vrutti_pipe_" + std::to_string(GetCurrentProcessId());
    vrutti::core::ipc::IPCClient ipc(pipeName);

    // Wire up VSCodeProtocol. The protocol layer owns the extension host
    // handshake, so it must be initialised *before* the listener starts --
    // otherwise the "ready" frame can arrive before the handler is registered.
    vrutti::core::ipc::VSCodeProtocol vsProtocol(&ipc);
    vsProtocol.setWorkspaceFolder(initialWorkspace);
    vsProtocol.initialize();

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

    // Resolve node.exe and bootstrapper.js dynamically from the executable directory
    // (exePath and basePath already computed above for CWD fix)
    std::string nodePath = (basePath / "src" / "ext" / "bin" / "node.exe").string();
    std::string bootstrapperPath = (basePath / "src" / "ext" / "bootstrapper.js").string();

    std::string cmd = "\"" + nodePath + "\" --expose-gc \"" + bootstrapperPath +
                      "\" --pipe=\\\\.\\pipe\\" + pipeName;
    std::vector<char> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back('\0');
    
    std::cout << "[Core] Spawning Node.js Extension Host..." << std::endl;
    if (!CreateProcessA(NULL, cmdBuffer.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
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

    // Run the main IDE render loop
    window.run();

    return 0;
}
