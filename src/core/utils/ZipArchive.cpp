#include "ZipArchive.h"
#include "Inflate.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace vrutti::core::utils {

namespace {

    // The signatures of the ZIP format.
    constexpr uint32_t SIG_LOCAL_HEADER = 0x04034b50;
    constexpr uint32_t SIG_CENTRAL_HEADER = 0x02014b50;
    constexpr uint32_t SIG_END_OF_CENTRAL = 0x06054b50;
    constexpr uint32_t SIG_ZIP64_END = 0x06064b50;
    constexpr uint32_t SIG_ZIP64_LOCATOR = 0x07064b50;

    uint16_t readU16(const std::vector<uint8_t>& data, size_t offset) {
        return static_cast<uint16_t>(data[offset] | (data[offset + 1] << 8));
    }

    uint32_t readU32(const std::vector<uint8_t>& data, size_t offset) {
        return static_cast<uint32_t>(data[offset]) |
               (static_cast<uint32_t>(data[offset + 1]) << 8) |
               (static_cast<uint32_t>(data[offset + 2]) << 16) |
               (static_cast<uint32_t>(data[offset + 3]) << 24);
    }

    uint64_t readU64(const std::vector<uint8_t>& data, size_t offset) {
        return static_cast<uint64_t>(readU32(data, offset)) |
               (static_cast<uint64_t>(readU32(data, offset + 4)) << 32);
    }

    // The name of an entry is written with a slash. A backslash is a path
    // separator on Windows, so it is refused: a name with a backslash is a
    // name that means something different on the two systems, and that is
    // exactly how a hostile archive slips a file past the check.
    bool hasForbiddenCharacter(const std::string& name) {
        for (unsigned char c : name) {
            if (c == '\\') return true;
            if (c < 0x20) return true;
            if (c == 0x7f) return true;
        }
        return false;
    }

} // namespace

// ---------------------------------------------------------------------------
// Opening an archive
// ---------------------------------------------------------------------------

bool ZipArchive::open(const std::string& path, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error = "the file could not be opened";
        return false;
    }
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0) {
        error = "the file could not be read";
        return false;
    }
    input.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (size > 0) {
        input.read(reinterpret_cast<char*>(data.data()), size);
        if (input.gcount() != size) {
            error = "the file could not be read";
            return false;
        }
    }
    return openFromMemory(std::move(data), error);
}

