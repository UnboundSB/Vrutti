# fs (src/core/fs/)

## Purpose
File system services: watching, reading and writing files.

## Files
- **FileService.cpp**: Implementation of the FileService component.
- **FileService.h**: Header for the FileService component.
- **Glob.cpp**: Implementation of the Glob component.
- **Glob.h**: Header for the Glob component.
- **Path.cpp**: Implementation of the Path component.
- **Path.h**: Header for the Path component.
- **Stream.h**: Header for the Stream component.
- **TODO.md**: Open questions and planned work for this folder.
- **URI.cpp**: Implementation of the URI component.
- **URI.h**: Header for the URI component.
- **Workspace.cpp**: Implementation of the Workspace component.
- **Workspace.h**: Header for the Workspace component.

## Public Methods
- **FileResult FileResult::success()**
- **FileResult FileResult::failure(const std::string&, const std::string&)**
- **FileService::FileService()**
- **FileResult FileService::stat(const std::string&, FileStat&)**
- **FileResult FileService::readDirectory(const std::string&, std::vector<DirectoryEntry>&)**
- **FileResult FileService::readFile(const std::string&, std::vector<uint8_t>&)**
- **FileResult FileService::writeFile(const std::string&, const std::vector<uint8_t>&)**
- **FileResult FileService::rename(const std::string&, const std::string&, bool)**
- **FileResult FileService::copy(const std::string&, const std::string&, bool)**
- **FileResult FileService::createDirectory(const std::string&)**
- **FileResult FileService::remove(const std::string&, bool)**
- **bool Glob::match(const std::string&, const std::string&)**
- **std::string Path::basename(const std::string&)**
- **std::string Path::dirname(const std::string&)**
- **std::string Path::extname(const std::string&)**
- **std::string Path::join(const std::vector<std::string>&)**
- **std::string Path::resolve(const std::string&, const std::string&)**
- **std::string Path::normalize(const std::string&)**
- **bool Path::isAbsolute(const std::string&)**
- **IReadableStream::~IReadableStream()**
- **vrutti::core::events::Event<StreamChunk>& IReadableStream::onData()**
- **vrutti::core::events::Event<std::string>& IReadableStream::onError()**
- **vrutti::core::events::Event<void>& IReadableStream::onEnd()**
- **IReadableStream::pause()**
- **IReadableStream::resume()**
- **IReadableStream::destroy()**
- **StreamBuffer::append(const StreamChunk&)**
- **std::string StreamBuffer::toString()**
- **StreamBuffer::clear()**
- **std::string uriToNativePath(const UriComponents&)**
- **UriComponents nativePathToUri(const std::string&)**
- **URI::URI(std::string)**
- **URI::URI()**
- **std::string_view URI::scheme()**
- **std::string_view URI::authority()**
- **std::string_view URI::path()**
- **std::string_view URI::query()**
- **std::string_view URI::fragment()**
- **const std::string& URI::toString()**
- **Workspace::Workspace(const std::string&)**
- **const FileNode& Workspace::getRoot()**
- **FileNode& Workspace::getRootMutable()**
- **Workspace::scanDirectory(FileNode&)**
- **Workspace::unloadDirectory(FileNode&)**
