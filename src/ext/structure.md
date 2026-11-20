# ext (src/ext/)

## Purpose
The extension side: the launcher, the extension host entry point, the API surface and the bundled themes.

## Files
- **bootstrapper.js**: JavaScript module: bootstrapper.
- **crypto-polyfill.js**: Preload script for the VS Code Extension Host process.
- **dap-client.js**: JavaScript module: dap client.
- **errorCodes.js**: JavaScript module: errorCodes.
- **ipc.js**: JavaScript module: ipc.
- **lsp-client.js**: JavaScript module: lsp client.
- **package-lock.json**: Package data for package lock.
- **package.json**: Package data for package.
- **test_runner.js**: JavaScript module: test runner.
- **textmate-engine.js**: JavaScript module: textmate engine.
- **theme-resolver.js**: JavaScript module: theme resolver.
- **TODO.md**: Open questions and planned work for this folder.

## Public Methods
- **DapClient()** (exported)
- **ErrorCode()** (exported)
- **ERROR_CODE_NAMES()** (exported)
- **errorCodeId()** (exported)
- **errorCodeName()** (exported)
- **formatError()** (exported)
- **LspClient()** (exported)
- **initOniguruma()** (exported)
- **registerGrammars()** (exported)
- **tokenizeDocument()** (exported)
- **loadThemeRecursive()** (exported)

## Child Directories
- **api/**: The public extension API. The shape of the `vscode` namespace that extensions are written against.
- **bin/**: The runtime binaries shipped with the application, including the Node.js executable.
- **bridge/**: The bridge between the native core and the extension host: the protocol, the client and the server sides.
- **builtin-themes/**: The themes that ship with the application: the syntax highlighting and the user interface colours.
- **exthost/**: The extension host entry point. Vrutti's own launcher for the shared extension host implementation.
- **legacy/**: The previous standalone extension host, kept for reference only. It is not part of the running pipeline.
- **protocols/**: Protocol definitions shared between the core and the extension host.
