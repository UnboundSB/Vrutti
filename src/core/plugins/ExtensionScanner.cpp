#include "ExtensionScanner.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace vrutti {
namespace core {
namespace plugins {

std::string ExtensionScanner::getExtensionsDir() {
    fs::path homeDir;
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
        homeDir = path;
    } else {
        const char* userProfile = std::getenv("USERPROFILE");
        if (userProfile) homeDir = userProfile;
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        homeDir = home;
    } else {
        struct passwd* pw = getpwuid(getuid());
        if (pw) homeDir = pw->pw_dir;
    }
#endif
    return (homeDir / ".vrutti" / "extensions").string();
}

std::string ExtensionScanner::getInstalledExtensions() {
    fs::path extDirBase(getExtensionsDir());

    if (!fs::exists(extDirBase) || !fs::is_directory(extDirBase)) {
        return "[]";
    }

    std::string jsonBuilder = "[";
    bool first = true;

    for (const auto& entry : fs::directory_iterator(extDirBase)) {
        if (!entry.is_directory()) continue;

        fs::path pkgPath = entry.path() / "extension" / "package.json";
        if (!fs::exists(pkgPath)) {
            pkgPath = entry.path() / "package.json";
        }
        
        if (fs::exists(pkgPath)) {
            try {
                std::ifstream f(pkgPath, std::ios::binary);
                if (!f.is_open()) continue;
                
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                auto pkg = vrutti::core::utils::JsonParser::parse(content);
                if (!pkg || pkg->type != vrutti::core::utils::JsonNode::Type::Object) continue;
                
                std::string publisher = entry.path().filename().string();
                if (auto pNode = pkg->get("publisher"); pNode && pNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    publisher = vrutti::core::utils::JsonParser::unescapeString(pNode->stringValue);
                } else if (auto aNode = pkg->get("author"); aNode && aNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    publisher = vrutti::core::utils::JsonParser::unescapeString(aNode->stringValue);
                }
                
                std::string name = "";
                if (auto nNode = pkg->get("name"); nNode && nNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    name = vrutti::core::utils::JsonParser::unescapeString(nNode->stringValue);
                }
                
                std::string displayName = name;
                if (auto dnNode = pkg->get("displayName"); dnNode && dnNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    displayName = vrutti::core::utils::JsonParser::unescapeString(dnNode->stringValue);
                }

                std::string description = "";
                if (auto descNode = pkg->get("description"); descNode && descNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    description = vrutti::core::utils::JsonParser::unescapeString(descNode->stringValue);
                }
                
                std::string version = "1.0.0";
                if (auto vNode = pkg->get("version"); vNode && vNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    version = vrutti::core::utils::JsonParser::unescapeString(vNode->stringValue);
                }
                
                bool isTheme = false;
                std::string contributesStr = "{}";
                if (auto contributes = pkg->get("contributes"); contributes && contributes->type == vrutti::core::utils::JsonNode::Type::Object) {
                    contributesStr = vrutti::core::utils::JsonSerializer::stringify(contributes, 0, false);
                    if (contributes->get("themes") || contributes->get("iconThemes")) {
                        isTheme = true;
                    }
                }
                
                std::string icon = "";
                if (auto iconNode = pkg->get("icon"); iconNode && iconNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    icon = vrutti::core::utils::JsonParser::unescapeString(iconNode->stringValue);
                }
                
                std::string localPath = entry.path().string();
                std::replace(localPath.begin(), localPath.end(), '\\', '/');

                std::string iconUrlBase64 = "";
                if (!icon.empty()) {
                    std::filesystem::path iconPath = std::filesystem::path(localPath) / icon;
                    if (std::filesystem::exists(iconPath)) {
                        std::ifstream ifs(iconPath, std::ios::binary);
                        if (ifs) {
                            std::ostringstream ss;
                            ss << ifs.rdbuf();
                            std::string binary = ss.str();
                            static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                            std::string b64out;
                            int val = 0, valb = -6;
                            for (unsigned char c : binary) {
                                val = (val << 8) + c;
                                valb += 8;
                                while (valb >= 0) {
                                    b64out.push_back(b64[(val >> valb) & 0x3F]);
                                    valb -= 6;
                                }
                            }
                            if (valb > -6) b64out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
                            while (b64out.size() % 4) b64out.push_back('=');
                            
                            std::string ext = iconPath.extension().string();
                            std::string mime = "image/png";
                            if (ext == ".svg") mime = "image/svg+xml";
                            else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
                            else if (ext == ".gif") mime = "image/gif";
                            iconUrlBase64 = "data:" + mime + ";base64," + b64out;
                        }
                    }
                }

                if (!first) {
                    jsonBuilder += ",";
                }
                first = false;
                
                jsonBuilder += "{";
                jsonBuilder += "\"id\":" + vrutti::core::utils::JsonSerializer::escapeString(publisher + "." + name) + ",";
                jsonBuilder += "\"name\":" + vrutti::core::utils::JsonSerializer::escapeString(name) + ",";
                jsonBuilder += "\"displayName\":" + vrutti::core::utils::JsonSerializer::escapeString(displayName) + ",";
                jsonBuilder += "\"publisherDisplayName\":" + vrutti::core::utils::JsonSerializer::escapeString(publisher) + ",";
                jsonBuilder += "\"description\":" + vrutti::core::utils::JsonSerializer::escapeString(description) + ",";
                jsonBuilder += "\"version\":" + vrutti::core::utils::JsonSerializer::escapeString(version) + ",";
                jsonBuilder += "\"icon\":" + vrutti::core::utils::JsonSerializer::escapeString(icon) + ",";
                if (!iconUrlBase64.empty()) {
                    jsonBuilder += "\"iconUrl\":" + vrutti::core::utils::JsonSerializer::escapeString(iconUrlBase64) + ",";
                }
                jsonBuilder += "\"isTheme\":" + std::string(isTheme ? "true" : "false") + ",";
                jsonBuilder += "\"contributes\":" + contributesStr + ",";
                jsonBuilder += "\"localPath\":" + vrutti::core::utils::JsonSerializer::escapeString(localPath);
                jsonBuilder += "}";

            } catch (...) {}
        }
    }
    jsonBuilder += "]";
    return jsonBuilder;
}

} // namespace plugins
} // namespace core
} // namespace vrutti
