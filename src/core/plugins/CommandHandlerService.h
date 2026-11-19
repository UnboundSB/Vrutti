#pragma once

#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace vrutti::core::plugins {

    class CommandHandlerService : public IPlugin {
    public:
        CommandHandlerService() = default;
        ~CommandHandlerService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct CommandMetadata {
            std::string id;
            // Additional metadata like title, category can be stored here if passed
        };

        std::mutex m_mutex;
        std::unordered_map<std::string, CommandMetadata> m_commands;

        std::string handleRegisterCommand(const std::string& payload);
        std::string handleUnregisterCommand(const std::string& payload);
        std::string handleGetActiveCommands(const std::string& payload);
        std::string handleExecuteCommand(const std::string& payload);
    };

} // namespace vrutti::core::plugins
