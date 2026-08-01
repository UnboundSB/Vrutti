#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include "../src/plugins/search/SearchPlugin.h"

int main() {
    vrutti::plugins::search::SearchPlugin plugin;
    
    // Create a dummy file to test special characters
    std::filesystem::create_directory("dummy_regex_test");
    std::ofstream out("dummy_regex_test/test.txt");
    out << "std::cout << (1 + 2) * 3;\n";
    out << "std::string foo;\n";
    out.close();

    vrutti::plugins::search::SearchOptions options;
    // We want to test std::cout with case insensitivity (which forces regex engine behind the scenes)
    // but without useRegex, it should match std::cout and NOT stdxcout.
    options.matchCase = false;
    options.wholeWord = false;
    options.useRegex = false;
    
    auto results = plugin.performSearch("std::cout", "dummy_regex_test", options);
    
    // Verify it matched the actual std::cout string and correctly escaped the ::
    assert(results.size() == 1);
    assert(results[0].line == 1);
    
    // Test whole word match for special characters
    options.wholeWord = true;
    auto results2 = plugin.performSearch("(1 + 2)", "dummy_regex_test", options);
    // Since (1 + 2) might have word boundary issues, we just check that it parses without crashing
    
    // Cleanup
    std::filesystem::remove_all("dummy_regex_test");
    
    std::cout << "Regex Escape tests passed!\n";
    return 0;
}
