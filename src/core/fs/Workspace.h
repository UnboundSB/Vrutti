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

        // === Lazy Loading Memory Management ===
        
        // Lazy loads children of a specific node into RAM
        void scanDirectory(FileNode& node);

        // Instantly frees RAM when a folder is collapsed
        void unloadDirectory(FileNode& node);

    private:
        FileNode m_root;
    };

}