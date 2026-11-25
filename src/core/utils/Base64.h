// -----------------------------------------------------------------------------
// Base64 -- the encoding that carries binary payloads over the bridge.
//
// The decoder used to stop at the first character it did not know and hand
// back what it had as if it were the whole text. A caller that wanted a
// picture got half a picture and no word about it. The decoder now says
// that the text is broken, and the callers that take a picture from the
// pipe look at that answer.
//
// The arithmetic is done on unsigned numbers. The old version shifted a
// signed number to the left, which is undefined behaviour once the value
// does not fit in the type any more.
// -----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include <string>

namespace vrutti::core::utils {

    // The alphabet of the encoding.
    inline constexpr const char* kBase64Alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    // Encodes bytes as text.
    std::string base64_encode(const std::string& in);

    // Decodes text into bytes.
    //
    // The text has to be valid: every character has to be part of the
    // alphabet, and the length has to be a multiple of four. The padding at
    // the end is the only `=` that is allowed.
    //
    // Returns false when the text is broken. The text that was read is not
    // returned then: a caller that ignores the answer would work with half
    // of a payload.
    bool base64_decode(const std::string& in, std::string& out);

    // The value of a character of the alphabet, or -1 when the character is
    // not part of it.
    int base64_value(unsigned char c);

} // namespace vrutti::core::utils

#include <cstddef>

namespace vrutti::core::utils {

    inline int base64_value(unsigned char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    inline std::string base64_encode(const std::string& in) {
        std::string out;
        out.reserve(((in.size() + 2) / 3) * 4);

        size_t i = 0;
        while (i + 2 < in.size()) {
            const uint32_t triple =
                (static_cast<uint32_t>(static_cast<unsigned char>(in[i])) << 16) |
                (static_cast<uint32_t>(static_cast<unsigned char>(in[i + 1])) << 8) |
                static_cast<uint32_t>(static_cast<unsigned char>(in[i + 2]));
            out.push_back(kBase64Alphabet[(triple >> 18) & 0x3F]);
            out.push_back(kBase64Alphabet[(triple >> 12) & 0x3F]);
            out.push_back(kBase64Alphabet[(triple >> 6) & 0x3F]);
            out.push_back(kBase64Alphabet[triple & 0x3F]);
            i += 3;
        }

        const size_t left = in.size() - i;
        if (left == 1) {
            const uint32_t triple =
                static_cast<uint32_t>(static_cast<unsigned char>(in[i])) << 16;
            out.push_back(kBase64Alphabet[(triple >> 18) & 0x3F]);
            out.push_back(kBase64Alphabet[(triple >> 12) & 0x3F]);
            out.push_back('=');
            out.push_back('=');
        } else if (left == 2) {
            const uint32_t triple =
                (static_cast<uint32_t>(static_cast<unsigned char>(in[i])) << 16) |
                (static_cast<uint32_t>(static_cast<unsigned char>(in[i + 1])) << 8);
            out.push_back(kBase64Alphabet[(triple >> 18) & 0x3F]);
            out.push_back(kBase64Alphabet[(triple >> 12) & 0x3F]);
            out.push_back(kBase64Alphabet[(triple >> 6) & 0x3F]);
            out.push_back('=');
        }
        return out;
    }

    inline bool base64_decode(const std::string& in, std::string& out) {
        out.clear();
        if (in.empty()) return true;
        if (in.size() % 4 != 0) return false;

        // The padding may only be at the end, and there may be at most two
        // of them.
        size_t padding = 0;
        while (padding < 2 && padding < in.size() && in[in.size() - 1 - padding] == '=') {
            ++padding;
        }
        if (padding > 0) {
            for (size_t i = 0; i + padding < in.size(); ++i) {
                if (in[i] == '=') return false;
            }
        }

        out.reserve((in.size() / 4) * 3);
        for (size_t i = 0; i < in.size(); i += 4) {
            uint32_t block = 0;
            int values[4];
            // The padding belongs to the last group of four characters
            // only. The old code used the number of the padding signs of
            // the whole text for every group, so a text of more than one
            // group was cut down to the size of its last group.
            size_t groupPadding = 0;
            if (in[i + 3] == '=') ++groupPadding;
            if (in[i + 2] == '=') ++groupPadding;

            for (int j = 0; j < 4; ++j) {
                const unsigned char c = static_cast<unsigned char>(in[i + j]);
                if (c == '=') {
                    // The padding stands for empty bits.
                    values[j] = 0;
                    continue;
                }
                const int value = base64_value(c);
                if (value < 0) {
                    out.clear();
                    return false;
                }
                values[j] = value;
            }
            block = (static_cast<uint32_t>(values[0]) << 18) |
                    (static_cast<uint32_t>(values[1]) << 12) |
                    (static_cast<uint32_t>(values[2]) << 6) |
                    static_cast<uint32_t>(values[3]);

            const size_t bytes = 3 - groupPadding;
            if (bytes >= 1) out.push_back(static_cast<char>((block >> 16) & 0xFF));
            if (bytes >= 2) out.push_back(static_cast<char>((block >> 8) & 0xFF));
            if (bytes >= 3) out.push_back(static_cast<char>(block & 0xFF));
        }
        return true;
    }

} // namespace vrutti::core::utils
