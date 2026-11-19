#pragma once
#include <string>
#include <functional>
#include <memory>
#include "IPCClient.h"
#include "core/utils/Json.h"

namespace vrutti::core::ipc {

// Handles the official VS Code RPC Protocol (JSON-RPC)
class VSCodeProtocol {
public:
    VSCodeProtocol(IPCClient* ipc);
    ~VSCodeProtocol();

    // Start listening to IPC messages and send initialization payload
    void initialize();

    // Process incoming JSON-RPC from the Extension Host (typed handler from IPCClient callback)
    void handleMessage(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params);

    // Send a JSON-RPC notification to the Extension Host
    void sendNotification(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params);

    // Send a JSON-RPC request to the Extension Host
    int sendRequest(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params,
                    std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> onResolve,
                    std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> onReject = nullptr);

private:
    IPCClient* m_ipc;

    void sendInitData();
};

} // namespace vrutti::core::ipc
