#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include "core/editor/PieceTable.h"
#include "core/utils/StringPool.h"
#include "core/utils/LineScanner.h"

using namespace vrutti::core;

void testStringPool() {
    std::cout << "Running StringPool Tests..." << std::endl;
    utils::StringPool pool;
    
    std::string_view a = pool.intern("hello");
    std::string_view b = pool.intern("hello");
    std::string_view c = pool.intern("world");
    
    assert(a.data() == b.data()); // Pointers must be identical for identical strings
    assert(a.data() != c.data());
    std::cout << "StringPool OK." << std::endl;
}

void testLineScanner() {
    std::cout << "Running LineScanner Tests..." << std::endl;
    std::string text = "Line1\nLine2\n\nLine4";
    auto starts = utils::LineScanner::computeLineStarts(text);
    
    assert(starts.size() == 4);
    assert(starts[0] == 0);
    assert(starts[1] == 6);
    assert(starts[2] == 12);
    assert(starts[3] == 13);
    std::cout << "LineScanner OK." << std::endl;
}

void testPieceTable() {
    std::cout << "Running PieceTable Tests..." << std::endl;
    
    // Create a temporary file for lazy loading test
    std::string testFile = "temp_test_file.txt";
    std::string initialContent = "Hello World! This is a test file for the PieceTable.";
    {
        std::ofstream out(testFile);
        out << initialContent;
    }
    
    {
        editor::PieceTable pt(testFile, initialContent.length());
        
        // 1. Initial Load Test
        assert(pt.length() == initialContent.length());
        assert(pt.getText() == initialContent);
        
        // 2. Insert Test
        pt.insert(5, " Beautiful");
        std::string expected = "Hello Beautiful World! This is a test file for the PieceTable.";
        assert(pt.getText() == expected);
        
        // 3. Remove Test (across pieces)
        pt.remove(16, 7); // Remove " World!"
        expected = "Hello Beautiful This is a test file for the PieceTable.";
        if (pt.getText() != expected) {
            std::cerr << "Remove test failed!\nExpected: " << expected << "\nActual:   " << pt.getText() << std::endl;
            assert(false);
        }
        
        // 4. Boundary Tests
        pt.insert(pt.length(), " End.");
        expected += " End.";
        assert(pt.getText() == expected);
        
        pt.insert(0, "Start. ");
        expected = "Start. " + expected;
        if (pt.getText() != expected) {
            std::cerr << "Boundary test failed!\nExpected: " << expected << "\nActual:   " << pt.getText() << std::endl;
            assert(false);
        }
    }
    
    // Cleanup
    std::filesystem::remove(testFile);
    std::cout << "PieceTable OK." << std::endl;
}

#include "ui/compositor/Window.h"

int main() {
    std::cout << "--- Vrutti Core Rigorous Test Suite ---" << std::endl;
    
    try {
        testStringPool();
        testLineScanner();
        testPieceTable();
        std::cout << "ALL FAILSAFES AND TESTS PASSED SUCCESSFULLY!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "FAILSAFE TRIGGERED: " << e.what() << std::endl;
        return 1;
    }
    
    // Launch UI
    vrutti::ui::Window window(1280, 720, "Vrutti IDE");
    if (!window.init()) {
        std::cerr << "Failed to initialize UI window!" << std::endl;
        return 1;
    }

    // Currently we just run the window loop
    // Further integration with EditorView will happen here
    window.run();

    return 0;
}
