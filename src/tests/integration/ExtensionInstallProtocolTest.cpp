// -----------------------------------------------------------------------------
// ExtensionInstallProtocolTest -- proves that an extension that is installed
// while the editor is open reaches the extension host.
//
// The extension host keeps a registry of the extensions it knows. When the
// list of the extensions is only sent at the start-up, an extension that is
// installed afterwards is not there until the editor is started again.
//
// The host learns about the change through
// `ExtHostExtensionService.$deltaExtensions`, the call of the upstream
// protocol that carries the extensions that were added and the ones that
// were removed.
//
// The test drives the *real* router over the *real* named pipe, so it covers
// the frame that actually reaches an extension.
// -----------------------------------------------------------------------------

#include "../../core/ipc/IPCClient.h"
#include "../../core/ipc/VSCodeProtocol.h"
#include "../../core/ipc/VsCodeProxyIds.generated.h"
#include "../../core/utils/Inflate.h"
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
        ++g_failures;
    }
}

// A request that the core sent to the host.
struct HostRequest {
    std::string method;
    int proxyId = 0;
    int reqId = 0;
    std::shared_ptr<JsonNode> args;
};

// Reads the frames until one of them is wanted.
//
// Every request of the core is answered, the way the extension host answers
// it, so that the core is never left waiting.
std::shared_ptr<JsonNode> readUntil(PipeClient& client,
                                     std::vector<HostRequest>& served,
                                     const std::function<bool(const HostRequest&)>& wanted,
                                     int timeoutMs) {
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
            auto proxyNode = frame->get("proxyId");
            if (proxyNode && proxyNode->type == JsonNode::Type::Number) {
                request.proxyId = static_cast<int>(proxyNode->numberValue);
            }
            request.args = frame->get("args");
            served.push_back(request);

            // The host answers the request of the core.
            client.sendLine("{\"type\":\"reply\",\"reqId\":" +
                            std::to_string(frameReqId) + ",\"res\":null}");
            if (wanted(request)) return frame;
            continue;
        }
    }
#else
    (void)client; (void)served; (void)wanted; (void)timeoutMs;
#endif
    return nullptr;
}

// Writes a small ZIP archive with one file in it.
//
// The archive is built here, so that the test does not need a file that is
// kept in the repository.
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

// Writes a `.vsix` file with a manifest and an entry point.
bool writeVsix(const std::string& path, const std::string& name,
               const std::string& version) {
    const std::string manifest =
        "{\"name\":\"" + name + "\",\"publisher\":\"vrutti\",\"version\":\"" + version +
        "\",\"displayName\":\"" + name + "\",\"engines\":{\"vscode\":\"^1.90.0\"},"
        "\"main\":\"./extension.js\",\"activationEvents\":[\"onStartupFinished\"]}";
    const std::string entryPoint = "exports.activate = () => {};\n";

    struct Entry { std::string name; std::string content; uint32_t crc; uint32_t offset; };
    std::vector<Entry> entries;
    entries.push_back({"extension/package.json", manifest, crc32Of(manifest), 0});
    entries.push_back({"extension/extension.js", entryPoint, crc32Of(entryPoint), 0});

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

// The delta of the extensions that the core sent.
std::shared_ptr<JsonNode> deltaOf(const HostRequest& request) {
    if (!request.args || request.args->type != JsonNode::Type::Array) return nullptr;
    if (request.args->arrayElements.empty()) return nullptr;
    return request.args->arrayElements[0];
}

} // namespace

