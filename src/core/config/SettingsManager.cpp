#include "SettingsManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <iostream>
#include <cstdlib>

namespace vrutti::core::config {

    SettingsManager& SettingsManager::getInstance() {
        static SettingsManager instance;
        return instance;
    }

    SettingsManager::SettingsManager() {
        load();
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
        std::lock_guard<std::mutex> lock(mutex_);
        std::string filePath = getConfigFilePath();

        if (!std::filesystem::exists(filePath)) {
            // Default config
            cachedSource_ = "{\n  \"workbench.colorTheme\": \"vrutti default dark\",\n  \"editor.fontSize\": 14,\n  \"editor.fontFamily\": \"'Fira Code', monospace\",\n  \"editor.wordWrap\": false\n}";
            root_ = utils::JsonParser::parse(cachedSource_);
            saveAsync(); // Write defaults to disk
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
        std::lock_guard<std::mutex> lock(mutex_);
        if (!root_ || root_->type != utils::JsonNode::Type::Object) {
            root_ = std::make_shared<utils::JsonNode>(utils::JsonNode::Type::Object);
        }

        // Parse the incoming raw JSON value (e.g., "14" or "\"Light+\"" or "false")
        auto newValueNode = utils::JsonParser::parse(valueRawJson);
        if (!newValueNode) return; // invalid JSON payload

        bool updated = false;
        // Search and replace if exists
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

        // We stringify the updated DOM to refresh the backing cachedSource_ so string_views stay valid.
        cachedSource_ = utils::JsonSerializer::stringify(root_, 0, true);
        root_ = utils::JsonParser::parse(cachedSource_);

        saveAsync();
    }

    void SettingsManager::saveAsync() {
        // Deep copy the JSON string so the async task can write it safely
        std::string jsonStr = cachedSource_;
        std::string path = getConfigFilePath();

        if (m_saveFuture.valid()) {
            m_saveFuture.wait(); // Ensure sequential file writes
        }

        m_saveFuture = std::async(std::launch::async, [path, jsonStr]() {
            std::ofstream file(path);
            if (file.is_open()) {
                file << jsonStr;
            }
        });
    }

} // namespace vrutti::core::config
