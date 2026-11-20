# editor (src/core/editor/)

## Purpose
Editor core: the text buffer, the piece table and the document model.

## Files
- **PieceTable.cpp**: Implementation of the PieceTable component.
- **PieceTable.h**: Header for the PieceTable component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **PieceTable::PieceTable(const std::string&, size_t)**
- **PieceTable::~PieceTable()**
- **PieceTable::insert(size_t, const std::string&)**
- **PieceTable::remove(size_t, size_t)**
- **std::string PieceTable::getText()**
- **std::string PieceTable::substring(size_t, size_t)**
- **size_t PieceTable::length()**
- **PieceTable::garbageCollect()**
- **TreeNode::init(const Piece&)**
