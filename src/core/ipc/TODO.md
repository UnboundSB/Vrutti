# Vrutti IDE — src/core/ipc/ TODO

**Scope:** `vrutti_ide/src/core/ipc/` only. Parent `vrutti/` never touched.

## Purpose
The transport and the protocol that connect the C++ core to the Node.js
Extension Host. Newline-delimited JSON over a named pipe (Windows) or a Unix
domain socket. The wire format is called *VruttiJSONBridge* and is **not**
JSON-RPC 2.0.

## HARD REQUIREMENT: Extension Compatibility
The IPC protocol is the contract between the core and the Extension Host. An
extension that works in the reference editor must work in Vrutti. A call the
core cannot answer must therefore never become an error reply -- see
`docs/BACKEND_HARDENING.md`.

## Files
| File | Purpose | Status |
|------|---------|--------|
| `IPCClient.cpp/.h` | Pipe transport, frame decoding, pending-call tracking | Working |
| `VSCodeProtocol.cpp/.h` | The router for the `MainThread*` / `ExtHost*` shapes | 87 handlers |
| `VsCodeProxyIds.generated.h` | The numeric proxy id of every shape | Generated |
| `VsCodeProxyFallbacks.generated.h` | The benign answer of every call | Generated |

## The contract
* Every inbound request is answered. An unanswered request wedges the calling
  extension inside `await` forever.
* A call **without** an implementation is answered with the benign default of
  its declared return type (`null`, `""`, `0`, `false`, `[]`), never with an
  error. The extension host turns an error into a rejected promise, and the
  calls that are not awaited turn that into an unhandled rejection, which the
  host reports as an extension error.
* An error reply means "the call you made is broken", not "I do not have that
  feature".

## Tasks (Priority Order — the backend hardening plan)
- [x] **P0.1.** Answer a call without an implementation with the default of
      its return type. The table is generated from the upstream signatures by
      `scripts/generate_exthost_fallbacks.py`, so it cannot drift.
- [x] **P0.2.** Know the gap. `scripts/audit_exthost_handlers.py` compares the
      upstream protocol with the registered handlers and prints the backlog.
      Current state: 87 shapes, 516 methods, 87 handled, 429 missing.
- [ ] **P0.3.** Implement the shapes an ordinary extension touches, in this
      order:
  - [ ] `MainThreadDocuments` — opening, editing and saving the documents an
        extension works on.
  - [x] `MainThreadFileSystem` — reading and writing files. The file service
        lives in `src/core/fs/FileService.cpp`; the twelve file operations are
        answered by the router, and the provider calls are forwarded to the
        extension host when an extension registered a provider for the scheme.
  - [ ] `MainThreadEditor` — the visible state of an editor.
  - [ ] `MainThreadLanguages` — diagnostics and the language of a document.
  - [ ] `MainThreadLanguageFeatures` — completion, hover, signature providers.
  - [ ] `MainThreadTerminal` — terminal creation and shell integration.
  - [ ] `MainThreadTask` — task providers and execution.
  - [ ] `MainThreadQuickInput` — input boxes and pickers.
  - [ ] `MainThreadNotification`, `MainThreadMessageService` — progress and
        messages.
  - [ ] `MainThreadDebugService` — debug adapters.
  - [ ] `MainThreadSearch` — file and text search providers.
  - [ ] `MainThreadWebview` — webview panels and views.
  - [ ] `MainThreadConsole` — the output channel of an extension.
- [x] **P1.1.** Give every outbound request a timeout, so a promise that never
      settles cannot leak. The watchdog lives in `IPCClient`.
- [ ] **P1.2.** Drop the reply of a request that was cancelled.
- [ ] **P1.3.** Shut the terminal down without a deadlock.
- [ ] **P1.4.** Write the settings without blocking the caller.
- [ ] **P2.1.** Fuzz the JSON parser with random and truncated input.
- [ ] **P2.2.** Reject a frame that claims a length it does not have, and a
      frame that is larger than the limit.
- [ ] **P2.3.** Drop a malformed frame, log the error code, keep the
      connection.
- [ ] **P2.4.** Wrap every handler, so an exception becomes an error reply
      instead of a crash. *(The wrapper exists; the frame limits do not.)*
- [ ] **P3.1.** Bound the pending-request table and empty it when a request is
      cancelled, times out, or the peer dies.
- [ ] **P3.2.** Unload plugins that are not used any more.
- [ ] **P3.3.** Release the buffers of a document when it is closed.
- [ ] **P4.1.** Restrict the named pipe to the user that owns the session.
- [ ] **P4.2.** Validate every message of the webview bridge.
- [ ] **P4.3.** Resolve the paths that come from an extension inside the
      workspace before they are used.

## Related
- `docs/BACKEND_HARDENING.md` — the full plan and the verification table.
- `src/ext/protocols/TODO.md` — the shared protocol catalog.
- `src/ext/api/TODO.md` — the Node-side API shims.
- `docs/EXTENSION_COMPATIBILITY.md` — the compatibility mandate.
- `scripts/audit_exthost_handlers.py` — the missing-method report.
- `scripts/generate_exthost_fallbacks.py` — the generator of the default table.
