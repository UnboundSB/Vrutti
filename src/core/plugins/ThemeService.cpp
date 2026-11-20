#include "ThemeService.h"
#include "../utils/Json.h"
#include "../utils/Paths.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;
using vrutti::core::utils::JsonSerializer;

namespace vrutti::core::plugins {

namespace {

    // -------------------------------------------------------------------------
    // Small helpers
    // -------------------------------------------------------------------------

    std::string readFile(const fs::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return "";
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string stringOf(const std::shared_ptr<JsonNode>& node) {
        if (!node || node->type != JsonNode::Type::String) return "";
        return std::string(node->stringValue);
    }

    std::string userHome() {
#ifdef _WIN32
        char path[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) {
            return std::string(path);
        }
        const char* userProfile = std::getenv("USERPROFILE");
        return userProfile ? std::string(userProfile) : std::string();
#else
        const char* home = std::getenv("HOME");
        if (home) return std::string(home);
        struct passwd* pw = getpwuid(getuid());
        return pw ? std::string(pw->pw_dir) : std::string();
#endif
    }

    // Removes `//` and `/* */` comments from a JSON with comments document.
    // String literals are preserved verbatim, so a URL such as "http://x" is
    // not mistaken for a comment.
    std::string stripJsonComments(const std::string& src) {
        std::string out;
        out.reserve(src.size());

        bool inString = false;
        bool escaped = false;

        for (size_t i = 0; i < src.size(); i++) {
            char c = src[i];

            if (inString) {
                out += c;
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }

            if (c == '"') {
                inString = true;
                out += c;
                continue;
            }

            if (c == '/' && i + 1 < src.size()) {
                char next = src[i + 1];
                if (next == '/') {
                    while (i < src.size() && src[i] != '\n') i++;
                    if (i < src.size()) out += '\n';
                    continue;
                }
                if (next == '*') {
                    i += 2;
                    while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/')) i++;
                    i++; // skip the closing '*'
                    continue;
                }
            }

            out += c;
        }
        return out;
    }

    std::shared_ptr<JsonNode> parseJsonc(const std::string& text) {
        if (text.empty()) return nullptr;
        return JsonParser::parse(stripJsonComments(text));
    }

    // -------------------------------------------------------------------------
    // Theme discovery
    // -------------------------------------------------------------------------

    struct ThemeEntry {
        std::string id;
        std::string label;
        std::string uiTheme;
        fs::path themePath;
        std::string extensionName;
    };

    // Reads `contributes.themes` / `contributes.iconThemes` from a package.json
    // and appends every entry it finds.
    void collectThemes(const fs::path& packagePath,
                       const std::string& packageName,
                       std::vector<ThemeEntry>& out,
                       bool icons) {
        auto pkg = parseJsonc(readFile(packagePath));
        if (!pkg || pkg->type != JsonNode::Type::Object) return;

        auto contributes = pkg->get("contributes");
        if (!contributes || contributes->type != JsonNode::Type::Object) return;

        auto list = contributes->get(icons ? "iconThemes" : "themes");
        if (!list || list->type != JsonNode::Type::Array) return;

        fs::path themeRoot = packagePath.parent_path();

        for (const auto& item : list->arrayElements) {
            if (!item || item->type != JsonNode::Type::Object) continue;

            std::string rawId = stringOf(item->get("id"));
            std::string label = stringOf(item->get("label"));
            std::string uiTheme = stringOf(item->get("uiTheme"));
            std::string relPath = stringOf(item->get("path"));

            if (relPath.empty()) continue;

            ThemeEntry entry;
            // Built-in themes are addressed by their own id, extension themes
            // are namespaced with the contributing package.
            entry.id = packageName.empty() ? rawId : (rawId.empty() ? packageName : packageName + "." + rawId);
            entry.label = label.empty() ? entry.id : label;
            entry.uiTheme = uiTheme.empty() ? "vs-dark" : uiTheme;
            entry.themePath = fs::weakly_canonical(themeRoot / relPath);
            entry.extensionName = packageName;

            out.push_back(entry);
        }
    }

    // Collects every theme contributed by the built-in theme package and by
    // the installed user extensions.
    std::vector<ThemeEntry> discoverThemes(bool icons) {
        std::vector<ThemeEntry> themes;

        // Built-in themes ship with the editor. The path is resolved from the
        // installation root, not from the working directory, so the service
        // also works when it is driven by a test harness.
        fs::path builtinPkg = vrutti::core::utils::appResourcePath("src/ext/builtin-themes/package.json");
        if (fs::exists(builtinPkg)) {
            collectThemes(builtinPkg, "", themes, icons);
        }

        // Themes contributed by installed extensions.
        std::string home = userHome();
        if (!home.empty()) {
            fs::path extRoot = fs::path(home) / ".vrutti" / "extensions";
            if (fs::exists(extRoot) && fs::is_directory(extRoot)) {
                std::error_code ec;
                for (const auto& entry : fs::directory_iterator(extRoot, ec)) {
                    if (ec) break;
                    if (!entry.is_directory()) continue;

                    fs::path pkgPath = entry.path() / "extension" / "package.json";
                    if (!fs::exists(pkgPath)) {
                        pkgPath = entry.path() / "package.json";
                    }
                    if (!fs::exists(pkgPath)) continue;

                    auto pkg = parseJsonc(readFile(pkgPath));
                    if (!pkg || pkg->type != JsonNode::Type::Object) continue;

                    std::string packageName = stringOf(pkg->get("name"));
                    if (packageName.empty()) continue;

                    collectThemes(pkgPath, packageName, themes, icons);
                }
            }
        }

        return themes;
    }

    // Finds a theme by id, label or contributing extension name.
    const ThemeEntry* findTheme(const std::vector<ThemeEntry>& themes,
                                const std::string& wanted) {
        if (wanted.empty()) return nullptr;
        for (const auto& t : themes) {
            if (t.id == wanted) return &t;
        }
        for (const auto& t : themes) {
            if (t.label == wanted) return &t;
        }
        for (const auto& t : themes) {
            if (t.extensionName == wanted) return &t;
        }
        return nullptr;
    }

    // -------------------------------------------------------------------------
    // Theme loading
    // -------------------------------------------------------------------------

    // Copies every property of `src` into `dst`, overwriting existing values.
    void mergeInto(const std::shared_ptr<JsonNode>& dst,
                   const std::shared_ptr<JsonNode>& src) {
        if (!dst || !src || src->type != JsonNode::Type::Object) return;

        for (const auto& prop : src->objectProperties) {
            bool replaced = false;
            for (auto& existing : dst->objectProperties) {
                if (existing.first == prop.first) {
                    existing.second = prop.second;
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                dst->objectProperties.emplace_back(prop.first, prop.second);
            }
        }
    }

    // Loads a theme file and resolves its `include` chain. The base theme is
    // merged first, the including theme overrides it.
    std::shared_ptr<JsonNode> loadThemeRecursive(const fs::path& themePath,
                                                 std::set<std::string>& visited) {
        auto result = std::make_shared<JsonNode>(JsonNode::Type::Object);
        result->objectProperties.emplace_back("colors",
            std::make_shared<JsonNode>(JsonNode::Type::Object));
        result->objectProperties.emplace_back("tokenColors",
            std::make_shared<JsonNode>(JsonNode::Type::Array));

        if (themePath.empty()) return result;

        std::string key = fs::weakly_canonical(themePath).string();
        if (visited.count(key)) return result; // guard against include loops
        visited.insert(key);

        auto theme = parseJsonc(readFile(themePath));
        if (!theme || theme->type != JsonNode::Type::Object) return result;

        // Resolve the base theme first.
        std::string include = stringOf(theme->get("include"));
        if (!include.empty()) {
            fs::path includePath = fs::weakly_canonical(themePath.parent_path() / include);
            auto parent = loadThemeRecursive(includePath, visited);
            mergeInto(result, parent);
        }

        // The including theme wins.
        mergeInto(result, theme);

        // `colors` and `tokenColors` need a deep merge: a child theme only
        // overrides the colors it actually mentions.
        auto parentColors = result->get("colors");
        if (!parentColors || parentColors->type != JsonNode::Type::Object) {
            parentColors = std::make_shared<JsonNode>(JsonNode::Type::Object);
            result->objectProperties.emplace_back("colors", parentColors);
        }
        if (auto childColors = theme->get("colors");
            childColors && childColors->type == JsonNode::Type::Object) {
            mergeInto(parentColors, childColors);
        }

        auto parentTokens = result->get("tokenColors");
        if (!parentTokens || parentTokens->type != JsonNode::Type::Array) {
            parentTokens = std::make_shared<JsonNode>(JsonNode::Type::Array);
            result->objectProperties.emplace_back("tokenColors", parentTokens);
        }
        if (auto childTokens = theme->get("tokenColors");
            childTokens && childTokens->type == JsonNode::Type::Array) {
            for (const auto& token : childTokens->arrayElements) {
                parentTokens->arrayElements.push_back(token);
            }
        }

        return result;
    }

    // -------------------------------------------------------------------------
    // Colour translation
    // -------------------------------------------------------------------------

    const std::map<std::string, std::string>& tokenMap() {
        static const std::map<std::string, std::string> map = {
            { "editor.background",                    "--vrutti-bg" },
            { "sideBar.background",                   "--vrutti-surface" },
            { "activityBar.background",               "--vrutti-surface" },
            { "editorGroupHeader.tabsBackground",     "--vrutti-surface" },
            { "editor.foreground",                    "--vrutti-text-bright" },
            { "sideBarTitle.foreground",              "--vrutti-text" },
            { "tab.activeBackground",                 "--vrutti-surface-border" },
            { "button.background",                    "--vrutti-accent" },
            { "focusBorder",                          "--vrutti-accent" },
            { "editorLineNumber.foreground",          "--vrutti-text" },
            { "terminal.background",                  "--vrutti-bg" },
            { "gitDecoration.modifiedResourceForeground", "--vrutti-git-modified" },
            { "gitDecoration.untrackedResourceForeground", "--vrutti-git-untracked" },
            { "gitDecoration.deletedResourceForeground",  "--vrutti-git-deleted" },
        };
        return map;
    }

    std::string toFileUrl(const fs::path& path) {
        std::string p = path.string();
        std::replace(p.begin(), p.end(), '\\', '/');
        if (!p.empty() && p[0] != '/') p = "/" + p;
        return "file://" + p;
    }

} // namespace

// ---------------------------------------------------------------------------
// Plugin interface
// ---------------------------------------------------------------------------

bool ThemeService::initialize() { return true; }
void ThemeService::shutdown() {}

const char* ThemeService::getName() const { return "ThemeService"; }

std::string ThemeService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "list" || command == "listIcons") {
        bool icons = (command == "listIcons");
        auto themes = discoverThemes(icons);

        std::string out = "[";
        bool first = true;
        for (const auto& t : themes) {
            if (!first) out += ",";
            first = false;
            out += "{";
            out += "\"id\":" + JsonSerializer::escapeString(t.id) + ",";
            out += "\"label\":" + JsonSerializer::escapeString(t.label) + ",";
            out += "\"uiTheme\":" + JsonSerializer::escapeString(t.uiTheme) + ",";
            out += "\"themePath\":" + JsonSerializer::escapeString(t.themePath.string()) + ",";
            out += "\"extensionName\":" + JsonSerializer::escapeString(t.extensionName);
            out += "}";
        }
        out += "]";
        return out;
    }

