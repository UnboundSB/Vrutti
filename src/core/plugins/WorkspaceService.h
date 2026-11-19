#pragma once

#include "IPlugin.h"
#include <string>
#include <vector>
#include <mutex>

namespace vrutti::core::plugins {

    class WorkspaceService : public IPlugin {
    public:
        WorkspaceService() = default;
        ~WorkspaceService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct WorkspaceFolder {
            std::string uri;
            std::string name;
        };

        std::mutex m_mutex;
        std::vector<WorkspaceFolder> m_folders;

        std::string handleUpdateWorkspaceFolders(const std::string& payload);
        std::string handleGetWorkspaceFolders(const std::string& payload);
    };

} // namespace vrutti::core::plugins
