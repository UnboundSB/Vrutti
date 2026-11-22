#include "VSCodeProtocol.h"
#include "../utils/ErrorCodes.h"
#include "../utils/Paths.h"
#include "VsCodeProxyIds.generated.h"
#include "VsCodeProxyFallbacks.generated.h"
#include "core/fs/FileService.h"
#include "core/fs/URI.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <sstream>
#include <set>
#include "core/utils/Json.h"
#include "core/plugins/PluginLoader.h"

using namespace vrutti::core::utils;
// The alias is not `fs`: the enclosing namespace `vrutti::core` has a folder
// called `fs`, and that name would win over a global alias.
namespace stdfs = std::filesystem;

namespace {

    // --- argument helpers ------------------------------------------------------

    std::shared_ptr<JsonNode> argAt(const std::shared_ptr<JsonNode>& args, size_t index) {
        if (!args || args->type != JsonNode::Type::Array) return nullptr;
        if (index >= args->arrayElements.size()) return nullptr;
        return args->arrayElements[index];
    }

    std::string argString(const std::shared_ptr<JsonNode>& args, size_t index) {
        auto node = argAt(args, index);
        if (!node || node->type != JsonNode::Type::String) return "";
        return std::string(node->stringValue.data(), node->stringValue.length());
    }

    double argNumber(const std::shared_ptr<JsonNode>& args, size_t index) {
        auto node = argAt(args, index);
        if (!node || node->type != JsonNode::Type::Number) return 0.0;
        return node->numberValue;
    }

    bool argBool(const std::shared_ptr<JsonNode>& args, size_t index) {
        auto node = argAt(args, index);
        if (!node || node->type != JsonNode::Type::Boolean) return false;
        return node->boolValue;
    }

    // A flag of an options object, for example `overwrite` of the options of
    // a rename. The flag keeps the value it was given when it is missing.
    bool optionBool(const std::shared_ptr<JsonNode>& options, const char* name,
                    bool fallback = false) {
        if (!options || options->type != JsonNode::Type::Object) return fallback;
        auto node = options->get(name);
        if (!node || node->type != JsonNode::Type::Boolean) return fallback;
        return node->boolValue;
    }

    // --- node builders ---------------------------------------------------------

    std::shared_ptr<JsonNode> makeString(std::string value) {
        return vrutti::core::utils::makeJsonString(std::move(value));
    }

    std::shared_ptr<JsonNode> makeNumber(double value) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Number);
        node->numberValue = value;
        return node;
    }

    std::shared_ptr<JsonNode> makeBool(bool value) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        node->boolValue = value;
        return node;
    }

    std::shared_ptr<JsonNode> makeNull() {
        return std::make_shared<JsonNode>(JsonNode::Type::Null);
    }

    std::shared_ptr<JsonNode> makeObject() {
        return std::make_shared<JsonNode>(JsonNode::Type::Object);
    }

    std::shared_ptr<JsonNode> makeArray() {
        return std::make_shared<JsonNode>(JsonNode::Type::Array);
    }

    // --- the benign answer for a call without an implementation ---------------

    // The fallback table is sorted by key, so it can be searched.
    const vrutti::core::ipc::FallbackEntry* findFallback(const std::string& key) {
        int low = 0;
        int high = vrutti::core::ipc::FALLBACK_COUNT - 1;
        while (low <= high) {
            int middle = (low + high) / 2;
            int order = key.compare(vrutti::core::ipc::FALLBACKS[middle].key);
            if (order == 0) return &vrutti::core::ipc::FALLBACKS[middle];
            if (order < 0) high = middle - 1;
            else low = middle + 1;
        }
        return nullptr;
    }

    std::shared_ptr<JsonNode> fallbackValue(vrutti::core::ipc::FallbackKind kind) {
        switch (kind) {
            case vrutti::core::ipc::FallbackKind::EmptyString: return makeString("");
            case vrutti::core::ipc::FallbackKind::Zero:       return makeNumber(0);
            case vrutti::core::ipc::FallbackKind::False:      return makeBool(false);
            case vrutti::core::ipc::FallbackKind::EmptyArray: return makeArray();
            case vrutti::core::ipc::FallbackKind::Null:
            default:                                          return makeNull();
        }
    }

    void setProp(const std::shared_ptr<JsonNode>& obj, const std::string& key,
                 const std::shared_ptr<JsonNode>& value) {
        if (!obj || obj->type != JsonNode::Type::Object) return;
        // Object keys are owned by the tree, so they stay valid for as long as
        // the node itself does.
        obj->objectProperties.emplace_back(key, value);
    }

    // Builds an empty `IConfigurationModel`: { contents, keys, overrides }.
    // An empty object is *not* a valid model -- the host throws while parsing
    // it, which breaks every extension that reads a setting.
    std::shared_ptr<JsonNode> makeConfigurationModel() {
        auto model = makeObject();
        setProp(model, "contents", makeObject());
        setProp(model, "keys", makeArray());
        setProp(model, "overrides", makeArray());
        return model;
    }

    // --- URI helpers -----------------------------------------------------------

    // Builds a `file:` UriComponents object from a native filesystem path.
    // VS Code represents Windows paths as "/c:/dir/file" in the `path` field.
    std::shared_ptr<JsonNode> makeFileUri(const std::string& nativePath) {
        std::string p = nativePath;
        std::replace(p.begin(), p.end(), '\\', '/');

        // Lowercase a leading Windows drive letter, matching URI.file().
        if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
            p[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(p[0])));
        }
        if (p.empty() || p[0] != '/') p = "/" + p;

        auto uri = makeObject();
        setProp(uri, "scheme", makeString("file"));
        setProp(uri, "path", makeString(p));
        return uri;
    }

    // Extracts a native filesystem path from a `file:` UriComponents object.
    std::string uriToPath(const std::shared_ptr<JsonNode>& uri) {
        if (!uri || uri->type != JsonNode::Type::Object) return "";
        auto schemeNode = uri->get("scheme");
        if (schemeNode && schemeNode->type == JsonNode::Type::String &&
            schemeNode->stringValue != "file") {
            return "";
        }
        auto pathNode = uri->get("path");
        if (!pathNode || pathNode->type != JsonNode::Type::String) return "";
        std::string p(pathNode->stringValue.data(), pathNode->stringValue.length());

        // "/c:/dir/file" -> "c:/dir/file"
        if (p.size() >= 3 && p[0] == '/' && std::isalpha(static_cast<unsigned char>(p[1])) && p[2] == ':') {
            p = p.substr(1);
        }
#ifdef _WIN32
        std::replace(p.begin(), p.end(), '/', '\\');
#endif
        return p;
    }

    std::string sanitizeExtensionId(const std::string& id) {
        std::string out;
        out.reserve(id.size());
        for (char c : id) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_') {
                out += c;
            }
        }
        return out;
    }

    // The scheme of a URI object, for example "file".
    std::string uriScheme(const std::shared_ptr<JsonNode>& uri) {
        if (!uri || uri->type != JsonNode::Type::Object) return "";
        auto node = uri->get("scheme");
        if (!node || node->type != JsonNode::Type::String) return "";
        return std::string(node->stringValue.data(), node->stringValue.length());
    }

    // The options of a file system call: an object with the flag `overwrite`
    // or `recursive` and the other flags that travel with it.
    std::shared_ptr<JsonNode> boolOption(const std::shared_ptr<JsonNode>& options,
                                         const char* name, bool fallback) {
        auto result = makeObject();
        bool value = fallback;
        if (options && options->type == JsonNode::Type::Object) {
            auto node = options->get(name);
            if (node && node->type == JsonNode::Type::Boolean) value = node->boolValue;
        }
        setProp(result, name, makeBool(value));
        return result;
    }

} // namespace

