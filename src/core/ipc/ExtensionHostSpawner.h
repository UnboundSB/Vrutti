#pragma once

#include <filesystem>
#include <string>

namespace vrutti::core::ipc {

    // -------------------------------------------------------------------------
    // The starter of the Node.js extension host.
    //
    // The extension host is the real thing: it runs the `vscode.*` API of the
    // extensions. It is a separate process that connects to the named pipe of
    // the core and speaks the protocol over it.
    //
    // The starter is a component of its own, so that the application and the
    // tests start the host in exactly the same way. A test that starts the
    // host itself proves the chain that the user actually runs.
    // -------------------------------------------------------------------------
    class ExtensionHostSpawner {
    public:
        // Starts the extension host and lets it connect to the pipe.
        //
        // `pipeName` is the name of the pipe, not its full path: the full
        // path is built here, because that is what the host expects on
        // windows.
        //
        // Returns false when the host could not be started. The reason is in
        // `error`.
        static bool spawn(const std::string& pipeName, std::string& error);

        // The full path of the pipe, the way the host expects it.
        static std::string pipePath(const std::string& pipeName);

        // The path of the node runtime that runs the host.
        static std::filesystem::path nodePath();

        // The path of the bootstrapper, the program that starts the host.
        static std::filesystem::path bootstrapperPath();
    };

} // namespace vrutti::core::ipc
