#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace vrutti::core::editor {

    // -------------------------------------------------------------------------
    // A document that an extension can see.
    //
    // The extension host keeps its own mirror of a document, and the text of
    // the mirror is what an extension reads. The core owns the truth: this is
    // the document that the mirror is built from.
    // -------------------------------------------------------------------------
    struct Document {
        std::string scheme;      // "file" or "untitled"
        std::string uriPath;     // the path of the URI, for example "/c:/dir/file.txt"
        std::string fsPath;      // the native path, empty for an untitled document
        std::string languageId;  // the language of the document
        std::string encoding;    // the encoding of the text
        std::string eol;         // "\n" or "\r\n"
        std::string content;     // the text of the document
        int64_t versionId = 1;   // the version of the text, it grows with every edit
        bool isDirty = false;    // the document has changes that are not on disk

        // The key of the document: the scheme and the path of its URI.
        std::string key() const;
    };

    // -------------------------------------------------------------------------
    // The documents of the core.
    //
    // The store is the model side of the document contract. It reads the file
    // when a document is opened, it holds the text of an untitled document,
    // and it writes the file when a document is saved.
    //
    // The store is bounded: a document that has not been used for a while is
    // dropped, so a long session cannot grow without limit.
    // -------------------------------------------------------------------------
    class DocumentStore {
    public:
        DocumentStore() = default;

        // Opens a document from disk. The document is read into the store.
        //
        // Returns false when the file cannot be read. The reason is in
        // `error`, with the name of the failure in `errorCode`, for example
        // "FileNotFound".
        bool openFile(const std::string& uriPath,
                      const std::string& fsPath,
                      const std::string& encoding,
                      Document& out,
                      std::string& error,
                      std::string& errorCode);

        // Creates an untitled document. The content is what the caller passed,
        // and it may be empty.
        Document createUntitled(const std::string& languageId,
                                const std::string& content,
                                const std::string& encoding);

        // Writes the document to its file.
        //
        // An untitled document has no file yet, so it cannot be written. The
        // call fails with the reason in `error`.
        bool save(const std::string& key,
                  std::string& error,
                  std::string& errorCode);

        // The document of the key. The document is copied out, so the caller
        // holds a document even when the store changes.
        bool get(const std::string& key, Document& out) const;

        // Marks the document as changed and gives it a new version.
        bool setContent(const std::string& key, const std::string& content);

        // The keys of the documents that were dropped because the store was
        // full. The caller has to tell the extension host about them, so that
        // its mirror does not keep documents that no longer exist.
        std::vector<std::string> takeEvicted();

        // The language of a path, by its extension. "plaintext" when the
        // extension is not known.
        static std::string languageForPath(const std::string& path);

        // Splits text into the lines of a document and reports the end of line
        // that the text uses.
        static void splitLines(const std::string& text,
                               std::string& eol,
                               std::vector<std::string>& lines);

        // The number of documents the store holds.
        size_t size() const;

    private:
        // Drops the document that has not been used for the longest time.
        void evictIfNeeded();

        mutable std::mutex m_mutex;
        std::map<std::string, Document> m_documents;
        mutable uint64_t m_clock = 0;
        mutable std::map<std::string, uint64_t> m_used;
        std::vector<std::string> m_evicted;
        int m_untitledCounter = 0;
    };

} // namespace vrutti::core::editor
