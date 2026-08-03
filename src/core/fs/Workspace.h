#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace vrutti::core::fs {

    struct FileNode {
        std::string name;
        std::string path;
        bool isDirectory;
        bool isScanned = false;
        std::vector<FileNode> children;
    };

    class Workspace {
    public:
        // === Constructors ===
        Workspace(const std::string& rootPath);
        
        // === Getters ===
        const FileNode& getRoot() const { return m_root; }
        FileNode& getRootMutable() { return m_root; }

        // === Directory Management ===
        
        // Scans the directory to load its children
        void scanDirectory(FileNode& node);

        // Clears the loaded children to free memory
        void unloadDirectory(FileNode& node);

    private:
        FileNode m_root;
    };

}