namespace vrutti::core::ipc {

VSCodeProtocol::VSCodeProtocol(IPCClient* ipc) : m_ipc(ipc) {}

VSCodeProtocol::~VSCodeProtocol() {}

void VSCodeProtocol::initialize() {
    if (m_initialized) return;
    m_initialized = true;

    // The Extension Host announces itself with {"type":"ready"} once its socket
    // is up. Only then is it safe to push the init payload: the host waits for
    // it before constructing any of its services.
    m_ipc->setOnReady([this]() {
        this->onExtensionHostReady();
    });

    m_ipc->setOnRequest([this](const IPCClient::RpcRequest& req) {
        this->handleRequest(req);
    });

    // Requests coming from the webview. These are app-level commands, not
    // extension host RPC -- they are either handled locally or translated into
    // a proper extension host RPC call.
    m_ipc->setOnLocalRequest([this](const std::string& method,
                                    const std::shared_ptr<JsonNode>& params) {
        this->handleLocalRequest(method, params);
    });

    registerHandlers();
    std::cout << "[VSCodeProtocol] Initialized (" << m_handlers.size()
              << " extension host RPC handlers registered)\n";
}

void VSCodeProtocol::emitToUi(const std::string& method, const std::shared_ptr<JsonNode>& params) {
    m_ipc->emitEvent(method, params);
}

void VSCodeProtocol::emitThemeList(const std::string& method, bool icons) {
    vrutti::core::plugins::PluginLoader pluginManager;
    auto plugin = pluginManager.loadPlugin(
        vrutti::core::utils::pluginPath("ThemeService.dll").string());
    if (!plugin) return;

    std::string list = plugin->executeCommand(icons ? "listIcons" : "list", "");
    auto parsed = JsonParser::parse(list);
    if (!parsed || parsed->type != JsonNode::Type::Array) return;

    emitToUi(method, parsed);
}

int VSCodeProtocol::sendRequest(int proxyId,
                                const std::string& method,
                                const std::shared_ptr<JsonNode>& args,
                                std::function<void(const std::shared_ptr<JsonNode>&)> onResolve,
                                std::function<void(const std::string&, const std::string&)> onReject) {
    return m_ipc->sendRequest(proxyId, method, args, std::move(onResolve), std::move(onReject));
}

int VSCodeProtocol::sendRequest(const char* shapeName,
                                const std::string& method,
                                const std::shared_ptr<JsonNode>& args,
                                std::function<void(const std::shared_ptr<JsonNode>&)> onResolve,
                                std::function<void(const std::string&, const std::string&)> onReject) {
    int id = proxy::shapeId(shapeName);
    if (id < 0) {
        vrutti::core::utils::reportError("VSCodeProtocol",
            vrutti::core::utils::ErrorCode::IpcUnknownShape,
            std::string("unknown extension host shape: ") + shapeName);
        if (onReject) {
            onReject(std::string("Unknown extension host shape: ") + shapeName,
                     "UnknownShape");
        }
        return -1;
    }
    return sendRequest(id, method, args, std::move(onResolve), std::move(onReject));
}

// -----------------------------------------------------------------------------
// Start-up handshake
//
// The order below mirrors the upstream workbench:
//
//   1. init data                -> the host builds its services
//   2. $initializeConfiguration -> opens the barrier in front of the
//                                  configuration provider; without it every
//                                  extension that reads a setting hangs
//   3. $initializeWorkspace     -> opens the barrier that lets the host start
//                                  the extension host and activate extensions
//   4. $activateByEvent         -> triggers "onStartupFinished" extensions
//
// Skipping step 2 leaves the host waiting forever on
// `_extHostConfiguration.getConfigProvider()`; skipping step 3 leaves it
// waiting forever on `_extHostWorkspace.waitForInitializeCall()`.
// -----------------------------------------------------------------------------

void VSCodeProtocol::onExtensionHostReady() {
    if (m_handshakeDone) return;
    m_handshakeDone = true;

    std::cout << "[VSCodeProtocol] Extension Host ready -- starting handshake\n";
    sendInitData();
    sendConfigurationHandshake();
    sendWorkspaceHandshake();
    sendStartupActivation();
}

void VSCodeProtocol::sendInitData() {
    vrutti::core::plugins::PluginLoader pluginManager;
    auto plugin = pluginManager.loadPlugin(
        vrutti::core::utils::pluginPath("VSCodeExtHostService.dll").string());
    if (!plugin) {
        vrutti::core::utils::reportError("VSCodeProtocol",
            vrutti::core::utils::ErrorCode::PluginLoadFailed,
            "VSCodeExtHostService.dll -- the extension host will not initialise");
        return;
    }

    std::string payload = plugin->executeCommand("getInitData", m_workspaceFolder);
    if (payload.empty() || payload == "{}") {
        vrutti::core::utils::reportError("VSCodeProtocol",
            vrutti::core::utils::ErrorCode::IpcHandshakeFailed,
            "the extension host init data was empty");
        return;
    }

    // The bridge JSON-stringifies `data`, so it must be an object -- not a
    // string containing JSON (that was the original bug).
    auto initData = JsonParser::parse(payload);
    if (!initData || initData->type != JsonNode::Type::Object) {
        vrutti::core::utils::reportError("VSCodeProtocol",
            vrutti::core::utils::ErrorCode::JsonRootNotObject,
            "the extension host init data is not a JSON object");
        return;
    }

    m_ipc->sendInitData(initData);
    std::cout << "[VSCodeProtocol] Sent extension host init data ("
              << payload.size() << " bytes)\n";
}

void VSCodeProtocol::sendConfigurationHandshake() {
    // IConfigurationInitData = IConfigurationData + configurationScopes.
    // Every model is an IConfigurationModel: { contents, keys, overrides }.
    auto data = makeObject();
    setProp(data, "defaults", makeConfigurationModel());
    setProp(data, "policy", makeConfigurationModel());
    setProp(data, "application", makeConfigurationModel());
    setProp(data, "userLocal", makeConfigurationModel());
    setProp(data, "userRemote", makeConfigurationModel());
    setProp(data, "workspace", makeConfigurationModel());
    setProp(data, "folders", makeArray());
    setProp(data, "configurationScopes", makeArray());

    auto args = makeArray();
    args->arrayElements.push_back(data);

    sendRequest(proxy::Ext::ExtHostConfiguration, "$initializeConfiguration", args,
        [](const std::shared_ptr<JsonNode>&) {
            std::cout << "[VSCodeProtocol] Configuration initialised on the extension host\n";
        },
        [](const std::string& err, const std::string&) {
            std::cerr << "[VSCodeProtocol] $initializeConfiguration failed: " << err << "\n";
        });
}

void VSCodeProtocol::sendWorkspaceHandshake() {
    // IWorkspaceData: { id, name, folders: [{ uri, name, index }] }
    auto workspace = makeObject();

    std::string folder = m_workspaceFolder;
    std::string id = "vrutti-workspace";
    std::string name = "Vrutti Workspace";

    if (!folder.empty()) {
        stdfs::path p(folder);
        name = p.filename().string();
        if (name.empty()) name = folder;
        id = sanitizeExtensionId(name);
        if (id.empty()) id = "vrutti-workspace";
    }

    setProp(workspace, "id", makeString(id));
    setProp(workspace, "name", makeString(name));

    auto folders = makeArray();
    if (!folder.empty()) {
        auto entry = makeObject();
        setProp(entry, "uri", makeFileUri(folder));
        setProp(entry, "name", makeString(name));
        setProp(entry, "index", makeNumber(0));
        folders->arrayElements.push_back(entry);
    }
    setProp(workspace, "folders", folders);

    auto args = makeArray();
    args->arrayElements.push_back(workspace);
    args->arrayElements.push_back(makeBool(true)); // trusted

    sendRequest(proxy::Ext::ExtHostWorkspace, "$initializeWorkspace", args,
        [](const std::shared_ptr<JsonNode>&) {
            std::cout << "[VSCodeProtocol] Workspace initialised on the extension host\n";
        },
        [](const std::string& err, const std::string&) {
            std::cerr << "[VSCodeProtocol] $initializeWorkspace failed: " << err << "\n";
        });
}

void VSCodeProtocol::sendStartupActivation() {
    // ActivationKind.Normal == 0, ActivationKind.Immediate == 1
    auto args = makeArray();
    args->arrayElements.push_back(makeString("onStartupFinished"));
    args->arrayElements.push_back(makeNumber(0));

    sendRequest(proxy::Ext::ExtHostExtensionService, "$activateByEvent", args,
        [](const std::shared_ptr<JsonNode>&) {
            std::cout << "[VSCodeProtocol] Startup activation requested\n";
        },
        [](const std::string& err, const std::string&) {
            std::cerr << "[VSCodeProtocol] $activateByEvent failed: " << err << "\n";
        });
}

// -----------------------------------------------------------------------------
// Inbound dispatch
// -----------------------------------------------------------------------------

bool VSCodeProtocol::hasHandler(const std::string& shape, const std::string& method) const {
    const std::string key = shape + "." + method;
    return m_handlers.find(key) != m_handlers.end() ||
           m_asyncHandlers.find(key) != m_asyncHandlers.end();
}

void VSCodeProtocol::setAsyncHandler(const std::string& shape, const std::string& method,
                                     AsyncHandler handler) {
    m_asyncHandlers[shape + "." + method] = std::move(handler);
}

void VSCodeProtocol::handleRequest(const IPCClient::RpcRequest& req) {
    const char* shape = proxy::shapeName(req.proxyId);
    std::string key;
    if (shape) {
        key = std::string(shape) + "." + req.method;
    } else {
        key = req.method;
    }

    // Acknowledge first so the host knows the request was received.
    m_ipc->sendAck(req.reqId);

    // A handler that answers later gets the callbacks it must use. The
    // router keeps its hands off the request until one of them is called.
    auto asyncIt = m_asyncHandlers.find(key);
    if (asyncIt != m_asyncHandlers.end()) {
        const int reqId = req.reqId;
        try {
            asyncIt->second(
                req.args,
                [this, reqId](const std::shared_ptr<JsonNode>& value) {
                    m_ipc->sendReply(reqId, value ? value : makeNull());
                },
                [this, reqId](const std::string& message, const std::string& name) {
                    m_ipc->sendReplyError(reqId, message, name);
                });
        } catch (const std::exception& e) {
            m_ipc->sendReplyError(reqId, e.what(), "Error");
        } catch (...) {
            m_ipc->sendReplyError(reqId,
                "Unknown error in extension host RPC handler", "Error");
        }
        return;
    }

    auto it = m_handlers.find(key);
    if (it == m_handlers.end()) {
        // The feature is not implemented. The extension host turns an error
        // reply into a rejected promise, and most of these calls are not
        // awaited, so the rejection becomes an unhandled rejection and the
        // extension looks broken instead of limited.
        //
        // So the call is answered with the benign default of its return type.
        // The gap is still reported, but only once per call, so the log
        // shows what is missing without drowning in it.
        const bool firstTime = m_reportedGaps.insert(key).second;
        if (firstTime) {
            vrutti::core::utils::reportError("VSCodeProtocol",
                vrutti::core::utils::ErrorCode::IpcHandlerMissing,
                "no handler for " + key + " -- answered with the default");
        }

        const auto* fallback = findFallback(key);
        m_ipc->sendReply(req.reqId,
            fallback ? fallbackValue(fallback->kind) : makeNull());
        return;
    }

    HandlerResult result;
    try {
        result = it->second(req.args);
    } catch (const std::exception& e) {
        result = HandlerResult();
        result.ok = false;
        result.errorMessage = e.what();
    } catch (...) {
        result = HandlerResult();
        result.ok = false;
        result.errorMessage = "Unknown error in extension host RPC handler";
    }

    if (result.ok) {
        m_ipc->sendReply(req.reqId, result.value ? result.value : makeNull());
    } else {
        m_ipc->sendReplyError(req.reqId, result.errorMessage, result.errorName);
    }
}

namespace {
    VSCodeProtocol::HandlerResult okResult(std::shared_ptr<JsonNode> value = nullptr) {
        VSCodeProtocol::HandlerResult r;
        r.ok = true;
        r.value = value ? value : makeNull();
        return r;
    }

