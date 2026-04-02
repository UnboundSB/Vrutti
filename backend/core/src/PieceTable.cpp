#include "PieceTable.h"

// The '::' tells the compiler that this method belongs to the PieceTable class
PieceTable::PieceTable(const std::string& initial_text) {
    
    // STEP 1: Save the text
    // Python: self.original_buffer = initial_text
    // C++ Hint: You don't need 'self.' Just assign the variable directly.
    original_buffer = initial_text;



    // STEP 2: Create the first piece and add it to the list
    // Python: self.pieces.append(Piece("Original", 0, len(initial_text)))
    // C++ Hint: Use pieces.push_back({...}) and BufferType::Original
    pieces.push_back({ BufferType::Original, 0, initial_text.length() });

    // STEP 3: Set the cached length
    // Python: self.cached_length = len(initial_text)
    // C++ Hint: Use initial_text.length()
    cached_length=initial_text.length();
    
}
// The 'const' here guarantees we won't accidentally change the table while reading it
std::string PieceTable::get_text() const {
    
    // STEP 1: Create an empty string to hold our final stitched text
    // Python: result = ""
    std::string result = "";

    // STEP 2: Loop through every piece in our 'pieces' list
    // Python: for piece in self.pieces:
    // C++ Hint: Use the 'const auto&' loop syntax
    for(const auto& piece : pieces){
         // STEP 3: Check which buffer this piece points to
        // Python: if piece.source == "Original":
        // C++ Hint: use piece.source == BufferType::Original
        if(piece.source == BufferType::Original){
// STEP 4: If original, slice the original_buffer and add it to result
            // Python: result += self.original_buffer[piece.start : piece.start + piece.length]
            // C++ Hint: use original_buffer.substr(piece.start, piece.length)
            result += original_buffer.substr(piece.start, piece.length);
            // STEP 5: Else, it must be the Add buffer. Slice that and add it to result.
        // Python: else: result += self.add_buffer[...]
        }
        else{
            result += add_buffer.substr(piece.start, piece.length);
        }
    }   
    // STEP 6: Return the final string
    // Python: return result
    return result;

    
}
size_t PieceTable::length() const {
    return cached_length;
}