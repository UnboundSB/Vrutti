#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vrutti::core::terminal {

    class TerminalProcess {
    public:
        using OutputCallback = std::function<void(const std::string&)>;

        TerminalProcess();
        ~TerminalProcess();

        // Starts a shell.
        //
        // The reason of a failure is given back, so that the page may say
        // what went wrong instead of showing a terminal that stays empty.
        bool start(const std::string& cwd, OutputCallback callback, std::string& error);

        void stop();

        // Sends what the user typed to the shell.
        void writeInput(const std::string& input);

        void resize(int cols, int rows);

    private:
        int m_cols = 120;
        int m_rows = 30;

        // Reads the words of the shell.
        void readLoop();

        // Sends the keys of the user.
        void writeLoop();

        std::atomic<bool> m_running{false};
        std::thread m_readThread;
        std::thread m_writeThread;

        OutputCallback m_outputCallback;

        // The keys that wait to be sent.
        std::mutex m_writeMutex;
        std::condition_variable m_writeSignal;
        std::deque<std::string> m_writeQueue;

#ifdef _WIN32
        // The words of the shell.
        //
        // The shell of windows is a program of the console: it writes with
        // the calls of the console and it reads the keys with them. When it
        // is given a pipe instead, it writes plain words and reads plain
        // keys, and it stays alive. The editor holds both ways, so that the
        // programs of the console and the plain programs both work.
        HANDLE m_hChildStd_IN_Rd = NULL;   // the shell reads the keys here
        HANDLE m_hChildStd_IN_Wr = NULL;   // the editor writes the keys here
        HANDLE m_hChildStd_OUT_Rd = NULL;  // the editor reads the words here
        HANDLE m_hChildStd_OUT_Wr = NULL;  // the shell writes the words here

        // The console of the editor.
        HANDLE m_hConsoleIn_Rd = NULL;     // the console reads the keys here
        HANDLE m_hConsoleIn_Wr = NULL;     // the editor writes the keys here
        HANDLE m_hConsoleOut_Rd = NULL;    // the editor reads the console here
        HANDLE m_hConsoleOut_Wr = NULL;    // the console writes here

        HANDLE m_hProcess = NULL;
        HANDLE m_hJob = NULL;
        void* m_hPC = NULL; // HPCON type-erased
#endif
    };

} // namespace vrutti::core::terminal
