#include "FileService.h"
#include "../utils/ErrorCodes.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace vrutti::core::fs {

namespace fs = std::filesystem;

namespace {

    // The upstream `FileSystemProviderErrorCode` values. The extension host
    // maps them onto its own `FileSystemError`, so the names have to match
    // exactly.
    constexpr const char* kFileNotFound = "FileNotFound";
    constexpr const char* kFileExists = "FileExists";
    constexpr const char* kFileNotADirectory = "FileNotADirectory";
    constexpr const char* kFileIsADirectory = "FileIsADirectory";
    constexpr const char* kNoPermissions = "NoPermissions";
    constexpr const char* kUnavailable = "Unavailable";

    // The number of bytes that are read at a time when a file is copied.
    constexpr size_t kCopyChunk = 64 * 1024;

    // Turns a `std::error_code` into the failure the extension expects.
    FileResult failureOf(const std::error_code& ec, const std::string& path) {
        if (ec == std::errc::no_such_file_or_directory) {
            return FileResult::failure(kFileNotFound,
                "the file '" + path + "' does not exist");
        }
        if (ec == std::errc::permission_denied) {
            return FileResult::failure(kNoPermissions,
                "the file '" + path + "' cannot be accessed");
        }
        if (ec == std::errc::file_exists) {
            return FileResult::failure(kFileExists,
                "the file '" + path + "' already exists");
        }
        if (ec == std::errc::is_a_directory) {
            return FileResult::failure(kFileIsADirectory,
                "'" + path + "' is a directory");
        }
        if (ec == std::errc::not_a_directory) {
            return FileResult::failure(kFileNotADirectory,
                "'" + path + "' is not a directory");
        }
        return FileResult::failure(kUnavailable,
            "the file '" + path + "' could not be used: " + ec.message());
    }

    // The time of a file, in milliseconds since the unix epoch.
    //
    // The epoch of the file clock is not the unix epoch, so the distance
    // between the two clocks is measured at the moment of the call.
    uint64_t toUnixMillis(fs::file_time_type when) {
        using namespace std::chrono;
        auto nowFile = fs::file_time_type::clock::now();
        auto nowSystem = system_clock::now();
        auto delta = duration_cast<system_clock::duration>(nowFile - when);
        auto unix = nowSystem - delta;
        auto millis = duration_cast<milliseconds>(unix.time_since_epoch()).count();
        return millis < 0 ? 0 : static_cast<uint64_t>(millis);
    }

    // The kind of an entry, with the same rules as the upstream
    // `MainThreadFileSystem._asFileType`: a link carries the kind of its
    // target *and* the bit that says it is a link.
    int fileTypeOf(const fs::path& path) {
        std::error_code ec;
        fs::file_status status = fs::status(path, ec);
        if (ec) return FileTypeUnknown;

        int type = FileTypeUnknown;
        if (fs::is_regular_file(status)) {
            type |= FileTypeFile;
        } else if (fs::is_directory(status)) {
            type |= FileTypeDirectory;
        }

        std::error_code linkEc;
        if (fs::is_symlink(fs::symlink_status(path, linkEc)) && !linkEc) {
            type |= FileTypeSymbolicLink;
        }
        return type;
    }

    // The permissions of an entry, in the shape the extension expects.
    int permissionsOf(const fs::path& path) {
#ifdef _WIN32
        std::error_code ec;
        fs::file_status status = fs::status(path, ec);
        if (ec) return 0;
        // A file that cannot be written to is read only.
        if (fs::exists(status) && !fs::is_directory(status)) {
            std::ifstream probe(path, std::ios::binary);
            if (!probe.good()) return FilePermissionReadonly;
        }
        return 0;
#else
        std::error_code ec;
        fs::permissions permissions = fs::status(path, ec).permissions();
        if (ec) return 0;
        if ((permissions & fs::perms::owner_write) == fs::perms::none &&
            (permissions & fs::perms::group_write) == fs::perms::none &&
            (permissions & fs::perms::others_write) == fs::perms::none) {
            return FilePermissionReadonly;
        }
        return 0;
#endif
    }

