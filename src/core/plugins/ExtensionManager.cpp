#include "ExtensionManager.h"
#include "../utils/Json.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

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

    // Builds a `file:` UriComponents object (VS Code's URI DTO shape).
    std::shared_ptr<JsonNode> makeFileUri(const std::string& nativePath) {
        std::string p = toPosixPath(nativePath);
        if (p.size() >= 2 && std::isalpha(static_cast<unsigned char>(p[0])) && p[1] == ':') {
            p[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(p[0])));
        }
        if (p.empty() || p[0] != '/') p = "/" + p;

        auto uri = std::make_shared<JsonNode>(JsonNode::Type::Object);
        setProp(uri, "$mid", numNode(1)); // MarshalledId.Uri
        setProp(uri, "scheme", strNode("file"));
        setProp(uri, "path", strNode(p));
        return uri;
    }

    // Deterministic UUID (v3-style, MD5 of the extension id) so that extension
    // identity is stable across restarts, like upstream VS Code.
    std::string stableUuid(const std::string& seed) {
        // FNV-1a 128-bit-ish expansion -- good enough for a stable identifier.
        uint64_t h1 = 1469598103934665603ULL;
        uint64_t h2 = 1099511628211ULL;
        for (unsigned char c : seed) {
            h1 ^= c; h1 *= 1099511628211ULL;
            h2 ^= c; h2 *= 1469598103934665603ULL;
        }
        std::ostringstream out;
        out << std::hex << std::setfill('0');
        out << std::setw(8) << (h1 & 0xFFFFFFFFULL);
        out << std::setw(4) << ((h1 >> 32) & 0xFFFFULL);
        out << std::setw(4) << ((h2 >> 16) & 0xFFFFULL);
        out << std::setw(4) << (h2 & 0xFFFFULL);
        out << std::setw(12) << ((h1 ^ h2) & 0xFFFFFFFFFFFFULL);
        std::string hex = out.str();
        return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" +
               hex.substr(16, 4) + "-" + hex.substr(20, 12);
    }

} // namespace

bool ExtensionManager::initialize() { return true; }
void ExtensionManager::shutdown() {}
const char* ExtensionManager::getName() const { return "ExtensionManager"; }

