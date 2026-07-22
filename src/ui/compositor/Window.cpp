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
            // Set Icon
            HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(101));
            if (hIcon) {
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            }
            
            // Enable native dark mode title bar (Windows 10/11)
            // 20 is DWMWA_USE_IMMERSIVE_DARK_MODE in older SDKs, 19 in some. 
            // We try both safely.
            #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
            #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
            #endif
            #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_V2
            #define DWMWA_USE_IMMERSIVE_DARK_MODE_V2 19
            #endif
            
            BOOL value = TRUE;
            // DwmSetWindowAttribute is usually in dwmapi.dll, so we should dynamically load it or assume we link against it.
            // Since webview.h might not link dwmapi.lib by default, we'll dynamically load it.
            HMODULE hDwm = LoadLibraryA("dwmapi.dll");
            if (hDwm) {
                typedef HRESULT(WINAPI* DwmSetWindowAttribute_t)(HWND, DWORD, LPCVOID, DWORD);
                DwmSetWindowAttribute_t pDwmSetWindowAttribute = (DwmSetWindowAttribute_t)GetProcAddress(hDwm, "DwmSetWindowAttribute");
                if (pDwmSetWindowAttribute) {
                    pDwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
                    pDwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_V2, &value, sizeof(value));
                }
                FreeLibrary(hDwm);
            }

            // Make the window frameless (remove title bar) but keep it resizable
            LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
            style |= WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
            style &= ~WS_CAPTION; // Remove title bar
            SetWindowLongPtr(hwnd, GWL_STYLE, style);
            
            // Subclass the window to handle WM_NCCALCSIZE for true frameless and WM_GETMINMAXINFO for maximizing properly
            SetWindowSubclass(hwnd, [](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) -> LRESULT {
                if (uMsg == WM_NCCALCSIZE && wParam == TRUE) {
                    return 0; // Returning 0 removes the standard title bar entirely while keeping resize borders
                }
                if (uMsg == WM_GETMINMAXINFO) {
                    MINMAXINFO* mmi = (MINMAXINFO*)lParam;
                    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                    if (hMonitor) {
                        MONITORINFO mi;
                        mi.cbSize = sizeof(MONITORINFO);
                        if (GetMonitorInfo(hMonitor, &mi)) {
                            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
                        }
                    }
                    return 0;
                }
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }, 1, 0);

            SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        }
#endif

        // Bind a C++ function so JS can close the window
        w->bind("closeWindow", [this](const std::string& seq, const std::string& req, void* arg) {
            std::cout << "[UI] JS requested window close." << std::endl;
            this->shutdown();
        }, nullptr);

        w->bind("minimizeWindow", [this](const std::string& seq, const std::string& req, void* arg) {
#ifdef _WIN32
            if (m_windowHandle) {
                HWND hwnd = static_cast<HWND>(static_cast<webview::webview*>(m_windowHandle)->window());
                ShowWindow(hwnd, SW_MINIMIZE);
            }
#endif
        }, nullptr);

        w->bind("maximizeWindow", [this](const std::string& seq, const std::string& req, void* arg) {
#ifdef _WIN32
            if (m_windowHandle) {
                HWND hwnd = static_cast<HWND>(static_cast<webview::webview*>(m_windowHandle)->window());
                WINDOWPLACEMENT wp;
                wp.length = sizeof(WINDOWPLACEMENT);
                if (GetWindowPlacement(hwnd, &wp)) {
                    if (wp.showCmd == SW_SHOWMAXIMIZED) {
                        ShowWindow(hwnd, SW_RESTORE);
                    } else {
                        ShowWindow(hwnd, SW_MAXIMIZE);
                    }
                }
            }
#endif
        }, nullptr);

        w->bind("startWindowDrag", [this](const std::string& seq, const std::string& req, void* arg) {
#ifdef _WIN32
            if (m_windowHandle) {
                HWND hwnd = static_cast<HWND>(static_cast<webview::webview*>(m_windowHandle)->window());
                ReleaseCapture();
                SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
#endif
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
