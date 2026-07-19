#pragma once
#include <string>
#include <memory>
#include <cstdint>

namespace vrutti::core::editor {

    class PieceTable {
    public:
        // Initialize with the original file contents
        explicit PieceTable(const std::string& originalText);
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

            TreeNode(const Piece& p) : piece(p), size_left(0), color(NodeColor::Red), left(nullptr), right(nullptr), parent(nullptr) {}
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
        
        // Inserts a new node into the tree at the specified logical offset
        void insertNodeAt(size_t offset, TreeNode* newNode);
        
        // Splits a node at a given offset inside its piece
        void splitNode(TreeNode* node, size_t offsetInPiece);
        
        // Finds the node containing the logical offset
        PieceLocation findNode(size_t logicalOffset) const;
        
        // Updates the size_left of ancestors after an insertion or length change
        void updateSizeLeft(TreeNode* node, int64_t delta);
        
        // Recursive deletion helper
        void deleteTree(TreeNode* node);
        
        // In-order traversal helper for building strings
        void buildString(TreeNode* node, size_t offset, size_t length, std::string& result, size_t& currentOffset, size_t& remaining) const;

        std::string m_originalBuffer;
        std::string m_appendBuffer;
        size_t m_totalLength;
    };

} // namespace vrutti::core::editor
