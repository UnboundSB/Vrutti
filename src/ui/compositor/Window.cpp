#include "Window.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <cctype>
#include <algorithm>
#include <vector>
#include <chrono>

namespace {
    int fuzzyMatch(const std::string& lowerPattern, const std::string& str) {
        if (lowerPattern.empty()) return 1;
        int pIdx = 0;
        int score = 0;
        int consecutive = 0;
        
        size_t lastSlash = str.find_last_of("/\\");
        
        for (size_t i = 0; i < str.length(); ++i) {
            char c = static_cast<char>(std::tolower(static_cast<unsigned char>(str[i])));
            if (static_cast<size_t>(pIdx) < lowerPattern.length() && c == lowerPattern[pIdx]) {
                pIdx++;
                score += 10 + (consecutive * 5);
                if (lastSlash != std::string::npos && i > lastSlash) {
                    score += 10;
                }
                consecutive++;
            } else {
                consecutive = 0;
            }
        }
        
        if (static_cast<size_t>(pIdx) == lowerPattern.length()) {
            score -= static_cast<int>(str.length());
            return score;
        }
        return 0;
    }
}

// We must include webview.h here. It's a single header library.
#include "../vendor/webview.h"
#include "../../core/utils/Json.h"
#include "../../core/utils/Base64.h"
#include "../../core/config/SettingsManager.h"
#include "../../core/plugins/PluginLoader.h"
#include "../../core/plugins/ExtensionScanner.h"

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#endif

namespace vrutti::ui {

    Window::Window(int width, int height, const std::string& title, vrutti::core::ipc::IPCClient* ipc, const std::string& initialWorkspace) 
        : m_width(width), m_height(height), m_title(title), m_ipc(ipc), m_initialWorkspace(initialWorkspace), m_windowHandle(nullptr) {
    }

    Window::~Window() {
        if (m_searchPlugin) {
            m_pluginLoader.unloadPlugin(m_searchPlugin->getName());
        }
        shutdown();
    }

    bool Window::init() {
        std::cout << "[UI] Initializing Native Webview Window..." << std::endl;
        
        m_searchPlugin = m_pluginLoader.loadPlugin("build/vrutti_search.dll");
        if (m_searchPlugin) {
            m_searchPlugin->initialize();
            std::cout << "[Window] Successfully loaded search plugin dynamically." << std::endl;
        } else {
            std::cerr << "[Window] Failed to load search plugin from: build/vrutti_search.dll" << std::endl;
        }
        
        // Ensure webview.h compiles by setting up a dummy handle
        m_windowHandle = new webview::webview(true, nullptr);
        
        webview::webview* w = static_cast<webview::webview*>(m_windowHandle);
        w->set_title(m_title);
        w->set_size(m_width, m_height, WEBVIEW_HINT_NONE);

#ifdef _WIN32
        HWND hwnd = static_cast<HWND>(w->window());
        if (hwnd) {
            // Set Icon
            HICON hIconSmall = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(101), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
            HICON hIconBig = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(101), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
            
            if (hIconSmall) SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
            if (hIconBig) SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
            
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

        w->bind("sendIpcMessage", [this](const std::string& seq, const std::string& req, void* arg) {
            if (this->m_ipc) {
                auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
                if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                    auto methodNode = parsedReq->arrayElements[0];
                    auto payloadNode = parsedReq->arrayElements[1];
                    if (methodNode && methodNode->type == vrutti::core::utils::JsonNode::Type::String &&
                        payloadNode && payloadNode->type == vrutti::core::utils::JsonNode::Type::String) {
                        std::string method = vrutti::core::utils::JsonParser::unescapeString(methodNode->stringValue);
                        std::string payload = vrutti::core::utils::JsonParser::unescapeString(payloadNode->stringValue);
                        this->m_ipc->sendMessage(method, payload);
                    }
                }
            }
        }, nullptr);

        if (this->m_ipc) {
            this->m_ipc->setOnMessage([w](const std::string& msg) {
                std::string b64 = base64_encode(msg);
                w->dispatch([w, b64]() {
                    w->eval("if (window.vruttiIpcMessage) window.vruttiIpcMessage('" + b64 + "');");
                });
            });
        }

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

        w->bind("vruttiSearchExtensions", [this](const std::string& req) -> std::string {
            std::string payload = req;
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && !parsedReq->arrayElements.empty()) {
                auto argNode = parsedReq->arrayElements[0];
                if (argNode && argNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    payload = vrutti::core::utils::JsonParser::unescapeString(argNode->stringValue);
                } else if (argNode && argNode->type == vrutti::core::utils::JsonNode::Type::Object) {
                    payload = vrutti::core::utils::JsonSerializer::stringify(argNode);
                }
            }

            vrutti::core::plugins::IPlugin* vsxPlugin = m_pluginLoader.getPlugin("VSXRegistryService");
            if (!vsxPlugin) {
                vsxPlugin = m_pluginLoader.loadPlugin("build/VSXRegistryService.dll");
                if (vsxPlugin) {
                    vsxPlugin->initialize();
                }
            }
            
            if (vsxPlugin) {
                return vsxPlugin->executeCommand("search_extensions", payload);
            }
            return "{\"extensions\":[]}";
        });

