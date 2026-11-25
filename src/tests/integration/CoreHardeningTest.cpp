// -----------------------------------------------------------------------------
// CoreHardeningTest -- the parts of the core that hold data.
//
// The tests of the other files walk the paths that the editor uses every
// day. This file walks the paths that a stranger would use:
//
//   * an entry point of an extension that points outside of the extension,
//   * a version range that the installer reads the wrong way,
//   * a file that is too big to be read,
//   * a rename that takes a whole folder with it,
//   * a text that asks for a range that is not there,
//   * a document whose changes are thrown away,
//   * a plugin that takes the place of another one,
//   * a setting that is written while another thread is reading it,
//   * a text that is not base64.
//
// Every one of these was a way to lose data, to run code that should not
// run, or to make the editor do more than it was asked to.
// -----------------------------------------------------------------------------

#include "../../core/config/SettingsManager.h"
#include "../../core/editor/DocumentStore.h"
#include "../../core/editor/PieceTable.h"
#include "../../core/extensions/ExtensionService.h"
#include "../../core/fs/FileService.h"
#include "../../core/plugins/PluginLoader.h"
#include "../../core/utils/Base64.h"
#include "../../core/utils/Paths.h"
#include "TestZip.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

using vrutti::core::extensions::ExtensionDescription;
using vrutti::core::extensions::ExtensionService;
using vrutti::core::utils::base64_decode;
using vrutti::core::utils::base64_encode;
using vrutti::tests::manifestOf;
using vrutti::tests::TestZip;

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

// A folder of its own for a test.
fs::path makeTempFolder(const std::string& name) {
    const fs::path base = fs::temp_directory_path() / ("vrutti_hardening_" + name);
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    return base;
}

} // namespace

