#pragma once

#include "IPlugin.h"
#include <string>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <atomic>

namespace vrutti::core::plugins {

    class TerminalService : public IPlugin {
    public:
        TerminalService() = default;
        ~TerminalService() override;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct PtySession {
            std::string id;
            // Native PTY handles (e.g., HPCON on Windows, int fd on POSIX)
            void* ptyHandle = nullptr;
            void* processHandle = nullptr;
            void* stdinPipe = nullptr;
            void* stdoutPipe = nullptr;
            
            std::atomic<bool> active{false};
            std::thread readerThread;
        };

        std::mutex m_mutex;
        std::unordered_map<std::string, PtySession*> m_sessions;

        // IPC Handlers
        std::string handleTerminalCreate(const std::string& payload);
        std::string handleTerminalWrite(const std::string& payload);
        std::string handleTerminalResize(const std::string& payload);

        // Native PTY Management
        void spawnPty(PtySession* session, int rows, int cols, const std::string& cwd);
        void streamStdoutToUI(PtySession* session);
        void emitTerminalData(const std::string& id, const std::string& data);
    };

} // namespace vrutti::core::plugins
