#include "Window.h"
#include <iostream>
#include <filesystem>
#include <fstream>

// We must include webview.h here. It's a single header library.
#include "../vendor/webview.h"
#include "../../core/utils/Json.h"
#include "../../core/utils/Base64.h"
#include "../../core/config/SettingsManager.h"

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

namespace vrutti::ui {

    Window::Window(int width, int height, const std::string& title, vrutti::core::ipc::IPCClient* ipc, const std::string& initialWorkspace) 
        : m_width(width), m_height(height), m_title(title), m_ipc(ipc), m_initialWorkspace(initialWorkspace), m_windowHandle(nullptr) {
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
                SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE | 0x0002, 0);
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

        w->bind("vruttiTerminalInit", [this, w](const std::string& req) -> std::string {
            std::string id = "";
            std::string cwd = "";
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto idNode = parsedReq->arrayElements[0];
                auto pathNode = parsedReq->arrayElements[1];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    id = vrutti::core::utils::JsonParser::unescapeString(idNode->stringValue);
                }
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    cwd = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                }
            }
            if (id.empty()) return "{}";

            this->m_terminals[id] = std::make_unique<vrutti::core::terminal::TerminalProcess>();
            this->m_terminals[id]->start(cwd, [w, id](const std::string& out) {
                std::string b64 = base64_encode(out);
                w->dispatch([w, id, b64]() {
                    w->eval("if (window.vruttiTerminalOutput) window.vruttiTerminalOutput('" + id + "', '" + b64 + "');");
                });
            });
            return "{}";
        });

        w->bind("vruttiTerminalInput", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto idNode = parsedReq->arrayElements[0];
                auto inputNode = parsedReq->arrayElements[1];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String &&
                    inputNode && inputNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string id = vrutti::core::utils::JsonParser::unescapeString(idNode->stringValue);
                    std::string input = base64_decode(std::string(inputNode->stringValue));
                    if (this->m_terminals.count(id)) {
                        this->m_terminals[id]->writeInput(input);
                    }
                }
            }
            return "{}";
        });

        w->bind("vruttiTerminalResize", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 3) {
                std::string id = "";
                auto idNode = parsedReq->arrayElements[0];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    id = vrutti::core::utils::JsonParser::unescapeString(idNode->stringValue);
                }
                
                if (!id.empty() && this->m_terminals.count(id)) {
                    int cols = 120;
                    int rows = 30;
                    
                    auto colsNode = parsedReq->arrayElements[1];
                    auto rowsNode = parsedReq->arrayElements[2];
                    
                    if (colsNode && colsNode->type == vrutti::core::utils::JsonNode::Type::Number) {
                        cols = (int)colsNode->numberValue;
                    }
                    if (rowsNode && rowsNode->type == vrutti::core::utils::JsonNode::Type::Number) {
                        rows = (int)rowsNode->numberValue;
                    }
                    
                    this->m_terminals[id]->resize(cols, rows);
                }
            }
            return "{}";
        });

        w->bind("vruttiTerminalClose", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto idNode = parsedReq->arrayElements[0];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string id = vrutti::core::utils::JsonParser::unescapeString(idNode->stringValue);
                    if (this->m_terminals.count(id)) {
                        this->m_terminals[id]->stop();
                        this->m_terminals.erase(id);
                    }
                }
            }
            return "{}";
        });

        w->bind("vruttiDebugInit", [this, w](const std::string& req) -> std::string {
            if (this->m_repl) {
                this->m_repl->stop();
            }
            this->m_repl = std::make_unique<vrutti::core::terminal::ReplProcess>();
            this->m_repl->start([w](const std::string& out) {
                std::string b64 = base64_encode(out);
                w->dispatch([w, b64]() {
                    w->eval("if (window.vruttiDebugLog) window.vruttiDebugLog('" + b64 + "');");
                });
            });
            return "{}";
        });

        w->bind("vruttiDebugEval", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto inputNode = parsedReq->arrayElements[0];
                if (inputNode && inputNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string input = vrutti::core::utils::JsonParser::unescapeString(inputNode->stringValue);
                    if (this->m_repl) {
                        this->m_repl->evaluate(input);
                    }
                }
            }
            return "{}";
        });

        w->bind("vruttiReadDirectory", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    
                    std::string json = "[";
                    bool first = true;
                    try {
                        for (const auto& entry : std::filesystem::directory_iterator(path)) {
                            if (!first) json += ",";
                            json += "{";
                            json += "\"name\":" + vrutti::core::utils::JsonSerializer::escapeString(entry.path().filename().string()) + ",";
                            json += "\"isDirectory\":" + std::string(entry.is_directory() ? "true" : "false") + ",";
                            
                            std::string resStr = entry.path().string();
                            for (char& c : resStr) { if (c == '\\') c = '/'; }
                            json += "\"resource\":\"file:///" + vrutti::core::utils::JsonSerializer::escapeString(resStr).substr(1); // substr(1) to remove leading quote since file:/// is inside the string
                            
                            json += "}";
                            first = false;
                        }
                    } catch (...) {
                        // Return empty on error
                    }
                    json += "]";
                    return json;
                }
            }
            return "[]";
        });

        w->bind("vruttiCreateFile", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    try {
                        std::ofstream f(path);
                        f.close();
                        return "{\"success\":true}";
                    } catch (...) {}
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiCreateFolder", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    try {
                        std::filesystem::create_directories(path);
                        return "{\"success\":true}";
                    } catch (...) {}
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiOpenFolderDialog", [this](const std::string& req) -> std::string {
            std::string result = "";
