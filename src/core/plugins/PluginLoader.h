#pragma once

#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace vrutti::core::plugins {

    class PluginLoader {
    public:
        PluginLoader();
        ~PluginLoader();

        IPlugin* loadPlugin(const std::string& path);
        void unloadPlugin(const std::string& name);
        IPlugin* getPlugin(const std::string& name) const;

    private:
        struct PluginHandle {
            void* osHandle;
            IPlugin* instance;
            DestroyPluginFunc destroyFunc;
        };

        std::unordered_map<std::string, PluginHandle> m_loadedPlugins;
    };

}
