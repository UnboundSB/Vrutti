#pragma once
#include <string>
#include "../../core/ipc/IPCClient.h"
#include "../../core/terminal/TerminalProcess.h"
#include "../../core/terminal/ReplProcess.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "../../core/plugins/PluginLoader.h"

namespace vrutti::ui {
    class Window {
    public:
        Window(int width, int height, const std::string& title, vrutti::core::ipc::IPCClient* ipc = nullptr, const std::string& initialWorkspace = "");
        ~Window();

        bool init();
        void run();
        void shutdown();
        bool shouldClose() const;
        void logToOutput(const std::string& channel, const std::string& text);

    private:
        // A piece of work that runs on a thread of its own.
        //
        // The work of the editor that takes a while -- the installation of
        // an extension, the search in the files of the user -- runs on a
        // thread of its own, so that the page does not wait for it. The
        // thread is held here and is waited for before the window goes
        // away: a worker that is still running when the webview is gone
        // would write into a window that is not there any more.
        struct Worker {
            std::thread thread;
            std::shared_ptr<std::atomic<bool>> done;
        };

        // Starts a piece of work on a thread of its own.
        void startWorker(std::function<void()> job);

        // Waits for the work that is still running.
        void joinWorkers();

        int m_width;
        int m_height;
        std::string m_title;
        std::string m_initialWorkspace;
        void* m_windowHandle; // Type erased webview handle
        vrutti::core::ipc::IPCClient* m_ipc;
        std::unordered_map<std::string, std::unique_ptr<vrutti::core::terminal::TerminalProcess>> m_terminals;
        std::unique_ptr<vrutti::core::terminal::ReplProcess> m_repl;
        vrutti::core::plugins::PluginLoader m_pluginLoader;
        vrutti::core::plugins::IPlugin* m_searchPlugin = nullptr;
        std::mutex m_workerMutex;
        std::vector<Worker> m_workers;
    };
}
