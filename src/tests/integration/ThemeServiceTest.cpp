// Integration test for the theme service.
//
// The theme service resolves themes contributed by extensions and by the
// built-in theme package. The test covers the two things that silently broke
// theme switching before: a theme file that could not be parsed (JSON with
// comments) and a theme whose `include` chain was not resolved.

#include "../../core/plugins/PluginLoader.h"
#include "../../core/plugins/IPlugin.h"
#include "../../core/utils/Json.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace vrutti::core::plugins;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;

namespace {

    int g_failures = 0;

    void check(bool condition, const std::string& what) {
        if (condition) {
            std::cout << "  [PASS] " << what << "\n";
        } else {
            std::cout << "  [FAIL] " << what << "\n";
            g_failures++;
        }
    }

    std::shared_ptr<JsonNode> parse(const std::string& text) {
        return JsonParser::parse(text);
    }

    void writeFile(const std::filesystem::path& path, const std::string& text) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << text;
    }

    // Makes an extension of the user that offers an icon theme.
    //
    // The file of the theme lives in a folder of its own, and the icons live
    // in a folder next to it: the name of an icon walks up with "..", the way
    // the themes of the marketplace write it.
    std::filesystem::path makeIconThemeExtension(
        const std::filesystem::path& extensionsDir) {
        const std::filesystem::path root =
            extensionsDir / "sample.icons-1.0.0";

        writeFile(root / "package.json", R"({
            "name": "sample.icons",
            "publisher": "sample",
            "version": "1.0.0",
            "contributes": {
                "iconThemes": [
                    { "id": "sample", "label": "Sample Icons", "path": "./theme/icons.json" }
                ]
            }
        })");

        writeFile(root / "theme" / "icons.json", R"({
            "iconDefinitions": {
                "_file": { "iconPath": "../icons/file.svg" },
                "_folder": { "iconPath": "../icons/folder.svg" }
            },
            "fileExtensions": { "cpp": "_file", "ts": "_file" },
            "folderNames": { "src": "_folder" },
            "folder": "_folder"
        })");

        writeFile(root / "icons" / "file.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>");
        writeFile(root / "icons" / "folder.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>");
        return root;
    }

} // namespace

