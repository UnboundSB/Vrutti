#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vrutti::core::utils {

    // -------------------------------------------------------------------------
    // The reader of a ZIP archive.
    //
    // A `.vsix` file is a ZIP archive: a set of entries, each of them a file
    // that is either stored as it is or compressed with DEFLATE. The reader
    // below is deliberately small and strict:
    //
    //   * it never trusts a size that is written in the archive -- the output
    //     is bounded while it is produced, and the CRC of every entry is
    //     checked,
    //   * it never writes outside of the folder it was told to write to.
    // -------------------------------------------------------------------------

    // One file of an archive.
    struct ZipEntry {
        std::string name;            // the name of the file in the archive
        uint16_t method = 0;         // 0 == stored, 8 == deflate
        uint32_t crc = 0;            // the CRC-32 of the content
        uint64_t compressedSize = 0;
        uint64_t uncompressedSize = 0;
        uint64_t localHeaderOffset = 0;
        bool isDirectory = false;    // the name ends with a slash
    };

    // The reader of a ZIP archive.
    class ZipArchive {
    public:
        // The limits of an archive.
        //
        // They are the defence against a hostile file: an archive may not
        // hold an endless number of entries, may not expand to an endless
        // size, and may not be a tiny file that expands to gigabytes.
        struct Limits {
            size_t maxEntries = 20000;
            uint64_t maxEntryUncompressed = 64ull * 1024 * 1024;
            uint64_t maxTotalUncompressed = 256ull * 1024 * 1024;
            // A ratio of 500 means: 1 byte of archive may not become more
            // than 500 bytes of content. A ratio of 0 turns the check off.
            uint64_t maxCompressionRatio = 500;
        };

        ZipArchive() = default;

        // Opens an archive and reads its table of contents.
        //
        // The file is not decompressed here, only its directory is read.
        // Returns false when the file is not a ZIP archive or when it breaks
        // one of the rules of the format. The reason is in `error`.
        bool open(const std::string& path, std::string& error);

        // Opens an archive that is already in memory.
        bool openFromMemory(std::vector<uint8_t> data, std::string& error);

        // The entries of the archive, in the order of the archive.
        const std::vector<ZipEntry>& entries() const { return m_entries; }

        // True when the archive holds an entry with this name.
        bool has(const std::string& name) const;

        // Reads the content of one entry.
        //
        // The CRC of the content is checked, and the content may not be
        // bigger than the limits. Returns false when the entry is broken;
        // the reason is in `error`.
        bool read(const ZipEntry& entry,
                  std::string& out,
                  std::string& error);

        // Writes every entry of the archive into a folder.
        //
        // The names of the entries are checked: an entry may not leave the
        // folder (`..`, an absolute path or a drive letter is refused), so a
        // hostile archive cannot write outside of it.
        //
        // Returns false when the archive cannot be written safely. The reason
        // is in `error`.
        bool extractAll(const std::string& destinationDirectory,
                        std::string& error);

        // Writes the entries of the archive that are under a folder of the
        // archive into a folder of the disk, and drops the name of that
        // folder.
        //
        // A `.vsix` file holds the extension itself under `extension/`, next
        // to files that belong to the package (`[Content_Types].xml`, the
        // manifest of the package). Only the files of the extension are
        // wanted, and they are written without the folder in front of them.
        //
        // An entry that is not under the folder is skipped. An entry that
        // leaves the folder is refused.
        bool extractAll(const std::string& destinationDirectory,
                        const std::string& prefix,
                        std::string& error);

        // The name of a path is safe to write inside a folder: it is not
        // absolute, it has no parent part and it has no drive letter.
        static bool isSafeEntryName(const std::string& name);

    private:
        bool parseCentralDirectory(std::string& error);
        bool readEntryData(const ZipEntry& entry,
                           std::string& out,
                           std::string& error) const;

        std::vector<uint8_t> m_data;
        std::vector<ZipEntry> m_entries;
        Limits m_limits;
    };

} // namespace vrutti::core::utils
