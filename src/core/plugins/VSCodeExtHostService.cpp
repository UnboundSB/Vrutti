#include "VSCodeExtHostService.h"
#include "../utils/Json.h"
#include "../config/SettingsManager.h"
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;
namespace fs = std::filesystem;

namespace {

    std::shared_ptr<JsonNode> strNode(std::string value) {
        return vrutti::core::utils::makeJsonString(std::move(value));
    }

    std::shared_ptr<JsonNode> boolNode(bool value) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        node->boolValue = value;
        return node;
    }

    std::shared_ptr<JsonNode> numNode(double value) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Number);
        node->numberValue = value;
        return node;
    }

    std::shared_ptr<JsonNode> nullNode() {
        return std::make_shared<JsonNode>(JsonNode::Type::Null);
    }

    std::shared_ptr<JsonNode> makeObject() {
        return std::make_shared<JsonNode>(JsonNode::Type::Object);
    }

    std::shared_ptr<JsonNode> makeArray() {
        return std::make_shared<JsonNode>(JsonNode::Type::Array);
    }

    void setProp(const std::shared_ptr<JsonNode>& obj, const std::string& key,
                 const std::shared_ptr<JsonNode>& value) {
        if (!obj || obj->type != JsonNode::Type::Object) return;
        // Object keys are owned by the tree, so they stay valid for as long as
        // the node itself does.
        obj->objectProperties.emplace_back(key, value);
    }

    std::string toPosixPath(const std::string& p) {
        std::string out = p;
        std::replace(out.begin(), out.end(), '\\', '/');
        return out;
    }

    // Builds a `file:` UriComponents object (VS Code URI DTO shape).
    std::shared_ptr<JsonNode> makeFileUri(const std::string& nativePath) {
        std::string p = toPosixPath(nativePath);
        if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
            p[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(p[0])));
        }
        if (p.empty() || p[0] != '/') p = "/" + p;

        auto uri = makeObject();
        setProp(uri, "$mid", numNode(1)); // MarshalledId.Uri
        setProp(uri, "scheme", strNode("file"));
        setProp(uri, "path", strNode(p));
        return uri;
    }

    std::string getStorageHome(const std::string& type) {
#ifdef _WIN32
        const char* appData = std::getenv("APPDATA");
        if (appData) return std::string(appData) + "\\Vrutti\\" + type;
#else
        const char* home = std::getenv("HOME");
        if (home) return std::string(home) + "/.config/Vrutti/" + type;
#endif
        return (fs::current_path() / type).string();
    }

    int getParentPid() {
#ifdef _WIN32
        return _getpid();
#else
        return getpid();
#endif
    }

    // Loads the ExtensionManager plugin and returns the raw extension list.
    std::string listInstalledExtensions() {
#ifdef _WIN32
        HMODULE handle = LoadLibraryA("ExtensionManager.dll");
        if (!handle) return "[]";
        using CreateFunc = vrutti::core::plugins::IPlugin*(*)();
        using DestroyFunc = void(*)(vrutti::core::plugins::IPlugin*);
        CreateFunc create = (CreateFunc)GetProcAddress(handle, "createPlugin");
        DestroyFunc destroy = (DestroyFunc)GetProcAddress(handle, "destroyPlugin");
        std::string result = "[]";
        if (create && destroy) {
            if (auto plugin = create()) {
                result = plugin->executeCommand("list", "");
                destroy(plugin);
            }
        }
        FreeLibrary(handle);
        return result;
#else
        void* handle = dlopen("libExtensionManager.so", RTLD_LAZY);
        if (!handle) return "[]";
        using CreateFunc = vrutti::core::plugins::IPlugin*(*)();
        using DestroyFunc = void(*)(vrutti::core::plugins::IPlugin*);
        CreateFunc create = (CreateFunc)dlsym(handle, "createPlugin");
        DestroyFunc destroy = (DestroyFunc)dlsym(handle, "destroyPlugin");
        std::string result = "[]";
        if (create && destroy) {
            if (auto plugin = create()) {
                result = plugin->executeCommand("list", "");
                destroy(plugin);
            }
        }
        dlclose(handle);
        return result;
#endif
    }

} // namespace

bool VSCodeExtHostService::initialize() { return true; }
void VSCodeExtHostService::shutdown() {}
const char* VSCodeExtHostService::getName() const { return "VSCodeExtHostService"; }

