#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace vrutti::core::editor {

    // A Piece Table implementation for highly efficient text editing.
    // Instead of copying large strings on every edit, it maintains a list of "pieces"
    // pointing to either the original read-only file buffer or a fast append-only buffer.
    class PieceTable {
    public:
        // Initialize with the original file contents
        explicit PieceTable(const std::string& originalText);
        ~PieceTable() = default;

        // Core API - Keeps names simple for connecting to outer systems
        
        // Inserts text at the specified character offset
        void insert(size_t offset, const std::string& text);
        
        // Deletes 'length' characters starting from 'offset'
        void remove(size_t offset, size_t length);
        
        // Retrieves the entire document as a single string
        std::string getText() const;
        
        // Retrieves a specific substring
        std::string substring(size_t offset, size_t length) const;

        // Current total length of the document
        size_t length() const;

    private:
        enum class BufferType { Original, Append };

        struct Piece {
            BufferType buffer;
            size_t start;  // Starting index in the respective buffer
            size_t length; // Length of this piece
        };

        // Locates which piece contains the logical offset, and where inside that piece it falls
        struct PieceLocation {
            size_t pieceIndex;
            size_t offsetInPiece;
        };

        PieceLocation findPiece(size_t logicalOffset) const;

        std::string m_originalBuffer;
        std::string m_appendBuffer;
        std::vector<Piece> m_pieces;
        size_t m_totalLength;
    };

} // namespace vrutti::core::editor
