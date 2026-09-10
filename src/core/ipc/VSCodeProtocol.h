#pragma once
#include <string>
#include <functional>
#include <memory>
#include "IPCClient.h"

namespace vrutti::core::ipc {

    // Handles the official VS Code RPC Protocol (JSON-RPC)
    class VSCodeProtocol {
    public:
        VSCodeProtocol(IPCClient* ipc);
        ~VSCodeProtocol();

        // Start listening to IPC messages and send initialization payload
        void initialize();

        // Process incoming JSON-RPC from the Extension Host
        void handleMessage(const std::string& jsonMessage);

        // Send a JSON-RPC message to the Extension Host
        void sendMessage(const std::string& method, const std::string& params);

    private:
        IPCClient* m_ipc;

        void sendInitData();
    };

} // namespace vrutti::core::ipc
