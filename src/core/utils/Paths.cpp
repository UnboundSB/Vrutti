#include "Paths.h"

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace vrutti::core::utils {

    std::filesystem::path appBasePath() {
#ifdef _WIN32
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length > 0 && length < MAX_PATH) {
            std::filesystem::path exePath(buffer);
            std::filesystem::path base = exePath.parent_path();
            // Development builds live in `<root>/build`, releases in `<root>`.
            if (base.filename() == "build") {
                base = base.parent_path();
            }
            return base;
        }
#else
        char buffer[4096];
        ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
        if (length > 0) {
            buffer[length] = '\0';
            std::filesystem::path base = std::filesystem::path(buffer).parent_path();
            if (base.filename() == "build") {
                base = base.parent_path();
            }
            return base;
        }
#endif
        return std::filesystem::current_path();
    }

    std::filesystem::path appResourcePath(const std::string& relative) {
        return appBasePath() / std::filesystem::path(relative);
    }

    std::filesystem::path pluginPath(const std::string& name) {
        namespace fs = std::filesystem;

        const fs::path base = appBasePath();
        const fs::path releaseLayout = base / fs::path(name);
        const fs::path developmentLayout = base / "build" / fs::path(name);

        std::error_code error;
        if (fs::exists(releaseLayout, error)) {
            return releaseLayout;
        }
        if (fs::exists(developmentLayout, error)) {
            return developmentLayout;
        }
        return releaseLayout;
    }

} // namespace vrutti::core::utils
