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

using namespace vrutti::core::utils;

namespace vrutti::core::ipc {

IPCClient::IPCClient(const std::string& pipeName)
    : m_pipeName(pipeName), m_running(false), m_activeBuffer(nullptr),
      m_connectionHandle(nullptr), m_nextId(1)
{
#ifdef _WIN32
    m_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
}

IPCClient::~IPCClient() {
    stop();
#ifdef _WIN32
    if (m_stopEvent) CloseHandle(static_cast<HANDLE>(m_stopEvent));
#endif
}

void IPCClient::bindEditorBuffer(vrutti::core::editor::PieceTable* table) {
    m_activeBuffer = table;
}

void IPCClient::start() {
    if (m_running) return;
    m_running = true;
#ifdef _WIN32
    ResetEvent(static_cast<HANDLE>(m_stopEvent));
#endif

    std::thread([this]() {
        try { this->listenLoop(); }
        catch (...) { std::cerr << "[IPC] Fatal error in listen loop!\n"; }
    }).detach();

    std::cout << "[IPC] Bound to pipe/socket: " << m_pipeName << "\n";
    std::cout << "[IPC] Awaiting connection from Node.js Extension Host...\n";
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
    SetEvent(static_cast<HANDLE>(m_stopEvent));
    std::lock_guard<std::mutex> lock(m_pipeMutex);
    if (m_connectionHandle && m_connectionHandle != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(static_cast<HANDLE>(m_connectionHandle));
        CloseHandle(static_cast<HANDLE>(m_connectionHandle));
        m_connectionHandle = nullptr;
    }
#endif
}

std::string IPCClient::serializeJson(const std::shared_ptr<JsonNode>& node) {
    return JsonSerializer::stringify(node, 0, false);
}

void IPCClient::sendRaw(const std::string& payload) {
    if (!m_running) return;

#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        if (m_connectionHandle && m_connectionHandle != INVALID_HANDLE_VALUE && m_isConnected) {
            OVERLAPPED ol = { 0 };
            ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            DWORD bytesWritten = 0;
            if (!WriteFile(static_cast<HANDLE>(m_connectionHandle), payload.c_str(),
                           static_cast<DWORD>(payload.length()), NULL, &ol)) {
                if (GetLastError() == ERROR_IO_PENDING) {
                    GetOverlappedResult(static_cast<HANDLE>(m_connectionHandle), &ol, &bytesWritten, TRUE);
                } else {
                    std::cerr << "[IPC] WriteFile failed. Error: " << GetLastError() << "\n";
                }
            }
            CloseHandle(ol.hEvent);
        }
    }
#else
    if (m_connectionHandle) {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_connectionHandle));
        write(fd, payload.c_str(), payload.length());
    }
#endif
}

void IPCClient::sendNotification(const std::string& method, const std::shared_ptr<JsonNode>& params) {
    auto rpc = std::make_shared<JsonNode>(JsonNode::Type::Object);
    rpc->objectProperties.emplace_back("jsonrpc", std::make_shared<JsonNode>(JsonNode::Type::String));
    rpc->objectProperties.back().second->stringValue = "2.0";
    rpc->objectProperties.emplace_back("method", std::make_shared<JsonNode>(JsonNode::Type::String));
    rpc->objectProperties.back().second->stringValue = method;
    rpc->objectProperties.emplace_back("params", params);
    sendRaw(serializeJson(rpc) + "\n");
}

int IPCClient::sendRequest(const std::string& method, const std::shared_ptr<JsonNode>& params,
                           std::function<void(const std::shared_ptr<JsonNode>&)> onResolve,
                           std::function<void(const std::shared_ptr<JsonNode>&)> onReject) {
    int id = ++m_nextId;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        m_pendingCalls[id] = { std::move(onResolve), std::move(onReject) };
    }

    auto rpc = std::make_shared<JsonNode>(JsonNode::Type::Object);
    rpc->objectProperties.emplace_back("jsonrpc", std::make_shared<JsonNode>(JsonNode::Type::String));
    rpc->objectProperties.back().second->stringValue = "2.0";
    rpc->objectProperties.emplace_back("id", std::make_shared<JsonNode>(JsonNode::Type::Number));
    rpc->objectProperties.back().second->numberValue = static_cast<double>(id);
    rpc->objectProperties.emplace_back("method", std::make_shared<JsonNode>(JsonNode::Type::String));
    rpc->objectProperties.back().second->stringValue = method;
    rpc->objectProperties.emplace_back("params", params);
    sendRaw(serializeJson(rpc) + "\n");
    return id;
}

