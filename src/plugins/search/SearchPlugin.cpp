#include "SearchPlugin.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <cstdio>
#include <memory>
#include <new>
#include "../../core/utils/Json.h"
#include "../../core/utils/Paths.h"
#include "../../core/utils/ProcessRunner.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#include <cstdlib>
#include <unistd.h>
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {
    PLUGIN_EXPORT vrutti::core::plugins::IPlugin* createPlugin() {
        return new vrutti::plugins::search::SearchPlugin();
    }

    PLUGIN_EXPORT void destroyPlugin(vrutti::core::plugins::IPlugin* plugin) {
        delete plugin;
    }
}

namespace vrutti::plugins::search {

namespace {

    // The most text that is read from the search program.
    //
    // A folder of the user may hold a lot of files, and the program says
    // a line for every match. The text is kept inside of its limit, so
    // that a search cannot use all of the memory of the editor.
    constexpr size_t kMaxSearchOutputBytes = 32u * 1024u * 1024u;

    // The most matches that are kept.
    constexpr size_t kMaxWordMatches = 20000;

    // The most files that are named by a search.
    constexpr size_t kMaxFileMatches = 50000;

    // The biggest file that is changed by a replace.
    //
    // The file is read as a whole to be changed, so a file that is too big
    // is left alone.
    constexpr uint64_t kMaxReplaceFileBytes = 32ull * 1024ull * 1024ull;

    // The most files that are changed by one replace.
    constexpr size_t kMaxReplaceFiles = 2000;

    // The longest pattern of a regular expression.
    //
    // A pattern that is very long is a sign of a mistake, and the engine
    // of the regular expressions needs a long time for a pattern that is
    // built to make it go round in circles.
    constexpr size_t kMaxRegexPatternLength = 1000;

    // The longest line that the engine of the regular expressions is given.
    constexpr size_t kMaxRegexLineLength = 64 * 1024;

    // How long the search program may run.
    constexpr unsigned int kSearchTimeoutMs = 120000;

    // The path of the ripgrep binary inside the installation.
    //
    // The bare name is not enough, because the installation directory is
    // not on the PATH of the process, and the path may hold spaces. When
    // the binary cannot be found inside the installation, the name is
    // used, so a system wide installation still works.
    std::string ripgrepCommand() {
        namespace fs = std::filesystem;

#ifdef _WIN32
        const char* binary = "rg.exe";
#else
        const char* binary = "rg";
#endif
        const fs::path releaseLayout = vrutti::core::utils::appResourcePath(binary);
        const fs::path developmentLayout = vrutti::core::utils::appResourcePath(
            (fs::path("bin") / fs::path(binary)).string());

        std::error_code error;
        if (fs::exists(releaseLayout, error)) {
            return releaseLayout.string();
        }
        if (fs::exists(developmentLayout, error)) {
            return developmentLayout.string();
        }
        return binary;
    }

