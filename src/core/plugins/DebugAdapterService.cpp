#include "DebugAdapterService.h"
#include "../utils/Json.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

DebugAdapterService::~DebugAdapterService() {
    shutdown();
}

bool DebugAdapterService::initialize() {
    std::cout << "[DebugAdapterService] Initialized DAP broker." << std::endl;
    return true;
}

void DebugAdapterService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_sessions) {
        if (pair.second) {
            pair.second->active = false;
            if (pair.second->readerThread.joinable()) {
                pair.second->readerThread.join();
            }
            // Real implementation would properly close handles/pipes here
            delete pair.second;
        }
    }
    m_sessions.clear();
}

const char* DebugAdapterService::getName() const {
    return "DebugAdapterService";
}

std::string DebugAdapterService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "debug_start") {
        return handleDebugStart(payload);
    } else if (command == "debug_set_breakpoints") {
        return handleDebugSetBreakpoints(payload);
    } else if (command == "debug_action") {
        return handleDebugAction(payload);
    } else if (command == "debug_stop") {
        return handleDebugStop(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string DebugAdapterService::handleDebugStart(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string sessionId = "session_" + std::to_string(m_sessions.size() + 1);
    std::string type;
    
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "type" && prop.second->type == JsonNode::Type::String) {
            type = prop.second->stringValue;
        }
    }

    if (type.empty()) return "{\"status\": \"error\"}";

    auto session = new DebugSession();
    session->id = sessionId;
    session->type = type;
    session->active = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions[sessionId] = session;
    }

    // The program of the adapter is the one that the request names. The
    // editor does not guess a program: it does not ask for a Python to be
    // installed to debug a file, and an adapter that is not there is a
    // failure that is said out loud.
    std::string adapterCmd;
    std::vector<std::string> args;
    for (auto& prop : reqNode->objectProperties) {
        if ((prop.first == "program" || prop.first == "runtime") &&
            prop.second->type == JsonNode::Type::String) {
            adapterCmd = std::string(prop.second->stringValue);
        } else if (prop.first == "args" && prop.second->type == JsonNode::Type::Array) {
            for (const auto& item : prop.second->arrayElements) {
                if (!item || item->type != JsonNode::Type::String) continue;
                args.push_back(std::string(item->stringValue));
            }
        }
    }

    if (adapterCmd.empty()) {
        session->active = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_sessions.erase(sessionId);
        }
        delete session;
        return "{\"status\": \"error\", \"message\": \"the debug adapter has no program\"}";
    }

    spawnAdapterProcess(session, adapterCmd, args);

    return "{\"status\": \"success\", \"sessionId\": \"" + sessionId + "\"}";
}

std::string DebugAdapterService::handleDebugSetBreakpoints(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    // Parse breakpoints and send `setBreakpoints` DAP message to the specific adapter
    std::cout << "[DebugAdapterService] Forwarding setBreakpoints to adapter..." << std::endl;
    return "{\"status\": \"success\"}";
}

std::string DebugAdapterService::handleDebugAction(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    // Parse action (continue, stepIn, etc) and send DAP message
    std::cout << "[DebugAdapterService] Forwarding debug action to adapter..." << std::endl;
    return "{\"status\": \"success\"}";
}

std::string DebugAdapterService::handleDebugStop(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    std::cout << "[DebugAdapterService] Stopping debug session..." << std::endl;
    // Disconnect DAP and kill process
    return "{\"status\": \"success\"}";
}

void DebugAdapterService::spawnAdapterProcess(DebugSession* session, const std::string& adapterCmd, const std::vector<std::string>& args) {
    // Platform-specific process spawning goes here (CreateProcess on Windows, fork/exec on POSIX).
    // Creates anonymous pipes for stdin and stdout.
    std::cout << "[DebugAdapterService] Spawning native adapter process: " << adapterCmd << std::endl;
    
    // Spawn reader thread to asynchronously read DAP responses from stdout without blocking the main C++ thread
    session->readerThread = std::thread([this, session]() {
        streamStdoutToUI(session);
    });
}

void DebugAdapterService::streamStdoutToUI(DebugSession* session) {
    while (session->active) {
        // 1. Read Content-Length header from session->stdoutPipe
        // 2. Read the JSON payload
        // 3. Emit the event to the UI using emitDebugEvent
        
        // Mocking a DAP event stream
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        emitDebugEvent(session->id, "{\"type\": \"event\", \"event\": \"stopped\", \"body\": {\"reason\": \"breakpoint\"}}");
        
        // In a real implementation, a blocking read would return 0/EOF when closed.
        // We simulate process exit here by turning off the active flag for the mock.
        session->active = false;
    }
}

void DebugAdapterService::sendDAPMessageToAdapter(DebugSession* session, const std::string& jsonMessage) {
    // Format JSON with Content-Length header and write to session->stdinPipe
}

void DebugAdapterService::emitDebugEvent(const std::string& sessionId, const std::string& eventPayload) {
    std::cout << "[DebugAdapterService] Emitting vrutti_on_debug_event for " << sessionId << ": " << eventPayload << std::endl;
    // e.g. Window::emit("vrutti_on_debug_event", eventPayload)
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new DebugAdapterService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
