// -----------------------------------------------------------------------------
// JsonFuzzTest -- the JSON layer of the core under hostile input.
//
// Every message that crosses the pipe is JSON, and every setting file is
// JSON. The parser is therefore the first thing that a broken peer, a broken
// file or a stranger's extension can reach.
//
// The parser has to survive three things:
//
//   * text that is not JSON at all,
//   * text that is JSON but is cut in half,
//   * text that is JSON and is nested very deeply.
//
// The last one matters: the parser walks the document with recursion, so a
// document that is nested a hundred thousand levels deep would use more stack
// than the thread has. The parser also has to *finish*: a document that it
// cannot read may not turn into a wait that never ends.
//
// The writer matters just as much: a value that cannot be written (a number
// that is not a number) may not take the core down on its way out.
// -----------------------------------------------------------------------------

#include "../../core/utils/Json.h"
#include "HostileRunner.h"

#include <cstdio>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;
using vrutti::core::utils::JsonSerializer;
using vrutti::tests::HostileRunner;

namespace {

int g_failures = 0;

void check(bool condition, const std::string& message) {
    if (condition) {
        std::cout << "  [PASS] " << message << "\n";
    } else {
        std::cerr << "  [FAIL] " << message << "\n";
        ++g_failures;
    }
}

// The text of a node, for the checks.
std::string textOf(const std::shared_ptr<JsonNode>& node) {
    if (!node || node->type != JsonNode::Type::String) return "";
    return std::string(node->stringValue.data(), node->stringValue.length());
}

// Shows a text with the bytes that cannot be printed made visible.
std::string show(const std::string& text) {
    std::string out;
    for (unsigned char c : text) {
        if (c >= 32 && c < 127) {
            out += static_cast<char>(c);
        } else {
            char hex[8];
            std::snprintf(hex, sizeof(hex), "\\x%02x", c);
            out += hex;
        }
    }
    return out;
}

} // namespace

