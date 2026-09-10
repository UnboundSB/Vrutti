#include "VSCodeProtocol.h"
#include <iostream>
#include <nlohmann/json.hpp> // Assuming nlohmann/json is available for JSON-RPC

using json = nlohmann::json;

namespace vrutti::core::ipc {

    VSCodeProtocol::VSCodeProtocol(IPCClient* ipc) : m_ipc(ipc) {}

    VSCodeProtocol::~VSCodeProtocol() {}

    void VSCodeProtocol::initialize() {
        // Set the IPCClient message callback to route messages here
        m_ipc->setOnMessage([this](const std::string& msg) {
            this->handleMessage(msg);
        });

        // Wait for the Extension Host to connect and send VSCODE_EXTHOST_IPC_READY
        // Or proactively send the initialization data
        sendInitData();
    }

    void VSCodeProtocol::sendInitData() {
        // The official VS Code Extension Host expects a massive initialization payload
        // containing workspace details, environment variables, registered commands, etc.
        json initData = {
            {"type", 0}, // MessageType.Initialized
            {"commit", "unknown"},
            {"version", "1.90.0"},
            {"parentPid", 0},
            {"environment", {
                {"isExtensionDevelopmentDebug", false},
                {"appRoot", "d:\\vrutti"},
                {"appName", "Vrutti IDE"},
                {"appUriScheme", "vrutti"}
            }},
            {"workspace", {
                {"id", "vrutti-workspace"},
                {"name", "Vrutti Workspace"}
            }},
            {"remote", {
                {"isRemote", false}
            }},
            {"logsLocation", "d:\\vrutti\\logs"},
            {"logLevel", 1} // Trace
        };

        // Send this JSON over IPC
        std::string payload = initData.dump();
        // Since we are mocking the ExtHost, we format it as a VS Code RPC response
        m_ipc->sendMessage("VSCODE_EXTHOST_IPC_INIT", payload);
    }

    void VSCodeProtocol::handleMessage(const std::string& jsonMessage) {
        try {
            json msg = json::parse(jsonMessage);
            
            // Log incoming messages from the official VS Code ExtHost
            std::cout << "[VSCodeProtocol] Received: " << jsonMessage << std::endl;

            // Simple router for VS Code MainThread messages
            if (msg.contains("method")) {
                std::string method = msg["method"];
                
                // Route MainThreadCommands
                if (method == "MainThreadCommands.$registerCommand") {
                    std::cout << "[VSCodeProtocol] Registered command: " << msg["params"][0] << std::endl;
                }
                // Route MainThreadDocuments
                else if (method == "MainThreadDocuments.$trySaveDocument") {
                    std::cout << "[VSCodeProtocol] Trying to save document" << std::endl;
                }
                // ... we will expand this with a full router map ...
            }

        } catch (const std::exception& e) {
            std::cerr << "[VSCodeProtocol] Failed to parse message: " << e.what() << std::endl;
        }
    }

    void VSCodeProtocol::sendMessage(const std::string& method, const std::string& params) {
        m_ipc->sendMessage(method, params);
    }

} // namespace vrutti::core::ipc
