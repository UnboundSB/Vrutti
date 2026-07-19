#pragma once
#include <string>
#include <vector>

namespace vrutti::core::fs {

    // A utility class mirroring Node's `path` and VS Code's `vs/base/common/path.ts`
    // It provides cross-platform path string manipulations safely.
    // Underlying implementation uses <filesystem> to guarantee OS compliance.
    class Path {
    public:
        // Returns the last portion of a path (e.g. 'foo/bar/baz.txt' -> 'baz.txt')
        static std::string basename(const std::string& path);
        
        // Returns the directory name of a path (e.g. 'foo/bar/baz.txt' -> 'foo/bar')
        static std::string dirname(const std::string& path);
        
        // Returns the extension of the path (e.g. 'foo/bar/baz.txt' -> '.txt')
        static std::string extname(const std::string& path);
        
        // Joins all given path segments together using the platform-specific separator
        static std::string join(const std::vector<std::string>& segments);
        
        // Resolves a sequence of paths into an absolute path
        static std::string resolve(const std::string& basePath, const std::string& relativePath);
        
        // Normalizes a path, resolving '..' and '.' segments
        static std::string normalize(const std::string& path);

        // Determines if path is absolute
        static bool isAbsolute(const std::string& path);
    };

} // namespace vrutti::core::fs
