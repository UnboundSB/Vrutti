#include "Window.h"
#include <iostream>
#include <filesystem>

// We must include webview.h here. It's a single header library.
#include "../vendor/webview.h"

namespace vrutti::ui {

    Window::Window(int width, int height, const std::string& title)
        : m_width(width), m_height(height), m_title(title), m_windowHandle(nullptr) 
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

        // Bind a C++ function so JS can close the window
        w->bind("closeWindow", [this](const std::string& seq, const std::string& req, void* arg) {
            std::cout << "[UI] JS requested window close." << std::endl;
            this->shutdown();
        }, nullptr);

        // Calculate absolute path to the frontend index.html
        std::filesystem::path cwd = std::filesystem::current_path();
        std::filesystem::path htmlPath = cwd / "src" / "ui" / "frontend" / "index.html";
        
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
