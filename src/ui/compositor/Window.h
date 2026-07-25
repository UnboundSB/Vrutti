#pragma once
#include <string>
#include "../../core/ipc/IPCClient.h"
#include "../../core/terminal/TerminalProcess.h"
#include <memory>
#include <unordered_map>
namespace vrutti::ui {
    class Window {
    public:
        Window(int width, int height, const std::string& title, vrutti::core::ipc::IPCClient* ipc = nullptr, const std::string& initialWorkspace = "");
        ~Window();

        bool init();
        void run();
        void shutdown();
        bool shouldClose() const;

    private:
        int m_width;
        int m_height;
        std::string m_title;
        std::string m_initialWorkspace;
        void* m_windowHandle; // Type erased webview handle
        vrutti::core::ipc::IPCClient* m_ipc;
        std::unordered_map<std::string, std::unique_ptr<vrutti::core::terminal::TerminalProcess>> m_terminals;
    };
}
