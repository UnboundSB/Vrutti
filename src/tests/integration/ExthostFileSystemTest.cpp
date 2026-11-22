// -----------------------------------------------------------------------------
// ExthostFileSystemTest -- proves that an extension can use `vscode.workspace.fs`.
//
// The surface behind `workspace.fs` is the `MainThreadFileSystem` actor of
// the extension host protocol. The extension host calls it whenever an
// extension reads or writes a file:
//
//     workspace.fs.stat(uri)        -> MainThreadFileSystem.$stat
//     workspace.fs.readDirectory()  -> MainThreadFileSystem.$readdir
//     workspace.fs.readFile()       -> MainThreadFileSystem.$readFile
//     workspace.fs.writeFile()      -> MainThreadFileSystem.$writeFile
//     workspace.fs.createDirectory()-> MainThreadFileSystem.$mkdir
//     workspace.fs.delete()         -> MainThreadFileSystem.$delete
//     workspace.fs.rename()         -> MainThreadFileSystem.$rename
//     workspace.fs.copy()           -> MainThreadFileSystem.$copy
//
// Without these calls an extension cannot touch a file at all, so they are
// among the most important of the whole contract.
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

// Sends one request and returns the reply frame.
std::shared_ptr<JsonNode> call(PipeClient& client, int reqId, int proxyId,
                               const std::string& method, const std::string& argsJson) {
    const std::string request = "{\"type\":\"request\",\"reqId\":" + std::to_string(reqId) +
                                ",\"proxyId\":" + std::to_string(proxyId) +
                                ",\"method\":\"" + method + "\"" +
                                ",\"args\":" + argsJson + "}";
    if (!client.sendLine(request)) return nullptr;
    return vrutti::tests::awaitReply(client, reqId, 5000);
}

