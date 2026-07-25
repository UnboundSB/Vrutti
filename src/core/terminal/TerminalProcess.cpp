#include "TerminalProcess.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <strsafe.h>

// Typedefs for dynamically loading ConPTY APIs (in case we need to build against older SDKs, but here we assume modern SDK is used, but dynamically load for safety/compatibility)
typedef HRESULT (WINAPI *PFNCREATEPSEUDOCONSOLE)(COORD c, HANDLE hIn, HANDLE hOut, DWORD dwFlags, HPCON *phpcon);
typedef void (WINAPI *PFNRESIZEPSEUDOCONSOLE)(HPCON hpc, COORD size);
typedef void (WINAPI *PFNCLOSEPSEUDOCONSOLE)(HPCON hpc);

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
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (!hKernel32) return false;

        auto pfnCreatePseudoConsole = (PFNCREATEPSEUDOCONSOLE)GetProcAddress(hKernel32, "CreatePseudoConsole");
        if (!pfnCreatePseudoConsole) return false;

        HANDLE hPipePTYInRd = NULL, hPipePTYInWr = NULL;
        HANDLE hPipePTYOutRd = NULL, hPipePTYOutWr = NULL;
        SECURITY_ATTRIBUTES saAttr; 
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
        saAttr.bInheritHandle = FALSE; 
        saAttr.lpSecurityDescriptor = NULL; 

        if (!CreatePipe(&hPipePTYInRd, &m_hChildStd_IN_Wr, &saAttr, 0)) return false;
        if (!CreatePipe(&m_hChildStd_OUT_Rd, &hPipePTYOutWr, &saAttr, 0)) {
            CloseHandle(hPipePTYInRd);
            CloseHandle(m_hChildStd_IN_Wr);
            return false;
        }

        COORD consoleSize = { 120, 30 };
        HPCON hPC = NULL;
        HRESULT hr = pfnCreatePseudoConsole(consoleSize, hPipePTYInRd, hPipePTYOutWr, 0, &hPC);
        if (FAILED(hr)) {
            CloseHandle(hPipePTYInRd);
            CloseHandle(m_hChildStd_IN_Wr);
            CloseHandle(m_hChildStd_OUT_Rd);
            CloseHandle(hPipePTYOutWr);
            return false;
        }
        m_hPC = (void*)hPC;

        // Initialize StartupInfoEx
        STARTUPINFOEXW siEx;
        ZeroMemory(&siEx, sizeof(STARTUPINFOEXW));
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);

        SIZE_T attrListSize = 0;
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);
        siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, attrListSize);
        if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrListSize)) {
            // error
        }

        UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), NULL, NULL);

        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        wchar_t cmd[] = L"powershell.exe";
        BOOL bSuccess = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &siEx.StartupInfo, &piProcInfo);
        
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        HeapFree(GetProcessHeap(), 0, siEx.lpAttributeList);

        CloseHandle(hPipePTYInRd);
        CloseHandle(hPipePTYOutWr);

        if (!bSuccess) {
            auto pfnClosePseudoConsole = (PFNCLOSEPSEUDOCONSOLE)GetProcAddress(hKernel32, "ClosePseudoConsole");
            if (pfnClosePseudoConsole) pfnClosePseudoConsole((HPCON)m_hPC);
            return false;
        }

        m_hProcess = piProcInfo.hProcess;
        CloseHandle(piProcInfo.hThread);

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

        if (m_hPC) {
            HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
            if (hKernel32) {
                auto pfnClosePseudoConsole = (PFNCLOSEPSEUDOCONSOLE)GetProcAddress(hKernel32, "ClosePseudoConsole");
                if (pfnClosePseudoConsole) pfnClosePseudoConsole((HPCON)m_hPC);
            }
            m_hPC = NULL;
        }
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
#ifdef _WIN32
        if (m_hPC) {
            HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
            if (hKernel32) {
                auto pfnResizePseudoConsole = (PFNRESIZEPSEUDOCONSOLE)GetProcAddress(hKernel32, "ResizePseudoConsole");
                if (pfnResizePseudoConsole) {
                    COORD size;
                    size.X = cols;
                    size.Y = rows;
                    pfnResizePseudoConsole((HPCON)m_hPC, size);
                }
            }
        }
#endif
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
