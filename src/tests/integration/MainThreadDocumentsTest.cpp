// -----------------------------------------------------------------------------
// MainThreadDocumentsTest -- proves that an extension can open, create and
// save a text document.
//
// The surface behind `workspace.openTextDocument()` and `workspace.textDocuments`
// is the `MainThreadDocuments` actor of the extension host protocol:
//
//     workspace.openTextDocument(uri)  -> MainThreadDocuments.$tryOpenDocument
//     workspace.openTextDocument()     -> MainThreadDocuments.$tryCreateDocument
//     document.save()                  -> MainThreadDocuments.$trySaveDocument
//
// The text of a document lives in the core. The extension host keeps its own
// mirror of it, and that mirror is fed by the delta of
// `ExtHostDocumentsAndEditors.$acceptDocumentsAndEditorsDelta`. The mirror is
// what an extension reads, so the test checks the delta as well: the text has
// to reach the host, and it has to reach it *before* the call is answered.
//
// The test drives the *real* router over the *real* named pipe, so it covers
// the frame that actually reaches an extension.
// -----------------------------------------------------------------------------

#include "../../core/ipc/IPCClient.h"
#include "../../core/ipc/VSCodeProtocol.h"
#include "../../core/ipc/VsCodeProxyIds.generated.h"
#include "ExtHostPipeClient.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

using vrutti::core::ipc::IPCClient;
using vrutti::core::ipc::VSCodeProtocol;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;
using vrutti::tests::PipeClient;
namespace proxy = vrutti::core::ipc::proxy;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) {
        std::cout << "  [PASS] " << message << "\n";
    } else {
        std::cerr << "  [FAIL] " << message << "\n";
        g_failures++;
    }
}

// The URI of a native path, the way the extension host sends it.
std::string fileUri(const std::string& path) {
    std::string p = path;
    for (char& c : p) {
        if (c == '\\') c = '/';
    }
    if (p.size() >= 2 && p[1] == ':') {
        p[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(p[0])));
        p = "/" + p;
    }
    return "{\"scheme\":\"file\",\"authority\":\"\",\"path\":\"" + p +
           "\",\"query\":\"\",\"fragment\":\"\"}";
}

// A request that the core sent to the host.
struct HostRequest {
    std::string method;
    int reqId = 0;
    std::shared_ptr<JsonNode> args;
};

// Reads frames until the reply for `reqId` arrives.
//
// The frames that the core sends in the meantime are answered, the way the
// extension host answers them: a request gets a reply with no payload. The
// requests are collected, so the test can look at what the core asked for.
std::shared_ptr<JsonNode> awaitReplyServing(PipeClient& client,
                                             int reqId,
                                             int timeoutMs,
                                             std::vector<HostRequest>& served) {
#ifdef _WIN32
    const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
    std::string line;
    while (GetTickCount() < deadline) {
        if (!client.readLine(line, 1000)) continue;
        if (line.empty()) continue;
        auto frame = JsonParser::parse(line);
        if (!frame || frame->type != JsonNode::Type::Object) continue;

        auto typeNode = frame->get("type");
        if (!typeNode || typeNode->type != JsonNode::Type::String) continue;
        const std::string type(typeNode->stringValue.data(), typeNode->stringValue.length());

        if (type == "ack") continue;

        auto reqIdNode = frame->get("reqId");
        if (!reqIdNode || reqIdNode->type != JsonNode::Type::Number) continue;
        const int frameReqId = static_cast<int>(reqIdNode->numberValue);

        if (type == "request") {
            HostRequest request;
            request.reqId = frameReqId;
            request.method = vrutti::tests::stringField(frame, "method");
            request.args = frame->get("args");
            served.push_back(request);

            // The host answers the request of the core.
            client.sendLine("{\"type\":\"reply\",\"reqId\":" +
                            std::to_string(frameReqId) + ",\"res\":null}");
            continue;
        }

        if (frameReqId != reqId) continue;
        return frame;
    }
    return nullptr;
#else
    (void)client; (void)reqId; (void)timeoutMs; (void)served;
    return nullptr;
#endif
}

