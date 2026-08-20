#include "LSPClient.h"
#include <iostream>

namespace vrutti {
namespace core {
namespace lsp {

LSPClient::LSPClient(const std::string& serverCommand)
    : m_serverCommand(serverCommand) {
}

LSPClient::~LSPClient() {
    Stop();
}

bool LSPClient::Start() {
    // TODO: Spawn the language server process and establish stdio IPC
    std::cout << "Starting LSP server: " << m_serverCommand << std::endl;
    return true;
}

void LSPClient::Stop() {
    // TODO: Terminate the language server process
    std::cout << "Stopping LSP server" << std::endl;
}

void LSPClient::Initialize(const InitializeParams& params, std::function<void(bool success)> callback) {
    // TODO: Send JSON-RPC initialize request
    std::cout << "LSP Initialize: " << params.rootUri << std::endl;
    if (callback) {
        callback(true);
    }
}

void LSPClient::DidOpen(const std::string& uri, const std::string& text, int version, const std::string& languageId) {
    // TODO: Send textDocument/didOpen notification
}

void LSPClient::DidChange(const std::string& uri, const std::string& text, int version) {
    // TODO: Send textDocument/didChange notification
}

void LSPClient::DidClose(const std::string& uri) {
    // TODO: Send textDocument/didClose notification
}

} // namespace lsp
} // namespace core
} // namespace vrutti