bool ZipArchive::openFromMemory(std::vector<uint8_t> data, std::string& error) {
    m_data = std::move(data);
    m_entries.clear();

    if (m_data.size() < 22) {
        error = "the file is too small to be an archive";
        return false;
    }

    // --- the end of the central directory -------------------------------------
    // The directory is at the end of the file, but a comment may follow it,
    // so the search starts at the end and walks backwards.
    const size_t maxComment = 0xFFFF;
    const size_t searchStart = m_data.size() > (22 + maxComment)
        ? m_data.size() - (22 + maxComment)
        : 0;
    size_t endOffset = std::string::npos;
    for (size_t i = m_data.size() - 22 + 1; i-- > searchStart;) {
        if (readU32(m_data, i) == SIG_END_OF_CENTRAL) {
            endOffset = i;
            break;
        }
    }
    if (endOffset == std::string::npos) {
        error = "the file is not an archive";
        return false;
    }

    const uint16_t entryCount = readU16(m_data, endOffset + 10);
    const uint32_t directorySize = readU32(m_data, endOffset + 12);
    const uint32_t directoryOffset = readU32(m_data, endOffset + 16);

    // A ZIP64 archive is bigger than 4 GiB or holds more than 65535 entries.
    // Vrutti does not install those, and the sizes of such an archive are
    // written in a place this reader does not look at, so it is refused
    // instead of being read wrong.
    if (directoryOffset == 0xFFFFFFFFu || directorySize == 0xFFFFFFFFu || entryCount == 0xFFFFu) {
        error = "the archive is too big for this installer";
        return false;
    }
    if (static_cast<uint64_t>(directoryOffset) + directorySize > m_data.size()) {
        error = "the archive is broken: its directory points outside of the file";
        return false;
    }

    // --- the directory itself -------------------------------------------------
    size_t offset = directoryOffset;
    for (uint16_t i = 0; i < entryCount; ++i) {
        if (offset + 46 > m_data.size()) {
            error = "the archive is broken: its directory is cut off";
            return false;
        }
        if (readU32(m_data, offset) != SIG_CENTRAL_HEADER) {
            error = "the archive is broken: its directory is not valid";
            return false;
        }

        ZipEntry entry;
        entry.method = readU16(m_data, offset + 10);
        entry.crc = readU32(m_data, offset + 16);
        entry.compressedSize = readU32(m_data, offset + 20);
        entry.uncompressedSize = readU32(m_data, offset + 24);
        const uint16_t nameLength = readU16(m_data, offset + 28);
        const uint16_t extraLength = readU16(m_data, offset + 30);
        const uint16_t commentLength = readU16(m_data, offset + 32);
        entry.localHeaderOffset = readU32(m_data, offset + 42);

        if (entry.compressedSize == 0xFFFFFFFFu || entry.uncompressedSize == 0xFFFFFFFFu ||
            entry.localHeaderOffset == 0xFFFFFFFFu) {
            error = "the archive is too big for this installer";
            return false;
        }

        const size_t nameOffset = offset + 46;
        if (nameOffset + nameLength > m_data.size()) {
            error = "the archive is broken: the name of an entry is cut off";
            return false;
        }
        entry.name.assign(reinterpret_cast<const char*>(m_data.data() + nameOffset), nameLength);
        entry.isDirectory = !entry.name.empty() && entry.name.back() == '/';

        // The attributes of the entry: a file that is a link is refused, a
        // link could point anywhere, and the installer must not write one.
        const uint32_t externalAttributes = readU32(m_data, offset + 38);
        const uint16_t madeBy = readU16(m_data, offset + 4);
        if ((madeBy >> 8) == 3) { // made by a unix system
            const uint32_t unixMode = externalAttributes >> 16;
            if ((unixMode & 0xF000) == 0xA000) {
                error = "the archive holds a link, which this installer does not write";
                return false;
            }
        }

        if (entry.uncompressedSize > m_limits.maxEntryUncompressed) {
            error = "the archive holds a file that is bigger than the limit";
            return false;
        }
        if (m_limits.maxCompressionRatio > 0 && entry.compressedSize > 0) {
            const uint64_t ratio = entry.uncompressedSize / entry.compressedSize;
            if (ratio > m_limits.maxCompressionRatio) {
                error = "the archive holds a file that expands too much";
                return false;
            }
        }

        m_entries.push_back(entry);
        offset = nameOffset + nameLength + extraLength + commentLength;

        if (m_entries.size() > m_limits.maxEntries) {
            error = "the archive holds too many files";
            return false;
        }
    }

    return true;
}

