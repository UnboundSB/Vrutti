#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

namespace vrutti::core::memory {

    // A fast Arena Allocator designed for low-overhead, contiguous memory allocations.
    class ArenaAllocator {
    public:
        explicit ArenaAllocator(size_t blockSize = 1024 * 1024); // Default 1MB blocks
        ~ArenaAllocator();

        // Prevent copying to maintain memory safety
        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        // Allocate memory of specified size and alignment.
        //
        // The alignment may not be zero: an alignment of zero has no
        // meaning, and the old code divided by it.
        void* allocate(size_t size, size_t alignment = alignof(std::max_align_t));

        // Templated helper for type-safe allocation
        template <typename T, typename... Args>
        T* allocateObject(Args&&... args) {
            void* mem = allocate(sizeof(T), alignof(T));
            return new (mem) T(std::forward<Args>(args)...);
        }

        // Resets the arena, freeing all memory blocks
        void reset();

    private:
        struct Block {
            uint8_t* memory;
            size_t size;
            size_t used;
        };

        void allocateNewBlock(size_t minimumSize);

        // The block that is being filled, or nothing when there is none.
        //
        // The blocks are held in a list that grows, and a growing list
        // moves its blocks to another place. The index is used instead of
        // a pointer, so that the block that is being filled is always the
        // block that is meant and never a block of somewhere else.
        Block* currentBlock();
        static constexpr size_t kNoBlock = static_cast<size_t>(-1);

        std::vector<Block> m_blocks;
        size_t m_blockSize;
        size_t m_currentBlockIndex = kNoBlock;
    };

} // namespace vrutti::core::memory
