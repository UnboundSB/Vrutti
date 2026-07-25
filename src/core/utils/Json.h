#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>

namespace vrutti::core::utils {

    // A lightweight, zero-copy JSON Node. 
    // String values and keys are stored as `std::string_view` pointing directly 
    // into the original source buffer, radically minimizing RAM usage.
    class JsonNode {
    public:
        enum class Type { Null, Object, Array, String, Number, Boolean };

        JsonNode() : type(Type::Null) {}
        explicit JsonNode(Type t) : type(t) {}

        Type type;
        
        // Payload (using string_view for absolute zero-copy references)
        std::string_view stringValue;
        double numberValue = 0.0;
        bool boolValue = false;
        
        // Object / Array storage
        // Note: For objects, we use a simple vector of pairs to preserve ordering 
        // and keep allocation footprint smaller than unordered_map for small configs.
        std::vector<std::pair<std::string_view, std::shared_ptr<JsonNode>>> objectProperties;
        std::vector<std::shared_ptr<JsonNode>> arrayElements;

        // Helpers
        std::shared_ptr<JsonNode> get(std::string_view key) const;
    };

    // Parses a raw JSON string into a zero-copy DOM tree.
    // The `source` string must outlive the returned JsonNode tree, because 
    // the tree holds `std::string_view` references directly into `source`.
    class JsonParser {
    public:
        static std::shared_ptr<JsonNode> parse(const std::string& source);

    private:
        static std::shared_ptr<JsonNode> parseValue(const std::string& source, size_t& pos);
        static std::string_view parseString(const std::string& source, size_t& pos);
        static void skipWhitespace(const std::string& source, size_t& pos);
    };

    // Serializes a JsonNode tree back into a standard string.
    class JsonSerializer {
    public:
        static std::string stringify(const std::shared_ptr<JsonNode>& node, int indentLevel = 0, bool pretty = true);
        static std::string escapeString(std::string_view str);
    
    private:
        static void serializeNode(const std::shared_ptr<JsonNode>& node, std::string& out, int indentLevel, bool pretty);
    };

} // namespace vrutti::core::utils