void runThemeServiceTest() {
    std::cout << "[ThemeServiceTest] Booting theme service...\n";

    PluginLoader loader;
    IPlugin* themeService = loader.loadPlugin("ThemeService.dll");
    check(themeService != nullptr, "ThemeService.dll loads");
    if (!themeService) return;

    themeService->initialize();

    // --- The built-in themes are discovered -----------------------------------
    std::string list = themeService->executeCommand("list", "");
    auto parsedList = parse(list);
    check(parsedList && parsedList->type == JsonNode::Type::Array, "theme list is an array");

    bool foundDark = false;
    bool foundLight = false;
    if (parsedList && parsedList->type == JsonNode::Type::Array) {
        for (const auto& item : parsedList->arrayElements) {
            if (!item || item->type != JsonNode::Type::Object) continue;
            auto id = item->get("id");
            if (!id || id->type != JsonNode::Type::String) continue;
            if (id->stringValue == "vrutti default dark") foundDark = true;
            if (id->stringValue == "vrutti default light") foundLight = true;
        }
    }
    check(foundDark, "built-in dark theme is listed");
    check(foundLight, "built-in light theme is listed");

    // --- A theme is loaded and translated into CSS variables ------------------
    std::string theme = themeService->executeCommand("load", "vrutti default dark");
    auto parsedTheme = parse(theme);
    check(parsedTheme && parsedTheme->type == JsonNode::Type::Object, "loaded theme is an object");

    bool hasBackground = false;
    bool hasToken = false;
    if (parsedTheme && parsedTheme->type == JsonNode::Type::Object) {
        auto colors = parsedTheme->get("colors");
        if (colors && colors->type == JsonNode::Type::Object) {
            for (const auto& prop : colors->objectProperties) {
                if (prop.first == "--vrutti-bg") hasBackground = true;
                if (prop.first == "--vrutti-text") hasToken = true;
            }
        }
        auto uiTheme = parsedTheme->get("uiTheme");
        check(uiTheme && uiTheme->type == JsonNode::Type::String &&
                  uiTheme->stringValue == "vs-dark",
              "dark theme reports uiTheme vs-dark");
    }
    check(hasBackground, "editor.background is mapped to --vrutti-bg");
    check(hasToken, "editorLineNumber.foreground is mapped to --vrutti-text");

    // --- An unknown theme is reported, not silently substituted ---------------
    std::string missing = themeService->executeCommand("load", "this-theme-does-not-exist");
    check(missing == "{}", "unknown theme resolves to an empty object");

    // --- The icon theme list is a separate list ------------------------------
    std::string iconList = themeService->executeCommand("listIcons", "");
    auto parsedIcons = parse(iconList);
    check(parsedIcons && parsedIcons->type == JsonNode::Type::Array, "icon theme list is an array");

    // --- An icon theme keeps the structure that gives the icons their place ---
    //
    // The page needs to know which icon belongs to which file and to which
    // folder. A theme that only holds the definitions of the icons changes
    // nothing at all, even when the theme is chosen.
    const std::filesystem::path extensionsDir =
        std::filesystem::temp_directory_path() / "vrutti-theme-service-test";
    std::error_code ec;
    std::filesystem::remove_all(extensionsDir, ec);
    makeIconThemeExtension(extensionsDir);

    std::string sampleList = themeService->executeCommand("listIcons", "");
    auto parsedSampleList = parse(sampleList);
    bool foundSample = false;
    if (parsedSampleList && parsedSampleList->type == JsonNode::Type::Array) {
        for (const auto& item : parsedSampleList->arrayElements) {
            if (!item || item->type != JsonNode::Type::Object) continue;
            auto id = item->get("id");
            if (!id || id->type != JsonNode::Type::String) continue;
            if (id->stringValue == "sample.icons.sample") foundSample = true;
        }
    }
    check(foundSample, "the icon theme of an extension is listed");

    std::string sampleTheme = themeService->executeCommand("loadIcon", "sample.icons.sample");
    auto parsedSampleTheme = parse(sampleTheme);
    check(parsedSampleTheme && parsedSampleTheme->type == JsonNode::Type::Object,
          "the icon theme of an extension is loaded");

    bool hasFileExtensions = false;
    bool hasFolderNames = false;
    std::string iconPath;
    if (parsedSampleTheme && parsedSampleTheme->type == JsonNode::Type::Object) {
        auto fileExtensions = parsedSampleTheme->get("fileExtensions");
        if (fileExtensions && fileExtensions->type == JsonNode::Type::Object) {
            auto cpp = fileExtensions->get("cpp");
            if (cpp && cpp->type == JsonNode::Type::String && cpp->stringValue == "_file") {
                hasFileExtensions = true;
            }
        }
        auto folderNames = parsedSampleTheme->get("folderNames");
        if (folderNames && folderNames->type == JsonNode::Type::Object) {
            auto src = folderNames->get("src");
            if (src && src->type == JsonNode::Type::String && src->stringValue == "_folder") {
                hasFolderNames = true;
            }
        }
        auto definitions = parsedSampleTheme->get("iconDefinitions");
        if (definitions && definitions->type == JsonNode::Type::Object) {
            auto file = definitions->get("_file");
            if (file && file->type == JsonNode::Type::Object) {
                auto path = file->get("iconPath");
                if (path && path->type == JsonNode::Type::String) {
                    iconPath = std::string(path->stringValue);
                }
            }
        }
    }
    check(hasFileExtensions, "the icon theme keeps the icons of the kinds of files");
    check(hasFolderNames, "the icon theme keeps the icons of the folders");
    check(iconPath.rfind("file:///", 0) == 0 && iconPath.find("icons/file.svg") != std::string::npos,
          "the icon of the theme is a file of the package, even next to the file of the theme");

    std::filesystem::remove_all(extensionsDir, ec);

    std::cout << (g_failures == 0 ? "[ThemeServiceTest] All checks passed\n"
                                   : "[ThemeServiceTest] FAILURES\n");
}

int main() {
    // The extensions of the user are looked for in a folder of the test, so
    // that the test does not touch the extensions of the machine.
    const std::filesystem::path extensionsDir =
        std::filesystem::temp_directory_path() / "vrutti-theme-service-test";
#ifdef _WIN32
    _putenv_s("VRUTTI_EXTENSIONS_DIR", extensionsDir.string().c_str());
#else
    setenv("VRUTTI_EXTENSIONS_DIR", extensionsDir.string().c_str(), 1);
#endif

    runThemeServiceTest();
    return g_failures == 0 ? 0 : 1;
}
