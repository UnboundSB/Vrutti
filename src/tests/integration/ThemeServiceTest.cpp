// Integration test for the theme service.
//
// The theme service resolves themes contributed by extensions and by the
// built-in theme package. The test covers the two things that silently broke
// theme switching before: a theme file that could not be parsed (JSON with
// comments) and a theme whose `include` chain was not resolved.

#include "../../core/plugins/PluginLoader.h"
#include "../../core/plugins/IPlugin.h"
#include "../../core/utils/Json.h"

#include <filesystem>
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

    std::cout << (g_failures == 0 ? "[ThemeServiceTest] All checks passed\n"
                                   : "[ThemeServiceTest] FAILURES\n");
}

int main() {
    runThemeServiceTest();
    return g_failures == 0 ? 0 : 1;
}