    // Makes a file for the pattern of the search.
    //
    // The name of the file holds the number of the process and a counter,
    // so that two searches at the same time do not write into the same
    // file. The file is made with the flag that says that it may not
    // exist: a file that is already there is not taken over, so a name
    // that was prepared by somebody else is not written into.
    //
    // The file holds the words that the user is looking for, so it is
    // made for the user alone and is taken away when the search is done.
    std::string makePatternFile(const std::string& pattern) {
        namespace fs = std::filesystem;

        static int counter = 0;
        std::error_code error;
        const fs::path directory = fs::temp_directory_path();

#ifdef _WIN32
        HANDLE file = INVALID_HANDLE_VALUE;
        fs::path target;
        for (int attempt = 0; attempt < 16 && file == INVALID_HANDLE_VALUE; ++attempt) {
            const std::string name = "vrutti-search-" +
                                     std::to_string(GetCurrentProcessId()) + "-" +
                                     std::to_string(counter++) + ".txt";
            target = directory / name;
            // CREATE_NEW: the file may not be there already.
            file = CreateFileA(target.string().c_str(), GENERIC_WRITE,
                               0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_TEMPORARY, nullptr);
        }
        if (file == INVALID_HANDLE_VALUE) return "";

        const std::string text = pattern + "\n";
        DWORD written = 0;
        const BOOL ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()),
                                  &written, nullptr);
        CloseHandle(file);
        if (!ok || written != text.size()) {
            fs::remove(target, error);
            return "";
        }
        return target.string();
#else
        // The file is made with the flag that says that it may not exist,
        // and it belongs to the user alone.
        std::string name = directory.string() + "/vrutti-search-XXXXXX";
        std::vector<char> buffer(name.begin(), name.end());
        buffer.push_back('\0');
        const int fd = mkstemp(buffer.data());
        if (fd < 0) return "";
        const std::string text = pattern + "\n";
        const ssize_t written = write(fd, text.data(), text.size());
        close(fd);
        if (written != static_cast<ssize_t>(text.size())) {
            fs::remove(std::string(buffer.data()), error);
            return "";
        }
        return std::string(buffer.data());
#endif
    }

} // namespace

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
                        dir = dirNode->stringValue;
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
                    query = queryNode->stringValue;
                }
                auto dirNode = req->get("directory");
                if (dirNode && dirNode->type == vrutti::core::utils::JsonNode::Type::String) {
                    dir = dirNode->stringValue;
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
                    options.replaceString = replaceStringNode->stringValue;
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

        // The pattern is written into a file of its own, so that a pattern
        // with a quote or a newline in it cannot do anything but be a
        // pattern.
        const std::string tempPatternFile = makePatternFile(query);
        if (tempPatternFile.empty()) {
            std::cerr << "[SearchPlugin] The pattern of the search could not be written.\n";
            return response;
        }

        // The arguments are the arguments of the program. The program is
        // started without a shell, so a folder whose name holds a quote or
        // a semicolon cannot become a command of its own.
        std::vector<std::string> arguments;
        arguments.push_back("--json");
        if (!options.matchCase) arguments.push_back("-i");
        if (options.wholeWord) arguments.push_back("-w");
        if (!options.useRegex) arguments.push_back("-F");
        arguments.push_back("-f");
        arguments.push_back(tempPatternFile);
        arguments.push_back("--");
        arguments.push_back(directory);

        const auto run = vrutti::core::utils::runProcess(
            ripgrepCommand(), arguments, kMaxSearchOutputBytes, kSearchTimeoutMs);

        std::error_code removeError;
        std::filesystem::remove(tempPatternFile, removeError);

        if (!run.started) {
            std::cerr << "[SearchPlugin] The search program could not be started: "
                      << run.error << "\n";
            return response;
        }
        if (run.truncated) {
            std::cerr << "[SearchPlugin] The search was cut short: there was more to read.\n";
        }

        std::string resultLine;
        std::vector<std::string> filesToReplace;
        // The text of the program is read line by line.
        size_t position = 0;
        while (position < run.output.size()) {
            const size_t newline = run.output.find('\n', position);
            const size_t end = (newline == std::string::npos) ? run.output.size() : newline;
            resultLine = run.output.substr(position, end - position);
            position = (newline == std::string::npos) ? run.output.size() : newline + 1;

            if (resultLine.empty()) continue;

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
                            if (textNode) filePath = textNode->stringValue;
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
                                lineText = textNode->stringValue;
                                if (!lineText.empty() && lineText.back() == '\n') lineText.pop_back();
                                if (!lineText.empty() && lineText.back() == '\r') lineText.pop_back();
                            }
                        }
                        
                        if (!filePath.empty() && response.wordMatches.size() < kMaxWordMatches) {
                            response.wordMatches.push_back({filePath, lineNum, lineText});
                            if (options.isReplace && filesToReplace.size() < kMaxReplaceFiles &&
                                std::find(filesToReplace.begin(), filesToReplace.end(), filePath) == filesToReplace.end()) {
                                filesToReplace.push_back(filePath);
                            }
                        }
                    }
                }
            }
        }
        (void)0;

        if (options.isReplace && !filesToReplace.empty()) {
            replaceInFiles(filesToReplace, query, options);
        }

        return response;
    }

    std::vector<std::string> SearchPlugin::findFiles(const std::string& directory) {
        std::vector<std::string> files;
        std::cout << "[SearchPlugin] Ripgrep Finding files in " << directory << "...\n";

        // The program is started without a shell: the name of the folder is
        // an argument of the program and never a part of a command.
        std::vector<std::string> arguments;
        arguments.push_back("--files");
        arguments.push_back("--");
        arguments.push_back(directory);

        const auto run = vrutti::core::utils::runProcess(
            ripgrepCommand(), arguments, kMaxSearchOutputBytes, kSearchTimeoutMs);
        if (!run.started) {
            std::cerr << "[SearchPlugin] The search program could not be started: "
                      << run.error << "\n";
            return files;
        }

        size_t position = 0;
        while (position < run.output.size() && files.size() < kMaxFileMatches) {
            const size_t newline = run.output.find('\n', position);
            const size_t end = (newline == std::string::npos) ? run.output.size() : newline;
            std::string line = run.output.substr(position, end - position);
            position = (newline == std::string::npos) ? run.output.size() : newline + 1;

            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty()) files.push_back(line);
        }
        return files;
    }

    // Changes the words that were found in the files.
    //
    // The file is read as a whole to be changed, so a file that is too big
    // is left alone. The engine of the regular expressions may refuse a
    // pattern, and it may need a long time for a pattern that is built to
    // make it go round in circles: a pattern that is too long is refused,
    // and a line that is too long is left alone.
    //
    // Nothing that goes wrong here may take the editor with it, so every
    // failure is caught and the file is left as it was.
    void SearchPlugin::replaceInFiles(const std::vector<std::string>& files,
                                      const std::string& query,
                                      const SearchOptions& options) {
        namespace fs = std::filesystem;

        const bool useRegexEngine = options.useRegex || options.wholeWord;
        const bool caseInsensitiveSubstring =
            !options.useRegex && !options.wholeWord && !options.matchCase;

        std::regex::flag_type regexFlags = std::regex::ECMAScript;
        if (!options.matchCase) regexFlags |= std::regex::icase;

        std::string searchPattern = query;
        if (!options.useRegex && (options.wholeWord || !options.matchCase)) {
            std::string escaped;
            for (char c : searchPattern) {
                if (c == '^' || c == '$' || c == '\\' || c == '.' || c == '*' || c == '+' ||
                    c == '?' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' ||
                    c == '}' || c == '|') escaped += '\\';
                escaped += c;
            }
            searchPattern = escaped;
        }
        if (options.wholeWord && !options.useRegex) searchPattern = "\\b" + searchPattern + "\\b";

        std::regex re;
        if (useRegexEngine) {
            if (searchPattern.size() > kMaxRegexPatternLength) {
                std::cerr << "[SearchPlugin] The pattern is too long and nothing was changed.\n";
                return;
            }
            try {
                re = std::regex(searchPattern, regexFlags);
            } catch (const std::regex_error& error) {
                std::cerr << "[SearchPlugin] The pattern is not a pattern and nothing was changed: "
                          << error.what() << "\n";
                return;
            }
        }

        std::string lowerQuery = query;
        if (caseInsensitiveSubstring) {
            for (char& c : lowerQuery) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }

        for (const auto& file : files) {
            try {
                std::error_code sizeError;
                const uintmax_t size = fs::file_size(file, sizeError);
                if (sizeError) continue;
                if (size > kMaxReplaceFileBytes) {
                    std::cerr << "[SearchPlugin] The file '" << file
                              << "' is too big and was left alone.\n";
                    continue;
                }

                std::ifstream in(file, std::ios::binary);
                if (!in.is_open()) continue;
                std::string content((std::istreambuf_iterator<char>(in)),
                                     std::istreambuf_iterator<char>());
                in.close();

                std::istringstream stream(content);
                std::string line;
                std::vector<std::string> newLines;
                bool modified = false;

                while (std::getline(stream, line)) {
                    if (useRegexEngine) {
                        if (line.size() > kMaxRegexLineLength) {
                            // A line that is very long is left as it is: the
                            // engine of the regular expressions would need
                            // a long time for it.
                            newLines.push_back(line);
                            continue;
                        }
                        if (std::regex_search(line, re)) {
                            line = std::regex_replace(line, re, options.replaceString);
                            modified = true;
                        }
                    } else if (caseInsensitiveSubstring) {
                        std::string lowerLine = line;
                        for (char& c : lowerLine) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                        size_t pos = 0;
                        while ((pos = lowerLine.find(lowerQuery, pos)) != std::string::npos) {
                            line.replace(pos, lowerQuery.length(), options.replaceString);
                            lowerLine.replace(pos, lowerQuery.length(), options.replaceString);
                            for (size_t k = pos; k < pos + options.replaceString.length(); ++k) {
                                lowerLine[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerLine[k])));
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
                    // The new text is written into a file of its own, and
                    // that file takes the place of the old one. A file
                    // that is written in place may be read while it is
                    // written, and a program that stops in the middle
                    // leaves a file that is half written.
                    const fs::path target(file);
                    const fs::path temporary = fs::path(file + ".vrutti-replace");
                    {
                        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                        if (!out.is_open()) continue;
                        for (size_t i = 0; i < newLines.size(); ++i) {
                            out << newLines[i];
                            if (i < newLines.size() - 1) out << "\n";
                        }
                        out.flush();
                        if (!out.good()) {
                            out.close();
                            std::error_code removeError;
                            fs::remove(temporary, removeError);
                            continue;
                        }
                    }
                    std::error_code renameError;
                    fs::rename(temporary, target, renameError);
                    if (renameError) {
                        // The file that is there is kept: a broken file is
                        // worse than an old one.
                        std::error_code removeError;
                        fs::remove(temporary, removeError);
                    }
                }
            } catch (const std::exception& error) {
                std::cerr << "[SearchPlugin] The file '" << file
                          << "' was left alone: " << error.what() << "\n";
            } catch (...) {
                std::cerr << "[SearchPlugin] The file '" << file << "' was left alone.\n";
            }
        }
    }

} // namespace vrutti::plugins::search
