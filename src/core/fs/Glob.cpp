#include "Glob.h"
#include <regex>

namespace vrutti::core::fs {

    bool Glob::match(const std::string& pattern, const std::string& path) {
        // Convert a glob pattern to a standard regex string
        std::string regexStr = "^";
        
        for (size_t i = 0; i < pattern.length(); ++i) {
            char c = pattern[i];
            if (c == '*' && i + 1 < pattern.length() && pattern[i + 1] == '*') {
                // '**' matches anything, including directories
                regexStr += ".*";
                i++; // skip next star
            } else if (c == '*') {
                // '*' matches anything EXCEPT directory separators
                regexStr += "[^\\\\/]*";
            } else if (c == '?') {
                // '?' matches exactly one character EXCEPT directory separators
                regexStr += "[^\\\\/]";
            } else if (c == '.' || c == '\\' || c == '+' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '^' || c == '$' || c == '|') {
                // Escape special regex characters
                regexStr += "\\";
                regexStr += c;
            } else {
                regexStr += c;
            }
        }
        
        regexStr += "$";
        
        try {
            std::regex re(regexStr);
            return std::regex_match(path, re);
        } catch (...) {
            return false; // Malformed pattern
        }
    }

} // namespace vrutti::core::fs
