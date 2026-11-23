#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include "core/editor/PieceTable.h"
#include "core/utils/Json.h"

namespace vrutti::core::ipc {

// -----------------------------------------------------------------------------
// IPCClient -- transport for the VS Code Extension Host protocol.
//
// WIRE FORMAT ("VruttiJSONBridge")
// ---------------------------------
// The Extension Host is the *upstream* VS Code `extensionHostProcess`, which in
// the Vrutti source tree is patched to speak a newline-delimited JSON dialect
// instead of VS Code's binary `PersistentProtocol` framing. See
// `src/vr/workbench/api/node/extensionHostProcess.ts` in the VS Code tree.
//
// Every frame is a single JSON object on one line:
//
//   renderer -> ext host
//     {"type":"init",     "data":{ ...IExtensionHostInitData... }}
//     {"type":"request",  "reqId":N,"proxyId":P,"method":"M","args":[...]}
//     {"type":"reply",    "reqId":N,"res":VALUE}
//     {"type":"replyErr", "reqId":N,"err":{"name":..,"message":..,"stack":..}}
//     {"type":"ack",      "reqId":N}
//     {"type":"cancel",   "reqId":N}
//     {"type":"terminate"}
//
//   ext host -> renderer
//     {"type":"ready"}
//     {"type":"request",  "reqId":N,"proxyId":P,"method":"M","args":[...]}
//     {"type":"reply",    "reqId":N,"res":VALUE}
//     {"type":"replyErr", "reqId":N,"err":{...}}
//     {"type":"ack",      "reqId":N}
//     {"type":"cancel",   "reqId":N}
//
// IMPORTANT: this is NOT JSON-RPC. There is no `jsonrpc`/`method`/`params`
// envelope and no `id` field. A previous implementation of this class used the
// JSON-RPC shape, which the Extension Host silently discarded -- meaning the
// init handshake never completed and no extension could ever activate.
//
// `proxyId` is VS Code's numeric proxy identifier (`nid`) for the actor being
// addressed; see VsCodeProxyIds.generated.h.
// -----------------------------------------------------------------------------
class IPCClient {
public:
    // How long a request to the extension host may take before it is given
    // up on. The answer of a well behaved host arrives in milliseconds.
    static constexpr int kDefaultRequestTimeoutMs = 30000;

    // The largest frame the core accepts from the extension host.
    //
    // A frame is one line of JSON. A peer that sends a longer one is either
    // broken or hostile, and a frame that big would be copied around a few
    // times before it is even looked at.
    static constexpr size_t kMaxFrameBytes = 32u * 1024u * 1024u;

    // The most calls that may wait for an answer at the same time.
    //
    // Every open call holds a promise and a deadline. A peer that never
    // answers would let the table grow until the calls time out, so the
    // table has a limit of its own.
    static constexpr size_t kMaxPendingCalls = 4096;

    // How long a frame may take to reach the peer.
    //
    // The write waits for the peer to take the bytes. A peer that never
    // reads would block the thread that writes, and with it the whole
    // editor: the thread that reads the frames is the same one.
    static constexpr int kWriteTimeoutMs = 10000;

    // An inbound RPC request from the Extension Host.
    struct RpcRequest {
        int reqId = 0;
        int proxyId = 0;
        std::string method;
        std::shared_ptr<vrutti::core::utils::JsonNode> args;   // Array node (may be null)
        bool hasCancellation = false;
        bool mixedArgs = false;
    };

    IPCClient(const std::string& pipeName);
    ~IPCClient();

    // Starts listening for messages from the Node.js Extension Host on a background thread
    void start();
    void stop();

    // --- outbound: renderer -> extension host ---------------------------------

    // Sends the extension host initialization payload. The Extension Host waits
    // for this before it will construct its services or activate extensions.
    void sendInitData(const std::shared_ptr<vrutti::core::utils::JsonNode>& initData);

    // Sends an RPC request to the Extension Host. `args` must be a JSON array
    // node (the positional arguments of the RPC method).
    //
    // `timeoutMs` is how long the core waits for the answer. When the time
    // is up the pending call is rejected and the entry is dropped, so a peer
    // that never answers cannot wedge the caller forever.
    //
    // Returns the assigned request id.
    int sendRequest(int proxyId,
                    const std::string& method,
                    const std::shared_ptr<vrutti::core::utils::JsonNode>& args,
                    std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> onResolve,
                    std::function<void(const std::string&, const std::string&)> onReject = nullptr,
                    int timeoutMs = kDefaultRequestTimeoutMs);

    // Sends the result of an inbound request.
    void sendReply(int reqId, const std::shared_ptr<vrutti::core::utils::JsonNode>& result);

    // Sends a failure result for an inbound request.
    void sendReplyError(int reqId, const std::string& message, const std::string& name = "Error");

    // Acknowledges receipt of an inbound request (keeps the ExtHost responsive).
    void sendAck(int reqId);

    // Cancels an outbound request that we previously sent.
    void sendCancel(int reqId);

    // Politely asks the Extension Host to shut down.
    void sendTerminate();

    // --- inbound callbacks ----------------------------------------------------

    // Fired when the Extension Host reports it is ready to receive init data.
    void setOnReady(std::function<void()> cb) { m_onReady = std::move(cb); }

    // Fired for every inbound RPC request. The consumer is responsible for
    // replying (sendReply / sendReplyError); the Extension Host blocks on the
    // reply, so a missing reply hangs the calling extension.
    void setOnRequest(std::function<void(const RpcRequest&)> cb) { m_onRequest = std::move(cb); }

