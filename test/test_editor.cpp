#include <stdexcept>
#include <string>
#include <fstream>
#include <functional>
#include "../src/core/editor/PieceTable.h"

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + (expected) + " == " + (actual) + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

extern void run_test(const std::string& name, std::function<void()> test_func);

namespace {
    std::string create_temp_file(const std::string& content) {
        std::string path = "test_piece_table.txt";
        std::ofstream out(path, std::ios::binary);
        out << content;
        return path;
    }

    void test_piecetable_init() {
        std::string content = "Hello World";
        std::string path = create_temp_file(content);
        
        vrutti::core::editor::PieceTable table(path, content.length());
        ASSERT_EQ(content, table.getText());
        ASSERT_TRUE(table.length() == content.length());
    }

    void test_piecetable_insert() {
        std::string content = "Hello World";
        std::string path = create_temp_file(content);
        
        vrutti::core::editor::PieceTable table(path, content.length());
        table.insert(5, " Beautiful");
        ASSERT_EQ(std::string("Hello Beautiful World"), table.getText());
        
        table.insert(table.length(), "!");
        ASSERT_EQ(std::string("Hello Beautiful World!"), table.getText());
        
        table.insert(0, "Well ");
        ASSERT_EQ(std::string("Well Hello Beautiful World!"), table.getText());
    }

    void test_piecetable_remove() {
        std::string content = "Hello Beautiful World!";
        std::string path = create_temp_file(content);
        
        vrutti::core::editor::PieceTable table(path, content.length());
        table.remove(5, 10); // Remove " Beautiful"
        ASSERT_EQ(std::string("Hello World!"), table.getText());
        
        table.remove(table.length() - 1, 1); // Remove "!"
        ASSERT_EQ(std::string("Hello World"), table.getText());
        
        table.remove(0, 6); // Remove "Hello "
        ASSERT_EQ(std::string("World"), table.getText());
    }

    void test_piecetable_substring() {
        std::string content = "The quick brown fox";
        std::string path = create_temp_file(content);
        
        vrutti::core::editor::PieceTable table(path, content.length());
        ASSERT_EQ(std::string("quick"), table.substring(4, 5));
        
        table.insert(16, "lazy ");
        // "The quick brown lazy fox"
        ASSERT_EQ(std::string("lazy fox"), table.substring(16, 8));
    }
}

void run_editor_tests() {
    run_test("PieceTable - Init", test_piecetable_init);
    run_test("PieceTable - Insert", test_piecetable_insert);
    run_test("PieceTable - Remove", test_piecetable_remove);
    run_test("PieceTable - Substring", test_piecetable_substring);
}
