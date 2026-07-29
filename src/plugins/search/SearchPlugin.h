#pragma once

#include "../../core/plugins/IPlugin.h"
#include <string>
#include <vector>

namespace vrutti::plugins::search {

    class SearchPlugin : public vrutti::core::plugins::IPlugin {
    public:
        SearchPlugin();
        ~SearchPlugin() override;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;

        std::string executeCommand(const std::string& command, const std::string& payload) override;

        std::vector<std::string> performSearch(const std::string& query, const std::string& directory);
    };

}
