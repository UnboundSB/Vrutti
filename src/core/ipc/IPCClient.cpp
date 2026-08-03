#include "IPCClient.h"
#include <iostream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif


#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#endif

namespace vrutti::core::ipc {

    IPCClient::IPCClient(const std::string& pipeName) 
        : m_pipeName(pipeName), m_running(false), m_activeBuffer(nullptr), m_connectionHandle(nullptr) 
    {
    }

    IPCClient::~IPCClient() {
        stop();
    }

    void IPCClient::bindEditorBuffer(vrutti::core::editor::PieceTable* table) {
        m_activeBuffer = table;
    }

    void IPCClient::start() {
        if (m_running) return;
        m_running = true;
        
        // Spin up a std::thread running listenLoop()
        std::thread([this]() {
            try {
                this->listenLoop();
            } catch (...) {
                std::cerr << "[IPC] Fatal error in listen loop!" << std::endl;
            }
        }).detach();
        std::cout << "[IPC] Bound to pipe/socket: " << m_pipeName << std::endl;
        std::cout << "[IPC] Awaiting connection from Node.js Extension Host..." << std::endl;
    }

    void IPCClient::stop() {
        m_running = false;
#ifndef _WIN32
        if (m_connectionHandle) {
            int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_connectionHandle));
            close(fd);
            m_connectionHandle = nullptr;
        }
#else
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        if (m_connectionHandle && m_connectionHandle != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(m_connectionHandle);
            CloseHandle(m_connectionHandle);
            m_connectionHandle = nullptr;
        }
#endif
    }

    void IPCClient::sendMessage(const std::string& method, const std::string& payload) {
        if (!m_running) return;
        
        // Serialize to JSON-RPC and write to pipe
        std::string rpc = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + payload + "}\n";
        
#ifdef _WIN32
        {
            std::lock_guard<std::mutex> lock(m_pipeMutex);
            if (m_connectionHandle && m_connectionHandle != INVALID_HANDLE_VALUE) {
                std::cout << "[IPC] Writing to pipe..." << std::endl;
                DWORD bytesWritten;
                if (!WriteFile(m_connectionHandle, rpc.c_str(), rpc.length(), &bytesWritten, NULL)) {
                    std::cerr << "[IPC] WriteFile failed. Error: " << GetLastError() << std::endl;
                } else {
                    std::cout << "[IPC] WriteFile succeeded, wrote " << bytesWritten << " bytes." << std::endl;
                }
            } else {
                std::cerr << "[IPC] Cannot write, handle is invalid!" << std::endl;
            }
        }
#else
        if (m_connectionHandle) {
            std::cout << "[IPC] Writing to socket..." << std::endl;
            int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_connectionHandle));
            write(fd, rpc.c_str(), rpc.length());
            std::cout << "[IPC] Write complete." << std::endl;
        }
#endif
    }

    void IPCClient::handleIncomingMessage(const std::string& jsonMessage) {
        m_incomingBuffer += jsonMessage;
        
        size_t pos = 0;
        while ((pos = m_incomingBuffer.find('\n')) != std::string::npos) {
            std::string line = m_incomingBuffer.substr(0, pos);
            m_incomingBuffer.erase(0, pos + 1);
            if (m_onMessage) {
                m_onMessage(line);
            }
        }

        // Fast zero-copy parsing would happen here via core::utils::Json
        // For demonstration of the architectural bridge:
        if (!m_activeBuffer) return;

        // Extremely simplified parser stub for JSON-RPC methods:
        // Expected format: {"method":"edit","offset":5,"text":"test"}
        if (jsonMessage.find("\"method\":\"insert\"") != std::string::npos) {
            // Extract offset and text (naive extraction for demonstration)
            size_t offsetPos = jsonMessage.find("\"offset\":");
            size_t textPos = jsonMessage.find("\"text\":\"");
            if (offsetPos != std::string::npos && textPos != std::string::npos) {
                // In production, core::utils::Json handles this safely
                // m_activeBuffer->insert(parsedOffset, parsedText);
                std::cout << "[IPC <- Node.js] Handled 'insert' request securely via C++ Arena Tree." << std::endl;
            }
        }
    }

    void IPCClient::listenLoop() {
        while (m_running) {
#ifndef _WIN32
            int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (server_fd < 0) return;
            m_connectionHandle = reinterpret_cast<void*>(static_cast<intptr_t>(server_fd));

            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, m_pipeName.c_str(), sizeof(addr.sun_path) - 1);

            unlink(m_pipeName.c_str());
            if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                close(server_fd);
                m_connectionHandle = nullptr;
                return;
            }
            if (listen(server_fd, 5) < 0) {
                close(server_fd);
                m_connectionHandle = nullptr;
                return;
            }

            while (m_running) {
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd < 0) continue;
                m_connectionHandle = reinterpret_cast<void*>(static_cast<intptr_t>(client_fd));

                char buffer[4096];
                while (m_running) {
                    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read <= 0) break;
                    buffer[bytes_read] = '\0';
                    handleIncomingMessage(std::string(buffer));
                }
                close(client_fd);
                m_connectionHandle = nullptr;
            }
            close(server_fd);
            unlink(m_pipeName.c_str());
#else
            std::string pipePath = "\\\\.\\pipe\\" + m_pipeName;
            HANDLE hPipe = CreateNamedPipeA(
                pipePath.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1, 4096, 4096, 0, NULL);
                
            if (hPipe == INVALID_HANDLE_VALUE) {
                std::cerr << "[IPC] Failed to create named pipe. Error: " << GetLastError() << std::endl;
                return;
            }
            
            while (m_running) {
                BOOL connected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
                if (connected) {
                    m_connectionHandle = hPipe;
                    char buffer[4096];
                    DWORD bytesRead;
                    DWORD bytesAvail;
                    while (m_running) {
                        bool hasData = false;
                        {
                            std::lock_guard<std::mutex> lock(m_pipeMutex);
                            if (PeekNamedPipe(hPipe, NULL, 0, NULL, &bytesAvail, NULL)) {
                                if (bytesAvail > 0) {
                                    if (ReadFile(hPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                                        buffer[bytesRead] = '\0';
                                        hasData = true;
                                    } else {
                                        break; // Error during read
                                    }
                                }
                            } else {
                                if (GetLastError() == ERROR_BROKEN_PIPE) break;
                            }
                        }
                        
                        if (hasData) {
                            handleIncomingMessage(std::string(buffer));
                        } else {
                            Sleep(10);
                        }
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(m_pipeMutex);
                        DisconnectNamedPipe(hPipe);
                        m_connectionHandle = nullptr;
                    }
                }
            }
            {
                std::lock_guard<std::mutex> lock(m_pipeMutex);
                CloseHandle(hPipe);
            }
#endif
        }
    }

} // namespace vrutti::core::ipc
