#include "VSCodeProtocol.h"
#include <iostream>
#include <memory>
#include <string>
#include <filesystem>
#include "core/utils/Json.h"
#include "core/plugins/PluginLoader.h"

using namespace vrutti::core::utils;

namespace {
    // Static helper functions (not class members)
    std::string getAppRoot() {
        return std::filesystem::current_path().string();
    }

    std::string getLogsLocation() {
#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        if (appData) return std::string(appData) + "\\Vrutti\\logs";
#else
        const char* home = std::getenv("HOME");
        if (home) return std::string(home) + "/.config/Vrutti/logs";
#endif
        return (std::filesystem::current_path() / "logs").string();
    }
} // namespace

namespace vrutti::core::ipc {

    VSCodeProtocol::VSCodeProtocol(IPCClient* ipc) : m_ipc(ipc) {}

    VSCodeProtocol::~VSCodeProtocol() {}

    void VSCodeProtocol::initialize() {
        // Set the IPCClient message callback to route messages here
        m_ipc->setOnMessage([this](const std::string& method, const std::shared_ptr<JsonNode>& params) {
            this->handleMessage(method, params);
        });

        // Proactively send the initialization data
        sendInitData();
    }

    void VSCodeProtocol::sendInitData() {
        vrutti::core::plugins::PluginLoader pluginManager;
        auto plugin = pluginManager.loadPlugin("VSCodeExtHostService.dll");
        if (plugin) {
            std::string payload = plugin->executeCommand("getInitData", "");

            auto params = std::make_shared<JsonNode>(JsonNode::Type::Object);
            params->objectProperties.emplace_back("initData", std::make_shared<JsonNode>(JsonNode::Type::String));
            params->objectProperties.back().second->stringValue = payload;

            m_ipc->sendNotification("VSCODE_EXTHOST_IPC_INIT", params);
        }
    }

    void VSCodeProtocol::handleMessage(const std::string& method, const std::shared_ptr<JsonNode>& params) {
        std::cout << "[VSCodeProtocol] Received: " << method << std::endl;

        if (method == "MainThreadCommands.$registerCommand") {
            if (params && params->type == JsonNode::Type::Array && !params->arrayElements.empty()) {
                std::cout << "[VSCodeProtocol] Registered command: " << params->arrayElements[0]->stringValue << std::endl;
            }
        }
        else if (method == "MainThreadDocuments.$trySaveDocument") {
            std::cout << "[VSCodeProtocol] Trying to save document" << std::endl;
        }
        // ... we will expand this with a full router map ...
    }

    void VSCodeProtocol::sendNotification(const std::string& method, const std::shared_ptr<JsonNode>& params) {
        m_ipc->sendNotification(method, params);
    }

    int VSCodeProtocol::sendRequest(const std::string& method, const std::shared_ptr<JsonNode>& params,
                                    std::function<void(const std::shared_ptr<JsonNode>&)> onResolve,
                                    std::function<void(const std::shared_ptr<JsonNode>&)> onReject) {
        return m_ipc->sendRequest(method, params, std::move(onResolve), std::move(onReject));
    }

} // namespace vrutti::core::ipc