    // Copies a file or a directory tree.
    FileResult copyTree(const fs::path& from, const fs::path& to, bool overwrite) {
        std::error_code ec;
        fs::file_status status = fs::symlink_status(from, ec);
        if (ec) return failureOf(ec, from.string());

        if (fs::is_directory(status)) {
            fs::create_directories(to, ec);
            if (ec && ec != std::errc::file_exists) return failureOf(ec, to.string());

            fs::directory_iterator it(from, fs::directory_options::skip_permission_denied, ec);
            if (ec) return failureOf(ec, from.string());
            for (const auto& entry : it) {
                FileResult child = copyTree(entry.path(), to / entry.path().filename(), overwrite);
                if (!child.ok) return child;
            }
            return FileResult::success();
        }

        if (fs::exists(to) && !overwrite) {
            return FileResult::failure(kFileExists,
                "the file '" + to.string() + "' already exists");
        }

        std::ifstream input(from, std::ios::binary);
        if (!input.good()) {
            return FileResult::failure(kNoPermissions,
                "the file '" + from.string() + "' cannot be read");
        }

        std::ofstream output(to, std::ios::binary | std::ios::trunc);
        if (!output.good()) {
            return FileResult::failure(kNoPermissions,
                "the file '" + to.string() + "' cannot be written");
        }

        std::vector<char> buffer(kCopyChunk);
        while (input) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            std::streamsize got = input.gcount();
            if (got > 0) {
                output.write(buffer.data(), got);
                if (!output.good()) {
                    return FileResult::failure(kUnavailable,
                        "the file '" + to.string() + "' could not be written");
                }
            }
        }
        return FileResult::success();
    }

} // namespace

FileResult FileService::stat(const std::string& path, FileStat& out) const {
    if (path.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    fs::file_status status = fs::status(fs::path(path), ec);
    if (ec) return failureOf(ec, path);

    out.type = fileTypeOf(fs::path(path));
    out.size = fs::is_regular_file(status) ? fs::file_size(fs::path(path), ec) : 0;
    if (ec) out.size = 0;

    out.mtime = toUnixMillis(fs::last_write_time(fs::path(path), ec));
    if (ec) out.mtime = 0;

    out.ctime = out.mtime;
#ifdef _WIN32
    // The creation time is available through the platform API.
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExW(fs::path(path).wstring().c_str(), GetFileExInfoStandard, &data)) {
        auto toMillis = [](const FILETIME& ft) -> uint64_t {
            ULARGE_INTEGER value;
            value.LowPart = ft.dwLowDateTime;
            value.HighPart = ft.dwHighDateTime;
            // 100 nanosecond intervals since 1601, in milliseconds since 1970.
            const uint64_t kEpochDelta = 116444736000000000ULL;
            if (value.QuadPart < kEpochDelta) return 0;
            return (value.QuadPart - kEpochDelta) / 10000ULL;
        };
        out.ctime = toMillis(data.ftCreationTime);
        if (out.mtime == 0) out.mtime = toMillis(data.ftLastWriteTime);
    }
#endif

    out.permissions = permissionsOf(fs::path(path));
    return FileResult::success();
}

