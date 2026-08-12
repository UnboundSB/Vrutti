#pragma once

#include "../../core/plugins/IPlugin.h"
#include <string>
#include <vector>

namespace vrutti::plugins::search {

    struct SearchOptions {
        bool matchCase = false;
        bool wholeWord = false;
        bool useRegex = false;
        bool isReplace = false;
        std::string replaceString = "";
    };

    struct SearchResult {
        std::string file;
        int line;
        std::string text;
    };

    struct SearchResponse {
        std::vector<std::string> fileMatches;
        std::vector<std::string> folderMatches;
        std::vector<SearchResult> wordMatches;
    };

    class SearchPlugin : public vrutti::core::plugins::IPlugin {
    public:
        SearchPlugin();
        ~SearchPlugin() override;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;

        std::string executeCommand(const std::string& command, const std::string& payload) override;

        SearchResponse performSearch(const std::string& query, const std::string& directory, const SearchOptions& options);
        std::vector<std::string> findFiles(const std::string& directory);
    };

}
