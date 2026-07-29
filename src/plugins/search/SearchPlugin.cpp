#include "SearchPlugin.h"
#include <iostream>
#include <filesystem>
#include "../../core/utils/Json.h"

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

    std::string SearchPlugin::executeCommand(const std::string& command, const std::string& payload) {
        if (command == "search") {
            auto req = vrutti::core::utils::JsonParser::parse(payload);
            std::string query = "";
            std::string dir = "";
            
            if (req && req->type == vrutti::core::utils::JsonNode::Type::Object) {
                auto queryNode = req->get("query");
                if (queryNode && queryNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    query = vrutti::core::utils::JsonParser::unescapeString(queryNode->stringValue);
                }
                auto dirNode = req->get("directory");
                if (dirNode && dirNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    dir = vrutti::core::utils::JsonParser::unescapeString(dirNode->stringValue);
                }
            }
            
            if (query.empty() || dir.empty()) return "[]";
            
            auto results = performSearch(query, dir);
            
            std::string json = "[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) json += ",";
                json += vrutti::core::utils::JsonSerializer::escapeString(results[i]);
            }
            json += "]";
            return json;
        }
        return "{}";
    }

    std::vector<std::string> SearchPlugin::performSearch(const std::string& query, const std::string& directory) {
        std::vector<std::string> results;
        std::cout << "[SearchPlugin] Searching for '" << query << "' in " << directory << "...\n";
        results.push_back("Found: " + query + " at fake_file.txt:10");
        return results;
    }

} // namespace vrutti::plugins::search
