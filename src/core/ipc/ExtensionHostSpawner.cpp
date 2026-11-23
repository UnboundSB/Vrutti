#include "ExtensionHostSpawner.h"
#include "../utils/Paths.h"

#include <cstdlib>
#include <iostream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace vrutti::core::ipc {

std::string ExtensionHostSpawner::pipePath(const std::string& pipeName) {
#ifdef _WIN32
    return "\\\\.\\pipe\\" + pipeName;
#else
    return pipeName;
#endif
}

std::filesystem::path ExtensionHostSpawner::nodePath() {
    // The runtime that travels with the installation. When it is not there,
    // the node of the system is used.
    const auto bundled = vrutti::core::utils::appResourcePath("src/ext/bin/node.exe");
    std::error_code error;
    if (std::filesystem::exists(bundled, error)) return bundled;

#ifdef _WIN32
    return std::filesystem::path("node.exe");
#else
    return std::filesystem::path("node");
#endif
}

std::filesystem::path ExtensionHostSpawner::bootstrapperPath() {
    return vrutti::core::utils::appResourcePath("src/ext/bootstrapper.js");
}

bool ExtensionHostSpawner::spawn(const std::string& pipeName, std::string& error) {
    const auto node = nodePath();
    const auto bootstrapper = bootstrapperPath();

    std::error_code fsError;
    if (!std::filesystem::exists(bootstrapper, fsError)) {
        error = "the bootstrapper of the extension host is missing: " +
                bootstrapper.string();
        return false;
    }

    const std::string pipe = pipePath(pipeName);

#ifdef _WIN32
    STARTUPINFOA startup;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    // The console of the host is hidden: the editor has its own.
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION process;
    ZeroMemory(&process, sizeof(process));

    // The command line is built with quotes around the paths, so that a path
    // with a space in it is still one argument. The pipe is the last
    // argument and it is never put into a shell.
    const std::string command = "\"" + node.string() + "\" --expose-gc \"" +
                                bootstrapper.string() + "\" --pipe=" + pipe;
    std::vector<char> buffer(command.begin(), command.end());
    buffer.push_back('\0');

    if (!CreateProcessA(nullptr, buffer.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        nullptr, &startup, &process)) {
        error = "the extension host could not be started (error " +
                std::to_string(GetLastError()) + ")";
        return false;
    }

    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    std::cout << "[Core] Spawning Node.js Extension Host..." << std::endl;
    return true;
#else
    const pid_t pid = fork();
    if (pid < 0) {
        error = "the extension host could not be started";
        return false;
    }
    if (pid == 0) {
        const std::string nodePath = node.string();
        const std::string bootstrapperPath = bootstrapper.string();
        const std::string pipeArgument = "--pipe=" + pipe;
        const char* argv[] = {nodePath.c_str(), "--expose-gc", bootstrapperPath.c_str(),
                              pipeArgument.c_str(), nullptr};
        execv(nodePath.c_str(), const_cast<char* const*>(argv));
        _exit(127);
    }
    std::cout << "[Core] Spawning Node.js Extension Host..." << std::endl;
    return true;
#endif
}

} // namespace vrutti::core::ipc