    VSCodeProtocol::HandlerResult failResult(const std::string& message) {
        VSCodeProtocol::HandlerResult r;
        r.ok = false;
        r.errorMessage = message;
        return r;
    }
} // namespace

// -----------------------------------------------------------------------------
// Local (webview) requests
// -----------------------------------------------------------------------------

void VSCodeProtocol::handleLocalRequest(const std::string& method,
                                        const std::shared_ptr<JsonNode>& params) {
    if (method == "extensions/activateEvent") {
        std::string event = "onStartupFinished";
        if (params && params->type == JsonNode::Type::Object) {
            auto evNode = params->get("event");
            if (evNode && evNode->type == JsonNode::Type::String) {
                event = std::string(evNode->stringValue.data(), evNode->stringValue.length());
            }
        }
        auto args = makeArray();
        args->arrayElements.push_back(makeString(event));
        args->arrayElements.push_back(makeNumber(0)); // ActivationKind.Normal
        sendRequest(proxy::Ext::ExtHostExtensionService, "$activateByEvent", args,
            [](const std::shared_ptr<JsonNode>&) {},
            [event](const std::string& err, const std::string&) {
                std::cerr << "[VSCodeProtocol] $activateByEvent('" << event << "') failed: " << err << "\n";
            });
        return;
    }

    if (method == "extensions/request_installed") {
        vrutti::core::plugins::PluginLoader pluginManager;
        auto plugin = pluginManager.loadPlugin(
            vrutti::core::utils::pluginPath("ExtensionManager.dll").string());
        if (!plugin) return;
        std::string list = plugin->executeCommand("list", "");
        auto parsed = JsonParser::parse(list);
        if (!parsed) return;
        auto ev = makeObject();
        setProp(ev, "extensions", parsed);
        emitToUi("extensions/installed", ev);

        // The settings UI lists the available themes next to the installed
        // extensions, so they are published together.
        emitThemeList("themes/available", false);
        emitThemeList("icon_themes/available", true);
        return;
    }

    if (method == "themes/available" || method == "icon_themes/available") {
        emitThemeList(method, method == "icon_themes/available");
        return;
    }

    if (method == "theme/set" || method == "icon_theme/set") {
        bool icons = (method == "icon_theme/set");
        std::string wanted;
        if (params && params->type == JsonNode::Type::Object) {
            if (auto nameNode = params->get("name");
                nameNode && nameNode->type == JsonNode::Type::String) {
                wanted = std::string(nameNode->stringValue);
            }
            if (wanted.empty()) {
                if (auto idNode = params->get("id");
                    idNode && idNode->type == JsonNode::Type::String) {
                    wanted = std::string(idNode->stringValue);
                }
            }
        }

        // "default" means: fall back to the built-in colours.
        if (wanted.empty() || wanted == "default") {
            emitToUi(icons ? "icon_theme/load" : "theme/load", makeNull());
            return;
        }

        vrutti::core::plugins::PluginLoader pluginManager;
        auto plugin = pluginManager.loadPlugin("ThemeService.dll");
        if (!plugin) {
            std::cerr << "[VSCodeProtocol] Could not load ThemeService.dll -- "
                         "the theme cannot be applied\n";
            return;
        }

        std::string payload = plugin->executeCommand(icons ? "loadIcon" : "load", wanted);
        auto theme = JsonParser::parse(payload);
        if (!theme || theme->type != JsonNode::Type::Object) {
            std::cerr << "[VSCodeProtocol] Theme '" << wanted << "' could not be loaded\n";
            return;
        }
        emitToUi(icons ? "icon_theme/load" : "theme/load", theme);
        return;
    }

    if (method == "command/execute") {
        std::string command;
        if (params && params->type == JsonNode::Type::Object) {
            auto cmdNode = params->get("command");
            if (cmdNode && cmdNode->type == JsonNode::Type::String) {
                command = std::string(cmdNode->stringValue.data(), cmdNode->stringValue.length());
            }
        }
        if (command.empty()) return;

        // Extension-contributed commands are executed by the extension host.
        auto args = makeArray();
        args->arrayElements.push_back(makeString(command));
        sendRequest(proxy::Ext::ExtHostCommands, "$executeContributedCommand", args,
            [command](const std::shared_ptr<JsonNode>&) {
                std::cout << "[VSCodeProtocol] Command executed: " << command << "\n";
            },
            [command](const std::string& err, const std::string&) {
                std::cerr << "[VSCodeProtocol] Command '" << command << "' failed: " << err << "\n";
            });
        return;
    }

    // Anything else is surfaced to the UI so the frontend can react to it.
    auto ev = makeObject();
    setProp(ev, "params", params ? params : makeNull());
    emitToUi(method, ev);
}

// -----------------------------------------------------------------------------
// Handler table
//
// Key format: "<ShapeName>.<method>" where ShapeName is the VS Code proxy
// identifier (see VsCodeProxyIds.generated.h).
// -----------------------------------------------------------------------------

void VSCodeProtocol::registerHandlers() {
    using HR = HandlerResult;

    // --- MainThreadLanguageModelTools -----------------------------------------
    // The host iterates the returned list, so it must be an array -- returning
    // `null` makes the host throw and the error surfaces as an unexpected
    // error inside the extension host.
    m_handlers["MainThreadLanguageModelTools.$getTools"] = [](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(makeArray());
    };

    // --- MainThreadCommands ----------------------------------------------------
    m_handlers["MainThreadCommands.$registerCommand"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        std::string id = argString(args, 0);
        if (std::find(m_registeredCommands.begin(), m_registeredCommands.end(), id) == m_registeredCommands.end()) {
            m_registeredCommands.push_back(id);
        }
        auto ev = makeObject();
        setProp(ev, "command", makeString(id));
        emitToUi("commands/registered", ev);
        return okResult();
    };

    m_handlers["MainThreadCommands.$unregisterCommand"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        std::string id = argString(args, 0);
        m_registeredCommands.erase(
            std::remove(m_registeredCommands.begin(), m_registeredCommands.end(), id),
            m_registeredCommands.end());
        auto ev = makeObject();
        setProp(ev, "command", makeString(id));
        emitToUi("commands/unregistered", ev);
        return okResult();
    };

    m_handlers["MainThreadCommands.$fireCommandActivationEvent"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto ev = makeObject();
        setProp(ev, "command", makeString(argString(args, 0)));
        return okResult(ev);
    };

    m_handlers["MainThreadCommands.$executeCommand"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        // The renderer owns command execution; surface it to the UI. The result
        // is not round-tripped through this channel yet.
        auto ev = makeObject();
        setProp(ev, "command", makeString(argString(args, 0)));
        setProp(ev, "args", argAt(args, 1) ? argAt(args, 1) : makeArray());
        emitToUi("commands/execute", ev);
        return okResult();
    };

    m_handlers["MainThreadCommands.$getCommands"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        auto arr = makeArray();
        for (const auto& id : m_registeredCommands) {
            arr->arrayElements.push_back(makeString(id));
        }
        return okResult(arr);
    };

    // --- MainThreadExtensionService --------------------------------------------
    m_handlers["MainThreadExtensionService.$getExtension"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(); // undefined -- the host falls back to its own registry
    };

    m_handlers["MainThreadExtensionService.$activateExtension"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadExtensionService.$onWillActivateExtension"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadExtensionService.$onDidActivateExtension"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadExtensionService.$onExtensionRuntimeError"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        std::string extId = argString(args, 0);
        auto errNode = argAt(args, 1);
        std::string message;
        if (errNode && errNode->type == JsonNode::Type::Object) {
            auto m = errNode->get("message");
            if (m && m->type == JsonNode::Type::String) {
                message = std::string(m->stringValue.data(), m->stringValue.length());
            }
        }
        std::cerr << "[ExtensionRuntimeError] " << extId << ": " << message << "\n";
        auto ev = makeObject();
        setProp(ev, "extensionId", makeString(extId));
        setProp(ev, "message", makeString(message));
        emitToUi("extensions/runtime-error", ev);
        return okResult();
    };

    m_handlers["MainThreadExtensionService.$onExtensionActivationError"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        std::string extId = argString(args, 0);
        auto errNode = argAt(args, 1);
        std::string message;
        if (errNode && errNode->type == JsonNode::Type::Object) {
            auto m = errNode->get("message");
            if (m && m->type == JsonNode::Type::String) {
                message = std::string(m->stringValue.data(), m->stringValue.length());
            }
        }
        std::cerr << "[ExtensionActivationError] " << extId << ": " << message << "\n";
        auto ev = makeObject();
        setProp(ev, "extensionId", makeString(extId));
        setProp(ev, "message", makeString(message));
        emitToUi("extensions/activation-error", ev);
        return okResult();
    };

    m_handlers["MainThreadExtensionService.$setPerformanceMarks"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadExtensionService.$asBrowserUri"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        // Vrutti is a desktop app: a file URI is already browser-addressable.
        auto uri = argAt(args, 0);
        return okResult(uri ? uri : makeNull());
    };

    // --- MainThreadErrors ------------------------------------------------------
    m_handlers["MainThreadErrors.$onUnexpectedError"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto errNode = argAt(args, 0);
        std::string message;
        if (errNode && errNode->type == JsonNode::Type::Object) {
            auto m = errNode->get("message");
            if (m && m->type == JsonNode::Type::String) {
                message = std::string(m->stringValue.data(), m->stringValue.length());
            }
        }
        std::cerr << "[ExtensionHostError] " << message << "\n";
        auto ev = makeObject();
        setProp(ev, "message", makeString(message));
        emitToUi("extensions/unexpected-error", ev);
        return okResult();
    };

    // --- MainThreadMessageService ---------------------------------------------
    m_handlers["MainThreadMessageService.$showMessage"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        // severity: 1=Error 2=Warning 3=Info
        auto ev = makeObject();
        setProp(ev, "severity", makeNumber(argNumber(args, 0)));
        setProp(ev, "message", makeString(argString(args, 1)));
        setProp(ev, "commands", argAt(args, 3) ? argAt(args, 3) : makeArray());
        emitToUi("window/showMessage", ev);
        return okResult(); // undefined -- no interactive response channel yet
    };

    // --- MainThreadStatusBar ---------------------------------------------------
    m_handlers["MainThreadStatusBar.$setEntry"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        std::string id = argString(args, 0);
        auto entry = makeObject();
        setProp(entry, "id", makeString(id));
        setProp(entry, "statusId", makeString(argString(args, 1)));
        setProp(entry, "extensionId", makeString(argString(args, 2)));
        setProp(entry, "name", makeString(argString(args, 3)));
        setProp(entry, "text", makeString(argString(args, 4)));
        setProp(entry, "tooltip", argAt(args, 5) ? argAt(args, 5) : makeNull());
        setProp(entry, "command", argAt(args, 7) ? argAt(args, 7) : makeNull());
        setProp(entry, "alignLeft", makeBool(argBool(args, 10)));
        setProp(entry, "priority", argAt(args, 11) ? argAt(args, 11) : makeNull());
        m_statusBarEntries[id] = entry;

        auto ev = makeObject();
        setProp(ev, "entry", entry);
        emitToUi("statusbar/setEntry", ev);
        return okResult();
    };

    m_handlers["MainThreadStatusBar.$disposeEntry"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        std::string id = argString(args, 0);
        m_statusBarEntries.erase(id);
        auto ev = makeObject();
        setProp(ev, "id", makeString(id));
        emitToUi("statusbar/disposeEntry", ev);
        return okResult();
    };

    // --- MainThreadWindow ------------------------------------------------------
    m_handlers["MainThreadWindow.$getInitialState"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        auto state = makeObject();
        setProp(state, "isFocused", makeBool(true));
        setProp(state, "isActive", makeBool(true));
        return okResult(state);
    };

    m_handlers["MainThreadWindow.$openUri"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto ev = makeObject();
        setProp(ev, "uri", argAt(args, 0) ? argAt(args, 0) : makeNull());
        emitToUi("window/openUri", ev);
        return okResult(makeBool(false));
    };

    m_handlers["MainThreadWindow.$asExternalUri"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto uri = argAt(args, 0);
        return okResult(uri ? uri : makeNull());
    };

    // --- MainThreadWorkspace ----------------------------------------------------
    m_handlers["MainThreadWorkspace.$save"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto uri = argAt(args, 0);
        std::string path = uriToPath(uri);
        if (path.empty()) return okResult();
        auto ev = makeObject();
        setProp(ev, "uri", uri ? uri : makeNull());
        setProp(ev, "path", makeString(path));
        emitToUi("workspace/save", ev);
        return okResult(uri ? uri : makeNull());
    };

    m_handlers["MainThreadWorkspace.$saveAll"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        emitToUi("workspace/saveAll", makeObject());
        return okResult(makeBool(true));
    };

    m_handlers["MainThreadWorkspace.$checkExists"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(makeBool(false));
    };

    m_handlers["MainThreadWorkspace.$startFileSearch"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(); // null -> no results
    };

    m_handlers["MainThreadWorkspace.$startTextSearch"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(); // null -> no results
    };

    m_handlers["MainThreadWorkspace.$updateWorkspaceFolders"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$resolveProxy"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$lookupAuthorization"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$lookupKerberosAuthorization"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$loadCertificates"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(makeArray());
    };

    m_handlers["MainThreadWorkspace.$requestResourceTrust"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(makeBool(true));
    };

    m_handlers["MainThreadWorkspace.$requestWorkspaceTrust"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(makeBool(true));
    };

    m_handlers["MainThreadWorkspace.$isResourceTrusted"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(makeBool(true));
    };

    m_handlers["MainThreadWorkspace.$registerEditSessionIdentityProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$unregisterEditSessionIdentityProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$registerCanonicalUriProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$unregisterCanonicalUriProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadWorkspace.$resolveDecoding"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        auto res = makeObject();
        setProp(res, "preferredEncoding", makeString("utf8"));
        setProp(res, "guessEncoding", makeBool(false));
        setProp(res, "candidateGuessEncodings", makeArray());
        return okResult(res);
    };

    m_handlers["MainThreadWorkspace.$validateDetectedEncoding"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        return okResult(makeString(argString(args, 1)));
    };

    m_handlers["MainThreadWorkspace.$resolveEncoding"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        auto res = makeObject();
        setProp(res, "encoding", makeString("utf8"));
        setProp(res, "addBOM", makeBool(false));
        return okResult(res);
    };

    // --- MainThreadConfiguration ------------------------------------------------
    m_handlers["MainThreadConfiguration.$updateConfigurationOption"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto ev = makeObject();
        setProp(ev, "key", makeString(argString(args, 1)));
        setProp(ev, "value", argAt(args, 2) ? argAt(args, 2) : makeNull());
        emitToUi("configuration/update", ev);
        return okResult();
    };

    m_handlers["MainThreadConfiguration.$removeConfigurationOption"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto ev = makeObject();
        setProp(ev, "key", makeString(argString(args, 1)));
        emitToUi("configuration/remove", ev);
        return okResult();
    };

    // --- MainThreadTelemetry ----------------------------------------------------
    m_handlers["MainThreadTelemetry.$publicLog"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTelemetry.$publicLog2"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTelemetry.$setExperimentProperty"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    // --- MainThreadLogger -------------------------------------------------------
    m_handlers["MainThreadLogger.$log"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto messages = argAt(args, 1);
        if (messages && messages->type == JsonNode::Type::Array) {
            for (const auto& entry : messages->arrayElements) {
                if (!entry || entry->type != JsonNode::Type::Array || entry->arrayElements.size() < 2) continue;
                auto level = entry->arrayElements[0];
                auto text = entry->arrayElements[1];
                if (text && text->type == JsonNode::Type::String) {
                    std::cout << "[ExtHostLog] "
                              << std::string(text->stringValue.data(), text->stringValue.length())
                              << "\n";
                }
            }
        }
        return okResult();
    };

    m_handlers["MainThreadLogger.$flush"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadLogger.$createLogger"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadLogger.$registerLogger"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadLogger.$deregisterLogger"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadLogger.$setVisibility"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    // --- MainThreadTerminalService ---------------------------------------------
    m_handlers["MainThreadTerminalService.$createTerminal"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto ev = makeObject();
        setProp(ev, "extHostTerminalId", makeString(argString(args, 0)));
        setProp(ev, "config", argAt(args, 1) ? argAt(args, 1) : makeNull());
        emitToUi("terminal/create", ev);
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$dispose"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto ev = makeObject();
        setProp(ev, "id", argAt(args, 0) ? argAt(args, 0) : makeNull());
        emitToUi("terminal/dispose", ev);
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$hide"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$show"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$sendText"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto ev = makeObject();
        setProp(ev, "id", argAt(args, 0) ? argAt(args, 0) : makeNull());
        setProp(ev, "text", makeString(argString(args, 1)));
        setProp(ev, "shouldExecute", makeBool(argBool(args, 2)));
        emitToUi("terminal/sendText", ev);
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$registerProcessSupport"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$registerProfileProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$unregisterProfileProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$registerCompletionProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$unregisterCompletionProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$registerQuickFixProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$unregisterQuickFixProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$setEnvironmentVariableCollection"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$startSendingDataEvents"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$stopSendingDataEvents"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$startSendingCommandEvents"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$stopSendingCommandEvents"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$startLinkProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    m_handlers["MainThreadTerminalService.$stopLinkProvider"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    // --- MainThreadClipboard ----------------------------------------------------
    m_handlers["MainThreadClipboard.$readText"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(makeString(""));
    };

    m_handlers["MainThreadClipboard.$writeText"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult();
    };

    // --- MainThreadDocuments ----------------------------------------------------
    m_handlers["MainThreadDocuments.$tryCreateDocument"] = [this](const std::shared_ptr<JsonNode>&) -> HR {
        return okResult(); // undefined -- untitled documents are not modelled yet
    };

    m_handlers["MainThreadDocuments.$tryOpenDocument"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto uri = argAt(args, 0);
        return okResult(uri ? uri : makeNull());
    };

    m_handlers["MainThreadDocuments.$trySaveDocument"] = [this](const std::shared_ptr<JsonNode>& args) -> HR {
        auto uri = argAt(args, 0);
        std::string path = uriToPath(uri);
        if (path.empty()) return okResult(makeBool(false));
        auto ev = makeObject();
        setProp(ev, "uri", uri ? uri : makeNull());
        setProp(ev, "path", makeString(path));
        emitToUi("documents/save", ev);
        return okResult(makeBool(true));
    };

    // --- MainThreadFileSystem ---------------------------------------------------
    //
    // This is the `vscode.workspace.fs` surface: the way an extension reads
    // and writes a file. The extension host calls these actors when an
    // extension uses `workspace.fs`.
    //
    // The `file:` scheme is served by the core. A scheme that an extension
    // registered is served by that extension: the call is forwarded to the
    // extension host, which knows the provider.
    //
    // The calls are answered later, because the answer of a provider comes
    // from the extension host itself.

    m_handlers["MainThreadFileSystem.$registerFileSystemProvider"] =
        [this](const std::shared_ptr<JsonNode>& args) -> HR {
            const int handle = static_cast<int>(argNumber(args, 0));
            const std::string scheme = argString(args, 1);
            if (!scheme.empty()) {
                m_fsProviders[scheme] = handle;
                m_fsProviderSchemes[handle] = scheme;
            }
            return okResult();
        };

    m_handlers["MainThreadFileSystem.$unregisterProvider"] =
        [this](const std::shared_ptr<JsonNode>& args) -> HR {
            const int handle = static_cast<int>(argNumber(args, 0));
            auto it = m_fsProviderSchemes.find(handle);
            if (it != m_fsProviderSchemes.end()) {
                m_fsProviders.erase(it->second);
                m_fsProviderSchemes.erase(it);
            }
            return okResult();
        };

    m_handlers["MainThreadFileSystem.$onFileSystemChange"] =
        [this](const std::shared_ptr<JsonNode>& args) -> HR {
            // The changes are published to the UI, so an open editor can pick
            // up a file that an extension changed.
            auto changes = argAt(args, 1);
            emitToUi("files/changed", changes ? changes : makeArray());
            return okResult();
        };

    m_handlers["MainThreadFileSystem.$ensureActivation"] =
        [this](const std::shared_ptr<JsonNode>&) -> HR {
            // The extension that owns the provider is already running: it is
            // the one that called this method.
            return okResult();
        };

    // $stat(uri) -> IStat
    setAsyncHandler("MainThreadFileSystem", "$stat",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto uri = argAt(args, 0);
            const int provider = providerForUri(uri);
            if (provider >= 0) {
                forwardToProvider(provider, "$stat", { uri }, reply, fail);
                return;
            }

            const std::string path = vrutti::core::fs::uriToNativePath(uri);
            if (path.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            vrutti::core::fs::FileStat stat;
            const auto result = m_fileService.stat(path, stat);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }

            auto answer = makeObject();
            setProp(answer, "type", makeNumber(stat.type));
            setProp(answer, "ctime", makeNumber(static_cast<double>(stat.ctime)));
            setProp(answer, "mtime", makeNumber(static_cast<double>(stat.mtime)));
            setProp(answer, "size", makeNumber(static_cast<double>(stat.size)));
            if (stat.permissions != 0) {
                setProp(answer, "permissions", makeNumber(stat.permissions));
            }
            reply(answer);
        });

    // $readdir(uri) -> [name, FileType][]
    setAsyncHandler("MainThreadFileSystem", "$readdir",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto uri = argAt(args, 0);
            const int provider = providerForUri(uri);
            if (provider >= 0) {
                forwardToProvider(provider, "$readdir", { uri }, reply, fail);
                return;
            }

            const std::string path = vrutti::core::fs::uriToNativePath(uri);
            if (path.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            std::vector<vrutti::core::fs::DirectoryEntry> entries;
            const auto result = m_fileService.readDirectory(path, entries);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }

            auto answer = makeArray();
            for (const auto& entry : entries) {
                auto pair = makeArray();
                pair->arrayElements.push_back(makeString(entry.name));
                pair->arrayElements.push_back(makeNumber(entry.type));
                answer->arrayElements.push_back(pair);
            }
            reply(answer);
        });

    // $readFile(uri) -> VSBuffer
    setAsyncHandler("MainThreadFileSystem", "$readFile",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto uri = argAt(args, 0);
            const int provider = providerForUri(uri);
            if (provider >= 0) {
                forwardToProvider(provider, "$readFile", { uri }, reply, fail);
                return;
            }

            const std::string path = vrutti::core::fs::uriToNativePath(uri);
            if (path.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            std::vector<uint8_t> content;
            const auto result = m_fileService.readFile(path, content);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }
            reply(IPCClient::encodeVSBuffer(content));
        });

    // $writeFile(uri, content) -> void
    setAsyncHandler("MainThreadFileSystem", "$writeFile",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto uri = argAt(args, 0);
            auto content = argAt(args, 1);
            const int provider = providerForUri(uri);
            if (provider >= 0) {
                // The provider of an extension takes the options of the
                // write as well.
                auto options = makeObject();
                setProp(options, "create", makeBool(true));
                setProp(options, "overwrite", makeBool(true));
                forwardToProvider(provider, "$writeFile",
                                  { uri, content, options }, reply, fail);
                return;
            }

            const std::string path = vrutti::core::fs::uriToNativePath(uri);
            if (path.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            std::vector<uint8_t> bytes;
            if (!IPCClient::decodeVSBuffer(content, bytes)) {
                fail("the content of the file is not a binary payload", "Unavailable");
                return;
            }

            const auto result = m_fileService.writeFile(path, bytes);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }
            reply(makeNull());
        });

    // $mkdir(uri) -> void
    setAsyncHandler("MainThreadFileSystem", "$mkdir",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto uri = argAt(args, 0);
            const int provider = providerForUri(uri);
            if (provider >= 0) {
                forwardToProvider(provider, "$mkdir", { uri }, reply, fail);
                return;
            }

            const std::string path = vrutti::core::fs::uriToNativePath(uri);
            if (path.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            const auto result = m_fileService.createDirectory(path);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }
            reply(makeNull());
        });

