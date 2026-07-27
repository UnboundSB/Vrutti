#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vrutti::core::terminal {

    class ReplProcess {
    public:
        using OutputCallback = std::function<void(const std::string&)>;

        ReplProcess();
        ~ReplProcess();

        bool start(OutputCallback callback);
        void stop();
        void evaluate(const std::string& input);

    private:
        void readLoop();
        void readErrLoop();

        std::atomic<bool> m_running{false};
        std::thread m_readThread;
        std::thread m_readErrThread;
        OutputCallback m_outputCallback;

#ifdef _WIN32
        HANDLE m_hChildStd_IN_Wr = NULL;
        HANDLE m_hChildStd_OUT_Rd = NULL;
        HANDLE m_hChildStd_ERR_Rd = NULL;
        HANDLE m_hProcess = NULL;
        HANDLE m_hJob = NULL;
#endif
    };

} // namespace vrutti::core::terminal
