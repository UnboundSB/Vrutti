# Backend hardening plan

The backend is the native core, the IPC layer and the extension host
contract. This is the order in which it is made airtight, and why.

The rule that drives everything: **an extension that works in the reference
editor must work in Vrutti**. That means the extension host must never be
surprised by the renderer.

---

## P0 -- The extension host contract

### P0.1 Never answer a missing feature with an error

The extension host turns an error reply into a rejected promise. Most of
those calls are not awaited, so the rejection becomes an unhandled rejection,
which the host reports as an extension error. An extension that uses a
feature Vrutti does not have would look broken instead of limited.

So a call without an implementation is answered with the benign default of
its declared return type:

| Return type | Answer |
| ----------- | ------ |
| `void`, `Promise<void>` | `null` |
| `Promise<string>` | `""` |
| `Promise<number>` | `0` |
| `Promise<boolean>` | `false` |
| `Promise<Some[]>` | `[]` |
| `Promise<Some \| undefined>` | `null` |

The table is generated from the upstream protocol definition by
`scripts/generate_exthost_fallbacks.py`, so it cannot drift from the
signatures it was derived from.

An error reply is still correct when a handler exists and the operation
genuinely failed, and when the peer broke the protocol.

### P0.2 Know the gap

`scripts/audit_exthost_handlers.py` compares the upstream protocol with the
handlers Vrutti registers and prints what is missing, grouped by shape. The
current state:

```
upstream shapes:   87
upstream methods:  516
handled:            87
missing:           429
```

A missing call is a missing feature, not a failure -- but it is a gap, and
the gap is the backlog.

The audit counts the handlers that answer later as well, so the number it
prints is the number that is really answered.

A handler that only answers with the default of the return type is a
**placeholder**: the call is answered, but the feature is not there. The
number above is the number of *answered* calls, not the number of
*implemented* features. The placeholders are the real backlog.

### P0.3 Implement the shapes an ordinary extension touches

Priority order, by how likely an extension is to use them:

| # | Shape | Why | State |
| - | ----- | --- | ----- |
| 1 | `MainThreadDocuments` | Opening, editing and saving the documents an extension works on. Without it no extension can do anything with a file. | done |
| 2 | `MainThreadFileSystem` | Reading and writing files. | done |
| 3 | `MainThreadEditor` | The visible state of an editor: selections, visible ranges. | |
| 4 | `MainThreadLanguages` | Diagnostics, tokenization, the language of a document. | |
| 5 | `MainThreadLanguageFeatures` | The completion, hover and signature providers an extension registers. | |
| 6 | `MainThreadTerminal` | Terminal creation and shell integration. | |
| 7 | `MainThreadTask` | Task providers and execution. | |
| 8 | `MainThreadQuickInput` | Input boxes and pickers. | |
| 9 | `MainThreadNotification`, `MainThreadMessageService` | Progress and messages. | |
| 10 | `MainThreadDebugService` | Debug adapters. | |
| 11 | `MainThreadSearch` | File and text search providers. | |
| 12 | `MainThreadWebview` | Webview panels and views. | |
| 13 | `MainThreadConsole` | The output channel of an extension. | |

The shapes that belong to features Vrutti does not have -- chat, agents,
embeddings, browsers, authentication, notebooks -- stay on the fallback
answer. That is a deliberate, documented decision, not an oversight.

### The document contract

`MainThreadDocuments` is the first shape that has a *mirror* on the other
side of the pipe. The text of a document lives in the core, in
`src/core/editor/DocumentStore.cpp`. The extension host keeps its own mirror
of every document, and that mirror is what an extension reads. The mirror is
fed by the delta of `ExtHostDocumentsAndEditors.$acceptDocumentsAndEditorsDelta`.

Two rules make it work:

* **The delta goes first.** The host resolves `workspace.openTextDocument()`
  only when the document is in its mirror, so the call is answered *after*
  the delta was taken. That is why the document calls are answered later
  rather than on the spot.
* **The store and the mirror stay in sync.** A document that the store drops
  is reported to the host as a removed document, so the mirror never keeps a
  document that no longer exists.

---

## The installation of an extension

An extension is code of a stranger. The installation is therefore the place
where the editor is most careful.

The work is done by `src/core/extensions/ExtensionService.cpp`. The service
is the only place that knows what is installed, so the list the user
interface shows and the list the extension host gets come from the same
source and can never drift apart.

### The rules