        w->bind("vruttiSetBackgroundMedia", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* mediaPlugin = m_pluginLoader.loadPlugin("build/MediaController.dll");
            if (mediaPlugin) {
                mediaPlugin->initialize();
                return mediaPlugin->executeCommand("set_background_media", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiDocumentOpened", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin("build/DocumentSyncService.dll");
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("document_opened", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiDocumentClosed", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin("build/DocumentSyncService.dll");
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("document_closed", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiDocumentEdited", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin("build/DocumentSyncService.dll");
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("document_edited", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiSelectionChanged", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin("build/DocumentSyncService.dll");
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("selection_changed", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiGetActiveCommands", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* cmdPlugin = m_pluginLoader.loadPlugin("build/CommandHandlerService.dll");
            if (cmdPlugin) {
                cmdPlugin->initialize();
                return cmdPlugin->executeCommand("get_active_commands", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiExecuteCommand", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* cmdPlugin = m_pluginLoader.loadPlugin("build/CommandHandlerService.dll");
            if (cmdPlugin) {
                cmdPlugin->initialize();
                return cmdPlugin->executeCommand("execute_command", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiRequestCompletions", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/LanguageFeatureService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("request_completions", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiRequestHover", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/LanguageFeatureService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("request_hover", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiGetDiagnostics", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/DiagnosticService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("get_diagnostics", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_fs_read_dir", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/FileSystemService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("read_directory", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_fs_read_file", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/FileSystemService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("read_file", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_debug_start", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/DebugAdapterService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("debug_start", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_debug_set_breakpoints", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/DebugAdapterService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("debug_set_breakpoints", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_debug_action", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/DebugAdapterService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("debug_action", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_terminal_create", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/TerminalService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("terminal_create", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_terminal_write", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/TerminalService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("terminal_write", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_terminal_resize", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/TerminalService.dll");
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("terminal_resize", req);
            }
            return "{\"status\":\"error\"}";
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

        w->bind("vruttiGetSettings", [this](const std::string& req) -> std::string {
            return vrutti::core::config::SettingsManager::getInstance().getSettingsJson();
        });

        w->bind("vruttiUpdateSetting", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto keyNode = parsedReq->arrayElements[0];
                auto valNode = parsedReq->arrayElements[1];
                if (keyNode && keyNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string key = vrutti::core::utils::JsonParser::unescapeString(keyNode->stringValue);
                    std::string valStr = vrutti::core::utils::JsonSerializer::stringify(valNode, 0, false);
                    vrutti::core::config::SettingsManager::getInstance().updateSetting(key, valStr);
                }
            }
            return "{}";
        });

        w->bind("vruttiInstallExtension", [this, w](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto urlNode = parsedReq->arrayElements[0];
                auto nameNode = parsedReq->arrayElements[1];
                if (urlNode && urlNode->type == vrutti::core::utils::JsonNode::Type::String &&
                    nameNode && nameNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    
                    std::string url = vrutti::core::utils::JsonParser::unescapeString(urlNode->stringValue);
                    std::string name = vrutti::core::utils::JsonParser::unescapeString(nameNode->stringValue);
                    
                    std::cout << "[Core] Request to install extension '" << name << "' from " << url << std::endl;
                    
                    std::thread([this, w, url, name]() {
#ifdef _WIN32
                        std::string cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"$ErrorActionPreference = 'Stop'; Write-Output 'PROGRESS_10'; $name = '" + name + "'; $url = '" + url + "'; $extDir = Join-Path $env:USERPROFILE '.vrutti\\extensions\\'; if (!(Test-Path $extDir)) { New-Item -ItemType Directory -Force -Path $extDir | Out-Null }; $destDir = Join-Path $extDir $name; if (Test-Path $destDir) { Remove-Item -Recurse -Force $destDir }; $tmpZip = Join-Path $env:TEMP '" + name + ".zip'; [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri $url -OutFile $tmpZip -UseBasicParsing; Write-Output 'PROGRESS_50'; Expand-Archive -Path $tmpZip -DestinationPath $destDir -Force; Remove-Item $tmpZip; Write-Output 'PROGRESS_100'; Write-Output 'SUCCESS';\"";
                        FILE* pipe = _popen(cmd.c_str(), "r");
#else
                        std::string cmd = "mkdir -p ~/.vrutti/extensions/" + name + " && curl -L \"" + url + "\" -o /tmp/" + name + ".zip && echo 'PROGRESS_50' && unzip -o /tmp/" + name + ".zip -d ~/.vrutti/extensions/" + name + " && rm /tmp/" + name + ".zip && echo 'PROGRESS_100' && echo 'SUCCESS'";
                        FILE* pipe = popen(cmd.c_str(), "r");
#endif
                        if (!pipe) {
                            std::cerr << "[Core] Failed to execute installer" << std::endl;
                            return;
                        }

                        char buffer[1024];
                        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                            std::string line = buffer;
                            // Remove trailing newlines
                            line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
                            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                            
                            if (line.empty()) continue;

                            // Send progress back to webview
                            std::string b64 = base64_encode(line);
                            w->dispatch([w, name, b64]() {
                                w->eval("if (window.dispatchEvent) { "
                                        "try { const out = atob('" + b64 + "');"
                                        "if (out.startsWith('PROGRESS_')) {"
                                        "  const msg = { method: 'extensions/progress', params: { name: '" + name + "', percentage: parseInt(out.split('_')[1]) } };"
                                        "  window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                                        "} else if (out === 'SUCCESS' || out === 'ERROR') {"
                                        "  const msg = { method: 'extensions/install-result', params: { name: '" + name + "', type: out.toLowerCase() } };"
                                        "  window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                                        "  if (out === 'SUCCESS' && window.vruttiRequestInstalledExtensions) { window.vruttiRequestInstalledExtensions('{}'); }"
                                        "} } catch(e) { console.warn('Ignored ext_manager output:', atob('" + b64 + "')); }"
                                        "}");
                            });
                        }
#ifdef _WIN32
                        _pclose(pipe);
#else
                        pclose(pipe);
#endif
                    }).detach();
                }
            }
            return "{}";
        });

        w->bind("vruttiUninstallExtension", [this, w](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto nameNode = parsedReq->arrayElements[0];
                if (nameNode && nameNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string name = vrutti::core::utils::JsonParser::unescapeString(nameNode->stringValue);
                    
                    std::thread([this, w, name]() {
#ifdef _WIN32
                        std::string cmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"$dir = Join-Path $env:USERPROFILE '.vrutti\\extensions\\" + name + "'; if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }; \"";
                        FILE* pipe = _popen(cmd.c_str(), "r");
#else
                        std::string cmd = "rm -rf ~/.vrutti/extensions/" + name;
                        FILE* pipe = popen(cmd.c_str(), "r");
#endif
                        if (pipe) {
#ifdef _WIN32
                            _pclose(pipe);
#else
                            pclose(pipe);
#endif
                        }
                        
                        // Notify webview to refresh
                        w->dispatch([w]() {
                            w->eval("if (window.dispatchEvent) { "
                                    "const msg = { method: 'extensions/uninstalled', params: {} };"
                                    "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                                    "if (window.vruttiRequestInstalledExtensions) { window.vruttiRequestInstalledExtensions('{}'); }"
                                    "}");
                        });
                    }).detach();
                }
            }
            return "{}";
        });

        w->bind("vruttiRequestInstalledExtensions", [this, w](const std::string& req) -> std::string {
            std::thread([this, w]() {
                std::string jsonStr = "[]";
                vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin("build/ExtensionManager.dll");
                if (plugin) {
                    plugin->initialize();
                    jsonStr = plugin->executeCommand("list", "");
                }
                
                std::string b64 = base64_encode(jsonStr);
                
                w->dispatch([w, b64]() {
                    w->eval("if (window.dispatchEvent) { "
                            "try { "
                            "const jsonStr = atob('" + b64 + "');"
                            "const installed = JSON.parse(jsonStr);"
                            "const msg = { method: 'extensions/installed', params: installed };"
                            "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                            "const themes = [];"
                            "const iconThemes = [];"
                            "for (const ext of installed) {"
                            "  if (ext.contributes) {"
                            "    if (ext.contributes.themes) {"
                            "      for (const t of ext.contributes.themes) {"
                            "        themes.push({ id: t.id || t.label, label: t.label, path: ext.localPath + '/' + t.path });"
                            "      }"
                            "    }"
                            "    if (ext.contributes.iconThemes) {"
                            "      for (const t of ext.contributes.iconThemes) {"
                            "        iconThemes.push({ id: t.id || t.label, label: t.label, path: ext.localPath + '/' + t.path });"
                            "      }"
                            "    }"
                            "  }"
                            "}"
                            "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: { method: 'themes/available', params: themes } }));"
                            "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: { method: 'icon_themes/available', params: iconThemes } }));"
                            "} catch(e) { console.warn('Failed to parse extensions list:', e); }"
                            "}");
                });
            }).detach();
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
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to read directory: " << e.what() << std::endl;
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
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to create file: " << e.what() << std::endl;
                    }
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiDeleteFile", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    try {
                        std::filesystem::remove_all(path);
                        return "{\"success\":true}";
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to delete file: " << e.what() << std::endl;
                    }
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiRenameFile", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto oldPathNode = parsedReq->arrayElements[0];
                auto newPathNode = parsedReq->arrayElements[1];
                if (oldPathNode && oldPathNode->type == vrutti::core::utils::JsonNode::Type::String && newPathNode && newPathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string oldPath = vrutti::core::utils::JsonParser::unescapeString(oldPathNode->stringValue);
                    std::string newPath = vrutti::core::utils::JsonParser::unescapeString(newPathNode->stringValue);
                    try {
                        std::filesystem::rename(oldPath, newPath);
                        return "{\"success\":true}";
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to rename file: " << e.what() << std::endl;
                    }
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
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to create folder: " << e.what() << std::endl;
                    }
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


        w->bind("vruttiReadFile", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    try {
                        std::ifstream f(path, std::ios::binary);
                        if (f.is_open()) {
                            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                            return vrutti::core::utils::JsonSerializer::escapeString(content);
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to read file: " << e.what() << std::endl;
                    }
                }
            }
            return "\"\"";
        });

        w->bind("vruttiWriteFile", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto pathNode = parsedReq->arrayElements[0];
                auto contentNode = parsedReq->arrayElements[1];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String && contentNode && contentNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    std::string content = vrutti::core::utils::JsonParser::unescapeString(contentNode->stringValue);
                    try {
                        std::ofstream f(path, std::ios::binary);
                        f << content;
                        f.close();
                        return "{\"success\":true}";
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to write file: " << e.what() << std::endl;
                    }
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiOpenFileDialog", [this](const std::string& req) -> std::string {
            std::string result = "";
#ifdef _WIN32
            IFileDialog *pfd = NULL;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
                DWORD dwOptions;
                if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
                    pfd->SetOptions(dwOptions | FOS_FORCEFILESYSTEM);
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
            std::string json = "{\"success\":true,\"path\":" + vrutti::core::utils::JsonSerializer::escapeString(result) + "}";
            return json;
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

        w->bind("vruttiGitCommand", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            std::string stdoutStr = "";
            int exitCode = -1;

            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto cwdNode = parsedReq->arrayElements[0];
                auto cmdNode = parsedReq->arrayElements[1];
                if (cwdNode && cwdNode->type == vrutti::core::utils::JsonNode::Type::String &&
                    cmdNode && cmdNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    
                    std::string cwd = vrutti::core::utils::JsonParser::unescapeString(cwdNode->stringValue);
                    std::string cmd = vrutti::core::utils::JsonParser::unescapeString(cmdNode->stringValue);
                    
#ifdef _WIN32
                    std::string fullCmd = "cd /d \"" + cwd + "\" && " + cmd + " 2>&1";
                    FILE* pipe = _popen(fullCmd.c_str(), "r");
#else
                    std::string fullCmd = "cd \"" + cwd + "\" && " + cmd + " 2>&1";
                    FILE* pipe = popen(fullCmd.c_str(), "r");
#endif
                    if (pipe) {
                        char buffer[1024];
                        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                            stdoutStr += buffer;
                        }
#ifdef _WIN32
                        exitCode = _pclose(pipe);
#else
                        exitCode = pclose(pipe);
                        if (WIFEXITED(exitCode)) {
                            exitCode = WEXITSTATUS(exitCode);
                        }
#endif
                    }
                }
            }
            std::string result = "{";
            result += "\"stdout\":" + vrutti::core::utils::JsonSerializer::escapeString(stdoutStr) + ",";
            result += "\"exitCode\":" + std::to_string(exitCode);
            result += "}";
            return result;
        });

        w->bind("vruttiToggleDevTools", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            return "{}";
        });

        w->bind("vruttiRunFile", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            std::string stdoutStr = "";
            int exitCode = -1;

            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path = vrutti::core::utils::JsonParser::unescapeString(pathNode->stringValue);
                    
                    std::filesystem::path fsPath(path);
                    std::string ext = fsPath.extension().string();
                    std::string dir = fsPath.parent_path().string();
                    std::string stem = fsPath.stem().string();
                    
                    std::string cmd = "";
                    if (ext == ".py") {
                        cmd = "python \"" + path + "\"";
                    } else if (ext == ".js") {
                        cmd = "node \"" + path + "\"";
                    } else if (ext == ".ts") {
                        cmd = "npx ts-node \"" + path + "\"";
                    } else if (ext == ".cpp") {
#ifdef _WIN32
                        std::string exeName = stem + ".exe";
                        cmd = "g++ \"" + path + "\" -o \"" + dir + "\\" + exeName + "\" && \"" + dir + "\\" + exeName + "\"";
#else
                        std::string exeName = stem;
                        cmd = "g++ \"" + path + "\" -o \"" + dir + "/" + exeName + "\" && \"" + dir + "/" + exeName + "\"";
#endif
                    } else {
                        return "{\"error\":\"Unsupported file extension for running\"}";
                    }
                    
#ifdef _WIN32
                    std::string fullCmd = "cd /d \"" + dir + "\" && " + cmd + " 2>&1";
                    FILE* pipe = _popen(fullCmd.c_str(), "r");
#else
                    std::string fullCmd = "cd \"" + dir + "\" && " + cmd + " 2>&1";
                    FILE* pipe = popen(fullCmd.c_str(), "r");
#endif
                    if (pipe) {
                        char buffer[1024];
                        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                            stdoutStr += buffer;
                        }
#ifdef _WIN32
                        exitCode = _pclose(pipe);
#else
                        exitCode = pclose(pipe);
                        if (WIFEXITED(exitCode)) {
                            exitCode = WEXITSTATUS(exitCode);
                        }
#endif
                    } else {
                        return "{\"error\":\"Failed to spawn process\"}";
                    }
                }
            }
            std::string result = "{";
            result += "\"stdout\":" + vrutti::core::utils::JsonSerializer::escapeString(stdoutStr) + ",";
            result += "\"exitCode\":" + std::to_string(exitCode);
            result += "}";
            return result;
        });

