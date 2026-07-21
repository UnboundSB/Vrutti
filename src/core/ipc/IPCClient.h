#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include "core/editor/PieceTable.h"

namespace vrutti::core::ipc {

    // A lightweight IPC (Inter-Process Communication) endpoint.
    // This connects our ultra-fast C++ native core to the background Node.js
    // Extension Host process using JSON-RPC over Named Pipes/Domain Sockets.
    class IPCClient {
    public:
        IPCClient(const std::string& pipeName);
        ~IPCClient();

        // Starts listening for messages from the Node.js extension host on a background thread
        void start();
        void stop();

        // Sends an event back to Node.js (e.g., text changed, window resized)
        void sendMessage(const std::string& method, const std::string& payload);

        // Binds the active PieceTable so IPC can directly manipulate the C++ text buffer
        void bindEditorBuffer(vrutti::core::editor::PieceTable* table);

    private:
        std::string m_pipeName;
        bool m_running;
        vrutti::core::editor::PieceTable* m_activeBuffer;

        // Platform-specific connection handle (e.g. HANDLE for Windows Named Pipes)
        void* m_connectionHandle;

        void listenLoop();

        // Dispatches incoming JSON-RPC commands from VS Code extensions to the C++ core
        void handleIncomingMessage(const std::string& jsonMessage);
    };

} // namespace vrutti::core::ipc
