// -----------------------------------------------------------------------------
// ExtensionInstallE2ETest -- the whole chain, from the archive to the running
// extension.
//
// The other tests check the parts: the archive is read safely, the host is
// told about the new extension. This one checks the chain that the user
// actually runs:
//
//   1. a `.vsix` file is written on the disk,
//   2. the extension is installed through the API of the core,
//   3. the core tells the extension host about it,
//   4. the *real* extension host wakes the extension up,
//   5. the extension calls back into the core (`vscode.workspace.fs`),
//   6. the core answers, and the file the extension asked for is there.
//
// The extension host is the real thing: the same process, the same bundle and
// the same starter that the editor uses. Nothing is faked, so a pass means an
// extension that was installed while the editor was open really runs.
// -----------------------------------------------------------------------------

#include "../../core/ipc/ExtensionHostSpawner.h"
#include "../../core/ipc/IPCClient.h"
#include "../../core/ipc/VSCodeProtocol.h"
#include "../../core/utils/Inflate.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

using vrutti::core::ipc::ExtensionHostSpawner;
using vrutti::core::ipc::IPCClient;
using vrutti::core::ipc::VSCodeProtocol;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) {
        std::cout << "  [PASS] " << message << "\n";
    } else {
        std::cerr << "  [FAIL] " << message << "\n";
        ++g_failures;
    }
}

// --- the archive -------------------------------------------------------------

void writeU16(std::ofstream& out, uint16_t value) {
    const char bytes[2] = {static_cast<char>(value & 0xFF),
                           static_cast<char>((value >> 8) & 0xFF)};
    out.write(bytes, 2);
}

void writeU32(std::ofstream& out, uint32_t value) {
    const char bytes[4] = {static_cast<char>(value & 0xFF),
                           static_cast<char>((value >> 8) & 0xFF),
                           static_cast<char>((value >> 16) & 0xFF),
                           static_cast<char>((value >> 24) & 0xFF)};
    out.write(bytes, 4);
}

uint32_t crc32Of(const std::string& content) {
    return vrutti::core::utils::crc32(
        reinterpret_cast<const uint8_t*>(content.data()), content.size());
}

// Writes a ZIP archive with the given files in it.
//
// The archive is built here, so that the test does not need a file that is
// kept in the repository.
bool writeZip(const std::string& path,
              const std::vector<std::pair<std::string, std::string>>& files) {
    struct Entry {
        std::string name;
        std::string content;
        uint32_t crc = 0;
        uint32_t offset = 0;
    };

    std::vector<Entry> entries;
    for (const auto& file : files) {
        Entry entry;
        entry.name = file.first;
        entry.content = file.second;
        entry.crc = crc32Of(entry.content);
        entries.push_back(entry);
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;

    uint32_t offset = 0;
    for (auto& entry : entries) {
        entry.offset = offset;
        const uint16_t nameLength = static_cast<uint16_t>(entry.name.size());
        writeU32(out, 0x04034b50);
        writeU16(out, 20);
        writeU16(out, 0);
        writeU16(out, 0);              // stored
        writeU16(out, 0);
        writeU16(out, 0);
        writeU32(out, entry.crc);
        writeU32(out, static_cast<uint32_t>(entry.content.size()));
        writeU32(out, static_cast<uint32_t>(entry.content.size()));
        writeU16(out, nameLength);
        writeU16(out, 0);
        out.write(entry.name.data(), nameLength);
        out.write(entry.content.data(), static_cast<std::streamsize>(entry.content.size()));
        offset += 30 + nameLength + static_cast<uint32_t>(entry.content.size());
    }

    const uint32_t directoryOffset = offset;
    for (const auto& entry : entries) {
        const uint16_t nameLength = static_cast<uint16_t>(entry.name.size());
        writeU32(out, 0x02014b50);
        writeU16(out, 20);
        writeU16(out, 20);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU32(out, entry.crc);
        writeU32(out, static_cast<uint32_t>(entry.content.size()));
        writeU32(out, static_cast<uint32_t>(entry.content.size()));
        writeU16(out, nameLength);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU32(out, 0);
        writeU32(out, entry.offset);
        out.write(entry.name.data(), nameLength);
    }
    const uint32_t directorySize = offset - directoryOffset;

    writeU32(out, 0x06054b50);
    writeU16(out, 0);
    writeU16(out, 0);
    writeU16(out, static_cast<uint16_t>(entries.size()));
    writeU16(out, static_cast<uint16_t>(entries.size()));
    writeU32(out, directorySize);
    writeU32(out, directoryOffset);
    writeU16(out, 0);
    return out.good();
}

// --- the extension -----------------------------------------------------------

// The extension of the test.
//
// It does two things when it is woken up: it registers a command, and it
// writes a file through `vscode.workspace.fs`. The file is written by the
// core, so its presence proves that the whole chain works.
std::string extensionSource(const std::string& markerPath, const std::string& commandId) {
    // The paths that go into the source of the extension use slashes: a
    // backslash in a string of javascript starts an escape, and a path like
    // `C:\Users\...` would turn into something else entirely.
    std::string marker = markerPath;
    for (char& c : marker) {
        if (c == '\\') c = '/';
    }

    std::string source;
    source += "const vscode = require('vscode');\n";
    source += "const fs = require('fs');\n";
    source += "const path = require('path');\n";
    source += "exports.activate = async function (context) {\n";
    source += "    const marker = '" + marker + "';\n";
    source += "    try {\n";
    source += "        await vscode.workspace.fs.writeFile(vscode.Uri.file(marker), Buffer.from('vrutti-e2e-ok'));\n";
    source += "    } catch (err) {\n";
    source += "        fs.writeFileSync(marker, 'write-file-failed:' + String(err));\n";
    source += "    }\n";
    source += "    const command = vscode.commands.registerCommand(" +
              std::string("'") + commandId + "', function () { return 'hello'; });\n";
    source += "    context.subscriptions.push(command);\n";
    source += "    fs.writeFileSync(path.join(__dirname, 'activated.txt'), 'activated');\n";
    source += "    return { exports: { hello: function () { return 'hello'; } } };\n";
    source += "};\n";
    source += "exports.deactivate = function () {};\n";
    return source;
}

} // namespace

