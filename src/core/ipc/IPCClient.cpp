#include "IPCClient.h"
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
#ifdef _WIN32
        m_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
    }

    IPCClient::~IPCClient() {
        stop();
#ifdef _WIN32
        if (m_stopEvent) {
            CloseHandle((HANDLE)m_stopEvent);
        }
#endif
    }

    void IPCClient::bindEditorBuffer(vrutti::core::editor::PieceTable* table) {
        m_activeBuffer = table;
    }

    void IPCClient::start() {
        if (m_running) return;
        m_running = true;
#ifdef _WIN32
        ResetEvent((HANDLE)m_stopEvent);
#endif
        
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
        if (!m_running) return;
        m_running = false;

#ifndef _WIN32
        if (m_connectionHandle) {
            int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_connectionHandle));
            close(fd);
            m_connectionHandle = nullptr;
        }
#else
        SetEvent((HANDLE)m_stopEvent);

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
        
        std::string rpc = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + payload + "}\n";
        
#ifdef _WIN32
        {
            std::lock_guard<std::mutex> lock(m_pipeMutex);
            if (m_connectionHandle && m_connectionHandle != INVALID_HANDLE_VALUE) {
                OVERLAPPED ol = { 0 };
                ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                DWORD bytesWritten = 0;
                if (!WriteFile(m_connectionHandle, rpc.c_str(), rpc.length(), NULL, &ol)) {
                    if (GetLastError() == ERROR_IO_PENDING) {
                        GetOverlappedResult(m_connectionHandle, &ol, &bytesWritten, TRUE);
                    } else {
                        std::cerr << "[IPC] WriteFile failed. Error: " << GetLastError() << std::endl;
                    }
                }
                CloseHandle(ol.hEvent);
            }
        }
#else
        if (m_connectionHandle) {
            int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_connectionHandle));
            write(fd, rpc.c_str(), rpc.length());
        }
#endif
    }

    void IPCClient::handleIncomingMessage(const std::string& jsonMessage) {
        std::lock_guard<std::mutex> lock(m_bufferMutex);
        m_incomingBuffer += jsonMessage;
        
        size_t pos = 0;
        while ((pos = m_incomingBuffer.find('\n')) != std::string::npos) {
            std::string line = m_incomingBuffer.substr(0, pos);
            m_incomingBuffer.erase(0, pos + 1);
            if (m_onMessage) {
                m_onMessage(line);
            }
        }

        if (!m_activeBuffer) return;

        if (jsonMessage.find("\"method\":\"insert\"") != std::string::npos) {
            size_t offsetPos = jsonMessage.find("\"offset\":");
            size_t textPos = jsonMessage.find("\"text\":\"");
            if (offsetPos != std::string::npos && textPos != std::string::npos) {
                // TODO: Properly parse JSON instead of string matching
                std::cout << "[IPC <- Node.js] Handled 'insert' request." << std::endl;
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
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1, 4096, 4096, 0, NULL);
                
            if (hPipe == INVALID_HANDLE_VALUE) {
                std::cerr << "[IPC] Failed to create named pipe. Error: " << GetLastError() << std::endl;
                return;
            }

            {
                std::lock_guard<std::mutex> lock(m_pipeMutex);
                m_connectionHandle = hPipe;
            }

            OVERLAPPED olConnect = { 0 };
            olConnect.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            BOOL connected = ConnectNamedPipe(hPipe, &olConnect) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (!connected && GetLastError() == ERROR_IO_PENDING) {
                HANDLE waitHandles[2] = { olConnect.hEvent, (HANDLE)m_stopEvent };
                DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                if (waitRes == WAIT_OBJECT_0) {
                    connected = TRUE;
                } else {
                    CancelIo(hPipe);
                }
            }

            if (connected && m_running) {
                char buffer[4096];
                OVERLAPPED olRead = { 0 };
                olRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
                
                while (m_running) {
                    DWORD bytesRead = 0;
                    BOOL readOk = ReadFile(hPipe, buffer, sizeof(buffer) - 1, NULL, &olRead);
                    if (!readOk && GetLastError() == ERROR_IO_PENDING) {
                        HANDLE waitHandles[2] = { olRead.hEvent, (HANDLE)m_stopEvent };
                        DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                        if (waitRes == WAIT_OBJECT_0) {
                            readOk = GetOverlappedResult(hPipe, &olRead, &bytesRead, FALSE);
                        } else {
                            CancelIo(hPipe);
                            break;
                        }
                    } else if (readOk) {
                        GetOverlappedResult(hPipe, &olRead, &bytesRead, FALSE);
                    }

                    if (readOk && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        handleIncomingMessage(std::string(buffer));
                    } else {
                        break;
                    }
                    ResetEvent(olRead.hEvent);
                }
                CloseHandle(olRead.hEvent);
            }
            
            CloseHandle(olConnect.hEvent);

            {
                std::lock_guard<std::mutex> lock(m_pipeMutex);
                if (m_connectionHandle == hPipe) {
                    DisconnectNamedPipe(hPipe);
                    CloseHandle(hPipe);
                    m_connectionHandle = nullptr;
                }
            }
#endif
        }
    }

} // namespace vrutti::core::ipc
