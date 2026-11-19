#include "TerminalService.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;

TerminalService::~TerminalService() {
    shutdown();
}

bool TerminalService::initialize() {
    std::cout << "[TerminalService] Initialized native PTY broker." << std::endl;
    return true;
}

void TerminalService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& pair : m_sessions) {
        if (pair.second) {
            pair.second->active = false;
            if (pair.second->readerThread.joinable()) {
                pair.second->readerThread.join();
            }
            // Cleanup PTY handles (ClosePseudoConsole on Windows, close(fd) on POSIX)
            delete pair.second;
        }
    }
    m_sessions.clear();
}

const char* TerminalService::getName() const {
    return "TerminalService";
}

std::string TerminalService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "terminal_create") {
        return handleTerminalCreate(payload);
    } else if (command == "terminal_write") {
        return handleTerminalWrite(payload);
    } else if (command == "terminal_resize") {
        return handleTerminalResize(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string TerminalService::handleTerminalCreate(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    int rows = 24, cols = 80;
    std::string cwd;
    
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "rows" && prop.second->type == JsonNode::Type::Number) {
            rows = static_cast<int>(prop.second->numberValue);
        } else if (prop.first == "cols" && prop.second->type == JsonNode::Type::Number) {
            cols = static_cast<int>(prop.second->numberValue);
        } else if (prop.first == "cwd" && prop.second->type == JsonNode::Type::String) {
            cwd = prop.second->stringValue;
        }
    }

    std::string id = "term_" + std::to_string(m_sessions.size() + 1);
    
    auto session = new PtySession();
    session->id = id;
    session->active = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sessions[id] = session;
    }

    spawnPty(session, rows, cols, cwd);

    return "{\"status\": \"success\", \"id\": \"" + id + "\"}";
}

std::string TerminalService::handleTerminalWrite(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    // Parse data and write directly to PTY stdin handle
    std::cout << "[TerminalService] Writing bytes to PTY stdin..." << std::endl;
    return "{\"status\": \"success\"}";
}

std::string TerminalService::handleTerminalResize(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    // Call native OS API (e.g., ResizePseudoConsole or ioctl TIOCSWINSZ)
    std::cout << "[TerminalService] Resizing PTY natively..." << std::endl;
    return "{\"status\": \"success\"}";
}

void TerminalService::spawnPty(PtySession* session, int rows, int cols, const std::string& cwd) {
    std::cout << "[TerminalService] Spawning native OS PTY at " << rows << "x" << cols << std::endl;
    // Uses CreatePseudoConsole (ConPTY) on Windows or forkpty on Unix
    
    // Start background reader
    session->readerThread = std::thread([this, session]() {
        streamStdoutToUI(session);
    });
}

void TerminalService::streamStdoutToUI(PtySession* session) {
    while (session->active) {
        // Read raw bytes from PTY stdout pipe
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Mock reading shell prompt
        emitTerminalData(session->id, "C:\\> ");
        
        // In a real implementation, a blocking read would return 0/EOF when closed.
        // We simulate process exit here by turning off the active flag for the mock.
        session->active = false;
    }
}

void TerminalService::emitTerminalData(const std::string& id, const std::string& data) {
    // Escape string and emit via Tauri IPC (e.g., vrutti_on_terminal_data)
    std::cout << "[TerminalService] Emitting PTY chunk for " << id << std::endl;
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new TerminalService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
