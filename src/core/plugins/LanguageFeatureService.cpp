#include "LanguageFeatureService.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

bool LanguageFeatureService::initialize() {
    std::cout << "[LanguageFeatureService] Initialized for IntelliSense bridge." << std::endl;
    return true;
}

void LanguageFeatureService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_providers.clear();
}

const char* LanguageFeatureService::getName() const {
    return "LanguageFeatureService";
}

std::string LanguageFeatureService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "register_provider") {
        return handleRegisterProvider(payload);
    } else if (command == "unregister_provider") {
        return handleUnregisterProvider(payload);
    } else if (command == "request_completions") {
        return handleRequestCompletions(payload);
    } else if (command == "request_hover") {
        return handleRequestHover(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string LanguageFeatureService::handleRegisterProvider(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    int handle = -1;
    std::string type;
    std::vector<std::string> languageIds;

    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "handle" && prop.second->type == JsonNode::Type::Number) {
            handle = static_cast<int>(prop.second->numberValue);
        } else if (prop.first == "type" && prop.second->type == JsonNode::Type::String) {
            type = prop.second->stringValue;
        } else if (prop.first == "languageIds" && prop.second->type == JsonNode::Type::Array) {
            for (auto& elem : prop.second->arrayElements) {
                if (elem->type == JsonNode::Type::String) {
                    languageIds.push_back(std::string(elem->stringValue));
                }
            }
        }
    }

    if (handle == -1 || type.empty()) return "{\"status\": \"error\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    auto meta = std::make_shared<ProviderMetadata>();
    meta->handle = handle;
    meta->type = type;
    meta->languageIds = languageIds;
    m_providers[handle] = meta;

    return "{\"status\": \"success\"}";
}

std::string LanguageFeatureService::handleUnregisterProvider(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    int handle = -1;
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "handle" && prop.second->type == JsonNode::Type::Number) {
            handle = static_cast<int>(prop.second->numberValue);
        }
    }

    if (handle == -1) return "{\"status\": \"error\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    m_providers.erase(handle);

    return "{\"status\": \"success\"}";
}

std::string LanguageFeatureService::handleRequestCompletions(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri;
    int line = 0, character = 0;
    
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        } else if (prop.first == "line" && prop.second->type == JsonNode::Type::Number) {
            line = static_cast<int>(prop.second->numberValue);
        } else if (prop.first == "character" && prop.second->type == JsonNode::Type::Number) {
            character = static_cast<int>(prop.second->numberValue);
        }
    }

    if (uri.empty()) return "{\"status\": \"error\"}";

    // In a full implementation, we route this to ExtHostLanguageFeaturesShape.$provideCompletionItems
    // and await the JSON response array of ISuggestResultDto
    std::cout << "[LanguageFeatureService] Routing completions request for " << uri << " at " << line << ":" << character << std::endl;

    return "{\"status\": \"success\", \"items\": []}";
}

std::string LanguageFeatureService::handleRequestHover(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri;
    int line = 0, character = 0;

    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        } else if (prop.first == "line" && prop.second->type == JsonNode::Type::Number) {
            line = static_cast<int>(prop.second->numberValue);
        } else if (prop.first == "character" && prop.second->type == JsonNode::Type::Number) {
            character = static_cast<int>(prop.second->numberValue);
        }
    }

    if (uri.empty()) return "{\"status\": \"error\"}";

    // Route to ExtHostLanguageFeaturesShape.$provideHover
    std::cout << "[LanguageFeatureService] Routing hover request for " << uri << " at " << line << ":" << character << std::endl;

    return "{\"status\": \"success\", \"contents\": []}";
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new LanguageFeatureService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
