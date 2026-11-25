#include "ArenaAllocator.h"
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <new>

namespace vrutti::core::memory {

    ArenaAllocator::ArenaAllocator(size_t blockSize)
        : m_blockSize(blockSize) {
        if (m_blockSize == 0) m_blockSize = 1024 * 1024;
    }

    ArenaAllocator::~ArenaAllocator() {
        reset();
    }

    ArenaAllocator::Block* ArenaAllocator::currentBlock() {
        if (m_currentBlockIndex == kNoBlock) return nullptr;
        if (m_currentBlockIndex >= m_blocks.size()) return nullptr;
        return &m_blocks[m_currentBlockIndex];
    }

    void* ArenaAllocator::allocate(size_t size, size_t alignment) {
        // An alignment of zero has no meaning. The old code divided by it,
        // which takes the whole process down.
        if (alignment == 0) alignment = 1;

        if (!currentBlock()) {
            allocateNewBlock(size);
        }

        // Calculate aligned offset
        Block* block = currentBlock();
        size_t alignmentOffset = reinterpret_cast<uintptr_t>(block->memory + block->used) % alignment;
        size_t padding = (alignmentOffset == 0) ? 0 : (alignment - alignmentOffset);

        // The sum is never made before it is checked. A size that is very
        // large would make the sum wrap around, and the check would then
        // say that the piece fits into a block that it does not fit into.
        const bool fits = padding <= block->size - block->used &&
                          size <= block->size - block->used - padding;
        if (!fits) {
            // Guarantee room for object + alignment padding
            if (size > std::numeric_limits<size_t>::max() - alignment) {
                throw std::bad_alloc();
            }
            allocateNewBlock(size + alignment);
            block = currentBlock();
            size_t newAlignmentOffset = reinterpret_cast<uintptr_t>(block->memory) % alignment;
            padding = (newAlignmentOffset == 0) ? 0 : (alignment - newAlignmentOffset);
        }

        void* ptr = block->memory + block->used + padding;
        block->used += padding + size;
        
        return ptr;
    }

    void ArenaAllocator::allocateNewBlock(size_t minimumSize) {
        size_t sizeToAllocate = std::max(m_blockSize, minimumSize);
        uint8_t* mem = static_cast<uint8_t*>(std::malloc(sizeToAllocate));
        if (!mem) {
            throw std::bad_alloc();
        }
        m_blocks.push_back({ mem, sizeToAllocate, 0 });
        // The index is used instead of a pointer: the list of the blocks
        // grows, and a growing list moves its blocks to another place.
        m_currentBlockIndex = m_blocks.size() - 1;
    }

    void ArenaAllocator::reset() {
        for (auto& block : m_blocks) {
            std::free(block.memory);
        }
        m_blocks.clear();
        m_currentBlockIndex = kNoBlock;
    }

} // namespace vrutti::core::memory
