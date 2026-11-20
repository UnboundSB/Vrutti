#pragma once

#include <filesystem>

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

} // namespace vrutti::core::utils
