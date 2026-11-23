#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>

namespace vrutti::core::utils {

    // A lightweight JSON DOM node.
    //
    // String *values* are stored as `std::string_view` into an owned buffer
    // (`stringOwner` / `sourceOwner`), which keeps parsing allocation-light.
    // Object *keys* are stored by value: they are short, they are looked up
    // by name and they are mutated at runtime (settings updates), so owning
    // them removes a whole class of dangling-view bugs.
    class JsonNode {
    public:
        enum class Type { Null, Object, Array, String, Number, Boolean };

        JsonNode() : type(Type::Null) {}
        explicit JsonNode(Type t) : type(t) {}

        Type type;

        // The *decoded* text of a string value. Escape sequences have already
        // been resolved by the parser, so this is the logical value of the
        // document, not its raw source text.
        std::string_view stringValue;
        double numberValue = 0.0;
        bool boolValue = false;

        // Keeps `stringValue` alive when it points at storage the node does
        // not otherwise own (e.g. a temporary std::string used while building
        // a tree).
        std::shared_ptr<const std::string> stringOwner;

        // Keeps the parsed source text alive. The parser stores the source on
        // every node, so every `string_view` in the tree stays valid even
        // when the caller passed a temporary to `JsonParser::parse`.
        std::shared_ptr<const std::string> sourceOwner;

        // Object / Array storage
        // A vector of pairs preserves ordering and keeps the allocation
        // footprint small for the small objects this app deals with.
        std::vector<std::pair<std::string, std::shared_ptr<JsonNode>>> objectProperties;
        std::vector<std::shared_ptr<JsonNode>> arrayElements;

        // Looks up a property of an object node.
        std::shared_ptr<JsonNode> get(std::string_view key) const;
    };

    // Creates a JSON string node that owns its text, so it is safe to build
    // trees out of temporary std::strings.
    std::shared_ptr<JsonNode> makeJsonString(std::string value);

    // Parses a raw JSON string into a DOM tree.
    //
    // The tree keeps its own copy of the source text, so the caller may pass
    // a temporary: every `string_view` in the tree stays valid for as long as
    // the returned root node is alive.
    //
    // String values in the resulting tree are *decoded* -- escape sequences
    // are resolved -- so `stringValue` always holds the logical value. Never
    // unescape it a second time.
    class JsonParser {
    public:
        // How deep a document may be nested.
        //
        // The parser walks the document with recursion, so a document that is
        // nested deeper than this would use more stack than the thread has.
        // A document like that is either broken or hostile, and neither is a
        // reason to take the core down with it.
        static constexpr int kMaxDepth = 128;

        static std::shared_ptr<JsonNode> parse(const std::string& source);

        // Resolves JSON escape sequences in raw (escaped) text.
        //
        // Unknown escape sequences keep their backslash instead of being
        // dropped: legacy settings files contain raw Windows paths such as
        // "D:\projects\app", and silently removing the backslash would
        // corrupt them into "D:projectsapp".
        static std::string unescapeString(std::string_view sv);

    private:
        static std::shared_ptr<JsonNode> parseValue(const std::string& source, size_t& pos,
                                                    const std::shared_ptr<const std::string>& sourceOwner,
                                                    int depth, bool& aborted);
        static std::string_view parseString(const std::string& source, size_t& pos,
                                            std::shared_ptr<const std::string>& owner);
        static void skipWhitespace(const std::string& source, size_t& pos);
    };

    // Serializes a JsonNode tree back into a standard string.
    class JsonSerializer {
    public:
        static std::string stringify(const std::shared_ptr<JsonNode>& node, int indentLevel = 0, bool pretty = true);
        static std::string escapeString(std::string_view str);

        // Writes a number the way JSON wants it.
        //
        // A number that is not a number has no JSON form: it is written as
        // null. A whole number keeps its shape, and a number that is too
        // big for a whole number is written in the short form ("1e+300").
        static std::string formatNumber(double value);

    private:
        static void serializeNode(const std::shared_ptr<JsonNode>& node, std::string& out,
                                  int indentLevel, bool pretty, int depth);
    };

} // namespace vrutti::core::utils
