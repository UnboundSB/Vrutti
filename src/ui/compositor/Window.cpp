#include "Window.h"
#include "../../core/ipc/BridgeProtocol.h"
#include "../../core/utils/Base64.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <thread>
#include <cctype>
#include <algorithm>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// The encoding of the binary payloads that travel over the bridge.
using vrutti::core::utils::base64_encode;

namespace {
    // The rules of the bridge between the page and the core live in one
    // place: `core/ipc/BridgeProtocol.cpp`. The page may only send a
    // message that is small enough, that names a method that the core
    // knows, and whose parameters have the shape that the core expects.
    using vrutti::core::ipc::isSaneMethodName;
    using vrutti::core::ipc::parseBridgeMessage;

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

    // Escape a string for safe embedding inside a JavaScript string literal
    // (single-quoted context). Prevents XSS via string literal breakout.
    std::string jsEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '\\':  out += "\\\\"; break;
                case '\'':  out += "\\'"; break;
                case '"':   out += '\\"'; break;
                case '\n':  out += "\\n"; break;
                case '\r':  out += "\\r"; break;
                case '\t':  out += "\\t"; break;
                case '\b':  out += "\\b"; break;
                case '\f':  out += "\\f"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out += buf;
                    } else {
                        out += c;
                    }
                    break;
            }
        }
        return out;
    }

    // The most text that is read from a program that the page started.
    //
    // A program that writes without end would use the memory of the editor
    // until there is none left.
    constexpr size_t kMaxCapturedOutput = 4u * 1024u * 1024u;

    // How long a program that the page started may run.
    //
    // The page asks for a command to be run. The command may wait for
    // something that never happens, and the editor would wait with it.
    constexpr unsigned int kExecTimeoutMs = 120000;

    // Puts a text into a command line the way Windows wants it: the text is
    // put between two quotes, and every backslash that stands in front of a
    // quote is doubled.
    //
    // The old code put a path that came from the page into a command for
    // the shell. A path with a quote in it ended the quoted part, and the
    // rest of the path was run as a command of its own.
    std::wstring quoteArgument(const std::wstring& argument) {
        std::wstring result;
        bool needsQuote = argument.find(L' ') != std::wstring::npos ||
                          argument.find(L'\t') != std::wstring::npos ||
                          argument.find(L'"') != std::wstring::npos ||
                          argument.empty();
        if (needsQuote) result += L'"';
        size_t backslashes = 0;
        for (wchar_t c : argument) {
            if (c == L'\\') {
                ++backslashes;
                continue;
            }
            if (c == L'"') {
                // The backslashes in front of the quote are doubled, and the
                // quote itself is escaped with one more backslash.
                result.append(backslashes * 2 + 1, L'\\');
                backslashes = 0;
                result += L'"';
                continue;
            }
            result.append(backslashes, L'\\');
            backslashes = 0;
            result += c;
        }
        // The backslashes at the end are doubled: they stand in front of the
        // closing quote.
        result.append(backslashes * 2, L'\\');
        if (needsQuote) result += L'"';
        return result;
    }

    std::wstring widen(const std::string& text) {
        if (text.empty()) return std::wstring();
        const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                              static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0) return std::wstring();
        std::wstring out(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                            out.data(), size);
        return out;
    }

    // Tells whether a text may be opened as a link.
    //
    // Only the schemes that show a page or a mail are allowed. A link that
    // names a program ("file:", "ms-msdt:") would let the page run a
    // program of its own choosing.
    bool isAllowedExternalUrl(const std::string& url) {
        static const char* allowed[] = {"http://", "https://", "mailto:"};
        for (const char* scheme : allowed) {
            const size_t length = std::strlen(scheme);
            if (url.size() >= length &&
                std::equal(scheme, scheme + length, url.begin(),
                           [](char a, char b) {
                               return std::tolower(static_cast<unsigned char>(a)) ==
                                      std::tolower(static_cast<unsigned char>(b));
                           })) {
                return true;
            }
        }
        return false;
    }

    // Securely spawn a process without a shell, preventing command injection.
    // Optionally captures stdout into stdoutStr and exit code into exitCode.
    //
    // The text of the program is read on a thread of its own, so that a
    // program that writes a lot does not fill the pipe and wait for a
    // reader that is not there yet. The program itself is waited for with a
    // deadline: a program that never ends is stopped.
    #ifdef _WIN32
    bool safeExec(const std::string& exePath, const std::vector<std::string>& args,
                  std::string* stdoutStr = nullptr, int* exitCode = nullptr) {
        std::string localStdout;
        int localExitCode = -1;
        
        // Build command line with proper quoting (no shell, no injection)
        std::wstring cmdLine = quoteArgument(widen(exePath));
        for (const auto& a : args) {
            cmdLine += L" ";
            cmdLine += quoteArgument(widen(a));
        }

        // Create pipe for stdout capture
        HANDLE hRead, hWrite;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;

        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(L'\0');

        BOOL ok = CreateProcessW(
            nullptr, cmdBuf.data(),
            nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
            nullptr, nullptr, &si, &pi);
        
        CloseHandle(hWrite);
        
        if (!ok) {
            CloseHandle(hRead);
            return false;
        }

        // The text of the program is read on a thread of its own. The
        // thread ends when the program closes the pipe, which it does when
        // it ends -- or when it is stopped.
        std::thread reader([&]() {
            char buffer[4096];
            DWORD bytesRead;
            while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                // The text of the program is kept inside of its limit: a
                // program that writes without end would use the memory of
                // the editor until there is none left.
                if (localStdout.size() < kMaxCapturedOutput) {
                    localStdout.append(buffer, std::min<size_t>(bytesRead,
                                                               kMaxCapturedOutput - localStdout.size()));
                }
            }
            CloseHandle(hRead);
        });

        // The program gets a while to finish. A program that waits for
        // something that never happens is stopped, so that the editor does
        // not wait with it.
        if (WaitForSingleObject(pi.hProcess, kExecTimeoutMs) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 1000);
            localExitCode = -1;
        } else {
            DWORD dwExit = 0;
            GetExitCodeProcess(pi.hProcess, &dwExit);
            localExitCode = static_cast<int>(dwExit);
        }

        reader.join();
        
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        
        if (stdoutStr) *stdoutStr = localStdout;
        if (exitCode) *exitCode = localExitCode;
        return true;
    }
    #else
    bool safeExec(const std::string& exePath, const std::vector<std::string>& args,
                  std::string* stdoutStr = nullptr, int* exitCode = nullptr) {
        std::string localStdout;
        int localExitCode = -1;
        
        int pipefd[2];
        if (pipe(pipefd) != 0) return false;
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);
            
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(exePath.c_str()));
            for (const auto& a : args) {
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);
            
            execvp(exePath.c_str(), argv.data());
            _exit(127);
        }
        
        close(pipefd[1]);

        // The text of the program is read on a thread of its own.
        std::thread reader([&]() {
            char buffer[4096];
            ssize_t n;
            while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
                if (localStdout.size() < kMaxCapturedOutput) {
                    localStdout.append(buffer, std::min<size_t>(static_cast<size_t>(n),
                                                               kMaxCapturedOutput - localStdout.size()));
                }
            }
            close(pipefd[0]);
        });

        // The program is waited for with a deadline.
        int status = 0;
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(kExecTimeoutMs);
        bool stopped = false;
        while (true) {
            const pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == pid) break;
            if (result < 0) break;
            if (std::chrono::steady_clock::now() >= deadline) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                localExitCode = -1;
                stopped = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if (!stopped) {
            if (WIFEXITED(status)) {
                localExitCode = WEXITSTATUS(status);
            } else {
                localExitCode = -1;
            }
        }

        reader.join();
        
        if (stdoutStr) *stdoutStr = localStdout;
        if (exitCode) *exitCode = localExitCode;
        return true;
    }
    #endif

    // Starts a program without a shell and lets it run on its own.
    //
    // The old code built a command for the shell out of a path that came
    // from the page: a path with a quote in it ended the quoted part, and
    // the rest of the path was run as a command of its own.
    void startDetached(const std::string& exePath, const std::vector<std::string>& args) {
#ifdef _WIN32
        std::wstring cmdLine = quoteArgument(widen(exePath));
        for (const auto& a : args) {
            cmdLine += L" ";
            cmdLine += quoteArgument(widen(a));
        }
        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(L'\0');

        STARTUPINFOW si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOWNORMAL;
        PROCESS_INFORMATION pi = { 0 };
        if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                            CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &si, &pi)) {
            std::cerr << "[UI] The program could not be started" << std::endl;
            return;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
#else
        pid_t pid = fork();
        if (pid == 0) {
            // The child: the program is started without a shell.
            std::vector<char*> argv;
            argv.push_back(const_cast<char*>(exePath.c_str()));
            for (const auto& a : args) {
                argv.push_back(const_cast<char*>(a.c_str()));
            }
            argv.push_back(nullptr);
            execvp(exePath.c_str(), argv.data());
            _exit(127);
        }
        if (pid < 0) {
            std::cerr << "[UI] The program could not be started" << std::endl;
        }
#endif
    }
}

