#include "Json.h"
#include <cctype>
#include <cstdint>

namespace vrutti::core::utils {

    std::shared_ptr<JsonNode> JsonNode::get(std::string_view key) const {
        if (type != Type::Object) return nullptr;
        for (const auto& pair : objectProperties) {
            if (pair.first.compare(key) == 0) {
                return pair.second;
            }
        }
        return nullptr;
    }

    void JsonParser::skipWhitespace(const std::string& source, size_t& pos) {
        while (pos < source.length() && std::isspace(static_cast<unsigned char>(source[pos]))) {
            pos++;
        }
    }

    std::string_view JsonParser::parseString(const std::string& source, size_t& pos,
                                             std::shared_ptr<const std::string>& owner) {
        owner.reset();

        // Assume pos is at '"'
        pos++;
        size_t start = pos;
        bool hasEscape = false;
        while (pos < source.length() && source[pos] != '"') {
            if (source[pos] == '\\') {
                hasEscape = true;
                pos++; // skip escaped char
                if (pos >= source.length()) break;
            }
            pos++;
        }
        size_t len = pos - start;
        if (pos < source.length()) pos++; // Consume closing quote

        std::string_view raw(source.data() + start, len);
        if (!hasEscape) {
            // No escape sequences: the raw text *is* the value and can stay a
            // view into the source buffer.
            return raw;
        }

        // The value contains escape sequences, so it has to be decoded. The
        // decoded text is owned by `owner` so that it outlives the view.
        auto decoded = std::make_shared<const std::string>(unescapeString(raw));
        owner = decoded;
        return std::string_view(decoded->data(), decoded->size());
    }

