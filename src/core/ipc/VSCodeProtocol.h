#pragma once
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include "IPCClient.h"
#include "core/utils/Json.h"

namespace vrutti::core::ipc {

using vrutti::core::utils::JsonNode;

// -----------------------------------------------------------------------------
// VSCodeProtocol -- renderer-side router for the VS Code Extension Host RPC.
//
// The Extension Host (upstream `extensionHostProcess`, Vrutti-patched to speak
// newline-delimited JSON) invokes "MainThread*" actors that live in the
// renderer. This class:
//
//   * owns the start-up handshake (init data -> workspace -> activation),
//   * dispatches inbound RPC calls to C++ handlers,
//   * ALWAYS replies -- an unanswered request leaves the calling extension
//     blocked inside `await` forever.
//
// Outbound calls (renderer -> extension host) use the "ExtHost*" actors, e.g.
// ExtHostWorkspace.$initializeWorkspace and
// ExtHostExtensionService.$activateByEvent.
// -----------------------------------------------------------------------------
class VSCodeProtocol {
public:
    VSCodeProtocol(IPCClient* ipc);
    ~VSCodeProtocol();

    // Registers the inbound callbacks on the IPC client and prepares handlers.
    void initialize();

    // Dispatches one inbound RPC request and sends the corresponding reply.
    void handleRequest(const IPCClient::RpcRequest& req);

    // Handles a request issued by the webview (app-level, not extension host RPC).
    void handleLocalRequest(const std::string& method,
                            const std::shared_ptr<JsonNode>& params);

    // Sends a request to an Extension Host actor, addressed by numeric proxy id.
    int sendRequest(int proxyId,
                    const std::string& method,
                    const std::shared_ptr<JsonNode>& args,
                    std::function<void(const std::shared_ptr<JsonNode>&)> onResolve = nullptr,
                    std::function<void(const std::string&)> onReject = nullptr);

    // Sends a request to an Extension Host actor, addressed by shape name.
    int sendRequest(const char* shapeName,
                    const std::string& method,
                    const std::shared_ptr<JsonNode>& args,
                    std::function<void(const std::shared_ptr<JsonNode>&)> onResolve = nullptr,
                    std::function<void(const std::string&)> onReject = nullptr);

    // Absolute path of the folder that was opened on startup (may be empty).
    void setWorkspaceFolder(const std::string& path) { m_workspaceFolder = path; }
    const std::string& workspaceFolder() const { return m_workspaceFolder; }

    // Forwards an event to the UI layer (webview).
    void emitToUi(const std::string& method, const std::shared_ptr<JsonNode>& params);

    // Result of a handled RPC call.
    struct HandlerResult {
        bool ok = true;
        std::shared_ptr<JsonNode> value;      // result payload (null == undefined)
        std::string errorMessage;
        std::string errorName = "Error";
    };

    using Handler = std::function<HandlerResult(const std::shared_ptr<JsonNode>& args)>;

private:
    void registerHandlers();
    void onExtensionHostReady();
    void sendInitData();
    void sendConfigurationHandshake();
    void sendWorkspaceHandshake();
    void sendStartupActivation();

    // Publishes the list of available colour or icon themes to the UI.
    void emitThemeList(const std::string& method, bool icons);

    IPCClient* m_ipc;
    std::unordered_map<std::string, Handler> m_handlers;

    // Commands registered by extensions through MainThreadCommands.$registerCommand.
    std::vector<std::string> m_registeredCommands;

    // Status bar entries contributed by extensions.
    std::unordered_map<std::string, std::shared_ptr<JsonNode>> m_statusBarEntries;

    std::string m_workspaceFolder;
    bool m_initialized = false;
    bool m_handshakeDone = false;
};

} // namespace vrutti::core::ipc
