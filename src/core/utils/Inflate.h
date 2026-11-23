#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace vrutti::core::utils {

    // -------------------------------------------------------------------------
    // The DEFLATE decoder (RFC 1951) and the CRC-32 of ZIP.
    //
    // Both are implemented here, so that the installation of an extension
    // does not drag a compression library into the release.
    // -------------------------------------------------------------------------

    // The CRC-32 of ZIP (the polynomial of IEEE 802.3, reflected).
    //
    // `seed` is the value of a running CRC: pass 0 for the first block and
    // the result of the previous call for the next one.
    uint32_t crc32(const uint8_t* data, size_t size, uint32_t seed = 0);

    // Decompresses a raw DEFLATE stream (RFC 1951, no zlib header).
    //
    // The output is appended to `out` and never grows past `maxOutput` bytes:
    // a stream that wants to produce more is refused, because that is how a
    // "zip bomb" tries to fill the disk.
    //
    // Returns false when the stream is broken or when it wants to produce
    // more than `maxOutput` bytes. The reason is in `error`.
    bool rawInflate(const uint8_t* data,
                    size_t size,
                    std::string& out,
                    size_t maxOutput,
                    std::string& error);

} // namespace vrutti::core::utils
