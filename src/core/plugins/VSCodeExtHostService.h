#pragma once

#include "IPlugin.h"

namespace vrutti::core::plugins {
    class VSCodeExtHostService : public IPlugin {
    public:
        VSCodeExtHostService() = default;
        ~VSCodeExtHostService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;
    };
}
