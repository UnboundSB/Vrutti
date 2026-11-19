#pragma once

#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

namespace vrutti::core::plugins {

    class LanguageFeatureService : public IPlugin {
    public:
        LanguageFeatureService() = default;
        ~LanguageFeatureService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct ProviderMetadata {
            int handle;
            std::string type; // "hover", "completion", "definition", etc.
            // Simplified document selector
            std::vector<std::string> languageIds;
        };

        std::mutex m_mutex;
        std::unordered_map<int, std::shared_ptr<ProviderMetadata>> m_providers;

        std::string handleRegisterProvider(const std::string& payload);
        std::string handleUnregisterProvider(const std::string& payload);
        std::string handleRequestCompletions(const std::string& payload);
        std::string handleRequestHover(const std::string& payload);
    };

} // namespace vrutti::core::plugins