FileResult FileService::readDirectory(const std::string& path,
                                      std::vector<DirectoryEntry>& out) const {
    out.clear();
    if (path.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    fs::file_status status = fs::status(fs::path(path), ec);
    if (ec) return failureOf(ec, path);
    if (!fs::is_directory(status)) {
        return FileResult::failure(kFileNotADirectory,
            "'" + path + "' is not a directory");
    }

    fs::directory_iterator it(fs::path(path), fs::directory_options::skip_permission_denied, ec);
    if (ec) return failureOf(ec, path);

    for (const auto& entry : it) {
        DirectoryEntry item;
        item.name = entry.path().filename().string();
        item.type = fileTypeOf(entry.path());
        out.push_back(std::move(item));
    }

    // The order of the entries is not defined by the file system, so it is
    // sorted here. An extension that lists a directory gets the same order
    // every time.
    std::sort(out.begin(), out.end(),
              [](const DirectoryEntry& a, const DirectoryEntry& b) {
                  return a.name < b.name;
              });
    return FileResult::success();
}

FileResult FileService::readFile(const std::string& path, std::vector<uint8_t>& out) const {
    out.clear();
    if (path.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    fs::file_status status = fs::status(fs::path(path), ec);
    if (ec) return failureOf(ec, path);
    if (fs::is_directory(status)) {
        return FileResult::failure(kFileIsADirectory,
            "'" + path + "' is a directory");
    }

    std::ifstream input(fs::path(path), std::ios::binary);
    if (!input.good()) {
        return FileResult::failure(kNoPermissions,
            "the file '" + path + "' cannot be read");
    }

    input.seekg(0, std::ios::end);
    std::streamoff size = input.tellg();
    if (size < 0) size = 0;
    input.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(size));
    if (size > 0) {
        input.read(reinterpret_cast<char*>(out.data()), size);
        if (input.gcount() != size) {
            // The file changed while it was read. Whatever was read is
            // returned, which is what a reader can live with.
            out.resize(static_cast<size_t>(input.gcount()));
        }
    }
    return FileResult::success();
}

FileResult FileService::writeFile(const std::string& path,
                                  const std::vector<uint8_t>& content) const {
    if (path.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    fs::path target(path);

    // The directories that are needed are created, so writing a file into a
    // folder that does not exist yet works.
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path(), ec);
        if (ec && ec != std::errc::file_exists) return failureOf(ec, path);
    }

    std::ofstream output(target, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        return FileResult::failure(kNoPermissions,
            "the file '" + path + "' cannot be written");
    }

    if (!content.empty()) {
        output.write(reinterpret_cast<const char*>(content.data()),
                     static_cast<std::streamsize>(content.size()));
    }
    output.flush();
    if (!output.good()) {
        return FileResult::failure(kUnavailable,
            "the file '" + path + "' could not be written");
    }
    return FileResult::success();
}

FileResult FileService::rename(const std::string& from, const std::string& to,
                               bool overwrite) const {
    if (from.empty() || to.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    if (!fs::exists(fs::path(from))) {
        return FileResult::failure(kFileNotFound,
            "the file '" + from + "' does not exist");
    }
    if (fs::exists(fs::path(to)) && !overwrite) {
        return FileResult::failure(kFileExists,
            "the file '" + to + "' already exists");
    }

    fs::rename(fs::path(from), fs::path(to), ec);
    if (ec) {
        // Windows refuses to rename over an existing file.
        if (overwrite && fs::exists(fs::path(to))) {
            std::error_code removeEc;
            fs::remove_all(fs::path(to), removeEc);
            ec.clear();
            fs::rename(fs::path(from), fs::path(to), ec);
        }
    }
    if (ec) return failureOf(ec, from);
    return FileResult::success();
}

FileResult FileService::copy(const std::string& from, const std::string& to,
                             bool overwrite) const {
    if (from.empty() || to.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    if (!fs::exists(fs::path(from))) {
        return FileResult::failure(kFileNotFound,
            "the file '" + from + "' does not exist");
    }
    if (fs::exists(fs::path(to)) && !overwrite) {
        return FileResult::failure(kFileExists,
            "the file '" + to + "' already exists");
    }
    return copyTree(fs::path(from), fs::path(to), overwrite);
}

FileResult FileService::createDirectory(const std::string& path) const {
    if (path.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    if (fs::exists(fs::path(path))) {
        return FileResult::failure(kFileExists,
            "the folder '" + path + "' already exists");
    }
    fs::create_directories(fs::path(path), ec);
    if (ec) return failureOf(ec, path);
    return FileResult::success();
}

FileResult FileService::remove(const std::string& path, bool recursive) const {
    if (path.empty()) {
        return FileResult::failure(kFileNotFound, "no path was given");
    }

    std::error_code ec;
    fs::file_status status = fs::symlink_status(fs::path(path), ec);
    if (ec) return failureOf(ec, path);

    if (fs::is_directory(status)) {
        if (!recursive) {
            // An empty folder can be removed, a folder with content cannot.
            fs::directory_iterator it(fs::path(path), ec);
            if (ec) return failureOf(ec, path);
            if (it != fs::directory_iterator()) {
                return FileResult::failure(kFileIsADirectory,
                    "'" + path + "' is a directory that is not empty");
            }
        }
        fs::remove_all(fs::path(path), ec);
    } else {
        fs::remove(fs::path(path), ec);
    }
    if (ec) return failureOf(ec, path);
    return FileResult::success();
}

} // namespace vrutti::core::fs
