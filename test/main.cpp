#include <iostream>
#include <string>
#include <vector>
#include <functional>

int g_tests_passed = 0;
int g_tests_failed = 0;

void run_test(const std::string& name, std::function<void()> test_func) {
    try {
        test_func();
        std::cout << "[PASS] " << name << std::endl;
        g_tests_passed++;
    } catch (const std::exception& e) {
        std::cout << "[FAIL] " << name << " - Exception: " << e.what() << std::endl;
        g_tests_failed++;
    } catch (...) {
        std::cout << "[FAIL] " << name << " - Unknown exception" << std::endl;
        g_tests_failed++;
    }
}

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #expected + " == " + #actual + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

// Forward declarations of test suites
void run_memory_tests();
void run_concurrency_tests();
void run_editor_tests();
void run_fs_utils_tests();

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "       VRUTTI IDE - TEST RUNNER         " << std::endl;
    std::cout << "========================================" << std::endl;

    // We will uncomment these as we implement them
    // run_memory_tests();
    // run_concurrency_tests();
    // run_editor_tests();
    // run_fs_utils_tests();

    std::cout << "========================================" << std::endl;
    std::cout << "Tests Passed: " << g_tests_passed << std::endl;
    std::cout << "Tests Failed: " << g_tests_failed << std::endl;
    std::cout << "========================================" << std::endl;

    return g_tests_failed > 0 ? 1 : 0;
}
