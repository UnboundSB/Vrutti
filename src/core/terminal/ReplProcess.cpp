#include "ReplProcess.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vrutti::core::terminal {

    ReplProcess::ReplProcess() {
    }

    ReplProcess::~ReplProcess() {
        stop();
    }

    bool ReplProcess::start(OutputCallback callback) {
        if (m_running) return false;
        m_outputCallback = callback;

#ifdef _WIN32
        HANDLE hChildStd_IN_Rd = NULL;
        HANDLE hChildStd_OUT_Wr = NULL;
        HANDLE hChildStd_ERR_Wr = NULL;

        SECURITY_ATTRIBUTES saAttr; 
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
        saAttr.bInheritHandle = TRUE; 
        saAttr.lpSecurityDescriptor = NULL; 

        if (!CreatePipe(&hChildStd_IN_Rd, &m_hChildStd_IN_Wr, &saAttr, 0)) return false;
        SetHandleInformation(m_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

        if (!CreatePipe(&m_hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
            CloseHandle(hChildStd_IN_Rd);
            CloseHandle(m_hChildStd_IN_Wr);
            m_hChildStd_IN_Wr = NULL;
            return false;
        }
        SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

        if (!CreatePipe(&m_hChildStd_ERR_Rd, &hChildStd_ERR_Wr, &saAttr, 0)) {
            CloseHandle(hChildStd_IN_Rd);
            CloseHandle(m_hChildStd_IN_Wr);
            CloseHandle(m_hChildStd_OUT_Rd);
            CloseHandle(hChildStd_OUT_Wr);
            m_hChildStd_IN_Wr = NULL;
            m_hChildStd_OUT_Rd = NULL;
            return false;
        }
        SetHandleInformation(m_hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW siStartInfo;
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
        siStartInfo.cb = sizeof(STARTUPINFOW);
        siStartInfo.hStdError = hChildStd_ERR_Wr;
        siStartInfo.hStdOutput = hChildStd_OUT_Wr;
        siStartInfo.hStdInput = hChildStd_IN_Rd;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        siStartInfo.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        // Use node -i for interactive REPL
        wchar_t cmd[] = L"node -i";

        BOOL bSuccess = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &siStartInfo, &piProcInfo);
        
        CloseHandle(hChildStd_IN_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_ERR_Wr);

        if (!bSuccess) {
            CloseHandle(m_hChildStd_IN_Wr);
            CloseHandle(m_hChildStd_OUT_Rd);
            CloseHandle(m_hChildStd_ERR_Rd);
            m_hChildStd_IN_Wr = NULL;
            m_hChildStd_OUT_Rd = NULL;
            m_hChildStd_ERR_Rd = NULL;
            return false;
        }

        m_hProcess = piProcInfo.hProcess;
        CloseHandle(piProcInfo.hThread);

        m_hJob = CreateJobObject(NULL, NULL);
        if (m_hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(m_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
            AssignProcessToJobObject(m_hJob, m_hProcess);
        }

        m_running = true;
        m_readThread = std::thread(&ReplProcess::readLoop, this);
        m_readErrThread = std::thread(&ReplProcess::readErrLoop, this);
        return true;
#else
        return false;
#endif
    }

    void ReplProcess::stop() {
        if (!m_running) return;
        m_running = false;
        
#ifdef _WIN32
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 0);
            CloseHandle(m_hProcess);
            m_hProcess = NULL;
        }

        if (m_hJob) {
            CloseHandle(m_hJob);
            m_hJob = NULL;
        }

        if (m_hChildStd_IN_Wr) {
            CloseHandle(m_hChildStd_IN_Wr);
            m_hChildStd_IN_Wr = NULL;
        }
        if (m_hChildStd_OUT_Rd) {
            CloseHandle(m_hChildStd_OUT_Rd);
            m_hChildStd_OUT_Rd = NULL;
        }
        if (m_hChildStd_ERR_Rd) {
            CloseHandle(m_hChildStd_ERR_Rd);
            m_hChildStd_ERR_Rd = NULL;
        }
#endif

        if (m_readThread.joinable()) m_readThread.join();
        if (m_readErrThread.joinable()) m_readErrThread.join();
    }

    void ReplProcess::evaluate(const std::string& input) {
        if (!m_running) return;
        std::string full_input = input + "\n";
#ifdef _WIN32
        DWORD dwWritten;
        WriteFile(m_hChildStd_IN_Wr, full_input.c_str(), full_input.size(), &dwWritten, NULL);
#endif
    }

    void ReplProcess::readLoop() {
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

    void ReplProcess::readErrLoop() {
#ifdef _WIN32
        DWORD dwRead;
        CHAR chBuf[4096];
        bool bSuccess = false;

        while (m_running) {
            bSuccess = ReadFile(m_hChildStd_ERR_Rd, chBuf, 4096 - 1, &dwRead, NULL);
            if (!bSuccess || dwRead == 0) break;
            
            chBuf[dwRead] = '\0';
            std::string output(chBuf, dwRead);
            if (m_outputCallback) {
                // Let's just prefix it with [ERROR] or something if needed, but for now just send it.
                m_outputCallback(output);
            }
        }
#endif
    }

} // namespace vrutti::core::terminal
