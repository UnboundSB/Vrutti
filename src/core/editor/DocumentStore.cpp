#include "DocumentStore.h"
#include "../fs/FileService.h"
#include <algorithm>
#include <cctype>

namespace vrutti::core::editor {

namespace {

    // The number of documents the store holds. A document that has to be
    // dropped is the one that has not been used for the longest time.
    constexpr size_t MAX_DOCUMENTS = 64;

    // The encodings the core can read.
    constexpr const char* ENCODING_UTF8 = "utf8";
    constexpr const char* ENCODING_UTF8_BOM = "utf8bom";

    bool isUtf8(const std::string& encoding) {
        return encoding.empty() || encoding == ENCODING_UTF8 || encoding == ENCODING_UTF8_BOM;
    }

    std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    // The extension of a path, without the dot, in lower case.
    std::string extensionOf(const std::string& path) {
        const size_t slash = path.find_last_of("/\\");
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos) return "";
        if (slash != std::string::npos && dot < slash) return "";
        return toLower(path.substr(dot + 1));
    }

} // namespace

std::string Document::key() const {
    return scheme + ":" + uriPath;
}

// ---------------------------------------------------------------------------
// Opening a document
// ---------------------------------------------------------------------------

bool DocumentStore::openFile(const std::string& uriPath,
                             const std::string& fsPath,
                             const std::string& encoding,
                             Document& out,
                             std::string& error,
                             std::string& errorCode) {
    if (!isUtf8(encoding)) {
        error = "the encoding '" + encoding + "' is not supported";
        errorCode = "Unavailable";
        return false;
    }

    if (fsPath.empty()) {
        error = "the document has no path";
        errorCode = "FileNotFound";
        return false;
    }

    vrutti::core::fs::FileService files;
    std::vector<uint8_t> bytes;
    const auto result = files.readFile(fsPath, bytes);
    if (!result.ok) {
        error = result.errorMessage;
        errorCode = result.errorCode.empty() ? "FileNotFound" : result.errorCode;
        return false;
    }

    std::string text(bytes.begin(), bytes.end());

    // A byte order mark is not part of the text.
    std::string effectiveEncoding = encoding.empty() ? ENCODING_UTF8 : encoding;
    if (text.size() >= 3 &&
        static_cast<uint8_t>(text[0]) == 0xEF &&
        static_cast<uint8_t>(text[1]) == 0xBB &&
        static_cast<uint8_t>(text[2]) == 0xBF) {
        text = text.substr(3);
        effectiveEncoding = ENCODING_UTF8_BOM;
    }

    Document document;
    document.scheme = "file";
    document.uriPath = uriPath;
    document.fsPath = fsPath;
    document.languageId = languageForPath(fsPath);
    document.encoding = effectiveEncoding;
    document.content = text;
    document.versionId = 1;
    document.isDirty = false;
    {
        std::vector<std::string> lines;
        splitLines(text, document.eol, lines);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const std::string key = document.key();
        auto it = m_documents.find(key);
        if (it != m_documents.end()) {
            // The document is already open. The text on disk is the truth, so
            // the copy in the store is refreshed.
            it->second.content = document.content;
            it->second.eol = document.eol;
            it->second.encoding = document.encoding;
            it->second.languageId = document.languageId;
            it->second.versionId = document.versionId;
            it->second.isDirty = false;
            m_used[key] = ++m_clock;
            out = it->second;
            return true;
        }
        m_documents[key] = document;
        m_used[key] = ++m_clock;
        out = document;
    }

    std::vector<std::string> evicted;
    evictIfNeeded();
    return true;
}

// ---------------------------------------------------------------------------
// Creating an untitled document
// ---------------------------------------------------------------------------

Document DocumentStore::createUntitled(const std::string& languageId,
                                       const std::string& content,
                                       const std::string& encoding) {
    Document document;
    document.scheme = "untitled";
    document.languageId = languageId.empty() ? "plaintext" : languageId;
    document.encoding = encoding.empty() ? ENCODING_UTF8 : encoding;
    document.content = content;
    document.versionId = 1;
    // A document that was never written is dirty: it only exists in memory.
    document.isDirty = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        document.uriPath = "Untitled-" + std::to_string(++m_untitledCounter);
        const std::string key = document.key();
        m_documents[key] = document;
        m_used[key] = ++m_clock;
    }

    std::string eol;
    std::vector<std::string> lines;
    splitLines(document.content, eol, lines);
    document.eol = eol;

    std::vector<std::string> evicted;
    evictIfNeeded();
    return document;
}

