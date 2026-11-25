#include "ProcessRunner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
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

// The most parts a command line may have, and the most letters of one part.
constexpr size_t kMaxCommandParts = 64;
constexpr size_t kMaxPartLength = 4096;

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

std::vector<std::string> splitCommandLine(const std::string& text) {
    std::vector<std::string> parts;

    std::string current;
    bool inQuotes = false;
    bool started = false;

    for (size_t i = 0; i < text.size(); i++) {
        const char c = text[i];

        if (c == '"') {
            // The quotes only group the text; they are not part of it.
            inQuotes = !inQuotes;
            started = true;
            continue;
        }

        if (c == '\\' && inQuotes && i + 1 < text.size() && text[i + 1] == '"') {
            // A backslash in front of a quote keeps the quote inside the
            // part. A backslash in front of anything else is a backslash:
            // the name of a folder of Windows is full of them.
            current += '"';
            started = true;
            i++;
            if (current.size() > kMaxPartLength) return {};
            continue;
        }

        if (!inQuotes && (c == ' ' || c == '\t' || c == '\r' || c == '\n')) {
            if (started) {
                parts.push_back(current);
                current.clear();
                started = false;
                if (parts.size() > kMaxCommandParts) return {};
            }
            continue;
        }

        current += c;
        started = true;
        if (current.size() > kMaxPartLength) return {};
    }

    if (started) {
        parts.push_back(current);
        if (parts.size() > kMaxCommandParts) return {};
    }

    return parts;
}

ProcessResult runProcess(const std::string& executable,
                         const std::vector<std::string>& arguments,
                         size_t maxOutputBytes,
                         unsigned int timeoutMs,
                         const std::string& workingDirectory) {
    ProcessResult result;
    if (executable.empty()) {
        result.error = "the program has no name";
        return result;
    }

    // The folder the program is run in. A folder that is not there is a
    // failure of its own: the program would otherwise run in the folder of
    // the editor and would touch the wrong files.
    std::string folder = workingDirectory;
    if (!folder.empty()) {
        std::error_code ec;
        if (!std::filesystem::is_directory(folder, ec) || ec) {
            result.error = "the folder of the program is not there";
            return result;
        }
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
    std::wstring wideFolder;
    LPCWSTR currentFolder = nullptr;
    if (!folder.empty()) {
        wideFolder = widen(folder);
        currentFolder = wideFolder.c_str();
    }

    // The program is put in a job of its own, so that a program that is
    // stopped takes its children with it. A program that leaves a child
    // behind keeps the pipe of the text open, and the editor would wait for
    // a child that nobody is going to read.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = { };
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                &limits, sizeof(limits));
    }

    const BOOL started = CreateProcessW(
        nullptr, commandBuffer.data(),
        nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        nullptr, currentFolder, &startup, &process);

    CloseHandle(hWrite);

    if (!started) {
        CloseHandle(hRead);
        if (job) CloseHandle(job);
        result.error = "the program could not be started";
        return result;
    }
    result.started = true;

    if (job && !AssignProcessToJobObject(job, process.hProcess)) {
        // The program is already in a job of another program and may not be
        // put in ours. It is still stopped, but its children may stay.
        CloseHandle(job);
        job = nullptr;
    }

    // The text of the program is read on a thread of its own, so that a
    // program that writes a lot does not fill the pipe and wait for a
    // reader that is not there yet.
    std::atomic<bool> stop{ false };
    std::atomic<bool> done{ false };
    std::thread reader([&]() {
        char buffer[4096];
        DWORD bytesRead = 0;
        while (!stop.load()) {
            // The pipe is asked what it has, so that the reader may stop
            // even when the program keeps the pipe open with a child of
            // its own.
            DWORD available = 0;
            if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &available, nullptr)) {
                break; // the pipe is closed
            }
            if (available == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }
            if (!ReadFile(hRead, buffer, sizeof(buffer), &bytesRead, nullptr)) {
                break;
            }
            if (bytesRead == 0) break;
            // The text of the program is kept inside of its limit: a
            // program that writes without end would use the memory of the
            // editor until there is none left.
            if (result.output.size() < maxOutputBytes) {
                const size_t room = maxOutputBytes - result.output.size();
                result.output.append(buffer, std::min<size_t>(bytesRead, room));
                if (bytesRead > room) result.truncated = true;
            } else {
                result.truncated = true;
            }
        }
        CloseHandle(hRead);
        done.store(true);
    });

    if (WaitForSingleObject(process.hProcess, timeoutMs) == WAIT_TIMEOUT) {
        // The program and everything it started are stopped.
        if (job) {
            TerminateJobObject(job, 1);
        } else {
            TerminateProcess(process.hProcess, 1);
        }
        WaitForSingleObject(process.hProcess, 1000);
        result.exitCode = -1;
        result.error = "the program was stopped: it took too long";
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(process.hProcess, &exitCode);
        result.exitCode = static_cast<int>(exitCode);
    }

    // The reader is given a moment to read what is left in the pipe. A
    // child that kept the pipe open does not make the editor wait: after a
    // while the reader is asked to stop.
    for (int i = 0; i < 100 && !done.load(); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    stop.store(true);
    reader.join();
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (job) CloseHandle(job);
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
        if (!folder.empty()) {
            if (chdir(folder.c_str()) != 0) {
                _exit(126);
            }
        }
        // The program is the head of a group of its own, so that a program
        // that is stopped takes its children with it.
        setpgid(0, 0);
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

    // The group of the program is made here as well: the child may not have
    // done it yet when the parent gets here.
    setpgid(pid, pid);

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
            // The program and everything it started are stopped.
            kill(-pid, SIGKILL);
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