    // $delete(uri, options) -> void
    setAsyncHandler("MainThreadFileSystem", "$delete",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto uri = argAt(args, 0);
            auto options = argAt(args, 1);
            const int provider = providerForUri(uri);
            if (provider >= 0) {
                forwardToProvider(provider, "$delete", { uri, options }, reply, fail);
                return;
            }

            const std::string path = vrutti::core::fs::uriToNativePath(uri);
            if (path.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            const bool recursive = optionBool(options, "recursive");
            const auto result = m_fileService.remove(path, recursive);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }
            reply(makeNull());
        });

    // $rename(source, target, options) -> void
    setAsyncHandler("MainThreadFileSystem", "$rename",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto source = argAt(args, 0);
            auto target = argAt(args, 1);
            auto options = argAt(args, 2);
            const int provider = providerForUri(source);
            if (provider >= 0) {
                forwardToProvider(provider, "$rename",
                                  { source, target, options }, reply, fail);
                return;
            }

            const std::string from = vrutti::core::fs::uriToNativePath(source);
            const std::string to = vrutti::core::fs::uriToNativePath(target);
            if (from.empty() || to.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            const bool overwrite = optionBool(options, "overwrite");
            const auto result = m_fileService.rename(from, to, overwrite);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }
            reply(makeNull());
        });

    // $copy(source, target, options) -> void
    setAsyncHandler("MainThreadFileSystem", "$copy",
        [this](const std::shared_ptr<JsonNode>& args,
               std::function<void(const std::shared_ptr<JsonNode>&)> reply,
               std::function<void(const std::string&, const std::string&)> fail) {
            auto source = argAt(args, 0);
            auto target = argAt(args, 1);
            auto options = argAt(args, 2);
            const int provider = providerForUri(source);
            if (provider >= 0) {
                forwardToProvider(provider, "$copy",
                                  { source, target, options }, reply, fail);
                return;
            }

            const std::string from = vrutti::core::fs::uriToNativePath(source);
            const std::string to = vrutti::core::fs::uriToNativePath(target);
            if (from.empty() || to.empty()) {
                fail("no file system can serve this URI", "Unavailable");
                return;
            }

            const bool overwrite = optionBool(options, "overwrite");
            const auto result = m_fileService.copy(from, to, overwrite);
            if (!result.ok) {
                fail(result.errorMessage, result.errorCode);
                return;
            }
            reply(makeNull());
        });
}