// ---------------------------------------------------------------------------
// Saving a document
// ---------------------------------------------------------------------------

bool DocumentStore::save(const std::string& key,
                         std::string& error,
                         std::string& errorCode) {
    std::string path;
    std::string content;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_documents.find(key);
        if (it == m_documents.end()) {
            error = "the document is not open";
            errorCode = "FileNotFound";
            return false;
        }
        if (it->second.fsPath.empty()) {
            error = "the document has no file yet";
            errorCode = "FileNotFound";
            return false;
        }
        path = it->second.fsPath;
        content = it->second.content;
    }

    vrutti::core::fs::FileService files;
    const std::vector<uint8_t> bytes(content.begin(), content.end());
    const auto result = files.writeFile(path, bytes);
    if (!result.ok) {
        error = result.errorMessage;
        errorCode = result.errorCode.empty() ? "Unavailable" : result.errorCode;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_documents.find(key);
        if (it != m_documents.end()) {
            it->second.isDirty = false;
            m_used[key] = ++m_clock;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Reading and changing a document
// ---------------------------------------------------------------------------

bool DocumentStore::get(const std::string& key, Document& out) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_documents.find(key);
    if (it == m_documents.end()) return false;
    m_used[key] = ++m_clock;
    out = it->second;
    return true;
}

bool DocumentStore::setContent(const std::string& key, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_documents.find(key);
    if (it == m_documents.end()) return false;
    it->second.content = content;
    it->second.versionId += 1;
    it->second.isDirty = true;
    std::string eol;
    std::vector<std::string> lines;
    splitLines(content, eol, lines);
    it->second.eol = eol;
    m_used[key] = ++m_clock;
    return true;
}

std::vector<std::string> DocumentStore::takeEvicted() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> evicted;
    for (const auto& entry : m_evicted) {
        evicted.push_back(entry);
    }
    m_evicted.clear();
    return evicted;
}

size_t DocumentStore::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_documents.size();
}

void DocumentStore::evictIfNeeded() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (m_documents.size() > MAX_DOCUMENTS) {
        // The document that has not been used for the longest time goes.
        auto oldest = m_used.begin();
        for (auto it = m_used.begin(); it != m_used.end(); ++it) {
            if (it->second < oldest->second) oldest = it;
        }
        if (oldest == m_used.end()) break;
        m_documents.erase(oldest->first);
        m_used.erase(oldest);
        m_evicted.push_back(oldest->first);
    }
}

// ---------------------------------------------------------------------------
// The language of a path
// ---------------------------------------------------------------------------

std::string DocumentStore::languageForPath(const std::string& path) {
    const std::string extension = extensionOf(path);
    if (extension == "cpp" || extension == "cc" || extension == "cxx" ||
        extension == "h" || extension == "hpp" || extension == "hh") {
        return "cpp";
    }
    if (extension == "js" || extension == "mjs" || extension == "cjs" ||
        extension == "jsx" || extension == "ts" || extension == "tsx") {
        return "javascript";
    }
    if (extension == "json") return "json";
    if (extension == "py" || extension == "pyi") return "python";
    if (extension == "md" || extension == "markdown") return "markdown";
    if (extension == "rs") return "rust";
    if (extension == "go") return "go";
    if (extension == "java") return "java";
    if (extension == "cs") return "csharp";
    if (extension == "rb") return "ruby";
    if (extension == "sh" || extension == "bash") return "shellscript";
    if (extension == "html" || extension == "htm") return "html";
    if (extension == "css") return "css";
    if (extension == "yml" || extension == "yaml") return "yaml";
    if (extension == "toml") return "toml";
    if (extension == "xml") return "xml";
    return "plaintext";
}

// ---------------------------------------------------------------------------
// The lines of a document
// ---------------------------------------------------------------------------

void DocumentStore::splitLines(const std::string& text,
                               std::string& eol,
                               std::vector<std::string>& lines) {
    eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";

    lines.clear();
    std::string current;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                lines.push_back(current);
                current.clear();
                ++i;
                continue;
            }
            // A lone carriage return ends the line as well.
            lines.push_back(current);
            current.clear();
            continue;
        }
        if (text[i] == '\n') {
            lines.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(text[i]);
    }
    lines.push_back(current);
}

} // namespace vrutti::core::editor
