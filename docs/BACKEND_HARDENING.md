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

* The named pipe is restricted to the user that owns the session. The pipe is
  made with a security that only the user of the session and the system may
  open; the socket of the other systems belongs to the user alone.
* The webview bridge validates every message before it reaches the core: a
  message that is too big is not read, and the name of a method has to be a
  name.
* The paths that come from an extension are resolved inside the workspace
  before they are used. The paths of the installation, of a theme and of an
  extension are resolved inside their own root; the paths of the workspace
  are not yet.

---

## The hardening of the parts that hold data

The transport is only one part of the core. The parts that hold the data of
the user -- the files, the documents, the settings, the themes, the plugins
and the extensions -- were walked one by one, in three directions:

* **The safety of the data.** Can a name that comes from outside reach a file
  that it should not reach? Can a file of the user be taken away, changed or
  read without being asked for?
* **The wholeness of the data.** Can two things be half done at the same
  time? Can a file be read while it is written? Can a length be counted
  wrongly and take the memory of the editor with it?
* **The closeness of the parts.** Does one part reach into another one
  directly? Does a worker of the window outlive the window? Does a plugin
  that takes the name of another one leave the old one running?

Every hole that was found was closed, and every fix has a test.

| What was wrong | What it is now | Where |
| -------------- | -------------- | ----- |
| The decoder of the binary payloads stopped at the first character it did not know and handed back half of the payload as if it were all of it. The arithmetic was done on a signed number, which is undefined behaviour once the value does not fit. | The decoder refuses a text that is not valid and says so; the arithmetic is done on unsigned numbers. | `core/utils/Base64.h` |
| A name of a file of the installation could walk out of the installation with `..` or name a place of its own. | The name has to stay inside; the path of the installation itself is returned otherwise. | `core/utils/Paths.cpp` |
| The entry point of an extension could name a file outside of the extension, and the extension host would load it. | The entry point has to be a file of the extension, before the link is followed. | `core/extensions/ExtensionService.cpp` |
| The version range of an extension was read as the *lowest* version it accepts: `<0.5.0` and `>=1.0.0 <1.50.0` were both read as "1.0.0". | The range is read as it is written: `^`, `~`, `>`, `>=`, `<`, `<=`, `=`, `*`, a space between two parts, `||` between two alternatives. A range that cannot be read is let through. | `core/extensions/ExtensionService.cpp` |
| A file could be read into the memory of the core without a limit. | A file that is bigger than 64 MiB is not read, and the bytes of it are not kept. | `core/fs/FileService.cpp` |
| A rename over a folder removed the whole folder with everything in it. | A folder is never replaced by a file; the target stays as it was. | `core/fs/FileService.cpp` |
| A folder tree was walked without a limit, and a folder was listed without a limit. | The walk has a depth of 64 and 100 000 entries; a folder holds 100 000 entries. | `core/fs/FileService.cpp` |
| A range of a document that was not there took the length of the text with it, and a text of a length that the caller chose was made ready before it was filled. | A range outside of the document changes nothing; a text is cut at the end of the document. | `core/editor/PieceTable.cpp` |
| A document that was opened again took the text from the disk and threw the changes of the user away. | The changes that were not written are kept, and the document stays marked as changed. | `core/editor/DocumentStore.cpp` |
| A plugin that took the name of another one was thrown away without being shut down: the old plugin kept running and its memory was never given back. | The old plugin is shut down and given back first. | `core/plugins/PluginLoader.cpp` |
| A theme could name a file outside of its own package -- in the theme itself, in the file it includes, and in the path of an icon. | Every file of a theme has to be inside of the package that offers it. | `core/plugins/ThemeService.cpp` |
| The settings were written in place, so a reader could see half of a file, and the queue of the changes had no limit. | The file is written in one go and then put in place; the queue holds 64 changes; a file that is broken is never written. | `core/config/SettingsManager.cpp` |
| A path from the page was put into a command for the shell, so a quote in the path was the end of the path and the start of a command. A link could name a program of its own choosing. | The program is started without a shell; only `http`, `https` and `mailto` are opened; the output of a program is bounded and the program is stopped after two minutes. | `ui/compositor/Window.cpp` |
| A worker of the window was left running when the window went away, and it wrote into a window that was not there any more. | The workers are held by the window and are waited for before it goes away. | `ui/compositor/Window.cpp` |

The proof is in `src/tests/integration/CoreHardeningTest.cpp`.

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
| The JSON layer holds against hostile input | `build/vrutti_json_fuzz_test.exe` |
| The pipe holds against hostile input | `build/vrutti_ipc_fuzz_test.exe` |
| The parts that hold data hold against hostile input | `build/vrutti_core_hardening_test.exe` |
| Everything at once | `ctest --test-dir build --output-on-failure` |
