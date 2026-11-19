#include "CommandHandlerService.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

bool CommandHandlerService::initialize() {
    std::cout << "[CommandHandlerService] Initialized command registry." << std::endl;
    return true;
}

void CommandHandlerService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commands.clear();
}

const char* CommandHandlerService::getName() const {
    return "CommandHandlerService";
}

std::string CommandHandlerService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "register_command") {
        return handleRegisterCommand(payload);
    } else if (command == "unregister_command") {
        return handleUnregisterCommand(payload);
    } else if (command == "get_active_commands") {
        return handleGetActiveCommands(payload);
    } else if (command == "execute_command") {
        return handleExecuteCommand(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string CommandHandlerService::handleRegisterCommand(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string id;
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "id" && prop.second->type == JsonNode::Type::String) {
            id = prop.second->stringValue;
        }
    }

    if (id.empty()) return "{\"status\": \"error\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    CommandMetadata meta;
    meta.id = id;
    m_commands[id] = meta;

    return "{\"status\": \"success\"}";
}

std::string CommandHandlerService::handleUnregisterCommand(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string id;
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "id" && prop.second->type == JsonNode::Type::String) {
            id = prop.second->stringValue;
        }
    }

    if (id.empty()) return "{\"status\": \"error\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    m_commands.erase(id);

    return "{\"status\": \"success\"}";
}

std::string CommandHandlerService::handleGetActiveCommands(const std::string& payload) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string result = "{\"status\": \"success\", \"commands\": [";
    bool first = true;
    for (const auto& pair : m_commands) {
        if (!first) result += ", ";
        result += "\"" + pair.first + "\"";
        first = false;
    }
    result += "]}";
    return result;
}

std::string CommandHandlerService::handleExecuteCommand(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string id;
    // args could be parsed here
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "id" && prop.second->type == JsonNode::Type::String) {
            id = prop.second->stringValue;
        }
    }

    if (id.empty()) return "{\"status\": \"error\"}";

    // In a full implementation, we route this to ExtHostCommandsShape.$executeCommand over RPC
    std::cout << "[CommandHandlerService] Routing command execution to ExtHost: " << id << std::endl;

    return "{\"status\": \"success\"}";
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new CommandHandlerService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
