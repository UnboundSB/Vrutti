#include <iostream>
#include <cassert>
#include <string>
#include "../src/core/fs/Workspace.h"

using namespace vrutti::core::fs;

// Minimalist, zero-bloat testing macro
#define ASSERT_TEST(condition, msg) \
    if (!(condition)) { \
        std::cerr << "[FAIL] " << msg << std::endl; \
        return false; \
    } else { \
        std::cout << "[PASS] " << msg << std::endl; \
    }

bool testWorkspaceEdgeCases() {
    std::cout << "--- Testing Workspace Edge Cases ---" << std::endl;
    
    // Test 1: Invalid Path (Failing behavior)
    Workspace ws("C:/invalid_path_that_does_not_exist_vrutti_123");
    FileNode& root = ws.getRootMutable();
    
    // Attempting to scan a bad directory must not crash the editor
    ws.scanDirectory(root);
    ASSERT_TEST(root.children.empty(), "Invalid path should result in empty children without crashing.");
    // The try/catch in Workspace.cpp catches the filesystem error and skips setting isScanned to true.
    ASSERT_TEST(!root.isScanned, "Invalid path should not mark node as successfully scanned.");

    // Test 2: Unload unscanned node (Edge case)
    ws.unloadDirectory(root);
    ASSERT_TEST(!root.isScanned, "Unloading an unscanned/empty node should be perfectly safe.");
    
    return true;
}

bool testIntegration() {
    std::cout << "--- Testing Integration (Deep Lazy Loading Memory Unload) ---" << std::endl;

    // Test 3: Load real path, verify state, unload (Integration)
    // We use the current directory '.' as a guaranteed valid path.
    Workspace ws(".");
    FileNode& root = ws.getRootMutable();
    
    ws.scanDirectory(root);
    ASSERT_TEST(root.isScanned, "Scanning valid directory '.' sets isScanned to true.");
    
    // We expect some files/folders in the current project directory (like CMakeLists.txt)
    bool hasChildren = !root.children.empty();
    ASSERT_TEST(hasChildren, "Valid directory '.' should successfully populate children vector.");

    // Verify the deep lazy loading optimization
    ws.unloadDirectory(root);
    ASSERT_TEST(!root.isScanned, "Unloading the directory sets isScanned to false.");
    ASSERT_TEST(root.children.empty(), "Unloading instantly clears the children array (freeing RAM).");

    return true;
}

int main() {
    std::cout << "=== Vrutti IDE Rigorous Native Test Suite ===" << std::endl;

    bool allPassed = true;
    allPassed &= testWorkspaceEdgeCases();
    allPassed &= testIntegration();

    if (allPassed) {
        std::cout << "=== ALL TESTS PASSED ===" << std::endl;
        return 0;
    } else {
        std::cerr << "=== SOME TESTS FAILED ===" << std::endl;
        return 1;
    }
}
