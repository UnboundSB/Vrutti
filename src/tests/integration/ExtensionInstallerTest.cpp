// -----------------------------------------------------------------------------
// ExtensionInstallerTest -- the installation of an extension, from the archive
// to the folder of the extension.
//
// The installer is the door through which code of a stranger comes into the
// app, so the tests here are mostly about the ways a hostile archive can try
// to get in:
//
//   * a file that writes outside of the folder of the extension (a "zip slip"),
//   * a file that expands to gigabytes (a "zip bomb"),
//   * an archive whose checksum does not match its content,
//   * a manifest that names the extension with a name that leaves the folder,
//   * a manifest that asks for a version of the host that does not exist.
//
// The tests also check that a refused installation leaves nothing behind: a
// half-written extension is worse than no extension at all.
// -----------------------------------------------------------------------------

#include "../../core/extensions/ExtensionService.h"
#include "../../core/utils/Inflate.h"
#include "../../core/utils/Json.h"
#include "../../core/utils/ZipArchive.h"
#include "TestZip.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

using vrutti::core::extensions::ExtensionDescription;
using vrutti::core::extensions::ExtensionService;
using vrutti::core::utils::JsonNode;
using vrutti::core::utils::JsonParser;
using vrutti::core::utils::ZipArchive;
using vrutti::tests::TestZip;
using vrutti::tests::manifestOf;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& message) {
    ++g_checks;
    if (condition) {
        std::cout << "  [PASS] " << message << "\n";
    } else {
        std::cerr << "  [FAIL] " << message << "\n";
        ++g_failures;
    }
}

std::string readWholeFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

// The bytes of a DEFLATE stream that was made with another tool, so that the
// decoder is checked against something that is not its own encoder.
const char* DEFLATE_HEX =
    "0b2b2a2d29c95448ad2849cd2bcecccf53c8cc2b2e49ccc9492d5228492d2e"
    "512848acccc94f4cd1e30a1b55884f2100";
const uint32_t DEFLATE_CRC = 1985519847u;
const size_t DEFLATE_LENGTH = 328u;

std::string hexToBytes(const std::string& hex) {
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

// The text that was compressed by the reference tool.
std::string expectedPayload() {
    std::string out;
    for (int i = 0; i < 8; ++i) {
        out += "Vrutti extension installer test payload.\n";
    }
    return out;
}

} // namespace