std::string readWholeFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main() {
#ifdef _WIN32
    std::cout << "[ExthostFileSystemTest] An extension must be able to use"
                 " `vscode.workspace.fs`.\n\n";
#else
    std::cout << "[ExthostFileSystemTest] SKIPPED: the named pipe transport is"
                 " Windows only.\n";
    return 0;
#endif

    // A folder of its own, so the test never touches a file of the user.
    const fs::path root = fs::temp_directory_path() /
        ("vrutti_fs_test_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const std::string pipeName = "vrutti_test_fs_" + std::to_string(GetCurrentProcessId());

    IPCClient ipc(pipeName);
    ipc.start();

    VSCodeProtocol protocol(&ipc);
    protocol.initialize();

    PipeClient client;
    if (!client.connect(pipeName, 5000)) {
        std::cerr << "  [FAIL] could not connect to the test pipe\n";
        return 1;
    }

    int nextReqId = 2000;
    auto nextId = [&]() { return nextReqId++; };

    // --- 1. write a file ------------------------------------------------------
    {
        const fs::path target = root / "hello.txt";
        const std::string content = "hello from an extension\n";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$writeFile",
                          "[" + fileUri(target.string()) + "," +
                          vrutti::tests::makeBufferArgument(content) + "]");
        check(frame != nullptr, "the write call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the write call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(fs::exists(target), "the file was written to the disk");
        const std::string written = readWholeFile(target.string());
        check(written == content,
              "the file holds the content that was written (got " +
              std::to_string(written.size()) + " bytes, expected " +
              std::to_string(content.size()) + ")");
    }

    // --- 2. read the file back ------------------------------------------------
    {
        const fs::path target = root / "hello.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$readFile", "[" + fileUri(target.string()) + "]");
        check(frame != nullptr, "the read call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the read call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(vrutti::tests::decodeBufferResult(frame) == "hello from an extension\n",
              "the read call returned the content of the file (got " +
              std::to_string(vrutti::tests::decodeBufferResult(frame).size()) + " bytes)");
    }

    // --- 3. the stat of a file ------------------------------------------------
    {
        const fs::path target = root / "hello.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$stat", "[" + fileUri(target.string()) + "]");
        check(frame != nullptr, "the stat call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the stat call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");

        auto res = frame ? frame->get("res") : nullptr;
        // FileType.File is 1.
        check(vrutti::tests::numberField(res, "type") == 1,
              "the stat of a file has the type of a file");
        check(vrutti::tests::numberField(res, "size") == 24,
              "the stat of a file has its size (got " +
              std::to_string(vrutti::tests::numberField(res, "size", -1)) + ")");
        check(vrutti::tests::numberField(res, "mtime", 0) > 0,
              "the stat of a file has the time it was changed");
    }

    // --- 4. the stat of a folder ----------------------------------------------
    {
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$stat", "[" + fileUri(root.string()) + "]");
        auto res = frame ? frame->get("res") : nullptr;
        // FileType.Directory is 2.
        check(vrutti::tests::numberField(res, "type") == 2,
              "the stat of a folder has the type of a folder");
    }

    // --- 5. the stat of a file that is not there ------------------------------
    {
        const fs::path target = root / "missing.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$stat", "[" + fileUri(target.string()) + "]");
        check(frame != nullptr, "the stat of a missing file was answered");
        check(vrutti::tests::frameType(frame) == "replyErr",
              "the stat of a missing file fails, as it must (got " +
              vrutti::tests::frameType(frame) + ")");
        // The extension host maps the name onto its own file system error.
        check(vrutti::tests::errorName(frame) == "FileNotFound",
              "the stat of a missing file reports the code FileNotFound (got " +
              vrutti::tests::errorName(frame) + ")");
    }

    // --- 6. the entries of a folder -------------------------------------------
    {
        fs::create_directories(root / "sub", ec);
        std::ofstream(root / "b.txt") << "b";
        std::ofstream(root / "a.txt") << "a";

        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$readdir", "[" + fileUri(root.string()) + "]");
        check(frame != nullptr, "the list call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the list call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");

        auto res = frame ? frame->get("res") : nullptr;
        check(res && res->type == JsonNode::Type::Array,
              "the list call answered with a list");
        if (res && res->type == JsonNode::Type::Array) {
            bool hasA = false, hasB = false, hasFolder = false;
            for (const auto& entry : res->arrayElements) {
                if (!entry || entry->type != JsonNode::Type::Array) continue;
                if (entry->arrayElements.size() < 2) continue;
                auto nameNode = entry->arrayElements[0];
                auto typeNode = entry->arrayElements[1];
                if (!nameNode || nameNode->type != JsonNode::Type::String) continue;
                const std::string name(nameNode->stringValue.data(), nameNode->stringValue.length());
                const double type = typeNode ? typeNode->numberValue : -1;
                if (name == "a.txt") hasA = true;
                if (name == "b.txt") hasB = true;
                if (name == "sub" && type == 2) hasFolder = true;
            }
            check(hasA, "the list holds the file a.txt");
            check(hasB, "the list holds the file b.txt");
            check(hasFolder, "the list holds the folder sub, with the type of a folder");
        }
    }

    // --- 7. create a folder ----------------------------------------------------
    {
        const fs::path folder = root / "created";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$mkdir", "[" + fileUri(folder.string()) + "]");
        check(frame != nullptr, "the folder call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the folder call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(fs::is_directory(folder), "the folder was created");
    }

    // --- 8. create a folder that is already there ------------------------------
    {
        const fs::path folder = root / "created";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$mkdir", "[" + fileUri(folder.string()) + "]");
        check(vrutti::tests::frameType(frame) == "replyErr",
              "creating a folder that exists fails, as it must (got " +
              vrutti::tests::frameType(frame) + ")");
        check(vrutti::tests::errorName(frame) == "FileExists",
              "creating a folder that exists reports the code FileExists (got " +
              vrutti::tests::errorName(frame) + ")");
    }

    // --- 9. rename a file ------------------------------------------------------
    {
        const fs::path from = root / "a.txt";
        const fs::path to = root / "renamed.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$rename",
                          "[" + fileUri(from.string()) + "," + fileUri(to.string()) +
                          ",{\"overwrite\":false}]");
        check(frame != nullptr, "the rename call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the rename call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(!fs::exists(from) && fs::exists(to), "the file was renamed");
    }

    // --- 10. copy a file -------------------------------------------------------
    {
        const fs::path from = root / "renamed.txt";
        const fs::path to = root / "copied.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$copy",
                          "[" + fileUri(from.string()) + "," + fileUri(to.string()) +
                          ",{\"overwrite\":false}]");
        check(frame != nullptr, "the copy call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the copy call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(fs::exists(from) && fs::exists(to), "the file was copied");
        check(readWholeFile(to.string()) == "a",
              "the copy holds the content of the original");
    }

    // --- 11. delete a file -----------------------------------------------------
    {
        const fs::path target = root / "copied.txt";
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$delete",
                          "[" + fileUri(target.string()) + ",{\"recursive\":false}]");
        check(frame != nullptr, "the delete call was answered");
        check(vrutti::tests::frameType(frame) == "reply",
              "the delete call was answered with a reply, not an error (got " +
              vrutti::tests::frameType(frame) + ")");
        check(!fs::exists(target), "the file was deleted");
    }

    // --- 12. a scheme that no file system can serve ----------------------------
    {
        auto frame = call(client, nextId(), proxy::Main::MainThreadFileSystem,
                          "$stat",
                          "[{\"scheme\":\"nosuchscheme\",\"authority\":\"\",\"path\":\"/x\","
                          "\"query\":\"\",\"fragment\":\"\"}]");
        check(frame != nullptr, "the call for an unknown scheme was answered");
        check(vrutti::tests::frameType(frame) == "replyErr",
              "the call for an unknown scheme fails, as it must (got " +
              vrutti::tests::frameType(frame) + ")");
        check(vrutti::tests::errorName(frame) == "Unavailable",
              "the call for an unknown scheme reports the code Unavailable (got " +
              vrutti::tests::errorName(frame) + ")");
    }

    client.disconnect();
    ipc.stop();

    fs::remove_all(root, ec);

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[ExthostFileSystemTest] FAILED (" << g_failures << " assertion(s))\n";
        return 1;
    }
    std::cout << "[ExthostFileSystemTest] OK: an extension can use the file system\n";
    return 0;
}
