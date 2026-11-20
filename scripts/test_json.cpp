// Round-trip tests for the JSON layer.
//
// These cover the two failure modes that broke the editor:
//   * a settings file that contains a raw Windows path ("D:\projects\app")
//   * a temporary source string being parsed (the tree used to dangle)
//
// Build & run:
//     g++ -std=c++20 -I src scripts/test_json.cpp src/core/utils/Json.cpp -o test_json && test_json

#include "../src/core/utils/Json.h"

#include <cstdio>
#include <iostream>
#include <string>

using namespace vrutti::core::utils;

static int g_failures = 0;

static void check(bool condition, const std::string& what) {
    if (condition) {
        std::cout << "  [PASS] " << what << "\n";
    } else {
        std::cout << "  [FAIL] " << what << "\n";
        g_failures++;
    }
}

static std::string strOf(const std::shared_ptr<JsonNode>& node) {
    return node ? std::string(node->stringValue) : std::string("<null>");
}

int main() {
    std::cout << "JSON round-trip tests\n";

    // --- A raw (invalid but common) Windows path must survive parsing. ---
    {
        auto root = JsonParser::parse("{\"path\":\"D:\\projects\\DermaScanApp\"}");
        auto path = root->get("path");
        check(strOf(path) == "D:\\projects\\DermaScanApp",
              "raw Windows path keeps its backslashes");
    }

    // --- A properly escaped path must decode to the same value. ---
    {
        auto root = JsonParser::parse("{\"path\":\"D:\\\\projects\\\\DermaScanApp\"}");
        auto path = root->get("path");
        check(strOf(path) == "D:\\projects\\DermaScanApp",
              "escaped Windows path decodes to the same value");
    }

    // --- Both spellings must serialize to the same, valid JSON. ---
    {
        auto raw = JsonParser::parse("{\"path\":\"D:\\projects\\app\"}");
        auto escaped = JsonParser::parse("{\"path\":\"D:\\\\projects\\\\app\"}");
        check(JsonSerializer::stringify(raw, 0, false) ==
                  JsonSerializer::stringify(escaped, 0, false),
              "raw and escaped paths serialize identically");
    }

    // --- Paths that begin with a known escape letter must not be mangled. ---
    {
        auto root = JsonParser::parse("{\"p\":\"C:\\\\new\\\\test\\\\build\\\\bin\"}");
        check(strOf(root->get("p")) == "C:\\new\\test\\build\\bin",
              "escape-letter path segments are preserved");
    }

    // --- Parsing a temporary must not dangle. ---
    {
        auto root = JsonParser::parse(std::string("{\"a\":\"b\"}"));
        std::string value = strOf(root->get("a"));
        check(value == "b", "tree parsed from a temporary stays valid");
    }

    // --- Standard escapes are decoded. ---
    {
        auto root = JsonParser::parse("{\"s\":\"line1\\nline2\\ttab \\\"q\\\" \\\\ \u0041\"}");
        check(strOf(root->get("s")) == "line1\nline2\ttab \"q\" \\ A",
              "standard escape sequences are decoded");
    }

    // --- Unknown escapes keep their backslash. ---
    {
        auto root = JsonParser::parse("{\"s\":\"a\\qb\"}");
        check(strOf(root->get("s")) == "a\\qb", "unknown escape keeps the backslash");
    }

    // --- Unicode escapes become UTF-8. ---
    {
        auto root = JsonParser::parse("{\"s\":\"\\u00e9\"}");
        check(strOf(root->get("s")) == "\xC3\xA9", "\\uXXXX becomes UTF-8");
    }

    // --- Text content is decoded, not kept raw. ---
    {
        auto root = JsonParser::parse("{\"text\":\"first\\nsecond\"}");
        check(strOf(root->get("text")) == "first\nsecond", "text content is decoded");
    }

    // --- Serializing a decoded value re-escapes it. ---
    {
        auto root = JsonParser::parse("{\"text\":\"first\\nsecond\"}");
        auto out = JsonSerializer::stringify(root, 0, false);
        check(out == "{\"text\":\"first\\nsecond\"}", "serialize re-escapes control characters");
    }

    // --- Object keys are found by name. ---
    {
        auto root = JsonParser::parse("{\"workbench.colorTheme\":\"dark\"}");
        check(root->get("workbench.colorTheme") != nullptr, "dotted key is found");
        check(root->get("missing") == nullptr, "missing key returns null");
    }

    // --- A key with escapes is decoded and still found. ---
    {
        auto root = JsonParser::parse("{\"a\\u0062c\":\"v\"}");
        check(root->get("abc") != nullptr, "escaped key is decoded and found");
    }

    // --- Mutating a value must not invalidate the key. ---
    {
        auto root = JsonParser::parse("{\"a\\u0062c\":\"v\"}");
        root->get("abc")->stringValue = std::string_view("w");
        check(strOf(root->get("abc")) == "w", "mutating a value keeps the key valid");
    }

    // --- Numbers, booleans and null. ---
    {
        auto root = JsonParser::parse("{\"n\":42,\"b\":true,\"z\":null}");
        check(root->get("n")->numberValue == 42.0, "number is parsed");
        check(root->get("b")->boolValue == true, "boolean is parsed");
        check(root->get("z")->type == JsonNode::Type::Null, "null is parsed");
    }

    // --- Nested structures. ---
    {
        auto root = JsonParser::parse("{\"a\":{\"b\":[\"x\",\"y\"]}}");
        auto inner = root->get("a")->get("b");
        check(inner && inner->arrayElements.size() == 2, "nested array is parsed");
        check(strOf(inner->arrayElements[1]) == "y", "nested array element is decoded");
    }

    std::cout << (g_failures == 0 ? "\nAll tests passed\n" : "\nFAILURES\n");
    return g_failures == 0 ? 0 : 1;
}
