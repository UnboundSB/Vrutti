#include "TerminalProcess.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vrutti::core::terminal {

    TerminalProcess::TerminalProcess() {
    }

    TerminalProcess::~TerminalProcess() {
        stop();
    }

    bool TerminalProcess::start(OutputCallback callback) {
        if (m_running) return false;
        m_outputCallback = callback;

#ifdef _WIN32
        SECURITY_ATTRIBUTES saAttr; 
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
        saAttr.bInheritHandle = TRUE; 
        saAttr.lpSecurityDescriptor = NULL; 

        if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0)) return false;
        if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) return false;

        if (!CreatePipe(&m_hChildStd_IN_Rd, &m_hChildStd_IN_Wr, &saAttr, 0)) return false;
        if (!SetHandleInformation(m_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) return false;

        PROCESS_INFORMATION piProcInfo; 
        STARTUPINFO siStartInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO); 
        siStartInfo.hStdError = m_hChildStd_OUT_Wr;
        siStartInfo.hStdOutput = m_hChildStd_OUT_Wr;
        siStartInfo.hStdInput = m_hChildStd_IN_Rd;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        char cmd[] = "cmd.exe";
        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, (STARTUPINFOA*)&siStartInfo, &piProcInfo)) {
            return false;
        }

        m_hProcess = piProcInfo.hProcess;
        CloseHandle(piProcInfo.hThread);
        
        // Close handles to the write end of stdout and read end of stdin,
        // because they belong to the child process.
        CloseHandle(m_hChildStd_OUT_Wr);
        m_hChildStd_OUT_Wr = NULL;
        CloseHandle(m_hChildStd_IN_Rd);
        m_hChildStd_IN_Rd = NULL;

        m_running = true;
        m_readThread = std::thread(&TerminalProcess::readLoop, this);
        return true;
#else
        return false;
#endif
    }

    void TerminalProcess::stop() {
        if (!m_running) return;
        m_running = false;
        
#ifdef _WIN32
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 0);
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }
        if (m_hChildStd_IN_Wr) CloseHandle(m_hChildStd_IN_Wr);
        if (m_hChildStd_OUT_Rd) CloseHandle(m_hChildStd_OUT_Rd);
#endif

        if (m_readThread.joinable()) {
            m_readThread.join();
        }
    }

    void TerminalProcess::writeInput(const std::string& input) {
        if (!m_running) return;
#ifdef _WIN32
        DWORD dwWritten;
        WriteFile(m_hChildStd_IN_Wr, input.c_str(), input.size(), &dwWritten, NULL);
#endif
    }

    void TerminalProcess::resize(int cols, int rows) {
        // Pseudo-terminal resize not easily supported with simple pipes.
        // Requires ConPTY for Windows.
    }

    void TerminalProcess::readLoop() {
#ifdef _WIN32
        DWORD dwRead;
        CHAR chBuf[4096];
        bool bSuccess = false;

        while (m_running) {
            bSuccess = ReadFile(m_hChildStd_OUT_Rd, chBuf, 4096 - 1, &dwRead, NULL);
            if (!bSuccess || dwRead == 0) break;
            
            chBuf[dwRead] = '\0';
            std::string output(chBuf, dwRead);
            if (m_outputCallback) {
                m_outputCallback(output);
            }
        }
#endif
    }

} // namespace vrutti::core::terminal
