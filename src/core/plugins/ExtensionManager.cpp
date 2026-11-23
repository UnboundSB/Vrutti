// -----------------------------------------------------------------------------
// ExtensionManager -- the plugin that answers what is installed and what can
// be installed.
//
// The work is done by the service of the core
// (`src/core/extensions/ExtensionService.cpp`). The plugin is the *interface*
// of that service: the parts of the app that do not live in the core (the
// window, the tools) talk to it through the plugin ABI:
//
//     executeCommand("list", "")                  -> the extensions that are installed
//     executeCommand("listForUserInterface", "")  -> the same list, in the shape of the interface
//     executeCommand("install",  "{\"path\":\"...\"}")   -> installs a `.vsix`
//     executeCommand("install",  "{\"url\":\"...\"}")    -> fetches a `.vsix` and installs it
//     executeCommand("uninstall", "{\"id\":\"...\"}")    -> removes an extension
//     executeCommand("enable",   "{\"id\":\"...\"}")    -> turns an extension on
//     executeCommand("disable",  "{\"id\":\"...\"}")    -> turns an extension off
//
// Every answer is a JSON object that says whether the work was done:
//
//     {"ok": true,  "identifier": "publisher.name", "version": "1.0.0"}
//     {"ok": false, "error": "what is wrong", "code": "InvalidExtension"}
//
// The service is the only place that knows what is installed, so the list the
// user interface shows and the list the extension host gets can never drift
// apart.
// -----------------------------------------------------------------------------

#include "ExtensionManager.h"
#include "../extensions/ExtensionService.h"
#include "../utils/HttpDownload.h"
#include "../utils/Json.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;
using vrutti::core::extensions::ExtensionDescription;
using vrutti::core::extensions::ExtensionService;

namespace {

    std::shared_ptr<JsonNode> strNode(std::string value) {
        return vrutti::core::utils::makeJsonString(std::move(value));
    }

    std::shared_ptr<JsonNode> boolNode(bool value) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        node->boolValue = value;
        return node;
    }

    std::shared_ptr<JsonNode> makeObject() {
        return std::make_shared<JsonNode>(JsonNode::Type::Object);
    }

    void setProp(const std::shared_ptr<JsonNode>& obj, const std::string& key,
                 const std::shared_ptr<JsonNode>& value) {
        if (!obj || obj->type != JsonNode::Type::Object) return;
        obj->objectProperties.emplace_back(key, value);
    }

    std::string stringOf(const std::shared_ptr<JsonNode>& node) {
        if (!node || node->type != JsonNode::Type::String) return "";
        return std::string(node->stringValue.data(), node->stringValue.length());
    }

    // The answer of a command that worked.
    std::string okAnswer() {
        auto answer = makeObject();
        setProp(answer, "ok", boolNode(true));
        return JsonSerializer::stringify(answer, 0, false);
    }

    // The answer of a command that did not work.
    std::string errorAnswer(const std::string& error, const std::string& code) {
        auto answer = makeObject();
        setProp(answer, "ok", boolNode(false));
        setProp(answer, "error", strNode(error));
        setProp(answer, "code", strNode(code));
        return JsonSerializer::stringify(answer, 0, false);
    }

} // namespace

bool ExtensionManager::initialize() { return true; }
void ExtensionManager::shutdown() {}

const char* ExtensionManager::getName() const { return "ExtensionManager"; }

std::string ExtensionManager::executeCommand(const std::string& command,
                                             const std::string& payload) {
    if (command == "list") {
        // The list in the shape of the extension host: the descriptions of
        // the extensions that are installed and turned on.
        return JsonSerializer::stringify(
            ExtensionService::descriptionsJson(ExtensionService::listEnabled()), 0, false);
    }

    if (command == "listForUserInterface") {
        // The list in the shape of the user interface. It holds the
        // extensions that are turned off as well, so that the interface can
        // show them.
        return JsonSerializer::stringify(
            ExtensionService::userInterfaceJson(ExtensionService::listAll()), 0, false);
    }

    if (command == "install") {
        auto request = JsonParser::parse(payload);
        std::string path;
        std::string address;
        if (request && request->type == JsonNode::Type::Object) {
            path = stringOf(request->get("path"));
            address = stringOf(request->get("url"));
        }
        if (path.empty() && address.empty()) {
            return errorAnswer("the extension has no file", "InvalidExtension");
        }

        std::string file = path;
        std::string downloaded;
        if (file.empty()) {
            std::string error;
            if (!HttpDownload::download(address, "", downloaded,
                                        256ull * 1024 * 1024, error)) {
                return errorAnswer("the extension could not be fetched: " + error, "Unavailable");
            }
            file = downloaded;
        }

        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        const bool ok = ExtensionService::installVsix(file, installed, error, errorCode);

        if (!downloaded.empty()) {
            std::error_code ec;
            fs::remove(downloaded, ec);
        }

        if (!ok) return errorAnswer(error, errorCode);

        auto answer = makeObject();
        setProp(answer, "ok", boolNode(true));
        setProp(answer, "identifier", strNode(installed.identifier));
        setProp(answer, "version", strNode(installed.version));
        return JsonSerializer::stringify(answer, 0, false);
    }

    if (command == "uninstall" || command == "enable" || command == "disable") {
        auto request = JsonParser::parse(payload);
        std::string identifier = stringOf(request ? request->get("id") : nullptr);
        if (identifier.empty()) {
            identifier = stringOf(request ? request->get("identifier") : nullptr);
        }
        if (identifier.empty()) {
            return errorAnswer("the extension has no name", "InvalidExtension");
        }

        std::string error;
        std::string errorCode;
        bool ok = false;
        if (command == "uninstall") {
            ok = ExtensionService::uninstall(identifier, error, errorCode);
        } else {
            ok = ExtensionService::setEnabled(identifier, command == "enable", error, errorCode);
        }
        if (!ok) return errorAnswer(error, errorCode);
        return okAnswer();
    }

    return errorAnswer("unknown command: " + command, "UnknownCommand");
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new ExtensionManager();
    }

    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
