#include "VSXRegistryService.h"
#include "../utils/Json.h"
#include <iostream>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#else
// Mock for non-windows (would use libcurl usually)
#include <cstdlib>
#endif

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

VSXRegistryService::VSXRegistryService() : m_maxCacheSize(50) {}

bool VSXRegistryService::initialize() {
    std::cout << "[VSXRegistryService] Initialized Extension Registry Service." << std::endl;
    return true;
}

void VSXRegistryService::shutdown() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_cacheMap.clear();
    m_cacheList.clear();
}

const char* VSXRegistryService::getName() const {
    return "VSXRegistryService";
}

std::string VSXRegistryService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "search_extensions") {
        auto reqNode = JsonParser::parse(payload);
        if (reqNode && reqNode->type == JsonNode::Type::Object) {
            std::string query = "";
            for (auto& prop : reqNode->objectProperties) {
                if (prop.first == "query" && prop.second->type == JsonNode::Type::String) {
                    query = prop.second->stringValue;
                }
            }
            return searchExtensions(query);
        }
        return "{\"error\": \"invalid payload\"}";
    }
    return "{\"error\": \"unknown command\"}";
}

std::string VSXRegistryService::fetchFromOpenVSX(const std::string& query) {
#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("VruttiIDE/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "{\"extensions\":[]}";

    std::string url = "https://open-vsx.org/api/-/search?query=" + query;
    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return "{\"extensions\":[]}";
    }

    std::string response;
    char buffer[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    
    if (response.empty()) return "{\"extensions\":[]}";
    return response;
#else
    // Fallback/Mock for non-Windows builds (in real life, use libcurl)
    return "{\"extensions\":[]}";
#endif
}

std::string VSXRegistryService::searchExtensions(const std::string& query) {
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_cacheMap.find(query);
        if (it != m_cacheMap.end()) {
            // Move to front (LRU)
            m_cacheList.splice(m_cacheList.begin(), m_cacheList, it->second);
            // Cache TTL: 5 minutes
            if (now - it->second->second.timestamp < 300000) {
                std::cout << "[VSXRegistryService] Cache hit for query: " << query << std::endl;
                return it->second->second.response;
            }
        }
    }

    std::cout << "[VSXRegistryService] Cache miss for query: " << query << ". Fetching..." << std::endl;
    std::string response = fetchFromOpenVSX(query);
    
    if (response.find("\"extensions\"") != std::string::npos) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cacheList.push_front({query, {response, now}});
        m_cacheMap[query] = m_cacheList.begin();
        
        if (m_cacheMap.size() > m_maxCacheSize) {
            auto last = m_cacheList.end();
            last--;
            m_cacheMap.erase(last->first);
            m_cacheList.pop_back();
        }
    }
    
    return response;
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new VSXRegistryService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
