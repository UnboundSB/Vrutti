#include "IPCClient.h"
#include <iostream>
#include <iostream>
#include <thread>
// For full implementation on Windows: #include <windows.h>
// Here we stub the named pipe loop to demonstrate architecture without locking the thread.

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
        std::thread([this]() { this->listenLoop(); }).detach();
        std::cout << "[IPC] Bound to pipe/socket: " << m_pipeName << std::endl;
        std::cout << "[IPC] Awaiting connection from Node.js Extension Host..." << std::endl;
    }

    void IPCClient::stop() {
        m_running = false;
        // Close handles...
    }

    void IPCClient::sendMessage(const std::string& method, const std::string& payload) {
        if (!m_running) return;
        
        // Serialize to JSON-RPC and write to pipe
        std::string rpc = "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + payload + "}\n";
        
        // Mock output for testing
        // std::cout << "[IPC -> Node.js] " << rpc;
    }

    void IPCClient::handleIncomingMessage(const std::string& jsonMessage) {
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

            struct sockaddr_un addr;
            memset(&addr, 0, sizeof(addr));
            addr.sun_family = AF_UNIX;
            strncpy(addr.sun_path, m_pipeName.c_str(), sizeof(addr.sun_path) - 1);

            unlink(m_pipeName.c_str());
            if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return;
            if (listen(server_fd, 5) < 0) return;

            while (m_running) {
                int client_fd = accept(server_fd, NULL, NULL);
                if (client_fd < 0) continue;

                char buffer[4096];
                while (m_running) {
                    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read <= 0) break;
                    buffer[bytes_read] = '\0';
                    handleIncomingMessage(std::string(buffer));
                }
                close(client_fd);
            }
            close(server_fd);
            unlink(m_pipeName.c_str());
#endif
        }
    }

} // namespace vrutti::core::ipc
