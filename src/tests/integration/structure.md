# integration (src/tests/integration/)

## Purpose
Integration tests: they exercise the services through their public interfaces.

## Files
- **ExthostFileSystemTest.cpp**: ExthostFileSystemTest -- proves that an extension can use `vscode.workspace.fs`.
- **ExtHostIntegrationTest.cpp**: Implementation of the ExtHostIntegrationTest component.
- **ExtHostPipeClient.h**: The pipe client of the integration tests.
- **ProtocolFallbackTest.cpp**: ProtocolFallbackTest -- proves the contract of the extension host router.
- **ThemeServiceTest.cpp**: Integration test for the theme service.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **PipeClient::PipeClient()**
- **PipeClient::~PipeClient()**
- **bool PipeClient::connect(const std::string&, int)**
- **PipeClient::Sleep(20)**
- **disconnect()**
- **CloseHandle(m_handle)**
- **bool sendLine(const std::string&)**
- **bool readLine(std::string&, int)**
- **m_pending. append(buffer, got)**
- **Sleep(5)**
- **bool lastBuffer(std::vector<uint8_t>&)**
- **std::shared_ptr<JsonNode> awaitReply(PipeClient&, int, int)**
- **const std::string type(typeNode->stringValue.data(), typeNode->stringValue.length())**
- **std::string frameType(const std::shared_ptr<JsonNode>&)**
- **std::string resultKind(const std::shared_ptr<JsonNode>&)**
- **std::string errorName(const std::shared_ptr<JsonNode>&)**
- **std::string errorMessage(const std::shared_ptr<JsonNode>&)**
- **double numberField(const std::shared_ptr<JsonNode>&, const char*, double)**
- **std::string stringField(const std::shared_ptr<JsonNode>&, const char*)**
- **std::string makeBufferArgument(const std::string&)**
- **std::string decodeBufferResult(const std::shared_ptr<JsonNode>&)**
