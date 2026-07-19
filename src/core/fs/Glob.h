#pragma once
#include <string>

namespace vrutti::core::fs {

    // A lightweight glob pattern matcher, mapped from VS Code's `glob.ts`
    class Glob {
    public:
        // Returns true if the provided path matches the glob pattern.
        // Supports '*' (matches any character except '/')
        // Supports '**' (matches any character including '/')
        // Supports '?' (matches exactly one character except '/')
        static bool match(const std::string& pattern, const std::string& path);
    };

} // namespace vrutti::core::fs
