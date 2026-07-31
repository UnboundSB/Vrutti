#include <stdexcept>
#include <string>
#include <functional>
#include "../src/core/fs/URI.h"
#include "../src/core/fs/Path.h"
#include "../src/core/fs/Glob.h"
#include "../src/core/utils/StringPool.h"
#include "../src/core/utils/LineScanner.h"

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        throw std::runtime_error(std::string("Assertion failed: ") + #condition + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

#define ASSERT_EQ(expected, actual) \
    if ((std::string(expected)) != (std::string(actual))) { \
        throw std::runtime_error(std::string("Assertion failed: ") + std::string(expected) + " == " + std::string(actual) + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
    }

extern void run_test(const std::string& name, std::function<void()> test_func);

namespace {
    void test_fs_uri() {
        vrutti::core::fs::URI uri("file:///usr/local/bin?debug=true#line=12");
        ASSERT_EQ("file", uri.scheme());
        ASSERT_EQ("/usr/local/bin", uri.path());
        ASSERT_EQ("debug=true", uri.query());
        ASSERT_EQ("line=12", uri.fragment());
    }

    void test_fs_path() {
        ASSERT_EQ("baz.txt", vrutti::core::fs::Path::basename("foo/bar/baz.txt"));
        // Basic check, might differ on Windows vs Linux but the implementation uses C++17 filesystem.
    }

    void test_fs_glob() {
        ASSERT_TRUE(vrutti::core::fs::Glob::match("*.txt", "foo.txt"));
    }

    void test_utils_stringpool() {
        vrutti::core::utils::StringPool pool;
        auto s1 = pool.intern("hello");
        auto s2 = pool.intern("hello");
        auto s3 = pool.intern("world");
        
        ASSERT_TRUE(s1.data() == s2.data()); // Same memory address
        ASSERT_TRUE(s1.data() != s3.data());
    }

    void test_utils_linescanner() {
        std::string text = "A\nB\r\nC";
        auto starts = vrutti::core::utils::LineScanner::computeLineStarts(text);
        ASSERT_TRUE(starts.size() == 3);
        ASSERT_TRUE(starts[0] == 0);
    }
}

void run_fs_utils_tests() {
    run_test("FS - URI", test_fs_uri);
    run_test("FS - Path", test_fs_path);
    run_test("FS - Glob", test_fs_glob);
    run_test("Utils - StringPool", test_utils_stringpool);
    run_test("Utils - LineScanner", test_utils_linescanner);
}
