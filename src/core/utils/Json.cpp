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

} // namespace vrutti::core::utils