int main() {
#ifdef _WIN32
    std::cout << "[ExtensionInstallProtocolTest] An extension that is installed "
                 "while the editor is open must reach the extension host.\n\n";
#else
    std::cout << "[ExtensionInstallProtocolTest] SKIPPED: the named pipe transport is"
                 " Windows only.\n";
    return 0;
#endif

    const fs::path root = fs::temp_directory_path() /
        ("vrutti_install_protocol_test_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const fs::path extensionsDir = root / "extensions";
    fs::create_directories(extensionsDir, ec);
#ifdef _WIN32
    _putenv_s("VRUTTI_EXTENSIONS_DIR", extensionsDir.string().c_str());
    _putenv_s("VRUTTI_BUILTIN_EXTENSIONS_DIR", (root / "builtin").string().c_str());
#else
    setenv("VRUTTI_EXTENSIONS_DIR", extensionsDir.string().c_str(), 1);
    setenv("VRUTTI_BUILTIN_EXTENSIONS_DIR", (root / "builtin").string().c_str(), 1);
#endif

    const fs::path vsix = root / "hello.vsix";
    check(writeVsix(vsix.string(), "hello", "1.0.0"), "the extension of the test was written");

    const std::string pipeName = "vrutti_test_install_" + std::to_string(GetCurrentProcessId());
    IPCClient ipc(pipeName);
    ipc.start();

    VSCodeProtocol protocol(&ipc);
    protocol.initialize();

    // The events that the core sends to the user interface.
    std::vector<std::pair<std::string, std::shared_ptr<JsonNode>>> uiEvents;
    ipc.setOnEvent([&uiEvents](const std::string& method,
                               const std::shared_ptr<JsonNode>& params) {
        uiEvents.emplace_back(method, params);
    });

    PipeClient client;
    if (!client.connect(pipeName, 5000)) {
        std::cerr << "  [FAIL] could not connect to the test pipe\n";
        return 1;
    }

    std::vector<HostRequest> served;

    // --- 1. the installation ----------------------------------------------------
    std::cout << "-- the installation\n";
    {
        auto params = std::make_shared<JsonNode>(JsonNode::Type::Object);
        params->objectProperties.emplace_back(
            "path", vrutti::core::utils::makeJsonString(vsix.string()));

        const size_t before = served.size();
        ipc.sendLocalRequest("extensions/install", params);

        // The core has to tell the host about the new extension.
        readUntil(client, served, [](const HostRequest& request) {
            return request.method == "$deltaExtensions";
        }, 5000);

        bool found = false;
        for (size_t i = before; i < served.size(); ++i) {
            if (served[i].method != "$deltaExtensions") continue;
            found = true;
            check(served[i].proxyId == proxy::Ext::ExtHostExtensionService,
                  "the delta goes to the extension service of the host");

            auto delta = deltaOf(served[i]);
            check(delta && delta->type == JsonNode::Type::Object,
                  "the delta is an object");
            if (!delta) continue;

            check(vrutti::tests::numberField(delta, "versionId", 0) >= 1,
                  "the delta has a version");

            auto toAdd = delta->get("toAdd");
            check(toAdd && toAdd->type == JsonNode::Type::Array &&
                  toAdd->arrayElements.size() == 1,
                  "the delta holds the extension that was installed");
            if (toAdd && !toAdd->arrayElements.empty()) {
                const auto& description = toAdd->arrayElements[0];
                auto identifier = description->get("identifier");
                check(identifier && identifier->type == JsonNode::Type::Object,
                      "the description of the extension has an identifier");
                if (identifier) {
                    check(vrutti::tests::stringField(identifier, "value") == "vrutti.hello",
                          "the extension of the delta is vrutti.hello (got " +
                          vrutti::tests::stringField(identifier, "value") + ")");
                    check(vrutti::tests::stringField(identifier, "_lower") == "vrutti.hello",
                          "the lower case of the identifier is there as well");
                }
                check(description->get("extensionLocation") != nullptr,
                      "the description has the folder of the extension");
                check(description->get("activationEvents") != nullptr,
                      "the description has the events that wake the extension up");
            }

            auto myToAdd = delta->get("myToAdd");
            check(myToAdd && myToAdd->type == JsonNode::Type::Array &&
                  myToAdd->arrayElements.size() == 1,
                  "the extension is one of the extensions of the user");

            auto activationEvents = delta->get("addActivationEvents");
            check(activationEvents && activationEvents->type == JsonNode::Type::Object &&
                  activationEvents->get("vrutti.hello") != nullptr,
                  "the delta carries the events that wake the extension up");

            auto toRemove = delta->get("toRemove");
            check(toRemove && toRemove->type == JsonNode::Type::Array &&
                  toRemove->arrayElements.empty(),
                  "the delta removes nothing");
        }
        check(found, "the core told the host about the new extension");
    }

    // --- 2. the extension is on the disk ---------------------------------------
    std::cout << "\n-- the extension is on the disk\n";
    {
        check(fs::exists(extensionsDir / "vrutti.hello-1.0.0" / "package.json"),
              "the extension was unpacked in the folder of the extensions");
    }

    // --- 3. the user interface is told -----------------------------------------
    std::cout << "\n-- the user interface is told\n";
    {
        bool found = false;
        for (const auto& event : uiEvents) {
            if (event.first != "extensions/installed") continue;
            found = true;
            check(event.second && event.second->type == JsonNode::Type::Array,
                  "the list of the extensions is an array: that is what the "
                  "interface expects");
            if (event.second && event.second->type == JsonNode::Type::Array &&
                !event.second->arrayElements.empty()) {
                const auto& first = event.second->arrayElements[0];
                check(vrutti::tests::stringField(first, "id") == "vrutti.hello",
                      "the list holds the extension that was installed (got " +
                      vrutti::tests::stringField(first, "id") + ")");
                check(!vrutti::tests::stringField(first, "localPath").empty(),
                      "the list has the folder of the extension");
            }
        }
        check(found, "the interface was told about the extensions");
    }

    // --- 4. turning the extension off ------------------------------------------
    std::cout << "\n-- turning the extension off\n";
    {
        auto params = std::make_shared<JsonNode>(JsonNode::Type::Object);
        params->objectProperties.emplace_back(
            "id", vrutti::core::utils::makeJsonString("vrutti.hello"));

        const size_t before = served.size();
        ipc.sendLocalRequest("extensions/disable", params);
        readUntil(client, served, [](const HostRequest& request) {
            return request.method == "$deltaExtensions";
        }, 5000);

        bool found = false;
        for (size_t i = before; i < served.size(); ++i) {
            if (served[i].method != "$deltaExtensions") continue;
            found = true;
            auto delta = deltaOf(served[i]);
            if (!delta) continue;
            auto toRemove = delta->get("toRemove");
            check(toRemove && toRemove->type == JsonNode::Type::Array &&
                  toRemove->arrayElements.size() == 1,
                  "the delta removes the extension that was turned off");
            auto toAdd = delta->get("toAdd");
            check(toAdd && toAdd->type == JsonNode::Type::Array &&
                  toAdd->arrayElements.empty(),
                  "the delta adds nothing");
        }
        check(found, "the core told the host that the extension is gone");
        check(fs::exists(extensionsDir / "vrutti.hello-1.0.0"),
              "the extension is still on the disk: it is only turned off");
    }

    // --- 5. the removal ---------------------------------------------------------
    std::cout << "\n-- the removal\n";
    {
        auto params = std::make_shared<JsonNode>(JsonNode::Type::Object);
        params->objectProperties.emplace_back(
            "id", vrutti::core::utils::makeJsonString("vrutti.hello"));

        const size_t before = served.size();
        ipc.sendLocalRequest("extensions/uninstall", params);
        readUntil(client, served, [](const HostRequest& request) {
            return request.method == "$deltaExtensions";
        }, 5000);

        bool found = false;
        for (size_t i = before; i < served.size(); ++i) {
            if (served[i].method != "$deltaExtensions") continue;
            found = true;
        }
        check(found, "the core told the host that the extension is gone");
        check(!fs::exists(extensionsDir / "vrutti.hello-1.0.0"),
              "the folder of the extension is gone");
    }

    // --- 6. an installation that fails ------------------------------------------
    std::cout << "\n-- an installation that fails\n";
    {
        const fs::path broken = root / "broken.vsix";
        {
            std::ofstream out(broken, std::ios::binary);
            out << "this is not an extension";
        }

        const size_t eventsBefore = uiEvents.size();
        auto params = std::make_shared<JsonNode>(JsonNode::Type::Object);
        params->objectProperties.emplace_back(
            "path", vrutti::core::utils::makeJsonString(broken.string()));
        ipc.sendLocalRequest("extensions/install", params);

        // The core answers the interface with the reason.
        bool found = false;
        for (size_t i = eventsBefore; i < uiEvents.size(); ++i) {
            if (uiEvents[i].first != "extensions/error") continue;
            found = true;
            check(!vrutti::tests::stringField(uiEvents[i].second, "message").empty(),
                  "the interface is told what is wrong");
        }
        check(found, "the interface was told that the installation did not work");
    }

    client.disconnect();
    ipc.stop();

    fs::remove_all(root, ec);

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[ExtensionInstallProtocolTest] FAILED (" << g_failures <<
                  " assertion(s))\n";
        return 1;
    }
    std::cout << "[ExtensionInstallProtocolTest] OK: an extension that is installed "
                 "reaches the extension host\n";
    return 0;
}
