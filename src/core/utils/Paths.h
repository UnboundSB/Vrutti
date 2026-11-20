#pragma once

#include <filesystem>
#include <string>

namespace vrutti::core::utils {

    // Resolves the root of the Vrutti installation.
    //
    // The editor runs from `<install>/build/vrutti.exe` during development and
    // from `<install>/vrutti.exe` in a release, so the root is the directory
    // that holds `src/`. It is derived from the executable location rather
    // than from the current working directory, so it stays correct no matter
    // where the process was started from.
    std::filesystem::path appBasePath();

    // Resolves a path inside the installation, e.g. "src/ext/bin/node.exe".
    std::filesystem::path appResourcePath(const std::string& relative);

    // Resolves the path of a plugin library.
    //
    // During development the plugin libraries are built into `<root>/build`,
    // in a release they sit next to the executable. Both layouts are checked,
    // so the same code works in both. The name that is returned first is the
    // one that exists on disk; when none does, the release layout is
    // returned, because that is the one the error message should name.
    std::filesystem::path pluginPath(const std::string& name);

} // namespace vrutti::core::utils
