#include "SearchPlugin.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
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
            
            // IPC routing fix: Window.cpp hardcodes executeCommand("search"), so we check the payload for "command"
            if (req && req->type == vrutti::core::utils::JsonNode::Type::Object) {
                auto cmdNode = req->get("command");
                if (cmdNode && cmdNode->type == vrutti::core::utils::JsonNode::Type::String && cmdNode->stringValue == "find_files") {
                    std::string dir = "";
                    auto dirNode = req->get("directory");
                    if (dirNode && dirNode->type == vrutti::core::utils::JsonNode::Type::String) {
                        dir = vrutti::core::utils::JsonParser::unescapeString(dirNode->stringValue);
                    }
                    if (dir.empty()) return "[]";
                    
                    auto files = findFiles(dir);
                    std::string json = "[";
                    for (size_t i = 0; i < files.size(); ++i) {
                        if (i > 0) json += ",";
                        json += "\"" + vrutti::core::utils::JsonSerializer::escapeString(files[i]) + "\"";
                    }
                    json += "]";
                    return json;
                }
            }

            std::string query = "";
            std::string dir = "";
            SearchOptions options;
            
            if (req && req->type == vrutti::core::utils::JsonNode::Type::Object) {
                auto queryNode = req->get("query");
                if (queryNode && queryNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    query = vrutti::core::utils::JsonParser::unescapeString(queryNode->stringValue);
                }
                auto dirNode = req->get("directory");
                if (dirNode && dirNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    dir = vrutti::core::utils::JsonParser::unescapeString(dirNode->stringValue);
                }
                
                auto matchCaseNode = req->get("matchCase");
                if (matchCaseNode && matchCaseNode->type == vrutti::core::utils::JsonNode::Type::Boolean) {
                    options.matchCase = matchCaseNode->boolValue;
                }
                auto wholeWordNode = req->get("wholeWord");
                if (wholeWordNode && wholeWordNode->type == vrutti::core::utils::JsonNode::Type::Boolean) {
                    options.wholeWord = wholeWordNode->boolValue;
                }
                auto useRegexNode = req->get("useRegex");
                if (useRegexNode && useRegexNode->type == vrutti::core::utils::JsonNode::Type::Boolean) {
                    options.useRegex = useRegexNode->boolValue;
                }
                auto isReplaceNode = req->get("isReplace");
                if (isReplaceNode && isReplaceNode->type == vrutti::core::utils::JsonNode::Type::Boolean) {
                    options.isReplace = isReplaceNode->boolValue;
                }
                auto replaceStringNode = req->get("replaceString");
                if (replaceStringNode && replaceStringNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    options.replaceString = vrutti::core::utils::JsonParser::unescapeString(replaceStringNode->stringValue);
                }
            }
            
            if (query.empty() || dir.empty()) return "[]";
            
            auto results = performSearch(query, dir, options);
            
            std::string json = "[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) json += ",";
                json += "{";
                json += "\"file\":\"" + vrutti::core::utils::JsonSerializer::escapeString(results[i].file) + "\",";
                json += "\"line\":" + std::to_string(results[i].line) + ",";
                json += "\"text\":\"" + vrutti::core::utils::JsonSerializer::escapeString(results[i].text) + "\"";
                json += "}";
            }
            json += "]";
            return json;
        }
        return "{}";
    }

    std::vector<SearchResult> SearchPlugin::performSearch(const std::string& query, const std::string& directory, const SearchOptions& options) {
        std::vector<SearchResult> results;
        std::cout << "[SearchPlugin] Searching for '" << query << "' in " << directory << "...\n";
        
        std::string searchPattern = query;
        if (!options.useRegex && (options.wholeWord || !options.matchCase)) {
            std::string escaped;
            for (char c : searchPattern) {
                if (c == '^' || c == '$' || c == '\\' || c == '.' || c == '*' || c == '+' || c == '?' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '|') {
                    escaped += '\\';
                }
                escaped += c;
            }
            searchPattern = escaped;
        }

        if (options.wholeWord && !options.useRegex) {
            searchPattern = "\\b" + searchPattern + "\\b";
        }
        
        std::regex::flag_type regexFlags = std::regex::ECMAScript;
        if (!options.matchCase) {
            regexFlags |= std::regex::icase;
        }
        
        std::regex re;
        bool useRegexEngine = options.useRegex || options.wholeWord || !options.matchCase;
        if (useRegexEngine) {
            try {
                re = std::regex(searchPattern, regexFlags);
            } catch (const std::regex_error& e) {
                std::cerr << "[SearchPlugin] Invalid regex pattern: " << e.what() << "\n";
                return results;
            }
        }
        
        try {
            auto it = std::filesystem::recursive_directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied);
            auto end = std::filesystem::recursive_directory_iterator();
            for (; it != end; ++it) {
                const auto& entry = *it;
                
                if (entry.is_directory()) {
                    std::string name = entry.path().filename().string();
                    if (name == ".git" || name == "build" || name == "build-linux" || name == "node_modules") {
                        it.disable_recursion_pending();
                    }
                    continue;
                }
                
                if (!entry.is_regular_file()) continue;
                
                std::string path = entry.path().string();
                
                std::ifstream file(path);
                if (!file.is_open()) continue;
                
                std::string line;
                int lineNumber = 1;
                bool fileModified = false;
                std::vector<std::string> newLines;
                
                while (std::getline(file, line)) {
                    bool matched = false;
                    
                    if (useRegexEngine) {
                        if (std::regex_search(line, re)) {
                            matched = true;
                            if (options.isReplace) {
                                line = std::regex_replace(line, re, options.replaceString);
                                fileModified = true;
                            }
                        }
                    } else {
                        // Simple substring search (case sensitive)
                        if (line.find(query) != std::string::npos) {
                            matched = true;
                            if (options.isReplace) {
                                size_t pos = 0;
                                while ((pos = line.find(query, pos)) != std::string::npos) {
                                    line.replace(pos, query.length(), options.replaceString);
                                    pos += options.replaceString.length();
                                }
                                fileModified = true;
                            }
                        }
                    }
                    
                    if (matched) {
                        results.push_back({path, lineNumber, line});
                    }
                    
                    if (options.isReplace) {
                        newLines.push_back(line);
                    }
                    lineNumber++;
                }
                
                file.close();
                
                if (options.isReplace && fileModified) {
                    std::ofstream out(path);
                    for (size_t i = 0; i < newLines.size(); ++i) {
                        out << newLines[i];
                        if (i < newLines.size() - 1) out << "\n";
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[SearchPlugin] Exception: " << e.what() << "\n";
        }
        
        return results;
    }

    std::vector<std::string> SearchPlugin::findFiles(const std::string& directory) {
        std::vector<std::string> files;
        std::cout << "[SearchPlugin] Finding files in " << directory << "...\n";
        try {
            auto it = std::filesystem::recursive_directory_iterator(directory, std::filesystem::directory_options::skip_permission_denied);
            auto end = std::filesystem::recursive_directory_iterator();
            for (; it != end; ++it) {
                const auto& entry = *it;
                
                if (entry.is_directory()) {
                    std::string name = entry.path().filename().string();
                    if (name == ".git" || name == "build" || name == "build-linux" || name == "node_modules") {
                        it.disable_recursion_pending();
                    }
                    continue;
                }
                
                if (!entry.is_regular_file()) continue;
                
                std::string path = entry.path().string();
                files.push_back(path);
            }
        } catch (const std::exception& e) {
            std::cerr << "[SearchPlugin] Exception in findFiles: " << e.what() << "\n";
        }
        return files;
    }

} // namespace vrutti::plugins::search