// Sends one request to the core and returns the reply frame.
std::shared_ptr<JsonNode> call(PipeClient& client,
                               int reqId,
                               int proxyId,
                               const std::string& method,
                               const std::string& argsJson,
                               std::vector<HostRequest>& served) {
    const std::string request = "{\"type\":\"request\",\"reqId\":" + std::to_string(reqId) +
                                ",\"proxyId\":" + std::to_string(proxyId) +
                                ",\"method\":\"" + method + "\"" +
                                ",\"args\":" + argsJson + "}";
    if (!client.sendLine(request)) return nullptr;
    return awaitReplyServing(client, reqId, 5000, served);
}

// The document of a delta that the core sent.
std::shared_ptr<JsonNode> deltaDocument(const HostRequest& request) {
    if (!request.args || request.args->type != JsonNode::Type::Array) return nullptr;
    if (request.args->arrayElements.empty()) return nullptr;
    auto delta = request.args->arrayElements[0];
    if (!delta || delta->type != JsonNode::Type::Object) return nullptr;
    auto added = delta->get("addedDocuments");
    if (!added || added->type != JsonNode::Type::Array) return nullptr;
    if (added->arrayElements.empty()) return nullptr;
    return added->arrayElements[0];
}

// The lines of a document of a delta.
std::vector<std::string> deltaLines(const std::shared_ptr<JsonNode>& document) {
    std::vector<std::string> lines;
    if (!document) return lines;
    auto node = document->get("lines");
    if (!node || node->type != JsonNode::Type::Array) return lines;
    for (const auto& line : node->arrayElements) {
        if (!line || line->type != JsonNode::Type::String) continue;
        lines.push_back(std::string(line->stringValue.data(), line->stringValue.length()));
    }
    return lines;
}

std::string readWholeFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

// A flag of an object.
bool boolField(const std::shared_ptr<JsonNode>& node, const char* name) {
    if (!node || node->type != JsonNode::Type::Object) return false;
    auto value = node->get(name);
    if (!value || value->type != JsonNode::Type::Boolean) return false;
    return value->boolValue;
}

// The number of the requests the core sent that carry a method.
size_t countRequests(const std::vector<HostRequest>& served, const std::string& method) {
    size_t count = 0;
    for (const auto& request : served) {
        if (request.method == method) count++;
    }
    return count;
}

} // namespace