// -----------------------------------------------------------------------------
// The file system
// -----------------------------------------------------------------------------

int VSCodeProtocol::providerForUri(const std::shared_ptr<JsonNode>& uri) const {
    const std::string scheme = uriScheme(uri);
    if (scheme.empty() || scheme == "file") return -1;
    auto it = m_fsProviders.find(scheme);
    if (it == m_fsProviders.end()) return -1;
    return it->second;
}

void VSCodeProtocol::forwardToProvider(
        int handle,
        const char* method,
        const std::vector<std::shared_ptr<JsonNode>>& args,
        const std::function<void(const std::shared_ptr<JsonNode>&)>& reply,
        const std::function<void(const std::string&, const std::string&)>& fail) {
    // The call on the `ExtHostFileSystem` actor takes the handle of the
    // provider as its first argument.
    auto callArgs = makeArray();
    callArgs->arrayElements.push_back(makeNumber(handle));
    for (const auto& arg : args) {
        callArgs->arrayElements.push_back(arg ? arg : makeNull());
    }

    sendRequest(proxy::Ext::ExtHostFileSystem, method, callArgs,
        [reply](const std::shared_ptr<JsonNode>& value) {
            reply(value ? value : makeNull());
        },
        [fail](const std::string& message, const std::string& name) {
            fail(message, name.empty() ? std::string("Unavailable") : name);
        });
}

} // namespace vrutti::core::ipc
