#include "ArenaAllocator.h"
#include <cstdlib>
#include <algorithm>

namespace vrutti::core::memory {

    ArenaAllocator::ArenaAllocator(size_t blockSize)
        : m_blockSize(blockSize), m_currentBlock(nullptr) {
    }

    ArenaAllocator::~ArenaAllocator() {
        reset();
    }

    void* ArenaAllocator::allocate(size_t size, size_t alignment) {
        if (!m_currentBlock) {
            allocateNewBlock(size);
        }

        // Calculate aligned offset
        size_t alignmentOffset = reinterpret_cast<uintptr_t>(m_currentBlock->memory + m_currentBlock->used) % alignment;
        size_t padding = (alignmentOffset == 0) ? 0 : (alignment - alignmentOffset);
        
        if (m_currentBlock->used + padding + size > m_currentBlock->size) {
            allocateNewBlock(size + alignment); // Guarantee room for object + alignment padding
            size_t newAlignmentOffset = reinterpret_cast<uintptr_t>(m_currentBlock->memory) % alignment;
            padding = (newAlignmentOffset == 0) ? 0 : (alignment - newAlignmentOffset);
        }

        void* ptr = m_currentBlock->memory + m_currentBlock->used + padding;
        m_currentBlock->used += padding + size;
        
        return ptr;
    }

    void ArenaAllocator::allocateNewBlock(size_t minimumSize) {
        size_t sizeToAllocate = std::max(m_blockSize, minimumSize);
        uint8_t* mem = static_cast<uint8_t*>(std::malloc(sizeToAllocate));
        if (!mem) {
            throw std::bad_alloc();
        }
        m_blocks.push_back({ mem, sizeToAllocate, 0 });
        m_currentBlock = &m_blocks.back();
    }

    void ArenaAllocator::reset() {
        for (auto& block : m_blocks) {
            std::free(block.memory);
        }
        m_blocks.clear();
        m_currentBlock = nullptr;
    }

} // namespace vrutti::core::memory
