#include "PluginLoader.h"
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace vrutti::core::plugins {

    PluginLoader::PluginLoader() {}

    PluginLoader::~PluginLoader() {
        std::vector<std::string> names;
        for (const auto& pair : m_loadedPlugins) {
            names.push_back(pair.first);
        }
        for (const auto& name : names) {
            unloadPlugin(name);
        }
    }

    IPlugin* PluginLoader::loadPlugin(const std::string& path) {
        void* handle = nullptr;

#ifdef _WIN32
        handle = LoadLibraryA(path.c_str());
#else
        handle = dlopen(path.c_str(), RTLD_LAZY);
#endif

        if (!handle) {
            std::cerr << "[PluginLoader] Failed to load DLL: " << path << std::endl;
            return nullptr;
        }

        CreatePluginFunc createFunc = nullptr;
        DestroyPluginFunc destroyFunc = nullptr;

#ifdef _WIN32
        createFunc = reinterpret_cast<CreatePluginFunc>(GetProcAddress(static_cast<HMODULE>(handle), "CreatePlugin"));
        destroyFunc = reinterpret_cast<DestroyPluginFunc>(GetProcAddress(static_cast<HMODULE>(handle), "DestroyPlugin"));
#else
        createFunc = reinterpret_cast<CreatePluginFunc>(dlsym(handle, "CreatePlugin"));
        destroyFunc = reinterpret_cast<DestroyPluginFunc>(dlsym(handle, "DestroyPlugin"));
#endif

        if (!createFunc || !destroyFunc) {
            std::cerr << "[PluginLoader] Failed to find entry points in DLL: " << path << std::endl;
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        IPlugin* plugin = createFunc();
        if (!plugin) {
            std::cerr << "[PluginLoader] CreatePlugin returned null for: " << path << std::endl;
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        if (!plugin->initialize()) {
            std::cerr << "[PluginLoader] Plugin failed to initialize: " << plugin->getName() << std::endl;
            destroyFunc(plugin);
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        std::string name = plugin->getName();
        m_loadedPlugins[name] = { handle, plugin, destroyFunc };
        
        return plugin;
    }

    void PluginLoader::unloadPlugin(const std::string& name) {
        auto it = m_loadedPlugins.find(name);
        if (it != m_loadedPlugins.end()) {
            it->second.instance->shutdown();
            it->second.destroyFunc(it->second.instance);

#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(it->second.osHandle));
#else
            dlclose(it->second.osHandle);
#endif

            m_loadedPlugins.erase(it);
        }
    }

    IPlugin* PluginLoader::getPlugin(const std::string& name) const {
        auto it = m_loadedPlugins.find(name);
        if (it != m_loadedPlugins.end()) {
            return it->second.instance;
        }
        return nullptr;
    }

} // namespace vrutti::core::plugins
