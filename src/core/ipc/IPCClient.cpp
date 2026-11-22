#include "IPCClient.h"
#include "../utils/ErrorCodes.h"
#include "../utils/Base64.h"
#include <iostream>
#include <thread>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#endif

using namespace vrutti::core::utils;
using vrutti::core::utils::ErrorCode;

namespace vrutti::core::ipc {

namespace {
    // Builds a JSON string node that owns its text.
    std::shared_ptr<JsonNode> strNode(std::string value) {
        return vrutti::core::utils::makeJsonString(std::move(value));
    }

    std::shared_ptr<JsonNode> numNode(double value) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Number);
        node->numberValue = value;
        return node;
    }

    std::shared_ptr<JsonNode> boolNode(bool value) {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
        node->boolValue = value;
        return node;
    }

    std::shared_ptr<JsonNode> nullNode() {
        return std::make_shared<JsonNode>(JsonNode::Type::Null);
    }
} // namespace

IPCClient::IPCClient(const std::string& pipeName)
    : m_pipeName(pipeName), m_running(false), m_activeBuffer(nullptr),
      m_connectionHandle(nullptr), m_nextId(0)
{
#ifdef _WIN32
    m_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
}

IPCClient::~IPCClient() {
    stop();
#ifdef _WIN32
    if (m_stopEvent) CloseHandle(static_cast<HANDLE>(m_stopEvent));
#endif
}

void IPCClient::bindEditorBuffer(vrutti::core::editor::PieceTable* table) {
    m_activeBuffer = table;
}

void IPCClient::start() {
    if (m_running) return;
    m_running = true;
#ifdef _WIN32
    ResetEvent(static_cast<HANDLE>(m_stopEvent));
#endif

    std::thread([this]() {
        try { this->listenLoop(); }
        catch (const std::exception& e) {
            reportError("IPC", ErrorCode::IpcConnectionFailed,
                        std::string("the listen loop stopped: ") + e.what());
        }
        catch (...) {
            reportError("IPC", ErrorCode::IpcConnectionFailed, "the listen loop stopped unexpectedly");
        }
    }).detach();

    // The watchdog gives up on the calls the extension host never answers.
    // Without it, a promise on the other side would wait forever.
    m_watchdog = std::thread([this]() { this->watchPendingCalls(); });

    std::cout << "[IPC] Bound to pipe/socket: " << m_pipeName << "\n";
    std::cout << "[IPC] Awaiting connection from Node.js Extension Host...\n";
}

void IPCClient::stop() {
    if (!m_running) return;
    m_running = false;

    // The watchdog is woken up and joined, so it cannot touch the table
    // after this object is gone.
    m_watchdogSignal.notify_all();
    if (m_watchdog.joinable()) {
        m_watchdog.join();
    }

#ifndef _WIN32
    if (m_connectionHandle) {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_connectionHandle));
        close(fd);
        m_connectionHandle = nullptr;
    }
#else
    SetEvent(static_cast<HANDLE>(m_stopEvent));
    std::lock_guard<std::mutex> lock(m_pipeMutex);
    if (m_connectionHandle && m_connectionHandle != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(static_cast<HANDLE>(m_connectionHandle));
        CloseHandle(static_cast<HANDLE>(m_connectionHandle));
        m_connectionHandle = nullptr;
    }
#endif

    // The calls that are still open are given up on, so nothing waits for an
    // answer that can no longer arrive.
    {
        std::lock_guard<std::mutex> pendingLock(m_pendingMutex);
        m_pendingCalls.clear();
    }
}

std::string IPCClient::serializeJson(const std::shared_ptr<JsonNode>& node) {
    return JsonSerializer::stringify(node, 0, false);
}

