# fs (src/core/fs/)

## Purpose
File system services: watching, reading and writing files.

## Files
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
