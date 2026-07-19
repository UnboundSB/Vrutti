#include "Path.h"
#include <filesystem>

namespace vrutti::core::fs {

    std::string Path::basename(const std::string& path) {
        return std::filesystem::path(path).filename().string();
    }
    
    std::string Path::dirname(const std::string& path) {
        return std::filesystem::path(path).parent_path().string();
    }
    
    std::string Path::extname(const std::string& path) {
        return std::filesystem::path(path).extension().string();
    }
    
    std::string Path::join(const std::vector<std::string>& segments) {
        if (segments.empty()) return "";
        std::filesystem::path result = segments[0];
        for (size_t i = 1; i < segments.size(); ++i) {
            result /= segments[i];
        }
        return result.string();
    }
    
    std::string Path::resolve(const std::string& basePath, const std::string& relativePath) {
        std::filesystem::path base(basePath);
        std::filesystem::path rel(relativePath);
        
        if (rel.is_absolute()) {
            return rel.string(); // Already absolute
        }
        
        // weakly_canonical resolves relative dots without throwing if the file doesn't exist
        return std::filesystem::weakly_canonical(base / rel).string(); 
    }
    
    std::string Path::normalize(const std::string& path) {
        return std::filesystem::path(path).lexically_normal().string();
    }

    bool Path::isAbsolute(const std::string& path) {
        return std::filesystem::path(path).is_absolute();
    }

} // namespace vrutti::core::fs
