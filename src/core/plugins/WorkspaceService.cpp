#include "WorkspaceService.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

bool WorkspaceService::initialize() {
    std::cout << "[WorkspaceService] Initialized workspace topology manager." << std::endl;
    // For Vrutti we might set a default workspace or read from startup args
    return true;
}

void WorkspaceService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_folders.clear();
}

const char* WorkspaceService::getName() const {
    return "WorkspaceService";
}

std::string WorkspaceService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "update_workspace_folders") {
        return handleUpdateWorkspaceFolders(payload);
    } else if (command == "get_workspace_folders") {
        return handleGetWorkspaceFolders(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string WorkspaceService::handleUpdateWorkspaceFolders(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }
    
    // In a real implementation we would parse index, deleteCount, and workspaceFoldersToAdd
    // and then update the m_folders vector. Then broadcast the update via RPC.
    
    std::cout << "[WorkspaceService] Updating workspace folders..." << std::endl;

    return "{\"status\": \"success\"}";
}

std::string WorkspaceService::handleGetWorkspaceFolders(const std::string& payload) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string result = "{\"status\": \"success\", \"folders\": [";
    bool first = true;
    for (const auto& f : m_folders) {
        if (!first) result += ", ";
        result += "{\"uri\": \"" + f.uri + "\", \"name\": \"" + f.name + "\"}";
        first = false;
    }
    result += "]}";
    
    return result;
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new WorkspaceService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
