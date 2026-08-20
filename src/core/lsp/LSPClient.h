#pragma once

#include <string>
#include <memory>
#include <functional>

namespace vrutti {
namespace core {
namespace lsp {

struct InitializeParams {
    std::string rootUri;
};

class LSPClient {
public:
    LSPClient(const std::string& serverCommand);
    ~LSPClient();

    bool Start();
    void Stop();

    void Initialize(const InitializeParams& params, std::function<void(bool success)> callback);
    void DidOpen(const std::string& uri, const std::string& text, int version, const std::string& languageId);
    void DidChange(const std::string& uri, const std::string& text, int version);
    void DidClose(const std::string& uri);

private:
    std::string m_serverCommand;
    // Process and IPC handles would go here
};

} // namespace lsp
} // namespace core
} // namespace vrutti