int main() {
    std::cout << "[JsonFuzzTest] The JSON layer under hostile input.\n\n";

    // --- the values that have to be right --------------------------------------
    std::cout << "-- the values that have to be right\n";
    {
        auto node = JsonParser::parse("{\"a\":\"b\",\"n\":12,\"t\":true,\"z\":null,\"l\":[1,2]}");
        check(node && node->type == JsonNode::Type::Object, "an object is read");
        check(node && textOf(node->get("a")) == "b", "a string is read");
        check(node && node->get("n") && node->get("n")->numberValue == 12, "a number is read");
        check(node && node->get("t") && node->get("t")->boolValue, "a boolean is read");
        check(node && node->get("z") && node->get("z")->type == JsonNode::Type::Null,
              "a null is read");
        check(node && node->get("l") && node->get("l")->arrayElements.size() == 2,
              "an array is read");
    }

    std::cout << "\n-- the escapes\n";
    {
        auto node = JsonParser::parse("\"a\\nb\\u0041\\ud83d\\ude00\"");
        const std::string value = textOf(node);
        check(value == "a\nbA\xF0\x9F\x98\x80",
              "the escapes are decoded once (got " + value + ")");
    }
    {
        // A raw windows path in a settings file must survive.
        auto node = JsonParser::parse("\"D:\\projects\\app\"");
        check(textOf(node) == "D:\\projects\\app",
              "an unknown escape keeps its backslash (got " + textOf(node) + ")");
    }
    {
        // A lone surrogate is not a character: it has to become the mark of
        // "unknown" instead of a byte that no text can hold.
        auto node = JsonParser::parse("\"\\ud83d\"");
        check(textOf(node) == "\xEF\xBF\xBD",
              "a lone surrogate becomes the mark of unknown (got " + show(textOf(node)) + ")");
    }

    // --- the documents that must not hang --------------------------------------
    std::cout << "\n-- the documents that must not hang\n";
    {
        const char* documents[] = {
            "[x]",             // a value that is not JSON
            "[,]",             // a comma with nothing around it
            "[}]",             // a brace in an array
            "[1,2,]",          // a trailing comma
            "{\"a\":}",         // an object without a value
            "{\"a\" 1}",       // an object without a colon
            "{x}",             // an object without a quoted key
            "[[[[",            // an array that is cut in half
            "\"unterminated",  // a string that is cut in half
            "{\"a\":1",         // an object that is cut in half
            "[",               // an array that is cut in half
            "{",               // an object that is cut in half
            "",                // nothing at all
            "   ",             // nothing but spaces
        };

        HostileRunner runner;
        for (const char* document : documents) {
            const std::string text(document);
            runner.push([text]() {
                volatile auto node = JsonParser::parse(text);
                (void)node;
            });
        }
        // The wait has to be its own step: the order in which the arguments
        // of a call are read is not fixed, and the count has to be taken
        // after the wait.
        const bool finished = runner.waitIdle(10000);
        const size_t done = runner.done();
        check(finished, "the broken documents do not stop the reader (done " +
              std::to_string(done) + " of " +
              std::to_string(sizeof(documents) / sizeof(documents[0])) + ")");
    }

    // --- the documents that must not crash -------------------------------------
    std::cout << "\n-- the documents that must not crash\n";
    {
        HostileRunner runner;

        std::string deep(100000, '[');
        deep += std::string(100000, ']');
        runner.push([deep]() {
            volatile auto node = JsonParser::parse(deep);
            (void)node;
        });

        std::string deepObjects;
        for (int i = 0; i < 50000; ++i) deepObjects += "{\"a\":";
        deepObjects += "1";
        for (int i = 0; i < 50000; ++i) deepObjects += "}";
        runner.push([deepObjects]() {
            volatile auto node = JsonParser::parse(deepObjects);
            (void)node;
        });

        // A document that is deeper than the limit is refused, not read.
        std::string tooDeep;
        for (int i = 0; i < 200; ++i) tooDeep += "[";
        tooDeep += "1";
        for (int i = 0; i < 200; ++i) tooDeep += "]";
        runner.push([tooDeep]() {
            volatile auto node = JsonParser::parse(tooDeep);
            (void)node;
        });

        check(runner.waitIdle(20000), "the deep documents do not stop the reader");
    }
    {
        // The limit is a limit: a document that is deeper than it is refused.
        std::string tooDeep;
        for (int i = 0; i < JsonParser::kMaxDepth + 10; ++i) tooDeep += "[";
        tooDeep += "1";
        for (int i = 0; i < JsonParser::kMaxDepth + 10; ++i) tooDeep += "]";
        auto node = JsonParser::parse(tooDeep);
        check(!node, "a document that is nested too deeply is refused");
    }

    // --- random text -----------------------------------------------------------
    std::cout << "\n-- random text\n";
    {
        std::mt19937 random(20240917);
        std::uniform_int_distribution<int> byteDistribution(0, 255);
        std::uniform_int_distribution<int> lengthDistribution(0, 200);

        std::vector<std::string> texts;
        texts.reserve(20000);
        for (int i = 0; i < 20000; ++i) {
            const int length = lengthDistribution(random);
            std::string text;
            text.reserve(length);
            for (int j = 0; j < length; ++j) {
                text.push_back(static_cast<char>(byteDistribution(random)));
            }
            texts.push_back(std::move(text));
        }

        HostileRunner runner;
        for (const auto& text : texts) {
            runner.push([text]() {
                volatile auto node = JsonParser::parse(text);
                (void)node;
            });
        }

        const bool finished = runner.waitIdle(60000);
        const size_t done = runner.done();
        check(finished, "twenty thousand random texts were read (done " +
              std::to_string(done) + ")");
        if (!finished) {
            // The text that hangs is the one after the last one that was
            // done: the runner starts them in order.
            const size_t index = runner.started() > 0 ? runner.started() - 1 : 0;
            std::cerr << "  [note] the text that hangs is #" << index << ": ["
                      << show(index < texts.size() ? texts[index] : std::string()) << "]\n";
        }
    }

    // --- text that is cut in half ----------------------------------------------
    std::cout << "\n-- text that is cut in half\n";
    {
        const std::string document =
            "{\"name\":\"vrutti\",\"list\":[1,2,3],\"nested\":{\"a\":\"b\\nc\"},\"flag\":true}";
        std::vector<std::string> halves;
        for (size_t length = 0; length <= document.size(); ++length) {
            halves.push_back(document.substr(0, length));
        }

        HostileRunner runner;
        for (const auto& half : halves) {
            runner.push([half]() {
                volatile auto node = JsonParser::parse(half);
                (void)node;
            });
        }
        const bool finished = runner.waitIdle(30000);
        const size_t done = runner.done();
        check(finished, "every half of a document is read without a wait (done " +
              std::to_string(done) + " of " + std::to_string(halves.size()) + ")");
    }

    // --- the writer ------------------------------------------------------------
    std::cout << "\n-- the writer\n";
    {
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);
        node->objectProperties.emplace_back(
            "text", vrutti::core::utils::makeJsonString("a\"b\\c\nd\te"));
        node->objectProperties.emplace_back(
            "control", vrutti::core::utils::makeJsonString(std::string("\x01\x02", 2)));

        const std::string text = JsonSerializer::stringify(node, 0, false);
        check(text.find("\\\"") != std::string::npos, "a quote is escaped");
        check(text.find("\\\\") != std::string::npos, "a backslash is escaped");
        check(text.find("\\n") != std::string::npos, "a line break is escaped");
        check(text.find("\\u0001") != std::string::npos, "a control character is escaped");

        auto again = JsonParser::parse(text);
        check(again && textOf(again->get("text")) == "a\"b\\c\nd\te",
              "the text survives the round trip");
    }
    {
        // A number that is not a number cannot be written as JSON. Writing it
        // used to read past the end of an empty text: the text of such a
        // number has no digits at the end, and the code that cuts the zeros
        // away left nothing behind.
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);
        auto notANumber = std::make_shared<JsonNode>(JsonNode::Type::Number);
        notANumber->numberValue = std::numeric_limits<double>::quiet_NaN();
        node->objectProperties.emplace_back("value", notANumber);

        std::string text;
        bool threw = false;
        try {
            text = JsonSerializer::stringify(node, 0, false);
        } catch (...) {
            threw = true;
        }
        check(!threw, "a number that is not a number does not take the writer down");
        check(text.find("nan") == std::string::npos &&
              text.find("NaN") == std::string::npos &&
              text.find("inf") == std::string::npos,
              "a number that is not a number is not written as 'nan' (got " + text + ")");
    }
    {
        // A level of indent that is below zero would ask for a text of a
        // length that cannot be: the number of spaces is turned into an
        // unsigned number that is very large.
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);
        node->objectProperties.emplace_back(
            "a", std::make_shared<JsonNode>(JsonNode::Type::Array));

        std::string text;
        bool threw = false;
        try {
            text = JsonSerializer::stringify(node, -5, true);
        } catch (...) {
            threw = true;
        }
        check(!threw, "a level of indent below zero does not take the writer down");
        check(!text.empty(), "a level of indent below zero still writes the document");
    }
    {
        // A number that is too big for a whole number is still a number.
        auto node = std::make_shared<JsonNode>(JsonNode::Type::Object);
        auto big = std::make_shared<JsonNode>(JsonNode::Type::Number);
        big->numberValue = 1e300;
        node->objectProperties.emplace_back("value", big);

        const std::string text = JsonSerializer::stringify(node, 0, false);
        auto again = JsonParser::parse(text);
        check(again && again->get("value") &&
              again->get("value")->numberValue > 1e299,
              "a very big number survives the round trip (got " + text + ")");
    }

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[JsonFuzzTest] FAILED (" << g_failures << " assertion(s))\n";
        return 1;
    }
    std::cout << "[JsonFuzzTest] OK: the JSON layer survives hostile input\n";
    return 0;
}
