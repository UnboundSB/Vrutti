#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include "core/editor/PieceTable.h"
#include "core/utils/Json.h"

namespace vrutti::core::ipc {

// A lightweight IPC (Inter-Process Communication) endpoint.
// Connects the C++ core to the background Node.js
// Extension Host process using JSON-RPC over Named Pipes/Domain Sockets.
// All messages use the existing vrutti::core::utils::Json types.
class IPCClient {
public:
    IPCClient(const std::string& pipeName);
    ~IPCClient();

    // Starts listening for messages from the Node.js extension host on a background thread
    void start();
    void stop();

    // Sends a notification (fire-and-forget, no response expected) back to Node.js
    void sendNotification(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params);

    // Sends a request with an ID; onResolve/onReject are invoked when the reply arrives.
    // Returns the assigned request ID.
    int sendRequest(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params,
                    std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> onResolve,
                    std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> onReject = nullptr);

    // Internal: called when a reply from Node.js arrives, to resolve the matching pending call
    void resolvePending(int requestId, const std::shared_ptr<vrutti::core::utils::JsonNode>& payload, bool isError);

    // Callback for arbitrary messages/notifications from Node.js
    void setOnMessage(std::function<void(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params)> cb) {
        m_onMessage = cb;
    }

    // Callback for when the Node.js process successfully connects to the IPC pipe
    void setOnConnected(std::function<void()> cb) { m_onConnected = cb; }

    // Binds the active PieceTable so IPC can directly manipulate the C++ text buffer
    void bindEditorBuffer(vrutti::core::editor::PieceTable* table);

    // Dispatches incoming JSON-RPC commands from extensions to the C++ core
    void handleIncomingMessage(const std::string& jsonMessage);

private:
    void sendRaw(const std::string& payload);
    void listenLoop();
    std::string serializeJson(const std::shared_ptr<vrutti::core::utils::JsonNode>& node);

    std::string m_pipeName;
    bool m_running;
    vrutti::core::editor::PieceTable* m_activeBuffer;
    bool m_isConnected = false;

    // Platform-specific connection handle (e.g. HANDLE for Windows Named Pipes)
    void* m_connectionHandle;
    std::mutex m_pipeMutex;

#ifdef _WIN32
    void* m_stopEvent;
#endif

    std::function<void(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params)> m_onMessage;
    std::function<void()> m_onConnected;

    std::mutex m_bufferMutex;
    std::string m_incomingBuffer;

    // Pending request tracking for promise resolution
    std::mutex m_pendingMutex;
    int m_nextId{1};
    struct PendingCall {
        std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> resolve;
        std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> reject;
    };
    std::unordered_map<int, PendingCall> m_pendingCalls;

};

} // namespace vrutti::core::ipc
