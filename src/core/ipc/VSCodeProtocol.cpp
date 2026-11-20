#include "VSCodeProtocol.h"
#include "../utils/ErrorCodes.h"
#include "../utils/Paths.h"
#include "VsCodeProxyIds.generated.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <sstream>
#include "core/utils/Json.h"
#include "core/plugins/PluginLoader.h"

using namespace vrutti::core::utils;
namespace fs = std::filesystem;

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
                                std::function<void(const std::string&)> onReject) {
    return m_ipc->sendRequest(proxyId, method, args, std::move(onResolve), std::move(onReject));
}

int VSCodeProtocol::sendRequest(const char* shapeName,
                                const std::string& method,
                                const std::shared_ptr<JsonNode>& args,
                                std::function<void(const std::shared_ptr<JsonNode>&)> onResolve,
                                std::function<void(const std::string&)> onReject) {
    int id = proxy::shapeId(shapeName);
    if (id < 0) {
        vrutti::core::utils::reportError("VSCodeProtocol",
            vrutti::core::utils::ErrorCode::IpcUnknownShape,
            std::string("unknown extension host shape: ") + shapeName);
        if (onReject) onReject(std::string("Unknown extension host shape: ") + shapeName);
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
        [](const std::string& err) {
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
        fs::path p(folder);
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
        [](const std::string& err) {
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
        [](const std::string& err) {
            std::cerr << "[VSCodeProtocol] $activateByEvent failed: " << err << "\n";
        });
}

// -----------------------------------------------------------------------------
// Inbound dispatch
// -----------------------------------------------------------------------------

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

    auto it = m_handlers.find(key);
    if (it == m_handlers.end()) {
        // Answer anyway. An unanswered request wedges the calling extension.
        vrutti::core::utils::reportError("VSCodeProtocol",
            vrutti::core::utils::ErrorCode::IpcHandlerMissing,
            "no handler for " + key);
        m_ipc->sendReplyError(req.reqId,
            "Vrutti does not implement " + key +
            (shape ? std::string(" (shape '") + shape + "')" : std::string()));
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
            [event](const std::string& err) {
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
            [command](const std::string& err) {
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
}

} // namespace vrutti::core::ipc