void IPCClient::sendRaw(const std::string& payload) {
    if (!m_running) return;

#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(m_pipeMutex);
        if (m_connectionHandle && m_connectionHandle != INVALID_HANDLE_VALUE && m_isConnected) {
            OVERLAPPED ol = { 0 };
            ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            DWORD bytesWritten = 0;
            if (!WriteFile(static_cast<HANDLE>(m_connectionHandle), payload.c_str(),
                           static_cast<DWORD>(payload.length()), NULL, &ol)) {
                if (GetLastError() == ERROR_IO_PENDING) {
                    GetOverlappedResult(static_cast<HANDLE>(m_connectionHandle), &ol, &bytesWritten, TRUE);
                } else {
                    reportError("IPC", ErrorCode::IpcWriteFailed,
                                "WriteFile failed, error " + std::to_string(GetLastError()));
                }
            }
            CloseHandle(ol.hEvent);
        }
    }
#else
    if (m_connectionHandle) {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_connectionHandle));
        write(fd, payload.c_str(), payload.length());
    }
#endif
}

// -----------------------------------------------------------------------------
// Outbound: renderer -> extension host
// -----------------------------------------------------------------------------

void IPCClient::emitEvent(const std::string& method,
                          const std::shared_ptr<JsonNode>& params) {
    if (m_onEvent) m_onEvent(method, params);
}

void IPCClient::sendLocalRequest(const std::string& method,
                                 const std::shared_ptr<JsonNode>& params) {
    if (m_onLocalRequest) m_onLocalRequest(method, params);
}

void IPCClient::sendInitData(const std::shared_ptr<JsonNode>& initData) {
    // The bridge does `JSON.stringify(msg.data)`, so `data` must be an OBJECT.
    auto frame = std::make_shared<JsonNode>(JsonNode::Type::Object);
    frame->objectProperties.emplace_back("type", strNode("init"));
    frame->objectProperties.emplace_back("data", initData ? initData : nullNode());
    sendRaw(serializeJson(frame) + "\n");
    std::cout << "[IPC] Sent extension host init data\n";
}

int IPCClient::sendRequest(int proxyId,
                           const std::string& method,
                           const std::shared_ptr<JsonNode>& args,
                           std::function<void(const std::shared_ptr<JsonNode>&)> onResolve,
                           std::function<void(const std::string&, const std::string&)> onReject,
                           int timeoutMs) {
    int id = ++m_nextId;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        PendingCall call;
        call.resolve = std::move(onResolve);
        call.reject = std::move(onReject);
        call.deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs
                                                                : kDefaultRequestTimeoutMs);
        m_pendingCalls[id] = std::move(call);
    }

    auto frame = std::make_shared<JsonNode>(JsonNode::Type::Object);
    frame->objectProperties.emplace_back("type", strNode("request"));
    frame->objectProperties.emplace_back("reqId", numNode(static_cast<double>(id)));
    frame->objectProperties.emplace_back("proxyId", numNode(static_cast<double>(proxyId)));
    frame->objectProperties.emplace_back("method", strNode(method));
    frame->objectProperties.emplace_back("args",
        (args && args->type == JsonNode::Type::Array)
            ? args
            : std::make_shared<JsonNode>(JsonNode::Type::Array));
    sendRaw(serializeJson(frame) + "\n");
    return id;
}

// -----------------------------------------------------------------------------
// The watchdog
// -----------------------------------------------------------------------------

