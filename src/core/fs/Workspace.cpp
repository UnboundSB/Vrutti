#include "Workspace.h"
#include <iostream>

namespace vrutti::core::fs {

    Workspace::Workspace(const std::string& rootPath) {
        m_root.path = rootPath;
        m_root.name = std::filesystem::path(rootPath).filename().string();
        m_root.isDirectory = true;
        m_root.isScanned = false;
    }

    void Workspace::scanDirectory(FileNode& node) {
        if (!node.isDirectory || node.isScanned) {
            return;
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(node.path)) {
                FileNode child;
                child.path = entry.path().string();
                child.name = entry.path().filename().string();
                child.isDirectory = entry.is_directory();
                child.isScanned = false;
                node.children.push_back(child);
            }
            node.isScanned = true;
        } catch (const std::exception& e) {
            std::cerr << "[Workspace] Failed to scan directory " << node.path << ": " << e.what() << std::endl;
        }
    }

    void Workspace::unloadDirectory(FileNode& node) {
        if (node.isScanned) {
            node.children.clear();
            node.children.shrink_to_fit();
            node.isScanned = false;
        }
    }

} // namespace vrutti::core::fs
