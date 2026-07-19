#pragma once
#include <vector>
#include <string_view>
#include <cstdint>

namespace vrutti::core::utils {

    // A high-performance utility to scan raw memory buffers for line breaks.
    // It utilizes standard C library functions (which are heavily SIMD-optimized
    // on modern compilers) to skip characters 16 or 32 bytes at a time, making
    // line index generation extremely fast for multi-gigabyte files.
    class LineScanner {
    public:
        // Scans the provided text block and returns a vector of offsets where each line begins.
        // The first offset is always 0.
        static std::vector<size_t> computeLineStarts(std::string_view text);
    };

} // namespace vrutti::core::utils
