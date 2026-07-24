#pragma once
#include <string>
#include <memory>
#include <mutex>
#include "../utils/Json.h"

namespace vrutti::core::config {

    class SettingsManager {
    public:
        // Singleton access
        static SettingsManager& getInstance();

        // Gets the current configuration as a JSON string
        std::string getSettingsJson();

        // Updates a specific key in the configuration and triggers an async save
        void updateSetting(const std::string& key, const std::string& valueRawJson);

        // Loads settings from disk synchronously (called on boot)
        void load();

    private:
        SettingsManager();
        ~SettingsManager() = default;
        
        SettingsManager(const SettingsManager&) = delete;
        SettingsManager& operator=(const SettingsManager&) = delete;

        std::string getConfigFilePath() const;
        void saveAsync();

        std::mutex mutex_;
        std::shared_ptr<vrutti::core::utils::JsonNode> root_;
        std::string cachedSource_; // Keep the source alive since JsonNode holds string_views
    };

} // namespace vrutti::core::config
