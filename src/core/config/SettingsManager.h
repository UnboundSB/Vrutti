#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <atomic>
#include "../utils/Json.h"

namespace vrutti::core::config {

struct SaveJob {
    std::string path;
    std::string content;
};

class SettingsManager {
public:
    // Singleton access
    static SettingsManager& getInstance();

    // Gets the current configuration as a JSON string
    std::string getSettingsJson();

    // Updates a specific key in the configuration and triggers a non-blocking background save
    void updateSetting(const std::string& key, const std::string& valueRawJson);

    // Loads settings from disk synchronously (called on boot)
    void load();

    // Explicitly request a background save (non-blocking)
    void saveAsync();

    ~SettingsManager();

private:
    SettingsManager();

    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    std::string getConfigFilePath() const;
    void enqueueSave(const std::string& jsonStr);
    void writerLoop();

    std::mutex mutex_;
    std::shared_ptr<vrutti::core::utils::JsonNode> root_;
    std::string cachedSource_; // Keep the source alive since JsonNode holds string_views

    // Background writer thread
    std::thread m_writerThread;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::queue<SaveJob> m_saveQueue;
    std::atomic<bool> m_stop{false};
};

} // namespace vrutti::core::config
