#include "FileSystemService.h"
#include "../utils/Json.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;
namespace fs = std::filesystem;

bool FileSystemService::initialize() {
    std::cout << "[FileSystemService] Initialized native filesystem bridge." << std::endl;
    return true;
}

void FileSystemService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fileCache.clear();
}

const char* FileSystemService::getName() const {
    return "FileSystemService";
}

std::string FileSystemService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "stat") {
        return handleStat(payload);
    } else if (command == "read_file") {
        return handleReadFile(payload);
    } else if (command == "write_file") {
        return handleWriteFile(payload);
    } else if (command == "read_directory") {
        return handleReadDirectory(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

bool FileSystemService::shouldCache(const std::string& filename) {
    for (const auto& cached : m_cachedFiles) {
        if (filename == cached) return true;
    }
    return false;
}

std::string FileSystemService::handleStat(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    std::string path;
    if (reqNode && reqNode->type == JsonNode::Type::Object) {
        for (auto& prop : reqNode->objectProperties) {
            if (prop.first == "path" && prop.second->type == JsonNode::Type::String) {
                path = prop.second->stringValue;
            }
        }
    }
    
    if (path.empty()) return "{\"status\": \"error\"}";

    try {
        fs::path p(path);
        if (!fs::exists(p)) {
            return "{\"status\": \"error\", \"message\": \"File not found\"}";
        }
        
        auto ftime = fs::last_write_time(p);
        auto size = fs::is_regular_file(p) ? fs::file_size(p) : 0;
        int type = fs::is_directory(p) ? 2 : 1; // 2 = Directory, 1 = File (VS Code FileType enum)
        
        return "{\"status\": \"success\", \"type\": " + std::to_string(type) + ", \"size\": " + std::to_string(size) + "}";
    } catch (const std::exception& e) {
        return "{\"status\": \"error\"}";
    }
}

std::string FileSystemService::handleReadFile(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    std::string path;
    if (reqNode && reqNode->type == JsonNode::Type::Object) {
        for (auto& prop : reqNode->objectProperties) {
            if (prop.first == "path" && prop.second->type == JsonNode::Type::String) {
                path = prop.second->stringValue;
            }
        }
    }
    
    if (path.empty()) return "{\"status\": \"error\"}";

    try {
        fs::path p(path);
        if (!fs::exists(p) || !fs::is_regular_file(p)) {
            return "{\"status\": \"error\"}";
        }

        std::string filename = p.filename().string();
        
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_fileCache.count(path)) {
                // In a real impl, we'd base64 encode or escape the string properly
                return "{\"status\": \"success\", \"cached\": true, \"content\": \"...\"}"; 
            }
        }

        std::ifstream file(path, std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        std::string content = buffer.str();
        
        if (shouldCache(filename)) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_fileCache[path] = content;
        }

        return "{\"status\": \"success\", \"cached\": false, \"content\": \"...\"}"; // Mocked content for brevity
    } catch (...) {
        return "{\"status\": \"error\"}";
    }
}

std::string FileSystemService::handleWriteFile(const std::string& payload) {
    // simplified write handling
    auto reqNode = JsonParser::parse(payload);
    std::string path;
    if (reqNode && reqNode->type == JsonNode::Type::Object) {
        for (auto& prop : reqNode->objectProperties) {
            if (prop.first == "path" && prop.second->type == JsonNode::Type::String) {
                path = prop.second->stringValue;
            }
        }
    }
    
    if (path.empty()) return "{\"status\": \"error\"}";
    
    emitFileChanged(path);
    return "{\"status\": \"success\"}";
}

std::string FileSystemService::handleReadDirectory(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    std::string path;
    if (reqNode && reqNode->type == JsonNode::Type::Object) {
        for (auto& prop : reqNode->objectProperties) {
            if (prop.first == "path" && prop.second->type == JsonNode::Type::String) {
                path = prop.second->stringValue;
            }
        }
    }
    
    if (path.empty()) return "{\"status\": \"error\"}";

    try {
        fs::path p(path);
        if (!fs::exists(p) || !fs::is_directory(p)) {
            return "{\"status\": \"error\"}";
        }

        std::string result = "{\"status\": \"success\", \"entries\": [";
        bool first = true;
        for (const auto& entry : fs::directory_iterator(p)) {
            if (!first) result += ", ";
            int type = entry.is_directory() ? 2 : 1;
            result += "{\"name\": \"" + entry.path().filename().string() + "\", \"type\": " + std::to_string(type) + "}";
            first = false;
        }
        result += "]}";
        
        return result;
    } catch (...) {
        return "{\"status\": \"error\"}";
    }
}

void FileSystemService::emitFileChanged(const std::string& uri) {
    std::cout << "[FileSystemService] Emitting vrutti_on_file_changed for " << uri << std::endl;
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new FileSystemService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