std::string VSCodeExtHostService::executeCommand(const std::string& command, const std::string& payload) {
    if (command != "getInitData") return "{}";

    // `payload` is the absolute path of the folder opened on startup (may be empty).
    std::string workspaceFolder = payload;
    if (workspaceFolder.empty()) {
        workspaceFolder = fs::current_path().string();
    }

    // ---------------------------------------------------------------------
    // extensions: IExtensionDescriptionSnapshot
    // ---------------------------------------------------------------------
    auto extensionsObj = makeObject();
    setProp(extensionsObj, "versionId", numNode(1));

    auto allExtensions = makeArray();
    auto activationEvents = makeObject();
    auto myExtensions = makeArray();

    std::string extJsonOutput = listInstalledExtensions();
    try {
        auto extsNode = JsonParser::parse(extJsonOutput);
        if (extsNode && extsNode->type == JsonNode::Type::Array) {
            allExtensions = extsNode;

            for (const auto& desc : extsNode->arrayElements) {
                if (!desc || desc->type != JsonNode::Type::Object) continue;

                std::string id;
                auto identifierNode = desc->get("identifier");
                if (identifierNode && identifierNode->type == JsonNode::Type::Object) {
                    auto valueNode = identifierNode->get("value");
                    if (valueNode && valueNode->type == JsonNode::Type::String) {
                        id = std::string(valueNode->stringValue.data(), valueNode->stringValue.length());
                    }
                }
                if (id.empty()) continue;

                // activationEvents: { [extensionId]: string[] }
                auto events = makeArray();
                auto eventsNode = desc->get("activationEvents");
                if (eventsNode && eventsNode->type == JsonNode::Type::Array) {
                    for (const auto& ev : eventsNode->arrayElements) {
                        if (ev && ev->type == JsonNode::Type::String) {
                            events->arrayElements.push_back(ev);
                        }
                    }
                }
                setProp(activationEvents, id, events);

                // myExtensions: ExtensionIdentifier[]
                auto myIdent = makeObject();
                auto valueNode = identifierNode->get("value");
                auto lowerNode = identifierNode->get("_lower");
                setProp(myIdent, "value", valueNode ? valueNode : strNode(id));
                setProp(myIdent, "_lower", lowerNode ? lowerNode : strNode(id));
                myExtensions->arrayElements.push_back(myIdent);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[VSCodeExtHostService] Failed to parse extension list: " << e.what() << "\n";
    }

    setProp(extensionsObj, "allExtensions", allExtensions);
    setProp(extensionsObj, "activationEvents", activationEvents);
    setProp(extensionsObj, "myExtensions", myExtensions);

    // ---------------------------------------------------------------------
    // environment: IEnvironment
    // ---------------------------------------------------------------------
    auto envNode = makeObject();
    setProp(envNode, "isExtensionDevelopmentDebug", boolNode(false));
    setProp(envNode, "appName", strNode("Vrutti IDE"));
    setProp(envNode, "appHost", strNode("desktop"));
    setProp(envNode, "appRoot", makeFileUri(fs::current_path().string()));
    setProp(envNode, "appLanguage", strNode("en"));
    setProp(envNode, "isExtensionTelemetryLoggingOnly", boolNode(false));
    setProp(envNode, "appUriScheme", strNode("vrutti"));
    setProp(envNode, "globalStorageHome", makeFileUri(getStorageHome("globalStorage")));
    setProp(envNode, "workspaceStorageHome", makeFileUri(getStorageHome("workspaceStorage")));

    // ---------------------------------------------------------------------
    // workspace: IStaticWorkspaceData
    // ---------------------------------------------------------------------
    fs::path wsPath(workspaceFolder);
    std::string wsName = wsPath.filename().string();
    if (wsName.empty()) wsName = workspaceFolder;
    std::string wsId = wsName;
    std::transform(wsId.begin(), wsId.end(), wsId.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    auto workspaceNode = makeObject();
    setProp(workspaceNode, "id", strNode(wsId + "-" + std::to_string(getParentPid())));
    setProp(workspaceNode, "name", strNode(wsName));
    setProp(workspaceNode, "configuration", nullNode());
    setProp(workspaceNode, "isUntitled", boolNode(false));

    // ---------------------------------------------------------------------
    // telemetryInfo
    // ---------------------------------------------------------------------
    auto telemetryNode = makeObject();
    setProp(telemetryNode, "sessionId", strNode("vrutti-session"));
    setProp(telemetryNode, "machineId", strNode("vrutti-machine"));
    setProp(telemetryNode, "sqmId", strNode("vrutti-sqm"));
    setProp(telemetryNode, "devDeviceId", strNode("vrutti-device"));
    setProp(telemetryNode, "firstSessionDate", strNode("2024-01-01"));

    // ---------------------------------------------------------------------
    // remote
    // ---------------------------------------------------------------------
    auto remoteNode = makeObject();
    setProp(remoteNode, "isRemote", boolNode(false));
    setProp(remoteNode, "authority", nullNode());
    setProp(remoteNode, "connectionData", nullNode());

    // ---------------------------------------------------------------------
    // consoleForward
    // ---------------------------------------------------------------------
    auto consoleNode = makeObject();
    setProp(consoleNode, "includeStack", boolNode(false));
    setProp(consoleNode, "logNative", boolNode(false));

    // ---------------------------------------------------------------------
    // Assemble IExtensionHostInitData
    // ---------------------------------------------------------------------
    auto initData = makeObject();
    setProp(initData, "version", strNode("1.90.0"));
    setProp(initData, "quality", strNode("stable"));
    setProp(initData, "commit", strNode("vrutti"));
    setProp(initData, "date", strNode("2024-01-01"));
    setProp(initData, "parentPid", numNode(getParentPid()));
    setProp(initData, "environment", envNode);
    setProp(initData, "workspace", workspaceNode);
    setProp(initData, "extensions", extensionsObj);
    setProp(initData, "nlsBaseUrl", makeFileUri((fs::current_path() / "src" / "ext" / "nls").string()));
    setProp(initData, "telemetryInfo", telemetryNode);
    setProp(initData, "logLevel", numNode(1)); // LogLevel.Info
    setProp(initData, "loggers", makeArray());
    setProp(initData, "logsLocation", makeFileUri(getStorageHome("logs")));
    setProp(initData, "autoStart", boolNode(true));
    setProp(initData, "remote", remoteNode);
    setProp(initData, "consoleForward", consoleNode);
    setProp(initData, "uiKind", numNode(1)); // UIKind.Desktop

    return JsonSerializer::stringify(initData, 0, false);
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new VSCodeExtHostService();
    }

    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