void IPCClient::watchPendingCalls() {
    std::unique_lock<std::mutex> lock(m_watchdogMutex);
    while (m_running) {
        // The table is checked often enough that a call is given up on
        // shortly after its time is up, and rarely enough that the check
        // costs nothing.
        m_watchdogSignal.wait_for(lock, std::chrono::milliseconds(250), [this]() {
            return !m_running;
        });
        if (!m_running) break;

        std::vector<std::pair<int, std::function<void(const std::string&, const std::string&)>>> expired;
        {
            std::lock_guard<std::mutex> pendingLock(m_pendingMutex);
            auto now = std::chrono::steady_clock::now();
            for (auto it = m_pendingCalls.begin(); it != m_pendingCalls.end();) {
                if (it->second.deadline <= now) {
                    if (it->second.reject) {
                        expired.emplace_back(it->first, it->second.reject);
                    }
                    it = m_pendingCalls.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // The callbacks run outside the lock: a callback may answer a
        // request, which touches the table again.
        for (auto& entry : expired) {
            reportError("IPC", ErrorCode::IpcTimeout,
                        "the extension host did not answer request " +
                        std::to_string(entry.first) + " in time");
            entry.second("the extension host did not answer in time", "TimeoutError");
        }
    }
}

void IPCClient::sendReply(int reqId, const std::shared_ptr<JsonNode>& result) {
    auto frame = std::make_shared<JsonNode>(JsonNode::Type::Object);
    frame->objectProperties.emplace_back("type", strNode("reply"));
    frame->objectProperties.emplace_back("reqId", numNode(static_cast<double>(reqId)));
    frame->objectProperties.emplace_back("res", result ? result : nullNode());
    sendRaw(serializeJson(frame) + "\n");
}

void IPCClient::sendReplyError(int reqId, const std::string& message, const std::string& name) {
    // `$isError` makes the Extension Host rebuild a real Error object
    // (see RPCProtocol._receiveReplyErr) instead of handing the extension a
    // bare object.
    auto err = std::make_shared<JsonNode>(JsonNode::Type::Object);
    err->objectProperties.emplace_back("$isError", boolNode(true));
    err->objectProperties.emplace_back("name", strNode(name));
    err->objectProperties.emplace_back("message", strNode(message));

    auto frame = std::make_shared<JsonNode>(JsonNode::Type::Object);
    frame->objectProperties.emplace_back("type", strNode("replyErr"));
    frame->objectProperties.emplace_back("reqId", numNode(static_cast<double>(reqId)));
    frame->objectProperties.emplace_back("err", err);
    sendRaw(serializeJson(frame) + "\n");
}

void IPCClient::sendAck(int reqId) {
    auto frame = std::make_shared<JsonNode>(JsonNode::Type::Object);
    frame->objectProperties.emplace_back("type", strNode("ack"));
    frame->objectProperties.emplace_back("reqId", numNode(static_cast<double>(reqId)));
    sendRaw(serializeJson(frame) + "\n");
}

void IPCClient::sendCancel(int reqId) {
    auto frame = std::make_shared<JsonNode>(JsonNode::Type::Object);
    frame->objectProperties.emplace_back("type", strNode("cancel"));
    frame->objectProperties.emplace_back("reqId", numNode(static_cast<double>(reqId)));
    sendRaw(serializeJson(frame) + "\n");
}

void IPCClient::sendTerminate() {
    auto frame = std::make_shared<JsonNode>(JsonNode::Type::Object);
    frame->objectProperties.emplace_back("type", strNode("terminate"));
    sendRaw(serializeJson(frame) + "\n");
}

// -----------------------------------------------------------------------------
// Inbound: extension host -> renderer
// -----------------------------------------------------------------------------

void IPCClient::resolvePending(int requestId,
                               const std::shared_ptr<JsonNode>& payload,
                               bool isError,
                               const std::string& errorMessage) {
    std::function<void(const std::shared_ptr<JsonNode>&)> resolveCb;
    std::function<void(const std::string&, const std::string&)> rejectCb;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        auto it = m_pendingCalls.find(requestId);
        if (it == m_pendingCalls.end()) return;
        resolveCb = it->second.resolve;
        rejectCb = it->second.reject;
        m_pendingCalls.erase(it);
    }
    if (isError) {
        // The name of the error is the code the extension host expects, for
        // example "FileNotFound". It is carried by the error object.
        std::string name = "Error";
        if (payload && payload->type == JsonNode::Type::Object) {
            auto nameNode = payload->get("name");
            if (nameNode && nameNode->type == JsonNode::Type::String) {
                name = std::string(nameNode->stringValue.data(), nameNode->stringValue.length());
            }
        }
        if (rejectCb) rejectCb(errorMessage, name);
    } else {
        if (resolveCb) resolveCb(payload);
    }
}

namespace {
    // The largest binary payload the core accepts. A peer that asks for more
    // is broken or hostile, and a huge allocation is not worth the risk.
    constexpr size_t kMaxBufferBytes = 64u * 1024u * 1024u;

    // The index of a byte in the flattened buffer.
    //
    // The key of the property is the index of the byte and the value of the
    // property is the byte itself. The index has to be read from the key:
    // reading it from the value would make the size of the buffer depend on
    // the content of the file instead of on its length.
    bool parseBufferIndex(const std::string& key, size_t& out) {
        if (key.empty() || key.size() > 10) return false;
        size_t value = 0;
        for (char c : key) {
            if (c < '0' || c > '9') return false;
            value = value * 10 + static_cast<size_t>(c - '0');
        }
        if (value >= kMaxBufferBytes) return false;
        out = value;
        return true;
    }
} // namespace

bool IPCClient::decodeVSBuffer(const std::shared_ptr<JsonNode>& node, std::vector<uint8_t>& out) {
    out.clear();
    if (!node || node->type != JsonNode::Type::Object) return false;

    // The marker form: { "$vsbuffer": "<base64>" }. This is what the bridge
    // uses for a payload that the renderer sends.
    auto markerNode = node->get("$vsbuffer");
    if (markerNode && markerNode->type == JsonNode::Type::String) {
        std::string encoded(markerNode->stringValue.data(), markerNode->stringValue.length());
        std::string decoded = base64_decode(encoded);
        if (decoded.size() > kMaxBufferBytes) return false;
        out.assign(decoded.begin(), decoded.end());
        return true;
    }

    // The marshalled form: { "buffer": { "0": 12, ... } }. A VSBuffer that
    // travels through the upstream argument serializer arrives this way.
    auto bufferNode = node->get("buffer");
    if (!bufferNode || bufferNode->type != JsonNode::Type::Object) return false;

    size_t maxIndex = 0;
    bool any = false;
    for (const auto& prop : bufferNode->objectProperties) {
        if (!prop.second || prop.second->type != JsonNode::Type::Number) continue;
        size_t index = 0;
        if (!parseBufferIndex(prop.first, index)) continue;
        maxIndex = std::max(maxIndex, index);
        any = true;
    }
    if (!any) {
        // An empty buffer is a buffer of no bytes.
        return true;
    }

    out.assign(maxIndex + 1, 0);
    for (const auto& prop : bufferNode->objectProperties) {
        if (!prop.second || prop.second->type != JsonNode::Type::Number) continue;
        size_t index = 0;
        if (!parseBufferIndex(prop.first, index)) continue;
        if (index < out.size()) out[index] = static_cast<uint8_t>(prop.second->numberValue);
    }
    return true;
}

std::shared_ptr<JsonNode> IPCClient::encodeVSBuffer(const std::vector<uint8_t>& data) {
    auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);
    std::string raw(data.begin(), data.end());
    node->objectProperties.emplace_back("$vsbuffer",
        vrutti::core::utils::makeJsonString(base64_encode(raw)));
    return node;
}

void IPCClient::handleIncomingMessage(const std::string& jsonMessage) {
    try {
        auto msg = JsonParser::parse(jsonMessage);
        if (!msg || msg->type != JsonNode::Type::Object) return;

        auto typeNode = msg->get("type");
        if (!typeNode || typeNode->type != JsonNode::Type::String) return;
        std::string type(typeNode->stringValue.data(), typeNode->stringValue.length());

        if (type == "ready") {
            std::cout << "[IPC] Extension Host reported ready\n";
            if (m_onReady) m_onReady();
            return;
        }

        if (type == "initialized") {
            return; // informational only
        }

        if (type == "terminate") {
            std::cout << "[IPC] Extension Host requested termination\n";
            return;
        }

        if (type == "request") {
            RpcRequest req;
            auto reqIdNode = msg->get("reqId");
            if (reqIdNode && reqIdNode->type == JsonNode::Type::Number) {
                req.reqId = static_cast<int>(reqIdNode->numberValue);
            }
            auto proxyIdNode = msg->get("proxyId");
            if (proxyIdNode && proxyIdNode->type == JsonNode::Type::Number) {
                req.proxyId = static_cast<int>(proxyIdNode->numberValue);
            }
            auto methodNode = msg->get("method");
            if (methodNode && methodNode->type == JsonNode::Type::String) {
                req.method = std::string(methodNode->stringValue.data(), methodNode->stringValue.length());
            }
            req.args = msg->get("args");
            auto cancelNode = msg->get("cancellation");
            if (cancelNode && cancelNode->type == JsonNode::Type::Boolean) {
                req.hasCancellation = cancelNode->boolValue;
            }
            auto mixedNode = msg->get("mixed");
            if (mixedNode && mixedNode->type == JsonNode::Type::Boolean) {
                req.mixedArgs = mixedNode->boolValue;
            }

            if (m_onRequest) m_onRequest(req);
            return;
        }

        if (type == "reply") {
            auto reqIdNode = msg->get("reqId");
            if (reqIdNode && reqIdNode->type == JsonNode::Type::Number) {
                int id = static_cast<int>(reqIdNode->numberValue);
                auto resNode = msg->get("res");
                resolvePending(id, resNode ? resNode : nullNode(), false);
            }
            return;
        }

        if (type == "replyErr") {
            auto reqIdNode = msg->get("reqId");
            if (reqIdNode && reqIdNode->type == JsonNode::Type::Number) {
                int id = static_cast<int>(reqIdNode->numberValue);
                std::string message = "Unknown extension host error";
                auto errNode = msg->get("err");
                if (errNode && errNode->type == JsonNode::Type::Object) {
                    auto msgNode = errNode->get("message");
                    if (msgNode && msgNode->type == JsonNode::Type::String) {
                        message = std::string(msgNode->stringValue.data(), msgNode->stringValue.length());
                    }
                }
                resolvePending(id, errNode ? errNode : nullNode(), true, message);
            }
            return;
        }

        if (type == "ack") {
            auto reqIdNode = msg->get("reqId");
            if (reqIdNode && reqIdNode->type == JsonNode::Type::Number) {
                // The ExtHost acknowledged one of our requests; nothing to do.
            }
            return;
        }

        if (type == "cancel") {
            auto reqIdNode = msg->get("reqId");
            if (reqIdNode && reqIdNode->type == JsonNode::Type::Number) {
                std::cout << "[IPC] Extension Host cancelled request "
                          << static_cast<int>(reqIdNode->numberValue) << "\n";
            }
            return;
        }

        // Unknown frame type -- forward it to the UI channel as an event so the
        // frontend can decide what to do with it.
        if (m_onEvent) {
            auto paramsNode = msg->get("params");
            m_onEvent(type, paramsNode ? paramsNode : nullNode());
        }
    } catch (const std::exception& e) {
        reportError("IPC", ErrorCode::IpcFrameDecodeFailed, e.what());
    }
}

void IPCClient::listenLoop() {
#ifdef _WIN32
    while (m_running) {
        std::string pipePath = "\\\\.\\pipe\\" + m_pipeName;
        HANDLE hPipe = CreateNamedPipeA(
            pipePath.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, 4096, 4096, 0, NULL);

        if (hPipe == INVALID_HANDLE_VALUE) {
            reportError("IPC", ErrorCode::IpcPipeCreateFailed,
                        "CreateNamedPipe failed for " + pipePath +
                        ", error " + std::to_string(GetLastError()));
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_pipeMutex);
            m_connectionHandle = hPipe;
        }

        OVERLAPPED olConnect = { 0 };
        olConnect.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        BOOL connected = ConnectNamedPipe(hPipe, &olConnect) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (!connected && GetLastError() == ERROR_IO_PENDING) {
            HANDLE waitHandles[2] = { olConnect.hEvent, static_cast<HANDLE>(m_stopEvent) };
            DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
            if (waitRes == WAIT_OBJECT_0) {
                connected = TRUE;
            } else {
                CancelIo(hPipe);
                CloseHandle(olConnect.hEvent);
                continue;
            }
        }

        if (connected && m_running) {
            m_isConnected = true;
            std::cout << "[IPC] Extension Host connected\n";
            if (m_onConnected) {
                m_onConnected();
            }
            char buffer[4096];
            OVERLAPPED olRead = { 0 };
            olRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

            while (m_running) {
                DWORD bytesRead = 0;
                BOOL readOk = ReadFile(hPipe, buffer, sizeof(buffer) - 1, NULL, &olRead);
                if (!readOk && GetLastError() == ERROR_IO_PENDING) {
                    HANDLE waitHandles[2] = { olRead.hEvent, static_cast<HANDLE>(m_stopEvent) };
                    DWORD waitRes = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
                    if (waitRes == WAIT_OBJECT_0) {
                        readOk = GetOverlappedResult(hPipe, &olRead, &bytesRead, FALSE);
                    } else {
                        CancelIo(hPipe);
                        break;
                    }
                } else if (readOk) {
                    GetOverlappedResult(hPipe, &olRead, &bytesRead, FALSE);
                }

                if (readOk && bytesRead > 0) {
                    // NOTE: the length must be taken explicitly -- constructing a
                    // std::string from `buffer` before NUL-terminating it used to
                    // leak uninitialized bytes into every frame.
                    std::string chunk(buffer, bytesRead);

                    std::vector<std::string> frames;
                    {
                        std::lock_guard<std::mutex> lock(m_bufferMutex);
                        m_incomingBuffer += chunk;
                        size_t pos = 0;
                        while ((pos = m_incomingBuffer.find('\n')) != std::string::npos) {
                            frames.push_back(m_incomingBuffer.substr(0, pos));
                            m_incomingBuffer.erase(0, pos + 1);
                        }
                    }

                    // Dispatch outside the buffer lock: a handler may reply on the
                    // same channel and must not deadlock or re-enter the parser.
                    for (const auto& frame : frames) {
                        if (!frame.empty()) handleIncomingMessage(frame);
                    }
                } else {
                    break;
                }
                ResetEvent(olRead.hEvent);
            }
            CloseHandle(olRead.hEvent);
            m_isConnected = false;
        }

        CloseHandle(olConnect.hEvent);

        {
            std::lock_guard<std::mutex> lock(m_pipeMutex);
            if (m_connectionHandle == hPipe) {
                DisconnectNamedPipe(hPipe);
                CloseHandle(hPipe);
                m_connectionHandle = nullptr;
            }
        }
    }
#else
    while (m_running) {
        int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) return;
        m_connectionHandle = reinterpret_cast<void*>(static_cast<intptr_t>(server_fd));

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_pipeName.c_str(), sizeof(addr.sun_path) - 1);

        unlink(m_pipeName.c_str());
        if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(server_fd);
            m_connectionHandle = nullptr;
            return;
        }
        if (listen(server_fd, 5) < 0) {
            close(server_fd);
            m_connectionHandle = nullptr;
            return;
        }

        while (m_running) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd < 0) continue;
            m_connectionHandle = reinterpret_cast<void*>(static_cast<intptr_t>(client_fd));
            m_isConnected = true;
            std::cout << "[IPC] Extension Host connected\n";
            if (m_onConnected) m_onConnected();

            char buffer[4096];
            while (m_running) {
                ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                if (bytes_read <= 0) break;

                std::string chunk(buffer, bytes_read);
                std::vector<std::string> frames;
                {
                    std::lock_guard<std::mutex> lock(m_bufferMutex);
                    m_incomingBuffer += chunk;
                    size_t pos = 0;
                    while ((pos = m_incomingBuffer.find('\n')) != std::string::npos) {
                        frames.push_back(m_incomingBuffer.substr(0, pos));
                        m_incomingBuffer.erase(0, pos + 1);
                    }
                }

                for (const auto& frame : frames) {
                    if (!frame.empty()) handleIncomingMessage(frame);
                }
            }
            m_isConnected = false;
            close(client_fd);
            m_connectionHandle = nullptr;
        }
        close(server_fd);
        unlink(m_pipeName.c_str());
    }
#endif
}

} // namespace vrutti::core::ipc