int main() {
#ifdef _WIN32
    std::cout << "[ExtensionInstallE2ETest] A real extension, installed by the "
                 "core, running in the real extension host.\n\n";
#else
    std::cout << "[ExtensionInstallE2ETest] SKIPPED: the named pipe transport is"
                 " Windows only.\n";
    return 0;
#endif

    const fs::path root = fs::temp_directory_path() /
        ("vrutti_install_e2e_test_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const fs::path extensionsDir = root / "extensions";
    const fs::path workspace = root / "workspace";
    fs::create_directories(extensionsDir, ec);
    fs::create_directories(workspace, ec);

    _putenv_s("VRUTTI_EXTENSIONS_DIR", extensionsDir.string().c_str());
    _putenv_s("VRUTTI_BUILTIN_EXTENSIONS_DIR", (root / "builtin").string().c_str());

    // The file that the extension asks the core to write.
    const fs::path marker = root / "marker.txt";

    // The file that the extension writes itself: it proves that the code of
    // the extension really ran.
    const std::string commandId = "vrutti.e2e.ping";

    const std::string manifest =
        "{\"name\":\"writer\",\"publisher\":\"vrutti\",\"version\":\"1.0.0\","
        "\"displayName\":\"Writer\",\"description\":\"writes a file\","
        "\"engines\":{\"vscode\":\"^1.90.0\"},\"main\":\"./extension.js\","
        "\"activationEvents\":[\"onStartupFinished\"]}";

    const fs::path vsix = root / "writer.vsix";
    const bool archiveWritten = writeZip(vsix.string(), {
        {"extension/package.json", manifest},
        {"extension/extension.js", extensionSource(marker.string(), commandId)},
    });
    check(archiveWritten, "the archive of the extension was written");

    // --- the core and the host -------------------------------------------------
    const std::string pipeName = "vrutti_test_e2e_" + std::to_string(GetCurrentProcessId());

    IPCClient ipc(pipeName);
    ipc.start();

    VSCodeProtocol protocol(&ipc);
    protocol.setWorkspaceFolder(workspace.string());
    protocol.initialize();

    std::atomic<bool> handshakeDone{false};
    protocol.setOnHandshakeComplete([&handshakeDone]() { handshakeDone = true; });

    std::string spawnError;
    const bool spawned = ExtensionHostSpawner::spawn(pipeName, spawnError);
    check(spawned, "the real extension host was started (" + spawnError + ")");
    if (!spawned) {
        fs::remove_all(root, ec);
        return 1;
    }

    // The handshake has to be done before the extension is installed: the
    // host has to know the editor before it is told about a new extension.
    for (int i = 0; i < 300 && !handshakeDone; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    check(handshakeDone, "the extension host completed the start-up handshake");

    // --- the installation -------------------------------------------------------
    std::cout << "\n-- the installation\n";
    {
        auto params = std::make_shared<JsonNode>(JsonNode::Type::Object);
        params->objectProperties.emplace_back(
            "path", vrutti::core::utils::makeJsonString(vsix.string()));

        ipc.sendLocalRequest("extensions/install", params);
    }

    // The extension is woken up by the host, it writes its file and it asks
    // the core to write the marker. Both take a moment.
    bool markerWritten = false;
    bool activated = false;
    for (int i = 0; i < 600; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!markerWritten && fs::exists(marker)) markerWritten = true;
        if (!activated) {
            const fs::path extensionFolder = extensionsDir / "vrutti.writer-1.0.0";
            if (fs::exists(extensionFolder / "activated.txt")) activated = true;
        }
        if (markerWritten && activated) break;
    }

    check(fs::exists(extensionsDir / "vrutti.writer-1.0.0" / "package.json"),
          "the extension was unpacked in the folder of the extensions");
    check(activated,
          "the extension really ran: it wrote its own file when it was woken up");

    if (fs::exists(marker)) {
        std::ifstream input(marker, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(input)),
                            std::istreambuf_iterator<char>());
        check(content == "vrutti-e2e-ok",
              "the extension wrote its file through the core (got '" + content + "')");
    } else {
        check(false, "the extension wrote its file through the core: the file is missing");
    }

    // --- the list that the interface sees ---------------------------------------
    std::cout << "\n-- the list of the extensions\n";
    {
        bool found = false;
        for (const auto& entry : fs::directory_iterator(extensionsDir, ec)) {
            if (entry.path().filename() == "extensions.json") found = true;
        }
        check(found, "the list of the extensions was written");
    }

    ipc.sendTerminate();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ipc.stop();

    fs::remove_all(root, ec);

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[ExtensionInstallE2ETest] FAILED (" << g_failures <<
                  " assertion(s))\n";
        return 1;
    }
    std::cout << "[ExtensionInstallE2ETest] OK: an extension that was installed "
                 "runs in the real extension host\n";
    return 0;
}
