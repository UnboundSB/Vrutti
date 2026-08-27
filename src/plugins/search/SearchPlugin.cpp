#include "SearchPlugin.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <cstdio>
#include <memory>
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
                        json += vrutti::core::utils::JsonSerializer::escapeString(files[i]);
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
            
            std::string json = "{";
            
            json += "\"files\":[";
            for (size_t i = 0; i < results.fileMatches.size(); ++i) {
                if (i > 0) json += ",";
                json += vrutti::core::utils::JsonSerializer::escapeString(results.fileMatches[i]);
            }
            json += "],";
            
            json += "\"folders\":[";
            for (size_t i = 0; i < results.folderMatches.size(); ++i) {
                if (i > 0) json += ",";
                json += vrutti::core::utils::JsonSerializer::escapeString(results.folderMatches[i]);
            }
            json += "],";
            
            json += "\"words\":[";
            for (size_t i = 0; i < results.wordMatches.size(); ++i) {
                if (i > 0) json += ",";
                json += "{";
                json += "\"file\":" + vrutti::core::utils::JsonSerializer::escapeString(results.wordMatches[i].file) + ",";
                json += "\"line\":" + std::to_string(results.wordMatches[i].line) + ",";
                json += "\"text\":" + vrutti::core::utils::JsonSerializer::escapeString(results.wordMatches[i].text);
                json += "}";
            }
            json += "]";
            
            json += "}";
            return json;
        }
        return "{}";
    }

    SearchResponse SearchPlugin::performSearch(const std::string& query, const std::string& directory, const SearchOptions& options) {
        SearchResponse response;
        std::cout << "[SearchPlugin] Ripgrep Searching for '" << query << "' in " << directory << "...\n";
        
        std::string cmd = "rg --json ";
        if (!options.matchCase) cmd += "-i ";
        if (options.wholeWord) cmd += "-w ";
        if (!options.useRegex) cmd += "-F ";
        
        std::string tempPatternFile = (std::filesystem::temp_directory_path() / "rg_pattern.txt").string();
        std::ofstream pFile(tempPatternFile);
        pFile << query;
        pFile.close();
        
        cmd += "-f \"" + tempPatternFile + "\" ";
        cmd += "\"" + directory + "\"";

#ifdef _WIN32
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) {
            std::cerr << "[SearchPlugin] Failed to run ripgrep.\n";
            return response;
        }

        char buffer[4096];
        std::string resultLine;
        std::vector<std::string> filesToReplace;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            resultLine += buffer;
            if (!resultLine.empty() && resultLine.back() == '\n') {
                auto node = vrutti::core::utils::JsonParser::parse(resultLine);
                if (node && node->type == vrutti::core::utils::JsonNode::Type::Object) {
                    auto typeNode = node->get("type");
                    if (typeNode && typeNode->type == vrutti::core::utils::JsonNode::Type::String && typeNode->stringValue == "match") {
                        auto dataNode = node->get("data");
                        if (dataNode) {
                            auto pathNode = dataNode->get("path");
                            std::string filePath = "";
                            if (pathNode) {
                                auto textNode = pathNode->get("text");
                                if (textNode) filePath = vrutti::core::utils::JsonParser::unescapeString(textNode->stringValue);
                            }
                            
                            int lineNum = 1;
                            auto lineNode = dataNode->get("line_number");
                            if (lineNode && lineNode->type == vrutti::core::utils::JsonNode::Type::Number) {
                                lineNum = (int)lineNode->numberValue;
                            }
                            
                            std::string lineText = "";
                            auto linesNode = dataNode->get("lines");
                            if (linesNode) {
                                auto textNode = linesNode->get("text");
                                if (textNode) {
                                    lineText = vrutti::core::utils::JsonParser::unescapeString(textNode->stringValue);
                                    if (!lineText.empty() && lineText.back() == '\n') lineText.pop_back();
                                    if (!lineText.empty() && lineText.back() == '\r') lineText.pop_back();
                                }
                            }
                            
                            if (!filePath.empty()) {
                                response.wordMatches.push_back({filePath, lineNum, lineText});
                                if (options.isReplace && std::find(filesToReplace.begin(), filesToReplace.end(), filePath) == filesToReplace.end()) {
                                    filesToReplace.push_back(filePath);
                                }
                            }
                        }
                    }
                }
                resultLine.clear();
            }
        }
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        std::filesystem::remove(tempPatternFile);

        if (options.isReplace && !filesToReplace.empty()) {
            std::regex::flag_type regexFlags = std::regex::ECMAScript;
            if (!options.matchCase) regexFlags |= std::regex::icase;
            std::regex re;
            std::string searchPattern = query;
            
            bool useRegexEngine = options.useRegex || options.wholeWord;
            bool caseInsensitiveSubstring = !options.useRegex && !options.wholeWord && !options.matchCase;
            
            if (!options.useRegex && (options.wholeWord || !options.matchCase)) {
                std::string escaped;
                for (char c : searchPattern) {
                    if (c == '^' || c == '$' || c == '\\' || c == '.' || c == '*' || c == '+' || c == '?' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '|') escaped += '\\';
                    escaped += c;
                }
                searchPattern = escaped;
            }
            if (options.wholeWord && !options.useRegex) searchPattern = "\\b" + searchPattern + "\\b";
            if (useRegexEngine) re = std::regex(searchPattern, regexFlags);
            
            std::string lowerQuery = query;
            if (caseInsensitiveSubstring) {
                for (char& c : lowerQuery) c = std::tolower(static_cast<unsigned char>(c));
            }

            for (const auto& file : filesToReplace) {
                std::ifstream in(file);
                if (!in.is_open()) continue;
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                in.close();

                std::istringstream stream(content);
                std::string line;
                std::vector<std::string> newLines;
                bool modified = false;

                while (std::getline(stream, line)) {
                    if (useRegexEngine) {
                        if (std::regex_search(line, re)) {
                            line = std::regex_replace(line, re, options.replaceString);
                            modified = true;
                        }
                    } else if (caseInsensitiveSubstring) {
                        std::string lowerLine = line;
                        for (char& c : lowerLine) c = std::tolower(static_cast<unsigned char>(c));
                        size_t pos = 0;
                        while ((pos = lowerLine.find(lowerQuery, pos)) != std::string::npos) {
                            line.replace(pos, lowerQuery.length(), options.replaceString);
                            lowerLine.replace(pos, lowerQuery.length(), options.replaceString);
                            for (size_t k = pos; k < pos + options.replaceString.length(); ++k) {
                                lowerLine[k] = std::tolower(static_cast<unsigned char>(lowerLine[k]));
                            }
                            pos += options.replaceString.length();
                            modified = true;
                        }
                    } else {
                        size_t pos = 0;
                        while ((pos = line.find(query, pos)) != std::string::npos) {
                            line.replace(pos, query.length(), options.replaceString);
                            pos += options.replaceString.length();
                            modified = true;
                        }
                    }
                    newLines.push_back(line);
                }

                if (modified) {
                    std::ofstream out(file);
                    for (size_t i = 0; i < newLines.size(); ++i) {
                        out << newLines[i];
                        if (i < newLines.size() - 1) out << "\n";
                    }
                }
            }
        }

        return response;
    }

    std::vector<std::string> SearchPlugin::findFiles(const std::string& directory) {
        std::vector<std::string> files;
        std::cout << "[SearchPlugin] Ripgrep Finding files in " << directory << "...\n";
        
        std::string cmd = "rg --files \"" + directory + "\"";
#ifdef _WIN32
        FILE* pipe = _popen(cmd.c_str(), "r");
#else
        FILE* pipe = popen(cmd.c_str(), "r");
#endif
        if (!pipe) return files;
        
        char buffer[4096];
        std::string resultLine;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            resultLine += buffer;
            if (!resultLine.empty() && resultLine.back() == '\n') {
                resultLine.pop_back(); // remove \n
                if (!resultLine.empty() && resultLine.back() == '\r') resultLine.pop_back(); // remove \r
                if (!resultLine.empty()) {
                    files.push_back(resultLine);
                }
                resultLine.clear();
            }
        }
#ifdef _WIN32
        _pclose(pipe);
#else
        pclose(pipe);
#endif
        return files;
    }

} // namespace vrutti::plugins::search
