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
void PieceTable::insert_text(size_t index, const std::string& text) {
    // If they try to insert nothing, just return early.
    if (text.empty()) return;

    // STEP 1: Save the current length of the add_buffer before we modify it.
    // This is where our new piece will start!
    // Python: add_start = len(add_buffer)
    size_t add_start =  add_buffer.length();

    // STEP 2: Append the new text to the very end of the add_buffer
    // Python: add_buffer += text
    add_buffer.append(text);

    

    // STEP 3: Update our cached document length
    // Python: cached_length += len(text)
    cached_length+=text.length();


    // If they are inserting at the exact end of the document, we don't need to split anything!
    if (index == cached_length - text.length()) {
        pieces.push_back({BufferType::Add, add_start, text.length()});
        return;
    }

    // --- THE HUNT ---
    size_t current_offset = 0;
    
    // We use a standard index loop here because we need 'i' for the Iterators later
    for (size_t i = 0; i < pieces.size(); ++i) {
        
        // Copy the target piece so we can read it easily
        Piece target = pieces[i];

        // Did we find the piece that contains our target index?
        if (current_offset + target.length > index) {
            
            // MATH TIME. 
            // STEP 4: Calculate the split point
            // Python: split_point = index - current_offset
            size_t split_point = index - current_offset;

            // STEP 5: Create the three new pieces using the {} struct syntax
            // Remember: { source, start, length }
            
            // Left Piece: Same source, same start, length is the split point
            Piece left_piece = { target.source, target.start, split_point };
            
            // New Piece: Source is Add, start is where we appended, length is new text
            Piece new_piece = { BufferType::Add, add_start, text.length() };
            
            // Right Piece: Same source, start is shifted by split point, length is remainder
            Piece right_piece = { target.source, target.start + split_point, target.length - split_point };


            // ==========================================
            // C++ MAGIC: Replace the old piece with the new ones
            // (I wrote this part for you!)
            // ==========================================
            
            // 1. Delete the old target piece
            auto it = pieces.erase(pieces.begin() + i);

            // 2. Insert Right, then New, then Left 
            // (We insert backwards because 'insert' pushes elements to the right)
            if (right_piece.length > 0) {
                it = pieces.insert(it, right_piece);
            }
            it = pieces.insert(it, new_piece);
            
            if (left_piece.length > 0) {
                pieces.insert(it, left_piece);
            }

            // We found it and split it, so stop looping!
            break;
        }

        // If this wasn't the piece, add its length to our running total and check the next one
        current_offset += target.length;
    }
}