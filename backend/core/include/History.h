#pragma once
#include <string>
#include <stack>
#include <stdexcept>

// The two possible things a user can do
enum class ActionType {
    Insert,
    Delete
};

// The Struct: A lightweight O(1) snapshot of an event
struct Action {
    ActionType type;
    size_t index;
    std::string text; // If Inserted, the text added. If Deleted, the text removed.
};

class HistoryManager {
private:
    std::stack<Action> undo_stack;
    std::stack<Action> redo_stack;

public:
    // Records a new event and destroys the "redo" future
    void record(ActionType type, size_t index, const std::string& text);
    
    // Safety checks
    bool can_undo() const;
    bool can_redo() const;
    
    // Time travel commands
    Action pop_undo();
    Action pop_redo();
    
    // Clears all history (useful for full document wipes)
    void clear();
};