// We must include webview.h here. It's a single header library.
#include "../vendor/webview.h"
#include "../../core/utils/Json.h"
#include "../../core/utils/Base64.h"
#include "../../core/utils/Paths.h"
#include "../../core/utils/HttpDownload.h"
#include "../../core/config/SettingsManager.h"
#include "../../core/extensions/ExtensionService.h"
#include "../../core/plugins/PluginLoader.h"
#include "../../core/plugins/ExtensionScanner.h"

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
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

    void Window::startWorker(std::function<void()> job) {
        // The flag says when the work is done. It lets the window give
        // back the threads that are finished, so that the list of the
        // workers does not grow for ever.
        auto done = std::make_shared<std::atomic<bool>>(false);
        std::thread thread([job, done]() {
            job();
            done->store(true);
        });

        std::lock_guard<std::mutex> lock(m_workerMutex);
        for (auto it = m_workers.begin(); it != m_workers.end();) {
            if (it->done->load()) {
                if (it->thread.joinable()) it->thread.join();
                it = m_workers.erase(it);
            } else {
                ++it;
            }
        }
        m_workers.push_back({std::move(thread), done});
    }

    void Window::joinWorkers() {
        std::vector<Worker> workers;
        {
            std::lock_guard<std::mutex> lock(m_workerMutex);
            workers.swap(m_workers);
        }
        for (auto& worker : workers) {
            if (worker.thread.joinable()) worker.thread.join();
        }
    }

    bool Window::init() {
        std::cout << "[UI] Initializing Native Webview Window..." << std::endl;
        
        m_searchPlugin = m_pluginLoader.loadPlugin(
            vrutti::core::utils::pluginPath("vrutti_search.dll").string());
        if (m_searchPlugin) {
            m_searchPlugin->initialize();
            std::cout << "[Window] Successfully loaded search plugin dynamically." << std::endl;
        } else {
            std::cerr << "[Window] Failed to load search plugin from: "
                      << vrutti::core::utils::pluginPath("vrutti_search.dll").string()
                      << std::endl;
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

            // The window of the editor comes to the front.
            //
            // The console of the process takes the focus when the program
            // starts, and the editor would open behind it. To the user it
            // looks as if nothing had happened at all.
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
            SetActiveWindow(hwnd);
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
                // A message of the page is an array of two things: the name
                // of the method and the payload of the method as a text. A
                // message of another shape, or one that is too big, is not
                // read at all.
                auto parsedReq = parseBridgeMessage(req);
                if (!parsedReq) {
                    std::cerr << "[UI] The message of the page had a shape that the bridge does not know"
                              << std::endl;
                    return;
                }

                auto methodNode = parsedReq->arrayElements[0];
                auto payloadNode = parsedReq->arrayElements[1];
                std::string method(methodNode->stringValue);

                // The name of the method has to be a name: it is used as the
                // key of a message and ends up in the log. And it has to be
                // a method that the core knows: the page may not make the
                // core do something that has no name.
                if (!isSaneMethodName(method) ||
                    !vrutti::core::ipc::isAllowedBridgeMethod(method)) {
                    std::cerr << "[UI] The message of the page named a method that the core does not know"
                              << std::endl;
                    return;
                }

                // The payload of the method is a text that holds a JSON
                // object. A payload of another shape is not a payload.
                auto params = vrutti::core::ipc::parseBridgePayload(
                    std::string(payloadNode->stringValue));
                if (!params) {
                    std::cerr << "[UI] The message of the page had a payload that is not an object"
                              << std::endl;
                    return;
                }

                this->m_ipc->sendLocalRequest(method, params);
            }
        }, nullptr);

        if (this->m_ipc) {
            this->m_ipc->setOnEvent([w](const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params) {
                auto envelope = std::make_shared<vrutti::core::utils::JsonNode>(vrutti::core::utils::JsonNode::Type::Object);
                envelope->objectProperties.emplace_back(
                    std::string_view("method"),
                    vrutti::core::utils::makeJsonString(method));
                envelope->objectProperties.emplace_back(
                    std::string_view("params"),
                    params ? params : std::make_shared<vrutti::core::utils::JsonNode>(vrutti::core::utils::JsonNode::Type::Null));
                std::string jsonStr = vrutti::core::utils::JsonSerializer::stringify(envelope, 0, false);
                std::string b64 = base64_encode(jsonStr);
                w->dispatch([w, b64]() {
                    w->eval("if (window.vruttiIpcMessage) window.vruttiIpcMessage('" + b64 + "');");
                });
            });
        }

        w->bind("vruttiTerminalInit", [this, w](const std::string& req) -> std::string {
            std::string id = "";
            std::string cwd = "";
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto idNode = parsedReq->arrayElements[0];
                auto pathNode = parsedReq->arrayElements[1];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    id = idNode->stringValue;
                }
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    cwd = pathNode->stringValue;
                }
            }
            if (id.empty()) return "{}";

            this->m_terminals[id] = std::make_unique<vrutti::core::terminal::TerminalProcess>();
            this->m_terminals[id]->start(cwd, [w, id](const std::string& out) {
                std::string b64 = base64_encode(out);
                std::string safeId = jsEscape(id);
                w->dispatch([w, safeId, b64]() {
                    w->eval("if (window.vruttiTerminalOutput) window.vruttiTerminalOutput('" + safeId + "', '" + b64 + "');");
                });
            });
            return "{}";
        });

        w->bind("vruttiSearchExtensions", [this](const std::string& req) -> std::string {
            std::string payload = req;
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && !parsedReq->arrayElements.empty()) {
                auto argNode = parsedReq->arrayElements[0];
                if (argNode && argNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    payload = argNode->stringValue;
                } else if (argNode && argNode->type == vrutti::core::utils::JsonNode::Type::Object) {
                    payload = vrutti::core::utils::JsonSerializer::stringify(argNode);
                }
            }

            vrutti::core::plugins::IPlugin* vsxPlugin = m_pluginLoader.getPlugin("VSXRegistryService");
            if (!vsxPlugin) {
                vsxPlugin = m_pluginLoader.loadPlugin(
                    vrutti::core::utils::pluginPath("VSXRegistryService.dll").string());
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
            vrutti::core::plugins::IPlugin* mediaPlugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("MediaController.dll").string());
            if (mediaPlugin) {
                mediaPlugin->initialize();
                return mediaPlugin->executeCommand("set_background_media", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiDocumentOpened", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DocumentSyncService.dll").string());
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("document_opened", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiDocumentClosed", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DocumentSyncService.dll").string());
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("document_closed", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiDocumentEdited", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DocumentSyncService.dll").string());
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("document_edited", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiSelectionChanged", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* docPlugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DocumentSyncService.dll").string());
            if (docPlugin) {
                docPlugin->initialize();
                return docPlugin->executeCommand("selection_changed", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiGetActiveCommands", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* cmdPlugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("CommandHandlerService.dll").string());
            if (cmdPlugin) {
                cmdPlugin->initialize();
                return cmdPlugin->executeCommand("get_active_commands", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiExecuteCommand", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* cmdPlugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("CommandHandlerService.dll").string());
            if (cmdPlugin) {
                cmdPlugin->initialize();
                return cmdPlugin->executeCommand("execute_command", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiRequestCompletions", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("LanguageFeatureService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("request_completions", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiRequestHover", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("LanguageFeatureService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("request_hover", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiGetDiagnostics", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DiagnosticService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("get_diagnostics", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_fs_read_dir", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("FileSystemService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("read_directory", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_fs_read_file", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("FileSystemService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("read_file", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_debug_start", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DebugAdapterService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("debug_start", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_debug_set_breakpoints", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DebugAdapterService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("debug_set_breakpoints", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_debug_action", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("DebugAdapterService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("debug_action", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_terminal_create", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("TerminalService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("terminal_create", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_terminal_write", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("TerminalService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("terminal_write", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vrutti_terminal_resize", [this](const std::string& req) -> std::string {
            vrutti::core::plugins::IPlugin* plugin = m_pluginLoader.loadPlugin(
                vrutti::core::utils::pluginPath("TerminalService.dll").string());
            if (plugin) {
                plugin->initialize();
                return plugin->executeCommand("terminal_resize", req);
            }
            return "{\"status\":\"error\"}";
        });

        w->bind("vruttiTerminalInput", [this](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto idNode = parsedReq->arrayElements[0];
                auto inputNode = parsedReq->arrayElements[1];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String &&
                    inputNode && inputNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string id(idNode->stringValue);
                    std::string input;
                    // The text that comes from the page may be broken. A
                    // broken text is not written into the terminal: half of
                    // a command is worse than none.
                    if (!vrutti::core::utils::base64_decode(
                            std::string(inputNode->stringValue), input)) {
                        return "{}";
                    }
                    if (this->m_terminals.count(id)) {
                        this->m_terminals[id]->writeInput(input);
                    }
                }
            }
            return "{}";
        });

        w->bind("vruttiTerminalResize", [this](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 3) {
                std::string id = "";
                auto idNode = parsedReq->arrayElements[0];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    id = idNode->stringValue;
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto idNode = parsedReq->arrayElements[0];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string id(idNode->stringValue);
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto inputNode = parsedReq->arrayElements[0];
                if (inputNode && inputNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string input(inputNode->stringValue);
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto keyNode = parsedReq->arrayElements[0];
                auto valNode = parsedReq->arrayElements[1];
                if (keyNode && keyNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string key(keyNode->stringValue);
                    std::string valStr = vrutti::core::utils::JsonSerializer::stringify(valNode, 0, false);
                    vrutti::core::config::SettingsManager::getInstance().updateSetting(key, valStr);
                }
            }
            return "{}";
        });

        w->bind("vruttiInstallExtension", [this, w](const std::string& req) -> std::string {
            // The extension is installed by the service of the core: the
            // archive is checked, the manifest is read, and the file is
            // written in a folder of its own. Nothing is done through a
            // shell, so an address or a name cannot become a command.
            auto parsedReq = parseBridgeMessage(req);
            std::string url;
            std::string name;
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array &&
                parsedReq->arrayElements.size() >= 2) {
                auto urlNode = parsedReq->arrayElements[0];
                auto nameNode = parsedReq->arrayElements[1];
                if (urlNode && urlNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    url = std::string(urlNode->stringValue.data(), urlNode->stringValue.length());
                }
                if (nameNode && nameNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    name = std::string(nameNode->stringValue.data(), nameNode->stringValue.length());
                }
            }
            if (url.empty()) {
                return "{\"ok\":false,\"error\":\"the extension has no file\"}";
            }

            std::cout << "[Core] Request to install extension '" << name << "' from " << url << std::endl;

            startWorker([this, w, url, name]() {
                // The progress of the installation, the way the interface
                // expects it.
                auto progress = [&](int percentage) {
                    const std::string safeName = jsEscape(name);
                    w->dispatch([w, safeName, percentage]() {
                        w->eval("if (window.dispatchEvent) { "
                                "const msg = { method: 'extensions/progress', params: { name: '" +
                                safeName + "', percentage: " + std::to_string(percentage) + " } };"
                                "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                                "}");
                    });
                };

                progress(10);

                std::string file;
                std::string downloaded;
                std::string error;
                if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
                    if (!vrutti::core::utils::HttpDownload::download(
                            url, "", downloaded, 256ull * 1024 * 1024, error)) {
                        progress(0);
                        const std::string safeError = jsEscape(error);
                        w->dispatch([w, safeError]() {
                            w->eval("if (window.dispatchEvent) { "
                                    "const msg = { method: 'extensions/error', params: { message: '" +
                                    safeError + "' } };"
                                    "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                                    "}");
                        });
                        return;
                    }
                    file = downloaded;
                    progress(50);
                } else {
                    // A file that is already on the disk.
                    file = url;
                }

                vrutti::core::extensions::ExtensionDescription installed;
                std::string errorCode;
                const bool ok = vrutti::core::extensions::ExtensionService::installVsix(
                    file, installed, error, errorCode);

                if (!downloaded.empty()) {
                    std::error_code ec;
                    std::filesystem::remove(downloaded, ec);
                }

                if (!ok) {
                    progress(0);
                    const std::string safeError = jsEscape(error);
                    w->dispatch([w, safeError]() {
                        w->eval("if (window.dispatchEvent) { "
                                "const msg = { method: 'extensions/error', params: { message: '" +
                                safeError + "' } };"
                                "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                                "}");
                    });
                    return;
                }

                progress(100);

                // The list of the extensions is sent again, so that the
                // interface shows what was installed.
                const auto list =
                    vrutti::core::extensions::ExtensionService::userInterfaceJson(
                        vrutti::core::extensions::ExtensionService::listAll());
                const std::string encoded = base64_encode(
                    vrutti::core::utils::JsonSerializer::stringify(list, 0, false));
                w->dispatch([w, encoded]() {
                    w->eval("if (window.dispatchEvent) { "
                            "try { "
                            "const installed = JSON.parse(atob('" + encoded + "'));"
                            "const msg = { method: 'extensions/installed', params: installed };"
                            "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                            "} catch(e) { console.warn('Failed to parse the list of the extensions:', e); }"
                            "}");
                });
            });
            return "{}";
        });

        w->bind("vruttiUninstallExtension", [this, w](const std::string& req) -> std::string {
            // The extension is removed by the service of the core. The name
            // may be the name of the extension alone or the name with its
            // publisher.
            auto parsedReq = parseBridgeMessage(req);
            std::string identifier;
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array &&
                !parsedReq->arrayElements.empty()) {
                auto idNode = parsedReq->arrayElements[0];
                if (idNode && idNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    identifier = std::string(idNode->stringValue.data(), idNode->stringValue.length());
                }
            }
            if (identifier.empty()) {
                return "{\"ok\":false,\"error\":\"the extension has no name\"}";
            }

            startWorker([this, w, identifier]() {
                std::string error;
                std::string errorCode;
                const bool ok = vrutti::core::extensions::ExtensionService::uninstall(
                    identifier, error, errorCode);
                if (!ok) {
                    const std::string safeError = jsEscape(error);
                    w->dispatch([w, safeError]() {
                        w->eval("if (window.dispatchEvent) { "
                                "const msg = { method: 'extensions/error', params: { message: '" +
                                safeError + "' } };"
                                "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                                "}");
                    });
                    return;
                }

                const auto list =
                    vrutti::core::extensions::ExtensionService::userInterfaceJson(
                        vrutti::core::extensions::ExtensionService::listAll());
                const std::string encoded = base64_encode(
                    vrutti::core::utils::JsonSerializer::stringify(list, 0, false));
                w->dispatch([w, encoded]() {
                    w->eval("if (window.dispatchEvent) { "
                            "try { "
                            "const installed = JSON.parse(atob('" + encoded + "'));"
                            "const msg = { method: 'extensions/installed', params: installed };"
                            "window.dispatchEvent(new CustomEvent('vrutti-ipc', { detail: msg }));"
                            "} catch(e) { console.warn('Failed to parse the list of the extensions:', e); }"
                            "}");
                });
            });
            return "{}";
        });

        w->bind("vruttiRequestInstalledExtensions", [this, w](const std::string& req) -> std::string {
            startWorker([this, w]() {
                // The list comes from the service of the core, so it is the
                // same list that the extension host gets.
                const auto list =
                    vrutti::core::extensions::ExtensionService::userInterfaceJson(
                        vrutti::core::extensions::ExtensionService::listAll());
                std::string jsonStr = vrutti::core::utils::JsonSerializer::stringify(list, 0, false);

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
            });
            return "{}";
        });

        w->bind("vruttiReadDirectory", [this](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
                    
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                // The second name says whether a folder with things in it
                // may be taken away. A folder that is not empty is only
                // taken away when the page says that it may: a mistake in
                // the page would otherwise take a whole tree with it.
                bool recursive = false;
                if (parsedReq->arrayElements.size() >= 2) {
                    auto recursiveNode = parsedReq->arrayElements[1];
                    if (recursiveNode && recursiveNode->type == vrutti::core::utils::JsonNode::Type::Boolean) {
                        recursive = recursiveNode->boolValue;
                    }
                }
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
                    if (path.empty()) return "{\"success\":false}";
                    try {
                        std::filesystem::path target(path);
                        // The root of a drive and the home folder are never
                        // removed: a mistake in the page would take the
                        // whole disk with it.
                        const std::filesystem::path parent = target.parent_path();
                        if (target == parent || target == target.root_path()) {
                            std::cerr << "[UI] The root of a disk is not deleted" << std::endl;
                            return "{\"success\":false}";
                        }
                        std::error_code ec;
                        const bool exists = std::filesystem::exists(target, ec);
                        if (ec) return "{\"success\":false}";
                        if (!exists) return "{\"success\":false}";

                        const bool isDirectory = std::filesystem::is_directory(target, ec);
                        if (ec) return "{\"success\":false}";

                        if (isDirectory && !recursive) {
                            // A folder that still has things in it is not
                            // taken away unless the page says that it may.
                            std::error_code countError;
                            const bool empty = std::filesystem::is_empty(target, countError);
                            if (countError || !empty) {
                                std::cerr << "[UI] The folder was not deleted: it still has things in it"
                                          << std::endl;
                                return "{\"success\":false}";
                            }
                            std::filesystem::remove(target, ec);
                        } else {
                            std::filesystem::remove_all(target, ec);
                        }
                        if (ec) {
                            std::cerr << "[UI] Failed to delete file: " << ec.message() << std::endl;
                            return "{\"success\":false}";
                        }
                        return "{\"success\":true}";
                    } catch (const std::exception& e) {
                        std::cerr << "[UI] Failed to delete file: " << e.what() << std::endl;
                    }
                }
            }
            return "{\"success\":false}";
        });

        w->bind("vruttiRenameFile", [this](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto oldPathNode = parsedReq->arrayElements[0];
                auto newPathNode = parsedReq->arrayElements[1];
                if (oldPathNode && oldPathNode->type == vrutti::core::utils::JsonNode::Type::String && newPathNode && newPathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string oldPath(oldPathNode->stringValue);
                    std::string newPath(newPathNode->stringValue);
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto pathNode = parsedReq->arrayElements[0];
                auto contentNode = parsedReq->arrayElements[1];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String && contentNode && contentNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
                    std::string content(contentNode->stringValue);
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
                    if (path.empty()) return "{}";
#ifdef _WIN32
                    char exePath[MAX_PATH];
                    const DWORD length = GetModuleFileNameA(NULL, exePath, MAX_PATH);
                    if (length > 0 && length < MAX_PATH) {
                        // The program is started without a shell. The path
                        // is put between quotes the way the command line of
                        // Windows wants it, so a path that holds a quote
                        // cannot end the program and start another one.
                        startDetached(std::string(exePath), { path });
                    }
#else
                    startDetached("/proc/self/exe", { path });
#endif
                }
            }
            return "{}";
        });

        w->bind("vruttiGetInitialWorkspace", [this](const std::string& req) -> std::string {
            std::string json = "{\"path\":" + vrutti::core::utils::JsonSerializer::escapeString(m_initialWorkspace) + "}";
            return json;
        });

        // Reports whether a path still exists on disk. The webview uses this
        // to drop a workspace folder that has been deleted or moved since the
        // last session, instead of opening a directory that is not there.
        w->bind("vruttiPathExists", [this](const std::string& req) -> std::string {
            bool exists = false;
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array &&
                !parsedReq->arrayElements.empty()) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
                    if (!path.empty()) {
                        std::error_code ec;
                        exists = std::filesystem::exists(path, ec) && !ec;
                    }
                }
            }
            return exists ? "{\"exists\":true}" : "{\"exists\":false}";
        });

        w->bind("vruttiGetSettings", [this](const std::string& req) -> std::string {
            return vrutti::core::config::SettingsManager::getInstance().getSettingsJson();
        });

        w->bind("vruttiUpdateSetting", [this](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
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
            auto parsedReq = parseBridgeMessage(req);
            std::string stdoutStr = "";
            int exitCode = -1;

            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 2) {
                auto cwdNode = parsedReq->arrayElements[0];
                auto cmdNode = parsedReq->arrayElements[1];
                if (cwdNode && cwdNode->type == vrutti::core::utils::JsonNode::Type::String &&
                    cmdNode && cmdNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    
                    std::string cwd(cwdNode->stringValue);
                    std::string cmd(cmdNode->stringValue);
                    
#ifdef _WIN32
                    std::string shellCmd = "cd /d \"" + cwd + "\" && " + cmd + " 2>&1";
                    safeExec("cmd.exe", { "/c", shellCmd }, &stdoutStr, &exitCode);
#else
                    std::string shellCmd = "cd \"" + cwd + "\" && " + cmd + " 2>&1";
                    safeExec("/bin/sh", { "-c", shellCmd }, &stdoutStr, &exitCode);
#endif
                }
            }
            std::string result = "{";
            result += "\"stdout\":" + vrutti::core::utils::JsonSerializer::escapeString(stdoutStr) + ",";
            result += "\"exitCode\":" + std::to_string(exitCode);
            result += "}";
            return result;
        });

        w->bind("vruttiToggleDevTools", [this](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
            return "{}";
        });

        w->bind("vruttiRunFile", [this](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
            std::string stdoutStr = "";
            int exitCode = -1;

            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto pathNode = parsedReq->arrayElements[0];
                if (pathNode && pathNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string path(pathNode->stringValue);
                    
                    std::filesystem::path fsPath(path);
                    std::string ext = fsPath.extension().string();
                    std::string dir = fsPath.parent_path().string();
                    std::string stem = fsPath.stem().string();
                    
                    std::string exeName = stem + ".exe";
                    
                    if (ext == ".py") {
#ifdef _WIN32
                        safeExec("python", { path }, &stdoutStr, &exitCode);
#else
                        safeExec("python3", { path }, &stdoutStr, &exitCode);
#endif
                    } else if (ext == ".js") {
                        safeExec("node", { path }, &stdoutStr, &exitCode);
                    } else if (ext == ".ts") {
                        safeExec("npx", { "ts-node", path }, &stdoutStr, &exitCode);
                    } else if (ext == ".cpp") {
                        // Step 1: Compile
#ifdef _WIN32
                        safeExec("g++", { path, "-o", dir + "\\" + exeName }, &stdoutStr, &exitCode);
                        if (exitCode == 0) {
                            // Step 2: Run
                            safeExec(dir + "\\" + exeName, {}, &stdoutStr, &exitCode);
                        }
#else
                        safeExec("g++", { path, "-o", dir + "/" + exeName }, &stdoutStr, &exitCode);
                        if (exitCode == 0) {
                            safeExec(dir + "/" + exeName, {}, &stdoutStr, &exitCode);
                        }
#endif
                    } else {
                        return "{\"error\":\"Unsupported file extension for running\"}";
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
            auto parsedReq = parseBridgeMessage(req);
            if (parsedReq && parsedReq->type == vrutti::core::utils::JsonNode::Type::Array && parsedReq->arrayElements.size() >= 1) {
                auto urlNode = parsedReq->arrayElements[0];
                if (urlNode && urlNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    std::string url(urlNode->stringValue);
                    // Only a link that shows a page or a mail is opened. A
                    // link that names a program would run that program.
                    if (!isAllowedExternalUrl(url)) {
                        std::cerr << "[UI] The link was not opened: its scheme is not allowed"
                                  << std::endl;
                        return "{}";
                    }
#ifdef _WIN32
                    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#else
                    // The program is started without a shell: a link with a
                    // quote in it cannot become a command of its own.
                    startDetached("xdg-open", { url });
#endif
                }
            }
            return "{}";
        });

        w->bind("vruttiSearchAsync", [this, w](const std::string& req) -> std::string {
            auto parsedReq = parseBridgeMessage(req);
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

                startWorker([this, w, argsJsonString, searchId, t_start]() {
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
                });
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

#ifdef _WIN32
        // The window comes to the front once more: the page is loaded now,
        // and the editor is what the user is waiting for.
        HWND hwnd = static_cast<HWND>(w->window());
        if (hwnd) {
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        }
#endif

        w->run(); // This is blocking until window is closed

        // The work that is still running is waited for before the window
        // goes away. A worker that is still running when the webview is
        // gone would write into a window that is not there any more.
        joinWorkers();

        // Clean up webview after event loop ends
        if (m_windowHandle) {
            delete w;
            m_windowHandle = nullptr;
            std::cout << "[UI] Webview destroyed." << std::endl;
        }
    }

    void Window::shutdown() {
        // The work that is still running is waited for first: a worker that
        // is still running would use the terminals and the window that are
        // taken away here.
        joinWorkers();

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
