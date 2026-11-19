#pragma once

#include "IPlugin.h"

namespace vrutti::core::plugins {
    class ExtensionManager : public IPlugin {
    public:
        ExtensionManager() = default;
        ~ExtensionManager() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;
    };
}