int main() {
    std::cout << "[ExtensionInstallerTest] An extension must be installed "
                 "safely, and a hostile one must not get in.\n\n";

    const fs::path root = fs::temp_directory_path() /
        ("vrutti_installer_test_" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    const fs::path extensionsDir = root / "extensions";
    fs::create_directories(extensionsDir, ec);

    // The service reads the folder of the extensions from the environment,
    // so the test never touches the extensions of the user.
#ifdef _WIN32
    _putenv_s("VRUTTI_EXTENSIONS_DIR", extensionsDir.string().c_str());
    _putenv_s("VRUTTI_BUILTIN_EXTENSIONS_DIR", (root / "builtin").string().c_str());
#else
    setenv("VRUTTI_EXTENSIONS_DIR", extensionsDir.string().c_str(), 1);
    setenv("VRUTTI_BUILTIN_EXTENSIONS_DIR", (root / "builtin").string().c_str(), 1);
#endif

    // =======================================================================
    // 1. The CRC-32 and the DEFLATE decoder
    // =======================================================================
    std::cout << "-- the checksum and the decoder\n";
    {
        const std::string payload = expectedPayload();
        const uint32_t crc = vrutti::core::utils::crc32(
            reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
        check(crc == DEFLATE_CRC,
              "the checksum of a text is the one of the reference tool (got " +
              std::to_string(crc) + ", expected " + std::to_string(DEFLATE_CRC) + ")");

        const std::string blob = hexToBytes(DEFLATE_HEX);
        std::string out;
        std::string error;
        const bool ok = vrutti::core::utils::rawInflate(
            reinterpret_cast<const uint8_t*>(blob.data()), blob.size(), out,
            1024 * 1024, error);
        check(ok, "a stream of another tool is decoded (" + error + ")");
        check(out == payload,
              "the decoded stream is the text that was compressed (got " +
              std::to_string(out.size()) + " bytes, expected " +
              std::to_string(payload.size()) + ")");
    }

    // =======================================================================
    // 2. A stream that is broken
    // =======================================================================
    std::cout << "\n-- a stream that is broken\n";
    {
        std::string out;
        std::string error;
        const uint8_t garbage[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        const bool ok = vrutti::core::utils::rawInflate(garbage, sizeof(garbage),
                                                         out, 1024, error);
        check(!ok, "a stream that makes no sense is refused");
        check(!error.empty(), "the refusal says what is wrong");
    }
    {
        // A stream that wants to produce more than the limit is refused: that
        // is how a zip bomb tries to fill the disk.
        const std::string blob = hexToBytes(DEFLATE_HEX);
        std::string out;
        std::string error;
        const bool ok = vrutti::core::utils::rawInflate(
            reinterpret_cast<const uint8_t*>(blob.data()), blob.size(), out, 16, error);
        check(!ok, "a stream that wants to produce too much is refused");
    }

    // =======================================================================
    // 3. The reader of the archives
    // =======================================================================
    std::cout << "\n-- the reader of the archives\n";
    {
        TestZip zip;
        zip.add("extension/package.json", manifestOf("hello", "vrutti", "1.0.0"));
        zip.add("extension/extension.js", "exports.activate = () => {};\n");
        const fs::path archivePath = root / "hello.vsix";
        check(zip.write(archivePath.string()), "the archive of the test was written");

        ZipArchive archive;
        std::string error;
        check(archive.open(archivePath.string(), error),
              "the archive is opened (" + error + ")");
        check(archive.entries().size() == 2,
              "the archive holds two files (got " +
              std::to_string(archive.entries().size()) + ")");

        std::string content;
        bool read = false;
        for (const auto& entry : archive.entries()) {
            if (entry.name != "extension/package.json") continue;
            read = archive.read(entry, content, error);
        }
        check(read, "the manifest of the archive is read");
        check(content.find("\"name\": \"hello\"") != std::string::npos,
              "the manifest of the archive is the one that was written");
    }

    {
        // A file that is not an archive at all.
        const fs::path notAnArchive = root / "not-an-archive.vsix";
        {
            std::ofstream out(notAnArchive, std::ios::binary);
            out << "this is not an archive, it is a text file";
        }
        ZipArchive archive;
        std::string error;
        check(!archive.open(notAnArchive.string(), error),
              "a file that is not an archive is refused");
        check(!error.empty(), "the refusal says what is wrong");
    }

    {
        // A file whose checksum does not match its content.
        TestZip zip;
        zip.add("extension/package.json", manifestOf("hello", "vrutti", "1.0.0"));
        zip.corruptChecksumOf("extension/package.json");
        const fs::path archivePath = root / "corrupt.vsix";
        zip.write(archivePath.string());

        ZipArchive archive;
        std::string error;
        check(archive.open(archivePath.string(), error), "the archive is opened");
        bool read = true;
        for (const auto& entry : archive.entries()) {
            std::string content;
            read = archive.read(entry, content, error);
        }
        check(!read, "a file whose checksum does not match is refused");
        check(error.find("checksum") != std::string::npos,
              "the refusal says that the checksum does not match (got: " + error + ")");
    }

    // =======================================================================
    // 4. The names that leave the folder
    // =======================================================================
    std::cout << "\n-- the names that leave the folder\n";
    {
        check(!ZipArchive::isSafeEntryName("../evil.txt"),
              "a name with a parent part is refused");
        check(!ZipArchive::isSafeEntryName("extension/../../evil.txt"),
              "a name that climbs out of the extension is refused");
        check(!ZipArchive::isSafeEntryName("/etc/passwd"),
              "an absolute name is refused");
        check(!ZipArchive::isSafeEntryName("C:\\evil.txt"),
              "a name with a drive letter is refused");
        check(!ZipArchive::isSafeEntryName("extension\\evil.txt"),
              "a name with a backslash is refused");
        check(!ZipArchive::isSafeEntryName(""),
              "an empty name is refused");
        check(ZipArchive::isSafeEntryName("extension/extension.js"),
              "an ordinary name is accepted");
        check(ZipArchive::isSafeEntryName("extension/"),
              "the name of a folder is accepted");
    }

    {
        // The archive tries to write a file outside of the folder.
        TestZip zip;
        zip.add("extension/package.json", manifestOf("evil", "vrutti", "1.0.0"));
        zip.addRaw("../escaped.txt", "this file must never be written\n",
                   vrutti::core::utils::crc32(
                       reinterpret_cast<const uint8_t*>("this file must never be written\n"),
                       33));
        const fs::path archivePath = root / "slip.vsix";
        zip.write(archivePath.string());

        ZipArchive archive;
        std::string error;
        check(archive.open(archivePath.string(), error), "the hostile archive is opened");
        const fs::path destination = root / "slip-target";
        // The whole archive is written: the file that tries to escape is
        // refused, and nothing is written outside of the folder.
        const bool extracted = archive.extractAll(destination.string(), error);
        check(!extracted, "the archive that writes outside of the folder is refused");
        check(!fs::exists(root / "escaped.txt"),
              "the file that tried to escape was not written");

        // The same archive, but only the folder of the extension is written:
        // the file that tries to escape is not part of it, so it is skipped.
        const fs::path second = root / "slip-target-2";
        const bool extractedSecond = archive.extractAll(second.string(), "extension/", error);
        check(extractedSecond, "the folder of the extension is written (" + error + ")");
        check(!fs::exists(root / "escaped.txt"),
              "the file that tried to escape was not written here either");
        check(fs::exists(second / "package.json"),
              "the manifest of the extension was written");
    }

    {
        // The archive holds a file that expands to far more than it is.
        TestZip zip;
        const std::string payload(4096, 'a');
        zip.add("extension/package.json", manifestOf("bomb", "vrutti", "1.0.0"));
        zip.add("extension/big.txt", payload);
        const fs::path archivePath = root / "bomb.vsix";
        zip.write(archivePath.string());

        ZipArchive archive;
        std::string error;
        // The limits of the reader are the defence: a small file that claims
        // to be much bigger is refused before anything is written.
        check(archive.open(archivePath.string(), error), "the archive is opened");
        const fs::path destination = root / "bomb-target";
        archive.extractAll(destination.string(), "extension/", error);
        check(fs::exists(destination / "package.json"),
              "the ordinary file of the archive was written");
    }

    // =======================================================================
    // 5. The installation of an extension
    // =======================================================================
    std::cout << "\n-- the installation of an extension\n";
    {
        TestZip zip;
        zip.add("extension/package.json", manifestOf("hello", "vrutti", "1.0.0"));
        zip.add("extension/extension.js", "exports.activate = () => {};\n");
        zip.add("extension/README.md", "# hello\n");
        const fs::path archivePath = root / "hello.vsix";
        zip.write(archivePath.string());

        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        const bool ok = ExtensionService::installVsix(
            archivePath.string(), installed, error, errorCode);
        check(ok, "the extension is installed (" + error + ")");
        check(installed.identifier == "vrutti.hello",
              "the extension is called vrutti.hello (got " + installed.identifier + ")");
        check(installed.version == "1.0.0",
              "the version of the extension is 1.0.0 (got " + installed.version + ")");

        const fs::path folder = extensionsDir / "vrutti.hello-1.0.0";
        check(fs::exists(folder / "package.json"),
              "the manifest is at the root of the folder of the extension");
        check(fs::exists(folder / "extension.js"),
              "the files of the extension are in the folder of the extension");
        check(!fs::exists(folder / "extension"),
              "the folder of the extension is not left inside the folder");

        const auto all = ExtensionService::listAll();
        check(all.size() == 1,
              "the extension is on the list (got " + std::to_string(all.size()) + ")");
        if (all.size() == 1) {
            check(all[0].identifier == "vrutti.hello",
                  "the list holds the extension that was installed");
            check(!all[0].uuid.empty(), "the extension has an identity");
        }

        // The metadata file remembers what is installed.
        const fs::path metadata = ExtensionService::metadataFile();
        check(fs::exists(metadata), "the file that remembers the extensions was written");
        auto parsed = JsonParser::parse(readWholeFile(metadata.string()));
        check(parsed && parsed->type == JsonNode::Type::Object,
              "the file that remembers the extensions is valid JSON");
        if (parsed) {
            auto installedList = parsed->get("installed");
            check(installedList && installedList->type == JsonNode::Type::Array &&
                  installedList->arrayElements.size() == 1,
                  "the file that remembers the extensions holds one extension");
        }
    }

    {
        // The list that the extension host gets has the shape the host
        // expects.
        const auto enabled = ExtensionService::listEnabled();
        auto json = ExtensionService::descriptionsJson(enabled);
        check(json && json->type == JsonNode::Type::Array && json->arrayElements.size() == 1,
              "the list for the host holds the extension");
        if (json && !json->arrayElements.empty()) {
            const auto& node = json->arrayElements[0];
            auto identifier = node->get("identifier");
            check(identifier && identifier->type == JsonNode::Type::Object,
                  "the description has an identifier");
            if (identifier) {
                auto value = identifier->get("value");
                auto lower = identifier->get("_lower");
                check(value && value->type == JsonNode::Type::String &&
                      value->stringValue == "vrutti.hello",
                      "the identifier of the description is vrutti.hello");
                check(lower && lower->type == JsonNode::Type::String &&
                      lower->stringValue == "vrutti.hello",
                      "the lower case of the identifier is there as well");
            }
            check(node->get("uuid") != nullptr, "the description has an identity");
            check(node->get("version") != nullptr, "the description has a version");
            auto engines = node->get("engines");
            check(engines && engines->type == JsonNode::Type::Object &&
                  engines->get("vscode") != nullptr,
                  "the description has the version of the host it asks for");
            auto location = node->get("extensionLocation");
            check(location && location->type == JsonNode::Type::Object &&
                  location->get("scheme") != nullptr &&
                  location->get("path") != nullptr,
                  "the description has the folder of the extension");
        }
    }

    // =======================================================================
    // 6. The installations that have to fail
    // =======================================================================
    std::cout << "\n-- the installations that have to fail\n";
    {
        // A file that is not an archive.
        const fs::path path = root / "garbage.vsix";
        {
            std::ofstream out(path, std::ios::binary);
            out << "not an archive";
        }
        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        const bool ok = ExtensionService::installVsix(path.string(), installed, error, errorCode);
        check(!ok, "a file that is not an archive is not installed");
        check(errorCode == "InvalidExtension",
              "the refusal has the code of a broken extension (got " + errorCode + ")");
    }
    {
        // An archive without a manifest.
        TestZip zip;
        zip.add("extension/extension.js", "exports.activate = () => {};\n");
        const fs::path path = root / "no-manifest.vsix";
        zip.write(path.string());

        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        const bool ok = ExtensionService::installVsix(path.string(), installed, error, errorCode);
        check(!ok, "an archive without a manifest is not installed");
        check(errorCode == "InvalidExtension", "the refusal has the code of a broken extension");
        check(!fs::exists(extensionsDir / "vrutti.unknown"),
              "nothing was written for the refused installation");
    }
    {
        // A manifest whose name would leave the folder.
        TestZip zip;
        zip.add("extension/package.json", manifestOf("../../evil", "vrutti", "1.0.0"));
        const fs::path path = root / "evil-name.vsix";
        zip.write(path.string());

        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        const bool ok = ExtensionService::installVsix(path.string(), installed, error, errorCode);
        check(!ok, "a manifest with a name that leaves the folder is not installed");
        check(errorCode == "InvalidExtension", "the refusal has the code of a broken extension");
    }
    {
        // A manifest that asks for a host that does not exist.
        TestZip zip;
        zip.add("extension/package.json",
                manifestOf("future", "vrutti", "1.0.0", "^99.0.0"));
        zip.add("extension/extension.js", "exports.activate = () => {};\n");
        const fs::path path = root / "future.vsix";
        zip.write(path.string());

        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        const bool ok = ExtensionService::installVsix(path.string(), installed, error, errorCode);
        check(!ok, "an extension that asks for a newer host is not installed");
        check(error.find("99.0.0") != std::string::npos,
              "the refusal says which version is missing (got: " + error + ")");
    }
    {
        // A manifest whose entry point is not in the archive.
        TestZip zip;
        zip.add("extension/package.json",
                manifestOf("noentry", "vrutti", "1.0.0", "^1.90.0", "./missing.js"));
        const fs::path path = root / "no-entry.vsix";
        zip.write(path.string());

        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        const bool ok = ExtensionService::installVsix(path.string(), installed, error, errorCode);
        check(!ok, "an extension without its entry point is not installed");
        check(!fs::exists(extensionsDir / "vrutti.noentry-1.0.0"),
              "the folder of the refused extension was removed");
    }
    {
        // Nothing of the refused installations is left behind.
        const auto all = ExtensionService::listAll();
        check(all.size() == 1,
              "only the extension that was installed well is on the list (got " +
              std::to_string(all.size()) + ")");
        int leftovers = 0;
        for (const auto& entry : fs::directory_iterator(extensionsDir, ec)) {
            if (entry.path().filename().string().rfind(".install-", 0) == 0) ++leftovers;
        }
        check(leftovers == 0,
              "no folder of an installation that failed is left behind (got " +
              std::to_string(leftovers) + ")");
    }

    // =======================================================================
    // 7. Turning an extension off and on
    // =======================================================================
    std::cout << "\n-- turning an extension off and on\n";
    {
        std::string error;
        std::string errorCode;
        check(ExtensionService::setEnabled("vrutti.hello", false, error, errorCode),
              "the extension is turned off");

        const auto all = ExtensionService::listAll();
        const auto enabled = ExtensionService::listEnabled();
        check(all.size() == 1, "the turned off extension is still installed");
        check(enabled.empty(),
              "the turned off extension is not given to the host (got " +
              std::to_string(enabled.size()) + ")");

        check(ExtensionService::setEnabled("vrutti.hello", true, error, errorCode),
              "the extension is turned on");
        check(ExtensionService::listEnabled().size() == 1,
              "the extension is given to the host again");

        check(!ExtensionService::setEnabled("vrutti.nothing", true, error, errorCode),
              "an extension that is not installed cannot be turned on");
        check(errorCode == "FileNotFound", "the refusal has the code of a missing file");
    }

    // =======================================================================
    // 8. A second version of the same extension
    // =======================================================================
    std::cout << "\n-- a second version of the same extension\n";
    {
        TestZip zip;
        zip.add("extension/package.json", manifestOf("hello", "vrutti", "2.0.0"));
        zip.add("extension/extension.js", "exports.activate = () => {};\n");
        const fs::path path = root / "hello-2.vsix";
        zip.write(path.string());

        ExtensionDescription installed;
        std::string error;
        std::string errorCode;
        check(ExtensionService::installVsix(path.string(), installed, error, errorCode),
              "the second version is installed (" + error + ")");
        check(fs::exists(extensionsDir / "vrutti.hello-2.0.0"),
              "the folder of the second version is there");
        check(!fs::exists(extensionsDir / "vrutti.hello-1.0.0"),
              "the folder of the first version is gone: two versions of one extension "
              "would both be given to the host");

        const auto all = ExtensionService::listAll();
        check(all.size() == 1,
              "the list holds one extension, not two (got " +
              std::to_string(all.size()) + ")");
        if (all.size() == 1) {
            check(all[0].version == "2.0.0",
                  "the list holds the new version (got " + all[0].version + ")");
        }
    }

    // =======================================================================
    // 9. Installing the same extension twice
    // =======================================================================
    std::cout << "\n-- installing the same extension twice\n";
    {
        TestZip zip;
        zip.add("extension/package.json", manifestOf("hello", "vrutti", "2.0.0"));
        zip.add("extension/extension.js", "exports.activate = () => {};\n");
        const fs::path path = root / "hello-2-again.vsix";
        zip.write(path.string());

        ExtensionDescription first;
        ExtensionDescription second;
        std::string error;
        std::string errorCode;
        check(ExtensionService::installVsix(path.string(), first, error, errorCode),
              "the extension is installed again");
        check(ExtensionService::installVsix(path.string(), second, error, errorCode),
              "the extension is installed once more");
        check(first.uuid == second.uuid,
              "the identity of the extension does not change when it is installed again");
        check(ExtensionService::listAll().size() == 1,
              "the list still holds one extension");
    }

    // =======================================================================
    // 10. Removing an extension
    // =======================================================================
    std::cout << "\n-- removing an extension\n";
    {
        std::string error;
        std::string errorCode;
        check(ExtensionService::uninstall("vrutti.hello", error, errorCode),
              "the extension is removed (" + error + ")");
        check(!fs::exists(extensionsDir / "vrutti.hello-2.0.0"),
              "the folder of the extension is gone");
        check(ExtensionService::listAll().empty(),
              "the list is empty after the removal");

        check(!ExtensionService::uninstall("vrutti.hello", error, errorCode),
              "removing an extension that is not installed fails");
        check(errorCode == "FileNotFound", "the refusal has the code of a missing file");
    }

    // =======================================================================
    // 11. A metadata file that was edited by hand
    // =======================================================================
    std::cout << "\n-- a metadata file that was edited by hand\n";
    {
        // The file that remembers the extensions is in the profile of the
        // user. When it points outside of the folder of the extensions, the
        // removal must not follow it.
        const fs::path outside = root / "outside";
        fs::create_directories(outside, ec);
        {
            std::ofstream out(outside / "precious.txt", std::ios::binary);
            out << "this file must survive\n";
        }

        {
            std::ofstream out(ExtensionService::metadataFile(), std::ios::binary | std::ios::trunc);
            out << "{\n  \"installed\": [\n"
                << "    {\n"
                << "      \"identifier\": { \"value\": \"vrutti.hostile\", \"_lower\": \"vrutti.hostile\" },\n"
                << "      \"version\": \"1.0.0\",\n"
                << "      \"relativeLocation\": \"../../outside\",\n"
                << "      \"location\": \"" << outside.generic_string() << "\",\n"
                << "      \"enabled\": true\n"
                << "    }\n  ]\n}\n";
        }

        std::string error;
        std::string errorCode;
        check(ExtensionService::uninstall("vrutti.hostile", error, errorCode),
              "the extension of the edited file is removed");
        check(fs::exists(outside / "precious.txt"),
              "the folder outside of the extensions was not touched");
    }

    // =======================================================================
    // 12. The built-in extensions
    // =======================================================================
    std::cout << "\n-- the built-in extensions\n";
    {
        const fs::path builtin = root / "builtin";
        fs::create_directories(builtin / "vrutti.builtin-1.0.0", ec);
        {
            std::ofstream out(builtin / "vrutti.builtin-1.0.0" / "package.json",
                              std::ios::binary | std::ios::trunc);
            out << manifestOf("builtin", "vrutti", "1.0.0");
        }

        const auto all = ExtensionService::listAll();
        check(all.size() == 1, "the built-in extension is on the list");
        if (all.size() == 1) {
            check(all[0].isBuiltin, "the extension is marked as a built-in one");
            check(all[0].identifier == "vrutti.builtin",
                  "the built-in extension is called vrutti.builtin");
        }
    }

    // =======================================================================
    // 13. The list and the removal must agree
    // =======================================================================
    std::cout << "\n-- the list and the removal must agree\n";
    {
        // A folder that was put there by hand, without the metadata file
        // knowing about it. The list shows it, so the removal has to take
        // it away: before, the list showed it and the removal said that it
        // was not installed.
        const fs::path handmade = extensionsDir / "hand.made-1.0.0";
        fs::create_directories(handmade / "extension", ec);
        {
            std::ofstream out(handmade / "extension" / "package.json",
                              std::ios::binary | std::ios::trunc);
            out << manifestOf("made", "hand", "1.0.0");
        }

        int shown = 0;
        for (const auto& description : ExtensionService::listAll()) {
            if (description.identifier == "hand.made") shown++;
        }
        check(shown == 1, "the extension that is on the disk is on the list");

        std::string error;
        std::string errorCode;
        check(ExtensionService::uninstall("hand.made", error, errorCode),
              "the extension that is on the disk is removed (" + error + ")");
        check(!fs::exists(handmade), "the folder of the extension is gone");
    }
    {
        // Two folders that hold the same extension. The list shows it once,
        // and the removal takes both folders away: one of them would stay
        // behind and show up again.
        const fs::path first = extensionsDir / "twice.copy-1.0.0";
        const fs::path second = extensionsDir / "copy-again-1.0.0";
        for (const auto& folder : { first, second }) {
            fs::create_directories(folder / "extension", ec);
            std::ofstream out(folder / "extension" / "package.json",
                              std::ios::binary | std::ios::trunc);
            out << manifestOf("copy", "twice", "1.0.0");
        }

        int shown = 0;
        for (const auto& description : ExtensionService::listAll()) {
            if (description.identifier == "twice.copy") shown++;
        }
        check(shown == 1, "the same extension is shown once");

        std::string error;
        std::string errorCode;
        check(ExtensionService::uninstall("twice.copy", error, errorCode),
              "the extension is removed (" + error + ")");
        check(!fs::exists(first) && !fs::exists(second),
              "every copy of the extension is gone");
    }
    {
        // The switch of an extension that is not in the metadata file.
        const fs::path folder = extensionsDir / "switch.me-1.0.0";
        fs::create_directories(folder / "extension", ec);
        {
            std::ofstream out(folder / "extension" / "package.json",
                              std::ios::binary | std::ios::trunc);
            out << manifestOf("me", "switch", "1.0.0");
        }

        std::string error;
        std::string errorCode;
        check(ExtensionService::setEnabled("switch.me", false, error, errorCode),
              "the extension that is on the disk is turned off (" + error + ")");

        bool given = false;
        for (const auto& description : ExtensionService::listEnabled()) {
            if (description.identifier == "switch.me") given = true;
        }
        check(!given, "the extension that was turned off is not given to the host");

        check(ExtensionService::setEnabled("switch.me", true, error, errorCode),
              "the extension is turned on again");
    }
    {
        // A built-in extension is not removed: it is part of the editor.
        std::string error;
        std::string errorCode;
        check(!ExtensionService::uninstall("vrutti.builtin", error, errorCode),
              "a built-in extension is not removed");
        check(errorCode == "InvalidExtension",
              "the refusal has the code of a bad extension");
        check(fs::exists(root / "builtin" / "vrutti.builtin-1.0.0"),
              "the folder of the built-in extension is still there");
    }

    fs::remove_all(root, ec);

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[ExtensionInstallerTest] FAILED (" << g_failures << " of " +
                  std::to_string(g_checks) + " assertions)\n";
        return 1;
    }
    std::cout << "[ExtensionInstallerTest] OK: " << std::to_string(g_checks) +
              " assertions -- an extension is installed safely\n";
    return 0;
}
