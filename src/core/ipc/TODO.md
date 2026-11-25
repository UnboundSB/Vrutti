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
| `ExtensionHostSpawner.cpp/.h` | Starts the Node.js extension host | Working |
| `VsCodeProxyIds.generated.h` | The numeric proxy id of every shape | Generated |
| `VsCodeProxyFallbacks.generated.h` | The benign answer of every call | Generated |

## The extensions
The extensions are installed by the service of the core, in
`src/core/extensions/ExtensionService.cpp`. The service is the only place
that knows what is installed, so the list the user interface shows and the
list the extension host gets can never drift apart.

The host keeps a registry of the extensions it knows. When the list is only
sent at the start-up, an extension that is installed afterwards is not there
until the editor is started again. The service therefore tells the host about
every change through `ExtHostExtensionService.$deltaExtensions`, the call of
the upstream protocol that carries the extensions that were added and the ones
that were removed.

The host does not wake an extension up by itself when it hears about it: the
caller says which event woke it up. The core therefore follows the delta with
`ExtHostExtensionService.$activate`, with the reason of the upstream
`ExtensionActivationReason`. The rule for the reason is the one of the
upstream workbench: an event that already happened, the star (`*`), or the end
of the start-up (`onStartupFinished`).

| Command | What it does |
|---------|--------------|
| `extensions/install` | Installs a `.vsix` from a file or from an address. |
| `extensions/uninstall` | Removes an extension. |
| `extensions/enable`, `extensions/disable` | Turns an extension on or off. |
| `extensions/request_installed` | Publishes the list of the extensions. |

The archive of an extension is read by the reader of the core, in
`src/core/utils/ZipArchive.cpp`. The reader refuses a file that wants to
leave the folder of the extension, a file that expands too much, and a file
whose checksum does not match.

| Test | What it proves |
|------|----------------|
| `ExtensionInstallerTest` | The archive is read safely and the manifest is checked. |
| `ExtensionInstallProtocolTest` | The host is told about the extension that was installed. |
| `ExtensionInstallE2ETest` | A real extension, installed by the core, runs in the real extension host. |

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

## How the audit counts
The audit counts a method as handled when a handler is registered for it. A
handler that only answers with the default of the return type is a
**placeholder**: the call is answered, but the feature is not there. The
number the audit prints is therefore the number of *answered* calls, not the
number of *implemented* features. The placeholders are the real backlog, and
they are the ones that are worked through shape by shape.

## Tasks (Priority Order — the backend hardening plan)
- [x] **P0.1.** Answer a call without an implementation with the default of
      its return type. The table is generated from the upstream signatures by
      `scripts/generate_exthost_fallbacks.py`, so it cannot drift.
- [x] **P0.2.** Know the gap. `scripts/audit_exthost_handlers.py` compares the
      upstream protocol with the registered handlers and prints the backlog.
      Current state: 87 shapes, 516 methods, 87 handled, 429 missing.
- [ ] **P0.3.** Implement the shapes an ordinary extension touches, in this
      order:
  - [x] `MainThreadDocuments` — opening, editing and saving the documents an
        extension works on. The text of a document lives in the core, in
        `src/core/editor/DocumentStore.cpp`. The extension host keeps its own
        mirror of a document, and the mirror is fed by the delta of
        `ExtHostDocumentsAndEditors.$acceptDocumentsAndEditorsDelta`. The
        delta is sent *before* the call is answered, because the host
        resolves `workspace.openTextDocument()` only when the document is in
        its mirror. Proven by `MainThreadDocumentsTest`.
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
- [x] **P1.4.** Write the settings without blocking the caller. The writer
      runs on a thread of its own, the queue of the changes has a limit, and
      the file is written in one go and then put in place, so a reader never
      sees half of a file.
- [x] **P2.1.** Fuzz the JSON parser with random and truncated input.
      (`JsonFuzzTest.cpp`.)
- [x] **P2.2.** Reject a frame that claims a length it does not have, and a
      frame that is larger than the limit. (`IpcFuzzTest.cpp`.)
- [x] **P2.3.** Drop a malformed frame, log the error code, keep the
      connection.
- [x] **P2.4.** Wrap every handler, so an exception becomes an error reply
      instead of a crash. *(The wrapper exists; the frame limits do not.)*
- [x] **P3.1.** Bound the pending-request table and empty it when a request is
      cancelled, times out, or the peer dies. The table holds 4096 calls; a
      call that is full, that cannot be written, or that is out of time is
      given up on at once.
- [ ] **P3.2.** Unload plugins that are not used any more.
- [ ] **P3.3.** Release the buffers of a document when it is closed.
- [ ] **P4.1.** Restrict the named pipe to the user that owns the session.
- [ ] **P4.2.** Validate every message of the webview bridge.
- [ ] **P4.3.** Resolve the paths that come from an extension inside the
      workspace before they are used.

## The hardening of the parts that hold data

The transport is only one part of the core. The parts that hold the data of
the user -- the files, the documents, the settings, the themes and the
extensions -- were walked one by one, and every hole that was found was
closed with a test.

| What was wrong | What it is now |
| -------------- | -------------- |
| The decoder of the binary payloads stopped at the first character it did not know and handed back half of the payload as if it were all of it. | The decoder refuses a text that is not valid and says so. (`Base64.h`.) |
| A name of a file of the installation could walk out of the installation with `..`. | The name has to stay inside. (`Paths.cpp`.) |
| The entry point of an extension could name a file outside of the extension. | The entry point has to be a file of the extension. (`ExtensionService.cpp`.) |
| The version range of an extension was read as the *lowest* version it accepts. | The range is read as it is written: `^`, `~`, `>`, `>=`, `<`, `<=`, `=`, `*`, a space between two parts, `||` between two alternatives. |
| A file could be read into the memory of the core without a limit. | A file that is bigger than 64 MiB is not read. (`FileService.cpp`.) |
| A rename over a folder removed the whole folder with everything in it. | A folder is never replaced by a file. |
| A folder tree was walked without a limit. | The walk has a depth and a number of entries. |
| A range of a document that was not there took the length of the text with it. | A range outside of the document changes nothing. (`PieceTable.cpp`.) |
| A document that was opened again took the text from the disk and threw the changes of the user away. | The changes that were not written are kept. (`DocumentStore.cpp`.) |
| A plugin that took the name of another one was thrown away without being shut down. | The old plugin is shut down and given back first. (`PluginLoader.cpp`.) |
| A theme could name a file outside of its own package. | The file has to be inside of the package. (`ThemeService.cpp`.) |
| A path from the page was put into a command for the shell. | The program is started without a shell. (`Window.cpp`.) |
| A worker of the window was left running when the window went away. | The workers are waited for before the window goes away. (`Window.cpp`.) |

The proof is in `src/tests/integration/CoreHardeningTest.cpp`.


## Related
- `docs/BACKEND_HARDENING.md` — the full plan and the verification table.
- `src/ext/protocols/TODO.md` — the shared protocol catalog.
- `src/ext/api/TODO.md` — the Node-side API shims.
- `docs/EXTENSION_COMPATIBILITY.md` — the compatibility mandate.
- `scripts/audit_exthost_handlers.py` — the missing-method report.
- `scripts/generate_exthost_fallbacks.py` — the generator of the default table.
