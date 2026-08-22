#include "SearchPlugin.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include "../../core/utils/Json.h"
#include "../../core/concurrency/ThreadPool.h"

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

#include <cctype>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <algorithm>

namespace vrutti::plugins::search {

    struct CacheEntry {
        std::string path;
        std::string name;
        bool isDirectory;
    };

    static std::vector<CacheEntry> g_cache;
    static std::string g_currentDir = "";
    static std::mutex g_cacheMutex;
    static std::atomic<bool> g_isIndexing = false;

    static void buildIndexSync(std::string dir) {
        g_isIndexing = true;
        std::vector<CacheEntry> localCache;
        try {
            auto it = std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied);
            auto end = std::filesystem::recursive_directory_iterator();
            for (; it != end; ++it) {
                const auto& entry = *it;
                std::string name = entry.path().filename().string();
                if (entry.is_directory()) {
                    if (name == ".git" || name == "build" || name == "build-linux" || name == "node_modules" || name == "dist" || name == "out" || name == ".vscode" || name == ".idea") {
                        it.disable_recursion_pending();
                    }
                    localCache.push_back({entry.path().string(), name, true});
                } else if (entry.is_regular_file()) {
                    localCache.push_back({entry.path().string(), name, false});
                }
            }
        } catch (...) {
        }
        
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            g_cache = std::move(localCache);
            g_currentDir = dir;
        }
        g_isIndexing = false;
        std::cout << "[SearchPlugin] Indexing complete for " << dir << ". Found " << g_cache.size() << " items.\n";
    }

    static void buildIndexAsync(std::string dir) {
        buildIndexSync(dir);
    }

    static bool fuzzyMatch(const std::string& query, const std::string& target, bool matchCase = false) {
        if (query.empty()) return true;
        size_t qIndex = 0;
        for (char c : target) {
            bool matches = matchCase ? (c == query[qIndex]) : (std::tolower(static_cast<unsigned char>(c)) == std::tolower(static_cast<unsigned char>(query[qIndex])));
            if (matches) {
                qIndex++;
                if (qIndex == query.length()) return true;
            }
        }
        return false;
    }

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

            {
                std::lock_guard<std::mutex> lock(g_cacheMutex);
                if (g_currentDir != dir && !g_isIndexing) {
                    g_currentDir = dir;
                    g_cache.clear();
                    std::thread(buildIndexAsync, dir).detach();
                }
            }
            
            auto results = performSearch(query, dir, options);
            
            std::string json = "{";
            
            // Serialize files
            json += "\"files\":[";
            for (size_t i = 0; i < results.fileMatches.size(); ++i) {
                if (i > 0) json += ",";
                json += vrutti::core::utils::JsonSerializer::escapeString(results.fileMatches[i]);
            }
            json += "],";
            
            // Serialize folders
            json += "\"folders\":[";
            for (size_t i = 0; i < results.folderMatches.size(); ++i) {
                if (i > 0) json += ",";
                json += vrutti::core::utils::JsonSerializer::escapeString(results.folderMatches[i]);
            }
            json += "],";
            
            // Serialize words
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
        bool useRegexEngine = options.useRegex || options.wholeWord;
        bool caseInsensitiveSubstring = !options.useRegex && !options.wholeWord && !options.matchCase;
        
        if (useRegexEngine) {
            try {
                re = std::regex(searchPattern, regexFlags);
            } catch (const std::regex_error& e) {
                std::cerr << "[SearchPlugin] Invalid regex pattern: " << e.what() << "\n";
                return response;
            }
        }
        
        std::string lowerQuery = query;
        if (caseInsensitiveSubstring) {
            for (char& c : lowerQuery) c = std::tolower(static_cast<unsigned char>(c));
        }
        
        bool isFuzzy = false; // We don't use fuzzy matching for text search
        
        std::vector<CacheEntry> localCache;
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            if (g_currentDir == directory && !g_cache.empty()) {
                localCache = g_cache;
            } else if (g_currentDir != directory && !g_isIndexing) {
                g_currentDir = directory;
                g_cache.clear();
            }
        }
        
        if (localCache.empty()) {
            buildIndexSync(directory);
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            localCache = g_cache;
        }
        
        try {
            std::atomic<size_t> resultCount = 0;
            std::mutex resultsMutex;
            vrutti::core::concurrency::ThreadPool pool;
            std::vector<std::future<void>> futures;
            
            size_t batchSize = 100;
            for (size_t i = 0; i < localCache.size(); i += batchSize) {
                size_t end = std::min(i + batchSize, localCache.size());
                std::vector<CacheEntry> batch(localCache.begin() + i, localCache.begin() + end);
                
                futures.push_back(pool.enqueue([&, batch]() {
                    std::vector<SearchResult> threadWordMatches;
                    
                    for (const auto& entry : batch) {
                        if (resultCount >= 2000 && !options.isReplace) break;
                        if (entry.isDirectory) continue;
                        
                        std::error_code ec;
                        auto fsize = std::filesystem::file_size(entry.path, ec);
                        if (ec || fsize > 2 * 1024 * 1024 || fsize == 0) continue;
                        
                        std::ifstream file(entry.path, std::ios::binary);
                        if (!file.is_open()) continue;
                        
                        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        file.close();
                        
                        if (content.find('\0') != std::string::npos) continue;
                        
                        bool fileMatched = false;
                        if (useRegexEngine) {
                            fileMatched = std::regex_search(content, re);
                        } else if (caseInsensitiveSubstring) {
                            auto it = std::search(content.begin(), content.end(), lowerQuery.begin(), lowerQuery.end(), 
                                [](char ch1, char ch2) { return std::tolower(static_cast<unsigned char>(ch1)) == ch2; });
                            fileMatched = (it != content.end());
                        } else {
                            fileMatched = (content.find(query) != std::string::npos);
                        }
                        
                        if (fileMatched) {
                            std::istringstream stream(content);
                            std::string line;
                            std::string lowerLine;
                            int lineNumber = 1;
                            bool fileModified = false;
                            std::vector<std::string> newLines;
                            
                            while (std::getline(stream, line)) {
                                bool matched = false;
                                
                                if (useRegexEngine) {
                                    if (std::regex_search(line, re)) {
                                        matched = true;
                                        if (options.isReplace) {
                                            line = std::regex_replace(line, re, options.replaceString);
                                            fileModified = true;
                                        }
                                    }
                                } else if (caseInsensitiveSubstring) {
                                    lowerLine.resize(line.size());
                                    for (size_t k = 0; k < line.size(); ++k) lowerLine[k] = std::tolower(static_cast<unsigned char>(line[k]));
                                    if (lowerLine.find(lowerQuery) != std::string::npos) {
                                        matched = true;
                                        if (options.isReplace) {
                                            size_t pos = 0;
                                            while ((pos = lowerLine.find(lowerQuery, pos)) != std::string::npos) {
                                                line.replace(pos, lowerQuery.length(), options.replaceString);
                                                lowerLine.replace(pos, lowerQuery.length(), options.replaceString);
                                                for (size_t k = pos; k < pos + options.replaceString.length(); ++k) {
                                                    lowerLine[k] = std::tolower(static_cast<unsigned char>(lowerLine[k]));
                                                }
                                                pos += options.replaceString.length();
                                            }
                                            fileModified = true;
                                        }
                                    }
                                } else {
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
                                    std::string snippet = line;
                                    if (snippet.length() > 200) {
                                        size_t qPos = std::string::npos;
                                        if (useRegexEngine) {
                                            std::smatch m;
                                            if (std::regex_search(snippet, m, re)) qPos = m.position();
                                        } else if (caseInsensitiveSubstring) {
                                            std::string snippetLower = snippet;
                                            for (char& c : snippetLower) c = std::tolower(static_cast<unsigned char>(c));
                                            qPos = snippetLower.find(lowerQuery);
                                        } else {
                                            qPos = snippet.find(query);
                                        }
                                        if (qPos != std::string::npos) {
                                            size_t start = (qPos > 50) ? (qPos - 50) : 0;
                                            snippet = (start > 0 ? "..." : "") + snippet.substr(start, 200) + (start + 200 < snippet.length() ? "..." : "");
                                        } else {
                                            snippet = snippet.substr(0, 200) + "...";
                                        }
                                    }
                                    threadWordMatches.push_back({entry.path, lineNumber, snippet});
                                }
                                
                                if (options.isReplace) {
                                    newLines.push_back(line);
                                }
                                lineNumber++;
                            }
                            
                            if (options.isReplace && fileModified) {
                                std::ofstream out(entry.path);
                                for (size_t k = 0; k < newLines.size(); ++k) {
                                    out << newLines[k];
                                    if (k < newLines.size() - 1) out << "\n";
                                }
                            }
                        }
                    }
                    
                    if (!threadWordMatches.empty()) {
                        std::lock_guard<std::mutex> lock(resultsMutex);
                        for (const auto& wm : threadWordMatches) {
                            if (resultCount >= 2000 && !options.isReplace) break;
                            response.wordMatches.push_back(wm);
                            resultCount++;
                        }
                    }
                }));
            }
            
            for (auto& f : futures) {
                f.get();
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[SearchPlugin] Exception: " << e.what() << "\n";
        }
        
        return response;
    }

    std::vector<std::string> SearchPlugin::findFiles(const std::string& directory) {
        std::vector<std::string> files;
        std::cout << "[SearchPlugin] Finding files in " << directory << "...\n";
        
        std::vector<CacheEntry> localCache;
        bool useCache = false;
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            if (g_currentDir == directory && !g_cache.empty()) {
                localCache = g_cache;
                useCache = true;
            } else if (g_currentDir != directory && !g_isIndexing) {
                g_currentDir = directory;
                g_cache.clear();
            }
        }
        
        if (!useCache) {
            buildIndexSync(directory);
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            localCache = g_cache;
        }
        
        for (const auto& entry : localCache) {
            if (!entry.isDirectory) {
                files.push_back(entry.path);
            }
        }
        return files;
    }

} // namespace vrutti::plugins::search
