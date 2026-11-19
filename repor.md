# Vrutti IDE — Architecture Document

**Scope:** This document describes the architecture of `vrutti_ide/` only. The parent `vrutti/` directory (upstream VS Code source) is left untouched.

**Status:** Master architecture manifest. All future development within `vrutti_ide/` must adhere to the boundaries, tiers, and communication protocols defined here.

---

## 1. Executive Summary

Vrutti IDE is a high-performance, native-first development environment built from the ground up in `vrutti_ide/`. It intentionally diverges from the upstream VS Code architecture (parent `vrutti/`, which is Electron + TypeScript + Monaco) by using:

- A **native C++ core** as the single source of truth for state, documents, filesystem, terminal, and process management.
- A **Lit + Vite TypeScript frontend** rendered in a Webview/WebView2 compositor (not Electron).
- A **separate Node.js Extension Host** process that runs VS Code-compatible extensions and bridges to the C++ core over JSON-RPC IPC via named pipes.

Unlike VS Code, Vrutti does **not** bundle Monaco as its editor engine; the frontend uses CodeMirror backed by a C++ Piece Table document model. Unlike VS Code, Vrutti does **not** run the extension host inside the renderer; it runs as an independent Node process controlled by a C++ bootstrapper.

This document exists because `vrutti_ide/` currently lacks a single source-of-truth architecture reference: the existing `architecture.md`, `structure.md`, `SYSTEM_ARCHITECTURE.md`, `problem.md`, `task.md`, and `summary.md` are scattered, partially redundant, and describe features at inconsistent abstraction levels. This document consolidates them, maps the gaps against the parent VS Code architecture, and defines the target state.

---

## 2. Architecture Comparison: VS Code (parent `vrutti/`) vs Vrutti (`vrutti_ide/`)

| Layer | VS Code (parent `vrutti/`) | Vrutti IDE (`vrutti_ide/`) | Gap / Delta |
|-------|---------------------------|---------------------------|-------------|
| **Process model** | Electron browser + renderer + utility + extension host processes | C++ main process + Node.js extension host + Webview compositor | Vrutti splits rendering into a native compositor; VS Code uses Chromium throughout |
| **Main entry** | `src/main.ts` (Electron `app` lifecycle, portable, crash reporter, NLS, schemes) | `src/app/main.cpp` (C++ `main`, CWD fix, IPC client, spawns Node extHost, launches Window) | Vrutti lacks Electron's portable/crash/NLS/scheme infrastructure |
| **UI shell** | `src/vr/workbench/` (parts: activitybar, sidebar, panel, editor groups, statusbar, titlebar; DI container `instantiation`) | `src/ui/frontend/` (Lit components, hand-rolled `Registry.ts` contribution registry) | Vrutti lacks a formal DI container, composite part model, and workbench part lifecycle |
| **Editor engine** | Monaco (`src/vr/editor/`) — GPU-accelerated, webworker-based | CodeMirror 6 (frontend) + C++ Piece Table (`src/core/editor/`) | Vrutti editor backend is custom; no webworker, no GPU render path |
| **Platform services** | `src/vr/platform/` (~100 modules: commands, keybindings, configuration, memento, theme, webview, terminal, git, SCM, lifecycle, update, telemetry, remote, sandbox, secrets, storage, etc.) | `src/core/{concurrency,config,editor,events,fs,ipc,memory,plugins,terminal,utils}` | Vrutti has far fewer platform services; no secret storage, telemetry, crash reporter, update, remote, sandbox |
| **Extension model** | Extensions run in Extension Host (`extHost.ts`), JSON-RPC to main (`MainThreadShape`/`ExtHostShape`), contract in `vscode.d.ts` | Extensions run in Node.js (`src/ext/bootstrapper.js` → `extensionHostProcess`), IPC over named pipe to C++ | Vrutti reuses the upstream extHost binary but wraps it in C++ IPC; the `vscode.d.ts` API surface is only partially shimmed |
| **Extension UI injection** | Extensions contribute via `package.json` `contributes` → workbench registers views, commands, menus | Frontend parses `extensions/injections` IPC and injects raw CSS/JS into DOM (XSS risk) | Vrutti lacks sandboxed extension iframes; direct DOM injection is insecure |
| **Settings / config** | `configuration` service + `memento` + `storage`, sync across machines | `SettingsManager.cpp` (blocking `saveAsync().wait()`) + `localStorage` | Vrutti lacks config sync, memento, and async-safe persistence |
| **Terminal** | `platform/terminal` + xterm.js in webview, PTY via Node | `core/terminal/` (PTY) + xterm.js frontend | Roughly equivalent, but Vrutti terminal has shutdown deadlock bugs |
| **Search** | `platform/search` + ripgrep integration | `src/plugins/search/` native C++ plugin | Vrutti has native search plugin; not yet wired to UI |
| **Debug** | `platform/debug` + DAP (`dap-client.js`) | `core/plugins/DebugAdapterService.cpp` (C++) + `src/ext/dap-client.js` (Node) — **split-brain duplicate** | Duplicate DAP implementations in C++ and Node |
| **Language features** | `platform/languages` + LSP (`lsp-client.js`) | `core/plugins/LanguageFeatureService.cpp` + `src/ext/lsp-client.js` | LSP bridge partially present; no IntelliSense/UI wiring |
| **Build system** | Gulp + TypeScript + Electron forge | CMake (C++) + Vite (frontend) + PowerShell installers | Vrutti build is fragmented across CMake/Vite/PS1 |

