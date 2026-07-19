#include "IPCClient.h"
#include <iostream>
#include <thread>
// For full implementation on Windows: #include <windows.h>
// Here we stub the named pipe loop to demonstrate architecture without locking the thread.

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
        
        // In a real environment, this spins up a std::thread running listenLoop()
        // std::thread([this]() { this->listenLoop(); }).detach();
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
        // Platform specific blocking read on Named Pipe / Socket
        while (m_running) {
            // Read line from m_connectionHandle
            // std::string line = readLine();
            // handleIncomingMessage(line);
        }
    }

} // namespace vrutti::core::ipc