int main() {
    std::cout << "[CoreHardeningTest] The parts of the core that hold data.\n\n";

    // --- the encoding that carries the binary payloads ---------------------------
    std::cout << "-- the encoding that carries the binary payloads\n";
    {
        const std::string text = "Hello, world!";
        const std::string encoded = base64_encode(text);
        check(encoded == "SGVsbG8sIHdvcmxkIQ==", "the text is encoded (got " + encoded + ")");

        std::string decoded;
        check(base64_decode(encoded, decoded), "the text is decoded again");
        check(decoded == text, "the text survives the round trip");
    }
    {
        // A text that is cut in half used to be decoded into what was there
        // before the break, and the caller was told that it was all of it.
        std::string decoded;
        check(!base64_decode("SGVsbG8sIHdvcmxkIQ", decoded),
              "a text whose length is not a multiple of four is refused");
        check(decoded.empty(), "nothing is returned when the text is refused");
    }
    {
        // A character that is not part of the alphabet used to end the text
        // silently: the caller got half of the payload and no word about it.
        std::string decoded;
        check(!base64_decode("SGVs*G8sIHdvcmxkIQ==", decoded),
              "a character that is not part of the alphabet is refused");
        check(!base64_decode("SGVsbG8sIHdvcmxk!Q==", decoded),
              "a character at the end that is not part of the alphabet is refused");
    }
    {
        // The padding may only be at the end.
        std::string decoded;
        check(!base64_decode("SGV=sbG8sIHdvcmxkIQ==", decoded),
              "padding in the middle is refused");
        check(base64_decode("SGVsbG8=", decoded), "the padding at the end is allowed");
        check(decoded == "Hello", "the text with padding is decoded (got " + decoded + ")");
    }
    {
        // Every byte of every value, so that the shifting cannot run over
        // the end of the number it uses.
        bool allOk = true;
        for (int i = 0; i < 256; ++i) {
            const std::string one(1, static_cast<char>(i));
            std::string decoded;
            if (!base64_decode(base64_encode(one), decoded) || decoded != one) {
                allOk = false;
                break;
            }
        }
        check(allOk, "every byte survives the round trip");

        // A long text: the old code shifted a signed number to the left,
        // which is undefined behaviour once the value does not fit.
        const std::string longText(100000, 'x');
        std::string decoded;
        check(base64_decode(base64_encode(longText), decoded) && decoded == longText,
              "a long text survives the round trip");
    }

    // --- the paths of the installation ---------------------------------------------
    std::cout << "\n-- the paths of the installation\n";
    {
        const fs::path base = vrutti::core::utils::appBasePath();

        // A name that walks out of the installation must not name a file
        // outside of it.
        const fs::path outside = vrutti::core::utils::appResourcePath("../../outside.txt");
        const std::string baseText = base.lexically_normal().generic_string();
        const std::string outsideText = outside.lexically_normal().generic_string();
        check(outsideText.compare(0, baseText.size(), baseText) == 0,
              "a name that walks up does not leave the installation (got " + outsideText + ")");

        const fs::path absolute = vrutti::core::utils::appResourcePath("C:/Windows/System32");
        check(absolute.lexically_normal().generic_string().compare(0, baseText.size(), baseText) == 0,
              "an absolute name does not leave the installation (got " +
              absolute.generic_string() + ")");

        const fs::path plugin = vrutti::core::utils::pluginPath("../../outside.dll");
        const std::string pluginText = plugin.lexically_normal().generic_string();
        check(pluginText.compare(0, baseText.size(), baseText) == 0,
              "a plugin that walks up does not leave the installation (got " + pluginText + ")");

        // A name that stays inside is still worked out.
        const fs::path inside = vrutti::core::utils::appResourcePath("src/ext/bin/node.exe");
        check(inside.generic_string().find("node.exe") != std::string::npos,
              "a name that stays inside is still worked out (got " + inside.generic_string() + ")");
    }

    // --- a file that has to stay inside of a folder ------------------------------------
    std::cout << "\n-- a file that has to stay inside of a folder\n";
    {
        const fs::path folder = makeTempFolder("containment");
        {
            std::ofstream out(folder / "inside.txt");
            out << "the file that belongs to the folder\n";
        }
        {
            std::ofstream out(folder.parent_path() / "vrutti_hardening_outside.txt");
            out << "the file that belongs to somewhere else\n";
        }

        const fs::path good = vrutti::core::utils::pathInsideFolder(folder, "inside.txt");
        check(!good.empty() && fs::exists(good),
              "a file that is inside of the folder is found (got " + good.generic_string() + ")");

        check(vrutti::core::utils::pathInsideFolder(folder, "../vrutti_hardening_outside.txt").empty(),
              "a file that walks out of the folder is refused");
        check(vrutti::core::utils::pathInsideFolder(folder, "sub/../inside.txt").empty() == false,
              "a name that walks back in is allowed");
        check(vrutti::core::utils::pathInsideFolder(
                  folder, (folder.parent_path() / "vrutti_hardening_outside.txt").generic_string()).empty(),
              "an absolute name is refused");
        check(vrutti::core::utils::pathInsideFolder(folder, "").empty(),
              "an empty name is refused");
        check(!vrutti::core::utils::pathStaysInside(folder, folder),
              "the folder itself is not inside of the folder");

        std::error_code ec;
        fs::remove(folder.parent_path() / "vrutti_hardening_outside.txt", ec);
        fs::remove_all(folder, ec);
    }

    // --- the version range of an extension ------------------------------------------
    std::cout << "\n-- the version range of an extension\n";
    {
        struct Case {
            const char* range;
            bool compatible;
            const char* what;
        };
        const Case cases[] = {
            { "^1.90.0", true,  "the version of the host itself" },
            { "^1.0.0",  true,  "an older big version" },
            { "^2.0.0",  false, "a newer big version" },
            { "~1.90.0", true,  "the small version of the host" },
            { "~1.90.5", false, "a newer small version" },
            { ">=1.0.0", true,  "a version that is not older" },
            { ">=2.0.0", false, "a version that is newer" },
            { ">1.90.0", false, "a version that is newer than the host" },
            { ">1.89.0", true,  "a version that is older than the host" },
            { "<1.90.0", false, "a version that is older than the host" },
            { "<0.5.0",  false, "a version that is much older" },
            { "<=1.90.0", true, "the version of the host" },
            { "=1.90.0", true,  "the version of the host" },
            { "1.90.0",  true,  "the version of the host without a sign" },
            { "*",       true,  "every version" },
            { "",        true,  "no range at all" },
            { ">=1.0.0 <1.50.0", false, "a range that ends before the host" },
            { ">=1.0.0 <2.0.0",  true,  "a range that holds the host" },
            { ">=2.0.0 || <0.5.0", false, "no alternative holds the host" },
            { ">=2.0.0 || >=1.0.0", true, "one of two alternatives holds the host" },
            { ">=2.0.0 || >=3.0.0", false, "no alternative holds the host" },
            { "not a range", true, "a range that cannot be read" },
        };

        bool allRight = true;
        for (const auto& item : cases) {
            std::string error;
            const bool compatible = ExtensionService::isCompatible(item.range, error);
            if (compatible != item.compatible) {
                std::cerr << "  [note] the range '" << item.range << "' was read as "
                          << (compatible ? "allowed" : "refused") << "\n";
                allRight = false;
            }
        }
        check(allRight, "every range is read the way it is written");
    }

    // --- the entry point of an extension ---------------------------------------------
    std::cout << "\n-- the entry point of an extension\n";
    {
        const fs::path folder = makeTempFolder("entrypoint");
        const fs::path extensions = folder / "extensions";
        fs::create_directories(extensions);

        // The service reads the folder of the extensions from the
        // environment, so the test never touches the extensions of the
        // user.
#ifdef _WIN32
        _putenv_s("VRUTTI_EXTENSIONS_DIR", extensions.string().c_str());
        _putenv_s("VRUTTI_BUILTIN_EXTENSIONS_DIR",
                  (folder / "builtin").string().c_str());
#else
        setenv("VRUTTI_EXTENSIONS_DIR", extensions.string().c_str(), 1);
        setenv("VRUTTI_BUILTIN_EXTENSIONS_DIR",
               (folder / "builtin").string().c_str(), 1);
#endif

        const fs::path outside = folder / "outside.js";
        {
            std::ofstream out(outside);
            out << "// this file is outside of the extension\n";
        }

        // The manifest names a file that is not inside of the extension.
        {
            TestZip zip;
            zip.add("extension/package.json",
                    manifestOf("escape", "test", "1.0.0", "^1.90.0", "../outside.js"));
            zip.add("extension/extension.js", "// the entry point\n");
            const fs::path vsix = folder / "escape.vsix";
            zip.write(vsix.string());

            ExtensionDescription installed;
            std::string error;
            std::string errorCode;
            const bool ok = ExtensionService::installVsix(vsix.string(), installed,
                                                           error, errorCode);
            check(!ok, "an entry point that walks out is refused");
            check(errorCode == "InvalidExtension",
                  "the refusal has the code of a broken extension (got " + errorCode + ")");
            check(!fs::exists(extensions / "test.escape-1.0.0"),
                  "nothing is left behind when the entry point walks out");
        }

        // The manifest names a file with an absolute path.
        {
            TestZip zip;
            zip.add("extension/package.json",
                    manifestOf("absolute", "test", "1.0.0", "^1.90.0",
                               outside.generic_string()));
            zip.add("extension/extension.js", "// the entry point\n");
            const fs::path vsix = folder / "absolute.vsix";
            zip.write(vsix.string());

            ExtensionDescription installed;
            std::string error;
            std::string errorCode;
            const bool ok = ExtensionService::installVsix(vsix.string(), installed,
                                                           error, errorCode);
            check(!ok, "an entry point with an absolute path is refused");
            check(!fs::exists(extensions / "test.absolute-1.0.0"),
                  "nothing is left behind when the entry point is absolute");
        }

        // The manifest names a file that is there.
        {
            TestZip zip;
            zip.add("extension/package.json",
                    manifestOf("good", "test", "1.0.0", "^1.90.0", "./extension.js"));
            zip.add("extension/extension.js", "// the entry point\n");
            const fs::path vsix = folder / "good.vsix";
            zip.write(vsix.string());

            ExtensionDescription installed;
            std::string error;
            std::string errorCode;
            const bool ok = ExtensionService::installVsix(vsix.string(), installed,
                                                           error, errorCode);
            check(ok, "an entry point that is there is accepted (" + error + ")");
            check(fs::exists(extensions / "test.good-1.0.0" / "extension.js"),
                  "the extension is put in place");
        }

        std::error_code ec;
        fs::remove_all(folder, ec);
    }

    // --- the files of the user ------------------------------------------------------
    std::cout << "\n-- the files of the user\n";
    {
        vrutti::core::fs::FileService files;
        const fs::path folder = makeTempFolder("files");

        // A file that is too big is not read into the memory of the core.
        {
            const fs::path big = folder / "big.bin";
            std::error_code ec;
            // The file is made of blocks of a megabyte, so that the test
            // does not need 65 megabytes of memory of its own.
            {
                std::ofstream out(big, std::ios::binary | std::ios::trunc);
                if (out.is_open()) {
                    const std::string block(1024 * 1024, 'x');
                    for (int i = 0; i < 65; ++i) out.write(block.data(),
                                                            static_cast<std::streamsize>(block.size()));
                }
            }
            check(fs::exists(big), "the big file was made");
            if (fs::exists(big)) {
                std::vector<uint8_t> bytes;
                const auto result = files.readFile(big.string(), bytes);
                check(!result.ok, "a file that is too big is not read");
                check(result.errorCode == "Unavailable",
                      "the refusal has the code of a file that cannot be used (got " +
                      result.errorCode + ")");
                check(bytes.empty(), "no bytes of the big file are kept");
            }
            std::error_code removeEc;
            fs::remove(big, removeEc);
        }

        // A rename over a folder must not take the folder with it.
        {
            const fs::path file = folder / "source.txt";
            const fs::path directory = folder / "target";
            const fs::path inside = directory / "inside.txt";
            {
                std::ofstream out(file);
                out << "the file\n";
            }
            fs::create_directories(directory);
            {
                std::ofstream out(inside);
                out << "the file that has to stay\n";
            }

            const auto result = files.rename(file.string(), directory.string(), true);
            check(!result.ok, "a rename over a folder is refused");
            check(fs::exists(inside), "the file inside of the folder is still there");
        }

        // A folder that is nested too deeply is not walked for ever.
        {
            const fs::path deep = folder / "d";
            fs::path current = deep;
            std::error_code ec;
            // The names are one character long, so that the whole path
            // stays short enough for the system that holds the test.
            for (int i = 0; i < 70; ++i) {
                current = current / "l";
            }
            fs::create_directories(current, ec);
            check(!ec, "the deep folder was made");
            if (!ec) {
                const auto result = files.copy(deep.string(), (folder / "deepCopy").string(), true);
                check(!result.ok, "a folder that is nested too deeply is not copied");
            }
        }

        std::error_code ec;
        fs::remove_all(folder, ec);
    }

    // --- the text of a document ------------------------------------------------------
    std::cout << "\n-- the text of a document\n";
    {
        vrutti::core::editor::PieceTable table("", 0);
        table.insert(0, "Hello, world!");
        check(table.length() == 13, "the text has the length it should have");
        check(table.getText() == "Hello, world!", "the text is what was put in");

        // A range that is not there must not take the length of the text
        // with it: the old code took the length away by a number that was
        // far too big.
        table.remove(100, 10);
        check(table.length() == 13, "a removal that is outside of the text changes nothing");

        // A text of a length that the caller chooses must not make a text
        // of that length.
        const std::string part = table.substring(0, static_cast<size_t>(-1));
        check(part.size() <= 13, "a text that is too long is cut at the end of the document");
        check(table.substring(100, 5).empty(), "a text that starts outside of the document is empty");
    }

    // --- a document that is opened twice -----------------------------------------------
    std::cout << "\n-- a document that is opened twice\n";
    {
        const fs::path folder = makeTempFolder("documents");
        const fs::path file = folder / "note.txt";
        {
            std::ofstream out(file);
            out << "the text on the disk\n";
        }

        vrutti::core::editor::DocumentStore store;
        vrutti::core::editor::Document document;
        std::string error;
        std::string errorCode;
        const bool opened = store.openFile("/note.txt", file.string(), "utf8",
                                           document, error, errorCode);
        check(opened, "the document was opened");

        // The user changes the text and does not save it.
        document.content = "the text that the user wrote\n";
        document.isDirty = true;
        check(store.setContent(document.key(), document.content),
              "the change of the user is in the store");

        // The document is opened again. The changes that were not written
        // have to be there: the old code took the text from the disk and
        // threw them away.
        vrutti::core::editor::Document again;
        const bool reopened = store.openFile("/note.txt", file.string(), "utf8",
                                             again, error, errorCode);
        check(reopened, "the document was opened again");
        check(again.content == "the text that the user wrote\n",
              "the changes that were not written are still there (got " + again.content + ")");
        check(again.isDirty, "the document is still marked as changed");

        std::error_code ec;
        fs::remove_all(folder, ec);
    }

    // --- a plugin that takes the place of another one ------------------------------------
    std::cout << "\n-- a plugin that takes the place of another one\n";
    {
        // The loader is asked for a file that is not a library: the answer
        // has to be "nothing", and the loader has to stay whole.
        vrutti::core::plugins::PluginLoader loader;
        vrutti::core::plugins::IPlugin* plugin =
            loader.loadPlugin("this-file-is-not-a-library.dll");
        check(plugin == nullptr, "a file that is not a library is not loaded");
        check(loader.getPlugin("nothing") == nullptr,
              "the loader still answers after a failed load");
    }

    // --- the settings ---------------------------------------------------------------------
    std::cout << "\n-- the settings\n";
    {
        // The settings are a singleton, so the test uses the one that is
        // there. A settings file that is broken must not take the editor
        // down, and the settings that are written have to be readable
        // afterwards.
        auto& settings = vrutti::core::config::SettingsManager::getInstance();
        settings.updateSetting("hardening.test", "42");
        settings.updateSetting("hardening.text", "\"a \\\"quoted\\\" text\"");

        const std::string json = settings.getSettingsJson();
        check(json.find("hardening.test") != std::string::npos,
              "the setting that was set is in the settings");
        check(json.find("\\\"quoted\\\"") != std::string::npos,
              "the text of the setting is escaped (got " + json + ")");

        // Many threads that change the settings at the same time. The
        // settings that are read have to be whole: a text that is half
        // written is a broken file on the disk.
        std::vector<std::thread> threads;
        for (int i = 0; i < 8; ++i) {
            threads.emplace_back([&settings, i]() {
                for (int j = 0; j < 50; ++j) {
                    settings.updateSetting("hardening.thread" + std::to_string(i),
                                            std::to_string(j));
                    settings.getSettingsJson();
                }
            });
        }
        for (auto& thread : threads) thread.join();

        const std::string after = settings.getSettingsJson();
        check(after.find("hardening.thread") != std::string::npos,
              "the settings are whole after many threads have changed them");
    }

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[CoreHardeningTest] FAILED (" << g_failures << " of " << g_checks
                  << " checks)\n";
        return 1;
    }
    std::cout << "[CoreHardeningTest] OK: " << g_checks << " checks\n";
    return 0;
}