    if (command == "load" || command == "loadIcon") {
        bool icons = (command == "loadIcon");
        auto themes = discoverThemes(icons);
        const ThemeEntry* entry = findTheme(themes, payload);
        if (!entry) {
            std::cerr << "[ThemeService] Theme not found: " << payload << "\n";
            return "{}";
        }

        std::set<std::string> visited;
        auto theme = loadThemeRecursive(entry->themePath, visited);
        if (!theme) theme = std::make_shared<JsonNode>(JsonNode::Type::Object);

        auto out = std::make_shared<JsonNode>(JsonNode::Type::Object);
        out->objectProperties.emplace_back("id", vrutti::core::utils::makeJsonString(entry->id));
        out->objectProperties.emplace_back("name", vrutti::core::utils::makeJsonString(entry->label));
        out->objectProperties.emplace_back("uiTheme", vrutti::core::utils::makeJsonString(entry->uiTheme));

        auto sourceColors = theme->get("colors");
        auto colors = std::make_shared<JsonNode>(JsonNode::Type::Object);

        if (icons) {
            // Icon themes keep their own structure; the icon paths have to be
            // turned into URLs the renderer can load.
            if (sourceColors && sourceColors->type == JsonNode::Type::Object) {
                mergeInto(colors, sourceColors);
            }
            auto definitions = theme->get("iconDefinitions");
            if (definitions && definitions->type == JsonNode::Type::Object) {
                fs::path themeDir = entry->themePath.parent_path();
                for (auto& def : definitions->objectProperties) {
                    if (!def.second || def.second->type != JsonNode::Type::Object) continue;
                    auto iconPath = def.second->get("iconPath");
                    if (!iconPath || iconPath->type != JsonNode::Type::String) continue;
                    fs::path absolute = fs::weakly_canonical(themeDir / std::string(iconPath->stringValue));
                    def.second->objectProperties.emplace_back(
                        "iconPath", vrutti::core::utils::makeJsonString(toFileUrl(absolute)));
                }
                out->objectProperties.emplace_back("iconDefinitions", definitions);
            }
        } else {
            // Translate the editor colour tokens into Vrutti CSS variables.
            if (sourceColors && sourceColors->type == JsonNode::Type::Object) {
                for (const auto& prop : sourceColors->objectProperties) {
                    if (!prop.second || prop.second->type != JsonNode::Type::String) continue;
                    auto mapped = tokenMap().find(prop.first);
                    std::string key = (mapped != tokenMap().end()) ? mapped->second : prop.first;
                    colors->objectProperties.emplace_back(key, prop.second);
                }
            }

            // A theme without a background is unusable, so fall back to the
            // default background for its light/dark variant.
            bool hasBackground = false;
            for (const auto& prop : colors->objectProperties) {
                if (prop.first == "--vrutti-bg") { hasBackground = true; break; }
            }
            if (!hasBackground) {
                std::string type = stringOf(theme->get("type"));
                std::string fallback = (type == "light") ? "#ffffff" : "#1e1e1e";
                colors->objectProperties.emplace_back(
                    "--vrutti-bg", vrutti::core::utils::makeJsonString(fallback));
            }
        }

        out->objectProperties.emplace_back("colors", colors);

        auto tokenColors = theme->get("tokenColors");
        out->objectProperties.emplace_back(
            "tokenColors",
            tokenColors ? tokenColors : std::make_shared<JsonNode>(JsonNode::Type::Array));

        return JsonSerializer::stringify(out, 0, false);
    }

    return "{}";
}

// C entry points for the dynamic loader.
extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new ThemeService();
    }

    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace vrutti::core::plugins
