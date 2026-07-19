#include "PieceTable.h"
#include <stdexcept>

namespace vrutti::core::editor {

    PieceTable::PieceTable(const std::string& originalText) 
        : m_originalBuffer(originalText), m_totalLength(originalText.length()) 
    {
        if (m_totalLength > 0) {
            m_pieces.push_back({ BufferType::Original, 0, m_totalLength });
        }
    }

    PieceTable::PieceLocation PieceTable::findPiece(size_t logicalOffset) const {
        if (logicalOffset > m_totalLength) {
            throw std::out_of_range("Offset out of bounds");
        }

        size_t currentOffset = 0;
        for (size_t i = 0; i < m_pieces.size(); ++i) {
            const auto& piece = m_pieces[i];
            if (currentOffset + piece.length > logicalOffset || 
               (currentOffset + piece.length == logicalOffset && logicalOffset == m_totalLength)) 
            {
                return { i, logicalOffset - currentOffset };
            }
            currentOffset += piece.length;
        }

        return { m_pieces.size(), 0 }; // Should only happen if empty
    }

    void PieceTable::insert(size_t offset, const std::string& text) {
        if (text.empty()) return;

        size_t appendStart = m_appendBuffer.length();
        m_appendBuffer += text;
        Piece newPiece = { BufferType::Append, appendStart, text.length() };

        if (m_pieces.empty()) {
            m_pieces.push_back(newPiece);
        } else {
            PieceLocation loc = findPiece(offset);
            
            if (loc.pieceIndex < m_pieces.size()) {
                Piece targetPiece = m_pieces[loc.pieceIndex];
                
                // Split the piece if we are inserting in the middle
                if (loc.offsetInPiece == 0) {
                    m_pieces.insert(m_pieces.begin() + loc.pieceIndex, newPiece);
                } else if (loc.offsetInPiece == targetPiece.length) {
                    m_pieces.insert(m_pieces.begin() + loc.pieceIndex + 1, newPiece);
                } else {
                    Piece left = { targetPiece.buffer, targetPiece.start, loc.offsetInPiece };
                    Piece right = { targetPiece.buffer, targetPiece.start + loc.offsetInPiece, targetPiece.length - loc.offsetInPiece };
                    
                    m_pieces[loc.pieceIndex] = left;
                    m_pieces.insert(m_pieces.begin() + loc.pieceIndex + 1, newPiece);
                    m_pieces.insert(m_pieces.begin() + loc.pieceIndex + 2, right);
                }
            } else {
                m_pieces.push_back(newPiece); // Append at end if empty fallback
            }
        }
        m_totalLength += text.length();
    }

    void PieceTable::remove(size_t offset, size_t length) {
        if (length == 0) return;
        if (offset + length > m_totalLength) {
            throw std::out_of_range("Delete range out of bounds");
        }

        PieceLocation startLoc = findPiece(offset);
        PieceLocation endLoc = findPiece(offset + length);
        
        // This is a simplified linear removal. A production version would merge 
        // adjacent chunks and properly slice overlapping pieces.
        // For now, we will handle a naive single-piece split or full piece drop.
        
        std::vector<Piece> newPieces;
        size_t currentOffset = 0;
        
        for (const auto& piece : m_pieces) {
            size_t pieceStart = currentOffset;
            size_t pieceEnd = currentOffset + piece.length;
            
            // If piece is entirely before the delete range
            if (pieceEnd <= offset) {
                newPieces.push_back(piece);
            }
            // If piece is entirely after the delete range
            else if (pieceStart >= offset + length) {
                newPieces.push_back(piece);
            }
            // Piece intersects with delete range
            else {
                // Keep the left part if any
                if (offset > pieceStart) {
                    size_t leftLen = offset - pieceStart;
                    newPieces.push_back({ piece.buffer, piece.start, leftLen });
                }
                // Keep the right part if any
                if (pieceEnd > offset + length) {
                    size_t rightLen = pieceEnd - (offset + length);
                    size_t rightStart = piece.start + piece.length - rightLen;
                    newPieces.push_back({ piece.buffer, rightStart, rightLen });
                }
            }
            currentOffset += piece.length;
        }

        m_pieces = std::move(newPieces);
        m_totalLength -= length;
    }

    std::string PieceTable::substring(size_t offset, size_t length) const {
        if (offset + length > m_totalLength) {
            throw std::out_of_range("Substring out of bounds");
        }

        std::string result;
        result.reserve(length);
        
        size_t currentOffset = 0;
        size_t remaining = length;
        size_t readOffset = offset;

        for (const auto& piece : m_pieces) {
            size_t pieceStart = currentOffset;
            size_t pieceEnd = currentOffset + piece.length;
            
            if (remaining > 0 && pieceEnd > readOffset) {
                size_t startInPiece = readOffset > pieceStart ? readOffset - pieceStart : 0;
                size_t charsToRead = std::min(piece.length - startInPiece, remaining);
                
                const std::string& buffer = (piece.buffer == BufferType::Original) ? m_originalBuffer : m_appendBuffer;
                result.append(buffer, piece.start + startInPiece, charsToRead);
                
                remaining -= charsToRead;
                readOffset += charsToRead;
            }
            currentOffset += piece.length;
            if (remaining == 0) break;
        }

        return result;
    }

    std::string PieceTable::getText() const {
        return substring(0, m_totalLength);
    }

    size_t PieceTable::length() const {
        return m_totalLength;
    }

} // namespace vrutti::core::editor
