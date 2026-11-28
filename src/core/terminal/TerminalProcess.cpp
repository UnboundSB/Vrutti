#include "TerminalProcess.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <strsafe.h>

// Typedefs for dynamically loading ConPTY APIs
typedef HRESULT (WINAPI *PFNCREATEPSEUDOCONSOLE)(COORD c, HANDLE hIn, HANDLE hOut, DWORD dwFlags, HPCON *phpcon);
typedef void (WINAPI *PFNRESIZEPSEUDOCONSOLE)(HPCON hpc, COORD size);
typedef void (WINAPI *PFNCLOSEPSEUDOCONSOLE)(HPCON hpc);

#endif

namespace vrutti::core::terminal {

    namespace {

        // The folder the shell starts in when no folder is opened.
        //
        // The editor sets its own folder to the place of the program, so
        // that the paths of the program work. The shell of the user must
        // not start there: the user would be in the inside of the editor
        // and would see the sources of the editor instead of their own
        // work. It starts in the root of the drive of the system.
        std::wstring defaultShellFolder() {
#ifdef _WIN32
            // The drive of the system is told by the environment.
            const char* drive = std::getenv("SystemDrive");
            if (drive && *drive) {
                std::wstring out;
                for (const char* p = drive; *p; ++p) {
                    out += static_cast<wchar_t>(*p);
                }
                if (!out.empty() && out.back() != L'\\') out += L'\\';
                return out;
            }

            // The folder of windows is on the drive of the system.
            wchar_t windows[MAX_PATH] = { 0 };
            if (GetWindowsDirectoryW(windows, MAX_PATH)) {
                std::wstring out(windows);
                const size_t slash = out.find(L'\\');
                if (slash != std::wstring::npos) out = out.substr(0, slash + 1);
                return out;
            }
#endif
            return std::wstring();
        }

    } // namespace

    TerminalProcess::TerminalProcess() {
    }

    TerminalProcess::~TerminalProcess() {
        stop();
    }

    bool TerminalProcess::start(const std::string& cwd,
                                OutputCallback callback,
                                std::string& error) {
        if (m_running) return false;
        m_outputCallback = callback;
        error.clear();

#ifdef _WIN32
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!hKernel32) {
            error = "the system does not know the console of windows";
            return false;
        }

        auto pfnCreatePseudoConsole = (PFNCREATEPSEUDOCONSOLE)GetProcAddress(hKernel32, "CreatePseudoConsole");
        if (!pfnCreatePseudoConsole) {
            error = "this version of windows has no console for a program";
            return false;
        }

