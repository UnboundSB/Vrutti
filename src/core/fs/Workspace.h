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
        Workspace(const std::string& rootPath);
        
        const FileNode& getRoot() const { return m_root; }
        FileNode& getRootMutable() { return m_root; }

        // Lazy loads children of a specific node
        void scanDirectory(FileNode& node);

    private:
        FileNode m_root;
    };

}