void IPCClient::resolvePending(int requestId, const std::shared_ptr<JsonNode>& payload, bool isError) {
    std::function<void(const std::shared_ptr<JsonNode>&)> callback;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        auto it = m_pendingCalls.find(requestId);
        if (it == m_pendingCalls.end()) return;
        callback = isError ? it->second.reject : it->second.resolve;
        m_pendingCalls.erase(it);
    }
    if (callback) callback(payload);
}

void IPCClient::handleIncomingMessage(const std::string& jsonMessage) {
    try {
        auto msg = JsonParser::parse(jsonMessage);
        if (!msg) return;

        // If it has an id, it's a response to one of our requests
        auto idNode = msg->get("id");
        if (idNode && idNode->type == JsonNode::Type::Number) {
            int id = static_cast<int>(idNode->numberValue);
            auto errorNode = msg->get("error");
            auto resultNode = msg->get("result");
            resolvePending(id, errorNode ? errorNode : resultNode, errorNode != nullptr);
            return;
        }

        // Otherwise it's a notification/request from Node.js
        auto methodNode = msg->get("method");
        if (methodNode && methodNode->type == JsonNode::Type::String) {
            std::string method(methodNode->stringValue.data(), methodNode->stringValue.length());
            auto paramsNode = msg->get("params");
            if (m_onMessage) {
                m_onMessage(method, paramsNode ? paramsNode : std::make_shared<JsonNode>(JsonNode::Type::Object));
            }
        }

        // Direct Piece Table integration (replaces fragile string matching)
        if (msg->get("method") && msg->get("method")->type == JsonNode::Type::String &&
            msg->get("method")->stringValue == "insert" && m_activeBuffer) {
            auto paramsNode = msg->get("params");
            if (paramsNode && paramsNode->type == JsonNode::Type::Object) {
                auto offsetNode = paramsNode->get("offset");
                auto textNode = paramsNode->get("text");
                if (offsetNode && textNode &&
                    offsetNode->type == JsonNode::Type::Number &&
                    textNode->type == JsonNode::Type::String) {
                    size_t offset = static_cast<size_t>(offsetNode->numberValue);
                    std::string text(textNode->stringValue.data(), textNode->stringValue.length());
                    m_activeBuffer->insert(offset, text);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[IPC] Failed to parse message: " << e.what() << "\n";
    }
}

void IPCClient::listenLoop() {
#ifdef _WIN32
    while (m_running) {
        std::string pipePath = "\\\\.\\pipe\\" + m_pipeName;
        HANDLE hPipe = CreateNamedPipeA(
            pipePath.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, 4096, 4096, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "[IPC] Failed to create named pipe. Error: " << GetLastError() << "\n";
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
            HANDLE waitHandles[2] = { olConnect.hEvent, static_cast<HANDLE>(m_stopEvent) };
            DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            if (waitRes == WAIT_OBJECT_0) {
                connected = TRUE;
            } else {
                CancelIo(hPipe);
                CloseHandle(olConnect.hEvent);
                continue;
            }
        }

        if (connected && m_running) {
            m_isConnected = true;
            if (m_onConnected) {
                m_onConnected();
            }
            char buffer[4096];
            OVERLAPPED olRead = { 0 };
            olRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            while (m_running) {
                DWORD bytesRead = 0;
                BOOL readOk = ReadFile(hPipe, buffer, sizeof(buffer) - 1, NULL, &olRead);
                if (!readOk && GetLastError() == ERROR_IO_PENDING) {
                    HANDLE waitHandles[2] = { olRead.hEvent, static_cast<HANDLE>(m_stopEvent) };
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
                    std::string line(buffer);

                    {
                        std::lock_guard<std::mutex> lock(m_bufferMutex);
                        m_incomingBuffer += line;
                        size_t pos = 0;
                        while ((pos = m_incomingBuffer.find('\n')) != std::string::npos) {
                            std::string complete = m_incomingBuffer.substr(0, pos);
                            m_incomingBuffer.erase(0, pos + 1);
                            handleIncomingMessage(complete);
                        }
                    }
                } else {
                    break;
                }
                ResetEvent(olRead.hEvent);
            }
            CloseHandle(olRead.hEvent);
            m_isConnected = false;
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
    }
#else
    while (m_running) {
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

                {
                    std::lock_guard<std::mutex> lock(m_bufferMutex);
                    m_incomingBuffer += buffer;
                    size_t pos = 0;
                    while ((pos = m_incomingBuffer.find('\n')) != std::string::npos) {
                        std::string complete = m_incomingBuffer.substr(0, pos);
                        m_incomingBuffer.erase(0, pos + 1);
                        handleIncomingMessage(complete);
                    }
                }
            }
            close(client_fd);
            m_connectionHandle = nullptr;
        }
        close(server_fd);
        unlink(m_pipeName.c_str());
    }
#endif
}

} // namespace vrutti::core::ipc
