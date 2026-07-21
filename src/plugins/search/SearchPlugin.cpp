#include "SearchPlugin.h"
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
    PLUGIN_EXPORT vrutti::core::plugins::IPlugin* CreatePlugin() {
        return new vrutti::plugins::search::SearchPlugin();
    }

    PLUGIN_EXPORT void DestroyPlugin(vrutti::core::plugins::IPlugin* plugin) {
        delete plugin;
    }
}

namespace vrutti::plugins::search {

    SearchPlugin::SearchPlugin() {
        std::cout << "[SearchPlugin] Constructor called.\n";
    }

    SearchPlugin::~SearchPlugin() {
        std::cout << "[SearchPlugin] Destructor called.\n";
    }

    bool SearchPlugin::initialize() {
        std::cout << "[SearchPlugin] Initialized successfully.\n";
        return true;
    }

    void SearchPlugin::shutdown() {
        std::cout << "[SearchPlugin] Shutting down.\n";
    }

    const char* SearchPlugin::getName() const {
        return "Search";
    }

    std::vector<std::string> SearchPlugin::performSearch(const std::string& query, const std::string& directory) {
        std::vector<std::string> results;
        std::cout << "[SearchPlugin] Searching for '" << query << "' in " << directory << "...\n";
        results.push_back("Found: " + query + " at fake_file.txt:10");
        return results;
    }

} // namespace vrutti::plugins::search
