#include "DocumentSyncService.h"
#include "../utils/Json.h"
#include <iostream>

using namespace vrutti::core::plugins;
using namespace vrutti::core::utils;
using namespace vrutti::core::editor;

DocumentSyncService::DocumentSyncService() {}

bool DocumentSyncService::initialize() {
    std::cout << "[DocumentSyncService] Initialized Document Broker with Piece Table integration." << std::endl;
    return true;
}

void DocumentSyncService::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_documents.clear();
}

const char* DocumentSyncService::getName() const {
    return "DocumentSyncService";
}

std::string DocumentSyncService::executeCommand(const std::string& command, const std::string& payload) {
    if (command == "document_edited") {
        return handleDocumentEdited(payload);
    } else if (command == "document_opened") {
        return handleDocumentOpened(payload);
    } else if (command == "document_closed") {
        return handleDocumentClosed(payload);
    } else if (command == "selection_changed") {
        return handleSelectionChanged(payload);
    } else if (command == "flush_sync") {
        return handleSyncRequest(payload);
    }
    return "{\"status\": \"unknown_command\"}";
}

std::string DocumentSyncService::handleDocumentOpened(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri, filePath;
    size_t fileLength = 0;
    
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        } else if (prop.first == "filePath" && prop.second->type == JsonNode::Type::String) {
            filePath = prop.second->stringValue;
        } else if (prop.first == "fileLength" && prop.second->type == JsonNode::Type::Number) {
            fileLength = static_cast<size_t>(prop.second->numberValue);
        }
    }

    if (uri.empty()) return "{\"status\": \"error\", \"message\": \"missing uri\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_documents.find(uri) == m_documents.end()) {
        auto doc = std::make_shared<DocumentState>();
        doc->uri = uri;
        doc->versionId = 1;
        doc->eol = "\n";
        doc->isDirty = false;
        doc->hasPendingEdits = false;
        try {
            doc->pieceTable = std::make_unique<PieceTable>(filePath, fileLength);
        } catch(const std::exception& e) {
            return "{\"status\": \"error\", \"message\": \"" + std::string(e.what()) + "\"}";
        }
        m_documents[uri] = doc;
    }

    return "{\"status\": \"success\"}";
}

std::string DocumentSyncService::handleDocumentClosed(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (reqNode && reqNode->type == JsonNode::Type::Object) {
        for (auto& prop : reqNode->objectProperties) {
            if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_documents.erase(std::string(prop.second->stringValue));
                return "{\"status\": \"success\"}";
            }
        }
    }
    return "{\"status\": \"error\"}";
}

std::string DocumentSyncService::handleDocumentEdited(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri;
    size_t offset = 0;
    size_t length = 0;
    std::string text;
    bool isInsert = false, isRemove = false;

    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        } else if (prop.first == "offset" && prop.second->type == JsonNode::Type::Number) {
            offset = static_cast<size_t>(prop.second->numberValue);
        } else if (prop.first == "length" && prop.second->type == JsonNode::Type::Number) {
            length = static_cast<size_t>(prop.second->numberValue);
        } else if (prop.first == "text" && prop.second->type == JsonNode::Type::String) {
            text = prop.second->stringValue;
        }
    }

    if (uri.empty()) return "{\"status\": \"error\"}";

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_documents.find(uri);
    if (it != m_documents.end()) {
        auto doc = it->second;
        if (length > 0) {
            doc->pieceTable->remove(offset, length);
        }
        if (!text.empty()) {
            doc->pieceTable->insert(offset, text);
        }
        doc->versionId++;
        doc->isDirty = true;
        
        // Batching logic: We record the pending edit. 
        // In a full implementation, we'd append to pendingChangesBuffer.
        doc->hasPendingEdits = true;
        doc->lastEditTime = std::chrono::steady_clock::now();

        return "{\"status\": \"success\", \"versionId\": " + std::to_string(doc->versionId) + "}";
    }
    
    return "{\"status\": \"error\", \"message\": \"document not found\"}";
}

std::string DocumentSyncService::handleSelectionChanged(const std::string& payload) {
    auto reqNode = JsonParser::parse(payload);
    if (!reqNode || reqNode->type != JsonNode::Type::Object) {
        return "{\"status\": \"error\"}";
    }

    std::string uri;
    for (auto& prop : reqNode->objectProperties) {
        if (prop.first == "uri" && prop.second->type == JsonNode::Type::String) {
            uri = prop.second->stringValue;
        }
    }

    if (uri.empty()) return "{\"status\": \"error\"}";

    // In a full implementation, we'd record the new selections and notify the ExtHost 
    // via ExtHostEditorsShape.$acceptEditorPropertiesChanged (debounced/batched).
    return "{\"status\": \"success\"}";
}

void DocumentSyncService::flushPendingEdits(std::shared_ptr<DocumentState> doc) {
    // Generate ISerializedModelContentChangedEvent and send over RPC
    // ... This would invoke VSCodeExtHostService to send the ExtHostDocumentsShape.$acceptModelChanged
    doc->hasPendingEdits = false;
    doc->pendingChangesBuffer.clear();
}

std::string DocumentSyncService::handleSyncRequest(const std::string& payload) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    for (auto& pair : m_documents) {
        auto doc = pair.second;
        if (doc->hasPendingEdits) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - doc->lastEditTime).count();
            if (elapsed > 50) { // 50ms debounce
                flushPendingEdits(doc);
            }
        }
    }
    return "{\"status\": \"success\"}";
}

extern "C" {
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    IPlugin* createPlugin() {
        return new DocumentSyncService();
    }
    
    #ifdef _WIN32
    __declspec(dllexport)
    #endif
    void destroyPlugin(IPlugin* plugin) {
        delete plugin;
    }
}
