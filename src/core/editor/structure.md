# editor (src/core/editor/)

## Purpose
Editor core: the text buffer, the piece table and the document model.

## Files
- **DocumentStore.cpp**: Implementation of the DocumentStore component.
- **DocumentStore.h**: Header for the DocumentStore component.
- **PieceTable.cpp**: Implementation of the PieceTable component.
- **PieceTable.h**: Header for the PieceTable component.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **std::string Document::key()**
- **DocumentStore::DocumentStore()**
- **bool DocumentStore::openFile(const std::string&, const std::string&, const std::string&, Document&, std::string&, std::string&)**
- **Document DocumentStore::createUntitled(const std::string&, const std::string&, const std::string&)**
- **bool DocumentStore::save(const std::string&, std::string&, std::string&)**
- **bool DocumentStore::get(const std::string&, Document&)**
- **bool DocumentStore::setContent(const std::string&, const std::string&)**
- **std::vector<std::string> DocumentStore::takeEvicted()**
- **std::string DocumentStore::languageForPath(const std::string&)**
- **DocumentStore::splitLines(const std::string&, std::string&, std::vector<std::string>&)**
- **size_t DocumentStore::size()**
- **PieceTable::PieceTable(const std::string&, size_t)**
- **PieceTable::~PieceTable()**
- **PieceTable::insert(size_t, const std::string&)**
- **PieceTable::remove(size_t, size_t)**
- **std::string PieceTable::getText()**
- **std::string PieceTable::substring(size_t, size_t)**
- **size_t PieceTable::length()**
- **PieceTable::garbageCollect()**
- **TreeNode::init(const Piece&)**
