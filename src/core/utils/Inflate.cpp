#include "Inflate.h"

#include <array>
#include <cstring>

namespace vrutti::core::utils {

namespace {

    // -------------------------------------------------------------------------
    // The CRC-32 of ZIP.
    //
    // The table is built once, on the first call. It is the same table as
    // the one of zlib, so the CRC of an entry can be compared with the CRC
    // that any other tool computes.
    // -------------------------------------------------------------------------
    const std::array<uint32_t, 256>& crcTable() {
        static const std::array<uint32_t, 256> table = [] {
            std::array<uint32_t, 256> t{};
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k) {
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                t[i] = c;
            }
            return t;
        }();
        return table;
    }

    // -------------------------------------------------------------------------
    // The bit reader of DEFLATE.
    //
    // DEFLATE is a stream of bits, read from the least significant bit of
    // every byte. The reader never reads past the end of the data: when it
    // runs out, it reports the error instead of walking into memory that is
    // not there.
    // -------------------------------------------------------------------------
    class BitReader {
    public:
        BitReader(const uint8_t* data, size_t size)
            : m_data(data), m_size(size) {}

        // Reads `count` bits (at most 16). The bits that are missing at the
        // end of the data are zero, and the reader is marked as broken.
        uint32_t bits(int count) {
            uint32_t value = m_bitBuffer;
            while (m_bitCount < count) {
                if (m_position >= m_size) {
                    m_broken = true;
                    // The missing bits are zero: the caller checks the flag.
                    break;
                }
                value |= static_cast<uint32_t>(m_data[m_position++]) << m_bitCount;
                m_bitCount += 8;
            }
            m_bitBuffer = value >> count;
            m_bitCount -= count;
            return value & ((1u << count) - 1);
        }

        // Throws the bits that are left in the buffer away, so that the next
        // byte starts at a byte boundary.
        void alignToByte() {
            m_bitBuffer = 0;
            m_bitCount = 0;
        }

        // Reads `count` bytes that are not part of the bit stream.
        bool readBytes(uint8_t* out, size_t count) {
            if (m_position + count > m_size) {
                m_broken = true;
                return false;
            }
            if (out) std::memcpy(out, m_data + m_position, count);
            m_position += count;
            return true;
        }

        bool broken() const { return m_broken; }
        size_t position() const { return m_position; }

    private:
        const uint8_t* m_data;
        size_t m_size;
        size_t m_position = 0;
        uint32_t m_bitBuffer = 0;
        int m_bitCount = 0;
        bool m_broken = false;
    };

    // -------------------------------------------------------------------------
    // The Huffman table of DEFLATE.
    //
    // The table is the "canonical" one of the format: the codes of a length
    // are consecutive, and the symbols are sorted by their code. The decoder
    // walks the lengths one by one, which is slower than a lookup table but
    // short and easy to check.
    // -------------------------------------------------------------------------
    constexpr int MAX_BITS = 15;

    struct Huffman {
        std::array<uint16_t, MAX_BITS + 1> count{};
        std::array<uint16_t, 288> symbol{};
    };

    // Builds the table of the symbols of a list of code lengths.
    //
    // Returns 0 when the table is complete, a positive number when it is not
    // (a code is missing, which is allowed for the distance table) and a
    // negative number when the lengths are wrong (a code is used twice).
    int buildHuffman(Huffman& table, const uint8_t* lengths, int count) {
        table.count.fill(0);
        for (int symbol = 0; symbol < count; ++symbol) {
            if (lengths[symbol] > MAX_BITS) return -1;
            table.count[lengths[symbol]]++;
        }
        if (table.count[0] == count) return 0;

        // The codes may not use more space than the tree allows.
        int left = 1;
        for (int length = 1; length <= MAX_BITS; ++length) {
            left <<= 1;
            left -= table.count[length];
            if (left < 0) return -1;
        }

        uint16_t offsets[MAX_BITS + 2];
        offsets[1] = 0;
        for (int length = 1; length < MAX_BITS; ++length) {
            offsets[length + 1] = static_cast<uint16_t>(offsets[length] + table.count[length]);
        }
        for (int symbol = 0; symbol < count; ++symbol) {
            if (lengths[symbol] != 0) {
                table.symbol[offsets[lengths[symbol]]++] = static_cast<uint16_t>(symbol);
            }
        }
        return left;
    }

