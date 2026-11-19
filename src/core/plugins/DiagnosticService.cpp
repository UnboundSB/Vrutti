#include "DiagnosticService.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

bool DiagnosticService::initialize() {
    std::cout << "[DiagnosticService] Initialized diagnostic registry." << std::endl;
    return true;
}

void DiagnosticService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_diagnostics.clear();
}

const char* DiagnosticService::getName() const {
    return "DiagnosticService";
}

std::string DiagnosticService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "change_many") {
        return handleChangeMany(payload);
    } else if (command == "clear") {
        return handleClear(payload);
    } else if (command == "get_diagnostics") {
        return handleGetDiagnostics(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string DiagnosticService::handleChangeMany(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri;
    std::vector<Diagnostic> diagnostics;

    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        } else if (prop.first == "diagnostics" && prop.second->type == JsonNode::Type::Array) {
            // Simplified parsing for brevity, in reality we map IMarkerData here
            for (auto& elem : prop.second->arrayElements) {
                if (elem->type == JsonNode::Type::Object) {
                    Diagnostic d;
                    // ... parse fields like severity, message, startLine, etc.
                    diagnostics.push_back(d);
                }
            }
        }
    }

    if (uri.empty()) return "{\"status\": \"error\"}";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_diagnostics[uri] = diagnostics;
    }

    emitDiagnosticsChanged(uri);

    return "{\"status\": \"success\"}";
}

std::string DiagnosticService::handleClear(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri;
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        }
    }

    if (uri.empty()) return "{\"status\": \"error\"}";

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_diagnostics.erase(uri);
    }

    emitDiagnosticsChanged(uri);

    return "{\"status\": \"success\"}";
}

std::string DiagnosticService::handleGetDiagnostics(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri;
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    // Return serialized JSON of diagnostics
    // ...
    return "{\"status\": \"success\", \"diagnostics\": []}";
}

void DiagnosticService::emitDiagnosticsChanged(const std::string& uri) {
    // In a full implementation, this triggers a Tauri event to the frontend
    // e.g. Window::emit("vrutti_on_diagnostics_changed", payload)
    std::cout << "[DiagnosticService] Emitting vrutti_on_diagnostics_changed for " << uri << std::endl;
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new DiagnosticService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
