#pragma once

#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <list>

namespace vrutti::core::plugins {
    class VSXRegistryService : public IPlugin {
    public:
        VSXRegistryService();
        ~VSXRegistryService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct CacheEntry {
            std::string response;
            uint64_t timestamp;
        };

        std::string searchExtensions(const std::string& query);
        std::string fetchFromOpenVSX(const std::string& query);

        std::unordered_map<std::string, std::list<std::pair<std::string, CacheEntry>>::iterator> m_cacheMap;
        std::list<std::pair<std::string, CacheEntry>> m_cacheList;
        size_t m_maxCacheSize;
        std::mutex m_cacheMutex;
    };
}
