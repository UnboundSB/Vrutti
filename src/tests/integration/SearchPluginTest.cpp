// -----------------------------------------------------------------------------
// A small harness that drives the search plugin the way the window does.
//
// The search plugin is the part of the editor that reads the files of the
// user. The harness checks that it still finds things, that it does not run
// a shell, and that a folder whose name holds a quote cannot become a
// command of its own.
// -----------------------------------------------------------------------------

#include "../../core/plugins/PluginLoader.h"
#include "../../core/plugins/IPlugin.h"
#include "../../core/utils/Json.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

using vrutti::core::plugins::IPlugin;
using vrutti::core::plugins::PluginLoader;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;

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

std::string jsonOf(const std::string& text) {
    // The payload of the plugin is a JSON object.
    return text;
}

} // namespace

int main() {
    std::cout << "[SearchPluginHarness] The search plugin.\n\n";

    const fs::path root = fs::temp_directory_path() / "vrutti_search_harness";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "sub", ec);

    {
        std::ofstream out(root / "sub" / "note.txt", std::ios::binary);
        out << "the quick brown fox\njumps over the lazy dog\n";
    }
    {
        std::ofstream out(root / "other.txt", std::ios::binary);
        out << "nothing to see here\n";
    }

    // A file whose name holds the characters that a shell gives a meaning
    // to. The old code put the name of the folder into a command for a
    // shell, so the name could run a command of its own.
    const fs::path tricky = root / "tricky & calc & .txt";
    {
        std::ofstream out(tricky, std::ios::binary);
        out << "a file whose name is a trap\n";
    }

    PluginLoader loader;
    IPlugin* plugin = loader.loadPlugin("vrutti_search.dll");
    check(plugin != nullptr, "the search plugin is loaded");
    if (!plugin) return 1;
    plugin->initialize();

    // --- the files of a folder are found -----------------------------------
    {
        const std::string payload = std::string("{\"command\":\"find_files\",\"directory\":\"") +
                                    root.generic_string() + "\"}";
        const std::string answer = plugin->executeCommand("search", payload);
        auto node = JsonParser::parse(answer);
        check(node && node->type == JsonNode::Type::Array, "the list of the files is an array");
        int found = 0;
        bool foundNote = false;
        if (node && node->type == JsonNode::Type::Array) {
            for (const auto& item : node->arrayElements) {
                if (!item || item->type != JsonNode::Type::String) continue;
                ++found;
                if (item->stringValue.find("note.txt") != std::string::npos) foundNote = true;
            }
        }
        check(found >= 3, "the files of the folder are found (got " + std::to_string(found) + ")");
        check(foundNote, "the file in the folder below is found");
    }

    // --- a word is found ----------------------------------------------------
    {
        const std::string payload = std::string("{\"query\":\"fox\",\"directory\":\"") +
                                    root.generic_string() + "\"}";
        const std::string answer = plugin->executeCommand("search", payload);
        auto node = JsonParser::parse(answer);
        bool found = false;
        if (node && node->type == JsonNode::Type::Object) {
            auto words = node->get("words");
            if (words && words->type == JsonNode::Type::Array) {
                found = !words->arrayElements.empty();
            }
        }
        check(found, "the word is found in the files of the user");
    }

    // --- a folder whose name is a trap --------------------------------------
    {
        const fs::path trapFolder = root / "trap & echo pwned & .dir";
        fs::create_directories(trapFolder, ec);
        {
            std::ofstream out(trapFolder / "inside.txt", std::ios::binary);
            out << "a fox is here\n";
        }

        const std::string payload = std::string("{\"query\":\"fox\",\"directory\":\"") +
                                    trapFolder.generic_string() + "\"}";
        const std::string answer = plugin->executeCommand("search", payload);
        auto node = JsonParser::parse(answer);
        check(node != nullptr, "the search of a folder whose name is a trap answers");

        // The search still has to find the file inside of the folder. A
        // name that was put into a command for a shell would break the
        // command in two, and the search would look in the wrong place.
        bool foundInside = false;
        if (node && node->type == JsonNode::Type::Object) {
            auto words = node->get("words");
            if (words && words->type == JsonNode::Type::Array) {
                for (const auto& item : words->arrayElements) {
                    if (!item || item->type != JsonNode::Type::Object) continue;
                    auto fileNode = item->get("file");
                    if (fileNode && fileNode->type == JsonNode::Type::String &&
                        fileNode->stringValue.find("inside.txt") != std::string::npos) {
                        foundInside = true;
                    }
                }
            }
        }
        check(foundInside, "the search of a folder whose name is a trap still finds the file inside it");
    }

    // --- a word is changed ----------------------------------------------------
    {
        const fs::path target = root / "replace.txt";
        {
            std::ofstream out(target, std::ios::binary);
            out << "one fox\ntwo fox\n";
        }

        const std::string payload = std::string("{\"query\":\"fox\",\"directory\":\"") +
                                    root.generic_string() +
                                    "\",\"isReplace\":true,\"replaceString\":\"cat\"}";
        plugin->executeCommand("search", payload);

        std::ifstream in(target, std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        check(content.find("one cat") != std::string::npos,
              "the word was changed (got " + content + ")");
        check(content.find("two cat") != std::string::npos,
              "the word was changed on every line");
    }

    // --- a pattern that is not a pattern ---------------------------------------
    {
        const fs::path target = root / "broken.txt";
        {
            std::ofstream out(target, std::ios::binary);
            out << "nothing is changed here\n";
        }

        const std::string payload = std::string("{\"query\":\"(unclosed\",\"directory\":\"") +
                                    root.generic_string() +
                                    "\",\"isReplace\":true,\"replaceString\":\"x\",\"useRegex\":true}";
        // The answer has to come back: a pattern that the engine refuses
        // may not take the editor with it.
        const std::string answer = plugin->executeCommand("search", payload);
        check(!answer.empty(), "a pattern that is not a pattern does not take the plugin down");

        std::ifstream in(target, std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        check(content == "nothing is changed here\n",
              "the file was left alone when the pattern was refused (got " + content + ")");
    }

    plugin->shutdown();
    fs::remove_all(root, ec);

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[SearchPluginHarness] FAILED (" << g_failures << " of "
                  << g_checks << " checks)\n";
        return 1;
    }
    std::cout << "[SearchPluginHarness] OK: " << g_checks << " checks\n";
    return 0;
}
