#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <stdexcept>

// ==========================================
// 1. Types & Data Structures
// ==========================================

// 'enum class' is strongly typed. It prevents you from accidentally 
// comparing 'Original' to an integer, which is a common source of bugs in older C.
enum class BufferType {
    Original,
    Add
};

// The core memory pointer. Lightweight, fast, and simple.
struct Piece {
    BufferType source;
    size_t start;
    size_t length;
};

// ==========================================
// 2. The Engine Blueprint
// ==========================================

class PieceTable {
private:
    // The memory blocks
    std::string original_buffer;
    std::string add_buffer;
    
    // The brain/map
    std::vector<Piece> pieces;
    
    // Optimization: We cache the total length here.
    // This allows length() to be O(1) instant instead of O(N) loop calculation.
    size_t cached_length;

public:
    // Constructor: 'explicit' prevents Python/C++ from accidentally converting 
    // a random string into a PieceTable object behind your back.
    explicit PieceTable(const std::string& initial_text);

    // ------------------------------------------
    // Core Mutation Operations
    // ------------------------------------------
    void insert_text(size_t index, const std::string& text);
    void delete_text(size_t index, size_t length);

    // ------------------------------------------
    // State Retrieval
    // ------------------------------------------
    // [[nodiscard]] is a C++ safety feature. If you call get_text() 
    // but forget to assign it to a variable, the compiler will throw an error.
    // The 'const' at the end guarantees this method will NEVER modify the buffers.
    [[nodiscard]] std::string get_text() const;
    [[nodiscard]] size_t length() const;
};