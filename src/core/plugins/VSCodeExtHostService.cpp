#include "VSCodeExtHostService.h"
#include "../utils/Json.h"
#include "../extensions/ExtensionService.h"
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
    //
    // The list comes from the service of the core, so the host and the user
    // interface always know the same extensions.
    // ---------------------------------------------------------------------
    auto extensionsObj = makeObject();
    setProp(extensionsObj, "versionId", numNode(1));

    auto activationEvents = makeObject();
    auto myExtensions = makeArray();

    const auto installed =
        vrutti::core::extensions::ExtensionService::listEnabled();
    auto allExtensions =
        vrutti::core::extensions::ExtensionService::descriptionsJson(installed);

    for (const auto& description : installed) {
        // activationEvents: { [extensionId]: string[] }
        auto events = makeArray();
        for (const auto& event : description.activationEvents) {
            events->arrayElements.push_back(strNode(event));
        }
        setProp(activationEvents, description.identifier, events);

        // myExtensions: ExtensionIdentifier[]
        auto myIdent = makeObject();
        setProp(myIdent, "value", strNode(description.identifier));
        setProp(myIdent, "_lower", strNode(description.lowerIdentifier));
        myExtensions->arrayElements.push_back(myIdent);
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
