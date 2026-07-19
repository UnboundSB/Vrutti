#include "StringPool.h"
#include <cstring>

namespace vrutti::core::utils {

    StringPool::StringPool() : m_arena(65536) {} // 64KB blocks

    std::string_view StringPool::intern(std::string_view str) {
        if (str.empty()) return {};

        // Fast path: Check if it already exists
        auto it = m_pool.find(str);
        if (it != m_pool.end()) {
            return *it;
        }

        // Slow path: Allocate it in the arena and add to the pool
        void* mem = m_arena.allocate(str.length() + 1, alignof(char));
        char* dest = static_cast<char*>(mem);
        std::memcpy(dest, str.data(), str.length());
        dest[str.length()] = '\0'; // Null terminate for safety, though string_view doesn't strictly need it

        std::string_view pooledView(dest, str.length());
        m_pool.insert(pooledView);
        
        return pooledView;
    }

} // namespace vrutti::core::utils