int main() {
#ifdef _WIN32
    std::cout << "[MainThreadDocumentsTest] An extension must be able to open,"
                 " create and save a text document.\n\n";
#else
    std::cout << "[MainThreadDocumentsTest] SKIPPED: the named pipe transport is"
                 " Windows only.\n";
    return 0;
#endif

    // A folder of its own, so the test never touches a file of the user.
    const fs::path root = fs::temp_directory_path() /
        ("vrutti_doc_test_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const std::string pipeName = "vrutti_test_docs_" + std::to_string(GetCurrentProcessId());

    IPCClient ipc(pipeName);
    ipc.start();

    VSCodeProtocol protocol(&ipc);
    protocol.initialize();

    PipeClient client;
    if (!client.connect(pipeName, 5000)) {
        std::cerr << "  [FAIL] could not connect to the test pipe\n";
        return 1;
    }

    int nextReqId = 3000;
    auto nextId = [&]() { return nextReqId++; };
    std::vector<HostRequest> served;

    // --- 1. open a document ----------------------------------------------------
    {
        const fs::path target = root / "hello.txt";
        {
            std::ofstream out(target, std::ios::binary);
            out << "line one\nline two\n";
        }

        const size_t before = served.size();
        auto frame = call(client, nextId(), proxy::Main::MainThreadDocuments,
                          "$tryOpenDocument", "[" + fileUri(target.string()) + "]",
                          served);
        check(frame != nullptr, "the open call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the open call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");

        auto res = frame ? frame->get("res") : nullptr;
        check(res && res->type == JsonNode::Type::Object,
              "the open call answered with the URI of the document");
        check(vrutti::tests::stringField(res, "path").find("hello.txt") != std::string::npos,
              "the open call answered with the URI of the file (got " +
              vrutti::tests::stringField(res, "path") + ")");

        // The text has to reach the host, and it has to reach it before the
        // call is answered: the host resolves `openTextDocument()` only when
        // the document is in its mirror.
        check(served.size() > before,
              "the core sent the document to the host before it answered");

        bool found = false;
        for (size_t i = before; i < served.size(); i++) {
            if (served[i].method != "$acceptDocumentsAndEditorsDelta") continue;
            auto document = deltaDocument(served[i]);
            if (!document) continue;
            found = true;
            const auto lines = deltaLines(document);
            check(lines.size() == 3,
                  "the document that reached the host has three lines (got " +
                  std::to_string(lines.size()) + ")");
            if (lines.size() == 3) {
                check(lines[0] == "line one" && lines[1] == "line two" && lines[2] == "",
                      "the document that reached the host has the text of the file");
            }
            check(vrutti::tests::stringField(document, "EOL") == "\n",
                  "the document that reached the host has the end of line of the file");
            check(vrutti::tests::stringField(document, "languageId") == "plaintext",
                  "the document that reached the host has the language of the file (got " +
                  vrutti::tests::stringField(document, "languageId") + ")");
            check(vrutti::tests::numberField(document, "versionId") == 1,
                  "the document that reached the host has its first version");
            check(vrutti::tests::stringField(document, "encoding") == "utf8",
                  "the document that reached the host has its encoding");
        }
        check(found, "the core sent the document with the delta of the documents");
    }

    // --- 2. the language of a file ---------------------------------------------
    {
        const fs::path target = root / "code.cpp";
        {
            std::ofstream out(target, std::ios::binary);
            out << "int main() { return 0; }\n";
        }

        const size_t before = served.size();
        call(client, nextId(), proxy::Main::MainThreadDocuments,
             "$tryOpenDocument", "[" + fileUri(target.string()) + "]", served);

        bool found = false;
        for (size_t i = before; i < served.size(); i++) {
            if (served[i].method != "$acceptDocumentsAndEditorsDelta") continue;
            auto document = deltaDocument(served[i]);
            if (!document) continue;
            found = true;
            check(vrutti::tests::stringField(document, "languageId") == "cpp",
                  "a C++ file is opened with the language cpp (got " +
                  vrutti::tests::stringField(document, "languageId") + ")");
        }
        check(found, "the C++ file reached the host");
    }

    // --- 3. create an untitled document ----------------------------------------
    {
        const size_t before = served.size();
        auto frame = call(client, nextId(), proxy::Main::MainThreadDocuments,
                          "$tryCreateDocument",
                          "[{\"language\":\"javascript\",\"content\":\"const x = 1;\\n\"}]",
                          served);
        check(frame != nullptr, "the create call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the create call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");

        auto res = frame ? frame->get("res") : nullptr;
        check(res && res->type == JsonNode::Type::Object,
              "the create call answered with the URI of the new document");
        check(vrutti::tests::stringField(res, "scheme") == "untitled",
              "the new document is an untitled one (got " +
              vrutti::tests::stringField(res, "scheme") + ")");
        check(!vrutti::tests::stringField(res, "path").empty(),
              "the new document has a path of its own");

        bool found = false;
        for (size_t i = before; i < served.size(); i++) {
            if (served[i].method != "$acceptDocumentsAndEditorsDelta") continue;
            auto document = deltaDocument(served[i]);
            if (!document) continue;
            found = true;
            const auto lines = deltaLines(document);
            check(lines.size() == 2,
                  "the new document has the lines of its content (got " +
                  std::to_string(lines.size()) + ")");
            if (lines.size() == 2) {
                check(lines[0] == "const x = 1;", "the new document has the content it was given");
            }
            check(vrutti::tests::stringField(document, "languageId") == "javascript",
                  "the new document has the language it was given (got " +
                  vrutti::tests::stringField(document, "languageId") + ")");
            check(boolField(document, "isDirty"),
                  "the new document is dirty: it was never written");
        }
        check(found, "the new document reached the host");
    }

    // --- 4. save a document ----------------------------------------------------
    {
        const fs::path target = root / "saved.txt";
        {
            std::ofstream out(target, std::ios::binary);
            out << "the content of the document\n";
        }

        const size_t before = served.size();
        call(client, nextId(), proxy::Main::MainThreadDocuments,
             "$tryOpenDocument", "[" + fileUri(target.string()) + "]", served);

        // Something else changes the file behind the back of the core. The
        // save has to put the content of the document back.
        {
            std::ofstream out(target, std::ios::binary);
            out << "tampered\n";
        }

        auto frame = call(client, nextId(), proxy::Main::MainThreadDocuments,
                          "$trySaveDocument", "[" + fileUri(target.string()) + "]", served);
        check(frame != nullptr, "the save call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the save call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(vrutti::tests::resultKind(frame) == "true",
              "the save call answered true (got " +
              vrutti::tests::resultKind(frame) + ")");
        check(readWholeFile(target.string()) == "the content of the document\n",
              "the file holds the content of the document again after the save");
        check(countRequests(served, "$acceptModelSaved") == 1,
              "the core told the host that the document was saved");
    }

    // --- 5. save a document that was never opened -------------------------------
    {
        const fs::path target = root / "never-opened.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadDocuments,
                          "$trySaveDocument", "[" + fileUri(target.string()) + "]", served);
        check(frame != nullptr, "the save of an unknown document was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the save of an unknown document is not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(vrutti::tests::resultKind(frame) == "false",
              "the save of an unknown document answers false (got " +
              vrutti::tests::resultKind(frame) + ")");
        check(!fs::exists(target), "the save of an unknown document wrote nothing");
    }

    // --- 6. open a file that is not there ---------------------------------------
    {
        const fs::path target = root / "missing.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadDocuments,
                          "$tryOpenDocument", "[" + fileUri(target.string()) + "]", served);
        check(frame != nullptr, "the open of a missing file was answered");
        check(vrutti::tests::frameType(frame) == "replyErr",
              "the open of a missing file fails, as it must (got " +
              vrutti::tests::frameType(frame) + ")");
    }

    // --- 7. open a URI that is not a URI ----------------------------------------
    {
        auto frame = call(client, nextId(), proxy::Main::MainThreadDocuments,
                          "$tryOpenDocument", "[{\"path\":\"/no/scheme\"}]", served);
        check(frame != nullptr, "the open of a broken URI was answered");
        check(vrutti::tests::frameType(frame) == "replyErr",
              "the open of a broken URI fails, as it must (got " +
              vrutti::tests::frameType(frame) + ")");
        check(vrutti::tests::errorMessage(frame).find("Invalid uri") != std::string::npos,
              "the open of a broken URI says what is wrong (got " +
              vrutti::tests::errorMessage(frame) + ")");
    }

    // --- 8. open a file with an encoding the core cannot read --------------------
    {
        const fs::path target = root / "encoded.txt";
        {
            std::ofstream out(target, std::ios::binary);
            out << "text\n";
        }
        auto frame = call(client, nextId(), proxy::Main::MainThreadDocuments,
                          "$tryOpenDocument",
                          "[" + fileUri(target.string()) + ",{\"encoding\":\"utf16le\"}]",
                          served);
        check(frame != nullptr, "the open with an unknown encoding was answered");
        check(vrutti::tests::frameType(frame) == "replyErr",
              "the open with an encoding the core cannot read fails, as it must (got " +
              vrutti::tests::frameType(frame) + ")");
    }

    // --- 9. the host is never left waiting --------------------------------------
    {
        // Every call above was answered. A call that is never answered leaves
        // an extension waiting forever, so the test checks the count.
        check(served.size() >= 5,
              "the core sent the documents to the host (got " +
              std::to_string(served.size()) + ")");
    }

    client.disconnect();
    ipc.stop();

    fs::remove_all(root, ec);

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[MainThreadDocumentsTest] FAILED (" << g_failures << " assertion(s))\n";
        return 1;
    }
    std::cout << "[MainThreadDocumentsTest] OK: an extension can open, create and save a document\n";
    return 0;
}
