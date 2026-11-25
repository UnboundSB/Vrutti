#include "ProcessRunner.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace vrutti::core::utils {

namespace {

#ifdef _WIN32
    // Puts a name into a command line the way Windows wants it: the name is
    // put between two quotes, and every backslash that stands in front of a
    // quote is doubled.
    std::wstring quoteArgument(const std::wstring& argument) {
        std::wstring result;
        bool needsQuote = argument.find(L' ') != std::wstring::npos ||
                          argument.find(L'\t') != std::wstring::npos ||
                          argument.find(L'"') != std::wstring::npos ||
                          argument.empty();
        if (needsQuote) result += L'"';
        size_t backslashes = 0;
        for (wchar_t c : argument) {
            if (c == L'\\') {
                ++backslashes;
                continue;
            }
            if (c == L'"') {
                result.append(backslashes * 2 + 1, L'\\');
                backslashes = 0;
                result += L'"';
                continue;
            }
            result.append(backslashes, L'\\');
            backslashes = 0;
            result += c;
        }
        result.append(backslashes * 2, L'\\');
        if (needsQuote) result += L'"';
        return result;
    }

    std::wstring widen(const std::string& text) {
        if (text.empty()) return std::wstring();
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                              static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0) return std::wstring();
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                            out.data(), size);
        return out;
    }
#endif

} // namespace

ProcessResult runProcess(const std::string& executable,
                         const std::vector<std::string>& arguments,
                         size_t maxOutputBytes,
                         unsigned int timeoutMs) {
    ProcessResult result;
    if (executable.empty()) {
        result.error = "the program has no name";
        return result;
    }

#ifdef _WIN32
    std::wstring commandLine = quoteArgument(widen(executable));
    for (const auto& argument : arguments) {
        commandLine += L" ";
        commandLine += quoteArgument(widen(argument));
    }
    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');

    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    SECURITY_ATTRIBUTES attributes = { sizeof(attributes), nullptr, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &attributes, 0)) {
        result.error = "the pipe of the program could not be made";
        return result;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup = { sizeof(startup) };
    startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = hWrite;
    startup.hStdError = hWrite;

    PROCESS_INFORMATION process = { 0 };
    const BOOL started = CreateProcessW(
        nullptr, commandBuffer.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        nullptr, nullptr, &startup, &process);

    CloseHandle(hWrite);

    if (!started) {
        CloseHandle(hRead);
        result.error = "the program could not be started";
        return result;
    }
    result.started = true;

    // The text of the program is read on a thread of its own, so that a
    // program that writes a lot does not fill the pipe and wait for a
    // reader that is not there yet.
    std::thread reader([&]() {
        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(hRead, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
            if (result.output.size() < maxOutputBytes) {
                const size_t room = maxOutputBytes - result.output.size();
                result.output.append(buffer, std::min<size_t>(bytesRead, room));
                if (bytesRead > room) result.truncated = true;
            } else {
                result.truncated = true;
            }
        }
        CloseHandle(hRead);
    });

    if (WaitForSingleObject(process.hProcess, timeoutMs) == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 1000);
        result.exitCode = -1;
        result.error = "the program was stopped: it took too long";
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(process.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
    }

    reader.join();
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
#else
    int fds[2] = { -1, -1 };
    if (pipe(fds) != 0) {
        result.error = "the pipe of the program could not be made";
        return result;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        result.error = "the program could not be started";
        return result;
    }
    if (pid == 0) {
        // The child: the program is started without a shell.
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[0]);
        close(fds[1]);

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(executable.c_str()));
        for (const auto& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);
        execvp(executable.c_str(), argv.data());
        _exit(127);
    }

    close(fds[1]);
    result.started = true;

    std::thread reader([&]() {
        char buffer[4096];
        ssize_t count = 0;
        while ((count = read(fds[0], buffer, sizeof(buffer))) > 0) {
            if (result.output.size() < maxOutputBytes) {
                const size_t room = maxOutputBytes - result.output.size();
                result.output.append(buffer, std::min<size_t>(static_cast<size_t>(count), room));
                if (static_cast<size_t>(count) > room) result.truncated = true;
            } else {
                result.truncated = true;
            }
        }
        close(fds[0]);
    });

    int status = 0;
    bool stopped = false;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeoutMs);
    while (true) {
        const pid_t waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0) break;
        if (std::chrono::steady_clock::now() >= deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            result.exitCode = -1;
            result.error = "the program was stopped: it took too long";
            stopped = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (!stopped) {
        result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    reader.join();
    return result;
#endif
}

} // namespace vrutti::core::utils