        // The handles that the shell is given have to be inherited by it.
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        // The pipes of the plain words of the shell.
        //
        // A shell whose words are a pipe stays alive and answers. A shell
        // that is given the words of the editor itself writes them to the
        // editor and leaves: that is what the terminal was doing, and the
        // terminal stayed empty for ever.
        if (!CreatePipe(&m_hChildStd_IN_Rd, &m_hChildStd_IN_Wr, &saAttr, 0) ||
            !CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0)) {
            error = "the pipes of the shell could not be made";
            return false;
        }

        // The pipes of the console of the editor.
        HANDLE hConsoleInRd = NULL, hConsoleOutWr = NULL;
        if (!CreatePipe(&hConsoleInRd, &m_hConsoleIn_Wr, &saAttr, 0) ||
            !CreatePipe(&m_hConsoleOut_Rd, &hConsoleOutWr, &saAttr, 0)) {
            error = "the pipes of the console could not be made";
            return false;
        }

        COORD consoleSize = { (SHORT)m_cols, (SHORT)m_rows };
        HPCON hPC = NULL;
        HRESULT hr = pfnCreatePseudoConsole(consoleSize, hConsoleInRd, hConsoleOutWr, 0, &hPC);
        if (FAILED(hr)) {
            error = "the console of the editor could not be made";
            CloseHandle(hConsoleInRd);
            CloseHandle(m_hConsoleIn_Wr);
            CloseHandle(m_hConsoleOut_Rd);
            CloseHandle(hConsoleOutWr);
            CloseHandle(m_hChildStd_IN_Rd);
            CloseHandle(m_hChildStd_IN_Wr);
            CloseHandle(m_hChildStd_OUT_Rd);
            CloseHandle(m_hChildStd_OUT_Wr);
            return false;
        }
        m_hPC = (void*)hPC;

        // The handles that the editor keeps for itself must not travel to
        // the shell.
        SetHandleInformation(m_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(m_hConsoleIn_Wr, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(m_hConsoleOut_Rd, HANDLE_FLAG_INHERIT, 0);

        // Initialize StartupInfoEx
        STARTUPINFOEXW siEx;
        ZeroMemory(&siEx, sizeof(siEx));
        siEx.StartupInfo.cb = sizeof(siEx);

        SIZE_T attrListSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);
        siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrListSize);
        if (!siEx.lpAttributeList ||
            !InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrListSize)) {
            error = "the console of the editor could not be made";
            if (siEx.lpAttributeList) HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);
            auto pfnClosePseudoConsole = (PFNCLOSEPSEUDOCONSOLE)GetProcAddress(hKernel32, "ClosePseudoConsole");
            if (pfnClosePseudoConsole) pfnClosePseudoConsole((HPCON)m_hPC);
            m_hPC = NULL;
            CloseHandle(hConsoleInRd);
            CloseHandle(m_hConsoleIn_Wr);
            CloseHandle(m_hConsoleOut_Rd);
            CloseHandle(hConsoleOutWr);
            CloseHandle(m_hChildStd_IN_Rd);
            CloseHandle(m_hChildStd_IN_Wr);
            CloseHandle(m_hChildStd_OUT_Rd);
            CloseHandle(m_hChildStd_OUT_Wr);
            return false;
        }

        UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), NULL, NULL);

        // The shell is given the pipes of its words and the console of the
        // editor.
        siEx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
        siEx.StartupInfo.hStdInput = m_hChildStd_IN_Rd;
        siEx.StartupInfo.hStdOutput = m_hChildStd_OUT_Wr;
        siEx.StartupInfo.hStdError = m_hChildStd_OUT_Wr;

        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(piProcInfo));

        // The room of the command has to hold the whole path of the shell:
        // windows writes that path into the room of the command.
        wchar_t cmd[512];
        _snwprintf_s(cmd, sizeof(cmd) / sizeof(cmd[0]), _TRUNCATE,
                     L"powershell.exe -NoLogo");

        // The folder of the workspace. A folder that is not there is not
        // given: the shell then starts in the root of the drive of the
        // system.
        LPCWSTR pCwd = NULL;
        std::wstring wCwd;
        std::string folder = cwd;
        if (!folder.empty()) {
            std::error_code ec;
            std::filesystem::path path(folder);
            if (!std::filesystem::is_directory(path, ec) || ec) folder.clear();
        }
        if (folder.empty()) {
            wCwd = defaultShellFolder();
        } else {
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, folder.data(), (int)folder.size(), NULL, 0);
            wCwd.resize(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, folder.data(), (int)folder.size(), &wCwd[0], size_needed);
        }
        if (!wCwd.empty()) pCwd = wCwd.c_str();

        BOOL bSuccess = CreateProcessW(NULL, cmd, NULL, NULL, TRUE,
                                       EXTENDED_STARTUPINFO_PRESENT, NULL, pCwd,
                                       &siEx.StartupInfo, &piProcInfo);

        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

        // The console keeps the ends of its pipes now.
        CloseHandle(hConsoleInRd);
        CloseHandle(hConsoleOutWr);

        if (!bSuccess) {
            error = "the shell of the editor could not be started";
            auto pfnClosePseudoConsole = (PFNCLOSEPSEUDOCONSOLE)GetProcAddress(hKernel32, "ClosePseudoConsole");
            if (pfnClosePseudoConsole) pfnClosePseudoConsole((HPCON)m_hPC);
            m_hPC = NULL;
            CloseHandle(m_hChildStd_IN_Rd);
            CloseHandle(m_hChildStd_IN_Wr);
            CloseHandle(m_hChildStd_OUT_Rd);
            CloseHandle(m_hChildStd_OUT_Wr);
            CloseHandle(m_hConsoleIn_Wr);
            CloseHandle(m_hConsoleOut_Rd);
            return false;
        }

        m_hProcess = piProcInfo.hProcess;
        CloseHandle(piProcInfo.hThread);

        // The handles of the shell are not needed by the editor anymore.
        CloseHandle(m_hChildStd_IN_Rd);
        CloseHandle(m_hChildStd_OUT_Wr);
        m_hChildStd_IN_Rd = NULL;
        m_hChildStd_OUT_Wr = NULL;

        // Assign to Job Object to ensure child processes are killed when IDE exits or crashes
        m_hJob = CreateJobObject(NULL, NULL);
        if (m_hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(m_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
            AssignProcessToJobObject(m_hJob, m_hProcess);
        }

        m_running = true;
        m_readThread = std::thread(&TerminalProcess::readLoop, this);
        m_writeThread = std::thread(&TerminalProcess::writeLoop, this);
        return true;
#else
        error = "this system has no terminal of the editor yet";
        return false;
#endif
    }

    void TerminalProcess::stop() {
        if (!m_running) return;
        m_running = false;

        // Wake the writer up.
        m_writeSignal.notify_all();

#ifdef _WIN32
        // The shell is taken away with everything it started.
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 0);
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }

        if (m_hJob) {
            CloseHandle(m_hJob);
            m_hJob = NULL;
        }

        if (m_hPC) {
            HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
            if (hKernel32) {
                auto pfnClosePseudoConsole = (PFNCLOSEPSEUDOCONSOLE)GetProcAddress(hKernel32, "ClosePseudoConsole");
                if (pfnClosePseudoConsole) pfnClosePseudoConsole((HPCON)m_hPC);
            }
            m_hPC = NULL;
        }
