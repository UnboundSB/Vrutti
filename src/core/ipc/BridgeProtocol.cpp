#include "BridgeProtocol.h"

#include <algorithm>
#include <array>

namespace vrutti::core::ipc {

namespace {

    // The methods that the page may call.
    //
    // Every method that the page sends is on this list: the ones that the
    // core answers itself (the extensions, the themes, the commands) and
    // the ones that the core hands over to the user interface (the debug
    // adapter, the tree views, the webview panels, the running of a file).
    //
    // A method that is not on the list is dropped. The page is not a
    // trusted caller, and a name that is not on the list has no meaning.
    constexpr std::array<const char*, 20> kAllowedMethods = {{
        // The extensions.
        "extensions/activateEvent",
        "extensions/request_installed",
        "extensions/install",
        "extensions/uninstall",
        "extensions/enable",
        "extensions/disable",
        // The themes.
        "themes/available",
        "icon_themes/available",
        "theme/set",
        "icon_theme/set",
        // The commands.
        "command/execute",
        // The debugger.
        "debug/evaluate",
        "debuggers/available",
        "dap/start",
        "dap/stop",
        "dap/request",
        // The editor, the tree views and the webview panels.
        "editor/run",
        "treeview/request",
        "webviewView/resolve",
        "webview/receiveMessage",
    }};

    // Tells whether a field of the parameters is a text that is not too long.
    //
    // The field may be missing, or it may be empty (null): the core then
    // uses the default that it has. A field that is there has to be a text,
    // and a text that is longer than its limit is refused: a path of a
    // megabyte is not a path.
    bool saneTextField(const std::shared_ptr<JsonNode>& params,
                       const char* name) {
        if (!params) return true;
        auto field = params->get(name);
        if (!field) return true;
        if (field->type == JsonNode::Type::Null) return true;
        if (field->type != JsonNode::Type::String) return false;
        return field->stringValue.size() <= kMaxBridgeTextLength;
    }

    // Tells whether a field of the parameters is an object.
    bool saneObjectField(const std::shared_ptr<JsonNode>& params,
                         const char* name) {
        if (!params) return true;
        auto field = params->get(name);
        if (!field) return true;
        return field->type == JsonNode::Type::Object;
    }

} // namespace

bool isSaneMethodName(const std::string& method) {
    if (method.empty() || method.size() > kMaxBridgeMethodLength) return false;
    for (unsigned char c : method) {
        if (c < 0x20 || c == 0x7F) return false;
    }
    return true;
}

bool isAllowedBridgeMethod(const std::string& method) {
    if (!isSaneMethodName(method)) return false;
    for (const char* allowed : kAllowedMethods) {
        if (method == allowed) return true;
    }
    return false;
}

std::shared_ptr<JsonNode> parseBridgeMessage(const std::string& raw) {
    // A message that is too big is not read at all: the page cannot make
    // the core do an amount of work that it chooses itself.
    if (raw.size() > kMaxBridgeMessageBytes) {
        return nullptr;
    }

    auto node = vrutti::core::utils::JsonParser::parse(raw);
    if (!node || node->type != JsonNode::Type::Array) return nullptr;
    if (node->arrayElements.size() < 2) return nullptr;

    // The first thing is the name of the method and the second is the
    // payload of the method as a text. A message of another shape is not a
    // message of the bridge.
    const auto& method = node->arrayElements[0];
    const auto& payload = node->arrayElements[1];
    if (!method || method->type != JsonNode::Type::String) return nullptr;
    if (!payload || payload->type != JsonNode::Type::String) return nullptr;

    return node;
}

std::shared_ptr<JsonNode> parseBridgePayload(const std::string& text) {
    if (text.size() > kMaxBridgeMessageBytes) return nullptr;

    auto node = vrutti::core::utils::JsonParser::parse(text);
    if (!node || node->type != JsonNode::Type::Object) return nullptr;
    return node;
}

bool checkBridgeParams(const std::string& method,
                       const std::shared_ptr<JsonNode>& params) {
    // The parameters of every method of the page are an object. A message
    // that holds something else does not have the shape that the core
    // expects, and the core does not guess what the page meant.
    if (params && params->type != JsonNode::Type::Object) {
        return false;
    }

    if (method == "extensions/activateEvent") {
        return saneTextField(params, "event");
    }

    if (method == "extensions/install") {
        return saneTextField(params, "path") && saneTextField(params, "url");
    }

    if (method == "extensions/uninstall" || method == "extensions/enable" ||
        method == "extensions/disable") {
        return saneTextField(params, "id") && saneTextField(params, "identifier");
    }

    if (method == "theme/set" || method == "icon_theme/set") {
        return saneTextField(params, "name") && saneTextField(params, "id");
    }

    if (method == "command/execute") {
        return saneTextField(params, "command");
    }

    if (method == "debug/evaluate") {
        return saneTextField(params, "expression");
    }

    if (method == "dap/start") {
        return saneTextField(params, "type") && saneTextField(params, "cwd");
    }

    if (method == "dap/request") {
        return saneTextField(params, "command") && saneObjectField(params, "args");
    }

    if (method == "editor/run") {
        return saneTextField(params, "file") && saneTextField(params, "mode");
    }

    if (method == "webviewView/resolve") {
        return saneTextField(params, "viewId") && saneTextField(params, "webviewId");
    }

    if (method == "webview/receiveMessage") {
        return saneTextField(params, "id");
    }

    if (method == "treeview/request") {
        // The number of the request is a text or a number: the page makes
        // it with `Date.now()`, and a number may lose its last digits on
        // the way through JSON.
        if (params) {
            auto requestId = params->get("reqId");
            if (requestId && requestId->type != JsonNode::Type::String &&
                requestId->type != JsonNode::Type::Number) {
                return false;
            }
        }
        return saneTextField(params, "viewId") &&
               saneTextField(params, "method") &&
               saneTextField(params, "nodeHandle");
    }

    // The methods that take nothing at all.
    return true;
}

} // namespace vrutti::core::ipc