    std::shared_ptr<JsonNode> JsonParser::parseValue(const std::string& source, size_t& pos,
                                                      const std::shared_ptr<const std::string>& sourceOwner) {
        skipWhitespace(source, pos);
        if (pos >= source.length()) return nullptr;

        char c = source[pos];

        if (c == '{') {
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);
            node->sourceOwner = sourceOwner;
            pos++; // consume '{'
            skipWhitespace(source, pos);
            while (pos < source.length() && source[pos] != '}') {
                std::shared_ptr<const std::string> keyOwner;
                std::string_view key = parseString(source, pos, keyOwner);
                skipWhitespace(source, pos);
                if (pos < source.length() && source[pos] == ':') {
                    pos++; // consume ':'
                    auto val = parseValue(source, pos, sourceOwner);
                    if (val) {
                        // The key is copied by value: it must stay valid even
                        // when the value node is replaced at runtime.
                        node->objectProperties.emplace_back(std::string(key), val);
                    }
                }
                skipWhitespace(source, pos);
                if (pos < source.length() && source[pos] == ',') {
                    pos++; // consume ','
                    skipWhitespace(source, pos);
                }
            }
            if (pos < source.length()) pos++; // consume '}'
            return node;
        }
        else if (c == '[') {
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Array);
            node->sourceOwner = sourceOwner;
            pos++;
            skipWhitespace(source, pos);
            while (pos < source.length() && source[pos] != ']') {
                auto val = parseValue(source, pos, sourceOwner);
                if (val) {
                    node->arrayElements.push_back(val);
                }
                skipWhitespace(source, pos);
                if (pos < source.length() && source[pos] == ',') {
                    pos++; // consume ','
                    skipWhitespace(source, pos);
                }
            }
            if (pos < source.length()) pos++;
            return node;
        }
        else if (c == '"') {
            auto node = std::make_shared<JsonNode>(JsonNode::Type::String);
            node->sourceOwner = sourceOwner;
            std::shared_ptr<const std::string> owner;
            node->stringValue = parseString(source, pos, owner);
            node->stringOwner = owner;
            return node;
        }
        else if (c == 't' || c == 'f') { // true or false
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
            node->sourceOwner = sourceOwner;
            if (source.compare(pos, 4, "true") == 0) {
                node->boolValue = true;
                pos += 4;
            } else if (source.compare(pos, 5, "false") == 0) {
                node->boolValue = false;
                pos += 5;
            }
            return node;
        }
        else if (c == 'n') { // null
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Null);
            node->sourceOwner = sourceOwner;
            if (source.compare(pos, 4, "null") == 0) pos += 4;
            return node;
        }
        else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = pos;
            while (pos < source.length() && (std::isdigit(static_cast<unsigned char>(source[pos])) || source[pos] == '.' || source[pos] == '-' || source[pos] == 'e' || source[pos] == 'E' || source[pos] == '+')) {
                pos++;
            }
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Number);
            node->sourceOwner = sourceOwner;
            try {
                node->numberValue = std::stod(source.substr(start, pos - start));
            } catch(...) {} // Invalid number fallback to 0.0
            return node;
        }

        return nullptr;
    }

    std::shared_ptr<JsonNode> JsonParser::parse(const std::string& source) {
        // The tree refers to the source text through `string_view`s. Keep an
        // owned copy on every node so that the text stays alive for as long as
        // any part of the tree is used -- parsing a temporary used to leave
        // every string in the tree dangling, which crashed the core as soon
        // as the tree was read.
        auto owner = std::make_shared<const std::string>(source);
        size_t pos = 0;
        return parseValue(*owner, pos, owner);
    }

    std::string JsonParser::unescapeString(std::string_view sv) {
        std::string out;
        out.reserve(sv.length());

        // Appends a Unicode code point as UTF-8.
        auto appendUtf8 = [&out](uint32_t cp) {
            if (cp <= 0x7F) {
                out += static_cast<char>(cp);
            } else if (cp <= 0x7FF) {
                out += static_cast<char>(0xC0 | ((cp >> 6) & 0x1F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp <= 0xFFFF) {
                out += static_cast<char>(0xE0 | ((cp >> 12) & 0x0F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | ((cp >> 18) & 0x07));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        };

        // Reads the four hex digits of a \uXXXX escape starting at `at`.
        auto readHex4 = [&sv](size_t at, uint32_t& value) -> bool {
            value = 0;
            for (size_t j = 0; j < 4; j++) {
                char c = sv[at + j];
                value *= 16;
                if (c >= '0' && c <= '9') value += static_cast<uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f') value += static_cast<uint32_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') value += static_cast<uint32_t>(c - 'A' + 10);
                else return false;
            }
            return true;
        };

        for (size_t i = 0; i < sv.length(); i++) {
            if (sv[i] != '\\' || i + 1 >= sv.length()) {
                out += sv[i];
                continue;
            }

            switch (sv[i + 1]) {
                case '"':  out += '"';  i += 1; break;
                case '\\': out += '\\'; i += 1; break;
                case '/':  out += '/';  i += 1; break;
                case 'b':  out += '\b'; i += 1; break;
                case 'f':  out += '\f'; i += 1; break;
                case 'n':  out += '\n'; i += 1; break;
                case 'r':  out += '\r'; i += 1; break;
                case 't':  out += '\t'; i += 1; break;
                case 'u': {
                    uint32_t cp = 0;
                    if (i + 5 < sv.length() && readHex4(i + 2, cp)) {
                        i += 5; // now points at the last hex digit
                        // Combine a surrogate pair into a single code point.
                        if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < sv.length() &&
                            sv[i + 1] == '\\' && sv[i + 2] == 'u') {
                            uint32_t low = 0;
                            if (readHex4(i + 3, low) && low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                i += 6; // now points at the last low hex digit
                            }
                        }
                        if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD; // lone surrogate
                        appendUtf8(cp);
                    } else {
                        // Invalid escape: keep it verbatim.
                        out += '\\';
                        out += 'u';
                        i += 1;
                    }
                    break;
                }
                default:
                    // Unknown escape sequence: keep the backslash. Strict JSON
                    // would reject this, but legacy settings files contain raw
                    // Windows paths such as "D:\projects\app" and dropping the
                    // backslash would corrupt them into "D:projectsapp".
                    out += '\\';
                    out += sv[i + 1];
                    i += 1;
                    break;
            }
        }
        return out;
    }

    std::shared_ptr<JsonNode> makeJsonString(std::string value) {
        auto owner = std::make_shared<const std::string>(std::move(value));
        auto node = std::make_shared<JsonNode>(JsonNode::Type::String);
        node->stringValue = std::string_view(owner->data(), owner->size());
        node->stringOwner = owner;
        return node;
    }

    std::string JsonSerializer::stringify(const std::shared_ptr<JsonNode>& node, int indentLevel, bool pretty) {
        std::string out;
        serializeNode(node, out, indentLevel, pretty);
        return out;
    }

    std::string JsonSerializer::escapeString(std::string_view str) {
        std::string out = "\"";
        for (unsigned char c : str) {
            if (c == '"') out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else if (c == '\b') out += "\\b";
            else if (c == '\f') out += "\\f";
            else if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else out += static_cast<char>(c);
        }
        out += "\"";
        return out;
    }

    void JsonSerializer::serializeNode(const std::shared_ptr<JsonNode>& node, std::string& out, int indentLevel, bool pretty) {
        if (!node) {
            out += "null";
            return;
        }

        auto getIndent = [](int level) { return std::string(level * 2, ' '); };

        switch (node->type) {
            case JsonNode::Type::Null:
                out += "null";
                break;
            case JsonNode::Type::String:
                out += escapeString(node->stringValue);
                break;
            case JsonNode::Type::Number: {
                auto str = std::to_string(node->numberValue);
                str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                if (str.back() == '.') str.pop_back();
                out += str;
                break;
            }
            case JsonNode::Type::Boolean:
                out += node->boolValue ? "true" : "false";
                break;
            case JsonNode::Type::Array: {
                out += "[";
                if (pretty && !node->arrayElements.empty()) out += "\n";
                for (size_t i = 0; i < node->arrayElements.size(); ++i) {
                    if (pretty) out += getIndent(indentLevel + 1);
                    serializeNode(node->arrayElements[i], out, indentLevel + 1, pretty);
                    if (i < node->arrayElements.size() - 1) out += ",";
                    if (pretty) out += "\n";
                }
                if (pretty && !node->arrayElements.empty()) out += getIndent(indentLevel);
                out += "]";
                break;
            }
            case JsonNode::Type::Object: {
                out += "{";
                if (pretty && !node->objectProperties.empty()) out += "\n";
                for (size_t i = 0; i < node->objectProperties.size(); ++i) {
                    if (pretty) out += getIndent(indentLevel + 1);
                    out += escapeString(node->objectProperties[i].first) + (pretty ? ": " : ":");
                    serializeNode(node->objectProperties[i].second, out, indentLevel + 1, pretty);
                    if (i < node->objectProperties.size() - 1) out += ",";
                    if (pretty) out += "\n";
                }
                if (pretty && !node->objectProperties.empty()) out += getIndent(indentLevel);
                out += "}";
                break;
            }
        }
    }

} // namespace vrutti::core::utils
