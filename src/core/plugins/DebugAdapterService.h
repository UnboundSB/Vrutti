#pragma once

#include "IPlugin.h"
#include <string>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <atomic>
#include <functional>

namespace vrutti::core::plugins {

    class DebugAdapterService : public IPlugin {
    public:
        DebugAdapterService() = default;
        ~DebugAdapterService() override;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct DebugSession {
            std::string id;
            std::string type;
            // Native process handles for Windows / POSIX
            void* processHandle = nullptr; 
            void* stdinPipe = nullptr;
            void* stdoutPipe = nullptr;
            
            std::atomic<bool> active{false};
            std::thread readerThread;
        };

        std::mutex m_mutex;
        std::unordered_map<std::string, DebugSession*> m_sessions;

        // IPC Handlers
        std::string handleDebugStart(const std::string& payload);
        std::string handleDebugSetBreakpoints(const std::string& payload);
        std::string handleDebugAction(const std::string& payload);
        std::string handleDebugStop(const std::string& payload);

        // DAP lifecycle and routing
        void spawnAdapterProcess(DebugSession* session, const std::string& adapterCmd, const std::vector<std::string>& args);
        void streamStdoutToUI(DebugSession* session);
        void sendDAPMessageToAdapter(DebugSession* session, const std::string& jsonMessage);
        void emitDebugEvent(const std::string& sessionId, const std::string& eventPayload);
    };

} // namespace vrutti::core::plugins