bool ZipArchive::has(const std::string& name) const {
    for (const auto& entry : m_entries) {
        if (entry.name == name) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// The content of an entry
// ---------------------------------------------------------------------------

bool ZipArchive::readEntryData(const ZipEntry& entry,
                                std::string& out,
                                std::string& error) const {
    const uint64_t headerOffset = entry.localHeaderOffset;
    if (headerOffset + 30 > m_data.size()) {
        error = "the archive is broken: the header of a file is outside of the file";
        return false;
    }
    if (readU32(m_data, static_cast<size_t>(headerOffset)) != SIG_LOCAL_HEADER) {
        error = "the archive is broken: the header of a file is not valid";
        return false;
    }
    const uint16_t nameLength = readU16(m_data, static_cast<size_t>(headerOffset) + 26);
    const uint16_t extraLength = readU16(m_data, static_cast<size_t>(headerOffset) + 28);
    const uint64_t dataOffset = headerOffset + 30 + nameLength + extraLength;
    if (dataOffset + entry.compressedSize > m_data.size()) {
        error = "the archive is broken: the content of a file is outside of the file";
        return false;
    }

    const uint8_t* data = m_data.data() + dataOffset;

    if (entry.method == 0) {
        // The file is stored as it is.
        out.assign(reinterpret_cast<const char*>(data), static_cast<size_t>(entry.compressedSize));
    } else if (entry.method == 8) {
        // The file is compressed with DEFLATE.
        const size_t limit = static_cast<size_t>(
            std::min<uint64_t>(m_limits.maxEntryUncompressed, entry.uncompressedSize + 1024));
        if (!rawInflate(data, static_cast<size_t>(entry.compressedSize), out, limit, error)) {
            return false;
        }
    } else {
        error = "the archive uses a compression that this installer does not know";
        return false;
    }

    if (out.size() != entry.uncompressedSize) {
        error = "the archive is broken: the size of a file does not match its header";
        return false;
    }

    // The CRC tells whether the content is the content that was written when
    // the archive was made.
    const uint32_t actual = crc32(reinterpret_cast<const uint8_t*>(out.data()), out.size());
    if (actual != entry.crc) {
        error = "the archive is broken: the checksum of a file does not match";
        return false;
    }
    return true;
}

bool ZipArchive::read(const ZipEntry& entry,
                      std::string& out,
                      std::string& error) {
    out.clear();
    return readEntryData(entry, out, error);
}

// ---------------------------------------------------------------------------
// Writing an archive to the disk
// ---------------------------------------------------------------------------

bool ZipArchive::isSafeEntryName(const std::string& name) {
    if (name.empty()) return false;
    if (hasForbiddenCharacter(name)) return false;

    // An absolute path is refused: on windows a name may start with a
    // slash or with a drive letter.
    if (name[0] == '/' || name[0] == '\\') return false;
    if (name.size() >= 2 && name[1] == ':') return false;

    // No part of the name may be a parent part.
    size_t start = 0;
    while (start <= name.size()) {
        const size_t slash = name.find('/', start);
        const std::string part = name.substr(start, slash == std::string::npos
            ? std::string::npos : slash - start);
        if (part == "..") return false;
        if (part.empty() && start != 0 && slash != std::string::npos) return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

bool ZipArchive::extractAll(const std::string& destinationDirectory,
                            std::string& error) {
    return extractAll(destinationDirectory, "", error);
}

bool ZipArchive::extractAll(const std::string& destinationDirectory,
                            const std::string& prefix,
                            std::string& error) {
    std::error_code ec;
    fs::path destination(destinationDirectory);
    fs::create_directories(destination, ec);
    if (ec && !fs::exists(destination)) {
        error = "the folder of the extension could not be created";
        return false;
    }

    // The folder that every file has to stay inside of.
    const fs::path root = fs::weakly_canonical(destination, ec);
    if (ec) {
        error = "the folder of the extension could not be resolved";
        return false;
    }

    uint64_t totalWritten = 0;
    for (const auto& entry : m_entries) {
        // The entries that are not under the folder of the extension are not
        // wanted, so they are skipped. Their names are not checked: they are
        // never written.
        std::string name = entry.name;
        if (!prefix.empty()) {
            if (name.size() <= prefix.size() ||
                name.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            name = name.substr(prefix.size());
        }

        if (!isSafeEntryName(name)) {
            error = "the archive holds a file with a name that leaves the folder";
            return false;
        }

        // The path of the file, built from the parts of the name. A part that
        // is "." is skipped, a part that is ".." is refused above.
        fs::path target = root;
        size_t start = 0;
        while (start <= name.size()) {
            const size_t slash = name.find('/', start);
            const std::string part = name.substr(start, slash == std::string::npos
                ? std::string::npos : slash - start);
            if (!part.empty() && part != ".") {
                target /= part;
            }
            if (slash == std::string::npos) break;
            start = slash + 1;
        }

        // The last check: the file has to be inside of the folder, after
        // every path has been resolved.
        const fs::path resolved = fs::weakly_canonical(target, ec);
        if (ec) {
            error = "the path of a file of the archive could not be resolved";
            return false;
        }
        const std::string resolvedString = resolved.generic_string();
        const std::string rootString = root.generic_string();
        if (resolvedString != rootString &&
            resolvedString.rfind(rootString + "/", 0) != 0) {
            error = "the archive holds a file with a name that leaves the folder";
            return false;
        }

        if (entry.isDirectory) {
            fs::create_directories(target, ec);
            if (ec) {
                error = "a folder of the archive could not be created";
                return false;
            }
            continue;
        }

        fs::create_directories(target.parent_path(), ec);
        if (ec && !fs::exists(target.parent_path())) {
            error = "a folder of the archive could not be created";
            return false;
        }

        std::string content;
        if (!readEntryData(entry, content, error)) return false;

        totalWritten += content.size();
        if (totalWritten > m_limits.maxTotalUncompressed) {
            error = "the archive expands to more than the limit";
            return false;
        }

        std::ofstream output(target, std::ios::binary | std::ios::trunc);
        if (!output.is_open()) {
            error = "a file of the archive could not be written";
            return false;
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.close();
        if (!output) {
            error = "a file of the archive could not be written";
            return false;
        }
    }

    return true;
}

} // namespace vrutti::core::utils
