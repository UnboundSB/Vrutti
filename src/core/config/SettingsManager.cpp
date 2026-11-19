#include "SettingsManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace vrutti::core::config {

SettingsManager& SettingsManager::getInstance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager() {
    // Start background writer thread before any load/save
    m_writerThread = std::thread([this]() { writerLoop(); });
    load();
}

SettingsManager::~SettingsManager() {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stop = true;
    }
    m_queueCv.notify_all();
    if (m_writerThread.joinable()) {
        m_writerThread.join();
    }
}

std::string SettingsManager::getConfigFilePath() const {
    std::string path;
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) {
        path = std::string(appData) + "\\Vrutti";
    } else {
        path = ".vrutti";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        path = std::string(home) + "/.config/Vrutti";
    } else {
        path = ".vrutti";
    }
#endif
    std::filesystem::create_directories(path);
    return path + "/settings.json";
}

void SettingsManager::load() {
    std::string filePath = getConfigFilePath();
    if (!std::filesystem::exists(filePath)) {
        cachedSource_ = "{}";
        root_ = utils::JsonParser::parse(cachedSource_);
        // Schedule default write (non-blocking)
        enqueueSave(cachedSource_);
        return;
    }

    std::ifstream file(filePath);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        cachedSource_ = buffer.str();
        root_ = utils::JsonParser::parse(cachedSource_);
    }
}

std::string SettingsManager::getSettingsJson() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!root_) return "{}";
    return utils::JsonSerializer::stringify(root_, 0, true);
}

void SettingsManager::updateSetting(const std::string& key, const std::string& valueRawJson) {
    auto newValueNode = utils::JsonParser::parse(valueRawJson);
    if (!newValueNode) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!root_ || root_->type != utils::JsonNode::Type::Object) {
            root_ = std::make_shared<utils::JsonNode>(utils::JsonNode::Type::Object);
        }

        bool updated = false;
        for (auto& prop : root_->objectProperties) {
            if (prop.first == key) {
                prop.second = newValueNode;
                updated = true;
                break;
            }
        }
        if (!updated) {
            root_->objectProperties.push_back({key, newValueNode});
        }

        cachedSource_ = utils::JsonSerializer::stringify(root_, 0, true);
        root_ = utils::JsonParser::parse(cachedSource_);
    }

    // Non-blocking: hand off to background writer
    enqueueSave(cachedSource_);
}

void SettingsManager::saveAsync() {
    enqueueSave(cachedSource_);
}

void SettingsManager::enqueueSave(const std::string& jsonStr) {
    std::string path = getConfigFilePath();
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_saveQueue.push({std::move(path), jsonStr});
    }
    m_queueCv.notify_one();
}

void SettingsManager::writerLoop() {
    while (true) {
        SaveJob job;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait(lock, [this]() { return m_stop || !m_saveQueue.empty(); });
            if (m_stop && m_saveQueue.empty()) return;
            job = std::move(m_saveQueue.front());
            m_saveQueue.pop();
        }

        std::ofstream file(job.path);
        if (file.is_open()) {
            file << job.content;
        }
    }
}

} // namespace vrutti::core::config