#ifdef _WIN32
            IFileDialog *pfd = NULL;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
                DWORD dwOptions;
                if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                    pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
                }
                HWND hwnd = static_cast<HWND>(static_cast<webview::webview*>(m_windowHandle)->window());
                if (SUCCEEDED(pfd->Show(hwnd))) {
                    IShellItem *psi;
                    if (SUCCEEDED(pfd->GetResult(&psi))) {
                        PWSTR pszFilePath = NULL;
                        if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath))) {
                            std::wstring wstr(pszFilePath);
                            int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
                            std::string strTo(size_needed, 0);
                            WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
                            result = strTo;
                            CoTaskMemFree(pszFilePath);
                        }
                        psi->Release();
                    }
                }
                pfd->Release();
            }
#endif
            // JSON stringify the result
            std::string json = "{\"path\":" + vrutti::core::utils::JsonSerializer::escapeString(result) + "}";
            return json;
        });

        w->bind("vruttiCreateFolder", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    try {
                        std::filesystem::create_directory(path);
                        return "{\"success\":true}";
                    } catch (...) {}
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiCreateFile", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    try {
                        std::ofstream f(path);
                        f.close();
                        return "{\"success\":true}";
                    } catch (...) {}
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiOpenNewWindow", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
#ifdef _WIN32
                    char exePath[MAX_PATH];
                    GetModuleFileNameA(NULL, exePath, MAX_PATH);
                    std::string cmd = "start \"\" \"" + std::string(exePath) + "\" \"" + path + "\"";
                    std::system(cmd.c_str());
#endif
                }
            }
            return "{}";
        });

        w->bind("vruttiGetInitialWorkspace", [this](const std::string& req) -> std::string {
            std::string json = "{\"path\":" + vrutti::core::utils::JsonSerializer::escapeString(m_initialWorkspace) + "}";
            return json;
        });

        w->bind("vruttiGetSettings", [this](const std::string& req) -> std::string {
            return vrutti::core::config::SettingsManager::getInstance().getSettingsJson();
        });

        w->bind("vruttiUpdateSetting", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto keyNode = parsedReq->arrayElements[0];
                auto valNode = parsedReq->arrayElements[1];
                if (keyNode && keyNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string key(keyNode->stringValue);
                    std::string valJson = vrutti::core::utils::JsonSerializer::stringify(valNode, 0, false);
                    vrutti::core::config::SettingsManager::getInstance().updateSetting(key, valJson);
                }
            }
            return "{}";
        });

        return true;
    }

    void Window::run() {
        if (!m_windowHandle) return;
        
        // Ensure initial HTML URI is formatted
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
        std::filesystem::path basePath = exePath.parent_path().parent_path();
        std::filesystem::path htmlPath = basePath / "src" / "ui" / "frontend" / "dist" / "index.html";
        
        std::string htmlStr = htmlPath.string();
        for (char& c : htmlStr) { if (c == '\\') c = '/'; }
        std::string uri = "file:///" + htmlStr;
        
        webview::webview* w = static_cast<webview::webview*>(m_windowHandle);
        w->navigate(uri);
        std::cout << "[UI] Webview navigated to: " << uri << std::endl;

        w->run(); // This is blocking until window is closed
        
        // Clean up webview after event loop ends
        if (m_windowHandle) {
            delete w;
            m_windowHandle = nullptr;
            std::cout << "[UI] Webview destroyed." << std::endl;
        }
    }

    void Window::shutdown() {
        for (auto& [id, term] : m_terminals) {
            if (term) term->stop();
        }
        m_terminals.clear();
        
        if (m_repl) {
            m_repl->stop();
            m_repl.reset();
        }

        if (m_windowHandle) {
            webview::webview* w = static_cast<webview::webview*>(m_windowHandle);
            w->terminate();
        }
    }

    bool Window::shouldClose() const {
        return m_windowHandle == nullptr;
    }

    void Window::logToOutput(const std::string& channel, const std::string& text) {
        if (!m_windowHandle) return;
        webview::webview* w = static_cast<webview::webview*>(m_windowHandle);
        std::string safeChannel = vrutti::core::utils::JsonSerializer::escapeString(channel);
        std::string safeText = vrutti::core::utils::JsonSerializer::escapeString(text);
        
        w->dispatch([w, safeChannel, safeText]() {
            std::string script = "if (window.vruttiWriteOutput) window.vruttiWriteOutput(" + safeChannel + ", " + safeText + ");";
            w->eval(script);
        });
    }
}