---

## 3. Target Architecture (the "Architectural Matrix")

Vrutti operates on a decoupled 3-tier architecture:

```
[ Lit UI / Vite (Webview2 compositor) ]
        <-- Tauri-style IPC --> 
[ C++ Backend Broker (native DLLs, single source of truth) ]
        <-- JSON-RPC named-pipe --> 
[ Node.js Extension Host (VS Code extHost, unmodified upstream) ]
```

### 3.1 Tier 1 — Lit UI / Frontend (`src/ui/frontend/`)

Presentation layer only. Owns zero application state. Responsibilities:

- Render shell: activity bar, sidebar, panel, editor layout, status bar, title bar, menus.
- Render editor via CodeMirror 6; sync text with C++ Piece Table over IPC.
- Register contributions via `Registry.ts` (activity bar, views, panels, status bar, menus, commands, keybindings, configurations, editors, quick pick).
- Consume settings from C++ via IPC; emit setting changes back over IPC.
- Display terminals (xterm.js), problems view, search results, git graph, extensions view.

**Constraints:**
- No direct filesystem access; all file operations go through C++ IPC.
- No raw `eval`/`innerHTML` of untrusted extension content; extension CSS/JS must be sandboxed (iframe or CSP).
- Event listeners must use arrow functions / explicit `this` binding to avoid leaks (see `problem.md` #13).

### 3.2 Tier 2 — C++ Backend Broker (`src/core/`, `src/app/`)

The definitive owner of all application state. Composed of lazy-loaded DLL plugins managed by `PluginLoader`:

| Service DLL | Responsibility |
|-------------|--------------|
| `DocumentSyncService` | Single source of truth for text documents; Piece Table; debounced sync (50ms) to ExtHost and UI |
| `FileSystemService` | Native `std::filesystem` ops; caching layer for frequently-requested configs |
| `TerminalService` | Native PTY (ConPTY/forkpty); detached non-blocking threads streaming bytes to UI |
| `DebugAdapterService` | Native DAP client; spawns debug adapters as OS processes |
| `LanguageFeatureService` & `DiagnosticService` | IntelliSense bridge; routes autocomplete/hover/diagnostics between extensions and UI |
| `VSCodeExtHostService` | Generates `IExtensionHostInitData` payload to trick extensions into activating |
| `ExtensionLifecycleManager` | Extension install/uninstall/enable/disable lifecycle |
| `ExtensionScanner` | Scans extension directories and reads `package.json` manifests |
| `VSXRegistryService` | Queries Open VSX / marketplace for extension metadata |
| `CommandHandlerService` | Routes commands from UI to ExtHost or native handlers |
| `WorkspaceService` | Workspace folder topology; single source of truth for open folders |
| `SettingsManager` | User settings persistence (must be fixed: async, no mutex-held disk wait) |
| `PluginLoader` | `LoadLibrary`/`dlopen` of DLLs; `createPlugin`/`destroyPlugin` externs; **must be thread-safe** |

**Constraints (from `SYSTEM_ARCHITECTURE.md` core mandates):**
- Zero hardcoding: no hardcoded paths, themes, or workspace structures.
- All heavy lifting (process spawning, PTYs, filesystem) done natively in C++; Node.js used only where the extension host requires it.
- Memory efficiency: DLLs loaded/unloaded on demand; minimal RAM footprint.
- Thread safety: all `std::thread` loops respect `std::atomic<bool> active`; graceful `shutdown()` joins threads before unload.

### 3.3 Tier 3 — Node.js Extension Host (`src/ext/`, upstream `extHost`)

Runs VS Code-compatible extensions. Responsibilities:

- Load and activate extensions; provide the `vscode` API surface (shimmed where Vrutti diverges).
- Run LSP clients (`lsp-client.js`), DAP clients (`dap-client.js`), TextMate engine (`textmate-engine.js`), theme resolver (`theme-resolver.js`).
- Bridge to C++ core via `ipc.js` (JSON-RPC over named pipe).
- Bootstrapped by `bootstrapper.js`, which spawns the upstream `extensionHostProcess` with the pipe name injected via env vars.

**Constraints:**
- Must not orphan child processes on exit (hook `process.on('exit')` to kill language servers/debuggers).
- Must resolve promises for every IPC request (no leaked pending promises).
- Must not duplicate C++ services (remove the C++ DAP service; rely on Node DAP client).

---

## 4. Communication Protocols

### 4.1 Frontend ↔ C++ Core (IPC)
- Transport: named pipe / Webview2 host messaging.
- Framing: newline-delimited JSON-RPC 2.0.
- Methods: `vrutti_*` commands (e.g., `vruttiGetSettings`, `vruttiUpdateSetting`, `vruttiOpenFolderDialog`, `vruttiInstallExtension`).
- Requirement: every request must resolve its promise; C++ must call the webview's promise resolver after sending a reply (fixes `problem.md` #9).

### 4.2 C++ Core ↔ Extension Host (JSON-RPC)
- Transport: named pipe (`vrutti_pipe` → must be per-instance, e.g. `vrutti_pipe_<PID>`, fixing `problem.md` #11).
- Protocol: VS Code's native `MainThreadShape`/`ExtHostShape` JSON-RPC.
- Serialization: use a proper C++ JSON library (nlohmann/json) instead of string concatenation (fixes `problem.md` #16, #17).

### 4.3 Extension → Extension Host
- Standard VS Code Extension Host API (`vscode.d.ts` contract).
- Extension contributions (`package.json` `contributes`) registered into the frontend `Registry.ts` via IPC notifications.

---

## 5. State Ownership ("Single Source of Truth")

| State | Owner | Consumers |
|-------|-------|-----------|
| Text documents / Piece Table | C++ `DocumentSyncService` | UI (read), ExtHost (read via IPC) |
| Workspace folders | C++ `WorkspaceService` | UI, ExtHost |
| User settings | C++ `SettingsManager` | UI (read/write via IPC) |
| Extension registry/state | ExtHost (Node) | C++ (via IPC notifications), UI (read) |
| Terminal sessions | C++ `TerminalService` | UI (xterm.js) |
| Diagnostics / problems | C++ `DiagnosticService` | UI (problems panel) |
| UI layout / view visibility | Frontend (Lit state) | — (presentation only) |

If a file is modified externally, C++ `Piece Table` / `WorkspaceService` resolves and pushes deltas to both UI and ExtHost simultaneously.

---

## 6. Security Boundary

| Threat | Location | Mitigation |
|--------|----------|------------|
| Command injection (RCE) via `_popen` | `Window.cpp` extension install / git / run handlers | Replace shell concat with `CreateProcess`/`execvp` arg arrays; `libcurl` + `libzip` for downloads |
| Webview XSS via `eval` | `Window.cpp` IPC handlers | Escape all parameters; avoid `eval` |
| Extension DOM injection (XSS) | `main.ts` `extensions/injections` | Sandboxed iframe + postMessage; no direct DOM `innerHTML` |
| Lit `unsafeHTML` XSS | `vrutti-scm.ts` icons | `DOMPurify` sanitize before `unsafeHTML` |
| Hardcoded pipe collision | `main.cpp` `vrutti_pipe` | Per-instance `vrutti_pipe_<PID>` |
| Base64 UTF-8 corruption | `main.ts` `decodeURIComponent(escape(atob()))` | `TextDecoder` API |

---

## 7. Detailed Tier Specifications

### 7.1 App Entry (`src/app/main.cpp`)

Responsibilities: process bootstrap, argument parsing, CWD resolution to the install dir, IPC client initialization, VSCodeProtocol wiring, spawning the Node.js Extension Host, and launching the native compositor window.

Current issues to fix:
- Hardcoded pipe name `vrutti_pipe` → per-instance `vrutti_pipe_<PID>` (multi-instance collision).
- Hardcoded relative path `src\\ext\\bin\\node.exe` → resolve dynamically from executable directory.
- `CreateProcessA` called but handles closed immediately without tracking the process for graceful shutdown → track `PROCESS_INFORMATION` and terminate on exit.

### 7.2 Core Engine (`src/core/`)

Native C++ framework. Each subsystem below must expose a C `createPlugin`/`destroyPlugin` extern and be loadable/unloadable by `PluginLoader`.

| Subsystem | Files | Current State | Required Fix |
|-----------|-------|---------------|--------------|
| `concurrency/` | thread pools, async tasks | Present | None known |
| `config/` | `SettingsManager.cpp/h` | Blocking `saveAsync().wait()` while holding mutex | Push writes to a dedicated background worker thread queue; never block the caller on disk I/O |
| `editor/` | `PieceTable.cpp/h` | Piece Table document model present | Wire to CodeMirror sync over IPC; debounce 50ms |
| `events/` | Event bus | Present | None known |
| `fs/` | Filesystem ops | Listed in structure.md; verify implementation exists | Resolve paths dynamically relative to exe dir |
| `ipc/` | `IPCClient.cpp/h`, `VSCodeProtocol.cpp/h` | Naive string-concatenated JSON; string-match parsing | Integrate nlohmann/json for serialize/deserialize |
| `memory/` | `ArenaAllocator.cpp/h`, `Disposable.h` | Custom allocators present | None known |
| `plugins/` | ~16 service DLLs + `PluginLoader.cpp/h` | `PluginLoader` modifies `m_loadedPlugins` without mutex | Add `std::mutex` + `lock_guard` around map accesses |
| `terminal/` | `TerminalProcess.cpp/h`, `ReplProcess.cpp/h` | `stop()` joins a thread blocked on `ReadFile` → shutdown deadlock | Use `CancelIoEx` or async detach; remove dead sessions from `m_sessions` to stop leaks |

### 7.3 Native Plugins (`src/plugins/`)

Independent high-performance DLLs loaded at runtime.

| Plugin | Status |
|--------|--------|
| `search/` | Native regex/plain-text code scanner; present but not yet wired to frontend search UI |

### 7.4 Extension Host (`src/ext/`)

Node.js environment loading VS Code extensions.

| File | Role | Issues |
|------|------|--------|
| `bootstrapper.js` | Spawns upstream `extensionHostProcess` with pipe env | On C++ core exit, calls `process.exit(0)` without killing children → orphaned language servers/debuggers |
| `ipc.js` | JSON-RPC client over named pipe | Connection `end`/`error` calls `process.exit(0)`; pending promises never resolved on disconnect |
| `dap-client.js` | DAP client | Regex `Content-Length: (\d+)` expects exactly one space → use `\s*` |
| `lsp-client.js` | LSP client | Present; verify wiring to `LanguageFeatureService` |
| `textmate-engine.js` | TextMate grammar tokenizer | Present |
| `theme-resolver.js` | VS Code theme → IDE color mapping | Present |

Duplicate to remove: `src/core/plugins/DebugAdapterService.cpp` duplicates `src/ext/dap-client.js`. Deprecate and delete the C++ DAP service; rely purely on the Extension Host.

### 7.5 User Interface (`src/ui/`)

| Subsystem | Files | Role |
|-----------|-------|------|
| `compositor/` | `Window.cpp/h` | Native Win32/WebView2 window; IPC bindings; **contains security flaws** (_popen, eval, string-concatenated JS) |
| `frontend/` | Lit + Vite TS app | Presentation; hand-rolled `Registry.ts` contribution registry; CodeMirror editor; xterm.js terminal |
| `vendor/` | Third-party assets | Codicons, xterm CSS |

UI components already built: sidebar, statusbar, menubar, panel, editor-layout, quick-pick, extension-details, tree-view, search, scm, debug-console, debug-sidebar, diff-editor, output-view, problems-view, settings, terminal-view, terminal, webview, task-manager, git-graph.

Missing UI subsystems (not yet implemented): minimap, breadcrumbs, LSP diagnostics inline, notebook editor, merge editor, remote explorer, source control timeline.

---

## 8. Gap Analysis: What `vrutti_ide/` Lacks vs VS Code (parent `vrutti/`)

This is the core answer to "what our app lacks." The gaps are grouped by subsystem.

### 8.1 Platform Services Missing Entirely

VS Code `src/vr/platform/` has ~100 modules. Vrutti has none of the following as first-class services:

- **Secret storage / credentials** (`secrets/`) — no keytar/credential-manager integration for extension auth tokens (GitHub, Azure, etc.).
- **Telemetry & crash reporting** (`telemetry/`, crash reporter) — no telemetry pipeline, no crash dump capture, no App Center integration.
- **Auto-update** (`update/`) — no update check/download/installer mechanism.
- **Remote development** (`remote/`, `remoteTunnel/`, `tunnel/`) — no SSH/WSL/remote tunnel support.
- **Sandboxing** (`sandbox/`) — no renderer sandbox, no process isolation for extensions beyond a single Node process.
- **User data sync / profiles** (`userDataSync/`, `userDataProfile/`) — no roaming settings, no profile switching.
- **Accessibility** (`accessibility/`) — no screen-reader announcements, ARIA management, high-contrast forced modes.
- **Authentication** (`authProvider`, `authSession`) — no OAuth provider framework for extensions.
- **Encryption** (`encryption/`) — no at-rest encryption of credentials/storage.
- **Network filter / proxy** (`networkFilter/`) — no proxy configuration pipeline.
- **Opener / URI handling** (`opener/`, `uriIdentity/`) — no registered URI scheme handler (`vrutti://`).
- **Native dialogs** (`dialogs/`) — limited; open folder/file dialogs exist but no save-as dialog (currently `window.alert('Save As not implemented')`).
- **Notifications** (`notification/`) — no toast/notifications API for extensions.
- **Progress** (`progress/`) — no progress bar/service for long operations.
- **Quick input** (`quickinput/`) — quick pick exists but no input box/password prompt.
- **Markdown** (`markdown/`) — no markdown previewer.
- **Backup** (`backup/`) — no unsaved-edits backup/recovery.
- **History / undo-redo across workbench** (`undoRedo/`) — editor-level only, no workbench-level command history.
- **Labels / URI identity** (`label/`, `uriIdentity/`) — no consistent URI/label formatting service.
- **Lifecycle** (`lifecycle/`) — no shutdown veto / startup event sequencing beyond a 2500ms timeout.

### 8.2 Workbench Shell Missing vs VS Code

VS Code `src/vr/workbench/` is built on a formal DI container (`instantiation/`) and a composite part model (`parts/`). Vrutti's `Registry.ts` is a hand-rolled event emitter without:

- **Service dependency injection** — no `ServiceCollection`, no `createDecorator`, no lazy service instantiation.
- **Composite parts** (`paneCompositePart`, `compositePart`) — no split-pane editor groups with drag-to-rearrange; editor layout is fixed (single editor + bottom terminal panel).
- **Part lifecycle** (`part.ts`, `workbench.contribution.ts`) — no `WorkbenchContribution` registration/startup ordering.
- **Title bar parts** — custom frameless title bar exists but lacks window management integration (snap, restore, RDP display tracking).
- **Activity bar / sidebar / panel / status bar services** — registered via `Registry.ts` but no `viewContainer` service managing visibility, collapsed state, persistence.
- **Editor groups** — no split editors (roadmap item: "Split Editors: Exists, Dynamic: Yes" but not verified in code); no editor group drag-and-drop.
- **Zen mode / center layout** — not present.
- **Welcome page** — not present.

### 8.3 Editor Engine Missing vs Monaco

VS Code uses Monaco (`src/vr/editor/`) with:
- GPU-accelerated rendering (`editor/gpu/`), webworker-based language features (`editor.worker.start.ts`), observable model (`observableCodeEditor.ts`), stable scroll, find/replace widget, inline comments, insets, peek views, peek editors.

Vrutti uses CodeMirror 6 (frontend) + C++ Piece Table (backend). Missing:
- **Webworker language features** — language services run on the main thread or C++ DLLs; no isolated worker sandbox.
- **GPU render path** — none.
- **Find/replace widget** — `vrutti-search.ts` exists but find-in-editor widget not confirmed.
- **Peek editor / inline comments** — not present.
- **Minimap** — roadmap says "Minimap & Breadcrumbs: Exists: No".
- **Breadcrumbs** — not present.
- **Diff editor** — `vrutti-diff-editor.ts` exists as a component stub; not wired to a diff service.
- **Notebook editor** — not present.

### 8.4 Extension Ecosystem Gaps

VS Code extensions rely on the full `vscode.d.ts` API surface (179 proposed `.d.ts` files in parent `src/vscode-dts/`). Vrutti shims only a subset:

- **Extension contributions** — `contributes` partially handled (activitybar views, views, statusbar, menus). Missing: `commands`, `keybindings`, `breakpoints`, `languages`, `grammars`, `snippets`, `themes`, `problemMatchers`, `debuggers`, `typescript`, `jsonValidation`, `customEditors`, `notebookRenderer`, `remoteSolvers`, `terminalProfiles`, `walkthroughs`, `welcomePage`.
- **Extension host API surface** — only a fraction of `vscode` API methods are proxied over IPC; most will throw "not implemented".
- **Extension marketplace** — Open VSX URL hardcoded in frontend; no `VSXRegistryService` wiring confirmed; no extension install/uninstall flow that isn't RCE-vulnerable.
- **Extension sandbox** — extensions inject raw CSS/JS into the main DOM; no iframe sandbox.

### 8.5 Security & Correctness Gaps (from `problem.md`)

27 cataloged issues. Critical unmitigated:
1. RCE via `_popen` in extension installer — **unfixed**.
2. RCE via git/run command string concat — **unfixed**.
3. XSS via `eval` in IPC — **unfixed**.
4. Extension DOM injection XSS — **unfixed**.
5. Lit `unsafeHTML` XSS — **unfixed**.
6. Terminal shutdown deadlock — **unfixed**.
7. Thread sync leaks in services — **unfixed**.
8. Blocking disk I/O in `SettingsManager` — **unfixed**.
9. Leaked pending IPC promises — **unfixed**.
10. Orphaned child processes — **unfixed**.
11. Hardcoded pipe collision — **unfixed**.
12. `PluginLoader` race — **unfixed**.
13. Lit listener `this` leaks — **unfixed**.
14. Split-brain extension manager (C++ `Window.cpp` vs Node `ExtensionManager`) — **unfixed**.
15. Split-brain DAP (C++ `DebugAdapterService` vs Node `dap-client.js`) — **unfixed**.
16-17. Brittle string-concatenated JSON IPC — **unfixed**.
18-23. Hardcoded paths/URLs/colors/UTF-8/base64 — **partially unfixed**.

### 8.6 Feature Roadmap Gaps (from `structure.md`)

| Feature | Status |
|---------|--------|
| Problems / Diagnostics Panel | Exists: Yes, Dynamic: Yes |
| Split Editors | Exists: Yes, Dynamic: Yes (unverified in code) |
| Search & Replace | Exists: Yes, Dynamic: Yes |
| LSP | Exists: No |
| Diff Editor | Exists: No |
| Minimap & Breadcrumbs | Exists: No |

Additional missing: notebook editor, merge editor, remote explorer, source control timeline, welcome page, settings sync, auto-update.

---

## 9. Remediation Roadmap (Priority Order)

### Phase 1 — Security (critical, unmitigated RCE/XSS)
1. Replace `_popen` shell concat with `CreateProcess`/`execvp` arg arrays + `libcurl`/`libzip` for extension install.
2. Remove `eval`; escape all IPC parameters into JS string literals.
3. Sandbox extension CSS/JS injections (iframe + postMessage); remove direct `innerHTML`/`script.innerHTML`.
4. Sanitize all `unsafeHTML` inputs with `DOMPurify`.
5. Fix base64 UTF-8 decoding → `TextDecoder`.
6. Fix DAP `Content-Length` regex → `\s*`.

### Phase 2 — Stability (deadlocks, leaks, races)
7. Fix terminal `stop()` deadlock (`CancelIoEx` / async detach).
8. Fix service thread shutdown (condition variables; remove dead sessions).
9. Fix `SettingsManager` blocking disk I/O (background worker queue).
10. Resolve pending IPC promises on every response.
11. Hook `process.on('exit')` in bootstrapper to kill child processes.
12. Per-instance pipe name `vrutti_pipe_<PID>`.
13. Add `std::mutex` to `PluginLoader`.
14. Fix Lit listener `this` binding (arrow functions).
15. Resolve all paths dynamically from executable directory.

### Phase 3 — Architecture schizophrenia
16. Remove C++ shell-script extension management; route all extension ops over IPC to Node `ExtensionManager`.
17. Delete C++ `DebugAdapterService`; rely on Node `dap-client.js`.
18. Use nlohmann/json for IPC serialization (replace string concat).

### Phase 4 — Missing platform services
19. Secret storage, telemetry, crash reporter, auto-update.
20. Notifications, progress, quick input, dialogs (save-as).
21. Remote development, profiles, user data sync.

### Phase 5 — Missing editor/workbench features
22. Split editor groups, minimap, breadcrumbs, diff editor wiring, notebook editor, welcome page, zen mode.

---

## Appendix A — IPC Message Catalog (Reference)

| Direction | Method | Params | Purpose |
|-----------|--------|--------|---------|
| UI → C++ | `extensions/activateEvent` | `{ event: '*' \| 'onStartupFinished' }` | Activate extensions on startup event |
| UI → C++ | `theme/set` | `{ name }` | Apply color theme |
| UI → C++ | `icon_theme/set` | `{ name }` | Apply icon theme |
| UI → C++ | `extensions/installed` | `[]` | Register extension views/activity bar |
| UI → C++ | `command/execute` | `{ command }` | Execute a registry command |
| C++ → UI | `extensions/injections` | `[{type:'css'|'js', content}]` | Inject extension CSS/JS (secure only after sandboxing) |
| C++ → UI | `run/output` | `{ text }` | Terminal/run output stream |
| C++ → UI | `menu/action` | `{ command \| action }` | Menu action triggered from backend |
| C++ → UI | `statusbar/show\|update\|hide` | `{ id, text, tooltip, alignment, priority }` | Status bar item lifecycle |
| C++ → UI | `treeview/register` | `{ id }` | Register a tree view |

All messages are newline-delimited JSON-RPC 2.0. Every request carries an `id`; C++ must resolve the matching frontend promise.

---

## Appendix B — Build & Deployment Mapping (`vrutti_ide/` only)

| Artifact | Path | Purpose |
|----------|------|---------|
| C++ core build | `CMakeLists.txt`, `Makefile`, `build/`, `build-linux/`, `build_ucrt64/` | Compiles native core DLLs + `vrutti_app.exe` |
| Frontend build | `src/ui/frontend/vite.config.ts`, `package.json` | Vite + TypeScript build of Lit app |
| Installer | `build_installer.ps1` | Inno Setup 6 → `Vrutti_Alpha_0.0.3_SleepyRabbit_Setup.exe` |
| Portable ZIP | `package_release.ps1` | Bundles core + frontend into ZIP |
| Product config | `product.json` | App identity (name, data folder, mutex, URL protocol) |
| Test harness | `test_scanner.cpp`, `test_scanner.exe`, `test_install.ps1` | Search plugin + installer validation |
| Docker frontend | `docker-compose.frontend.yml`, `Dockerfile.frontend` | Frontend containerized build |

---

## Appendix C — File-to-Tier Index (`vrutti_ide/` only)

| Tier | Directory | Key Files |
|------|-----------|-----------|
| App Entry | `src/app/` | `main.cpp`, `structure.md`, `vrutti_app.rc` |
| Core Engine | `src/core/` | `concurrency/`, `config/`, `editor/`, `events/`, `fs/`, `ipc/`, `memory/`, `plugins/`, `terminal/`, `utils/` |
| Native Plugins | `src/plugins/` | `search/`, `CommandHandlerService.*`, `DebugAdapterService.*`, `DiagnosticService.*`, `DocumentSyncService.*`, `ExtensionLifecycleManager.*`, `ExtensionManager.*`, `ExtensionScanner.*`, `FileSystemService.*`, `LanguageFeatureService.*`, `MediaController.*`, `PluginLoader.*`, `TerminalService.*`, `VSCodeExtHostService.*`, `VSXRegistryService.*`, `WorkspaceService.*` |
| Extension Host | `src/ext/` | `bootstrapper.js`, `crypto-polyfill.js`, `dap-client.js`, `ipc.js`, `lsp-client.js`, `textmate-engine.js`, `theme-resolver.js`, `bootstrapper/` legacy, `builtin-themes/`, `bin/`, `node_modules/` |
| UI | `src/ui/` | `compositor/Window.cpp`, `frontend/src/` (Lit app), `vendor/` |
| Config | Root | `product.json`, `package.json`, `CMakeLists.txt`, `Makefile` |

---

## Appendix D — Governance & Change Control

This `architecture.md` is the single source of truth for `vrutti_ide/`. The `SYSTEM_ARCHITECTURE.md` core mandates remain in force:

1. **Zero hardcoding** — no hardcoded environment paths, themes, or workspace structures.
2. **No Node.js backend services** — except the extension host process; all heavy lifting is native C++.
3. **Memory efficiency** — DLLs loaded/unloaded on demand; minimal RAM footprint.
4. **Graceful termination** — all threads respect `std::atomic<bool> active`; `shutdown()` joins threads before unload.

Any PR that modifies tier boundaries, adds a platform service, or changes the IPC protocol must update this document. Scattered architecture notes (`structure.md`, `SYSTEM_ARCHITECTURE.md`, `problem.md`, `task.md`, `summary.md`) remain as supplementary references but do not override this document.

End of architecture.md.