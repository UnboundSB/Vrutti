// -----------------------------------------------------------------------------
// ProtocolFallbackTest -- proves the contract of the extension host router.
//
// THE CONTRACT
// ------------
// The extension host calls the renderer through the `MainThread*` actors. A
// call that Vrutti does not implement must NOT be answered with an error.
//
// Why: the host turns an error reply into a rejected promise
// (`rpcProtocol.ts`: `if (value.$isError) ... pendingReply.resolveErr(err)`).
// Most of those calls are made without `await` (for example
// `extHostSearch.ts`: `this._proxy.$registerTextSearchProvider(...)`), so the
// rejection becomes an *unhandled rejection*. The host reports those as an
// extension error, and the extension is shown as broken.
//
// A missing feature is therefore answered with the benign default of the
// declared return type: `null`, `""`, `0`, `false` or `[]`.
//
// The test drives the *real* router over the *real* named pipe, so it covers
// the frame that actually reaches an extension:
//   * a call without an implementation  -> `reply` with the default
//   * a call with an implementation      -> `reply` with the real value
//   * never                              -> `replyErr`
//
// The test does not hard-code which calls Vrutti implements. It asks the
// router, so the test keeps its meaning as the backend grows.
// -----------------------------------------------------------------------------

#include "../../core/ipc/IPCClient.h"
#include "../../core/ipc/VSCodeProtocol.h"
#include "../../core/ipc/VsCodeProxyIds.generated.h"
#include "../../core/utils/Json.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using vrutti::core::ipc::IPCClient;
using vrutti::core::ipc::VSCodeProtocol;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;
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

// -----------------------------------------------------------------------------
// The pipe client -- the test plays the part of the extension host.
// -----------------------------------------------------------------------------

class PipeClient {
public:
    PipeClient() = default;
    ~PipeClient() { disconnect(); }

    // Connects to the named pipe the core is listening on.
    bool connect(const std::string& pipeName, int timeoutMs) {
#ifdef _WIN32
        std::string path = "\\\\.\\pipe\\" + pipeName;
        DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
        while (GetTickCount() < deadline) {
            HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                   0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                m_handle = h;
                return true;
            }
            Sleep(20);
        }
        return false;
#else
        (void)pipeName; (void)timeoutMs;
        return false;
#endif
    }

    void disconnect() {
#ifdef _WIN32
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
#endif
    }

    bool sendLine(const std::string& line) {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        std::string payload = line + "\n";
        DWORD written = 0;
        return WriteFile(m_handle, payload.c_str(),
                         static_cast<DWORD>(payload.size()), &written, nullptr) != FALSE;
#else
        return false;
#endif
    }

    // Reads the next complete frame (one JSON object on one line).
    bool readLine(std::string& out, int timeoutMs) {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
        while (GetTickCount() < deadline) {
            size_t pos = m_pending.find('\n');
            if (pos != std::string::npos) {
                out = m_pending.substr(0, pos);
                m_pending.erase(0, pos + 1);
                return true;
            }
            DWORD available = 0;
            if (PeekNamedPipe(m_handle, nullptr, 0, nullptr, &available, nullptr)
                && available > 0) {
                char buffer[4096];
                DWORD toRead = available < sizeof(buffer) ? available : sizeof(buffer);
                DWORD got = 0;
                if (ReadFile(m_handle, buffer, toRead, &got, nullptr) && got > 0) {
                    m_pending.append(buffer, got);
                    continue;
                }
            }
            Sleep(5);
        }
        return false;
#endif
    }

private:
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#endif
    std::string m_pending;
};

// Reads frames until the reply for `reqId` arrives. Returns the parsed reply
// frame, or nullptr when the peer answered with something else.
std::shared_ptr<JsonNode> awaitReply(PipeClient& client, int reqId, int timeoutMs) {
    DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
    std::string line;
    while (GetTickCount() < deadline) {
        if (!client.readLine(line, 1000)) continue;
        if (line.empty()) continue;
        auto frame = JsonParser::parse(line);
        if (!frame || frame->type != JsonNode::Type::Object) continue;

        auto typeNode = frame->get("type");
        if (!typeNode || typeNode->type != JsonNode::Type::String) continue;
        std::string type(typeNode->stringValue.data(), typeNode->stringValue.length());

        if (type == "ack") continue;

        auto reqIdNode = frame->get("reqId");
        if (!reqIdNode || reqIdNode->type != JsonNode::Type::Number) continue;
        if (static_cast<int>(reqIdNode->numberValue) != reqId) continue;

        return frame;
    }
    return nullptr;
}

std::string frameType(const std::shared_ptr<JsonNode>& frame) {
    if (!frame) return "<none>";
    auto node = frame->get("type");
    if (!node || node->type != JsonNode::Type::String) return "<none>";
    return std::string(node->stringValue.data(), node->stringValue.length());
}

// The kind of the `res` payload of a reply.
std::string resultKind(const std::shared_ptr<JsonNode>& frame) {
    if (!frame) return "<none>";
    auto res = frame->get("res");
    if (!res) return "<missing>";
    switch (res->type) {
        case JsonNode::Type::Null:    return "null";
        case JsonNode::Type::Boolean: return res->boolValue ? "true" : "false";
        case JsonNode::Type::Number:  return "number";
        case JsonNode::Type::String:  return "string";
        case JsonNode::Type::Array:   return "array";
        case JsonNode::Type::Object:  return "object";
    }
    return "<unknown>";
}

// The kind of answer a call without an implementation must receive, derived
// from the declared return type of the upstream protocol.
enum class Default { Null, EmptyString, Zero, False, EmptyArray };

