#include "Paths.h"

#include <string>

#ifdef _WIN32
#include <windows.h>
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
#endif
        return std::filesystem::current_path();
    }

    std::filesystem::path appResourcePath(const std::string& relative) {
        return appBasePath() / std::filesystem::path(relative);
    }

} // namespace vrutti::core::utils
