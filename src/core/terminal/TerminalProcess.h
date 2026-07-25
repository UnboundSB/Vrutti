#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vrutti::core::terminal {

    class TerminalProcess {
    public:
        using OutputCallback = std::function<void(const std::string&)>;

        TerminalProcess();
        ~TerminalProcess();

        bool start(const std::string& cwd, OutputCallback callback);
        void stop();
        void writeInput(const std::string& input);
        void resize(int cols, int rows);

    private:
        int m_cols = 120;
        int m_rows = 30;
        void readLoop();

        std::atomic<bool> m_running{false};
        std::thread m_readThread;
        OutputCallback m_outputCallback;

#ifdef _WIN32
        HANDLE m_hChildStd_IN_Wr = NULL;
        HANDLE m_hChildStd_OUT_Rd = NULL;
        HANDLE m_hProcess = NULL;
        void* m_hPC = NULL; // HPCON type-erased
#endif
    };

} // namespace vrutti::core::terminal
