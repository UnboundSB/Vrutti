#pragma once

#include "IPlugin.h"

#include <string>

namespace vrutti::core::plugins {

    // Resolves and loads colour themes and icon themes.
    //
    // Themes are contributed by extensions through `contributes.themes` and
    // `contributes.iconThemes`. A theme file may pull in a base theme with
    // an `include` directive, and theme files are JSON with comments -- both
    // are handled here so that the renderer only ever sees a resolved,
    // ready-to-apply theme object.
    class ThemeService : public IPlugin {
    public:
        ThemeService() = default;
        ~ThemeService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;

        // Commands:
        //   "list"      -> [{ id, label, uiTheme, themePath, extensionName }]
        //   "listIcons" -> [{ id, label, uiTheme, themePath, extensionName }]
        //   "load"      -> resolved theme object for the id/label in `payload`
        //   "loadIcon"  -> resolved icon theme object for the id/label in `payload`
        std::string executeCommand(const std::string& command, const std::string& payload) override;
    };

} // namespace vrutti::core::plugins