        w->bind("vruttiToggleDevTools", [this](const std::string& req) -> std::string {
#ifdef _WIN32
            INPUT ip;
            ip.type = INPUT_KEYBOARD;
            ip.ki.wScan = 0;
            ip.ki.time = 0;
            ip.ki.dwExtraInfo = 0;
            ip.ki.wVk = VK_F12;
            ip.ki.dwFlags = 0; 
            SendInput(1, &ip, sizeof(INPUT));
            ip.ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(1, &ip, sizeof(INPUT));
#endif
            return "{}";
        });

        w->bind("vruttiOpenExternalUrl", [this](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto urlNode = parsedReq->arrayElements[0];
                if (urlNode && urlNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string url = vrutti::core::utils::JsonParser::unescapeString(urlNode->stringValue);
#ifdef _WIN32
                    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
                    std::string cmd = "xdg-open \"" + url + "\"";
                    std::system(cmd.c_str());
#endif
                }
            }
            return "{}";
        });

        w->bind("vruttiSearchAsync", [this, w](const std::string& req) -> std::string {
            auto parsedReq = vrutti::core::utils::JsonParser::parse(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto argsNode = parsedReq->arrayElements[0];
                
                int searchId = 0;
                
                std::string argsJsonString = "";
                if (argsNode && argsNode->type == vrutti::core::utils::JsonNode::Type::Object) {
                    argsJsonString = vrutti::core::utils::JsonSerializer::stringify(argsNode, 0, false);
                    auto searchIdNode = argsNode->get("searchId");
                    if (searchIdNode && searchIdNode->type == vrutti::core::utils::JsonNode::Type::Number) {
                        searchId = (int)searchIdNode->numberValue;
                    }
                }

                auto t_start = std::chrono::high_resolution_clock::now();

                std::thread([this, w, argsJsonString, searchId, t_start]() {
                    std::string resultJson = "{\"files\":[],\"folders\":[],\"words\":[]}";
                    if (m_searchPlugin) {
                        resultJson = m_searchPlugin->executeCommand("search", argsJsonString);
                    }
                    
                    std::string fullResultJson = "{\"searchId\":" + std::to_string(searchId) + ",\"results\":" + resultJson + "}";
                    std::string ipcMsg = "{\"method\":\"search/results\",\"params\":" + fullResultJson + "}";
                    std::string b64 = base64_encode(ipcMsg);
                    
                    std::string ipcMsgJs = "if (window.vruttiIpcMessage) { window.vruttiIpcMessage('" + b64 + "'); }";
                    
                    w->dispatch([w, ipcMsgJs]() {
                        w->eval(ipcMsgJs);
                    });
                    
                    auto t_end = std::chrono::high_resolution_clock::now();
                    auto ms_total = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
                    printf("[SearchProfile] SearchId %d FINISHED in %lld ms using SearchPlugin.\n", searchId, ms_total);
                }).detach();
            }
            return "{}";
        });

        return true;
    }

    void Window::run() {
        if (!m_windowHandle) return;
        
        // Ensure initial HTML URI is formatted
        std::filesystem::path basePath = std::filesystem::current_path();
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
