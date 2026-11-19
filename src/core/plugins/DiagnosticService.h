#pragma once

#include "IPlugin.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

namespace vrutti::core::plugins {

    class DiagnosticService : public IPlugin {
    public:
        DiagnosticService() = default;
        ~DiagnosticService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct Diagnostic {
            int severity;
            std::string message;
            std::string source;
            int startLine;
            int startColumn;
            int endLine;
            int endColumn;
        };

        std::mutex m_mutex;
        // Map from URI to diagnostics array
        std::unordered_map<std::string, std::vector<Diagnostic>> m_diagnostics;

        std::string handleChangeMany(const std::string& payload);
        std::string handleClear(const std::string& payload);
        std::string handleGetDiagnostics(const std::string& payload);
        
        void emitDiagnosticsChanged(const std::string& uri);
    };

} // namespace vrutti::core::plugins
