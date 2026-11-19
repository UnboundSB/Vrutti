#include "MediaController.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

bool MediaController::initialize() {
    return true;
}

void MediaController::shutdown() {
    currentMediaId.clear();
    currentMediaPath.clear();
}

const char* MediaController::getName() const {
    return "MediaController";
}

std::string MediaController::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "set_background_media") {
        auto reqNode = JsonParser::parse(payload);
        if (reqNode && reqNode->type == JsonNode::Type::Object) {
            std::string url = "";
            for (auto& prop : reqNode->objectProperties) {
                if (prop.first == "url" && prop.second->type == JsonNode::Type::String) {
                    url = prop.second->stringValue;
                }
            }
            if (!url.empty()) {
                currentMediaPath = url;
                std::cout << "[MediaController] Setting background media to: " << url << std::endl;
                // Here we would interface with actual UI compositing elements to aggressively manage caching and rendering
                return "{\"status\": \"success\"}";
            }
        }
        return "{\"status\": \"error\", \"message\": \"invalid payload\"}";
    }
    if (command == "release_media") {
        currentMediaPath.clear();
        std::cout << "[MediaController] Aggressively releasing background media." << std::endl;
        return "{\"status\": \"success\"}";
    }
    return "{\"status\": \"unknown_command\"}";
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new MediaController();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
