#pragma once

#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <vector>

namespace vrutti::core::plugins {

    class FileSystemService : public IPlugin {
    public:
        FileSystemService() = default;
        ~FileSystemService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        std::mutex m_mutex;
        // Basic caching for small frequently accessed files
        std::unordered_map<std::string, std::string> m_fileCache;
        std::vector<std::string> m_cachedFiles = { "package.json", ".gitignore", "tsconfig.json" };

        bool shouldCache(const std::string& filename);

        std::string handleStat(const std::string& payload);
        std::string handleReadFile(const std::string& payload);
        std::string handleWriteFile(const std::string& payload);
        std::string handleReadDirectory(const std::string& payload);
        
        // Emits a file changed event to the UI and ExtHost
        void emitFileChanged(const std::string& uri);
    };

} // namespace vrutti::core::plugins
