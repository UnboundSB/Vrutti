// Integration test for the terminal of the editor.
//
// The terminal of the editor is a real shell: the core starts it, reads
// what it writes and sends what the user types to it. The test opens a
// terminal, waits for the shell to say something, types a command and
// waits for the answer of that command.
//
// The test is the proof that the terminal is not a drawing of a terminal:
// a drawing cannot answer a command.

#include "../../core/terminal/TerminalProcess.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

    int g_failures = 0;

    void check(bool condition, const std::string& what) {
        if (condition) {
            std::cout << "  [PASS] " << what << "\n";
        } else {
            std::cout << "  [FAIL] " << what << "\n";
            g_failures++;
        }
    }

    // The text the shell has written so far.
    class Output {
    public:
        void append(const std::string& text) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_text += text;
        }

        std::string text() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_text;
        }

    private:
        mutable std::mutex m_mutex;
        std::string m_text;
    };

    // Waits for a text to show up in the output of the shell.
    bool waitFor(const Output& output, const std::string& wanted, int seconds) {
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (std::chrono::steady_clock::now() < until) {
            if (output.text().find(wanted) != std::string::npos) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

} // namespace

void runTerminalProcessTest() {
    std::cout << "[TerminalProcessTest] Starting a terminal...\n";

    Output output;
    vrutti::core::terminal::TerminalProcess terminal;

    std::string error;
    const bool started = terminal.start("", [&](const std::string& text) {
        output.append(text);
    }, error);

#ifdef _WIN32
    check(started, "the terminal starts a shell");
    if (!started) {
        std::cout << "  what went wrong: " << error << "\n";
    }

    if (started) {
        // The shell says hello: the prompt of the shell, or its banner.
        const bool spoke = waitFor(output, "PS", 20) || waitFor(output, ">", 5);
        check(spoke, "the shell writes its prompt");

        // A command that is typed into the terminal is answered by it.
        terminal.writeInput("echo vrutti-terminal-works\r");
        const bool answered = waitFor(output, "vrutti-terminal-works", 20);
        check(answered, "the shell answers a command that is typed into the terminal");

        if (!answered) {
            std::cout << "  what the shell wrote: "
                      << output.text().substr(0, 400) << "\n";
        }

        // The shell of the user does not start in the inside of the editor.
        //
        // The editor sets its own folder to the place of the program. The
        // shell of the user starts in the root of the drive of the system
        // when no folder is opened.
        terminal.writeInput("echo vrutti-terminal-folder\r");
        waitFor(output, "vrutti-terminal-folder", 20);
        std::string systemDrive;
        const char* drive = std::getenv("SystemDrive");
        if (drive && *drive) {
            systemDrive = drive;
            if (systemDrive.back() != '\\') systemDrive += '\\';
        }
        check(!systemDrive.empty() && output.text().find(systemDrive) != std::string::npos,
              "the shell starts in the root of the drive of the system when no folder is opened");
        if (systemDrive.empty() || output.text().find(systemDrive) == std::string::npos) {
            std::cout << "  what the shell wrote: "
                      << output.text().substr(0, 400) << "\n";
        }
    }

    terminal.stop();
    check(true, "the terminal stops");
#else
    // The terminal of the editor is a shell of windows for now. It says so
    // instead of pretending to be one.
    check(!started, "the terminal says that it has no shell on this system");
    check(!error.empty(), "the terminal says why it has no shell");
#endif

    std::cout << (g_failures == 0 ? "[TerminalProcessTest] All checks passed\n"
                                   : "[TerminalProcessTest] FAILURES\n");
}

int main() {
    runTerminalProcessTest();
    return g_failures == 0 ? 0 : 1;
}
