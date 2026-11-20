#include "PluginLoader.h"
#include "../utils/ErrorCodes.h"
#include <iostream>
#include <mutex>

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
        {
            std::lock_guard<std::mutex> lock(m_loaderMutex);
            for (const auto& pair : m_loadedPlugins) {
                names.push_back(pair.first);
            }
        }
        for (const auto& name : names) {
            unloadPlugin(name);
        }
    }

    IPlugin* PluginLoader::loadPlugin(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_loaderMutex);

        void* handle = nullptr;

#ifdef _WIN32
        handle = LoadLibraryA(path.c_str());
#else
        handle = dlopen(path.c_str(), RTLD_LAZY);
#endif

        if (!handle) {
            vrutti::core::utils::reportError("PluginLoader",
                vrutti::core::utils::ErrorCode::PluginLoadFailed,
                "could not load " + path);
            return nullptr;
        }

        CreatePluginFunc createFunc = nullptr;
        DestroyPluginFunc destroyFunc = nullptr;

#ifdef _WIN32
        createFunc = reinterpret_cast<CreatePluginFunc>(GetProcAddress(static_cast<HMODULE>(handle), "createPlugin"));
        destroyFunc = reinterpret_cast<DestroyPluginFunc>(GetProcAddress(static_cast<HMODULE>(handle), "destroyPlugin"));
#else
        createFunc = reinterpret_cast<CreatePluginFunc>(dlsym(handle, "createPlugin"));
        destroyFunc = reinterpret_cast<DestroyPluginFunc>(dlsym(handle, "destroyPlugin"));
#endif

        if (!createFunc || !destroyFunc) {
            vrutti::core::utils::reportError("PluginLoader",
                vrutti::core::utils::ErrorCode::PluginLoadFailed,
                "no entry points in " + path);
#ifdef _WIN32
            FreeLibrary(static_cast<HMODULE>(handle));
#else
            dlclose(handle);
#endif
            return nullptr;
        }

        IPlugin* plugin = createFunc();
        if (!plugin) {
            vrutti::core::utils::reportError("PluginLoader",
                vrutti::core::utils::ErrorCode::PluginLoadFailed,
                "createPlugin returned null for " + path);
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
        std::lock_guard<std::mutex> lock(m_loaderMutex);
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
        std::lock_guard<std::mutex> lock(m_loaderMutex);
        auto it = m_loadedPlugins.find(name);
        if (it != m_loadedPlugins.end()) {
            return it->second.instance;
        }
        return nullptr;
    }

} // namespace vrutti::core::plugins
