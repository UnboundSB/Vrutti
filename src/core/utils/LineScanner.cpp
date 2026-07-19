#include "LineScanner.h"
#include <cstring>

namespace vrutti::core::utils {

    std::vector<size_t> LineScanner::computeLineStarts(std::string_view text) {
        std::vector<size_t> lineStarts;
        // The first line always starts at index 0
        lineStarts.push_back(0);

        if (text.empty()) {
            return lineStarts;
        }

        const char* start = text.data();
        const char* end = start + text.length();
        const char* current = start;

        // std::memchr is highly optimized, often using AVX2 or SSE4.2 underneath.
        while (current < end) {
            const void* ptr = std::memchr(current, '\n', end - current);
            if (!ptr) {
                break;
            }
            
            current = static_cast<const char*>(ptr);
            // The next line starts immediately after the '\n'
            size_t nextLineStart = (current - start) + 1;
            lineStarts.push_back(nextLineStart);
            
            current++;
        }

        return lineStarts;
    }

} // namespace vrutti::core::utils
