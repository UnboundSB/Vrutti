#pragma once
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <memory>
#include "core/memory/ArenaAllocator.h"

namespace vrutti::core::utils {

    // A fast, localized string pool intended to be used on a per-file or per-document basis.
    // It deduplicates identical strings to drastically reduce RAM usage.
    // By keeping it per-file, we avoid global mutex contention and keep cache locality high.
    class StringPool {
    public:
        StringPool();
        ~StringPool() = default;

        // Interns a string. If it already exists in the pool, returns a view to the existing copy.
        // If it's new, copies it into the pool's arena and returns a view to the new copy.
        std::string_view intern(std::string_view str);

    private:
        vrutti::core::memory::ArenaAllocator m_arena;
        
        // We use a set of string_views that point directly into the arena's memory
        std::unordered_set<std::string_view> m_pool;
    };

} // namespace vrutti::core::utils