* **The archive is read by a reader that trusts nothing.** The reader of the
  core (`src/core/utils/ZipArchive.cpp`) never trusts a size that is written
  in the archive. The content is bounded while it is produced, and the
  checksum of every file is checked when it is written.
* **A file may not leave the folder of the extension.** A name with a parent
  part (`..`), an absolute path, a drive letter or a backslash is refused,
  and the path of every file is checked again after it has been resolved.
* **A file may not expand without end.** The number of files, the size of a
  file, the size of the whole archive and the ratio between the archive and
  its content all have a limit.
* **The manifest is checked before anything is written.** The name, the
  publisher and the version have to be usable as a part of a folder name, the
  version of the host that the extension asks for has to exist, and the entry
  point of the extension has to be in the archive.
* **A refused installation leaves nothing behind.** The extension is unpacked
  into a folder of its own first. When anything goes wrong, that folder is
  removed and the old version of the extension stays as it was.
* **Nothing goes through a shell.** The address of an archive is an argument
  of a program, never a part of a command. The old installer built a command
  with the address and the name of the extension inside it, which made a
  quote in either of them a way to run a command.

### The host has to know

The extension host keeps a registry of the extensions it knows. When the list
is only sent at the start-up, an extension that is installed afterwards is
not there until the editor is started again.

Every change is therefore sent to the host through
`ExtHostExtensionService.$deltaExtensions`, the call of the upstream protocol
that carries the extensions that were added and the ones that were removed.
The shape is the upstream `IExtensionDescriptionDelta`: `versionId`,
`toRemove`, `toAdd`, `addActivationEvents`, `myToRemove` and `myToAdd`.

Knowing about an extension is not the same as running it. The host does not
wake an extension up by itself: the caller says which event woke it up. The
core therefore follows the delta with
`ExtHostExtensionService.$activate` and the upstream
`ExtensionActivationReason`. The rule for the reason is the one of the
upstream workbench: an event that already happened, the star (`*`), or the
end of the start-up (`onStartupFinished`).

The order matters. The delta has to be taken by the host before the
extension is woken up, otherwise the host does not know the extension yet and
the wake-up is answered with "unknown extension". The core therefore sends
the wake-up from the answer of the delta, never before it.

---

## P1 -- Nothing may hang

* Every request that the core sends to the extension host gets a timeout.
  A promise that never settles is a memory leak and a wedged extension.
* The reply to a request that was cancelled is dropped, not delivered.
* The terminal shuts down without a deadlock.
* The settings writer does not block the caller.

---

## P2 -- Nothing may crash

* The JSON parser is fuzzed with random and truncated input.
* The IPC frame decoder rejects a frame that claims a length it does not
  have, and a frame that is larger than the limit.
* A malformed frame never takes the core down: the frame is dropped, the
  error code is logged, the connection continues.
* Every handler is wrapped so that an exception becomes an error reply
  instead of a crash.

---

## P3 -- Nothing may leak

* The pending request table of the IPC client is bounded and is emptied when
  a request is cancelled, times out or the peer dies.
* Plugins are unloaded when they are not used any more, so their libraries
  do not stay locked in the process.
* The buffers of the editor are released when a document is closed.

---

## P4 -- Nothing may be reached that should not be

* The named pipe is restricted to the user that owns the session.
* The webview bridge validates every message before it reaches the core.
* The paths that come from an extension are resolved inside the workspace
  before they are used.

---

## How it is verified

| Check | Command |
| ----- | ------- |
| The extension contract has no unhandled failure | `python scripts/audit_exthost_handlers.py` |
| The fallback table matches the upstream signatures | `python scripts/generate_exthost_fallbacks.py --check` |
| The proxy ids match the upstream tree | `python scripts/generate_proxy_ids.py --check` |
| The error code registry is in sync | `python scripts/generate_error_codes.py --check` |
| Every folder is documented | `python scripts/generate_structure_docs.py --check` |
| The extension host activates | `node scripts/test_exthost_activation.js` |
| The JSON layer is correct | `build/test_json.exe` |
| The theme service is correct | `build/vrutti_theme_test.exe` |
| The file system surface is correct | `build/vrutti_exthost_fs_test.exe` |
| The document surface is correct | `build/vrutti_documents_test.exe` |
| The installation of an extension is safe | `build/vrutti_installer_test.exe` |
| An installed extension reaches the host | `build/vrutti_install_protocol_test.exe` |
| An installed extension really runs | `build/vrutti_install_e2e_test.exe` |
