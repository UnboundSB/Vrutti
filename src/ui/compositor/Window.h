#pragma once
#include <string>
#include "../../core/ipc/IPCClient.h"

namespace vrutti::ui {
    class Window {
    public:
        Window(int width, int height, const std::string& title, vrutti::core::ipc::IPCClient* ipc = nullptr);
        ~Window();

        bool init();
        void run();
        void shutdown();
        bool shouldClose() const;

    private:
        int m_width;
        int m_height;
        std::string m_title;
        void* m_windowHandle; // Type erased webview handle
        vrutti::core::ipc::IPCClient* m_ipc;
    };
}