std::string ExtensionManager::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "list") {
        auto extensionsArr = std::make_shared<JsonNode>(JsonNode::Type::Array);

        std::vector<std::string> extDirs;

        // Built-in extensions, resolved relative to the running executable.
        fs::path builtinPath = fs::current_path() / "extensions";
        if (fs::exists(builtinPath)) {
            extDirs.push_back(builtinPath.string());
        }

        // User extensions in ~/.vrutti/extensions
#ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
#else
        const char* home = std::getenv("HOME");
#endif
        if (home) {
            fs::path userExtPath = fs::path(home) / ".vrutti" / "extensions";
            if (fs::exists(userExtPath)) {
                extDirs.push_back(userExtPath.string());
            }
        }

        for (const auto& baseDir : extDirs) {
            if (!fs::exists(baseDir) || !fs::is_directory(baseDir)) continue;
            for (const auto& entry : fs::directory_iterator(baseDir)) {
                if (!entry.is_directory()) continue;
                fs::path extPath = entry.path();

                // An extension folder may hold the manifest at its root or in
                // an `extension/` sub-folder (VS Code marketplace layout).
                fs::path pkgPath = extPath / "extension" / "package.json";
                fs::path extRoot = extPath / "extension";
                if (!fs::exists(pkgPath)) {
                    pkgPath = extPath / "package.json";
                    extRoot = extPath;
                }
                if (!fs::exists(pkgPath)) continue;

                std::ifstream pkgFile(pkgPath);
                if (!pkgFile.is_open()) continue;

                std::stringstream buffer;
                buffer << pkgFile.rdbuf();
                auto pkgNode = JsonParser::parse(buffer.str());
                if (!pkgNode || pkgNode->type != JsonNode::Type::Object) continue;

                auto nameNode = pkgNode->get("name");
                auto publisherNode = pkgNode->get("publisher");
                std::string name = (nameNode && nameNode->type == JsonNode::Type::String)
                    ? std::string(nameNode->stringValue.data(), nameNode->stringValue.length())
                    : extPath.filename().string();
                std::string publisher = (publisherNode && publisherNode->type == JsonNode::Type::String)
                    ? std::string(publisherNode->stringValue.data(), publisherNode->stringValue.length())
                    : "unknown";
                std::string id = publisher + "." + name;

                // --- IExtensionDescription ---
                auto desc = std::make_shared<JsonNode>(JsonNode::Type::Object);

                // identifier: ExtensionIdentifier { value, _lower }
                std::string lower = id;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                auto identifier = std::make_shared<JsonNode>(JsonNode::Type::Object);
                setProp(identifier, "value", strNode(id));
                setProp(identifier, "_lower", strNode(lower));
                setProp(identifier, "id", strNode(id));
                setProp(desc, "identifier", identifier);
                setProp(desc, "uuid", strNode(stableUuid(id)));

                setProp(desc, "name", strNode(name));
                setProp(desc, "publisher", strNode(publisher));
                setProp(desc, "publisherDisplayName", strNode(publisher));

                auto displayNameNode = pkgNode->get("displayName");
                if (displayNameNode && displayNameNode->type == JsonNode::Type::String) {
                    setProp(desc, "displayName", displayNameNode);
                }

                auto versionNode = pkgNode->get("version");
                setProp(desc, "version", (versionNode && versionNode->type == JsonNode::Type::String)
                    ? versionNode : strNode("0.0.0"));

                // engines: { vscode: "^x.y.z" }
                auto engines = std::make_shared<JsonNode>(JsonNode::Type::Object);
                auto enginesNode = pkgNode->get("engines");
                if (enginesNode && enginesNode->type == JsonNode::Type::Object) {
                    auto vscodeNode = enginesNode->get("vscode");
                    if (vscodeNode) setProp(engines, "vscode", vscodeNode);
                }
                if (!engines->objectProperties.empty()) {
                    setProp(desc, "engines", engines);
                } else {
                    setProp(desc, "engines", strNode("^1.90.0"));
                }

                // Entry points are relative to extensionLocation.
                auto mainNode = pkgNode->get("main");
                if (mainNode && mainNode->type == JsonNode::Type::String) {
                    setProp(desc, "main", mainNode);
                }
                auto browserNode = pkgNode->get("browser");
                if (browserNode && browserNode->type == JsonNode::Type::String) {
                    setProp(desc, "browser", browserNode);
                }

                auto activationNode = pkgNode->get("activationEvents");
                if (activationNode && activationNode->type == JsonNode::Type::Array) {
                    setProp(desc, "activationEvents", activationNode);
                }

                auto contributesNode = pkgNode->get("contributes");
                if (contributesNode && contributesNode->type == JsonNode::Type::Object) {
                    setProp(desc, "contributes", contributesNode);
                }

                auto extensionKindNode = pkgNode->get("extensionKind");
                if (extensionKindNode) setProp(desc, "extensionKind", extensionKindNode);

                auto capabilitiesNode = pkgNode->get("capabilities");
                if (capabilitiesNode) setProp(desc, "capabilities", capabilitiesNode);

                setProp(desc, "extensionLocation", makeFileUri(extRoot.string()));
                setProp(desc, "targetPlatform", strNode("undefined"));
                setProp(desc, "isBuiltin", boolNode(false));
                setProp(desc, "isUserBuiltin", boolNode(false));
                setProp(desc, "isUnderDevelopment", boolNode(false));
                setProp(desc, "preRelease", boolNode(false));

                extensionsArr->arrayElements.push_back(desc);
            }
        }

        return JsonSerializer::stringify(extensionsArr, 0, false);
    }
    return "[]";
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
