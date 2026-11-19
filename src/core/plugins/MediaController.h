#pragma once

#include "IPlugin.h"

namespace vrutti::core::plugins {
    class MediaController : public IPlugin {
    public:
        MediaController() = default;
        ~MediaController() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        std::string currentMediaId;
        std::string currentMediaPath;
    };
}
