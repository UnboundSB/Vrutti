#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vrutti::core::fs {

    // -------------------------------------------------------------------------
    // The kinds of a file system entry.
    //
    // The values are the ones of the upstream `FileType` enum, because the
    // extension host compares them against its own copy. An extension that
    // asks for the type of a file has to get the same number here as it would
    // get anywhere else.
    // -------------------------------------------------------------------------
    enum FileType {
        FileTypeUnknown = 0,
        FileTypeFile = 1,
        FileTypeDirectory = 2,
        FileTypeSymbolicLink = 64
    };

    // The permission bits. The values are the ones of the upstream
    // `FilePermission` enum.
    enum FilePermission {
        FilePermissionReadonly = 1,
        FilePermissionLocked = 2,
        FilePermissionExecutable = 4
    };

    // The stat of a resource. The shape is the one of the upstream `IStat`.
    struct FileStat {
        int type = FileTypeUnknown;
        uint64_t mtime = 0;   // milliseconds since the unix epoch
        uint64_t ctime = 0;
        uint64_t size = 0;
        int permissions = 0;  // 0 when there is nothing to report
    };

    // One entry of a directory.
    struct DirectoryEntry {
        std::string name;
        int type = FileTypeUnknown;
    };

    // The result of an operation.
    //
    // `errorCode` carries the upstream `FileSystemProviderErrorCode`, so the
    // extension sees the same failure it would see from any other file
    // system provider: `FileNotFound`, `FileExists`, `FileNotADirectory`,
    // `FileIsADirectory`, `NoPermissions` or `Unavailable`.
    struct FileResult {
        bool ok = false;
        std::string errorMessage;
        std::string errorCode;

        static FileResult success() {
            FileResult r;
            r.ok = true;
            return r;
        }

        static FileResult failure(const std::string& code, const std::string& message) {
            FileResult r;
            r.ok = false;
            r.errorCode = code;
            r.errorMessage = message;
            return r;
        }
    };

    // -------------------------------------------------------------------------
    // The native file system of the core.
    //
    // These are the operations behind `vscode.workspace.fs`. The extension
    // host calls them through the `MainThreadFileSystem` actor, so an
    // extension that reads or writes a file goes through this class.
    //
    // Every method works on a native path. Turning a URI into a path is the
    // job of `uriToNativePath`, and deciding whether a URI belongs to this
    // file system or to a provider of an extension is the job of the router.
    // -------------------------------------------------------------------------
    class FileService {
    public:
        FileService() = default;

        // Reads the stat of a path.
        FileResult stat(const std::string& path, FileStat& out) const;

        // Lists the entries of a directory.
        FileResult readDirectory(const std::string& path,
                                 std::vector<DirectoryEntry>& out) const;

        // Reads the bytes of a file.
        FileResult readFile(const std::string& path, std::vector<uint8_t>& out) const;

        // Writes the bytes of a file. The directories it needs are created.
        FileResult writeFile(const std::string& path,
                             const std::vector<uint8_t>& content) const;

        // Renames a file or a directory. When `overwrite` is false and the
        // target exists, the operation fails with `FileExists`.
        FileResult rename(const std::string& from, const std::string& to,
                          bool overwrite) const;

        // Copies a file or a directory.
        FileResult copy(const std::string& from, const std::string& to,
                        bool overwrite) const;

        // Creates a directory. When it already exists, the operation fails
        // with `FileExists`, which is what the extension host expects.
        FileResult createDirectory(const std::string& path) const;

        // Deletes a file or a directory.
        FileResult remove(const std::string& path, bool recursive) const;
    };

} // namespace vrutti::core::fs