    // Fired for inbound messages that are not RPC traffic (e.g. UI events).
    void setOnEvent(std::function<void(const std::string& method,
                                       const std::shared_ptr<vrutti::core::utils::JsonNode>& params)> cb) {
        m_onEvent = std::move(cb);
    }

    // Publishes a local event on the same channel used for inbound non-RPC
    // traffic, so the UI layer has a single subscription point.
    void emitEvent(const std::string& method,
                   const std::shared_ptr<vrutti::core::utils::JsonNode>& params);

    // --- local (UI) request channel --------------------------------------------
    // The webview asks the core to do something. These are NOT extension host
    // RPC calls -- they are handled locally (or translated into a proper
    // extension host RPC call by the protocol layer).
    void setOnLocalRequest(std::function<void(const std::string& method,
                                             const std::shared_ptr<vrutti::core::utils::JsonNode>& params)> cb) {
        m_onLocalRequest = std::move(cb);
    }

    void sendLocalRequest(const std::string& method,
                          const std::shared_ptr<vrutti::core::utils::JsonNode>& params);

    // Fired when the underlying pipe/socket is connected.
    void setOnConnected(std::function<void()> cb) { m_onConnected = std::move(cb); }

    // Binds the active PieceTable so IPC can directly manipulate the C++ text buffer
    void bindEditorBuffer(vrutti::core::editor::PieceTable* table);

    // Internal: called when a reply from the Extension Host arrives.
    void resolvePending(int requestId,
                        const std::shared_ptr<vrutti::core::utils::JsonNode>& payload,
                        bool isError,
                        const std::string& errorMessage = "");

    // Internal: dispatches a single inbound frame.
    void handleIncomingMessage(const std::string& jsonMessage);

    // Decodes a VSBuffer that was flattened by JSON.stringify into
    // { "buffer": { "0": 12, "1": 34, ... } }.
    static bool decodeVSBuffer(const std::shared_ptr<vrutti::core::utils::JsonNode>& node,
                               std::vector<uint8_t>& out);

    // Encodes binary content for the bridge.
    //
    // The bridge carries binary payloads as { "$vsbuffer": "<base64>" }. The
    // extension host turns that marker back into a real VSBuffer, so a
    // payload that is not marked would arrive as a plain object.
    static std::shared_ptr<vrutti::core::utils::JsonNode>
        encodeVSBuffer(const std::vector<uint8_t>& data);

private:
    // What happened to a frame that was sent.
    enum class WriteResult {
        Sent,          // the frame reached the pipe
        NotConnected,  // there is no peer at the moment
        Failed         // the frame could not be written
    };

    WriteResult sendRaw(const std::string& payload);
    void listenLoop();
    void watchPendingCalls();
    std::string serializeJson(const std::shared_ptr<vrutti::core::utils::JsonNode>& node);

    // Takes the frames that are complete out of the incoming buffer.
    //
    // Returns false when the peer sent something that is bigger than the
    // limit: a frame that never ends, or a frame that is too long. The
    // connection has to be dropped in that case.
    bool takeFrames(std::vector<std::string>& frames);

    std::string m_pipeName;
    // Read by the listen thread and by the watchdog, so it is atomic.
    std::atomic<bool> m_running;
    vrutti::core::editor::PieceTable* m_activeBuffer;
    // The state of the connection is read by the thread that writes and by
    // the thread that listens, so it is atomic.
    std::atomic<bool> m_isConnected{false};

    // Platform-specific connection handle (e.g. HANDLE for Windows Named Pipes)
    void* m_connectionHandle;
    std::mutex m_pipeMutex;

#ifdef _WIN32
    void* m_stopEvent;
#else
    // The socket that waits for the extension host. It is kept apart from
    // the connection, so that stopping the client does not close a handle
    // that the listen thread still owns.
    int m_serverSocket = -1;
#endif

    std::function<void()> m_onReady;
    std::function<void(const RpcRequest&)> m_onRequest;
    std::function<void(const std::string& method,
                       const std::shared_ptr<vrutti::core::utils::JsonNode>& params)> m_onEvent;
    std::function<void(const std::string& method,
                       const std::shared_ptr<vrutti::core::utils::JsonNode>& params)> m_onLocalRequest;
    std::function<void()> m_onConnected;

    std::mutex m_bufferMutex;
    std::string m_incomingBuffer;

    // How far the buffer has been looked at for a line break.
    //
    // A peer that sends a frame in many pieces would otherwise make the
    // core read the whole buffer again for every piece, which turns a big
    // frame into a long wait.
    size_t m_scanPosition = 0;

    // Pending request tracking for promise resolution
    std::mutex m_pendingMutex;
    int m_nextId{0};
    struct PendingCall {
        std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)> resolve;
        std::function<void(const std::string&, const std::string&)> reject;
        // When the answer has to be there. A call that passes it is given up
        // on, so a peer that never answers cannot wedge the caller.
        std::chrono::steady_clock::time_point deadline;
    };
    std::unordered_map<int, PendingCall> m_pendingCalls;

    // The watchdog that gives up on the calls the extension host never
    // answers.
    std::thread m_watchdog;
    std::mutex m_watchdogMutex;
    std::condition_variable m_watchdogSignal;

    // The thread that listens for the extension host.
    //
    // The thread is joined when the client is stopped, so the object cannot
    // be used after it is gone: a thread that is still running would read
    // the members of an object that no longer exists.
    std::thread m_listenThread;
};

} // namespace vrutti::core::ipc