    // Reads one symbol of the table.
    int decodeSymbol(BitReader& reader, const Huffman& table) {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int length = 1; length <= MAX_BITS; ++length) {
            code |= static_cast<int>(reader.bits(1));
            if (reader.broken()) return -1;
            const int count = table.count[length];
            if (code - count < first) {
                return table.symbol[index + (code - first)];
            }
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }

    // The lengths of the codes of the literals and of the lengths.
    const uint16_t LENGTH_BASE[29] = {
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    const uint8_t LENGTH_EXTRA[29] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    const uint16_t DISTANCE_BASE[30] = {
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
    const uint8_t DISTANCE_EXTRA[30] = {
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    // Appends `count` bytes to the output, but never past the limit.
    bool appendBounded(std::string& out, size_t count, size_t maxOutput, std::string& error) {
        if (out.size() + count > maxOutput) {
            error = "the content of the archive is bigger than the limit";
            return false;
        }
        out.resize(out.size() + count, '\0');
        return true;
    }

} // namespace

// ---------------------------------------------------------------------------
// The CRC-32 of ZIP
// ---------------------------------------------------------------------------

uint32_t crc32(const uint8_t* data, size_t size, uint32_t seed) {
    const auto& table = crcTable();
    uint32_t crc = seed ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// ---------------------------------------------------------------------------
// The DEFLATE decoder
// ---------------------------------------------------------------------------

bool rawInflate(const uint8_t* data,
                size_t size,
                std::string& out,
                size_t maxOutput,
                std::string& error) {
    if (!data && size > 0) {
        error = "the compressed stream is missing";
        return false;
    }

    BitReader reader(data, size);
    Huffman fixedLiterals;
    Huffman fixedDistances;
    bool fixedReady = false;

    bool last = false;
    while (!last) {
        last = reader.bits(1) != 0;
        const uint32_t type = reader.bits(2);
        if (reader.broken()) {
            error = "the compressed stream ends in the middle of a block";
            return false;
        }

        if (type == 0) {
            // --- a block that is stored as it is ------------------------------
            reader.alignToByte();
            uint8_t header[4];
            if (!reader.readBytes(header, 4)) {
                error = "the compressed stream ends in the middle of a stored block";
                return false;
            }
            const uint16_t length = static_cast<uint16_t>(header[0] | (header[1] << 8));
            const uint16_t inverse = static_cast<uint16_t>(header[2] | (header[3] << 8));
            if (static_cast<uint16_t>(~length) != inverse) {
                error = "a stored block of the archive has a wrong length";
                return false;
            }
            if (!appendBounded(out, length, maxOutput, error)) return false;
            if (length > 0) {
                if (!reader.readBytes(reinterpret_cast<uint8_t*>(&out[out.size() - length]), length)) {
                    error = "the compressed stream ends in the middle of a stored block";
                    return false;
                }
            }
            continue;
        }

        Huffman literals;
        Huffman distances;

        if (type == 1) {
            // --- the fixed tables of the format --------------------------------
            if (!fixedReady) {
                uint8_t lengths[288];
                for (int i = 0; i < 144; ++i) lengths[i] = 8;
                for (int i = 144; i < 256; ++i) lengths[i] = 9;
                for (int i = 256; i < 280; ++i) lengths[i] = 7;
                for (int i = 280; i < 288; ++i) lengths[i] = 8;
                buildHuffman(fixedLiterals, lengths, 288);
                uint8_t distanceLengths[30];
                for (int i = 0; i < 30; ++i) distanceLengths[i] = 5;
                buildHuffman(fixedDistances, distanceLengths, 30);
                fixedReady = true;
            }
            literals = fixedLiterals;
            distances = fixedDistances;
        } else if (type == 2) {
            // --- the tables of the block ---------------------------------------
            const uint32_t literalCount = reader.bits(5) + 257;
            const uint32_t distanceCount = reader.bits(5) + 1;
            const uint32_t codeLengthCount = reader.bits(4) + 4;
            if (reader.broken()) {
                error = "the compressed stream ends in the middle of a block header";
                return false;
            }
            if (literalCount > 286 || distanceCount > 30) {
                error = "a block of the archive has too many codes";
                return false;
            }

            // The order of the lengths of the code lengths is fixed by the
            // format.
            static const uint8_t ORDER[19] = {
                16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            uint8_t codeLengths[19] = {0};
            for (uint32_t i = 0; i < codeLengthCount; ++i) {
                codeLengths[ORDER[i]] = static_cast<uint8_t>(reader.bits(3));
            }
            if (reader.broken()) {
                error = "the compressed stream ends in the middle of a block header";
                return false;
            }

            Huffman codeLengthTable;
            if (buildHuffman(codeLengthTable, codeLengths, 19) != 0) {
                error = "a block of the archive has broken code lengths";
                return false;
            }

            uint8_t lengths[288 + 30];
            std::memset(lengths, 0, sizeof(lengths));
            uint32_t index = 0;
            const uint32_t total = literalCount + distanceCount;
            while (index < total) {
                const int symbol = decodeSymbol(reader, codeLengthTable);
                if (symbol < 0) {
                    error = "a block of the archive has a broken code";
                    return false;
                }
                if (symbol < 16) {
                    lengths[index++] = static_cast<uint8_t>(symbol);
                    continue;
                }
                uint32_t repeat = 0;
                uint8_t value = 0;
                if (symbol == 16) {
                    if (index == 0) {
                        error = "a block of the archive repeats a code length too early";
                        return false;
                    }
                    value = lengths[index - 1];
                    repeat = 3 + reader.bits(2);
                } else if (symbol == 17) {
                    repeat = 3 + reader.bits(3);
                } else {
                    repeat = 11 + reader.bits(7);
                }
                if (reader.broken()) {
                    error = "the compressed stream ends in the middle of a code length";
                    return false;
                }
                if (index + repeat > total) {
                    error = "a block of the archive repeats a code length too often";
                    return false;
                }
                for (uint32_t i = 0; i < repeat; ++i) lengths[index++] = value;
            }

            if (lengths[256] == 0) {
                error = "a block of the archive has no end-of-block code";
                return false;
            }
            if (buildHuffman(literals, lengths, static_cast<int>(literalCount)) < 0) {
                error = "a block of the archive has broken code lengths";
                return false;
            }
            if (buildHuffman(distances, lengths + literalCount, static_cast<int>(distanceCount)) < 0) {
                error = "a block of the archive has broken code lengths";
                return false;
            }
        } else {
            error = "the archive uses a block type that does not exist";
            return false;
        }

        // --- the content of the block ------------------------------------------
        while (true) {
            const int symbol = decodeSymbol(reader, literals);
            if (symbol < 0) {
                error = "a block of the archive has a broken code";
                return false;
            }
            if (symbol < 256) {
                if (!appendBounded(out, 1, maxOutput, error)) return false;
                out[out.size() - 1] = static_cast<char>(symbol);
                continue;
            }
            if (symbol == 256) break; // the end of the block

            const int lengthIndex = symbol - 257;
            if (lengthIndex >= 29) {
                error = "a block of the archive has a broken length code";
                return false;
            }
            const size_t length = LENGTH_BASE[lengthIndex] + reader.bits(LENGTH_EXTRA[lengthIndex]);

            const int distanceSymbol = decodeSymbol(reader, distances);
            if (distanceSymbol < 0 || distanceSymbol >= 30) {
                error = "a block of the archive has a broken distance code";
                return false;
            }
            const size_t distance = DISTANCE_BASE[distanceSymbol] +
                                   reader.bits(DISTANCE_EXTRA[distanceSymbol]);
            if (reader.broken()) {
                error = "the compressed stream ends in the middle of a match";
                return false;
            }
            if (distance > out.size()) {
                error = "a block of the archive points before the start of the content";
                return false;
            }
            if (!appendBounded(out, length, maxOutput, error)) return false;

            // The match may overlap with the content that was just written,
            // so the copy is done byte by byte: every byte is read from the
            // content that is already there, and the bytes that were just
            // written are the ones that the copy reads from.
            const size_t base = out.size() - length;
            const size_t start = out.size() - length - distance;
            for (size_t i = 0; i < length; ++i) {
                out[base + i] = out[start + i];
            }
        }
    }

    return true;
}

} // namespace vrutti::core::utils
