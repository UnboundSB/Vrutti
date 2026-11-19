# Vrutti IDE — src/ext/protocols/ TODO

**Scope:** `vrutti_ide/src/ext/protocols/` only. Parent `vrutti/` never touched.

## Purpose
Shared JSON-RPC protocol definitions. Single source of truth for IPC method names, params, and result schemas used by both C++ (`VSCodeProtocol`) and Node.js (`ipc.js`).

## HARD REQUIREMENT: Extension Compatibility
Both C++ and Node implementations must match this catalog exactly. Extensions depend on predictable IPC behavior.

## Planned Files
| File | Purpose |
|------|---------|
| `methods.json` | Method catalog: name, direction, params schema, result schema, example payload |
| `protocol.ts` | TypeScript interfaces generated from methods.json |
| `protocol.h` | C++ structs/enums generated from methods.json |

## Tasks (Priority Order)
- [ ] **PRT1.** Create `methods.json` — catalog every IPC method from `docs/EXTENSION_COMPATIBILITY.md`:
  - `workspace/folders`, `document/open`, `document/get`, `document/edited`
  - `configuration/get`, `configuration/set`
  - `command/register`, `command/execute`, `command/get`
  - `notification/show`, `notification/hide`
  - `terminal/create`, `terminal/write`, `terminal/resize`, `terminal/data`, `terminal/show`
  - `extensions/list`, `extensions/get`, `extensions/install`, `extensions/activate`
  - `language/completion/register`, `language/completion/request`, `language/hover/request`
  - `debug/start`, `debug/setBreakpoints`, `debug/action`, `debug/stop`
  - `memento/get`, `memento/set`, `memento/delete`
  - `secrets/get`, `secrets/store`, `secrets/delete`
  - `webview/create`, `webview/setHtml`, `webview/postMessage`, `webview/onDidMessage`
  - `treeview/register`, `treeview/reveal`
  - `settings/get`, `settings/set`
- [ ] **PRT2.** Define JSON Schema for each method's `params` and `result`
- [ ] **PRT3.** Generate TypeScript types from `methods.json` for `ipc.js` and API shims
- [ ] **PRT4.** Generate C++ headers from `methods.json` for `VSCodeProtocol`
- [ ] **PRT5.** Add protocol versioning (forward/backward compatibility)
- [ ] **PRT6.** Document direction for every method (C++→Node, Node→C++, bi-di)

## Related
- `src/ext/api/TODO.md` — API shims consume these IPC methods
- `src/core/ipc/TODO.md` — C++ IPC implementation
- `docs/EXTENSION_COMPATIBILITY.md` — compatibility mandate
