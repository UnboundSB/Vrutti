#include "History.h"

void HistoryManager::record(ActionType type, size_t index, const std::string& text) {
    // 1. Save the action to the past
    undo_stack.push({type, index, text});
    
    // 2. The Prime Timeline Rule: If you do something new, you destroy the alternate future.
    while (!redo_stack.empty()) {
        redo_stack.pop();
    }
}

bool HistoryManager::can_undo() const {
    return !undo_stack.empty();
}

bool HistoryManager::can_redo() const {
    return !redo_stack.empty();
}

Action HistoryManager::pop_undo() {
    if (undo_stack.empty()) {
        throw std::runtime_error("Nothing to undo.");
    }
    
    // Grab the most recent past action
    Action action = undo_stack.top();
    undo_stack.pop();
    
    // Push it to the alternate future
    redo_stack.push(action);
    
    return action;
}

Action HistoryManager::pop_redo() {
    if (redo_stack.empty()) {
        throw std::runtime_error("Nothing to redo.");
    }
    
    // Grab the most recent alternate future action
    Action action = redo_stack.top();
    redo_stack.pop();
    
    // Push it back to the past
    undo_stack.push(action);
    
    return action;
}

void HistoryManager::clear() {
    while (!undo_stack.empty()) undo_stack.pop();
    while (!redo_stack.empty()) redo_stack.pop();
}