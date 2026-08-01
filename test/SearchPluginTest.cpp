#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include "../src/plugins/search/SearchPlugin.h"

int main() {
    vrutti::plugins::search::SearchPlugin plugin;
    
    // Create a dummy file to test
    std::filesystem::create_directory("dummy_search_test");
    std::ofstream out("dummy_search_test/test.txt");
    out << "hello world\nthis is a test\nend of file\n";
    out.close();

    vrutti::plugins::search::SearchOptions options;
    options.matchCase = false;
    options.wholeWord = false;
    options.useRegex = false;
    
    auto results = plugin.performSearch("test", "dummy_search_test", options);
    
    assert(results.size() == 1);
    assert(results[0].line == 2);
    
    // Cleanup
    std::filesystem::remove_all("dummy_search_test");
    
    std::cout << "SearchPlugin tests passed!\n";
    return 0;
}
