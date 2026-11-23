#pragma once
#include <string>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include "IPCClient.h"
#include "core/editor/DocumentStore.h"
#include "core/fs/FileService.h"
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

    // True when Vrutti has a handler for the call. A call without a handler is
    // answered with the benign default of its return type, so this tells a
    // caller whether the answer is the real value or the default.
    bool hasHandler(const std::string& shape, const std::string& method) const;

    // Handles a request issued by the webview (app-level, not extension host RPC).
    void handleLocalRequest(const std::string& method,
                            const std::shared_ptr<JsonNode>& params);

    // Sends a request to an Extension Host actor, addressed by numeric proxy id.
    int sendRequest(int proxyId,
                    const std::string& method,
                    const std::shared_ptr<JsonNode>& args,
                    std::function<void(const std::shared_ptr<JsonNode>&)> onResolve = nullptr,
                    std::function<void(const std::string&, const std::string&)> onReject = nullptr);

    // Sends a request to an Extension Host actor, addressed by shape name.
    int sendRequest(const char* shapeName,
                    const std::string& method,
                    const std::shared_ptr<JsonNode>& args,
                    std::function<void(const std::shared_ptr<JsonNode>&)> onResolve = nullptr,
                    std::function<void(const std::string&, const std::string&)> onReject = nullptr);

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

    // A handler that answers later.
    //
    // Some calls cannot be answered on the spot: the answer may come from the
    // extension host itself, which is the peer that sent the request. The
    // handler gets the callbacks it must use to answer, and the router keeps
    // its hands off the request until one of them is called.
    //
    // The failure callback takes the message and the name of the error. The
    // name is the code the extension host expects, for example
    // "FileNotFound".
    using AsyncHandler = std::function<void(
        const std::shared_ptr<JsonNode>& args,
        std::function<void(const std::shared_ptr<JsonNode>&)> reply,
        std::function<void(const std::string&, const std::string&)> fail)>;

    // Registers a handler that answers later.
    void setAsyncHandler(const std::string& shape, const std::string& method,
                         AsyncHandler handler);

private:
    void registerHandlers();
    void onExtensionHostReady();
    void sendInitData();
    void sendConfigurationHandshake();
    void sendWorkspaceHandshake();
    void sendStartupActivation();

    // Publishes the list of available colour or icon themes to the UI.
    void emitThemeList(const std::string& method, bool icons);

    // --- the file system ------------------------------------------------------

    // The handle of the provider that serves the scheme of the URI, or -1
    // when the URI belongs to the native file system.
    int providerForUri(const std::shared_ptr<JsonNode>& uri) const;

    // Forwards a file system call to the provider of an extension.
    //
    // The handle of the provider is the first argument of the call on the
    // `ExtHostFileSystem` actor, the rest follows.
    void forwardToProvider(int handle,
                           const char* method,
                           const std::vector<std::shared_ptr<JsonNode>>& args,
                           const std::function<void(const std::shared_ptr<JsonNode>&)>& reply,
                           const std::function<void(const std::string&, const std::string&)>& fail);

    // --- the documents ---------------------------------------------------------

    // Publishes a document to the extension host.
    //
    // The host keeps a mirror of every document, and the text of the mirror
    // is what an extension reads. The mirror is fed by the delta of
    // `ExtHostDocumentsAndEditors.$acceptDocumentsAndEditorsDelta`.
    //
    // The call is answered when the host has taken the delta, because the
    // host resolves `workspace.openTextDocument()` only when the document is
    // in its mirror.
    void publishDocument(const std::string& key,
                         const std::shared_ptr<JsonNode>& uri,
                         std::function<void(bool, const std::string&)> done);

    // Tells the host about the documents that the store had to drop, so that
    // its mirror does not keep documents that no longer exist.
    void publishRemovedDocuments();

    // The file system of the core, used for the `file:` scheme.
    vrutti::core::fs::FileService m_fileService;

    // The documents of the core: the text that the extension host mirrors.
    vrutti::core::editor::DocumentStore m_documents;

    // The providers that extensions registered, by scheme and by handle.
    std::unordered_map<std::string, int> m_fsProviders;
    std::unordered_map<int, std::string> m_fsProviderSchemes;

    IPCClient* m_ipc;
    std::unordered_map<std::string, Handler> m_handlers;

    // The handlers that answer later.
    std::unordered_map<std::string, AsyncHandler> m_asyncHandlers;

    // The calls that were answered with the default because Vrutti has no
    // implementation for them. Each one is reported only once, so the log
    // shows the gap without repeating it on every call.
    std::set<std::string> m_reportedGaps;

    // Commands registered by extensions through MainThreadCommands.$registerCommand.
    std::vector<std::string> m_registeredCommands;

    // Status bar entries contributed by extensions.
    std::unordered_map<std::string, std::shared_ptr<JsonNode>> m_statusBarEntries;

    std::string m_workspaceFolder;
    bool m_initialized = false;
    bool m_handshakeDone = false;
};

} // namespace vrutti::core::ipc
