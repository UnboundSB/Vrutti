#pragma once

#include "IPlugin.h"
#include "../editor/PieceTable.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>

namespace vrutti::core::plugins {

    class DocumentSyncService : public IPlugin {
    public:
        DocumentSyncService();
        ~DocumentSyncService() override = default;

        bool initialize() override;
        void shutdown() override;
        const char* getName() const override;
        std::string executeCommand(const std::string& command, const std::string& payload) override;

    private:
        struct DocumentState {
            std::string uri;
            std::unique_ptr<vrutti::core::editor::PieceTable> pieceTable;
            uint32_t versionId;
            std::string eol;
            std::string languageId;
            bool isDirty;

            // Batching context
            std::string pendingChangesBuffer;
            std::chrono::time_point<std::chrono::steady_clock> lastEditTime;
            bool hasPendingEdits;
        };

        std::mutex m_mutex;
        std::unordered_map<std::string, std::shared_ptr<DocumentState>> m_documents;

        std::string handleDocumentEdited(const std::string& payload);
        std::string handleDocumentOpened(const std::string& payload);
        std::string handleDocumentClosed(const std::string& payload);
        std::string handleSelectionChanged(const std::string& payload);
        std::string handleSyncRequest(const std::string& payload); // Periodic or batched sync flush
        
        void flushPendingEdits(std::shared_ptr<DocumentState> doc);
    };

} // namespace vrutti::core::plugins
