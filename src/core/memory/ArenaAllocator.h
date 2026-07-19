#pragma once
#include <cstddef>
#include <vector>
#include <cstdint>

namespace vrutti::core::memory {

    // A simple, fast Arena Allocator designed for low-overhead, contiguous memory 
    // allocations. Ideal for UI trees or syntax trees where nodes are created in bulk
    // and destroyed together.
    class ArenaAllocator {
    public:
        explicit ArenaAllocator(size_t blockSize = 1024 * 1024); // Default 1MB blocks
        ~ArenaAllocator();

        // Prevent copying to maintain memory safety
        ArenaAllocator(const ArenaAllocator&) = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        // Allocate memory of specified size and alignment
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

        std::vector<Block> m_blocks;
        size_t m_blockSize;
        Block* m_currentBlock;
    };

} // namespace vrutti::core::memory
