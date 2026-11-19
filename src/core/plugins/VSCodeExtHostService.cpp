#include "VSCodeExtHostService.h"
#include "../utils/Json.h"
#include "../config/SettingsManager.h"
#include <filesystem>
#include <cstdlib>
#include <algorithm>

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

bool VSCodeExtHostService::initialize() { return true; }
void VSCodeExtHostService::shutdown() {}
const char* VSCodeExtHostService::getName() const { return "VSCodeExtHostService"; }

// Helper functions for dynamic paths
static std::string getStorageHome(const std::string& type) {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) return std::string(appData) + "\\Vrutti\\" + type;
#else
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.config/Vrutti/" + type;
#endif
    return (fs::current_path() / type).string();
}

static int getParentPid() {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

std::string VSCodeExtHostService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "getInitData") {
        auto initData = std::make_shared<JsonNode>(JsonNode::Type::Object);
        initData->objectProperties.emplace_back("type", std::make_shared<JsonNode>(JsonNode::Type::Number));
        initData->objectProperties.back().second->numberValue = 0; // MessageType.Initialized

        initData->objectProperties.emplace_back("commit", std::make_shared<JsonNode>(JsonNode::Type::String));
        initData->objectProperties.back().second->stringValue = "dynamic-commit-id";

        initData->objectProperties.emplace_back("version", std::make_shared<JsonNode>(JsonNode::Type::String));
        initData->objectProperties.back().second->stringValue = "1.90.0"; // Target API compatibility

        initData->objectProperties.emplace_back("parentPid", std::make_shared<JsonNode>(JsonNode::Type::Number));
        initData->objectProperties.back().second->numberValue = getParentPid();

        auto envNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        envNode->objectProperties.emplace_back("isExtensionDevelopmentDebug", std::make_shared<JsonNode>(JsonNode::Type::Boolean));
        envNode->objectProperties.back().second->boolValue = false;
        
        auto appRootNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        appRootNode->objectProperties.emplace_back("$mid", std::make_shared<JsonNode>(JsonNode::Type::Number));
        appRootNode->objectProperties.back().second->numberValue = 1;
        appRootNode->objectProperties.emplace_back("path", std::make_shared<JsonNode>(JsonNode::Type::String));
        std::string appRootPath = fs::current_path().string();
        std::replace(appRootPath.begin(), appRootPath.end(), '\\', '/');
        if (!appRootPath.empty() && appRootPath[0] != '/') appRootPath = "/" + appRootPath;
        appRootNode->objectProperties.back().second->stringValue = appRootPath;
        appRootNode->objectProperties.emplace_back("scheme", std::make_shared<JsonNode>(JsonNode::Type::String));
        appRootNode->objectProperties.back().second->stringValue = "file";
        envNode->objectProperties.emplace_back("appRoot", appRootNode);
        
        envNode->objectProperties.emplace_back("appName", std::make_shared<JsonNode>(JsonNode::Type::String));
        envNode->objectProperties.back().second->stringValue = "Vrutti IDE";
        envNode->objectProperties.emplace_back("appUriScheme", std::make_shared<JsonNode>(JsonNode::Type::String));
        envNode->objectProperties.back().second->stringValue = "vrutti";
        envNode->objectProperties.emplace_back("appLanguage", std::make_shared<JsonNode>(JsonNode::Type::String));
        envNode->objectProperties.back().second->stringValue = "en";
        
        auto globalStorageNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        globalStorageNode->objectProperties.emplace_back("$mid", std::make_shared<JsonNode>(JsonNode::Type::Number));
        globalStorageNode->objectProperties.back().second->numberValue = 1;
        globalStorageNode->objectProperties.emplace_back("path", std::make_shared<JsonNode>(JsonNode::Type::String));
        
        std::string globalStoragePath = getStorageHome("globalStorage");
        std::replace(globalStoragePath.begin(), globalStoragePath.end(), '\\', '/');
        if (!globalStoragePath.empty() && globalStoragePath[0] != '/') globalStoragePath = "/" + globalStoragePath;
        globalStorageNode->objectProperties.back().second->stringValue = globalStoragePath;
        
        globalStorageNode->objectProperties.emplace_back("scheme", std::make_shared<JsonNode>(JsonNode::Type::String));
        globalStorageNode->objectProperties.back().second->stringValue = "file";
        envNode->objectProperties.emplace_back("globalStorageHome", globalStorageNode);
        
        auto workspaceStorageNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        workspaceStorageNode->objectProperties.emplace_back("$mid", std::make_shared<JsonNode>(JsonNode::Type::Number));
        workspaceStorageNode->objectProperties.back().second->numberValue = 1;
        workspaceStorageNode->objectProperties.emplace_back("path", std::make_shared<JsonNode>(JsonNode::Type::String));
        
        std::string workspaceStoragePath = getStorageHome("workspaceStorage");
        std::replace(workspaceStoragePath.begin(), workspaceStoragePath.end(), '\\', '/');
        if (!workspaceStoragePath.empty() && workspaceStoragePath[0] != '/') workspaceStoragePath = "/" + workspaceStoragePath;
        workspaceStorageNode->objectProperties.back().second->stringValue = workspaceStoragePath;
        
        workspaceStorageNode->objectProperties.emplace_back("scheme", std::make_shared<JsonNode>(JsonNode::Type::String));
        workspaceStorageNode->objectProperties.back().second->stringValue = "file";
        envNode->objectProperties.emplace_back("workspaceStorageHome", workspaceStorageNode);

        initData->objectProperties.emplace_back("environment", envNode);

        fs::path cwd = fs::current_path();
        auto workspaceNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        workspaceNode->objectProperties.emplace_back("id", std::make_shared<JsonNode>(JsonNode::Type::String));
        workspaceNode->objectProperties.back().second->stringValue = cwd.filename().string() + "-id";
        workspaceNode->objectProperties.emplace_back("name", std::make_shared<JsonNode>(JsonNode::Type::String));
        workspaceNode->objectProperties.back().second->stringValue = cwd.filename().string() + " Workspace";
        initData->objectProperties.emplace_back("workspace", workspaceNode);

        auto remoteNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        remoteNode->objectProperties.emplace_back("isRemote", std::make_shared<JsonNode>(JsonNode::Type::Boolean));
        remoteNode->objectProperties.back().second->boolValue = false;
        initData->objectProperties.emplace_back("remote", remoteNode);

        auto logsLocNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        logsLocNode->objectProperties.emplace_back("$mid", std::make_shared<JsonNode>(JsonNode::Type::Number));
        logsLocNode->objectProperties.back().second->numberValue = 1;
        logsLocNode->objectProperties.emplace_back("path", std::make_shared<JsonNode>(JsonNode::Type::String));
        
        std::string logsLocPath = getStorageHome("logs");
        std::replace(logsLocPath.begin(), logsLocPath.end(), '\\', '/');
        if (!logsLocPath.empty() && logsLocPath[0] != '/') logsLocPath = "/" + logsLocPath;
        logsLocNode->objectProperties.back().second->stringValue = logsLocPath;
        
        logsLocNode->objectProperties.emplace_back("scheme", std::make_shared<JsonNode>(JsonNode::Type::String));
        logsLocNode->objectProperties.back().second->stringValue = "file";
        initData->objectProperties.emplace_back("logsLocation", logsLocNode);

        initData->objectProperties.emplace_back("logLevel", std::make_shared<JsonNode>(JsonNode::Type::Number));
        initData->objectProperties.back().second->numberValue = 1;
        
        initData->objectProperties.emplace_back("autoStart", std::make_shared<JsonNode>(JsonNode::Type::Boolean));
        initData->objectProperties.back().second->boolValue = true;
        
        initData->objectProperties.emplace_back("uiKind", std::make_shared<JsonNode>(JsonNode::Type::Number));
        initData->objectProperties.back().second->numberValue = 1; // UIKind.Desktop
        
        auto consoleNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        consoleNode->objectProperties.emplace_back("includeStack", std::make_shared<JsonNode>(JsonNode::Type::Boolean));
        consoleNode->objectProperties.back().second->boolValue = false;
        consoleNode->objectProperties.emplace_back("logNative", std::make_shared<JsonNode>(JsonNode::Type::Boolean));
        consoleNode->objectProperties.back().second->boolValue = false;
        initData->objectProperties.emplace_back("consoleForward", consoleNode);
        
        auto telemetryNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        telemetryNode->objectProperties.emplace_back("sessionId", std::make_shared<JsonNode>(JsonNode::Type::String));
        telemetryNode->objectProperties.back().second->stringValue = "dynamic-session-id";
        telemetryNode->objectProperties.emplace_back("machineId", std::make_shared<JsonNode>(JsonNode::Type::String));
        telemetryNode->objectProperties.back().second->stringValue = "dynamic-machine-id";
        telemetryNode->objectProperties.emplace_back("sqmId", std::make_shared<JsonNode>(JsonNode::Type::String));
        telemetryNode->objectProperties.back().second->stringValue = "dynamic-sqm-id";
        telemetryNode->objectProperties.emplace_back("devDeviceId", std::make_shared<JsonNode>(JsonNode::Type::String));
        telemetryNode->objectProperties.back().second->stringValue = "dynamic-dev-device-id";
        telemetryNode->objectProperties.emplace_back("firstSessionDate", std::make_shared<JsonNode>(JsonNode::Type::String));
        telemetryNode->objectProperties.back().second->stringValue = "2024-01-01";
        initData->objectProperties.emplace_back("telemetryInfo", telemetryNode);

        std::string extJsonOutput = "[]";
#ifdef _WIN32
        HMODULE handle = LoadLibraryA("ExtensionManager.dll");
        if (handle) {
            using CreateFunc = vrutti::core::plugins::IPlugin*(*)();
            using DestroyFunc = void(*)(vrutti::core::plugins::IPlugin*);
            CreateFunc create = (CreateFunc)GetProcAddress(handle, "createPlugin");
            DestroyFunc destroy = (DestroyFunc)GetProcAddress(handle, "destroyPlugin");
            if (create && destroy) {
                if (auto plugin = create()) {
                    extJsonOutput = plugin->executeCommand("list", "");
                    destroy(plugin);
                }
            }
            FreeLibrary(handle);
        }
#else
        void* handle = dlopen("libExtensionManager.so", RTLD_LAZY);
        if (handle) {
            using CreateFunc = vrutti::core::plugins::IPlugin*(*)();
            using DestroyFunc = void(*)(vrutti::core::plugins::IPlugin*);
            CreateFunc create = (CreateFunc)dlsym(handle, "createPlugin");
            DestroyFunc destroy = (DestroyFunc)dlsym(handle, "destroyPlugin");
            if (create && destroy) {
                if (auto plugin = create()) {
                    extJsonOutput = plugin->executeCommand("list", "");
                    destroy(plugin);
                }
            }
            dlclose(handle);
        }
#endif
        auto allExtensionsArr = std::make_shared<JsonNode>(JsonNode::Type::Array);
        
        try {
            if (!extJsonOutput.empty()) {
                auto extsNode = JsonParser::parse(extJsonOutput);
                if (extsNode && extsNode->type == JsonNode::Type::Array) {
                    allExtensionsArr = extsNode;
                }
            }
        } catch (const std::exception& e) {}

        auto extensionsNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
        extensionsNode->objectProperties.emplace_back("allExtensions", allExtensionsArr);
        extensionsNode->objectProperties.emplace_back("myExtensions", allExtensionsArr);
        initData->objectProperties.emplace_back("extensions", extensionsNode);

        return JsonSerializer::stringify(initData, 0, false);
    }
    return "{}";
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
