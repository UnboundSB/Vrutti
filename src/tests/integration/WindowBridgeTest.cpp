// -----------------------------------------------------------------------------
// The bridge of the window: the calls of the page and their arguments.
//
// The page calls the core through the bindings of the window. Every binding
// reads its arguments out of the text that the page sends. The text is a
// list of the things that the page gives.
//
// The test checks three things:
//
//   * the arguments of a call come out of the text, whatever their number,
//   * the things that the bindings of the window use are read with the
//     reader of the arguments, so a call of one thing is not read as a
//     call of two things,
//   * every binding of the window that reads arguments does it with that
//     reader, and not with the reader of a message of the bridge.
//
// The third one is a rule about the source of the window. It is here
// because the two readers look alike and a binding that asks for a message
// of the bridge asks for two things: a call that gives one thing is then
// read as nothing at all, and the feature of the page quietly stops
// working. That is exactly what happened once.
// -----------------------------------------------------------------------------

#include "../../core/ipc/BridgeProtocol.h"
#include "../../core/utils/Json.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using vrutti::core::ipc::bridgeBoolAt;
using vrutti::core::ipc::bridgeStringAt;
using vrutti::core::ipc::parseBridgeArguments;
using vrutti::core::ipc::parseBridgeMessage;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;

#ifndef VRUTTI_SOURCE_DIR
#define VRUTTI_SOURCE_DIR "."
#endif

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (condition) {
        std::cout << "  [PASS] " << what << "\n";
    } else {
        std::cerr << "  [FAIL] " << what << "\n";
        ++g_failures;
    }
}

// How many times a piece of text is in another piece of text.
size_t countOf(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return 0;
    size_t count = 0;
    size_t at = haystack.find(needle);
    while (at != std::string::npos) {
        ++count;
        at = haystack.find(needle, at + needle.size());
    }
    return count;
}

// Where a piece of text is in another piece of text.
size_t atOf(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle);
}

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

} // namespace

int main() {
    std::cout << "[WindowBridgeTest] The calls of the page and their arguments.\n\n";

    // --- the arguments of a call come out ------------------------------------
    std::cout << "-- the arguments of a call come out\n";
    {
        // The explorer gives one thing: the name of a folder.
        auto explorer = parseBridgeArguments("[\"C:/work/repo\"]");
        check(explorer != nullptr && explorer->type == JsonNode::Type::Array,
              "a call of one thing is a list of one thing");
        check(explorer && explorer->arrayElements.size() == 1,
              "a call of one thing has one thing in it");
        check(bridgeStringAt(explorer, 0) == "C:/work/repo",
              "the name of the folder comes out of the call");

        // The creation of a file gives one thing as well.
        auto create = parseBridgeArguments("[\"C:/work/repo/new.txt\"]");
        check(bridgeStringAt(create, 0) == "C:/work/repo/new.txt",
              "the name of the new file comes out of the call");

        // The renaming gives two things.
        auto rename = parseBridgeArguments("[\"a.txt\",\"b.txt\"]");
        check(bridgeStringAt(rename, 0) == "a.txt" &&
              bridgeStringAt(rename, 1) == "b.txt",
              "the two names of the renaming come out of the call");

        // The source control gives a folder and a command.
        auto git = parseBridgeArguments("[\"C:/work/repo\",\"git status\"]");
        check(bridgeStringAt(git, 0) == "C:/work/repo" &&
              bridgeStringAt(git, 1) == "git status",
              "the folder and the command of the source control come out of the call");

        // The removal of a file gives a name and a flag.
        auto remove = parseBridgeArguments("[\"C:/work/repo/folder\",true]");
        check(bridgeStringAt(remove, 0) == "C:/work/repo/folder",
              "the name of the folder comes out of the call");
        check(bridgeBoolAt(remove, 1, false),
              "the flag of the removal comes out of the call");
        check(!bridgeBoolAt(remove, 1, false) == false,
              "a flag that is not there is the answer that the caller gave");
        check(!bridgeBoolAt(remove, 5, false),
              "a flag that is not there is the answer that the caller gave");

        // A thing that is not a text is an empty text.
        auto wrong = parseBridgeArguments("[42]");
        check(bridgeStringAt(wrong, 0).empty(),
              "a thing that is not a text is an empty text");

        // A call that gives nothing is a list of nothing.
        auto empty = parseBridgeArguments("[]");
        check(empty != nullptr && empty->arrayElements.empty(),
              "a call that gives nothing is a list of nothing");
        check(bridgeStringAt(empty, 0).empty(),
              "a call that gives nothing has no name in it");

        // A text that is not a list is not arguments.
        check(parseBridgeArguments("{\"a\":1}") == nullptr,
              "a text that is not a list is not arguments");
    }

    // --- a message of the bridge is not arguments ------------------------------
    std::cout << "\n-- a message of the bridge is not arguments\n";
    {
        // The message of the bridge is a name and a payload. It is read with
        // its own reader.
        auto message = parseBridgeMessage("[\"theme/set\",\"{\\\"name\\\":\\\"Dark\\\"}\"]");
        check(message != nullptr, "a message of the bridge is read");
        check(parseBridgeMessage("[\"theme/set\"]") == nullptr,
              "a message of the bridge without a payload is not read");
        check(parseBridgeMessage("{\"method\":\"theme/set\"}") == nullptr,
              "a message of the bridge that is not a list is not read");

        // And the arguments are not read with that reader.
        check(parseBridgeMessage("[\"C:/work/repo\"]") == nullptr,
              "a call of one thing is not a message of the bridge");
    }

    // --- the rule about the source of the window ------------------------------
    std::cout << "\n-- the rule about the source of the window\n";
    {
        const std::string source = readFile(std::string(VRUTTI_SOURCE_DIR) +
                                            "/src/ui/compositor/Window.cpp");
        check(!source.empty(), "the source of the window is there");

        // The reader of a message of the bridge belongs to the bridge alone.
        const size_t messages = countOf(source, "parseBridgeMessage(");
        check(messages == 1,
              "the reader of a message of the bridge is used once (got " +
              std::to_string(messages) + ")");

        const size_t bridgeBinding = atOf(source, "w->bind(\"sendIpcMessage\"");
        const size_t messageUse = atOf(source, "parseBridgeMessage(");
        // The binding that comes after the bridge ends it.
        const size_t nextBinding = bridgeBinding == std::string::npos
            ? std::string::npos
            : atOf(source.substr(bridgeBinding + 1), "w->bind(");
        check(bridgeBinding != std::string::npos && messageUse != std::string::npos &&
              nextBinding != std::string::npos && bridgeBinding < messageUse &&
              messageUse < bridgeBinding + nextBinding,
              "the reader of a message of the bridge is used by the bridge itself");

        // Every binding that reads arguments uses the reader of the
        // arguments. A binding that used the other reader would ask for two
        // things, and a call of one thing would be read as nothing.
        const size_t arguments = countOf(source, "parseBridgeArguments(");
        check(arguments >= 20,
              "the bindings read their arguments with the reader of the arguments (got " +
              std::to_string(arguments) + ")");

        // And no binding goes back to the parser of the JSON by itself.
        check(countOf(source, "JsonParser::parse(req)") == 0,
              "no binding reads its arguments with the parser of the JSON");
    }

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[WindowBridgeTest] FAILED (" << g_failures << " of "
                  << g_checks << " checks)\n";
        return 1;
    }
    std::cout << "[WindowBridgeTest] OK: " << g_checks << " checks\n";
    return 0;
}
