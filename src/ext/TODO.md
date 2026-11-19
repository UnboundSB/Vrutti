# Vrutti IDE — src/ext/ TODO

**Scope:** `vrutti_ide/src/ext/` only. Parent `vrutti/` never touched.

## Purpose
Node.js Extension Host bridge. Boots the official VS Code Extension Host (`extHostProcess`) and bridges it to the C++ core via JSON-RPC IPC. Runs VS Code-compatible extensions.

## Directory Inventory
| Dir | Purpose | Status |
|-----|---------|--------|
| `bootstrapper.js` | Spawns extHost process with pipe env | Present |
| `ipc.js` | JSON-RPC client over named pipe | Present |
| `lsp-client.js` | LSP client | Present |
| `dap-client.js` | DAP client | Present |
| `textmate-engine.js` | TextMate grammar tokenizer | Present |
| `theme-resolver.js` | VS Code theme → color mapping | Present |
| `crypto-polyfill.js` | Crypto polyfill | Present |
| `legacy/` | Legacy bootstrapper / API shim | Present |
| `builtin-themes/` | Default bundled themes | Present |
| `bin/` | node.exe and helper binaries | Present |
| `bridge/` | Node-V8 bridge headers | Present |
| `test_exts/` | Test extensions (gitlens, prettier, eslint) | Present |
| `protocols/` | JSON-RPC protocol definitions (NEW) | Empty |
| `api/` | VS Code API shim layer (NEW) | Empty |

## Critical Issues (from problem.md)
- [ ] **EX1.** `bootstrapper.js` exits on ExtHost exit without killing child processes → orphaned language servers/debuggers. Fix: `process.on('exit')` kill tree.
- [ ] **EX2.** `ipc.js` connection `end`/`error` calls `process.exit(0)`; pending promises never resolved. Fix: resolve/reject all pending on disconnect.
- [ ] **EX3.** `dap-client.js` `Content-Length` regex expects exactly one space. Fix: `\\s*:\\s*(\\d+)`.
- [ ] **EX4.** Duplicate DAP: `core/plugins/DebugAdapterService.cpp` duplicates `dap-client.js`. Remove C++ version.

## ext-Level Tasks (Priority Order)
- [ ] **EX5.** Complete `protocols/` — shared JSON-RPC method catalog (MainThreadShape / ExtHostShape mirror).
- [ ] **EX6.** Build `api/` — VS Code API shim layer that extensions call; shims unsupported APIs gracefully.
- [ ] **EX7.** Wire `lsp-client.js` → `LanguageFeatureService` IPC for IntelliSense.
- [ ] **EX8.** Wire `textmate-engine.js` → frontend CodeMirror syntax highlighting.
- [ ] **EX9.** Wire `theme-resolver.js` → `core/theme/` for theme application.
- [ ] **EX10.** Implement extension install/uninstall over IPC (no `_popen` in C++).
- [ ] **EX11.** Extension activation events (`onStartupFinished`, `*`, custom events).
- [ ] **EX12.** Test extension compatibility with `test_exts/` (gitlens, prettier, eslint).

## Related
- `src/app/TODO.md` — spawns bootstrapper.js
- `src/core/ipc/TODO.md` — IPC protocol (C++ side)
- `src/ext/protocols/TODO.md` — shared protocol definitions
- `src/core/plugins/VSCodeExtHostService.cpp` — builds init data for ExtHost
