#include "Paths.h"

#include <algorithm>
#include <cctype>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace vrutti::core::utils {

    namespace {

        // The path of a thing inside the installation, or nothing when the
        // name asks for something outside of it.
        //
        // A name that is absolute, or that walks up with "..", would point
        // at a file outside of the installation: the editor would load a
        // plugin or a program that anybody could put there.
        bool staysInside(const std::filesystem::path& base,
                         const std::filesystem::path& relative) {
            if (relative.empty()) return false;
            if (relative.is_absolute()) return false;
            if (relative.has_root_name() || relative.has_root_directory()) return false;

            // The name is put into its plain form first: "a/../b" becomes
            // "b" and "../b" stays outside.
            std::error_code ec;
            std::filesystem::path normalized = relative.lexically_normal();
            if (normalized.empty()) return false;
            if (normalized == "..") return false;

            const std::filesystem::path first = *normalized.begin();
            if (first == "..") return false;

            // The root of the installation is put into its plain form as
            // well, so that the two can be compared.
            std::filesystem::path root = base.lexically_normal();
            if (root.empty()) return false;

            const std::filesystem::path candidate = (root / normalized).lexically_normal();
            const std::string rootText = root.generic_string();
            const std::string candidateText = candidate.generic_string();
            if (candidateText.size() <= rootText.size()) return false;
            if (candidateText.compare(0, rootText.size(), rootText) != 0) return false;
            // The root has to end with a separator, or the candidate would
            // only share its first letters with it.
            return candidateText.size() > rootText.size() &&
                   (rootText.back() == '/' || rootText.back() == '\\' ||
                    candidateText[rootText.size()] == '/' ||
                    candidateText[rootText.size()] == '\\');
        }

    } // namespace

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
        const std::filesystem::path base = appBasePath();
        if (!staysInside(base, std::filesystem::path(relative))) {
            // The name asks for something outside of the installation. The
            // path of the installation itself is returned: the caller will
            // not find a file there, and nothing outside of the
            // installation is ever named.
            return base;
        }
        return base / std::filesystem::path(relative);
    }

    std::filesystem::path pluginPath(const std::string& name) {
        namespace fs = std::filesystem;

        const fs::path base = appBasePath();
        if (!staysInside(base, fs::path(name))) {
            return base;
        }

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

    bool pathStaysInside(const std::filesystem::path& root,
                         const std::filesystem::path& candidate) {
        namespace fs = std::filesystem;

        if (root.empty() || candidate.empty()) return false;

        // Both paths are put into their plain form first. A link that
        // points somewhere else is then seen for what it is, and a name
        // that walks up with ".." is seen for what it is too.
        std::error_code error;
        fs::path resolvedRoot = fs::weakly_canonical(root, error);
        if (error) return false;
        fs::path resolvedCandidate = fs::weakly_canonical(candidate, error);
        if (error) return false;

        std::string rootText = resolvedRoot.generic_string();
        std::string candidateText = resolvedCandidate.generic_string();
#ifdef _WIN32
        // The paths of Windows do not care about the case.
        std::transform(rootText.begin(), rootText.end(), rootText.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(candidateText.begin(), candidateText.end(), candidateText.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
        if (rootText.empty() || candidateText.size() <= rootText.size()) return false;
        if (candidateText.compare(0, rootText.size(), rootText) != 0) return false;
        // The root has to end with a separator, or the candidate would only
        // share its first letters with it.
        return rootText.back() == '/' || rootText.back() == '\\' ||
               candidateText[rootText.size()] == '/' ||
               candidateText[rootText.size()] == '\\';
    }

    std::filesystem::path pathInsideFolder(const std::filesystem::path& root,
                                           const std::string& relative) {
        namespace fs = std::filesystem;

        if (root.empty() || relative.empty()) return fs::path();

        const fs::path name(relative);
        // A name that names a place of its own is not a name inside of
        // the folder.
        if (name.is_absolute() || name.has_root_name() || name.has_root_directory()) {
            return fs::path();
        }

        const fs::path candidate(root / name);
        if (!pathStaysInside(root, candidate)) return fs::path();

        std::error_code error;
        fs::path resolved = fs::weakly_canonical(candidate, error);
        if (error) return fs::path();
        return resolved;
    }

} // namespace vrutti::core::utils
