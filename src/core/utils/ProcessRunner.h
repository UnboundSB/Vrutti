// -----------------------------------------------------------------------------
// ProcessRunner -- runs a program of the installation and reads what it says.
//
// The program is started without a shell. Every name that comes from the
// user -- a folder of the workspace, a name of a file -- is an argument of
// the program and never a part of a command, so a name that holds a quote
// or a semicolon cannot become a command of its own.
//
// The text of the program is bounded, and the program is stopped when it
// takes too long. A program that writes without end, or that waits for
// something that never happens, may not take the editor with it.
// -----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

namespace vrutti::core::utils {

    // What a program that was run had to say.
    struct ProcessResult {
        bool started = false;        // the program was started at all
        int exitCode = -1;           // the number it left behind
        std::string output;          // what it wrote
        bool truncated = false;      // it wrote more than it was allowed to
        std::string error;           // why it could not be run
    };

    // Runs a program and reads what it writes.
    //
    // The arguments are the arguments of the program: they are put between
    // quotes the way the command line of the system wants them, and they
    // are never looked for as a command of a shell.
    //
    // `maxOutputBytes` is the most text that is kept. `timeoutMs` is how
    // long the program may run. A program that is stopped has the code -1.
    ProcessResult runProcess(const std::string& executable,
                             const std::vector<std::string>& arguments,
                             size_t maxOutputBytes,
                             unsigned int timeoutMs);

} // namespace vrutti::core::utils
