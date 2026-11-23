# utils (src/core/utils/)

## Purpose
Shared helpers: the JSON parser and serializer, path resolution, string pools and the error code registry.

## Files
- **Base64.h**: Header for the Base64 component.
- **ErrorCodes.cpp**: Implementation of the ErrorCodes component.
- **ErrorCodes.h**: Header for the ErrorCodes component.
- **HttpDownload.cpp**: Implementation of the HttpDownload component.
- **HttpDownload.h**: Header for the HttpDownload component.
- **Inflate.cpp**: Implementation of the Inflate component.
- **Inflate.h**: Header for the Inflate component.
- **Json.cpp**: Implementation of the Json component.
- **Json.h**: Header for the Json component.
- **LineScanner.cpp**: Implementation of the LineScanner component.
- **LineScanner.h**: Header for the LineScanner component.
- **Paths.cpp**: Implementation of the Paths component.
- **Paths.h**: Header for the Paths component.
- **StringPool.cpp**: Implementation of the StringPool component.
- **StringPool.h**: Header for the StringPool component.
- **TODO.md**: Open questions and planned work for this folder.
- **ZipArchive.cpp**: Implementation of the ZipArchive component.
- **ZipArchive.h**: Header for the ZipArchive component.

## Public Methods
- **std::string base64_encode(const std::string&)**
- **std::string base64_decode(const std::string&)**
- **std::string errorCodeId(ErrorCode)**
- **const char* errorCodeName(ErrorCode)**
- **std::string formatError(ErrorCode, const std::string&)**
- **reportError(const char*, ErrorCode, const std::string&)**
- **bool HttpDownload::download(const std::string&, const std::string&, std::string&, size_t, std::string&)**
- **bool HttpDownload::isAllowedUrl(const std::string&)**
- **std::string HttpDownload::uniqueFileName(const std::string&, const std::string&)**
- **uint32_t crc32(const uint8_t*, size_t, uint32_t)**
- **bool rawInflate(const uint8_t*, size_t, std::string&, size_t, std::string&)**
- **std::shared_ptr<JsonNode> JsonNode::get(std::string_view)**
- **std::shared_ptr<JsonNode> makeJsonString(std::string)**
- **std::shared_ptr<JsonNode> JsonParser::parse(const std::string&)**
- **std::string JsonParser::unescapeString(std::string_view)**
- **std::string JsonSerializer::stringify(const std::shared_ptr<JsonNode>&, int, bool)**
- **std::string JsonSerializer::escapeString(std::string_view)**
- **std::vector<size_t> LineScanner::computeLineStarts(std::string_view)**
- **std::filesystem::path appBasePath()**
- **std::filesystem::path appResourcePath(const std::string&)**
- **std::filesystem::path pluginPath(const std::string&)**
- **StringPool::StringPool()**
- **StringPool::~StringPool()**
- **std::string_view StringPool::intern(std::string_view)**
- **ZipArchive::ZipArchive()**
- **bool ZipArchive::open(const std::string&, std::string&)**
- **bool ZipArchive::openFromMemory(std::vector<uint8_t>, std::string&)**
- **const std::vector<ZipEntry>& ZipArchive::entries()**
- **bool ZipArchive::has(const std::string&)**
- **bool ZipArchive::read(const ZipEntry&, std::string&, std::string&)**
- **bool ZipArchive::extractAll(const std::string&, std::string&)**
- **bool ZipArchive::extractAll(const std::string&, const std::string&, std::string&)**
- **bool ZipArchive::isSafeEntryName(const std::string&)**