#endif

        // The pipes are closed, so that the reader and the writer do not
        // wait for ever.
#ifdef _WIN32
        if (m_hChildStd_IN_Wr) {
            CloseHandle(m_hChildStd_IN_Wr);
            m_hChildStd_IN_Wr = NULL;
        }
        if (m_hChildStd_OUT_Rd) {
            CloseHandle(m_hChildStd_OUT_Rd);
            m_hChildStd_OUT_Rd = NULL;
        }
        if (m_hConsoleIn_Wr) {
            CloseHandle(m_hConsoleIn_Wr);
            m_hConsoleIn_Wr = NULL;
        }
        if (m_hConsoleOut_Rd) {
            CloseHandle(m_hConsoleOut_Rd);
            m_hConsoleOut_Rd = NULL;
        }
#endif

        if (m_writeThread.joinable()) {
            m_writeThread.join();
        }
        if (m_readThread.joinable()) {
            m_readThread.join();
        }
    }

    void TerminalProcess::writeInput(const std::string& input) {
        if (!m_running || input.empty()) return;
        {
            std::lock_guard<std::mutex> lock(m_writeMutex);
            m_writeQueue.push_back(input);
        }
        m_writeSignal.notify_one();
    }

    void TerminalProcess::resize(int cols, int rows) {
        m_cols = cols;
        m_rows = rows;
#ifdef _WIN32
        if (m_hPC) {
            HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
            if (hKernel32) {
                auto pfnResizePseudoConsole = (PFNRESIZEPSEUDOCONSOLE)GetProcAddress(hKernel32, "ResizePseudoConsole");
                if (pfnResizePseudoConsole) {
                    COORD size;
                    size.X = (SHORT)cols;
                    size.Y = (SHORT)rows;
                    pfnResizePseudoConsole((HPCON)m_hPC, size);
                }
            }
        }
#endif
    }

    // Reads the words of the shell.
    //
    // The words come from two places: the pipe of the plain words and the
    // console of the editor. Both are watched, so that the page gets
    // everything the shell says.
    void TerminalProcess::readLoop() {
#ifdef _WIN32
        while (m_running) {
            HANDLE handles[2] = { m_hChildStd_OUT_Rd, m_hConsoleOut_Rd };

            if (!handles[0] || !handles[1]) break;

            // The wait is short, so that the reader notices that the
            // terminal was closed.
            WaitForMultipleObjects(2, handles, FALSE, 100);

            for (int i = 0; i < 2; i++) {
                DWORD available = 0;
                if (!PeekNamedPipe(handles[i], nullptr, 0, nullptr, &available, nullptr)) {
                    // The pipe is closed: the shell is gone.
                    if (i == 0) m_hChildStd_OUT_Rd = nullptr;
                    else m_hConsoleOut_Rd = nullptr;
                    continue;
                }
                if (available == 0) continue;
                if (available > 65536) available = 65536;

                std::string buffer;
                buffer.resize(available);
                DWORD read = 0;
                if (!ReadFile(handles[i], buffer.data(), available, &read, nullptr)) {
                    continue;
                }
                buffer.resize(read);
                if (!buffer.empty() && m_outputCallback) {
                    m_outputCallback(buffer);
                }
            }
        }
#endif
    }

    // Sends the keys of the user to the shell.
    //
    // The keys are sent on a thread of their own: a shell that does not
    // read them must not hold the editor.
    void TerminalProcess::writeLoop() {
#ifdef _WIN32
        while (m_running) {
            std::string text;
            {
                std::unique_lock<std::mutex> lock(m_writeMutex);
                if (m_writeQueue.empty()) {
                    m_writeSignal.wait_for(lock, std::chrono::milliseconds(100));
                    continue;
                }
                text = m_writeQueue.front();
                m_writeQueue.pop_front();
            }

            DWORD written = 0;
            if (m_hChildStd_IN_Wr) {
                WriteFile(m_hChildStd_IN_Wr, text.data(), (DWORD)text.size(), &written, nullptr);
            }
            // The console of the editor gets the keys as well, so that the
            // programs of the console hear them too.
            if (m_hConsoleIn_Wr) {
                WriteFile(m_hConsoleIn_Wr, text.data(), (DWORD)text.size(), &written, nullptr);
            }
        }
#endif
    }

} // namespace vrutti::core::terminal
