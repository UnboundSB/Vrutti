// -----------------------------------------------------------------------------
// The pipe client of the integration tests.
//
// The tests play the part of the extension host: they connect to the named
// pipe the core is listening on, send requests and read the replies. The
// wire format is the one of the bridge: one JSON object per line.
// -----------------------------------------------------------------------------
#pragma once

#include "../../core/ipc/IPCClient.h"
#include "../../core/utils/Json.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace vrutti::tests {

using vrutti::core::ipc::IPCClient;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;

class PipeClient {
public:
    PipeClient() = default;
    ~PipeClient() { disconnect(); }

    // Connects to the named pipe the core is listening on.
    bool connect(const std::string& pipeName, int timeoutMs) {
#ifdef _WIN32
        const std::string path = "\\\\.\\pipe\\" + pipeName;
        const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
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
        const std::string payload = line + "\n";
        DWORD written = 0;
        return WriteFile(m_handle, payload.c_str(),
                         static_cast<DWORD>(payload.size()), &written, nullptr) != FALSE;
#else
        return false;
#endif
    }

    // Writes bytes as they are, without a line break at the end.
    //
    // The tests use this to send a frame that never ends: the core has to
    // notice that the buffer grows without a limit.
    bool sendBytes(const std::string& bytes) {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        return WriteFile(m_handle, bytes.data(),
                         static_cast<DWORD>(bytes.size()), &written, nullptr) != FALSE;
#else
        return false;
#endif
    }

    // Reads whatever is there and throws it away.
    //
    // The tests use this to keep the pipe from filling up: a pipe whose
    // reader does not read makes the writer wait, and the writer of the
    // core is the thread that reads the answers.
    bool drainBytes(int timeoutMs) {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
        bool gotAnything = false;
        while (GetTickCount() < deadline) {
            DWORD available = 0;
            if (PeekNamedPipe(m_handle, nullptr, 0, nullptr, &available, nullptr)
                && available > 0) {
                char buffer[8192];
                const DWORD toRead = available < sizeof(buffer) ? available : sizeof(buffer);
                DWORD got = 0;
                if (ReadFile(m_handle, buffer, toRead, &got, nullptr) && got > 0) {
                    gotAnything = true;
                    continue;
                }
            }
            if (gotAnything) return true;
            Sleep(1);
        }
        return gotAnything;
#endif
        return false;
    }

    // True while the pipe is still open.
    bool connected() const {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        DWORD available = 0;
        if (!PeekNamedPipe(m_handle, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    // Reads the next complete frame (one JSON object on one line).
    bool readLine(std::string& out, int timeoutMs) {
#ifdef _WIN32
        if (m_handle == INVALID_HANDLE_VALUE) return false;
        const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeoutMs);
        while (GetTickCount() < deadline) {
            const size_t pos = m_pending.find('\n');
            if (pos != std::string::npos) {
                out = m_pending.substr(0, pos);
                m_pending.erase(0, pos + 1);
                return true;
            }
            DWORD available = 0;
            if (PeekNamedPipe(m_handle, nullptr, 0, nullptr, &available, nullptr)
                && available > 0) {
                char buffer[4096];
                const DWORD toRead = available < sizeof(buffer) ? available : sizeof(buffer);
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

    // The bytes of the last read frame, if it carried a binary payload.
    bool lastBuffer(std::vector<uint8_t>& out) const {
        return IPCClient::decodeVSBuffer(m_lastPayload, out);
    }

private:
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#endif
    std::string m_pending;
    std::shared_ptr<JsonNode> m_lastPayload;
};

// Reads frames until the reply for `reqId` arrives.
inline std::shared_ptr<JsonNode> awaitReply(PipeClient& client, int reqId,
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
        if (static_cast<int>(reqIdNode->numberValue) != reqId) continue;

        return frame;
    }
    return nullptr;
#else
    (void)client; (void)reqId; (void)timeoutMs;
    return nullptr;
#endif
}

// The type of a frame: "reply", "replyErr" or "<none>".
inline std::string frameType(const std::shared_ptr<JsonNode>& frame) {
    if (!frame) return "<none>";
    auto node = frame->get("type");
    if (!node || node->type != JsonNode::Type::String) return "<none>";
    return std::string(node->stringValue.data(), node->stringValue.length());
}

// The kind of the `res` payload of a reply.
inline std::string resultKind(const std::shared_ptr<JsonNode>& frame) {
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

// The name of the error of a failed reply.
inline std::string errorName(const std::shared_ptr<JsonNode>& frame) {
    if (!frame) return "<none>";
    auto err = frame->get("err");
    if (!err || err->type != JsonNode::Type::Object) return "<none>";
    auto node = err->get("name");
    if (!node || node->type != JsonNode::Type::String) return "<none>";
    return std::string(node->stringValue.data(), node->stringValue.length());
}

// The message of the error of a failed reply.
inline std::string errorMessage(const std::shared_ptr<JsonNode>& frame) {
    if (!frame) return "<none>";
    auto err = frame->get("err");
    if (!err || err->type != JsonNode::Type::Object) return "<none>";
    auto node = err->get("message");
    if (!node || node->type != JsonNode::Type::String) return "<none>";
    return std::string(node->stringValue.data(), node->stringValue.length());
}

// A number of a field of an object.
inline double numberField(const std::shared_ptr<JsonNode>& node, const char* name,
                          double fallback = -1) {
    if (!node || node->type != JsonNode::Type::Object) return fallback;
    auto value = node->get(name);
    if (!value || value->type != JsonNode::Type::Number) return fallback;
    return value->numberValue;
}

// A text of a field of an object.
inline std::string stringField(const std::shared_ptr<JsonNode>& node, const char* name) {
    if (!node || node->type != JsonNode::Type::Object) return "";
    auto value = node->get(name);
    if (!value || value->type != JsonNode::Type::String) return "";
    return std::string(value->stringValue.data(), value->stringValue.length());
}

// Builds the binary payload the way the extension host sends it: a VSBuffer
// that was flattened by JSON.stringify.
inline std::string makeBufferArgument(const std::string& content) {
    std::string json = "{\"buffer\":{";
    for (size_t i = 0; i < content.size(); ++i) {
        if (i > 0) json += ",";
        json += "\"" + std::to_string(i) + "\":" +
                std::to_string(static_cast<int>(static_cast<unsigned char>(content[i])));
    }
    json += "}}";
    return json;
}

// Decodes the binary payload the core sends: { "$vsbuffer": "<base64>" }.
inline std::string decodeBufferResult(const std::shared_ptr<JsonNode>& frame) {
    std::vector<uint8_t> bytes;
    if (!frame) return "";
    auto res = frame->get("res");
    if (!res) return "";
    if (!IPCClient::decodeVSBuffer(res, bytes)) return "";
    return std::string(bytes.begin(), bytes.end());
}

} // namespace vrutti::tests
