#pragma once

#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

namespace vrutti::core::plugins {

    class ExtensionLifecycleManager : public IPlugin {
    public:
        ExtensionLifecycleManager() = default;
        ~ExtensionLifecycleManager() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct ExtensionMetadata {
            std::string id;
            std::vector<std::string> activationEvents;
            bool isActivated;
        };

        std::mutex m_mutex;
        std::unordered_map<std::string, std::shared_ptr<ExtensionMetadata>> m_extensions;

        std::string handleRegisterExtension(const std::string& payload);
        std::string handleTriggerActivationEvent(const std::string& payload);
        
        void activateExtension(std::shared_ptr<ExtensionMetadata> ext, const std::string& reason);
    };

} // namespace vrutti::core::plugins
