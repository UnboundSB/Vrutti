// -----------------------------------------------------------------------------
// TestZip -- a writer of ZIP archives for the tests.
//
// The tests need archives that a normal program would never make: an
// archive with a file that writes outside of its folder, an archive whose
// checksum does not match its content, an archive with a file that expands
// to gigabytes. The writer is here so that every test can build the
// archive it needs.
//
// The archives are plain: the files are stored, not compressed. The one
// exception is the file that is added with `addDeflated`, which holds a
// DEFLATE stream that was made somewhere else.
// -----------------------------------------------------------------------------
#pragma once

#include "../../core/utils/Inflate.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace vrutti::tests {

class TestZip {
public:
    struct Entry {
        std::string name;
        std::string content;   // the content of the file
        std::string raw;       // what is really written in the archive
        uint16_t method = 0;   // 0 == stored, 8 == deflate
        uint32_t crc = 0;
        uint64_t localOffset = 0;
    };

    void add(const std::string& name, const std::string& content) {
        Entry entry;
        entry.name = name;
        entry.content = content;
        entry.raw = content;
        entry.method = 0;
        entry.crc = vrutti::core::utils::crc32(
            reinterpret_cast<const uint8_t*>(content.data()), content.size());
        m_entries.push_back(entry);
    }

    // Adds a file whose content is a DEFLATE stream that was made somewhere
    // else. The CRC of the *content* is given, because that is what the
    // archive has to hold.
    void addDeflated(const std::string& name,
                     const std::string& raw,
                     const std::string& content,
                     uint32_t crc) {
        Entry entry;
        entry.name = name;
        entry.content = content;
        entry.raw = raw;
        entry.method = 8;
        entry.crc = crc;
        m_entries.push_back(entry);
    }

    // Adds a file with a name that does not belong in an archive.
    void addRaw(const std::string& name, const std::string& content, uint32_t crc) {
        Entry entry;
        entry.name = name;
        entry.content = content;
        entry.raw = content;
        entry.method = 0;
        entry.crc = crc;
        m_entries.push_back(entry);
    }

    // Changes the checksum of a file, so that the content and the checksum
    // do not match any more.
    void corruptChecksumOf(const std::string& name) {
        for (auto& entry : m_entries) {
            if (entry.name == name) entry.crc ^= 0xFFFFu;
        }
    }

    // Writes the archive to a file.
    bool write(const std::string& path) const {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;

        uint32_t offset = 0;
        std::vector<uint32_t> localOffsets;
        localOffsets.reserve(m_entries.size());
        for (const auto& entry : m_entries) {
            localOffsets.push_back(offset);
            const uint16_t nameLength = static_cast<uint16_t>(entry.name.size());
            const uint32_t crc = entry.crc;
            const uint32_t compressed = static_cast<uint32_t>(entry.raw.size());
            const uint32_t uncompressed = static_cast<uint32_t>(entry.content.size());

            writeU32(out, 0x04034b50);
            writeU16(out, 20);            // the version that made the file
            writeU16(out, 0);             // the flags
            writeU16(out, entry.method);
            writeU16(out, 0);             // the time
            writeU16(out, 0);             // the day
            writeU32(out, crc);
            writeU32(out, compressed);
            writeU32(out, uncompressed);
            writeU16(out, nameLength);
            writeU16(out, 0);             // the extra field
            out.write(entry.name.data(), nameLength);
            out.write(entry.raw.data(), static_cast<std::streamsize>(entry.raw.size()));
            offset += 30 + nameLength + static_cast<uint32_t>(entry.raw.size());
        }

        const uint32_t directoryOffset = offset;
        for (size_t i = 0; i < m_entries.size(); ++i) {
            const auto& entry = m_entries[i];
            const uint16_t nameLength = static_cast<uint16_t>(entry.name.size());
            writeU32(out, 0x02014b50);
            writeU16(out, 20);            // the version that made the file
            writeU16(out, 20);            // the version that needs the file
            writeU16(out, 0);             // the flags
            writeU16(out, entry.method);
            writeU16(out, 0);
            writeU16(out, 0);
            writeU32(out, entry.crc);
            writeU32(out, static_cast<uint32_t>(entry.raw.size()));
            writeU32(out, static_cast<uint32_t>(entry.content.size()));
            writeU16(out, nameLength);
            writeU16(out, 0);             // the extra field
            writeU16(out, 0);             // the comment
            writeU16(out, 0);             // the disk
            writeU16(out, 0);             // the internal attributes
            writeU32(out, 0);             // the external attributes
            writeU32(out, localOffsets[i]);
            out.write(entry.name.data(), nameLength);
        }
        const uint32_t directorySize = offset - directoryOffset;

        writeU32(out, 0x06054b50);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, static_cast<uint16_t>(m_entries.size()));
        writeU16(out, static_cast<uint16_t>(m_entries.size()));
        writeU32(out, directorySize);
        writeU32(out, directoryOffset);
        writeU16(out, 0);                 // the comment
        return out.good();
    }

private:
    static void writeU16(std::ofstream& out, uint16_t value) {
        const char bytes[2] = {static_cast<char>(value & 0xFF),
                               static_cast<char>((value >> 8) & 0xFF)};
        out.write(bytes, 2);
    }
    static void writeU32(std::ofstream& out, uint32_t value) {
        const char bytes[4] = {static_cast<char>(value & 0xFF),
                               static_cast<char>((value >> 8) & 0xFF),
                               static_cast<char>((value >> 16) & 0xFF),
                               static_cast<char>((value >> 24) & 0xFF)};
        out.write(bytes, 4);
    }

    std::vector<Entry> m_entries;
};

// The manifest of a test extension.
inline std::string manifestOf(const std::string& name,
                              const std::string& publisher,
                              const std::string& version,
                              const std::string& engines = "^1.90.0",
                              const std::string& main = "./extension.js") {
    return "{\n"
           "  \"name\": \"" + name + "\",\n"
           "  \"publisher\": \"" + publisher + "\",\n"
           "  \"version\": \"" + version + "\",\n"
           "  \"displayName\": \"" + name + "\",\n"
           "  \"description\": \"An extension for the tests\",\n"
           "  \"engines\": { \"vscode\": \"" + engines + "\" },\n"
           "  \"main\": \"" + main + "\",\n"
           "  \"activationEvents\": [ \"onStartupFinished\" ]\n"
           "}\n";
}

} // namespace vrutti::tests
