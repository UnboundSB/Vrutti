// -----------------------------------------------------------------------------
// The runner of programs and the splitter of a command line.
//
// The source control of the editor used to ask git through a shell:
//
//     cmd.exe /c "cd /d \"C:\\work\" && git status 2>&1"
//
// A shell does not know what a backslash in front of a quote means, so the
// folder came out as `\"C:\\work\"` and every question of the source control
// failed with "the filename, directory name, or volume label syntax is
// incorrect". The program is now run without a shell and the folder of the
// workspace is given to it by the system.
//
// The test covers:
//
//   * the splitter: parts, quotes, backslashes, and the limits,
//   * the runner: the folder the program is run in, the limit of the text,
//     the deadline, and a program that is not there at all.
// -----------------------------------------------------------------------------

#include "../../core/utils/ProcessRunner.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using vrutti::core::utils::runProcess;
using vrutti::core::utils::splitCommandLine;

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

std::string toLower(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

// A folder of its own for the test, with a file in it.
fs::path makeWorkFolder() {
    const fs::path base = fs::temp_directory_path() / "vrutti_process_runner_test";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    return base;
}

} // namespace

int main() {
    std::cout << "[ProcessRunnerTest] The runner and the splitter.\n\n";

    // --- the splitter --------------------------------------------------------
    std::cout << "-- the splitter\n";
    {
        auto empty = splitCommandLine("");
        check(empty.empty(), "a text that is empty has no parts");

        auto one = splitCommandLine("git");
        check(one.size() == 1 && one[0] == "git", "a text of one word has one part");

        auto two = splitCommandLine("git status --porcelain");
        check(two.size() == 3 && two[0] == "git" && two[1] == "status" &&
              two[2] == "--porcelain",
              "the parts of a command line are the words of it");

        auto manySpaces = splitCommandLine("  git   status  ");
        check(manySpaces.size() == 2 && manySpaces[0] == "git" &&
              manySpaces[1] == "status",
              "the spaces between the words do not make parts of their own");

        // The format of the log of git holds quotes. The quotes group the
        // text and are taken away.
        auto quoted = splitCommandLine(
            "git log --format=\"%H@@@%P@@@%s\" --date=short");
        check(quoted.size() == 4 && quoted[2] == "--format=%H@@@%P@@@%s",
              "the quotes group a part and are taken away");

        // A folder of Windows is full of backslashes. They stay.
        auto windowsPath = splitCommandLine("git log \"C:\\work\\repo\"");
        check(windowsPath.size() == 3 && windowsPath[2] == "C:\\work\\repo",
              "the backslashes of a folder stay as they are");

        // A backslash in front of a quote keeps the quote inside the part.
        auto escaped = splitCommandLine("git log \"a\\\"b\"");
        check(escaped.size() == 3 && escaped[2] == "a\"b",
              "a backslash in front of a quote keeps the quote inside the part");

        // A part that is between two quotes stays together.
        auto together = splitCommandLine("git log \"one two\"");
        check(together.size() == 3 && together[2] == "one two",
              "a part that is between two quotes stays together");

        // The shell is not there: the letters that a shell would look for
        // are letters of an argument.
        auto meta = splitCommandLine("git log && rm -rf / && echo done");
        check(meta.size() == 9 && meta[2] == "&&",
              "the letters of a shell are letters of an argument");

        // The limits.
        std::string many(5000, 'x');
        auto tooMany = splitCommandLine(many);
        check(tooMany.empty(), "a part that is too long is refused");

        std::string tooManyParts;
        for (int i = 0; i < 100; i++) tooManyParts += "x ";
        check(splitCommandLine(tooManyParts).empty(),
              "a command line with too many parts is refused");
    }

    // --- the runner ---------------------------------------------------------
    std::cout << "\n-- the runner\n";
    {
        const fs::path work = makeWorkFolder();
        {
            std::ofstream marker(work / "marker.txt");
            marker << "the marker";
        }

        // The program is run in the folder that is given to it. The program
        // is not told to change folder: a shell does not know what a quote
        // means.
#ifdef _WIN32
        auto where = runProcess("cmd.exe", { "/c", "cd" }, 64 * 1024, 30000,
                                work.string());
        check(where.started, "the program is run");
        check(where.exitCode == 0, "the program ends well");
        check(toLower(where.output).find(toLower(work.string())) != std::string::npos,
              "the program is run in the folder that was given to it");
#else
        auto where = runProcess("/bin/pwd", {}, 64 * 1024, 30000, work.string());
        check(where.started, "the program is run");
        check(where.exitCode == 0, "the program ends well");
        check(where.output.find(work.string()) != std::string::npos,
              "the program is run in the folder that was given to it");
#endif

        // A folder that is not there is a failure of its own: the program
        // would otherwise run in the folder of the editor and would touch
        // the wrong files.
        auto missing = runProcess("git", { "--version" }, 64 * 1024, 30000,
                                  (work / "not-here").string());
        check(!missing.started, "a program is not run in a folder that is not there");
        check(!missing.error.empty(), "the reason is told");

        // A program that is not there.
        auto noProgram = runProcess("vrutti-no-such-program", {}, 64 * 1024, 30000,
                                    work.string());
        check(!noProgram.started, "a program that is not there is not started");
        check(!noProgram.error.empty(), "the reason is told");

        // The text of the program is kept inside of its limit.
        auto loud = runProcess("cmd.exe", { "/c", "echo hello" }, 4, 30000,
                               work.string());
        check(loud.truncated, "text that is too long is cut short");

        // A program that never ends is stopped.
        auto forever = runProcess("cmd.exe", { "/c", "ping -n 60 127.0.0.1 > nul" },
                                  64 * 1024, 1500, work.string());
        check(forever.exitCode == -1, "a program that takes too long is stopped");
        check(!forever.error.empty(), "the reason is told");

        std::error_code ec;
        fs::remove_all(work, ec);
    }

    std::cout << "\n";
    if (g_failures > 0) {
        std::cerr << "[ProcessRunnerTest] FAILED (" << g_failures << " of "
                  << g_checks << " checks)\n";
        return 1;
    }
    std::cout << "[ProcessRunnerTest] OK: " << g_checks << " checks\n";
    return 0;
}
