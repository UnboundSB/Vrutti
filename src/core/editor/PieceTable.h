#pragma once
#include <string>
#include <fstream>
#include <unordered_map>
#include <list>
#include <cstdint>
#include "core/memory/ArenaAllocator.h"

namespace vrutti::core::editor {

    class PieceTable {
    public:
        // Initialize with a file path for LAZY LOADING. 
        // We do not load the whole file into RAM.
        explicit PieceTable(const std::string& filePath, size_t fileLength);
        ~PieceTable();

        // Core API - Keeps names simple for connecting to outer systems
        void insert(size_t offset, const std::string& text);
        void remove(size_t offset, size_t length);
        std::string getText() const;
        std::string substring(size_t offset, size_t length) const;
        size_t length() const;

    private:
        enum class BufferType { Original, Append };
        enum class NodeColor { Red, Black };

        struct Piece {
            BufferType buffer;
            size_t start;
            size_t length;
        };

        struct TreeNode {
            Piece piece;
            size_t size_left; // Sum of lengths in the left subtree
            NodeColor color;
            TreeNode* left;
            TreeNode* right;
            TreeNode* parent;

            // Placement new allows us to construct directly in arena memory
            void init(const Piece& p) {
                piece = p;
                size_left = 0;
                color = NodeColor::Red;
                left = nullptr;
                right = nullptr;
                parent = nullptr;
            }
        };

        struct PieceLocation {
            TreeNode* node;
            size_t offsetInPiece;
        };

        // Red-Black Tree operations
        TreeNode* m_root;
        TreeNode* m_nil; // Sentinel node for leaves

        void leftRotate(TreeNode* x);
        void rightRotate(TreeNode* x);
        void insertFixup(TreeNode* z);
        void insertNodeAt(size_t offset, TreeNode* newNode);
        void splitNode(TreeNode* node, size_t offsetInPiece);
        PieceLocation findNode(size_t logicalOffset) const;
        void updateSizeLeft(TreeNode* node, int64_t delta);
        
        // Memory Architecture (Cache-Locality & Zero-Malloc)
        vrutti::core::memory::ArenaAllocator m_arena;
        TreeNode* m_freeList;
        TreeNode* allocateNode(const Piece& p);
        void freeNode(TreeNode* node); // Just adds to freelist

        void buildString(TreeNode* node, size_t offset, size_t length, std::string& result, size_t& currentOffset, size_t& remaining) const;

        // Lazy Loading Architecture
        mutable std::ifstream m_fileStream;
        std::string m_appendBuffer;
        size_t m_totalLength;

        // LRU Cache for disk chunks (4KB chunks)
        static constexpr size_t CHUNK_SIZE = 4096;
        static constexpr size_t MAX_CACHE_CHUNKS = 100; // 400KB max RAM overhead
        
        mutable std::list<size_t> m_cacheOrder;
        mutable std::unordered_map<size_t, std::string> m_chunkCache;
        mutable std::unordered_map<size_t, std::list<size_t>::iterator> m_cacheIterators;

        std::string readFromOriginalBuffer(size_t offset, size_t length) const;
    };

} // namespace vrutti::core::editor
