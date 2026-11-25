# ipc (src/core/ipc/)

## Purpose
Inter-process communication: the pipe transport and the protocol that connects the core to the extension host.

## Files
- **BridgeProtocol.cpp**: Implementation of the BridgeProtocol component.
- **BridgeProtocol.h**: BridgeProtocol -- the rules of the bridge between the page and the core.
- **ExtensionHostSpawner.cpp**: Implementation of the ExtensionHostSpawner component.
- **ExtensionHostSpawner.h**: Header for the ExtensionHostSpawner component.
- **IPCClient.cpp**: The security of the pipe is built with the text form of a descriptor, so the helper of the system is needed.
- **IPCClient.h**: Header for the IPCClient component.
- **TODO.md**: Open questions and planned work for this folder.
- **VSCodeProtocol.cpp**: Implementation of the VSCodeProtocol component.
- **VSCodeProtocol.h**: Header for the VSCodeProtocol component.
- **VsCodeProxyFallbacks.generated.h**: Header for the VsCodeProxyFallbacks generated component.
- **VsCodeProxyIds.generated.h**: Header for the VsCodeProxyIds generated component.

## Public Methods
- **bool isSaneMethodName(const std::string&)**
- **bool isAllowedBridgeMethod(const std::string&)**
- **std::shared_ptr<JsonNode> parseBridgeMessage(const std::string&)**
- **std::shared_ptr<JsonNode> parseBridgeArguments(const std::string&)**
- **std::string bridgeStringAt(const std::shared_ptr<JsonNode>&, size_t)**
- **bool bridgeBoolAt(const std::shared_ptr<JsonNode>&, size_t, bool)**
- **std::shared_ptr<JsonNode> parseBridgePayload(const std::string&)**
- **bool checkBridgeParams(const std::string&, const std::shared_ptr<JsonNode>&)**
- **bool ExtensionHostSpawner::spawn(const std::string&, std::string&)**
- **std::string ExtensionHostSpawner::pipePath(const std::string&)**
- **std::filesystem::path ExtensionHostSpawner::nodePath()**
- **std::filesystem::path ExtensionHostSpawner::bootstrapperPath()**
- **IPCClient::IPCClient(const std::string&)**
- **IPCClient::~IPCClient()**
- **IPCClient::start()**
- **IPCClient::stop()**
- **IPCClient::sendInitData(const std::shared_ptr<vrutti::core::utils::JsonNode>&)**
- **int IPCClient::sendRequest(int, const std::string&, const std::shared_ptr<vrutti::core::utils::JsonNode>&, std::function<void(const std::shared_ptr<vrutti::core::utils::JsonNode>&)>, std::function<void(const std::string&, const std::string&)>, int)**
- **IPCClient::sendReply(int, const std::shared_ptr<vrutti::core::utils::JsonNode>&)**
- **IPCClient::sendReplyError(int, const std::string&, const std::string&)**
- **IPCClient::sendAck(int)**
- **IPCClient::sendCancel(int)**
- **IPCClient::sendTerminate()**
- **IPCClient::setOnReady(std::function<void()>)**
- **IPCClient::setOnRequest(std::function<void(const RpcRequest&)>)**
- **IPCClient::setOnEvent(std::function<void(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params)>)**
- **IPCClient::emitEvent(const std::string&, const std::shared_ptr<vrutti::core::utils::JsonNode>&)**
- **IPCClient::setOnLocalRequest(std::function<void(const std::string& method, const std::shared_ptr<vrutti::core::utils::JsonNode>& params)>)**
- **IPCClient::sendLocalRequest(const std::string&, const std::shared_ptr<vrutti::core::utils::JsonNode>&)**
- **IPCClient::setOnConnected(std::function<void()>)**
- **IPCClient::bindEditorBuffer(vrutti::core::editor::PieceTable*)**
- **IPCClient::resolvePending(int, const std::shared_ptr<vrutti::core::utils::JsonNode>&, bool, const std::string&)**
- **IPCClient::handleIncomingMessage(const std::string&)**
- **bool IPCClient::decodeVSBuffer(const std::shared_ptr<vrutti::core::utils::JsonNode>&, std::vector<uint8_t>&)**
- **std::shared_ptr<vrutti::core::utils::JsonNode> IPCClient::encodeVSBuffer(const std::vector<uint8_t>&)**
- **VSCodeProtocol::VSCodeProtocol(IPCClient*)**
- **VSCodeProtocol::~VSCodeProtocol()**
- **VSCodeProtocol::initialize()**
- **VSCodeProtocol::handleRequest(const IPCClient::RpcRequest&)**
- **bool VSCodeProtocol::hasHandler(const std::string&, const std::string&)**
- **VSCodeProtocol::handleLocalRequest(const std::string&, const std::shared_ptr<JsonNode>&)**
- **int VSCodeProtocol::sendRequest(int, const std::string&, const std::shared_ptr<JsonNode>&, std::function<void(const std::shared_ptr<JsonNode>&)>, std::function<void(const std::string&, const std::string&)>)**
- **int VSCodeProtocol::sendRequest(const char*, const std::string&, const std::shared_ptr<JsonNode>&, std::function<void(const std::shared_ptr<JsonNode>&)>, std::function<void(const std::string&, const std::string&)>)**
- **VSCodeProtocol::setWorkspaceFolder(const std::string&)**
- **const std::string& VSCodeProtocol::workspaceFolder()**
- **VSCodeProtocol::emitToUi(const std::string&, const std::shared_ptr<JsonNode>&)**
- **VSCodeProtocol::setOnHandshakeComplete(std::function<void()>)**
- **VSCodeProtocol::setAsyncHandler(const std::string&, const std::string&, AsyncHandler)**
- **const char* shapeName(int)**
- **int shapeId(const char*)**
