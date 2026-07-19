#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace vrutti::core::fs {

    // A memory-optimized, lazy-evaluated URI (Uniform Resource Identifier).
    // To achieve ultra-low RAM consumption, it avoids allocating separate strings
    // for scheme, authority, path, query, and fragment. 
    // Instead, it stores the raw string once, and only computes/returns lightweight 
    // `std::string_view` spans into the original string when requested.
    class URI {
    public:
        // Constructs a URI from a raw string. 
        // Parsing is done lazily or by storing tiny 16-bit offset indices, keeping memory footprint tiny.
        explicit URI(std::string rawUri);
        
        // Disable default construction to ensure valid URIs
        URI() = delete;

        // --- Lazy Evaluated Getters (Zero-Copy) ---
        // These return string_views which point directly to memory inside `m_rawUri`
        // meaning zero heap allocations happen when you call these.

        std::string_view scheme() const;
        std::string_view authority() const;
        std::string_view path() const;
        std::string_view query() const;
        std::string_view fragment() const;

        // Returns the full raw URI string
        const std::string& toString() const;

    private:
        // Triggers the minimal parsing to find component boundaries if not already parsed.
        void parseIfNeeded() const;

        std::string m_rawUri;
        
        // We use mutable so the lazy-evaluation can happen inside const getter methods.
        // We only store the start/end index of components. (16-bit integers = 2 bytes each).
        // This is dramatically smaller than storing 5 separate std::strings.
        mutable bool m_isParsed = false;
        mutable uint16_t m_schemeEnd = 0;
        mutable uint16_t m_authorityStart = 0;
        mutable uint16_t m_authorityEnd = 0;
        mutable uint16_t m_pathStart = 0;
        mutable uint16_t m_pathEnd = 0;
        mutable uint16_t m_queryStart = 0;
        mutable uint16_t m_queryEnd = 0;
        mutable uint16_t m_fragmentStart = 0;
    };

} // namespace vrutti::core::fs
