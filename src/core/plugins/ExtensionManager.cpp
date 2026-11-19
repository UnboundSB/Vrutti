#include "ExtensionManager.h"
#include "../utils/Json.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;
namespace fs = std::filesystem;

bool ExtensionManager::initialize() { return true; }
void ExtensionManager::shutdown() {}
const char* ExtensionManager::getName() const { return "ExtensionManager"; }

std::string ExtensionManager::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "list") {
        auto extensionsArr = std::make_shared<JsonNode>(JsonNode::Type::Array);
        
        std::vector<std::string> extDirs;
        
        // Built-in extensions dynamically resolved from current executable path
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
                fs::path pkgPath = extPath / "extension" / "package.json";
                if (!fs::exists(pkgPath)) {
                    pkgPath = extPath / "package.json";
                }
                
                if (fs::exists(pkgPath)) {
                    std::ifstream pkgFile(pkgPath);
                    if (pkgFile.is_open()) {
                        std::stringstream buffer;
                        buffer << pkgFile.rdbuf();
                        auto pkgNode = JsonParser::parse(buffer.str());
                        if (pkgNode && pkgNode->type == JsonNode::Type::Object) {
                            auto extNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
                            
                            auto nameNode = pkgNode->get("name");
                            auto publisherNode = pkgNode->get("publisher");
                            std::string name = nameNode && nameNode->type == JsonNode::Type::String ? std::string(nameNode->stringValue) : extPath.filename().string();
                            std::string publisher = publisherNode && publisherNode->type == JsonNode::Type::String ? std::string(publisherNode->stringValue) : "unknown";
                            std::string id = publisher + "." + name;
                            
                            extNode->objectProperties.emplace_back("id", std::make_shared<JsonNode>(JsonNode::Type::String));
                            extNode->objectProperties.back().second->stringValue = id;
                            
                            extNode->objectProperties.emplace_back("name", nameNode ? nameNode : std::make_shared<JsonNode>(JsonNode::Type::String));
                            if (!nameNode) extNode->objectProperties.back().second->stringValue = name;
                            
                            extNode->objectProperties.emplace_back("publisher", publisherNode ? publisherNode : std::make_shared<JsonNode>(JsonNode::Type::String));
                            if (!publisherNode) extNode->objectProperties.back().second->stringValue = publisher;
                            
                            std::string fileUriPath = extPath.string();
                            std::replace(fileUriPath.begin(), fileUriPath.end(), '\\', '/');
                            if (!fileUriPath.empty() && fileUriPath[0] != '/') fileUriPath = "/" + fileUriPath;
                            
                            auto extLocNode = std::make_shared<JsonNode>(JsonNode::Type::Object);
                            extLocNode->objectProperties.emplace_back("$mid", std::make_shared<JsonNode>(JsonNode::Type::Number));
                            extLocNode->objectProperties.back().second->numberValue = 1;
                            extLocNode->objectProperties.emplace_back("path", std::make_shared<JsonNode>(JsonNode::Type::String));
                            extLocNode->objectProperties.back().second->stringValue = fileUriPath;
                            extLocNode->objectProperties.emplace_back("scheme", std::make_shared<JsonNode>(JsonNode::Type::String));
                            extLocNode->objectProperties.back().second->stringValue = "file";
                            
                            extNode->objectProperties.emplace_back("extensionLocation", extLocNode);
                            
                            extensionsArr->arrayElements.push_back(extNode);
                        }
                    }
                }
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