const char* defaultName(Default value) {
    switch (value) {
        case Default::Null:        return "null";
        case Default::EmptyString: return "string";
        case Default::Zero:        return "number";
        case Default::False:       return "false";
        case Default::EmptyArray:  return "array";
    }
    return "<unknown>";
}

// Sends one request and returns the reply frame.
std::shared_ptr<JsonNode> call(PipeClient& client, int reqId, int proxyId,
                               const std::string& method, const std::string& argsJson) {
    std::string request = "{\"type\":\"request\",\"reqId\":" + std::to_string(reqId) +
                          ",\"proxyId\":" + std::to_string(proxyId) +
                          ",\"method\":\"" + method + "\"" +
                          ",\"args\":" + argsJson + "}";
    if (!client.sendLine(request)) return nullptr;
    return awaitReply(client, reqId, 5000);
}

} // namespace

int main() {
#ifdef _WIN32
    std::cout << "[ProtocolFallbackTest] The router must never answer a missing"
                 " feature with an error.\n\n";
#else
    std::cout << "[ProtocolFallbackTest] SKIPPED: the named pipe transport is"
                 " Windows only.\n";
    return 0;
#endif

    // A pipe name of its own, so a running editor cannot disturb the test.
    std::string pipeName = "vrutti_test_fallback_" + std::to_string(GetCurrentProcessId());

    IPCClient ipc(pipeName);
    ipc.start();

    VSCodeProtocol protocol(&ipc);
    protocol.initialize();

    PipeClient client;
    if (!client.connect(pipeName, 5000)) {
        std::cerr << "  [FAIL] could not connect to the test pipe\n";
        return 1;
    }

    int nextReqId = 1000;
    auto nextId = [&]() { return nextReqId++; };

    // One case per kind of benign answer.
    struct Case {
        int proxyId;
        const char* shape;
        const char* method;
        const char* args;
        Default expected;
        const char* what;
    };

    const Case cases[] = {
        // `void` upstream -- the search provider registry does not exist.
        { proxy::Main::MainThreadSearch, "MainThreadSearch",
          "$registerTextSearchProvider", "[1,\"file\"]", Default::Null,
          "a call that returns nothing" },

        // `Promise<boolean>` upstream -- Vrutti has no workspace edit engine.
        { proxy::Main::MainThreadBulkEdits, "MainThreadBulkEdits",
          "$tryApplyWorkspaceEdit", "[{}]", Default::False,
          "a call that returns a boolean" },

        // `Promise<tasks.ITaskDTO[]>` upstream -- no task engine.
        { proxy::Main::MainThreadTask, "MainThreadTask",
          "$fetchTasks", "[{}]", Default::EmptyArray,
          "a call that returns a list" },

        // `Promise<string>` upstream -- no task engine.
        { proxy::Main::MainThreadTask, "MainThreadTask",
          "$createTaskId", "[{}]", Default::EmptyString,
          "a call that returns a string" },

        // `Promise<number>` upstream -- no battery service.
        { proxy::Main::MainThreadPower, "MainThreadPower",
          "$startPowerSaveBlocker", "[\"other\"]", Default::Zero,
          "a call that returns a number" },
    };

    for (const Case& c : cases) {
        auto frame = call(client, nextId(), c.proxyId, c.method, c.args);
        const std::string label = std::string(c.what) + " (" + c.shape + "." + c.method + ")";

        check(frame != nullptr, label + ": the call was answered");
        check(frameType(frame) == "reply",
              label + ": answered with a reply, not an error (got " + frameType(frame) + ")");

        if (!protocol.hasHandler(c.shape, c.method)) {
            // No implementation: the answer must be the default of the
            // declared return type.
            check(resultKind(frame) == defaultName(c.expected),
                  label + ": answered with the default " + defaultName(c.expected) +
                  " (got " + resultKind(frame) + ")");
        }
    }

    // --- the same call a second time ------------------------------------------
    // The gap is reported once, but the answer must be the same every time.
    {
        auto frame = call(client, nextId(), proxy::Main::MainThreadSearch,
                          "$registerTextSearchProvider", "[1,\"file\"]");
        check(frameType(frame) == "reply",
              "a repeated missing call: answered with a reply, not an error (got "
              + frameType(frame) + ")");
    }

    // --- a call that IS implemented -------------------------------------------
    // The fallback must not shadow a real handler.
    {
        auto frame = call(client, nextId(), proxy::Main::MainThreadCommands,
                          "$getCommands", "[]");
        check(frameType(frame) == "reply",
              "an implemented call: answered with a reply (got " + frameType(frame) + ")");
        check(resultKind(frame) == "array",
              "an implemented call: answered with the real value (got "
              + resultKind(frame) + ")");
    }

    // --- a call that is not part of the protocol at all ------------------------
    // A newer host may know calls that this build does not. That is a missing
    // feature, not a broken peer, so it is answered too.
    {
        auto frame = call(client, nextId(), 9999, "$somethingFromTheFuture", "[]");
        check(frame != nullptr, "an unknown call was answered");
        check(frameType(frame) == "reply",
              "an unknown call: answered with a reply, not an error (got "
              + frameType(frame) + ")");
        check(resultKind(frame) == "null",
              "an unknown call: answered with null (got " + resultKind(frame) + ")");
    }

    client.disconnect();
    ipc.stop();

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[ProtocolFallbackTest] FAILED (" << g_failures << " assertion(s))\n";
        return 1;
    }
    std::cout << "[ProtocolFallbackTest] OK: a missing feature is never an error\n";
    return 0;
}
