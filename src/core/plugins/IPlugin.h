#pragma once

#include <string>

namespace vrutti::core::plugins {

    class IPlugin {
    public:
        virtual ~IPlugin() = default;

        virtual bool initialize() = 0;
        virtual void shutdown() = 0;
        
        virtual const char* getName() const = 0;
        
        // Allows generic dynamic invocation of plugin functionality
        virtual std::string executeCommand(const std::string& command, const std::string& payload) { return "{}"; }
    };

}

// C-export function signature for the DLL entry point
extern "C" {
    typedef vrutti::core::plugins::IPlugin* (*CreatePluginFunc)();
    typedef void (*DestroyPluginFunc)(vrutti::core::plugins::IPlugin*);
}
