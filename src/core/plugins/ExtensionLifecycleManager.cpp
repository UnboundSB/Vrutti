#include "ExtensionLifecycleManager.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

bool ExtensionLifecycleManager::initialize() {
    std::cout << "[ExtensionLifecycleManager] Initialized for dynamic extension activation." << std::endl;
    return true;
}

void ExtensionLifecycleManager::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_extensions.clear();
}

const char* ExtensionLifecycleManager::getName() const {
    return "ExtensionLifecycleManager";
}

std::string ExtensionLifecycleManager::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "register_extension") {
        return handleRegisterExtension(payload);
    } else if (command == "trigger_activation_event") {
        return handleTriggerActivationEvent(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string ExtensionLifecycleManager::handleRegisterExtension(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string id;
    std::vector<std::string> activationEvents;

    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "id" && prop.second->type == JsonNode::Type::String) {
            id = prop.second->stringValue;
        } else if (prop.first == "activationEvents" && prop.second->type == JsonNode::Type::Array) {
            for (auto& elem : prop.second->arrayElements) {
                if (elem->type == JsonNode::Type::String) {
                    activationEvents.push_back(std::string(elem->stringValue));
                }
            }
        }
    }

    if (id.empty()) return "{\"status\": \"error\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    auto ext = std::make_shared<ExtensionMetadata>();
    ext->id = id;
    ext->activationEvents = activationEvents;
    ext->isActivated = false;
    m_extensions[id] = ext;

    return "{\"status\": \"success\"}";
}

std::string ExtensionLifecycleManager::handleTriggerActivationEvent(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string event;
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "event" && prop.second->type == JsonNode::Type::String) {
            event = prop.second->stringValue;
        }
    }

    if (event.empty()) return "{\"status\": \"error\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_extensions) {
        auto ext = pair.second;
        if (!ext->isActivated) {
            // Check if this extension listens to this event, or if it listens to "*"
            bool shouldActivate = false;
            for (const auto& ev : ext->activationEvents) {
                if (ev == event || ev == "*") {
                    shouldActivate = true;
                    break;
                }
            }
            if (shouldActivate) {
                activateExtension(ext, event);
            }
        }
    }

    return "{\"status\": \"success\"}";
}

void ExtensionLifecycleManager::activateExtension(std::shared_ptr<ExtensionMetadata> ext, const std::string& reason) {
    ext->isActivated = true;
    std::cout << "[ExtensionLifecycleManager] Activating extension: " << ext->id << " (Reason: " << reason << ")" << std::endl;
    // In a full implementation, this triggers VSCodeExtHostService to send ExtHostExtensionServiceShape.$activateByEvent
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new ExtensionLifecycleManager();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
