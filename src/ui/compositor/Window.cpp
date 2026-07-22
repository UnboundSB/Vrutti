#include "Window.h"
#include <iostream>
#include <filesystem>

// We must include webview.h here. It's a single header library.
#include "../vendor/webview.h"

namespace vrutti::ui {

    Window::Window(int width, int height, const std::string& title, vrutti::core::ipc::IPCClient* ipc)
        : m_width(width), m_height(height), m_title(title), m_windowHandle(nullptr), m_ipc(ipc) 
    {
    }

    Window::~Window() {
        shutdown();
    }

    bool Window::init() {
        std::cout << "[UI] Initializing Native Webview Window..." << std::endl;
        
        // Ensure webview.h compiles by setting up a dummy handle
        m_windowHandle = new webview::webview(true, nullptr);
        
        webview::webview* w = static_cast<webview::webview*>(m_windowHandle);
        w->set_title(m_title);
        w->set_size(m_width, m_height, WEBVIEW_HINT_NONE);

#ifdef _WIN32
        HWND hwnd = static_cast<HWND>(w->window());
        if (hwnd) {
            HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));
            if (hIcon) {
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            }
        }
#endif

        // Bind a C++ function so JS can close the window
        w->bind("closeWindow", [this](const std::string& seq, const std::string& req, void* arg) {
            std::cout << "[UI] JS requested window close." << std::endl;
            this->shutdown();
        }, nullptr);

        // Bind IPC message handler for Lit frontend to communicate with C++ Core natively
        w->bind("sendIpcMessage", [this](const std::string& seq, const std::string& req, void* arg) {
            if (this->m_ipc) {
                // The 'req' payload from webview is always a JSON array string containing the arguments.
                // We pass it directly to the IPC client to handle.
                this->m_ipc->handleIncomingMessage(req);
            }
        }, nullptr);

        // Calculate absolute path to the compiled frontend dist/index.html
        // We use the executable path to be independent of the Current Working Directory.
        std::filesystem::path exePath;
#ifdef _WIN32
        char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        exePath = std::filesystem::path(buffer);
#else
        char buffer[PATH_MAX];
        ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
        if (count != -1) {
            exePath = std::filesystem::path(std::string(buffer, (count > 0) ? count : 0));
        }
#endif
        // exePath is build/vrutti_app.exe. Go up two levels to get to vrutti_ide/
        std::filesystem::path basePath = exePath.parent_path().parent_path();
        std::filesystem::path htmlPath = basePath / "src" / "ui" / "frontend" / "dist" / "index.html";
        
        // Ensure string is correctly formatted for webview
        std::string htmlStr = htmlPath.string();
        for (char& c : htmlStr) { if (c == '\\') c = '/'; }
        std::string uri = "file:///" + htmlStr;
        
        w->navigate(uri);

        std::cout << "[UI] Webview navigated to: " << uri << std::endl;
        return true;
    }

    void Window::run() {
        // Run the webview event loop
        webview::webview* w = static_cast<webview::webview*>(m_windowHandle);
        if (w) {
            w->run(); // This is blocking until window is closed
        }
    }

    void Window::shutdown() {
        if (m_windowHandle) {
            webview::webview* w = static_cast<webview::webview*>(m_windowHandle);
            w->terminate();
            delete w;
            m_windowHandle = nullptr;
            std::cout << "[UI] Webview destroyed." << std::endl;
        }
    }

    bool Window::shouldClose() const {
        return m_windowHandle == nullptr;
    }
}
