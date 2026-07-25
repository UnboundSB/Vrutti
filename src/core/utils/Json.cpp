#include "Json.h"
#include <cctype>

namespace vrutti::core::utils {

    std::shared_ptr<JsonNode> JsonNode::get(std::string_view key) const {
        if (type != Type::Object) return nullptr;
        for (const auto& pair : objectProperties) {
            if (pair.first == key) {
                return pair.second;
            }
        }
        return nullptr;
    }

    void JsonParser::skipWhitespace(const std::string& source, size_t& pos) {
        while (pos < source.length() && std::isspace(source[pos])) {
            pos++;
        }
    }

    std::string_view JsonParser::parseString(const std::string& source, size_t& pos) {
        // Assume pos is at '"'
        pos++; 
        size_t start = pos;
        while (pos < source.length() && source[pos] != '"') {
            if (source[pos] == '\\') pos++; // skip escaped char
            pos++;
        }
        size_t len = pos - start;
        if (pos < source.length()) pos++; // Consume closing quote
        return std::string_view(source.data() + start, len);
    }

    std::shared_ptr<JsonNode> JsonParser::parseValue(const std::string& source, size_t& pos) {
        skipWhitespace(source, pos);
        if (pos >= source.length()) return nullptr;

        char c = source[pos];

        if (c == '{') {
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);
            pos++; // consume '{'
            skipWhitespace(source, pos);
            while (pos < source.length() && source[pos] != '}') {
                std::string_view key = parseString(source, pos);
                skipWhitespace(source, pos);
                if (pos < source.length() && source[pos] == ':') {
                    pos++; // consume ':'
                    auto val = parseValue(source, pos);
                    if (val) {
                        node->objectProperties.emplace_back(key, val);
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
            pos++;
            skipWhitespace(source, pos);
            while (pos < source.length() && source[pos] != ']') {
                auto val = parseValue(source, pos);
                if (val) {
                    node->arrayElements.push_back(val);
                }
                skipWhitespace(source, pos);
                if (pos < source.length() && source[pos] == ',') {
                    pos++;
                    skipWhitespace(source, pos);
                }
            }
            if (pos < source.length()) pos++;
            return node;
        }
        else if (c == '"') {
            auto node = std::make_shared<JsonNode>(JsonNode::Type::String);
            node->stringValue = parseString(source, pos);
            return node;
        }
        else if (c == 't' || c == 'f') { // true or false
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Boolean);
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
            if (source.compare(pos, 4, "null") == 0) pos += 4;
            return node;
        }
        else if (c == '-' || std::isdigit(c)) {
            size_t start = pos;
            while (pos < source.length() && (std::isdigit(source[pos]) || source[pos] == '.' || source[pos] == '-' || source[pos] == 'e' || source[pos] == 'E' || source[pos] == '+')) {
                pos++;
            }
            auto node = std::make_shared<JsonNode>(JsonNode::Type::Number);
            try {
                node->numberValue = std::stod(source.substr(start, pos - start));
            } catch(...) {} // Invalid number fallback to 0.0
            return node;
        }

        return nullptr;
    }

    std::shared_ptr<JsonNode> JsonParser::parse(const std::string& source) {
        size_t pos = 0;
        return parseValue(source, pos);
    }

    std::string JsonParser::unescapeString(std::string_view sv) {
        std::string out;
        out.reserve(sv.length());
        for (size_t i = 0; i < sv.length(); i++) {
            if (sv[i] == '\\' && i + 1 < sv.length()) {
                if (sv[i+1] == '"') out += '"';
                else if (sv[i+1] == '\\') out += '\\';
                else if (sv[i+1] == 'n') out += '\n';
                else if (sv[i+1] == 'r') out += '\r';
                else if (sv[i+1] == 't') out += '\t';
                else out += sv[i+1];
                i++;
            } else {
                out += sv[i];
            }
        }
        return out;
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
            else if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            }
            else out += c;
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
