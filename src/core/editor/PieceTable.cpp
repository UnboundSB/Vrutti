#include "PieceTable.h"
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace vrutti::core::editor {

    PieceTable::PieceTable(const std::string& filePath, size_t fileLength) 
        : m_arena(1024 * 1024), m_freeList(nullptr), m_totalLength(fileLength) 
    {
        m_fileStream.open(filePath, std::ios::binary);
        if (!m_fileStream && m_totalLength > 0) {
            throw std::runtime_error("Failed to open file for PieceTable: " + filePath);
        }

        m_nil = allocateNode({ BufferType::Original, 0, 0 });
        m_nil->color = NodeColor::Black;
        m_nil->left = m_nil;
        m_nil->right = m_nil;
        m_nil->parent = m_nil;
        m_root = m_nil;

        if (m_totalLength > 0) {
            TreeNode* initialNode = allocateNode({ BufferType::Original, 0, m_totalLength });
            initialNode->left = m_nil;
            initialNode->right = m_nil;
            initialNode->parent = m_nil;
            initialNode->color = NodeColor::Black;
            m_root = initialNode;
        }
    }

    PieceTable::~PieceTable() {
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
    }

    PieceTable::TreeNode* PieceTable::allocateNode(const Piece& p) {
        if (m_freeList) {
            TreeNode* node = m_freeList;
            m_freeList = m_freeList->right; // we use right ptr for freelist chain
            node->init(p);
            return node;
        }
        void* mem = m_arena.allocate(sizeof(TreeNode), alignof(TreeNode));
        TreeNode* node = static_cast<TreeNode*>(mem);
        node->init(p);
        return node;
    }

    void PieceTable::freeNode(TreeNode* node) {
        if (node == m_nil) return;
        node->right = m_freeList;
        m_freeList = node;
    }

    std::string PieceTable::readFromOriginalBuffer(size_t offset, size_t length) const {
        if (length == 0 || !m_fileStream.is_open()) return "";

        std::string result;
        result.reserve(length);
        size_t remaining = length;
        size_t currentOffset = offset;

        while (remaining > 0) {
            size_t chunkIndex = currentOffset / CHUNK_SIZE;
            size_t offsetInChunk = currentOffset % CHUNK_SIZE;
            size_t bytesToRead = std::min(remaining, CHUNK_SIZE - offsetInChunk);

            auto it = m_chunkCache.find(chunkIndex);
            if (it == m_chunkCache.end()) {
                // Cache miss: read from disk
                if (m_cacheOrder.size() >= MAX_CACHE_CHUNKS) {
                    size_t oldest = m_cacheOrder.back();
                    m_cacheOrder.pop_back();
                    m_chunkCache.erase(oldest);
                    m_cacheIterators.erase(oldest);
                }

                m_fileStream.clear();
                m_fileStream.seekg(chunkIndex * CHUNK_SIZE, std::ios::beg);
                
                std::string chunkBuf(CHUNK_SIZE, '\0');
                m_fileStream.read(chunkBuf.data(), CHUNK_SIZE);
                chunkBuf.resize(m_fileStream.gcount());

                m_chunkCache[chunkIndex] = chunkBuf;
                m_cacheOrder.push_front(chunkIndex);
                m_cacheIterators[chunkIndex] = m_cacheOrder.begin();
                it = m_chunkCache.find(chunkIndex);
            } else {
                // Cache hit: move to front (LRU)
                m_cacheOrder.erase(m_cacheIterators[chunkIndex]);
                m_cacheOrder.push_front(chunkIndex);
                m_cacheIterators[chunkIndex] = m_cacheOrder.begin();
            }

            const std::string& chunkData = it->second;
            size_t available = chunkData.length() > offsetInChunk ? chunkData.length() - offsetInChunk : 0;
            size_t actualRead = std::min(bytesToRead, available);
            
            result.append(chunkData, offsetInChunk, actualRead);
            remaining -= actualRead;
            currentOffset += actualRead;
            
            if (actualRead < bytesToRead) break; // EOF reached unexpectedly
        }

        return result;
    }

    void PieceTable::leftRotate(TreeNode* x) {
        TreeNode* y = x->right;
        x->right = y->left;
        if (y->left != m_nil) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == m_nil) m_root = y;
        else if (x == x->parent->left) x->parent->left = y;
        else x->parent->right = y;
        y->left = x;
        x->parent = y;
        y->size_left = y->size_left + x->size_left + x->piece.length;
    }

    void PieceTable::rightRotate(TreeNode* x) {
        TreeNode* y = x->left;
        x->left = y->right;
        if (y->right != m_nil) y->right->parent = x;
        y->parent = x->parent;
        if (x->parent == m_nil) m_root = y;
        else if (x == x->parent->right) x->parent->right = y;
        else x->parent->left = y;
        y->right = x;
        x->parent = y;
        x->size_left = x->size_left - (y->size_left + y->piece.length);
    }

    void PieceTable::insertFixup(TreeNode* z) {
        while (z->parent->color == NodeColor::Red) {
            if (z->parent == z->parent->parent->left) {
                TreeNode* y = z->parent->parent->right;
                if (y->color == NodeColor::Red) {
                    z->parent->color = NodeColor::Black;
                    y->color = NodeColor::Black;
                    z->parent->parent->color = NodeColor::Red;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        z = z->parent;
                        leftRotate(z);
                    }
                    z->parent->color = NodeColor::Black;
                    z->parent->parent->color = NodeColor::Red;
                    rightRotate(z->parent->parent);
                }
            } else {
                TreeNode* y = z->parent->parent->left;
                if (y->color == NodeColor::Red) {
                    z->parent->color = NodeColor::Black;
                    y->color = NodeColor::Black;
                    z->parent->parent->color = NodeColor::Red;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        rightRotate(z);
                    }
                    z->parent->color = NodeColor::Black;
                    z->parent->parent->color = NodeColor::Red;
                    leftRotate(z->parent->parent);
                }
            }
        }
        m_root->color = NodeColor::Black;
    }

    PieceTable::PieceLocation PieceTable::findNode(size_t logicalOffset) const {
        if (logicalOffset > m_totalLength) throw std::out_of_range("Offset out of bounds");
        TreeNode* current = m_root;
        while (current != m_nil) {
            if (logicalOffset < current->size_left) {
                current = current->left;
            } else if (logicalOffset > current->size_left + current->piece.length) {
                logicalOffset -= (current->size_left + current->piece.length);
                current = current->right;
            } else {
                if (logicalOffset == current->size_left + current->piece.length && current->right != m_nil) {
                    logicalOffset -= (current->size_left + current->piece.length);
                    current = current->right;
                } else {
                    return { current, logicalOffset - current->size_left };
                }
            }
        }
        return { m_nil, 0 };
    }

    void PieceTable::updateSizeLeft(TreeNode* node, int64_t delta) {
        while (node != m_root) {
            TreeNode* p = node->parent;
            if (p->left == node) p->size_left += delta;
            node = p;
        }
    }

    void PieceTable::insertNodeAt(size_t offset, TreeNode* newNode) {
        if (m_root == m_nil) {
            m_root = newNode;
            newNode->color = NodeColor::Black;
            return;
        }
        TreeNode* current = m_root;
        TreeNode* parent = m_nil;
        bool insertLeft = true;
        while (current != m_nil) {
            parent = current;
            if (offset <= current->size_left) {
                current->size_left += newNode->piece.length;
                insertLeft = true;
                current = current->left;
            } else {
                offset -= (current->size_left + current->piece.length);
                insertLeft = false;
                current = current->right;
            }
        }
        newNode->parent = parent;
        if (insertLeft) parent->left = newNode;
        else parent->right = newNode;
        insertFixup(newNode);
    }

    void PieceTable::splitNode(TreeNode* node, size_t offsetInPiece) {
        if (offsetInPiece == 0 || offsetInPiece == node->piece.length) return;
        Piece rightPiece = { node->piece.buffer, node->piece.start + offsetInPiece, node->piece.length - offsetInPiece };
        int64_t shrinkDelta = -static_cast<int64_t>(rightPiece.length);
        node->piece.length = offsetInPiece;
        updateSizeLeft(node, shrinkDelta);
        
        TreeNode* rightNode = allocateNode(rightPiece);
        rightNode->left = m_nil;
        rightNode->right = m_nil;
        
        size_t logicalOffset = 0;
        TreeNode* temp = node;
        logicalOffset += temp->size_left + temp->piece.length;
        while (temp != m_root) {
            if (temp != temp->parent->left) {
                logicalOffset += temp->parent->size_left + temp->parent->piece.length;
            }
            temp = temp->parent;
        }
        insertNodeAt(logicalOffset, rightNode);
    }

    void PieceTable::insert(size_t offset, const std::string& text) {
        if (text.empty()) return;
        size_t appendStart = m_appendBuffer.length();
        m_appendBuffer += text;
        Piece newPiece = { BufferType::Append, appendStart, text.length() };

        if (m_root != m_nil) {
            PieceLocation loc = findNode(offset);
            if (loc.node != m_nil) splitNode(loc.node, loc.offsetInPiece);
        }

        TreeNode* newNode = allocateNode(newPiece);
        newNode->left = m_nil;
        newNode->right = m_nil;
        insertNodeAt(offset, newNode);
        m_totalLength += text.length();
    }

    void PieceTable::remove(size_t offset, size_t length) {
        if (length == 0) return;
        PieceLocation startLoc = findNode(offset);
        if (startLoc.node != m_nil) splitNode(startLoc.node, startLoc.offsetInPiece);
        PieceLocation endLoc = findNode(offset + length);
        if (endLoc.node != m_nil) splitNode(endLoc.node, endLoc.offsetInPiece);
        
        auto deleteRange = [&](auto& self, TreeNode* n, size_t nOffset) -> void {
            if (n == m_nil) return;
            size_t nStart = nOffset + n->size_left;
            size_t originalLength = n->piece.length;
            size_t nEnd = nStart + originalLength;
            if (nStart < offset + length && nEnd > offset) {
                int64_t delta = -static_cast<int64_t>(originalLength);
                n->piece.length = 0;
                updateSizeLeft(n, delta);
                m_deletedNodesCount++;
            }

            if (nOffset + n->size_left > offset) self(self, n->left, nOffset);
            if (nStart + originalLength < offset + length) self(self, n->right, nStart + originalLength);
        };
        
        deleteRange(deleteRange, m_root, 0);
        m_totalLength -= length;

        if (m_deletedNodesCount > 1000) {
            garbageCollect();
        }
    }

    void PieceTable::buildString(TreeNode* node, size_t offset, size_t length, std::string& result, size_t& currentOffset, size_t& remaining) const {
        if (node == m_nil || remaining == 0) return;
        buildString(node->left, offset, length, result, currentOffset, remaining);
        if (remaining == 0) return;

        size_t pieceStart = currentOffset;
        size_t pieceEnd = currentOffset + node->piece.length;
        if (pieceEnd > offset && node->piece.length > 0) {
            size_t startInPiece = offset > pieceStart ? offset - pieceStart : 0;
            size_t charsToRead = std::min(node->piece.length - startInPiece, remaining);
            
            if (node->piece.buffer == BufferType::Original) {
                result.append(readFromOriginalBuffer(node->piece.start + startInPiece, charsToRead));
            } else {
                result.append(m_appendBuffer, node->piece.start + startInPiece, charsToRead);
            }
            
            remaining -= charsToRead;
            offset += charsToRead;
        }
        currentOffset += node->piece.length;
        buildString(node->right, offset, length, result, currentOffset, remaining);
    }

    std::string PieceTable::substring(size_t offset, size_t length) const {
        std::string result;
        result.reserve(length);
        size_t currentOffset = 0;
        size_t remaining = length;
        buildString(m_root, offset, length, result, currentOffset, remaining);
        return result;
    }

    std::string PieceTable::getText() const {
        return substring(0, m_totalLength);
    }

    size_t PieceTable::length() const {
        return m_totalLength;
    }

    void PieceTable::garbageCollect() {
        std::vector<Piece> validPieces;
        auto collect = [&](auto& self, TreeNode* n) -> void {
            if (n == m_nil) return;
            self(self, n->left);
            if (n->piece.length > 0) {
                validPieces.push_back(n->piece);
            }
            self(self, n->right);
        };
        collect(collect, m_root);

        auto freeAll = [&](auto& self, TreeNode* n) -> void {
            if (n == m_nil) return;
            self(self, n->left);
            self(self, n->right);
            freeNode(n);
        };
        freeAll(freeAll, m_root);

        m_root = m_nil;
        m_deletedNodesCount = 0;
        size_t offset = 0;

        for (const auto& p : validPieces) {
            TreeNode* newNode = allocateNode(p);
            newNode->left = m_nil;
            newNode->right = m_nil;
            insertNodeAt(offset, newNode);
            offset += p.length;
        }
    }

} // namespace vrutti::core::editor
