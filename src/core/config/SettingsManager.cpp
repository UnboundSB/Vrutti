#include "SettingsManager.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cstdlib>

namespace vrutti::core::config {

namespace {

    // The most changes that wait to be written.
    //
    // Every change asks for a write. A program that changes a setting in a
    // loop would fill the queue and use the memory of the editor. The
    // queue only has to hold the last change: the one before it is not
    // needed any more.
    constexpr size_t kMaxSaveQueue = 64;

} // namespace

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
    std::string source;
    bool hasFile = false;
    if (std::filesystem::exists(filePath)) {
        std::ifstream file(filePath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            source = buffer.str();
            hasFile = true;
        }
    }

    std::shared_ptr<utils::JsonNode> root;
    if (hasFile) {
        root = utils::JsonParser::parse(source);
        if (!root || root->type != utils::JsonNode::Type::Object) {
            // A malformed settings file must never take the editor down.
            root = utils::JsonParser::parse("{}");
            source = "{}";
        }
    } else {
        source = "{}";
        root = utils::JsonParser::parse(source);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cachedSource_ = std::move(source);
        root_ = std::move(root);
    }

    if (!hasFile) {
        // Schedule default write (non-blocking)
        enqueueSave("{}");
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

    std::string text;
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

        // Serialize the tree back to JSON. The serializer escapes backslashes
        // and control characters, so the file on disk is always valid JSON --
        // even when a legacy file contained raw Windows paths such as
        // "D:\projects\app".
        cachedSource_ = utils::JsonSerializer::stringify(root_, 0, true);
        text = cachedSource_;
    }

    // Non-blocking: hand off to background writer. The text is taken under
    // the lock above: reading it outside of the lock would read a value
    // that another thread is changing at the same time.
    enqueueSave(text);
}

void SettingsManager::saveAsync() {
    std::string json;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        json = root_ ? utils::JsonSerializer::stringify(root_, 0, true) : cachedSource_;
    }
    enqueueSave(json);
}

void SettingsManager::enqueueSave(const std::string& jsonStr) {
    std::string path = getConfigFilePath();
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // Only the last change is needed: the one before it would be
        // written over by it anyway.
        while (m_saveQueue.size() >= kMaxSaveQueue) {
            m_saveQueue.pop();
        }
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

        // The file is written in one go and then put in place.
        //
        // A file that is written directly can be read while it is written,
        // and a program that stops in the middle leaves a file that is half
        // written. The text is written into a file of its own first, and
        // that file takes the place of the old one.
        std::error_code ec;
        const std::filesystem::path target(job.path);
        const std::filesystem::path temporary =
            std::filesystem::path(job.path + ".tmp");

        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            if (file.is_open()) {
                file << job.content;
                file.flush();
                if (file.good()) {
                    file.close();
                    std::filesystem::rename(temporary, target, ec);
                    if (ec) {
                        // The file that is there is kept: a broken settings
                        // file is worse than an old one.
                        std::error_code removeEc;
                        std::filesystem::remove(temporary, removeEc);
                    }
                }
            }
        }
        if (ec) {
            std::cerr << "[Settings] the settings could not be written: "
                      << ec.message() << std::endl;
        }
    }
}

} // namespace vrutti::core::config
